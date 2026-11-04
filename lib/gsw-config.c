/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-config.c: SWORD-style INI configuration
 */

#include <gsword/gsw-config.h>
#include <gsword/gsw-error.h>
#include "gsw-utils.h"

#include <errno.h>
#include <string.h>

typedef struct {
    gchar *name;
    GHashTable *values;   /* key -> GPtrArray of gchar* */
    GPtrArray *key_order; /* gchar* pointers, owned by values hash? no: duplicate */
} GswSection;

struct _GswConfig {
    GObject parent_instance;
};

typedef struct {
    GHashTable *sections; /* gchar* -> GswSection* */
    GPtrArray *order;     /* GswSection* pointers */
    gchar *path;
} GswConfigPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GswConfig, gsw_config, G_TYPE_OBJECT)

static void
gsw_section_free(gpointer data)
{
    GswSection *section = data;

    if (section == NULL)
        return;

    g_free(section->name);
    g_clear_pointer(&section->values, g_hash_table_unref);
    g_clear_pointer(&section->key_order, g_ptr_array_unref);
    g_free(section);
}

static GswSection *
gsw_section_new(const gchar *name)
{
    GswSection *section = g_new0(GswSection, 1);

    section->name = g_strdup(name != NULL ? name : "");
    section->values = g_hash_table_new_full(g_str_hash, g_str_equal,
                                            g_free,
                                            (GDestroyNotify)g_ptr_array_unref);
    section->key_order = g_ptr_array_new_with_free_func(g_free);
    return section;
}

static GswSection *
gsw_config_lookup_section(GswConfig *self, const gchar *name, gboolean create)
{
    GswConfigPrivate *priv = gsw_config_get_instance_private(self);
    GswSection *section;

    section = g_hash_table_lookup(priv->sections, name);
    if (section == NULL && create) {
        section = gsw_section_new(name);
        g_hash_table_insert(priv->sections, g_strdup(name), section);
        g_ptr_array_add(priv->order, section);
    }
    return section;
}

static void
gsw_config_clear(GswConfig *self)
{
    GswConfigPrivate *priv = gsw_config_get_instance_private(self);

    g_hash_table_remove_all(priv->sections);
    g_ptr_array_set_size(priv->order, 0);
}

static void
gsw_config_finalize(GObject *object)
{
    GswConfig *self = GSW_CONFIG(object);
    GswConfigPrivate *priv = gsw_config_get_instance_private(self);

    g_clear_pointer(&priv->sections, g_hash_table_unref);
    g_clear_pointer(&priv->order, g_ptr_array_unref);
    g_free(priv->path);

    G_OBJECT_CLASS(gsw_config_parent_class)->finalize(object);
}

static void
gsw_config_class_init(GswConfigClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = gsw_config_finalize;
}

static void
gsw_config_init(GswConfig *self)
{
    GswConfigPrivate *priv = gsw_config_get_instance_private(self);

    priv->sections = g_hash_table_new_full(g_str_hash, g_str_equal,
                                           g_free, gsw_section_free);
    priv->order = g_ptr_array_new();
}

GswConfig *
gsw_config_new(void)
{
    return g_object_new(GSW_TYPE_CONFIG, NULL);
}

static gboolean
gsw_config_parse(GswConfig *self, const gchar *data, gsize len, GError **error)
{
    const gchar *p;
    const gchar *end;
    GswSection *current = NULL;

    if (data == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CONFIG,
                    "Configuration data is NULL");
        return FALSE;
    }

    p = data;
    end = data + len;

    if ((gsize)(end - p) >= 3 &&
        (guchar)p[0] == 0xef && (guchar)p[1] == 0xbb && (guchar)p[2] == 0xbf)
        p += 3;

    while (p < end) {
        const gchar *line_end;
        gchar *line;
        gchar *trimmed;

        line_end = memchr(p, '\n', (size_t)(end - p));
        if (line_end == NULL)
            line_end = end;

        line = g_strndup(p, (gsize)(line_end - p));
        if (line[0] != '\0' && line[strlen(line) - 1] == '\r')
            line[strlen(line) - 1] = '\0';

        trimmed = line;
        while (*trimmed != '\0' && g_ascii_isspace(*trimmed))
            trimmed++;

        if (*trimmed == '\0' || *trimmed == '#') {
            g_free(line);
            p = (line_end < end) ? line_end + 1 : end;
            continue;
        }

        if (*trimmed == '[') {
            gchar *close = strchr(trimmed + 1, ']');
            if (close == NULL) {
                g_set_error(error, GSW_ERROR, GSW_ERROR_CONFIG,
                            "Malformed section header: %s", trimmed);
                g_free(line);
                return FALSE;
            }
            *close = '\0';
            {
                gchar *name = gsw_strdup_trim(trimmed + 1);
                current = gsw_config_lookup_section(self, name, TRUE);
                g_free(name);
            }
        } else {
            gchar *eq = strchr(trimmed, '=');
            if (eq != NULL && current != NULL) {
                gchar *key;
                gchar *value;

                *eq = '\0';
                key = gsw_strdup_trim(trimmed);
                value = gsw_strdup_trim(eq + 1);

                if (*key != '\0')
                    gsw_config_add(self, current->name, key, value);

                g_free(key);
                g_free(value);
            }
        }

        g_free(line);
        p = (line_end < end) ? line_end + 1 : end;
    }

    return TRUE;
}

GswConfig *
gsw_config_new_from_file(const gchar *path, GError **error)
{
    GswConfig *self = gsw_config_new();

    if (!gsw_config_load(self, path, error)) {
        g_object_unref(self);
        return NULL;
    }
    return self;
}

GswConfig *
gsw_config_new_from_data(const gchar *data, gssize len, GError **error)
{
    GswConfig *self = gsw_config_new();

    if (len < 0)
        len = (gssize)strlen(data);

    if (!gsw_config_parse(self, data, (gsize)len, error)) {
        g_object_unref(self);
        return NULL;
    }
    return self;
}

gboolean
gsw_config_load(GswConfig *self, const gchar *path, GError **error)
{
    gchar *data = NULL;
    gsize len = 0;
    GError *local_error = NULL;
    gboolean ok;

    g_return_val_if_fail(GSW_IS_CONFIG(self), FALSE);
    g_return_val_if_fail(path != NULL, FALSE);

    gsw_config_clear(self);

    if (!g_file_get_contents(path, &data, &len, &local_error)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot read configuration %s: %s",
                    path, local_error->message);
        g_error_free(local_error);
        return FALSE;
    }

    ok = gsw_config_parse(self, data, len, error);
    g_free(data);

    if (ok) {
        GswConfigPrivate *priv = gsw_config_get_instance_private(self);
        g_free(priv->path);
        priv->path = g_strdup(path);
    }

    return ok;
}

gboolean
gsw_config_save(GswConfig *self, const gchar *path, GError **error)
{
    GswConfigPrivate *priv;
    GString *out;
    GError *local_error = NULL;
    gchar *dir;
    guint i;

    g_return_val_if_fail(GSW_IS_CONFIG(self), FALSE);
    g_return_val_if_fail(path != NULL, FALSE);

    priv = gsw_config_get_instance_private(self);
    out = g_string_new(NULL);

    for (i = 0; i < priv->order->len; i++) {
        GswSection *section = g_ptr_array_index(priv->order, i);
        guint j;

        g_string_append_printf(out, "\n[%s]\n", section->name);
        for (j = 0; j < section->key_order->len; j++) {
            const gchar *key = g_ptr_array_index(section->key_order, j);
            GPtrArray *values = g_hash_table_lookup(section->values, key);
            guint k;

            if (values == NULL)
                continue;
            for (k = 0; k < values->len; k++) {
                g_string_append_printf(out, "%s=%s\n", key,
                                       (const gchar *)g_ptr_array_index(values, k));
            }
        }
    }
    g_string_append_c(out, '\n');

    dir = g_path_get_dirname(path);
    if (!gsw_mkdir_with_parents(dir, error)) {
        g_free(dir);
        g_string_free(out, TRUE);
        return FALSE;
    }
    g_free(dir);

    if (!g_file_set_contents(path, out->str, (gssize)out->len, &local_error)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot write configuration %s: %s",
                    path, local_error->message);
        g_error_free(local_error);
        g_string_free(out, TRUE);
        return FALSE;
    }

    g_free(priv->path);
    priv->path = g_strdup(path);
    g_string_free(out, TRUE);
    return TRUE;
}

const gchar *
gsw_config_get_path(GswConfig *self)
{
    GswConfigPrivate *priv;

    g_return_val_if_fail(GSW_IS_CONFIG(self), NULL);
    priv = gsw_config_get_instance_private(self);
    return priv->path;
}

GswConfig *
gsw_config_copy(GswConfig *self)
{
    GswConfigPrivate *priv;
    GswConfig *copy;
    guint i;

    g_return_val_if_fail(GSW_IS_CONFIG(self), NULL);

    priv = gsw_config_get_instance_private(self);
    copy = gsw_config_new();

    for (i = 0; i < priv->order->len; i++) {
        GswSection *section = g_ptr_array_index(priv->order, i);
        guint j;

        for (j = 0; j < section->key_order->len; j++) {
            const gchar *key = g_ptr_array_index(section->key_order, j);
            GPtrArray *values = g_hash_table_lookup(section->values, key);
            guint k;

            for (k = 0; values != NULL && k < values->len; k++)
                gsw_config_add(copy, section->name, key,
                               (const gchar *)g_ptr_array_index(values, k));
        }
    }

    {
        GswConfigPrivate *copy_priv = gsw_config_get_instance_private(copy);
        copy_priv->path = g_strdup(priv->path);
    }
    return copy;
}

gboolean
gsw_config_has_section(GswConfig *self, const gchar *section)
{
    GswConfigPrivate *priv;

    g_return_val_if_fail(GSW_IS_CONFIG(self), FALSE);
    g_return_val_if_fail(section != NULL, FALSE);

    priv = gsw_config_get_instance_private(self);
    return g_hash_table_contains(priv->sections, section);
}

gchar **
gsw_config_get_sections(GswConfig *self)
{
    GswConfigPrivate *priv;
    gchar **result;
    guint i;

    g_return_val_if_fail(GSW_IS_CONFIG(self), NULL);

    priv = gsw_config_get_instance_private(self);
    result = g_new0(gchar *, priv->order->len + 1);
    for (i = 0; i < priv->order->len; i++) {
        GswSection *section = g_ptr_array_index(priv->order, i);
        result[i] = g_strdup(section->name);
    }
    return result;
}

gboolean
gsw_config_remove_section(GswConfig *self, const gchar *section)
{
    GswConfigPrivate *priv;
    guint i;
    gboolean found_index = FALSE;

    g_return_val_if_fail(GSW_IS_CONFIG(self), FALSE);
    g_return_val_if_fail(section != NULL, FALSE);

    priv = gsw_config_get_instance_private(self);

    for (i = 0; i < priv->order->len; i++) {
        GswSection *item = g_ptr_array_index(priv->order, i);
        if (g_strcmp0(item->name, section) == 0) {
            g_ptr_array_remove_index(priv->order, i);
            found_index = TRUE;
            break;
        }
    }

    if (!g_hash_table_remove(priv->sections, section))
        return FALSE;

    return found_index;
}

gboolean
gsw_config_has_key(GswConfig *self, const gchar *section, const gchar *key)
{
    GswSection *sec;

    g_return_val_if_fail(GSW_IS_CONFIG(self), FALSE);
    g_return_val_if_fail(section != NULL, FALSE);
    g_return_val_if_fail(key != NULL, FALSE);

    sec = gsw_config_lookup_section(self, section, FALSE);
    return sec != NULL && g_hash_table_contains(sec->values, key);
}

const gchar *
gsw_config_get(GswConfig *self, const gchar *section, const gchar *key)
{
    GswSection *sec;
    GPtrArray *values;

    g_return_val_if_fail(GSW_IS_CONFIG(self), NULL);
    g_return_val_if_fail(section != NULL, NULL);
    g_return_val_if_fail(key != NULL, NULL);

    sec = gsw_config_lookup_section(self, section, FALSE);
    if (sec == NULL)
        return NULL;

    values = g_hash_table_lookup(sec->values, key);
    if (values == NULL || values->len == 0)
        return NULL;

    return g_ptr_array_index(values, 0);
}

gchar **
gsw_config_get_all(GswConfig *self, const gchar *section, const gchar *key)
{
    GswSection *sec;
    GPtrArray *values;
    gchar **result;
    guint i;

    g_return_val_if_fail(GSW_IS_CONFIG(self), NULL);
    g_return_val_if_fail(section != NULL, NULL);
    g_return_val_if_fail(key != NULL, NULL);

    sec = gsw_config_lookup_section(self, section, FALSE);
    if (sec == NULL)
        return NULL;

    values = g_hash_table_lookup(sec->values, key);
    if (values == NULL)
        return NULL;

    result = g_new0(gchar *, values->len + 1);
    for (i = 0; i < values->len; i++)
        result[i] = g_strdup(g_ptr_array_index(values, i));
    return result;
}

gchar **
gsw_config_get_keys(GswConfig *self, const gchar *section)
{
    GswSection *sec;
    gchar **result;
    guint i;

    g_return_val_if_fail(GSW_IS_CONFIG(self), NULL);
    g_return_val_if_fail(section != NULL, NULL);

    sec = gsw_config_lookup_section(self, section, FALSE);
    if (sec == NULL)
        return NULL;

    result = g_new0(gchar *, sec->key_order->len + 1);
    for (i = 0; i < sec->key_order->len; i++)
        result[i] = g_strdup(g_ptr_array_index(sec->key_order, i));
    return result;
}

gboolean
gsw_config_set(GswConfig *self, const gchar *section, const gchar *key,
               const gchar *value)
{
    GswSection *sec;
    GPtrArray *values;
    guint i;

    g_return_val_if_fail(GSW_IS_CONFIG(self), FALSE);
    g_return_val_if_fail(section != NULL, FALSE);
    g_return_val_if_fail(key != NULL, FALSE);

    sec = gsw_config_lookup_section(self, section, TRUE);
    values = g_hash_table_lookup(sec->values, key);

    if (values == NULL) {
        values = g_ptr_array_new_with_free_func(g_free);
        g_hash_table_insert(sec->values, g_strdup(key), values);
        g_ptr_array_add(sec->key_order, g_strdup(key));
    } else {
        g_ptr_array_set_size(values, 0);
    }

    g_ptr_array_add(values, g_strdup(value != NULL ? value : ""));

    /* Avoid duplicate key order entries if the key already existed. */
    for (i = 0; i < sec->key_order->len; i++) {
        if (g_strcmp0(g_ptr_array_index(sec->key_order, i), key) == 0)
            break;
    }
    return TRUE;
}

gboolean
gsw_config_add(GswConfig *self, const gchar *section, const gchar *key,
               const gchar *value)
{
    GswSection *sec;
    GPtrArray *values;

    g_return_val_if_fail(GSW_IS_CONFIG(self), FALSE);
    g_return_val_if_fail(section != NULL, FALSE);
    g_return_val_if_fail(key != NULL, FALSE);

    sec = gsw_config_lookup_section(self, section, TRUE);
    values = g_hash_table_lookup(sec->values, key);
    if (values == NULL) {
        values = g_ptr_array_new_with_free_func(g_free);
        g_hash_table_insert(sec->values, g_strdup(key), values);
        g_ptr_array_add(sec->key_order, g_strdup(key));
    }
    g_ptr_array_add(values, g_strdup(value != NULL ? value : ""));
    return TRUE;
}

gboolean
gsw_config_remove_key(GswConfig *self, const gchar *section, const gchar *key)
{
    GswSection *sec;
    guint i;

    g_return_val_if_fail(GSW_IS_CONFIG(self), FALSE);
    g_return_val_if_fail(section != NULL, FALSE);
    g_return_val_if_fail(key != NULL, FALSE);

    sec = gsw_config_lookup_section(self, section, FALSE);
    if (sec == NULL || !g_hash_table_remove(sec->values, key))
        return FALSE;

    for (i = 0; i < sec->key_order->len; i++) {
        if (g_strcmp0(g_ptr_array_index(sec->key_order, i), key) == 0) {
            g_ptr_array_remove_index(sec->key_order, i);
            break;
        }
    }
    return TRUE;
}

void
gsw_config_augment(GswConfig *self, GswConfig *other)
{
    GswConfigPrivate *other_priv;
    guint i;

    g_return_if_fail(GSW_IS_CONFIG(self));
    g_return_if_fail(GSW_IS_CONFIG(other));

    other_priv = gsw_config_get_instance_private(other);

    for (i = 0; i < other_priv->order->len; i++) {
        GswSection *section = g_ptr_array_index(other_priv->order, i);
        guint j;

        for (j = 0; j < section->key_order->len; j++) {
            const gchar *key = g_ptr_array_index(section->key_order, j);
            GPtrArray *values = g_hash_table_lookup(section->values, key);
            GPtrArray *existing;
            GswSection *target;
            guint k;

            target = gsw_config_lookup_section(self, section->name, TRUE);
            existing = g_hash_table_lookup(target->values, key);

            if (existing == NULL) {
                for (k = 0; values != NULL && k < values->len; k++)
                    gsw_config_add(self, section->name, key,
                                   (const gchar *)g_ptr_array_index(values, k));
                continue;
            }

            if (existing->len == 1 && values != NULL && values->len == 1) {
                gsw_config_set(self, section->name, key,
                               (const gchar *)g_ptr_array_index(values, 0));
                continue;
            }

            for (k = 0; values != NULL && k < values->len; k++) {
                const gchar *value = g_ptr_array_index(values, k);
                guint m;
                gboolean found = FALSE;

                for (m = 0; m < existing->len; m++) {
                    if (g_strcmp0(g_ptr_array_index(existing, m), value) == 0) {
                        found = TRUE;
                        break;
                    }
                }
                if (!found)
                    gsw_config_add(self, section->name, key, value);
            }
        }
    }
}
