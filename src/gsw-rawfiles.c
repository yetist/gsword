/*
 * gsw-rawfiles.c: RawFiles backend (one file per entry)
 */

#include "gsw-module-backend.h"
#include "gsw-backend-utils.h"
#include "gsw-data.h"
#include "gsw-key-internal.h"
#include "gsw-utils.h"

#include <errno.h>
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
        gsw_rawfiles_free
    };

    backend->info = g_object_ref(info);
    backend->ops = &ops;
    backend->data = data;
    return backend;
}
