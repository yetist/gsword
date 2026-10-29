/*
 * gsw-hrefcom.c: HREFCom backend (entry is a URL/path)
 */

#include "gsw-module-backend.h"
#include "gsw-backend-utils.h"
#include "gsw-data.h"
#include "gsw-key-internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    gchar *data_dir;
    gchar *prefix;
    FILE *ot;
    FILE *nt;
    FILE *ot_vss;
    FILE *nt_vss;
} GswHrefComData;

static gboolean
gsw_hrefcom_open(GswModuleBackend *backend, GError **error)
{
    GswHrefComData *data = backend->data;
    gchar *path;

    if (backend->opened)
        return TRUE;

    data->data_dir = gsw_module_data_dir(backend->info);
    data->prefix = g_strdup(gsw_module_info_get_value(backend->info, "Prefix"));
    if (data->prefix == NULL)
        data->prefix = g_strdup("");

#define OPEN_FILE(field, name)                                               \
    do {                                                                     \
        path = g_build_filename(data->data_dir, name, NULL);                 \
        field = fopen(path, "rb");                                           \
        if (field == NULL) {                                                 \
            g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,               \
                        "Cannot open HREFCom file %s: %s", path,              \
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
gsw_hrefcom_close(GswModuleBackend *backend)
{
    GswHrefComData *data = backend->data;

    if (data->ot) fclose(data->ot);
    if (data->nt) fclose(data->nt);
    if (data->ot_vss) fclose(data->ot_vss);
    if (data->nt_vss) fclose(data->nt_vss);
    data->ot = data->nt = data->ot_vss = data->nt_vss = NULL;
    g_free(data->data_dir);
    g_free(data->prefix);
    data->data_dir = data->prefix = NULL;
    backend->opened = FALSE;
}

static gboolean
gsw_hrefcom_find(GswModuleBackend *backend, GswKey *key,
                 guint32 *start, guint32 *size, GError **error)
{
    GswHrefComData *data = backend->data;
    gint testament = gsw_verse_key_get_testament((GswVerseKey *)key);
    glong index = gsw_verse_key_get_testament_index((GswVerseKey *)key);
    FILE *idx;
    guint8 record[6];

    if (!backend->opened && !gsw_hrefcom_open(backend, error))
        return FALSE;

    idx = testament == 2 ? data->nt_vss : data->ot_vss;
    if (index < 0 || !gsw_file_read_at(idx, (gint64)index * 6,
                                       record, 6, error))
        return FALSE;

    *start = gsw_le32(record);
    *size = gsw_le16(record + 4);
    return TRUE;
}

static GBytes *
gsw_hrefcom_read(GswModuleBackend *backend, GswKey *key, GError **error)
{
    GswHrefComData *data = backend->data;
    guint32 start, size;
    gchar *entry;
    GString *result;
    gsize len;
    FILE *fp;

    if (!gsw_hrefcom_find(backend, key, &start, &size, error))
        return NULL;
    if (size == 0)
        return g_bytes_new(NULL, 0);

    fp = gsw_verse_key_get_testament((GswVerseKey *)key) == 2
       ? data->nt : data->ot;
    entry = g_malloc(size + 1);
    if (!gsw_file_read_at(fp, start, entry, size, error)) {
        g_free(entry);
        return NULL;
    }
    entry[size] = '\0';

    result = g_string_new(data->prefix);
    g_string_append(result, entry);
    len = result->len;
    g_free(entry);
    return g_bytes_new_take(g_string_free(result, FALSE), len);
}

static gboolean
gsw_hrefcom_has(GswModuleBackend *backend, GswKey *key)
{
    guint32 start, size;
    GError *error = NULL;

    if (!gsw_hrefcom_find(backend, key, &start, &size, &error)) {
        g_clear_error(&error);
        return FALSE;
    }
    return size > 0;
}

static gboolean
gsw_hrefcom_next(GswModuleBackend *backend, GswKey *key, gint steps,
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
gsw_hrefcom_set_position(GswModuleBackend *backend, GswKey *key,
                         GswPosition pos, GError **error)
{
    GswHrefComData *data = backend->data;
    glong ot_index, nt_index;

    if (pos == GSW_POSITION_TOP) {
        gsw_verse_key_set_index((GswVerseKey *)key, 0);
        return TRUE;
    }
    if (!gsw_hrefcom_open(backend, error))
        return FALSE;
    if (!gsw_find_last_nonempty(data->ot_vss, 6, 4, FALSE, &ot_index,
                                error) ||
        !gsw_find_last_nonempty(data->nt_vss, 6, 4, FALSE, &nt_index,
                                error))
        return FALSE;

    return gsw_verse_key_set_last(key, ot_index, nt_index, error);
}

static void
gsw_hrefcom_free(GswModuleBackend *backend)
{
    GswHrefComData *data = backend->data;

    gsw_hrefcom_close(backend);
    g_free(data);
    backend->data = NULL;
}

GswModuleBackend *
gsw_hrefcom_backend_new(GswModuleInfo *info)
{
    GswModuleBackend *backend = g_new0(GswModuleBackend, 1);
    GswHrefComData *data = g_new0(GswHrefComData, 1);
    static const GswModuleBackendOps ops = {
        gsw_hrefcom_open,
        gsw_hrefcom_close,
        gsw_hrefcom_read,
        gsw_hrefcom_has,
        gsw_hrefcom_next,
        gsw_hrefcom_set_position,
        NULL,
        NULL,
        NULL,
        gsw_hrefcom_free
    };

    backend->info = g_object_ref(info);
    backend->ops = &ops;
    backend->data = data;
    return backend;
}
