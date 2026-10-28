/*
 * gsw-zstr.c: zLD backend
 */

#include "gsw-module-backend.h"
#include "gsw-backend-utils.h"
#include "gsw-codec.h"
#include "gsw-data.h"
#include "gsw-utils.h"
#include <gsword/gsw-str-key.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    gint64 block;
    GBytes *data;
} GswZStrCache;

typedef struct {
    gboolean case_sensitive;
    gboolean strongs_padding;
    GswCompression compression;
    FILE *idx;
    FILE *dat;
    FILE *zdx;
    FILE *zdt;
    GswZStrCache cache;
} GswZStrData;

typedef struct {
    glong idx_offset;
    guint32 dat_offset;
    guint32 size;
    guint32 block;
    guint32 entry;
    gchar *key;
    gboolean exact;
} GswZStrEntry;

static gchar *
gsw_zstr_normalize(GswZStrData *data, const gchar *key)
{
    gchar *padded = data->strongs_padding ? gsw_strongs_pad(key) : g_strdup(key);
    gchar *upper;

    if (data->case_sensitive)
        return padded;
    upper = g_utf8_strup(padded, -1);
    g_free(padded);
    return upper;
}

static void
gsw_zstr_entry_clear(GswZStrEntry *entry)
{
    g_free(entry->key);
    entry->key = NULL;
}

static gboolean
gsw_zstr_read_entry(GswZStrData *data, glong idx_offset, GswZStrEntry *entry,
                    GError **error)
{
    guint8 record[8];
    GString *key;
    gint ch;

    memset(entry, 0, sizeof(*entry));
    entry->idx_offset = idx_offset;

    if (!gsw_file_read_at(data->idx, idx_offset, record, 8, error))
        return FALSE;

    entry->dat_offset = gsw_le32(record);
    entry->size = gsw_le32(record + 4);

    if (fseeko(data->dat, entry->dat_offset, SEEK_SET) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Cannot seek zLD data file");
        return FALSE;
    }

    key = g_string_new(NULL);
    while ((ch = fgetc(data->dat)) != EOF && ch != '\n' && ch != '\r') {
        if (key->len > 4096) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "zLD key is too long");
            g_string_free(key, TRUE);
            return FALSE;
        }
        g_string_append_c(key, (gchar)ch);
    }
    entry->key = g_string_free(key, FALSE);

    while (ch == '\r' || ch == '\n')
        ch = fgetc(data->dat);
    if (ch == EOF)
        return TRUE;

    /* We consumed the first byte of the block id; read the rest. */
    record[0] = (guint8)ch;
    if (fread(record + 1, 1, 7, data->dat) != 7) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Truncated zLD entry reference");
        return FALSE;
    }
    entry->block = gsw_le32(record);
    entry->entry = gsw_le32(record + 4);
    return TRUE;
}

static gboolean
gsw_zstr_find(GswZStrData *data, const gchar *key, GswZStrEntry *entry,
              GError **error)
{
    gchar *needle;
    gint64 size;
    glong count;
    glong low, high;
    gboolean exact = FALSE;

    needle = gsw_zstr_normalize(data, key);
    size = gsw_file_size(data->idx);
    count = (glong)(size / 8);
    low = 0;
    high = count - 1;

    while (low <= high) {
        glong mid = low + (high - low) / 2;
        GswZStrEntry candidate;

        if (!gsw_zstr_read_entry(data, mid * 8, &candidate, error)) {
            g_free(needle);
            return FALSE;
        }
        {
            gchar *candidate_key = gsw_zstr_normalize(data, candidate.key);
            gint cmp = g_utf8_collate(needle, candidate_key);
            g_free(candidate_key);

            if (cmp == 0) {
                candidate.exact = TRUE;
                *entry = candidate;
                exact = TRUE;
                break;
            }
            gsw_zstr_entry_clear(&candidate);
            if (cmp < 0)
                high = mid - 1;
            else
                low = mid + 1;
        }
    }

    if (!exact) {
        glong index = low;
        if (index < 0)
            index = 0;
        if (index >= count)
            index = count - 1;
        if (count <= 0 || !gsw_zstr_read_entry(data, index * 8, entry, error)) {
            g_free(needle);
            return FALSE;
        }
        entry->exact = FALSE;
    }

    g_free(needle);
    return TRUE;
}

static gboolean
gsw_zstr_load_block(GswZStrData *data, guint32 block, GError **error)
{
    guint8 record[8];
    guint32 offset, size;
    gchar *compressed;
    GBytes *out = NULL;

    if (data->cache.block == (gint64)block && data->cache.data != NULL)
        return TRUE;

    g_clear_pointer(&data->cache.data, g_bytes_unref);
    data->cache.block = -1;

    if (!gsw_file_read_at(data->zdx, (gint64)block * 8, record, 8, error))
        return FALSE;
    offset = gsw_le32(record);
    size = gsw_le32(record + 4);
    if (size > 128 * 1024 * 1024) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "zLD block is too large");
        return FALSE;
    }

    compressed = g_malloc(size + 1);
    if (!gsw_file_read_at(data->zdt, offset, compressed, size, error)) {
        g_free(compressed);
        return FALSE;
    }
    if (!gsw_decompress(data->compression, compressed, size, 0, &out, error)) {
        g_free(compressed);
        return FALSE;
    }
    g_free(compressed);

    data->cache.block = block;
    data->cache.data = out;
    return TRUE;
}

static GBytes *
gsw_zstr_get_entry(GswZStrData *data, guint32 block, guint32 index,
                   GError **error)
{
    gsize len;
    const guint8 *raw;
    guint32 count, offset, size;

    if (!gsw_zstr_load_block(data, block, error))
        return NULL;

    raw = g_bytes_get_data(data->cache.data, &len);
    if (len < 4) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "zLD block is too short");
        return NULL;
    }
    count = gsw_le32(raw);
    if (index >= count) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "zLD entry index is out of range");
        return NULL;
    }
    if (len < 4 + (gsize)index * 8 + 8) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "zLD block metadata is truncated");
        return NULL;
    }
    offset = gsw_le32(raw + 4 + (gsize)index * 8);
    size = gsw_le32(raw + 4 + (gsize)index * 8 + 4);
    if (offset == 0 || size == 0)
        return g_bytes_new(NULL, 0);
    if ((gsize)offset + size > len) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "zLD entry exceeds block data");
        return NULL;
    }
    return g_bytes_new(raw + offset, size > 0 ? size - 1 : 0);
}

static gboolean
gsw_zstr_open(GswModuleBackend *backend, GError **error)
{
    GswZStrData *data = backend->data;
    gchar *base;
    gchar *path;

    if (backend->opened)
        return TRUE;

    base = gsw_module_data_base(backend->info);
#define OPEN_Z(field, ext)                                                   \
    do {                                                                     \
        path = g_strconcat(base, ext, NULL);                                 \
        field = fopen(path, "rb");                                           \
        g_free(path);                                                        \
    } while (0)

    OPEN_Z(data->idx, ".idx");
    OPEN_Z(data->dat, ".dat");
    OPEN_Z(data->zdx, ".zdx");
    OPEN_Z(data->zdt, ".zdt");
#undef OPEN_Z
    g_free(base);

    if (!data->idx || !data->dat || !data->zdx || !data->zdt) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "Cannot open zLD data files");
        return FALSE;
    }
    backend->opened = TRUE;
    return TRUE;
}

static void
gsw_zstr_close(GswModuleBackend *backend)
{
    GswZStrData *data = backend->data;

    if (data->idx) fclose(data->idx);
    if (data->dat) fclose(data->dat);
    if (data->zdx) fclose(data->zdx);
    if (data->zdt) fclose(data->zdt);
    data->idx = data->dat = data->zdx = data->zdt = NULL;
    g_clear_pointer(&data->cache.data, g_bytes_unref);
    data->cache.block = -1;
    backend->opened = FALSE;
}

static GBytes *
gsw_zstr_read(GswModuleBackend *backend, GswKey *key, GError **error)
{
    GswZStrData *data = backend->data;
    GswZStrEntry entry;
    GBytes *result;

    if (!gsw_zstr_open(backend, error))
        return NULL;
    if (!gsw_zstr_find(data, gsw_key_get_text(key), &entry, error)) {
        gsw_zstr_entry_clear(&entry);
        return NULL;
    }
    if (entry.key != NULL && g_strcmp0(gsw_key_get_text(key), entry.key) != 0)
        gsw_key_set_text(key, entry.key);
    result = gsw_zstr_get_entry(data, entry.block, entry.entry, error);
    gsw_zstr_entry_clear(&entry);
    return result;
}

static gboolean
gsw_zstr_has(GswModuleBackend *backend, GswKey *key)
{
    GswZStrData *data = backend->data;
    GswZStrEntry entry;
    GError *error = NULL;
    gboolean result;

    if (!gsw_zstr_open(backend, &error)) {
        g_clear_error(&error);
        return FALSE;
    }
    result = gsw_zstr_find(data, gsw_key_get_text(key), &entry, &error);
    if (result && entry.exact && entry.size > 0) {
        gsw_zstr_entry_clear(&entry);
        return TRUE;
    }
    gsw_zstr_entry_clear(&entry);
    g_clear_error(&error);
    return FALSE;
}

static gboolean
gsw_zstr_move(GswModuleBackend *backend, GswKey *key, gint steps,
              GError **error)
{
    GswZStrData *data = backend->data;
    GswZStrEntry entry;
    glong count;
    glong index;

    if (!gsw_zstr_open(backend, error))
        return FALSE;
    if (!gsw_zstr_find(data, gsw_key_get_text(key), &entry, error))
        return FALSE;

    count = (glong)(gsw_file_size(data->idx) / 8);
    index = entry.idx_offset / 8 + steps;
    gsw_zstr_entry_clear(&entry);
    if (index < 0 || index >= count) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "zLD key moved out of bounds");
        return FALSE;
    }
    if (!gsw_zstr_read_entry(data, index * 8, &entry, error))
        return FALSE;
    gsw_key_set_text(key, entry.key);
    gsw_zstr_entry_clear(&entry);
    return TRUE;
}

static gboolean
gsw_zstr_set_position(GswModuleBackend *backend, GswKey *key,
                      GswPosition pos, GError **error)
{
    GswZStrData *data = backend->data;
    GswZStrEntry entry;
    glong count;

    if (!gsw_zstr_open(backend, error))
        return FALSE;

    if (pos == GSW_POSITION_TOP) {
        gsw_key_set_text(key, "");
        return TRUE;
    }

    count = (glong)(gsw_file_size(data->idx) / 8);
    if (count <= 0) {
        gsw_key_set_text(key, "");
        return TRUE;
    }
    if (!gsw_zstr_read_entry(data, (count - 1) * 8, &entry, error))
        return FALSE;
    gsw_key_set_text(key, entry.key);
    gsw_zstr_entry_clear(&entry);
    return TRUE;
}

static void
gsw_zstr_free(GswModuleBackend *backend)
{
    GswZStrData *data = backend->data;

    gsw_zstr_close(backend);
    g_free(data);
    backend->data = NULL;
}

GswModuleBackend *
gsw_zstr_backend_new(GswModuleInfo *info)
{
    GswModuleBackend *backend = g_new0(GswModuleBackend, 1);
    GswZStrData *data = g_new0(GswZStrData, 1);
    static const GswModuleBackendOps ops = {
        gsw_zstr_open,
        gsw_zstr_close,
        gsw_zstr_read,
        gsw_zstr_has,
        gsw_zstr_move,
        gsw_zstr_set_position,
        gsw_zstr_free
    };

    data->case_sensitive = gsw_str_is_true(
        gsw_module_info_get_value(info, "CaseSensitiveKeys"));
    {
        const gchar *strongs = gsw_module_info_get_value(info, "StrongsPadding");
        data->strongs_padding = strongs == NULL || gsw_str_is_true(strongs);
    }
    data->compression = gsw_compression_from_string(
        gsw_module_info_get_value(info, "CompressType"));
    data->cache.block = -1;
    backend->info = g_object_ref(info);
    backend->ops = &ops;
    backend->data = data;
    return backend;
}
