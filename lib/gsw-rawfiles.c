/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-rawfiles.c: RawFiles backend (one file per entry)
 */

#include "gsw-module-backend.h"
#include "gsw-backend-utils.h"
#include "gsw-data.h"
#include "gsw-key-internal.h"
#include "gsw-lock.h"
#include "gsw-utils.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    gchar *data_dir;
    FILE *ot;
    FILE *nt;
    FILE *ot_vss;
    FILE *nt_vss;
} GswRawFilesData;

static gboolean
gsw_rawfiles_open(GswModuleBackend *backend, GError **error)
{
    GswRawFilesData *data = backend->data;
    gchar *path;

    if (backend->opened)
        return TRUE;

    data->data_dir = gsw_module_data_dir(backend->info);

#define OPEN_FILE(field, name)                                               \
    do {                                                                     \
        path = g_build_filename(data->data_dir, name, NULL);                 \
        field = fopen(path, "rb");                                           \
        if (field == NULL) {                                                 \
            g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,               \
                        "Cannot open RawFiles file %s: %s", path,             \
                        g_strerror(errno));                                  \
            g_free(path);                                                    \
            goto fail;                                                       \
        }                                                                    \
        g_free(path);                                                        \
    } while (0)

    OPEN_FILE(data->ot, "ot");
    OPEN_FILE(data->nt, "nt");
    OPEN_FILE(data->ot_vss, "ot.vss");
    OPEN_FILE(data->nt_vss, "nt.vss");
#undef OPEN_FILE

    backend->opened = TRUE;
    return TRUE;

fail:
    if (data->ot) fclose(data->ot);
    if (data->nt) fclose(data->nt);
    if (data->ot_vss) fclose(data->ot_vss);
    if (data->nt_vss) fclose(data->nt_vss);
    data->ot = data->nt = data->ot_vss = data->nt_vss = NULL;
    return FALSE;
}

static void
gsw_rawfiles_close(GswModuleBackend *backend)
{
    GswRawFilesData *data = backend->data;

    if (data->ot) fclose(data->ot);
    if (data->nt) fclose(data->nt);
    if (data->ot_vss) fclose(data->ot_vss);
    if (data->nt_vss) fclose(data->nt_vss);
    data->ot = data->nt = data->ot_vss = data->nt_vss = NULL;
    g_free(data->data_dir);
    data->data_dir = NULL;
    backend->opened = FALSE;
}

static gboolean
gsw_rawfiles_find(GswModuleBackend *backend, GswKey *key,
                  guint32 *start, guint32 *size, GError **error)
{
    GswRawFilesData *data = backend->data;
    gint testament = gsw_verse_key_get_testament((GswVerseKey *)key);
    glong index = gsw_verse_key_get_testament_index((GswVerseKey *)key);
    FILE *idx;
    guint8 record[6];

    if (!backend->opened && !gsw_rawfiles_open(backend, error))
        return FALSE;

    if (testament == 2)
        idx = data->nt_vss;
    else
        idx = data->ot_vss;

    if (index < 0 || !gsw_file_read_at(idx, (gint64)index * 6,
                                       record, 6, error))
        return FALSE;

    *start = gsw_le32(record);
    *size = gsw_le16(record + 4);
    return TRUE;
}

static GBytes *
gsw_rawfiles_read(GswModuleBackend *backend, GswKey *key, GError **error)
{
    GswRawFilesData *data = backend->data;
    guint32 start, size;
    gchar *name = NULL;
    gchar *path = NULL;
    gchar *content = NULL;
    gsize len = 0;
    GError *local_error = NULL;

    if (!gsw_rawfiles_find(backend, key, &start, &size, error))
        return NULL;
    if (size == 0)
        return g_bytes_new(NULL, 0);

    name = g_malloc(size + 1);
    if (!gsw_file_read_at((gsw_verse_key_get_testament((GswVerseKey *)key) == 2)
                          ? data->nt : data->ot, start, name, size, error)) {
        g_free(name);
        return NULL;
    }
    name[size] = '\0';
    while (size > 0 && (name[size - 1] == '\r' || name[size - 1] == '\n'))
        name[--size] = '\0';

    path = gsw_path_join_under(data->data_dir, name, error);
    g_free(name);
    if (path == NULL)
        return NULL;

    if (!g_file_get_contents(path, &content, &len, &local_error)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot read RawFiles entry %s: %s", path,
                    local_error->message);
        g_error_free(local_error);
        g_free(path);
        return NULL;
    }
    g_free(path);
    return g_bytes_new_take(content, len);
}

static gboolean
gsw_rawfiles_has(GswModuleBackend *backend, GswKey *key)
{
    guint32 start, size;
    GError *error = NULL;

    if (!gsw_rawfiles_find(backend, key, &start, &size, &error)) {
        g_clear_error(&error);
        return FALSE;
    }
    return size > 0;
}

static gboolean
gsw_rawfiles_next(GswModuleBackend *backend, GswKey *key, gint steps,
                  GError **error)
{
    (void)backend;
    if (!gsw_key_next(key, steps)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Verse key moved out of bounds");
        return FALSE;
    }
    return TRUE;
}

static gboolean
gsw_rawfiles_set_position(GswModuleBackend *backend, GswKey *key,
                          GswPosition pos, GError **error)
{
    GswRawFilesData *data = backend->data;
    glong ot_index, nt_index;

    if (pos == GSW_POSITION_TOP) {
        gsw_verse_key_set_index((GswVerseKey *)key, 0);
        return TRUE;
    }
    if (!gsw_rawfiles_open(backend, error))
        return FALSE;
    if (!gsw_find_last_nonempty(data->ot_vss, 6, 4, FALSE, &ot_index,
                                error) ||
        !gsw_find_last_nonempty(data->nt_vss, 6, 4, FALSE, &nt_index,
                                error))
        return FALSE;

    return gsw_verse_key_set_last(key, ot_index, nt_index, error);
}

static gboolean
rawfiles_index_paths(GswRawFilesData *data, GswKey *key, gchar **table_path,
                     gchar **index_path)
{
    gint testament = gsw_verse_key_get_testament((GswVerseKey *)key);
    const gchar *table = testament == 2 ? "nt" : "ot";
    gchar *index;

    index = g_strconcat(table, ".vss", NULL);
    *table_path = g_build_filename(data->data_dir, table, NULL);
    *index_path = g_build_filename(data->data_dir, index, NULL);
    g_free(index);
    return TRUE;
}

static gboolean
rawfiles_write_record(FILE *idx, glong index, guint32 start, guint16 size,
                      GError **error)
{
    guint8 record[6];

    record[0] = (guint8)(start & 0xff);
    record[1] = (guint8)((start >> 8) & 0xff);
    record[2] = (guint8)((start >> 16) & 0xff);
    record[3] = (guint8)((start >> 24) & 0xff);
    record[4] = (guint8)(size & 0xff);
    record[5] = (guint8)((size >> 8) & 0xff);

    if (fseeko(idx, (off_t)(index * 6), SEEK_SET) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot seek RawFiles index: %s", g_strerror(errno));
        return FALSE;
    }
    if (fwrite(record, 1, sizeof(record), idx) != sizeof(record)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot write RawFiles index: %s", g_strerror(errno));
        return FALSE;
    }
    return fflush(idx) == 0;
}

static gboolean
gsw_rawfiles_write_entry(GswModuleBackend *backend, GswKey *key,
                         const guint8 *bytes, gsize len, GError **error)
{
    GswRawFilesData *data = backend->data;
    gchar *table_path = NULL;
    gchar *index_path = NULL;
    gchar *lock_path = NULL;
    gchar *name = NULL;
    gchar *path = NULL;
    gchar *parent = NULL;
    FILE *idx = NULL;
    gint lock_fd = -1;
    guint8 record[6];
    guint32 start = 0;
    guint16 size = 0;
    glong index;
    gboolean new_entry = FALSE;
    gboolean ok = FALSE;
    GError *local_error = NULL;

    if (gsw_module_info_is_ciphered(backend->info)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CIPHERED,
                    "Writing encrypted RawFiles modules is not supported");
        return FALSE;
    }
    if (!gsw_rawfiles_open(backend, error))
        return FALSE;

    index = gsw_verse_key_get_testament_index((GswVerseKey *)key);
    rawfiles_index_paths(data, key, &table_path, &index_path);
    lock_path = g_strconcat(index_path, ".lock", NULL);
    lock_fd = gsw_file_lock(lock_path, error);
    if (lock_fd < 0)
        goto out;
    idx = fopen(index_path, "r+b");
    if (idx == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot open RawFiles index %s: %s", index_path,
                    g_strerror(errno));
        goto out;
    }
    if (fseeko(idx, (off_t)(index * 6), SEEK_SET) != 0 ||
        fread(record, 1, sizeof(record), idx) != sizeof(record)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Cannot read RawFiles index record: %s", g_strerror(errno));
        goto out;
    }
    start = gsw_le32(record);
    size = gsw_le16(record + 4);
    new_entry = size == 0;

    if (size > 0) {
        FILE *table = gsw_verse_key_get_testament((GswVerseKey *)key) == 2
            ? data->nt : data->ot;

        name = g_malloc((gsize)size + 1);
        if (!gsw_file_read_at(table, start, name, size, error)) {
            g_free(name);
            name = NULL;
            goto out;
        }
        name[size] = '\0';
        while (size > 0 && (name[size - 1] == '\r' || name[size - 1] == '\n'))
            name[--size] = '\0';
    } else {
        GStatBuf st;
        FILE *table;
        guint suffix = 0;

        for (;;) {
            g_free(name);
            name = g_strdup_printf("verse-%d-%ld-%u.txt",
                                   gsw_verse_key_get_testament(
                                       (GswVerseKey *)key),
                                   index, suffix++);
            path = gsw_path_join_under(data->data_dir, name, error);
            if (path == NULL)
                goto out;
            if (!g_file_test(path, G_FILE_TEST_EXISTS))
                break;
            g_free(path);
            path = NULL;
        }
        g_free(path);
        path = NULL;

        if (g_stat(table_path, &st) != 0) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                        "Cannot stat RawFiles name table %s: %s",
                        table_path, g_strerror(errno));
            goto out;
        }
        start = (guint32)st.st_size;
        size = (guint16)strlen(name);
        table = fopen(table_path, "ab");
        if (table == NULL) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                        "Cannot append RawFiles name table %s: %s",
                        table_path, g_strerror(errno));
            goto out;
        }
        if (fwrite(name, 1, size, table) != size) {
            fclose(table);
            g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                        "Cannot write RawFiles name table %s: %s",
                        table_path, g_strerror(errno));
            goto out;
        }
        fclose(table);
    }

    path = gsw_path_join_under(data->data_dir, name, error);
    if (path == NULL)
        goto out;
    parent = g_path_get_dirname(path);
    if (!gsw_mkdir_with_parents(parent, error))
        goto out;
    {
        gchar *tmp = g_strconcat(path, ".tmp", NULL);

        if (!g_file_set_contents(tmp, (const gchar *)bytes, (gssize)len,
                                 &local_error)) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                        "Cannot write RawFiles entry %s: %s", tmp,
                        local_error->message);
            g_error_free(local_error);
            g_unlink(tmp);
            g_free(tmp);
            goto out;
        }
        if (g_rename(tmp, path) != 0) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                        "Cannot replace RawFiles entry %s: %s", path,
                        g_strerror(errno));
            g_unlink(tmp);
            g_free(tmp);
            goto out;
        }
        g_free(tmp);
    }

    if (new_entry &&
        !rawfiles_write_record(idx, index, start, size, error)) {
        g_unlink(path);
        goto out;
    }

    ok = TRUE;

out:
    if (idx != NULL)
        fclose(idx);
    gsw_file_unlock(lock_fd);
    g_free(table_path);
    g_free(index_path);
    g_free(lock_path);
    g_free(name);
    g_free(path);
    g_free(parent);
    if (ok) {
        gsw_rawfiles_close(backend);
        gsw_rawfiles_open(backend, NULL);
    }
    return ok;
}

static gboolean
gsw_rawfiles_delete_entry(GswModuleBackend *backend, GswKey *key,
                          GError **error)
{
    GswRawFilesData *data = backend->data;
    gchar *table_path = NULL;
    gchar *index_path = NULL;
    gchar *lock_path = NULL;
    gchar *name = NULL;
    gchar *path = NULL;
    FILE *idx = NULL;
    FILE *table;
    gint lock_fd = -1;
    guint8 record[6];
    guint32 start;
    guint16 size;
    guint16 old_size;
    glong index;
    gboolean ok = FALSE;

    if (!gsw_rawfiles_open(backend, error))
        return FALSE;

    index = gsw_verse_key_get_testament_index((GswVerseKey *)key);
    rawfiles_index_paths(data, key, &table_path, &index_path);
    lock_path = g_strconcat(index_path, ".lock", NULL);
    lock_fd = gsw_file_lock(lock_path, error);
    if (lock_fd < 0)
        goto out;
    idx = fopen(index_path, "r+b");
    if (idx == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot open RawFiles index %s: %s", index_path,
                    g_strerror(errno));
        goto out;
    }
    if (fseeko(idx, (off_t)(index * 6), SEEK_SET) != 0 ||
        fread(record, 1, sizeof(record), idx) != sizeof(record)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Cannot read RawFiles index record: %s", g_strerror(errno));
        goto out;
    }
    start = gsw_le32(record);
    size = gsw_le16(record + 4);
    old_size = size;
    if (size == 0) {
        ok = TRUE;
        goto out;
    }

    table = gsw_verse_key_get_testament((GswVerseKey *)key) == 2
        ? data->nt : data->ot;
    name = g_malloc((gsize)size + 1);
    if (!gsw_file_read_at(table, start, name, size, error)) {
        g_free(name);
        name = NULL;
        goto out;
    }
    name[size] = '\0';
    while (size > 0 && (name[size - 1] == '\r' || name[size - 1] == '\n'))
        name[--size] = '\0';

    path = gsw_path_join_under(data->data_dir, name, error);
    if (path == NULL)
        goto out;
    if (!rawfiles_write_record(idx, index, 0, 0, error)) {
        rawfiles_write_record(idx, index, start, old_size, NULL);
        goto out;
    }
    if (g_unlink(path) != 0 && errno != ENOENT) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot delete RawFiles entry %s: %s", path,
                    g_strerror(errno));
        rawfiles_write_record(idx, index, start, old_size, NULL);
        goto out;
    }
    ok = TRUE;

out:
    if (idx != NULL)
        fclose(idx);
    gsw_file_unlock(lock_fd);
    g_free(table_path);
    g_free(index_path);
    g_free(lock_path);
    g_free(name);
    g_free(path);
    if (ok) {
        gsw_rawfiles_close(backend);
        gsw_rawfiles_open(backend, NULL);
    }
    return ok;
}

static gboolean
gsw_rawfiles_is_writable(GswModuleBackend *backend, GswKey *key)
{
    (void)backend;
    (void)key;
    return TRUE;
}

static void
gsw_rawfiles_free(GswModuleBackend *backend)
{
    GswRawFilesData *data = backend->data;

    gsw_rawfiles_close(backend);
    g_free(data);
    backend->data = NULL;
}

GswModuleBackend *
gsw_rawfiles_backend_new(GswModuleInfo *info)
{
    GswModuleBackend *backend = g_new0(GswModuleBackend, 1);
    GswRawFilesData *data = g_new0(GswRawFilesData, 1);
    static const GswModuleBackendOps ops = {
        gsw_rawfiles_open,
        gsw_rawfiles_close,
        gsw_rawfiles_read,
        gsw_rawfiles_has,
        gsw_rawfiles_next,
        gsw_rawfiles_set_position,
        gsw_rawfiles_write_entry,
        gsw_rawfiles_delete_entry,
        gsw_rawfiles_is_writable,
        gsw_rawfiles_free
    };

    backend->info = g_object_ref(info);
    backend->ops = &ops;
    backend->data = data;
    return backend;
}
