/*
 * gsw-rawgenbook.c: RawGenBook backend
 */

#include "gsw-module-backend.h"
#include "gsw-backend-utils.h"
#include "gsw-data.h"
#include "gsw-key-private.h"
#include "gsw-cipher.h"
#include "gsw-lock.h"

#include <errno.h>
#include <glib/gstdio.h>
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

static gboolean
gsw_rawgenbook_write_entry(GswModuleBackend *backend, GswKey *key,
                           const guint8 *bytes, gsize len, GError **error)
{
    gchar *base;
    gchar *path;
    gchar *lock_path;
    gint lock_fd = -1;
    GStatBuf st;
    FILE *out;
    guint8 user_data[8];
    gboolean ok = FALSE;

    if (gsw_module_info_is_ciphered(backend->info)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CIPHERED,
                    "Writing encrypted general books is not supported");
        return FALSE;
    }
    if (!gsw_rawgenbook_open(backend, error))
        return FALSE;
    if (len > G_MAXUINT32) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "General book entry is too large");
        return FALSE;
    }

    base = gsw_module_data_base(backend->info);
    lock_path = g_strconcat(base, ".lock", NULL);
    lock_fd = gsw_file_lock(lock_path, error);
    g_free(lock_path);
    if (lock_fd < 0) {
        g_free(base);
        return FALSE;
    }
    path = g_strconcat(base, ".bdt", NULL);
    g_free(base);
    if (g_stat(path, &st) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot stat general book data %s: %s", path,
                    g_strerror(errno));
        goto out;
    }
    user_data[0] = (guint8)(st.st_size & 0xff);
    user_data[1] = (guint8)((st.st_size >> 8) & 0xff);
    user_data[2] = (guint8)((st.st_size >> 16) & 0xff);
    user_data[3] = (guint8)((st.st_size >> 24) & 0xff);
    user_data[4] = (guint8)(len & 0xff);
    user_data[5] = (guint8)((len >> 8) & 0xff);
    user_data[6] = (guint8)((len >> 16) & 0xff);
    user_data[7] = (guint8)((len >> 24) & 0xff);

    out = fopen(path, "ab");
    if (out == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot open general book data %s: %s", path,
                    g_strerror(errno));
        goto out;
    }
    if (len > 0 && fwrite(bytes, 1, len, out) != len) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot write general book data %s: %s", path,
                    g_strerror(errno));
        fclose(out);
        goto out;
    }
    if (fflush(out) != 0 || fclose(out) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot flush general book data %s: %s", path,
                    g_strerror(errno));
        goto out;
    }
    if (!gsw_tree_key_set_user_data(key, user_data, 8)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot update general book node data");
        goto out;
    }
    gsw_rawgenbook_close(backend);
    gsw_rawgenbook_open(backend, NULL);
    ok = TRUE;

out:
    gsw_file_unlock(lock_fd);
    g_free(path);
    return ok;
}

static gboolean
gsw_rawgenbook_delete_entry(GswModuleBackend *backend, GswKey *key,
                            GError **error)
{
    const guint8 empty[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    gchar *base;
    gchar *lock_path;
    gint lock_fd;
    gboolean ok;

    base = gsw_module_data_base(backend->info);
    lock_path = g_strconcat(base, ".lock", NULL);
    g_free(base);
    lock_fd = gsw_file_lock(lock_path, error);
    g_free(lock_path);
    if (lock_fd < 0)
        return FALSE;

    ok = gsw_tree_key_set_user_data(key, empty, 8);
    gsw_file_unlock(lock_fd);
    if (!ok) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot clear general book entry");
        return FALSE;
    }
    return TRUE;
}

static gboolean
gsw_rawgenbook_is_writable(GswModuleBackend *backend, GswKey *key)
{
    (void)backend;
    (void)key;
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
        gsw_rawgenbook_write_entry,
        gsw_rawgenbook_delete_entry,
        gsw_rawgenbook_is_writable,
        gsw_rawgenbook_free
    };

    backend->info = g_object_ref(info);
    backend->ops = &ops;
    backend->data = data;
    return backend;
}
