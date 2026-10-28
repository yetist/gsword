/*
 * gsw-rawgenbook.c: RawGenBook backend
 */

#include "gsw-module-backend.h"
#include "gsw-backend-utils.h"
#include "gsw-data.h"
#include "gsw-key-private.h"
#include "gsw-cipher.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    FILE *bdt;
    gchar *cipher_key;
} GswRawGenBookData;

static gboolean
gsw_rawgenbook_open(GswModuleBackend *backend, GError **error)
{
    GswRawGenBookData *data = backend->data;
    gchar *base;
    gchar *path;

    if (backend->opened)
        return TRUE;

    base = gsw_module_data_base(backend->info);
    path = g_strconcat(base, ".bdt", NULL);
    g_free(base);
    data->bdt = fopen(path, "rb");
    if (data->bdt == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "Cannot open general book data file %s: %s",
                    path, g_strerror(errno));
        g_free(path);
        return FALSE;
    }
    g_free(path);
    if (gsw_module_info_is_ciphered(backend->info)) {
        const gchar *key = gsw_module_info_get_value(backend->info, "CipherKey");
        if (key == NULL || *key == '\0') {
            g_set_error(error, GSW_ERROR, GSW_ERROR_CIPHERED,
                        "General book is encrypted but no CipherKey is configured");
            return FALSE;
        }
        data->cipher_key = g_strdup(key);
    }
    backend->opened = TRUE;
    return TRUE;
}

static void
gsw_rawgenbook_close(GswModuleBackend *backend)
{
    GswRawGenBookData *data = backend->data;

    if (data->bdt) fclose(data->bdt);
    data->bdt = NULL;
    g_free(data->cipher_key);
    data->cipher_key = NULL;
    backend->opened = FALSE;
}

static GBytes *
gsw_rawgenbook_read(GswModuleBackend *backend, GswKey *key, GError **error)
{
    GswRawGenBookData *data = backend->data;
    const gchar *user_data;
    gint user_size = 0;
    guint32 offset, size;
    guint8 *buffer;

    if (!gsw_rawgenbook_open(backend, error))
        return NULL;

    user_data = gsw_tree_key_get_user_data(key, &user_size);
    if (user_data == NULL || user_size < 8)
        return g_bytes_new(NULL, 0);

    offset = gsw_le32((const guint8 *)user_data);
    size = gsw_le32((const guint8 *)user_data + 4);
    if (size == 0)
        return g_bytes_new(NULL, 0);
    if (size > 64 * 1024 * 1024) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "General book entry is too large");
        return NULL;
    }

    buffer = g_malloc(size + 1);
    if (!gsw_file_read_at(data->bdt, offset, buffer, size, error)) {
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
gsw_rawgenbook_has(GswModuleBackend *backend, GswKey *key)
{
    const gchar *user_data;
    gint user_size = 0;

    (void)backend;
    user_data = gsw_tree_key_get_user_data(key, &user_size);
    if (user_data == NULL || user_size < 8)
        return FALSE;
    return gsw_le32((const guint8 *)user_data + 4) > 0;
}

static gboolean
gsw_rawgenbook_next(GswModuleBackend *backend, GswKey *key, gint steps,
                    GError **error)
{
    (void)backend;
    if (!gsw_tree_key_increment(key, steps)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Tree key moved out of bounds");
        return FALSE;
    }
    return TRUE;
}

static gboolean
gsw_rawgenbook_set_position(GswModuleBackend *backend, GswKey *key,
                            GswPosition pos, GError **error)
{
    (void)backend;
    if (!gsw_tree_key_set_position(key, pos)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Cannot position tree key");
        return FALSE;
    }
    return TRUE;
}

static void
gsw_rawgenbook_free(GswModuleBackend *backend)
{
    GswRawGenBookData *data = backend->data;

    gsw_rawgenbook_close(backend);
    g_free(data);
    backend->data = NULL;
}

GswModuleBackend *
gsw_rawgenbook_backend_new(GswModuleInfo *info)
{
    GswModuleBackend *backend = g_new0(GswModuleBackend, 1);
    GswRawGenBookData *data = g_new0(GswRawGenBookData, 1);
    static const GswModuleBackendOps ops = {
        gsw_rawgenbook_open,
        gsw_rawgenbook_close,
        gsw_rawgenbook_read,
        gsw_rawgenbook_has,
        gsw_rawgenbook_next,
        gsw_rawgenbook_set_position,
        gsw_rawgenbook_free
    };

    backend->info = g_object_ref(info);
    backend->ops = &ops;
    backend->data = data;
    return backend;
}
