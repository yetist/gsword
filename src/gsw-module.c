/*
 * gsw-module.c: readable module object
 */

#include <gsword/gsw-module.h>
#include <gsword/gsw-error.h>
#include <gsword/gsw-search.h>
#include "gsw-key-internal.h"
#include "gsw-module-backend.h"
#include "gsw-options.h"
#include "gsw-render.h"
#include "gsw-utils.h"
#include "gsw-encoding.h"
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
    GHashTable *options;          /* option name -> value */
    GHashTable *option_overrides; /* option names explicitly set */
    GHashTable *attributes;       /* "level1/level2/level3" -> value */
    gboolean attributes_valid;
    GswTestamentFlags testaments;
    gboolean testaments_valid;
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
            priv->options = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  g_free, g_free);
            priv->option_overrides = g_hash_table_new_full(
                g_str_hash, g_str_equal, g_free, NULL);
            gsw_options_init(priv->options);
            g_object_set_qdata_full(G_OBJECT(self), quark, priv, g_free);
        }
        return priv;
    }
}

static void
gsw_module_clear_entry_cache(GswModulePrivate *priv)
{
    g_clear_pointer(&priv->raw, g_bytes_unref);
    g_clear_pointer(&priv->raw_text, g_free);
    g_clear_pointer(&priv->attributes, g_hash_table_unref);
    priv->attributes_valid = FALSE;
    priv->raw_len = 0;
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
    g_clear_pointer(&priv->options, g_hash_table_unref);
    g_clear_pointer(&priv->option_overrides, g_hash_table_unref);
    g_clear_pointer(&priv->attributes, g_hash_table_unref);

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

GswTestamentFlags
gsw_module_get_testaments(GswModule *self)
{
    GswModulePrivate *priv;
    GswKey *saved;
    GError *local_error = NULL;

    g_return_val_if_fail(GSW_IS_MODULE(self), GSW_TESTAMENT_NONE);

    priv = gsw_module_priv(self);
    if (priv->testaments_valid)
        return priv->testaments;

    priv->testaments_valid = TRUE;
    priv->testaments = GSW_TESTAMENT_NONE;

    if (gsw_key_get_kind(priv->key) != GSW_KEY_KIND_VERSE)
        return priv->testaments;

    saved = gsw_key_clone(priv->key);
    if (saved == NULL)
        return priv->testaments;

    if (gsw_module_set_key_text(self, "Gen 1:1", NULL) &&
        gsw_module_has_entry(self))
        priv->testaments |= GSW_TESTAMENT_OLD;
    if (gsw_module_set_key_text(self, "Matt 1:1", NULL) &&
        gsw_module_has_entry(self))
        priv->testaments |= GSW_TESTAMENT_NEW;

    if (priv->testaments == GSW_TESTAMENT_NONE &&
        gsw_module_set_position(self, GSW_POSITION_BOTTOM, &local_error)) {
        gint testament = gsw_verse_key_get_testament(
            (GswVerseKey *)priv->key);
        if (testament == 1)
            priv->testaments |= GSW_TESTAMENT_OLD;
        else if (testament == 2)
            priv->testaments |= GSW_TESTAMENT_NEW;
    }
    g_clear_error(&local_error);

    gsw_module_set_key(self, saved, NULL);
    gsw_key_unref(saved);
    return priv->testaments;
}

gboolean
gsw_module_has_testament(GswModule *self, GswTestamentFlags testament)
{
    GswTestamentFlags present;

    g_return_val_if_fail(GSW_IS_MODULE(self), FALSE);
    if (testament == GSW_TESTAMENT_NONE)
        return FALSE;
    present = gsw_module_get_testaments(self);
    return (present & testament) == testament;
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
    gsw_module_clear_entry_cache(priv);
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
    gsw_module_clear_entry_cache(priv);
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
        gchar *scsu_out = NULL;
        gsize scsu_len = 0;

        if (!gsw_scsu_to_utf8(data, len, &scsu_out, &scsu_len, error))
            return NULL;
        return scsu_out;
    } else {
        return g_strndup(data, len);
    }

    converted = g_convert(data, (gssize)len, "UTF-8", from, NULL,
                          &converted_len, error);
    return converted;
}

static gboolean
gsw_module_option_on(GswModule *self, const gchar *name)
{
    GswModulePrivate *priv = gsw_module_priv(self);
    const gchar *value = g_hash_table_lookup(priv->options, name);

    return value != NULL && (g_ascii_strcasecmp(value, "On") == 0 ||
                             gsw_str_is_true(value));
}

static gboolean
gsw_module_option_on_any(GswModule *self, const gchar *const *names)
{
    guint i;

    for (i = 0; names[i] != NULL; i++) {
        if (gsw_module_option_on(self, names[i]))
            return TRUE;
    }
    return FALSE;
}

static gchar *
gsw_tag_attr(const gchar *attrs, const gchar *name)
{
    const gchar *p;
    gsize name_len = strlen(name);

    if (attrs == NULL)
        return NULL;

    p = attrs;
    while ((p = strstr(p, name)) != NULL) {
        const gchar *q = p + name_len;

        if (p > attrs && (g_ascii_isalnum(p[-1]) || p[-1] == '_')) {
            p = q;
            continue;
        }
        while (*q == ' ' || *q == '\t')
            q++;
        if (*q != '=') {
            p = q;
            continue;
        }
        q++;
        while (*q == ' ' || *q == '\t')
            q++;
        if (*q == '"' || *q == '\'') {
            gchar quote = *q++;
            const gchar *end = strchr(q, quote);
            if (end != NULL)
                return g_strndup(q, (gsize)(end - q));
        } else {
            const gchar *end = q;
            while (*end != '\0' && *end != ' ' && *end != '\t')
                end++;
            return g_strndup(q, (gsize)(end - q));
        }
        p = q;
    }
    return NULL;
}

static void
gsw_render_append(GString *out, const gchar *text, gsize len,
                  gboolean *last_space)
{
    gsize i;

    for (i = 0; i < len; i++) {
        gchar c = text[i];

        if (c == '\\' && i + 3 < len && !strncmp(text + i, "\\par", 4)) {
            if (!*last_space)
                g_string_append_c(out, ' ');
            *last_space = TRUE;
            i += 3;
            continue;
        }

        if (c == '&') {
            if (i + 4 < len && !strncmp(text + i, "&amp;", 5)) {
                g_string_append_c(out, '&');
                i += 4;
                *last_space = FALSE;
                continue;
            }
            if (i + 3 < len && !strncmp(text + i, "&lt;", 4)) {
                g_string_append_c(out, '<');
                i += 3;
                *last_space = FALSE;
                continue;
            }
            if (i + 3 < len && !strncmp(text + i, "&gt;", 4)) {
                g_string_append_c(out, '>');
                i += 3;
                *last_space = FALSE;
                continue;
            }
            if (i + 5 < len && !strncmp(text + i, "&quot;", 6)) {
                g_string_append_c(out, '"');
                i += 5;
                *last_space = FALSE;
                continue;
            }
            if (i + 5 < len && !strncmp(text + i, "&apos;", 6)) {
                g_string_append_c(out, '\'');
                i += 5;
                *last_space = FALSE;
                continue;
            }
            if (i + 2 < len && !strncmp(text + i, "&#", 2)) {
                const gchar *end = memchr(text + i, ';', len - i);
                if (end != NULL) {
                    gchar *number = g_strndup(text + i + 2,
                                              (gsize)(end - (text + i + 2)));
                    guint32 code = (guint32)g_ascii_strtoull(number, NULL, 10);
                    g_free(number);
                    if (code > 0) {
                        gchar utf8[8] = { 0 };
                        gint n = g_unichar_to_utf8((gunichar)code, utf8);
                        g_string_append_len(out, utf8, n);
                        i = (gsize)(end - text);
                        *last_space = FALSE;
                        continue;
                    }
                }
            }
        }

        if (g_ascii_isspace(c)) {
            if (!*last_space)
                g_string_append_c(out, ' ');
            *last_space = TRUE;
        } else {
            g_string_append_c(out, c);
            *last_space = FALSE;
        }
    }
}

static void
gsw_render_strongs(GString *out, const gchar *value)
{
    gchar **tokens;
    guint i;

    if (value == NULL)
        return;
    tokens = g_strsplit(value, " ", -1);
    for (i = 0; tokens[i] != NULL; i++) {
        if (g_str_has_prefix(tokens[i], "strong:"))
            g_string_append_printf(out, "{%s}", tokens[i] + 7);
    }
    g_strfreev(tokens);
}

static gchar *
gsw_render_markup(GswModule *self, const gchar *input)
{
    static const gchar *const strongs_names[] = {
        "Strong's Numbers", "Lemmas", NULL
    };
    static const gchar *const morph_names[] = {
        "Morphology", "Morphological Tags", NULL
    };
    static const gchar *const red_names[] = {
        "Red Letter Words", "Words of Christ in Red", NULL
    };
    GString *out = g_string_new(NULL);
    gboolean last_space = TRUE;
    gboolean strongs = gsw_module_option_on_any(self, strongs_names);
    gboolean footnotes = gsw_module_option_on(self, "Footnotes");
    gboolean headings = gsw_module_option_on(self, "Headings");
    gboolean redletter = gsw_module_option_on_any(self, red_names);
    gboolean morph = gsw_module_option_on_any(self, morph_names);
    gboolean xrefs = gsw_module_option_on(self, "Cross-references");
    gboolean glosses = gsw_module_option_on(self, "Glosses");
    gboolean images = gsw_module_option_on(self, "Images");
    gchar *skip_tag = NULL;
    gint skip_depth = 0;
    gboolean in_word = FALSE;
    gchar *word_lemma = NULL;
    gchar *word_morph = NULL;
    const gchar *p = input;

    while (*p != '\0') {
        if (*p == '<') {
            const gchar *end = strchr(p + 1, '>');
            gchar *tag;
            gchar *name;
            gchar *attrs;
            gboolean closing = FALSE;
            gboolean self_closing = FALSE;

            if (end == NULL)
                break;
            tag = g_strndup(p + 1, (gsize)(end - p - 1));
            p = end + 1;

            {
                gchar *q = tag;
                while (*q == ' ' || *q == '\t')
                    q++;
                if (*q == '/') {
                    closing = TRUE;
                    q++;
                }
                {
                    gchar *name_start = q;
                    while (*q != '\0' && *q != ' ' && *q != '\t' && *q != '/')
                        q++;
                    name = g_strndup(name_start, (gsize)(q - name_start));
                    attrs = g_strdup(q);
                }
                {
                    gchar *trim = g_strdup(attrs);
                    g_strstrip(trim);
                    if (trim[0] != '\0' && trim[strlen(trim) - 1] == '/')
                        self_closing = TRUE;
                    g_free(trim);
                }
            }

            if (skip_depth > 0) {
                if (g_strcmp0(name, skip_tag) == 0) {
                    if (closing) {
                        skip_depth--;
                        if (skip_depth == 0) {
                            g_free(skip_tag);
                            skip_tag = NULL;
                        }
                    } else if (!self_closing) {
                        skip_depth++;
                    }
                }
                g_free(name);
                g_free(attrs);
                g_free(tag);
                continue;
            }

            if (!closing &&
                ((!footnotes && (g_strcmp0(name, "note") == 0 ||
                                 g_strcmp0(name, "F") == 0)) ||
                 (!headings && (g_strcmp0(name, "title") == 0 ||
                                g_strcmp0(name, "H") == 0 ||
                                g_strcmp0(name, "h1") == 0 ||
                                g_strcmp0(name, "h2") == 0 ||
                                g_strcmp0(name, "h3") == 0 ||
                                g_strcmp0(name, "h4") == 0 ||
                                g_strcmp0(name, "h5") == 0 ||
                                g_strcmp0(name, "h6") == 0)) ||
                 (!xrefs && (g_strcmp0(name, "reference") == 0 ||
                             g_strcmp0(name, "xref") == 0)) ||
                 (!glosses && (g_strcmp0(name, "gloss") == 0 ||
                               g_strcmp0(name, "G") == 0)) ||
                 (!images && !self_closing &&
                  (g_strcmp0(name, "img") == 0 ||
                   g_strcmp0(name, "image") == 0)))) {
                g_free(skip_tag);
                skip_tag = g_strdup(name);
                skip_depth = 1;
                g_free(name);
                g_free(attrs);
                g_free(tag);
                continue;
            }

            if (g_strcmp0(name, "w") == 0 || g_strcmp0(name, "W") == 0) {
                if (closing) {
                    if (strongs && word_lemma != NULL)
                        gsw_render_strongs(out, word_lemma);
                    if (morph && word_morph != NULL)
                        g_string_append_printf(out, "{%s}", word_morph);
                    g_clear_pointer(&word_lemma, g_free);
                    g_clear_pointer(&word_morph, g_free);
                    in_word = FALSE;
                    last_space = FALSE;
                } else if (!self_closing) {
                    in_word = TRUE;
                    word_lemma = gsw_tag_attr(attrs, "lemma");
                    word_morph = gsw_tag_attr(attrs, "morph");
                }
            } else if (g_strcmp0(name, "note") == 0 || g_strcmp0(name, "F") == 0) {
                if (footnotes) {
                    g_string_append(out, closing ? ")" : " (");
                    last_space = TRUE;
                }
            } else if (g_strcmp0(name, "title") == 0 || g_strcmp0(name, "H") == 0) {
                if (headings) {
                    if (!last_space)
                        g_string_append_c(out, ' ');
                    last_space = TRUE;
                }
            } else if (g_strcmp0(name, "q") == 0 || g_strcmp0(name, "R") == 0) {
                if (redletter) {
                    g_string_append(out, closing ? "\xC2\xBB" : "\xC2\xAB");
                    last_space = FALSE;
                }
            } else if (g_strcmp0(name, "sync") == 0 && !closing) {
                gchar *type = gsw_tag_attr(attrs, "type");
                gchar *value = gsw_tag_attr(attrs, "value");
                if (type != NULL && value != NULL) {
                    if (strongs && g_ascii_strcasecmp(type, "Strongs") == 0)
                        g_string_append_printf(out, "{%s}", value);
                    else if (morph && g_ascii_strcasecmp(type, "morph") == 0)
                        g_string_append_printf(out, "{%s}", value);
                }
                g_free(type);
                g_free(value);
            } else if (g_strcmp0(name, "br") == 0 ||
                       g_strcmp0(name, "lb") == 0 ||
                       g_strcmp0(name, "p") == 0 ||
                       g_strcmp0(name, "div") == 0 ||
                       g_strcmp0(name, "verse") == 0 ||
                       g_strcmp0(name, "chapter") == 0) {
                if (!last_space)
                    g_string_append_c(out, ' ');
                last_space = TRUE;
            }

            (void)in_word;
            g_free(name);
            g_free(attrs);
            g_free(tag);
        } else {
            const gchar *start = p;

            while (*p != '\0' && *p != '<')
                p++;
            if (skip_depth == 0)
                gsw_render_append(out, start, (gsize)(p - start), &last_space);
        }
    }

    g_free(skip_tag);
    g_free(word_lemma);
    g_free(word_morph);
    g_strstrip(out->str);
    out->len = strlen(out->str);
    return g_string_free(out, FALSE);
}


gboolean
gsw_module_set_option(GswModule *self, const gchar *name, const gchar *value)
{
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), FALSE);
    g_return_val_if_fail(name != NULL, FALSE);

    priv = gsw_module_priv(self);
    if (!gsw_options_known(name) &&
        !g_hash_table_contains(priv->options, name))
        return FALSE;
    g_hash_table_replace(priv->options, g_strdup(name),
                         g_strdup(value != NULL ? value : "Off"));
    g_hash_table_replace(priv->option_overrides, g_strdup(name),
                         GINT_TO_POINTER(1));
    return TRUE;
}

const gchar *
gsw_module_get_option(GswModule *self, const gchar *name)
{
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);
    g_return_val_if_fail(name != NULL, NULL);

    priv = gsw_module_priv(self);
    return g_hash_table_lookup(priv->options, name);
}

GPtrArray *
gsw_module_get_options(GswModule *self)
{
    GswModulePrivate *priv;
    GPtrArray *result;
    GList *keys;
    GList *item;

    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);
    priv = gsw_module_priv(self);
    result = g_ptr_array_new_with_free_func(g_free);
    keys = g_hash_table_get_keys(priv->options);
    for (item = keys; item != NULL; item = item->next)
        g_ptr_array_add(result, g_strdup(item->data));
    g_list_free(keys);
    return result;
}

GPtrArray *
gsw_module_get_option_values(GswModule *self, const gchar *name)
{
    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);
    g_return_val_if_fail(name != NULL, NULL);

    if (gsw_module_get_option(self, name) == NULL)
        return NULL;
    return gsw_options_values(name);
}

void
gsw_module_apply_default_options(GswModule *self, GHashTable *defaults)
{
    GHashTableIter iter;
    gpointer key, value;
    GswModulePrivate *priv;

    g_return_if_fail(GSW_IS_MODULE(self));

    priv = gsw_module_priv(self);
    if (defaults == NULL)
        return;

    g_hash_table_iter_init(&iter, defaults);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        const gchar *name = key;

        if (g_hash_table_contains(priv->option_overrides, name))
            continue;
        g_hash_table_replace(priv->options, g_strdup(name),
                             g_strdup(value));
    }
}


gchar **
gsw_module_search(GswModule *self, const gchar *text, GswSearchType type,
                  GError **error)
{
    GswSearchOptions *options;
    GPtrArray *results;
    gchar **keys;
    guint i;

    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);
    g_return_val_if_fail(text != NULL, NULL);

    options = gsw_search_options_new();
    gsw_search_options_set_text(options, text);
    gsw_search_options_set_search_type(options, type);
    results = gsw_module_search_full(self, options, NULL, NULL, NULL, error);
    gsw_search_options_unref(options);
    if (results == NULL)
        return NULL;

    keys = g_new0(gchar *, results->len + 1);
    for (i = 0; i < results->len; i++) {
        GswSearchResult *result = g_ptr_array_index(results, i);
        keys[i] = g_strdup(gsw_search_result_get_key(result));
    }
    g_ptr_array_unref(results);
    return keys;
}

gchar *
gsw_module_get_render_text(GswModule *self, GError **error)
{
    return gsw_module_get_text(self, error);
}

static gchar *
gsw_module_get_current_markup(GswModule *self, GError **error)
{
    GswModulePrivate *priv;

    priv = gsw_module_priv(self);
    if (!gsw_module_update_raw(self, error))
        return NULL;
    return gsw_module_convert_to_utf8(
        priv->raw_text, priv->raw_len,
        gsw_module_info_get_encoding(priv->info), error);
}

gchar *
gsw_module_get_text(GswModule *self, GError **error)
{
    gchar *markup;
    gchar *text;

    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);

    markup = gsw_module_get_current_markup(self, error);
    if (markup == NULL)
        return NULL;
    text = gsw_module_render_text(self, markup, error);
    g_free(markup);
    return text;
}

gchar *
gsw_module_get_render_html(GswModule *self, GError **error)
{
    gchar *markup;
    gchar *html;

    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);

    markup = gsw_module_get_current_markup(self, error);
    if (markup == NULL)
        return NULL;
    html = gsw_module_render_html(self, markup, error);
    g_free(markup);
    return html;
}

gchar *
gsw_module_render_text(GswModule *self, const gchar *text, GError **error)
{
    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);
    g_return_val_if_fail(text != NULL, NULL);

    (void)error;
    return gsw_render_markup(self, text);
}

gchar *
gsw_module_render_html(GswModule *self, const gchar *text, GError **error)
{
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);
    g_return_val_if_fail(text != NULL, NULL);

    (void)error;
    priv = gsw_module_priv(self);
    return gsw_render_markup_html(priv->options, text);
}

gchar *
gsw_module_strip_text(GswModule *self, const gchar *text, GError **error)
{
    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);
    g_return_val_if_fail(text != NULL, NULL);

    (void)error;
    return gsw_render_strip_markup(text);
}

static GHashTable *
gsw_module_ensure_attributes(GswModule *self, GError **error)
{
    GswModulePrivate *priv;
    gchar *markup;

    priv = gsw_module_priv(self);
    if (priv->attributes_valid)
        return priv->attributes;

    markup = gsw_module_get_current_markup(self, error);
    if (markup == NULL)
        return NULL;

    priv->attributes = gsw_render_collect_attributes(markup);
    priv->attributes_valid = TRUE;
    g_free(markup);
    return priv->attributes;
}

gchar *
gsw_module_get_entry_attribute(GswModule *self, const gchar *level1,
                               const gchar *level2, const gchar *level3,
                               GError **error)
{
    GHashTable *attributes;
    gchar *key;
    const gchar *value;

    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);
    g_return_val_if_fail(level1 != NULL, NULL);
    g_return_val_if_fail(level2 != NULL, NULL);
    g_return_val_if_fail(level3 != NULL, NULL);

    attributes = gsw_module_ensure_attributes(self, error);
    if (attributes == NULL)
        return NULL;

    key = g_strdup_printf("%s/%s/%s", level1, level2, level3);
    value = g_hash_table_lookup(attributes, key);
    g_free(key);
    return value != NULL ? g_strdup(value) : NULL;
}

GPtrArray *
gsw_module_get_entry_attribute_keys(GswModule *self, const gchar *level1,
                                    const gchar *level2, GError **error)
{
    GHashTable *attributes;
    GHashTableIter iter;
    gpointer key, value;
    gchar *prefix;
    gsize prefix_len;
    GPtrArray *result;

    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);
    g_return_val_if_fail(level1 != NULL, NULL);
    g_return_val_if_fail(level2 != NULL, NULL);

    attributes = gsw_module_ensure_attributes(self, error);
    if (attributes == NULL)
        return NULL;

    prefix = g_strdup_printf("%s/%s/", level1, level2);
    prefix_len = strlen(prefix);
    result = g_ptr_array_new_with_free_func(g_free);
    g_hash_table_iter_init(&iter, attributes);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        const gchar *name = key;

        if (g_str_has_prefix(name, prefix))
            g_ptr_array_add(result, g_strdup(name + prefix_len));
    }
    g_free(prefix);
    return result;
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
    gsw_module_clear_entry_cache(priv);
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

    if (pos == GSW_POSITION_BOTTOM &&
        gsw_key_get_kind(priv->key) == GSW_KEY_KIND_VERSE) {
        guint attempts = 0;
        gboolean found = FALSE;

        while (attempts < 4096) {
            GError *local_error = NULL;

            if (priv->backend->ops->has_entry(priv->backend, priv->key)) {
                found = TRUE;
                break;
            }
            if (!priv->backend->ops->next(priv->backend, priv->key, -1,
                                          &local_error)) {
                g_clear_error(&local_error);
                break;
            }
            attempts++;
        }

        if (!found) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                        "No non-empty entry found at the bottom of the module");
            return FALSE;
        }
    }

    gsw_module_clear_entry_cache(priv);
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
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), FALSE);
    priv = gsw_module_priv(self);
    return priv->backend != NULL &&
           priv->backend->ops->is_writable != NULL &&
           priv->backend->ops->is_writable(priv->backend, priv->key);
}

gboolean
gsw_module_set_entry(GswModule *self, const gchar *entry, GError **error)
{
    GswModulePrivate *priv;
    const gchar *data = entry != NULL ? entry : "";

    g_return_val_if_fail(GSW_IS_MODULE(self), FALSE);
    priv = gsw_module_priv(self);
    if (!gsw_module_ensure_open(self, error))
        return FALSE;
    if (priv->backend->ops->write_entry == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                    "Module driver [%s] is read-only",
                    gsw_module_info_get_driver_name(priv->info));
        return FALSE;
    }
    if (!priv->backend->ops->write_entry(priv->backend, priv->key,
                                         (const guint8 *)data,
                                         strlen(data), error))
        return FALSE;
    gsw_module_clear_entry_cache(priv);
    return TRUE;
}

gboolean
gsw_module_delete_entry(GswModule *self, GError **error)
{
    GswModulePrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE(self), FALSE);
    priv = gsw_module_priv(self);
    if (!gsw_module_ensure_open(self, error))
        return FALSE;
    if (priv->backend->ops->delete_entry == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                    "Module driver [%s] is read-only",
                    gsw_module_info_get_driver_name(priv->info));
        return FALSE;
    }
    if (!priv->backend->ops->delete_entry(priv->backend, priv->key, error))
        return FALSE;
    gsw_module_clear_entry_cache(priv);
    return TRUE;
}
