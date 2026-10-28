/*
 * gsw-rawstr.c: RawLD/RawLD4 backend
 */

#include "gsw-module-backend.h"
#include "gsw-backend-utils.h"
#include "gsw-data.h"
#include "gsw-utils.h"
#include "gsw-cipher.h"
#include <gsword/gsw-str-key.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    gboolean is4;
    gboolean case_sensitive;
    gboolean strongs_padding;
    FILE *idx;
    FILE *dat;
    gchar *cipher_key;
} GswRawStrData;

typedef struct {
    glong idx_offset;
    guint32 dat_offset;
    guint32 size;
    gchar *key; /* original key without line endings */
    gboolean exact;
} GswRawStrEntry;

static gsize
gsw_rawstr_rec_size(GswRawStrData *data)
{
    return data->is4 ? 8 : 6;
}

static gchar *
gsw_rawstr_normalize(GswRawStrData *data, const gchar *key)
{
    gchar *padded;
    gchar *upper;

    if (key == NULL)
        return g_strdup("");
    padded = data->strongs_padding ? gsw_strongs_pad(key) : g_strdup(key);
    if (data->case_sensitive)
        return padded;
    upper = g_utf8_strup(padded, -1);
    g_free(padded);
    return upper;
}

static void
gsw_rawstr_entry_clear(GswRawStrEntry *entry)
{
    g_free(entry->key);
    entry->key = NULL;
}

static gboolean
gsw_rawstr_read_key(GswRawStrData *data, guint32 dat_offset, gchar **key,
                    GError **error)
{
    GString *buffer = g_string_new(NULL);
    gint ch;

    if (fseeko(data->dat, (off_t)dat_offset, SEEK_SET) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Cannot seek dictionary data file");
        g_string_free(buffer, TRUE);
        return FALSE;
    }

    while ((ch = fgetc(data->dat)) != EOF) {
        if (ch == '\n' || ch == '\r' || ch == '\\')
            break;
        if (buffer->len > 4096) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Dictionary key is too long");
            g_string_free(buffer, TRUE);
            return FALSE;
        }
        g_string_append_c(buffer, (gchar)ch);
    }

    if (ch == EOF && buffer->len == 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Truncated dictionary key");
        g_string_free(buffer, TRUE);
        return FALSE;
    }

    *key = g_string_free(buffer, FALSE);
    return TRUE;
}

static gboolean
gsw_rawstr_read_entry(GswRawStrData *data, glong idx_offset,
                      GswRawStrEntry *entry, GError **error)
{
    guint8 record[8];
    gsize rec_size = gsw_rawstr_rec_size(data);

    memset(entry, 0, sizeof(*entry));
    entry->idx_offset = idx_offset;

    if (!gsw_file_read_at(data->idx, idx_offset, record, rec_size, error))
        return FALSE;

    entry->dat_offset = gsw_le32(record);
    entry->size = data->is4 ? gsw_le32(record + 4) : gsw_le16(record + 4);

    return gsw_rawstr_read_key(data, entry->dat_offset, &entry->key, error);
}

static gboolean
gsw_rawstr_find(GswRawStrData *data, const gchar *key, gint away,
                GswRawStrEntry *entry, GError **error)
{
    gchar *needle;
    gint64 file_size;
    glong count;
    glong low, high;
    glong found = -1;
    gboolean exact = FALSE;

    if (data->idx == NULL || data->dat == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "Dictionary files are not open");
        return FALSE;
    }

    needle = gsw_rawstr_normalize(data, key);
    file_size = gsw_file_size(data->idx);
    count = (glong)(file_size / (gint64)gsw_rawstr_rec_size(data));
    low = 0;
    high = count - 1;

    while (low <= high) {
        glong mid = low + (high - low) / 2;
        GswRawStrEntry candidate;

        if (!gsw_rawstr_read_entry(data, mid * (glong)gsw_rawstr_rec_size(data),
                                   &candidate, error)) {
            g_free(needle);
            return FALSE;
        }
        {
            gchar *candidate_key = gsw_rawstr_normalize(data, candidate.key);
            gint cmp = g_utf8_collate(needle, candidate_key);
            g_free(candidate_key);

            if (cmp == 0) {
                found = mid;
                exact = TRUE;
                candidate.exact = TRUE;
                *entry = candidate;
                break;
            }
            gsw_rawstr_entry_clear(&candidate);
            if (cmp < 0)
                high = mid - 1;
            else
                low = mid + 1;
        }
    }

    if (found < 0) {
        glong index = low + away;
        if (index < 0)
            index = 0;
        if (index >= count)
            index = count - 1;
        if (count <= 0 || !gsw_rawstr_read_entry(data,
                index * (glong)gsw_rawstr_rec_size(data), entry, error)) {
            g_free(needle);
            return FALSE;
        }
        entry->exact = FALSE;
    }

    g_free(needle);
    return exact || entry->key != NULL;
}

static gboolean
gsw_rawstr_open(GswModuleBackend *backend, GError **error)
{
    GswRawStrData *data = backend->data;
    gchar *base;
    gchar *path;

    if (backend->opened)
        return TRUE;

    base = gsw_module_data_base(backend->info);
    path = g_strconcat(base, ".idx", NULL);
    data->idx = fopen(path, "rb");
    g_free(path);
    path = g_strconcat(base, ".dat", NULL);
    data->dat = fopen(path, "rb");
    g_free(path);
    g_free(base);

    if (data->idx == NULL || data->dat == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "Cannot open dictionary data files");
        return FALSE;
    }
    if (gsw_module_info_is_ciphered(backend->info)) {
        const gchar *key = gsw_module_info_get_value(backend->info, "CipherKey");
        if (key == NULL || *key == '\0') {
            g_set_error(error, GSW_ERROR, GSW_ERROR_CIPHERED,
                        "Dictionary is encrypted but no CipherKey is configured");
            return FALSE;
        }
        data->cipher_key = g_strdup(key);
    }
    backend->opened = TRUE;
    return TRUE;
}

static void
gsw_rawstr_close(GswModuleBackend *backend)
{
    GswRawStrData *data = backend->data;

    if (data->idx) fclose(data->idx);
    if (data->dat) fclose(data->dat);
    data->idx = data->dat = NULL;
    g_free(data->cipher_key);
    data->cipher_key = NULL;
    backend->opened = FALSE;
}

static GBytes *
gsw_rawstr_read(GswModuleBackend *backend, GswKey *key, GError **error)
{
    GswRawStrData *data = backend->data;
    GswRawStrEntry entry;
    guint8 *buffer;
    gsize pos = 0;
    gsize value_size;
    const guint8 *value_ptr;
    GBytes *decrypted_value = NULL;
    const gchar *key_text;

    if (!gsw_rawstr_open(backend, error))
        return NULL;

    key_text = gsw_key_get_text(key);
    if (!gsw_rawstr_find(data, key_text, 0, &entry, error)) {
        gsw_rawstr_entry_clear(&entry);
        return NULL;
    }

    if (entry.key != NULL && g_strcmp0(gsw_key_get_text(key), entry.key) != 0)
        gsw_key_set_text(key, entry.key);

    buffer = g_malloc(entry.size + 1);
    if (!gsw_file_read_at(data->dat, entry.dat_offset, buffer, entry.size, error)) {
        g_free(buffer);
        gsw_rawstr_entry_clear(&entry);
        return NULL;
    }

    /* Skip the key line. */
    while (pos < entry.size && buffer[pos] != '\n')
        pos++;
    if (pos < entry.size)
        pos++;

    value_size = entry.size > pos ? entry.size - pos : 0;
    value_ptr = buffer + pos;
    if (data->cipher_key != NULL && value_size > 0) {
        const gchar *decrypted_data;
        gsize decrypted_len = 0;

        if (!gsw_cipher_decrypt(data->cipher_key, (const gchar *)value_ptr,
                                value_size, &decrypted_value, error)) {
            g_free(buffer);
            gsw_rawstr_entry_clear(&entry);
            return NULL;
        }
        decrypted_data = g_bytes_get_data(decrypted_value, &decrypted_len);
        value_ptr = (const guint8 *)decrypted_data;
        value_size = decrypted_len;
    }

    if (value_size >= 6 && !strncmp((const gchar *)value_ptr, "@LINK ", 6)) {
        gchar *target = g_strndup((const gchar *)value_ptr + 6, value_size - 6);
        gchar *newline = strpbrk(target, "\r\n");
        if (newline != NULL)
            *newline = '\0';
        g_free(buffer);
        g_clear_pointer(&decrypted_value, g_bytes_unref);
        gsw_rawstr_entry_clear(&entry);
        {
            GswKey *link_key = gsw_str_key_new(target);
            GBytes *linked = gsw_rawstr_read(backend, link_key, error);
            gsw_key_unref(link_key);
            g_free(target);
            return linked;
        }
    }

    {
        guint8 *value = g_malloc(value_size + 1);
        memcpy(value, value_ptr, value_size);
        value[value_size] = 0;
        g_free(buffer);
        g_clear_pointer(&decrypted_value, g_bytes_unref);
        gsw_rawstr_entry_clear(&entry);
        return g_bytes_new_take(value, value_size);
    }
}

static gboolean
gsw_rawstr_has(GswModuleBackend *backend, GswKey *key)
{
    GswRawStrData *data = backend->data;
    GswRawStrEntry entry;
    GError *error = NULL;
    gboolean result;

    if (!gsw_rawstr_open(backend, &error)) {
        g_clear_error(&error);
        return FALSE;
    }
    result = gsw_rawstr_find(data, gsw_key_get_text(key), 0, &entry, &error);
    if (result && entry.exact && entry.size > 0) {
        gsw_rawstr_entry_clear(&entry);
        return TRUE;
    }
    gsw_rawstr_entry_clear(&entry);
    g_clear_error(&error);
    return FALSE;
}

static gboolean
gsw_rawstr_move(GswModuleBackend *backend, GswKey *key, gint steps,
                GError **error)
{
    GswRawStrData *data = backend->data;
    GswRawStrEntry entry;
    glong count;
    glong index;

    if (!gsw_rawstr_open(backend, error))
        return FALSE;

    if (!gsw_rawstr_find(data, gsw_key_get_text(key), 0, &entry, error))
        return FALSE;

    count = (glong)(gsw_file_size(data->idx) /
                    (gint64)gsw_rawstr_rec_size(data));
    index = entry.idx_offset / (glong)gsw_rawstr_rec_size(data) + steps;
    gsw_rawstr_entry_clear(&entry);

    if (index < 0 || index >= count) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Dictionary key moved out of bounds");
        return FALSE;
    }

    if (!gsw_rawstr_read_entry(data, index * (glong)gsw_rawstr_rec_size(data),
                               &entry, error))
        return FALSE;
    gsw_key_set_text(key, entry.key);
    gsw_rawstr_entry_clear(&entry);
    return TRUE;
}

static gboolean
gsw_rawstr_set_position(GswModuleBackend *backend, GswKey *key,
                        GswPosition pos, GError **error)
{
    GswRawStrData *data = backend->data;
    GswRawStrEntry entry;
    glong count;

    if (!gsw_rawstr_open(backend, error))
        return FALSE;

    if (pos == GSW_POSITION_TOP) {
        gsw_key_set_text(key, "");
        return TRUE;
    }

    count = (glong)(gsw_file_size(data->idx) /
                    (gint64)gsw_rawstr_rec_size(data));
    if (count <= 0) {
        gsw_key_set_text(key, "");
        return TRUE;
    }
    if (!gsw_rawstr_read_entry(data, (count - 1) *
                               (glong)gsw_rawstr_rec_size(data),
                               &entry, error))
        return FALSE;
    gsw_key_set_text(key, entry.key);
    gsw_rawstr_entry_clear(&entry);
    return TRUE;
}

static void
gsw_rawstr_free(GswModuleBackend *backend)
{
    GswRawStrData *data = backend->data;

    gsw_rawstr_close(backend);
    g_free(data);
    backend->data = NULL;
}

GswModuleBackend *
gsw_rawstr_backend_new(GswModuleInfo *info)
{
    GswModuleBackend *backend = g_new0(GswModuleBackend, 1);
    GswRawStrData *data = g_new0(GswRawStrData, 1);
    static const GswModuleBackendOps ops = {
        gsw_rawstr_open,
        gsw_rawstr_close,
        gsw_rawstr_read,
        gsw_rawstr_has,
        gsw_rawstr_move,
        gsw_rawstr_set_position,
        gsw_rawstr_free
    };
    const gchar *case_sensitive = gsw_module_info_get_value(info, "CaseSensitiveKeys");
    const gchar *strongs = gsw_module_info_get_value(info, "StrongsPadding");

    data->is4 = (gsw_module_info_get_driver(info) == GSW_DRIVER_RAW_LD4);
    data->case_sensitive = gsw_str_is_true(case_sensitive);
    data->strongs_padding = strongs == NULL || gsw_str_is_true(strongs);
    backend->info = g_object_ref(info);
    backend->ops = &ops;
    backend->data = data;
    return backend;
}
