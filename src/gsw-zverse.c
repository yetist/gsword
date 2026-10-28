/*
 * gsw-zverse.c: zText/zCom backend
 */

#include "gsw-module-backend.h"
#include "gsw-codec.h"
#include "gsw-data.h"
#include "gsw-key-internal.h"
#include "gsw-cipher.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    gint64 block;
    GBytes *data;
} GswZCache;

typedef struct {
    gboolean is4;
    gchar letter;
    GswCompression compression;
    FILE *index[2];
    FILE *text[2];
    FILE *comp[2];
    GswZCache cache[2][4];
    guint next[2];
    gchar *cipher_key;
} GswZVerseData;

static gchar
gsw_zverse_letter(GswModuleInfo *info)
{
    const gchar *block_type = gsw_module_info_get_value(info, "BlockType");

    if (block_type != NULL && g_ascii_strcasecmp(block_type, "VERSE") == 0)
        return 'v';
    if (block_type != NULL && g_ascii_strcasecmp(block_type, "BOOK") == 0)
        return 'b';
    return 'c';
}

static gboolean
gsw_zverse_open_one(GswModuleBackend *backend, gint testament, GError **error)
{
    GswZVerseData *data = backend->data;
    const gchar *path = gsw_module_info_get_absolute_data_path(backend->info);
    gchar *name;
    const gchar *prefix = testament == 2 ? "nt" : "ot";
    gint i = testament == 2 ? 1 : 0;

    {
        gchar *file;

        name = g_strdup_printf("%s.%czv", prefix, data->letter);
        file = g_build_filename(path, name, NULL);
        data->comp[i] = fopen(file, "rb");
        g_free(file);
        g_free(name);
        if (data->comp[i] == NULL)
            goto fail;

        name = g_strdup_printf("%s.%czs", prefix, data->letter);
        file = g_build_filename(path, name, NULL);
        data->index[i] = fopen(file, "rb");
        g_free(file);
        g_free(name);
        if (data->index[i] == NULL)
            goto fail;

        name = g_strdup_printf("%s.%czz", prefix, data->letter);
        file = g_build_filename(path, name, NULL);
        data->text[i] = fopen(file, "rb");
        g_free(file);
        g_free(name);
        if (data->text[i] == NULL)
            goto fail;
    }

    return TRUE;

fail:
    g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                "Cannot open compressed module data for %s", prefix);
    return FALSE;
}

static gboolean
gsw_zverse_open(GswModuleBackend *backend, GError **error)
{
    GswZVerseData *data = backend->data;

    if (backend->opened)
        return TRUE;
    if (gsw_module_info_is_ciphered(backend->info)) {
        const gchar *key = gsw_module_info_get_value(backend->info, "CipherKey");
        if (key == NULL || *key == '\0') {
            g_set_error(error, GSW_ERROR, GSW_ERROR_CIPHERED,
                        "Module is encrypted but no CipherKey is configured");
            return FALSE;
        }
        data->cipher_key = g_strdup(key);
    }
    if (!gsw_zverse_open_one(backend, 1, error))
        return FALSE;
    if (!gsw_zverse_open_one(backend, 2, error))
        return FALSE;
    backend->opened = TRUE;
    return TRUE;
}

static void
gsw_zverse_close(GswModuleBackend *backend)
{
    GswZVerseData *data = backend->data;
    gint i;

    for (i = 0; i < 2; i++) {
        guint j;

        if (data->index[i]) fclose(data->index[i]);
        if (data->text[i]) fclose(data->text[i]);
        if (data->comp[i]) fclose(data->comp[i]);
        data->index[i] = data->text[i] = data->comp[i] = NULL;
        for (j = 0; j < 4; j++) {
            g_clear_pointer(&data->cache[i][j].data, g_bytes_unref);
            data->cache[i][j].block = -1;
        }
        data->next[i] = 0;
    }
    g_free(data->cipher_key);
    data->cipher_key = NULL;
    backend->opened = FALSE;
}

static gboolean
gsw_zverse_load_block(GswModuleBackend *backend, gint testament,
                      guint32 block, GBytes **out, GError **error)
{
    GswZVerseData *data = backend->data;
    gint i = testament - 1;
    guint j;
    guint slot;
    guint8 record[12];
    guint32 comp_offset, comp_size, uncomp_size;
    gchar *compressed;
    GBytes *uncompressed = NULL;

    for (j = 0; j < 4; j++) {
        if (data->cache[i][j].block == (gint64)block &&
            data->cache[i][j].data != NULL) {
            *out = data->cache[i][j].data;
            return TRUE;
        }
    }

    slot = data->next[i]++ % 4;
    g_clear_pointer(&data->cache[i][slot].data, g_bytes_unref);
    data->cache[i][slot].block = -1;

    if (!gsw_file_read_at(data->index[i], (gint64)block * 12, record, 12, error))
        return FALSE;

    comp_offset = gsw_le32(record);
    comp_size = gsw_le32(record + 4);
    uncomp_size = gsw_le32(record + 8);

    if (comp_size > 128 * 1024 * 1024) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Compressed block is too large");
        return FALSE;
    }

    compressed = g_malloc(comp_size + 1);
    if (!gsw_file_read_at(data->text[i], comp_offset, compressed, comp_size, error)) {
        g_free(compressed);
        return FALSE;
    }

    if (data->cipher_key != NULL) {
        GBytes *decrypted = NULL;
        gsize decrypted_len = 0;
        const gchar *decrypted_data;

        if (!gsw_cipher_decrypt(data->cipher_key, compressed, comp_size,
                                &decrypted, error)) {
            g_free(compressed);
            return FALSE;
        }
        decrypted_data = g_bytes_get_data(decrypted, &decrypted_len);
        g_free(compressed);
        compressed = g_malloc(decrypted_len + 1);
        memcpy(compressed, decrypted_data, decrypted_len);
        compressed[decrypted_len] = '\0';
        comp_size = (guint32)decrypted_len;
        g_bytes_unref(decrypted);
    }

    if (!gsw_decompress(data->compression, compressed, comp_size,
                        uncomp_size, &uncompressed, error)) {
        g_free(compressed);
        return FALSE;
    }
    g_free(compressed);

    data->cache[i][slot].block = block;
    data->cache[i][slot].data = uncompressed;
    *out = uncompressed;
    return TRUE;
}

static gboolean
gsw_zverse_find(GswModuleBackend *backend, GswKey *key,
                guint32 *block, guint32 *start, guint32 *size,
                GError **error)
{
    GswZVerseData *data = backend->data;
    gint testament = gsw_verse_key_get_testament((GswVerseKey *)key);
    glong index = gsw_verse_key_get_testament_index((GswVerseKey *)key);
    guint8 record[12];
    gsize rec_size = data->is4 ? 12 : 10;
    gint i;

    if (!backend->opened && !gsw_zverse_open(backend, error))
        return FALSE;

    if (testament == 2)
        i = 1;
    else {
        testament = 1;
        i = 0;
    }

    if (index < 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Negative verse index");
        return FALSE;
    }

    if ((gint64)index * (gint64)rec_size + (gint64)rec_size >
        gsw_file_size(data->comp[i])) {
        *block = 0;
        *start = 0;
        *size = 0;
        return TRUE;
    }
    if (!gsw_file_read_at(data->comp[i], (gint64)index * (gint64)rec_size,
                          record, rec_size, error))
        return FALSE;

    *block = gsw_le32(record);
    *start = gsw_le32(record + 4);
    *size = data->is4 ? gsw_le32(record + 8) : gsw_le16(record + 8);
    return TRUE;
}

static GBytes *
gsw_zverse_read(GswModuleBackend *backend, GswKey *key, GError **error)
{
    gint testament = gsw_verse_key_get_testament((GswVerseKey *)key);
    guint32 block, start, size;
    gsize len;
    const guint8 *data_ptr;
    GBytes *block_data = NULL;

    if (!gsw_zverse_find(backend, key, &block, &start, &size, error))
        return NULL;
    if (size == 0)
        return g_bytes_new(NULL, 0);

    if (testament == 2)
        testament = 2;
    else
        testament = 1;

    if (!gsw_zverse_load_block(backend, testament, block, &block_data, error))
        return NULL;

    data_ptr = g_bytes_get_data(block_data, &len);
    if ((gsize)start + size > len) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Verse entry exceeds decompressed block");
        return NULL;
    }

    return g_bytes_new(data_ptr + start, size);
}

static gboolean
gsw_zverse_has(GswModuleBackend *backend, GswKey *key)
{
    guint32 block, start, size;
    GError *error = NULL;

    if (!gsw_zverse_find(backend, key, &block, &start, &size, &error)) {
        g_clear_error(&error);
        return FALSE;
    }
    return size > 0;
}

static gboolean
gsw_zverse_next(GswModuleBackend *backend, GswKey *key, gint steps,
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
gsw_zverse_set_position(GswModuleBackend *backend, GswKey *key,
                        GswPosition pos, GError **error)
{
    (void)backend;
    if (pos == GSW_POSITION_TOP) {
        gsw_verse_key_set_index((GswVerseKey *)key, 0);
        return TRUE;
    }
    g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                "Bottom position is not implemented for zVerse");
    return FALSE;
}

static void
gsw_zverse_free(GswModuleBackend *backend)
{
    GswZVerseData *data = backend->data;

    gsw_zverse_close(backend);
    g_free(data);
    backend->data = NULL;
}

GswModuleBackend *
gsw_zverse_backend_new(GswModuleInfo *info)
{
    GswModuleBackend *backend = g_new0(GswModuleBackend, 1);
    GswZVerseData *data = g_new0(GswZVerseData, 1);
    static const GswModuleBackendOps ops = {
        gsw_zverse_open,
        gsw_zverse_close,
        gsw_zverse_read,
        gsw_zverse_has,
        gsw_zverse_next,
        gsw_zverse_set_position,
        gsw_zverse_free
    };
    gint i;

    data->is4 = (gsw_module_info_get_driver(info) == GSW_DRIVER_Z_TEXT4 ||
                 gsw_module_info_get_driver(info) == GSW_DRIVER_Z_COM4);
    data->letter = gsw_zverse_letter(info);
    data->compression = gsw_compression_from_string(
        gsw_module_info_get_value(info, "CompressType"));
    for (i = 0; i < 2; i++) {
        guint j;
        for (j = 0; j < 4; j++)
            data->cache[i][j].block = -1;
        data->next[i] = 0;
    }

    backend->info = g_object_ref(info);
    backend->ops = &ops;
    backend->data = data;
    return backend;
}
