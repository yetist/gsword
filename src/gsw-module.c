/*
 * gsw-module.c: readable module object
 */

#include <gsword/gsw-module.h>
#include <gsword/gsw-error.h>
#include "gsw-key-internal.h"
#include "gsw-module-backend.h"
#include "gsw-module-internal.h"
#include "gsw-backend-utils.h"
#include <gsword/gsw-str-key.h>
#include <gsword/gsw-tree-key.h>

#include <string.h>

struct _GswModule {
    GObject parent_instance;
};

typedef struct {
    GswModuleInfo *info;
    GswModuleBackend *backend;
    GswKey *key;
    GBytes *raw;
    gchar *raw_text;
    gsize raw_len;
    GError *backend_error;
} GswModulePrivate;

G_DEFINE_TYPE(GswModule, gsw_module, G_TYPE_OBJECT)

static GswModulePrivate *
gsw_module_priv(GswModule *self)
{
    static GQuark quark = 0;

    if (quark == 0)
        quark = g_quark_from_static_string("gsw-module-private");

    {
        GswModulePrivate *priv = g_object_get_qdata(G_OBJECT(self), quark);
        if (priv == NULL) {
            priv = g_new0(GswModulePrivate, 1);
            g_object_set_qdata_full(G_OBJECT(self), quark, priv, g_free);
        }
        return priv;
    }
}

static void
gsw_module_finalize(GObject *object)
{
    GswModule *self = GSW_MODULE(object);
    GswModulePrivate *priv = gsw_module_priv(self);

    g_clear_object(&priv->info);
    if (priv->backend != NULL)
        gsw_module_backend_free(priv->backend);
    g_clear_pointer(&priv->key, gsw_key_unref);
    g_clear_pointer(&priv->raw, g_bytes_unref);
    g_free(priv->raw_text);
    g_clear_error(&priv->backend_error);

    G_OBJECT_CLASS(gsw_module_parent_class)->finalize(object);
}

static void
gsw_module_class_init(GswModuleClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = gsw_module_finalize;
}

static void
gsw_module_init(GswModule *self)
{
    (void)gsw_module_priv(self);
}

static GswKey *
gsw_module_new_key(GswModuleInfo *info)
{
    GswDriver driver = gsw_module_info_get_driver(info);

    switch (driver) {
    case GSW_DRIVER_RAW_LD:
    case GSW_DRIVER_RAW_LD4:
    case GSW_DRIVER_Z_LD:
        return gsw_key_new_string("");
    case GSW_DRIVER_RAW_GENBOOK:
        {
            gchar *base = gsw_module_data_base(info);
            GswKey *key = (GswKey *)gsw_tree_key_new(base);
            g_free(base);
            return key;
        }
    default:
        return gsw_key_new_verse("", gsw_module_info_get_versification(info));
    }
}

GswModule *
gsw_module_new_from_info(GswModuleInfo *info)
{
    GswModule *self;
    GswModulePrivate *priv;
    GError *error = NULL;

    g_return_val_if_fail(GSW_IS_MODULE_INFO(info), NULL);

    self = g_object_new(GSW_TYPE_MODULE, NULL);
    priv = gsw_module_priv(self);
    priv->info = g_object_ref(info);
    priv->key = gsw_module_new_key(info);

    priv->backend = gsw_module_backend_new(info, &error);
    if (priv->backend == NULL) {
        priv->backend_error = error;
        return self;
    }

    return self;
}

GswModuleInfo *
gsw_module_get_info(GswModule *self)
{
    return gsw_module_priv(self)->info;
}

const gchar *
gsw_module_get_name(GswModule *self)
{
    return gsw_module_info_get_name(gsw_module_priv(self)->info);
}

gboolean
gsw_module_is_open(GswModule *self)
{
    GswModulePrivate *priv = gsw_module_priv(self);
    return priv->backend != NULL && priv->backend->opened;
}

gboolean
gsw_module_is_supported(GswModule *self)
{
    GswModulePrivate *priv = gsw_module_priv(self);
    return priv->backend != NULL;
}

static gboolean
gsw_module_ensure_open(GswModule *self, GError **error)
{
    GswModulePrivate *priv = gsw_module_priv(self);

    if (priv->backend == NULL) {
        if (priv->backend_error != NULL)
            g_set_error(error, priv->backend_error->domain,
                        priv->backend_error->code, "%s",
                        priv->backend_error->message);
        else
            g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                        "Module driver is not supported");
        return FALSE;
    }
    if (priv->backend->opened)
        return TRUE;
    if (!priv->backend->ops->open(priv->backend, error))
        return FALSE;
    return TRUE;
}

GswKey *
gsw_module_create_key(GswModule *self)
{
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);
    priv = gsw_module_priv(self);
    return gsw_module_new_key(priv->info);
}

GswKey *
gsw_module_get_key(GswModule *self)
{
    return gsw_module_priv(self)->key;
}

gboolean
gsw_module_set_key(GswModule *self, GswKey *key, GError **error)
{
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), FALSE);
    g_return_val_if_fail(key != NULL, FALSE);

    priv = gsw_module_priv(self);
    if (!gsw_key_set_text(priv->key, gsw_key_get_text(key))) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Key is outside the module's bounds");
        return FALSE;
    }
    g_clear_pointer(&priv->raw, g_bytes_unref);
    g_clear_pointer(&priv->raw_text, g_free);
    return TRUE;
}

gboolean
gsw_module_set_key_text(GswModule *self, const gchar *text, GError **error)
{
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), FALSE);

    priv = gsw_module_priv(self);
    if (!gsw_key_set_text(priv->key, text)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Key '%s' is outside the module's bounds",
                    text != NULL ? text : "");
        return FALSE;
    }
    g_clear_pointer(&priv->raw, g_bytes_unref);
    g_clear_pointer(&priv->raw_text, g_free);
    return TRUE;
}

const gchar *
gsw_module_get_key_text(GswModule *self)
{
    return gsw_key_get_text(gsw_module_priv(self)->key);
}

static gboolean
gsw_module_update_raw(GswModule *self, GError **error)
{
    GswModulePrivate *priv = gsw_module_priv(self);
    GBytes *raw;
    gsize len;
    const gchar *data;

    if (!gsw_module_ensure_open(self, error))
        return FALSE;

    if (priv->raw != NULL)
        return TRUE;

    raw = priv->backend->ops->read_entry(priv->backend, priv->key, error);
    if (raw == NULL)
        return FALSE;

    g_clear_pointer(&priv->raw, g_bytes_unref);
    g_free(priv->raw_text);

    data = g_bytes_get_data(raw, &len);
    priv->raw = raw;
    priv->raw_len = len;
    priv->raw_text = g_malloc(len + 1);
    if (len > 0)
        memcpy(priv->raw_text, data, len);
    priv->raw_text[len] = '\0';
    return TRUE;
}

GBytes *
gsw_module_read_raw(GswModule *self, GError **error)
{
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);
    priv = gsw_module_priv(self);
    if (!gsw_module_update_raw(self, error))
        return NULL;
    return g_bytes_ref(priv->raw);
}

const gchar *
gsw_module_peek_raw(GswModule *self, GError **error)
{
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);
    priv = gsw_module_priv(self);
    if (!gsw_module_update_raw(self, error))
        return NULL;
    return priv->raw_text;
}

static gchar *
gsw_module_convert_to_utf8(const gchar *data, gsize len, const gchar *encoding,
                           GError **error)
{
    gchar *converted = NULL;
    gsize converted_len = 0;
    const gchar *from = NULL;

    if (encoding == NULL || *encoding == '\0' ||
        g_ascii_strcasecmp(encoding, "UTF-8") == 0 ||
        g_ascii_strcasecmp(encoding, "UTF8") == 0) {
        return g_strndup(data, len);
    }

    if (g_ascii_strcasecmp(encoding, "Latin-1") == 0 ||
        g_ascii_strcasecmp(encoding, "ISO-8859-1") == 0) {
        from = "ISO-8859-1";
    } else if (g_ascii_strcasecmp(encoding, "UTF-16") == 0) {
        from = "UTF-16";
    } else if (g_ascii_strcasecmp(encoding, "SCSU") == 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                    "SCSU encoding is not supported yet");
        return NULL;
    } else {
        return g_strndup(data, len);
    }

    converted = g_convert(data, (gssize)len, "UTF-8", from, NULL,
                          &converted_len, error);
    return converted;
}

static gchar *
gsw_strip_markup(const gchar *input)
{
    GString *out = g_string_new(NULL);
    const gchar *p = input;
    gboolean in_tag = FALSE;
    gboolean last_space = FALSE;

    while (*p != '\0') {
        if (*p == '<') {
            in_tag = TRUE;
            p++;
            continue;
        }
        if (*p == '>' && in_tag) {
            in_tag = FALSE;
            p++;
            continue;
        }
        if (in_tag) {
            p++;
            continue;
        }

        if (*p == '\\' && g_str_has_prefix(p, "\\par")) {
            if (!last_space)
                g_string_append_c(out, ' ');
            last_space = TRUE;
            p += 4;
            continue;
        }

        if (*p == '&') {
            if (g_str_has_prefix(p, "&amp;")) {
                g_string_append_c(out, '&');
                p += 5;
                last_space = FALSE;
                continue;
            }
            if (g_str_has_prefix(p, "&lt;")) {
                g_string_append_c(out, '<');
                p += 4;
                last_space = FALSE;
                continue;
            }
            if (g_str_has_prefix(p, "&gt;")) {
                g_string_append_c(out, '>');
                p += 4;
                last_space = FALSE;
                continue;
            }
            if (g_str_has_prefix(p, "&quot;")) {
                g_string_append_c(out, '"');
                p += 6;
                last_space = FALSE;
                continue;
            }
            if (g_str_has_prefix(p, "&apos;")) {
                g_string_append_c(out, '\'');
                p += 6;
                last_space = FALSE;
                continue;
            }
            if (g_str_has_prefix(p, "&#")) {
                const gchar *end = strchr(p, ';');
                if (end != NULL) {
                    gchar *number = g_strndup(p + 2, (gsize)(end - p - 2));
                    guint32 code = (guint32)g_ascii_strtoull(number, NULL, 10);
                    g_free(number);
                    if (code > 0 && code <= G_MAXUINT32) {
                        gchar utf8[8] = { 0 };
                        gint len = g_unichar_to_utf8((gunichar)code, utf8);
                        g_string_append_len(out, utf8, len);
                        p = end + 1;
                        last_space = FALSE;
                        continue;
                    }
                }
            }
        }

        if (g_ascii_isspace(*p)) {
            if (!last_space)
                g_string_append_c(out, ' ');
            last_space = TRUE;
        } else {
            g_string_append_c(out, *p);
            last_space = FALSE;
        }
        p++;
    }

    g_strstrip(out->str);
    out->len = strlen(out->str);
    return g_string_free(out, FALSE);
}

gchar *
gsw_module_get_text(GswModule *self, GError **error)
{
    GswModulePrivate *priv;
    gchar *utf8;
    gchar *text;

    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);

    priv = gsw_module_priv(self);
    if (!gsw_module_update_raw(self, error))
        return NULL;

    utf8 = gsw_module_convert_to_utf8(priv->raw_text, priv->raw_len,
                                      gsw_module_info_get_encoding(priv->info),
                                      error);
    if (utf8 == NULL)
        return NULL;

    text = gsw_strip_markup(utf8);
    g_free(utf8);
    return text;
}

gboolean
gsw_module_next(GswModule *self, gint steps, GError **error)
{
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), FALSE);
    priv = gsw_module_priv(self);
    if (!gsw_module_ensure_open(self, error))
        return FALSE;
    if (!priv->backend->ops->next(priv->backend, priv->key, steps, error))
        return FALSE;
    g_clear_pointer(&priv->raw, g_bytes_unref);
    g_clear_pointer(&priv->raw_text, g_free);
    return TRUE;
}

gboolean
gsw_module_prev(GswModule *self, gint steps, GError **error)
{
    return gsw_module_next(self, -steps, error);
}

gboolean
gsw_module_set_position(GswModule *self, GswPosition pos, GError **error)
{
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), FALSE);
    priv = gsw_module_priv(self);
    if (!gsw_module_ensure_open(self, error))
        return FALSE;
    if (!priv->backend->ops->set_position(priv->backend, priv->key, pos, error))
        return FALSE;
    g_clear_pointer(&priv->raw, g_bytes_unref);
    g_clear_pointer(&priv->raw_text, g_free);
    return TRUE;
}

gboolean
gsw_module_has_entry(GswModule *self)
{
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), FALSE);
    priv = gsw_module_priv(self);
    if (!gsw_module_ensure_open(self, NULL))
        return FALSE;
    return priv->backend->ops->has_entry(priv->backend, priv->key);
}

gsize
gsw_module_get_entry_size(GswModule *self)
{
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), 0);
    priv = gsw_module_priv(self);
    if (!gsw_module_update_raw(self, NULL))
        return 0;
    return priv->raw_len;
}

gboolean
gsw_module_is_writable(GswModule *self)
{
    (void)self;
    return FALSE;
}
