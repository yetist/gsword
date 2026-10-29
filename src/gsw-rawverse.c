/*
 * gsw-rawverse.c: RawText/RawCom backend
 */

#include "gsw-module-backend.h"
#include "gsw-data.h"
#include "gsw-key-internal.h"
#include "gsw-cipher.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    gboolean is4;
    FILE *ot;
    FILE *nt;
    FILE *ot_vss;
    FILE *nt_vss;
    gchar *cipher_key;
} GswRawVerseData;

static gboolean
gsw_rawverse_open(GswModuleBackend *backend, GError **error)
{
    GswRawVerseData *data = backend->data;
    const gchar *path = gsw_module_info_get_absolute_data_path(backend->info);
    gchar *file;

    if (data->ot != NULL)
        return TRUE;

#define OPEN_OR_FAIL(field, name)                                            \
    do {                                                                     \
        file = g_build_filename(path, name, NULL);                           \
        field = fopen(file, "rb");                                           \
        if (field == NULL) {                                                 \
            g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,               \
                        "Cannot open module data file %s: %s", file,          \
                        g_strerror(errno));                                  \
            g_free(file);                                                    \
            goto fail;                                                       \
        }                                                                    \
        g_free(file);                                                        \
    } while (0)

    OPEN_OR_FAIL(data->ot, "ot");
    OPEN_OR_FAIL(data->nt, "nt");
    OPEN_OR_FAIL(data->ot_vss, "ot.vss");
    OPEN_OR_FAIL(data->nt_vss, "nt.vss");

#undef OPEN_OR_FAIL

    if (gsw_module_info_is_ciphered(backend->info)) {
        const gchar *key = gsw_module_info_get_value(backend->info, "CipherKey");
        if (key == NULL || *key == '\0') {
            g_set_error(error, GSW_ERROR, GSW_ERROR_CIPHERED,
                        "RawVerse module is encrypted but no CipherKey is configured");
            goto fail;
        }
        data->cipher_key = g_strdup(key);
    }

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
gsw_rawverse_close(GswModuleBackend *backend)
{
    GswRawVerseData *data = backend->data;

    if (data->ot) fclose(data->ot);
    if (data->nt) fclose(data->nt);
    if (data->ot_vss) fclose(data->ot_vss);
    if (data->nt_vss) fclose(data->nt_vss);
    data->ot = data->nt = data->ot_vss = data->nt_vss = NULL;
    g_free(data->cipher_key);
    data->cipher_key = NULL;
    backend->opened = FALSE;
}

static gboolean
gsw_rawverse_find(GswModuleBackend *backend, GswKey *key,
                  guint32 *start, guint32 *size, GError **error)
{
    GswRawVerseData *data = backend->data;
    gint testament = gsw_verse_key_get_testament((GswVerseKey *)key);
    glong index = gsw_verse_key_get_testament_index((GswVerseKey *)key);
    FILE *idx;
    gint64 offset;
    guint8 record[8];
    gsize rec_size = data->is4 ? 8 : 6;

    if (!backend->opened && !gsw_rawverse_open(backend, error))
        return FALSE;

    if (testament == 2) {
        idx = data->nt_vss;
    } else {
        testament = 1;
        idx = data->ot_vss;
    }

    if (index < 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Negative verse index");
        return FALSE;
    }

    offset = (gint64)index * (gint64)rec_size;
    if (offset + (gint64)rec_size > gsw_file_size(idx)) {
        *start = 0;
        *size = 0;
        return TRUE;
    }
    if (!gsw_file_read_at(idx, offset, record, rec_size, error))
        return FALSE;

    *start = gsw_le32(record);
    *size = data->is4 ? gsw_le32(record + 4) : gsw_le16(record + 4);
    return TRUE;
}

static GBytes *
gsw_rawverse_read(GswModuleBackend *backend, GswKey *key, GError **error)
{
    GswRawVerseData *data = backend->data;
    gint testament = gsw_verse_key_get_testament((GswVerseKey *)key);
    guint32 start, size;
    guint8 *buffer;
    FILE *fp;

    if (!gsw_rawverse_find(backend, key, &start, &size, error))
        return NULL;

    if (size == 0)
        return g_bytes_new(NULL, 0);

    if (size > 64 * 1024 * 1024) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Verse entry is too large (%u bytes)", size);
        return NULL;
    }

    fp = (testament == 2) ? data->nt : data->ot;
    buffer = g_malloc(size + 1);
    if (!gsw_file_read_at(fp, start, buffer, size, error)) {
        g_free(buffer);
        return NULL;
    }
    buffer[size] = '\0';
    if (data->cipher_key != NULL) {
        GBytes *decrypted = NULL;
        const gchar *decrypted_data;
        gsize decrypted_len = 0;
        gchar *out;

        if (!gsw_cipher_decrypt(data->cipher_key, (const gchar *)buffer, size,
                                &decrypted, error)) {
            g_free(buffer);
            return NULL;
        }
        decrypted_data = g_bytes_get_data(decrypted, &decrypted_len);
        out = g_malloc(decrypted_len + 1);
        memcpy(out, decrypted_data, decrypted_len);
        out[decrypted_len] = '\0';
        g_bytes_unref(decrypted);
        g_free(buffer);
        return g_bytes_new_take(out, decrypted_len);
    }
    return g_bytes_new_take(buffer, size);
}

static gboolean
gsw_rawverse_has(GswModuleBackend *backend, GswKey *key)
{
    guint32 start, size;
    GError *error = NULL;

    if (!gsw_rawverse_find(backend, key, &start, &size, &error)) {
        g_clear_error(&error);
        return FALSE;
    }
    return size > 0;
}

static gboolean
gsw_rawverse_next(GswModuleBackend *backend, GswKey *key, gint steps,
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
gsw_rawverse_set_position(GswModuleBackend *backend, GswKey *key,
                          GswPosition pos, GError **error)
{
    GswRawVerseData *data = backend->data;
    gsize rec_size = data->is4 ? 8 : 6;
    glong ot_index, nt_index;

    if (pos == GSW_POSITION_TOP) {
        gsw_verse_key_set_index((GswVerseKey *)key, 0);
        return TRUE;
    }
    if (!gsw_rawverse_open(backend, error))
        return FALSE;
    if (!gsw_find_last_nonempty(data->ot_vss, rec_size, 4, data->is4,
                                &ot_index, error) ||
        !gsw_find_last_nonempty(data->nt_vss, rec_size, 4, data->is4,
                                &nt_index, error))
        return FALSE;

    return gsw_verse_key_set_last(key, ot_index, nt_index, error);
}

static void
gsw_rawverse_free(GswModuleBackend *backend)
{
    GswRawVerseData *data = backend->data;

    gsw_rawverse_close(backend);
    g_free(data);
    backend->data = NULL;
}

GswModuleBackend *
gsw_rawverse_backend_new(GswModuleInfo *info)
{
    GswModuleBackend *backend = g_new0(GswModuleBackend, 1);
    GswRawVerseData *data = g_new0(GswRawVerseData, 1);
    static const GswModuleBackendOps ops = {
        gsw_rawverse_open,
        gsw_rawverse_close,
        gsw_rawverse_read,
        gsw_rawverse_has,
        gsw_rawverse_next,
        gsw_rawverse_set_position,
        gsw_rawverse_free
    };

    data->is4 = (gsw_module_info_get_driver(info) == GSW_DRIVER_RAW_TEXT4 ||
                 gsw_module_info_get_driver(info) == GSW_DRIVER_RAW_COM4);
    backend->info = g_object_ref(info);
    backend->ops = &ops;
    backend->data = data;
    return backend;
}
