/*
 * gsw-locale.c: self-contained locale and book-name translation
 */

#include <gsword/gsw-locale.h>
#include <gsword/gsw-config.h>
#include <gsword/gsw-error.h>
#include "gsw-versification-data.h"

#include <string.h>

typedef struct {
    gchar *name;
    GHashTable *forward; /* English -> localized */
    GHashTable *reverse; /* localized -> English */
    GHashTable *aliases; /* normalized alias -> GPtrArray of OSIS names */
    GHashTable *pref_abbrevs; /* normalized OSIS abbreviation -> localized */
} GswLocale;

typedef struct {
    const gchar *alias;
    const gchar *osis;
} GswAliasMatch;

struct _GswLocaleMgr {
    GObject parent_instance;
};

typedef struct {
    GHashTable *locales; /* name -> GswLocale* */
    GPtrArray *order;
    gchar *default_locale;
    gboolean scanned_paths;
} GswLocaleMgrPrivate;

static GswLocaleMgr *default_manager = NULL;

G_DEFINE_TYPE_WITH_PRIVATE(GswLocaleMgr, gsw_locale_mgr, G_TYPE_OBJECT)

static gchar *
locale_normalize(const gchar *text)
{
    gchar *stripped;
    gchar *folded;

    if (text == NULL)
        return g_strdup("");
    stripped = g_strdup(text);
    g_strstrip(stripped);
    folded = g_utf8_casefold(stripped, -1);
    g_free(stripped);
    return folded;
}

static void
locale_alias_list_free(GPtrArray *list)
{
    if (list != NULL)
        g_ptr_array_unref(list);
}

static void
gsw_locale_free(gpointer data)
{
    GswLocale *locale = data;

    if (locale == NULL)
        return;
    g_free(locale->name);
    g_clear_pointer(&locale->forward, g_hash_table_unref);
    g_clear_pointer(&locale->reverse, g_hash_table_unref);
    g_clear_pointer(&locale->aliases, g_hash_table_unref);
    g_clear_pointer(&locale->pref_abbrevs, g_hash_table_unref);
    g_free(locale);
}

static void
locale_add_abbrev(GswLocale *locale, const gchar *abbrev, const gchar *osis)
{
    gchar *normalized;
    GPtrArray *candidates;

    if (locale == NULL || abbrev == NULL || *abbrev == '\0' ||
        osis == NULL || *osis == '\0')
        return;

    normalized = locale_normalize(abbrev);
    if (*normalized == '\0') {
        g_free(normalized);
        return;
    }

    candidates = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(candidates, g_strdup(osis));
    g_hash_table_replace(locale->aliases, normalized, candidates);
}

static void
locale_add_pref_abbrev(GswLocale *locale, const gchar *osis,
                       const gchar *abbrev)
{
    gchar *normalized;

    if (locale == NULL || osis == NULL || *osis == '\0' ||
        abbrev == NULL || *abbrev == '\0')
        return;

    normalized = locale_normalize(osis);
    if (*normalized == '\0') {
        g_free(normalized);
        return;
    }
    g_hash_table_replace(locale->pref_abbrevs, normalized, g_strdup(abbrev));
}

static void
locale_seed_builtin_aliases(GswLocale *locale)
{
    guint i;

    /* English aliases are always available, matching SWORD's builtin table.
     * Locale files can override an exact alias key with their own mapping. */
    for (i = 0; i < gsw_builtin_abbrevs_count; i++) {
        const GswV11nAbbrev *entry = &gsw_builtin_abbrevs[i];

        locale_add_abbrev(locale, entry->abbrev, entry->osis);
        locale_add_abbrev(locale, entry->osis, entry->osis);
    }

    {
        const GswV11nBook *books[2] = { gsw_kjv_ot, gsw_kjv_nt };
        guint j;

        for (j = 0; j < G_N_ELEMENTS(books); j++) {
            const GswV11nBook *book;

            for (book = books[j]; book->name != NULL && *book->name != '\0';
                 book++) {
                locale_add_abbrev(locale, book->name, book->osis);
                locale_add_abbrev(locale, book->osis, book->osis);
                locale_add_abbrev(locale, book->abbrev, book->osis);
                locale_add_pref_abbrev(locale, book->abbrev, book->abbrev);
            }
        }
    }
}

static const gchar *
locale_lookup_abbrev(GswLocale *locale, const gchar *abbrev)
{
    gchar *normalized;
    GPtrArray *candidates;
    const gchar *result = NULL;

    if (locale == NULL || abbrev == NULL || *abbrev == '\0')
        return NULL;

    normalized = locale_normalize(abbrev);
    candidates = g_hash_table_lookup(locale->aliases, normalized);
    if (candidates != NULL && candidates->len > 0)
        result = g_ptr_array_index(candidates, 0);
    g_free(normalized);
    return result;
}

static gint
alias_match_compare(gconstpointer a, gconstpointer b)
{
    const GswAliasMatch *left = a;
    const GswAliasMatch *right = b;
    gsize left_len = strlen(left->alias);
    gsize right_len = strlen(right->alias);

    if (left_len != right_len)
        return left_len < right_len ? -1 : 1;
    return g_strcmp0(left->alias, right->alias);
}

static GPtrArray *
locale_lookup_candidates(GswLocale *locale, const gchar *text)
{
    GPtrArray *result = g_ptr_array_new_with_free_func(g_free);
    GHashTable *seen = g_hash_table_new(g_str_hash, g_str_equal);
    gchar *normalized;
    GPtrArray *exact;
    guint i;

    if (locale == NULL || text == NULL || *text == '\0')
        goto out;

    normalized = locale_normalize(text);
    if (*normalized == '\0') {
        g_free(normalized);
        goto out;
    }

    exact = g_hash_table_lookup(locale->aliases, normalized);
    if (exact != NULL) {
        for (i = 0; i < exact->len; i++) {
            const gchar *osis = g_ptr_array_index(exact, i);

            if (!g_hash_table_contains(seen, osis)) {
                g_hash_table_add(seen, (gpointer)osis);
                g_ptr_array_add(result, g_strdup(osis));
            }
        }
    } else {
        GArray *matches = g_array_new(FALSE, FALSE, sizeof(GswAliasMatch));
        GHashTableIter iter;
        gpointer key, value;

        g_hash_table_iter_init(&iter, locale->aliases);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            const gchar *alias = key;
            GPtrArray *candidates = value;

            if (!g_str_has_prefix(alias, normalized))
                continue;
            for (i = 0; i < candidates->len; i++) {
                GswAliasMatch match = {
                    alias,
                    g_ptr_array_index(candidates, i)
                };

                g_array_append_val(matches, match);
            }
        }
        g_array_sort(matches, alias_match_compare);
        for (i = 0; i < matches->len; i++) {
            GswAliasMatch *match = &g_array_index(matches, GswAliasMatch, i);

            if (!g_hash_table_contains(seen, match->osis)) {
                g_hash_table_add(seen, (gpointer)match->osis);
                g_ptr_array_add(result, g_strdup(match->osis));
            }
        }
        g_array_unref(matches);
    }
    g_free(normalized);

out:
    g_hash_table_unref(seen);
    return result;
}

static const gchar *
locale_lookup_pref_abbrev(GswLocale *locale, const gchar *osis)
{
    gchar *normalized;
    const gchar *result;

    if (locale == NULL || osis == NULL || *osis == '\0')
        return NULL;

    normalized = locale_normalize(osis);
    result = g_hash_table_lookup(locale->pref_abbrevs, normalized);
    g_free(normalized);
    return result;
}

static GswLocale *
gsw_locale_new(const gchar *name)
{
    GswLocale *locale = g_new0(GswLocale, 1);

    locale->name = g_strdup(name);
    locale->forward = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                            g_free);
    locale->reverse = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                            g_free);
    locale->aliases = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                            (GDestroyNotify)locale_alias_list_free);
    locale->pref_abbrevs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                 g_free, g_free);
    locale_seed_builtin_aliases(locale);
    return locale;
}

static void
locale_mgr_store(GswLocaleMgr *self, GswLocale *locale)
{
    GswLocaleMgrPrivate *priv = gsw_locale_mgr_get_instance_private(self);
    guint i;
    gboolean replaced = FALSE;

    for (i = 0; i < priv->order->len; i++) {
        GswLocale *old = g_ptr_array_index(priv->order, i);

        if (g_strcmp0(old->name, locale->name) == 0) {
            g_ptr_array_index(priv->order, i) = locale;
            replaced = TRUE;
            break;
        }
    }
    if (!replaced)
        g_ptr_array_add(priv->order, locale);
    g_hash_table_replace(priv->locales, g_strdup(locale->name), locale);
}

static void
gsw_locale_mgr_finalize(GObject *object)
{
    GswLocaleMgr *self = GSW_LOCALE_MGR(object);
    GswLocaleMgrPrivate *priv = gsw_locale_mgr_get_instance_private(self);

    g_clear_pointer(&priv->locales, g_hash_table_unref);
    g_clear_pointer(&priv->order, g_ptr_array_unref);
    g_free(priv->default_locale);

    G_OBJECT_CLASS(gsw_locale_mgr_parent_class)->finalize(object);
}

static void
gsw_locale_mgr_class_init(GswLocaleMgrClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = gsw_locale_mgr_finalize;
}

static void
gsw_locale_mgr_init(GswLocaleMgr *self)
{
    GswLocaleMgrPrivate *priv = gsw_locale_mgr_get_instance_private(self);

    priv->locales = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                          gsw_locale_free);
    priv->order = g_ptr_array_new();
    priv->default_locale = g_strdup("en");
    priv->scanned_paths = FALSE;

    locale_mgr_store(self, gsw_locale_new("en"));
}

GswLocaleMgr *
gsw_locale_mgr_get_default(void)
{
    static gsize once = 0;

    if (g_once_init_enter(&once)) {
        default_manager = g_object_new(GSW_TYPE_LOCALE_MGR, NULL);
        g_once_init_leave(&once, 1);
    }
    return default_manager;
}

static void
locale_mgr_load_dir_if_exists(GswLocaleMgr *self, const gchar *path)
{
    if (path != NULL && g_file_test(path, G_FILE_TEST_IS_DIR))
        gsw_locale_mgr_load_directory(self, path, NULL);
}

static void
gsw_locale_mgr_scan_paths(GswLocaleMgr *self)
{
    GswLocaleMgrPrivate *priv = gsw_locale_mgr_get_instance_private(self);
    const gchar * const *system_dirs;
    const gchar *env;
    gchar *dir;
    guint i;

    if (priv->scanned_paths)
        return;
    priv->scanned_paths = TRUE;

    /* System data first, user data next, explicit GSW_LOCALE_PATH last.
     * A later file with the same locale name replaces the earlier one. */
    system_dirs = g_get_system_data_dirs();
    for (i = 0; system_dirs != NULL && system_dirs[i] != NULL; i++) {
        dir = g_build_filename(system_dirs[i], "gsword", "locales.d", NULL);
        locale_mgr_load_dir_if_exists(self, dir);
        g_free(dir);
    }

    dir = g_build_filename(g_get_user_data_dir(), "gsword", "locales.d", NULL);
    locale_mgr_load_dir_if_exists(self, dir);
    g_free(dir);

    env = g_getenv("GSW_LOCALE_PATH");
    if (env != NULL && *env != '\0') {
        gchar **paths = g_strsplit(env, G_SEARCHPATH_SEPARATOR_S, -1);

        for (i = 0; paths[i] != NULL; i++) {
            if (*paths[i] != '\0')
                locale_mgr_load_dir_if_exists(self, paths[i]);
        }
        g_strfreev(paths);
    }
}

gboolean
gsw_locale_mgr_load_file(GswLocaleMgr *self, const gchar *path, GError **error)
{
    GswConfig *config;
    GswLocale *locale;
    gchar **keys;
    const gchar *name;
    guint i;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), FALSE);
    g_return_val_if_fail(path != NULL, FALSE);

    config = gsw_config_new_from_file(path, error);
    if (config == NULL)
        return FALSE;

    name = gsw_config_get(config, "Meta", "Name");
    if (name == NULL || *name == '\0') {
        gchar *base = g_path_get_basename(path);
        gchar *dot = strchr(base, '.');

        if (dot != NULL)
            *dot = '\0';
        locale = gsw_locale_new(base);
        g_free(base);
    } else {
        locale = gsw_locale_new(name);
    }

    keys = gsw_config_get_keys(config, "Text");
    for (i = 0; keys != NULL && keys[i] != NULL; i++) {
        const gchar *translated = gsw_config_get(config, "Text", keys[i]);

        if (translated != NULL) {
            g_hash_table_replace(locale->forward, g_strdup(keys[i]),
                                 g_strdup(translated));
            g_hash_table_replace(locale->reverse, g_strdup(translated),
                                 g_strdup(keys[i]));
        }
    }
    g_strfreev(keys);

    keys = gsw_config_get_keys(config, "Book Abbrevs");
    for (i = 0; keys != NULL && keys[i] != NULL; i++) {
        const gchar *osis = gsw_config_get(config, "Book Abbrevs", keys[i]);

        locale_add_abbrev(locale, keys[i], osis);
    }
    g_strfreev(keys);

    keys = gsw_config_get_keys(config, "Pref Abbrevs");
    for (i = 0; keys != NULL && keys[i] != NULL; i++) {
        const gchar *abbrev = gsw_config_get(config, "Pref Abbrevs", keys[i]);

        locale_add_pref_abbrev(locale, keys[i], abbrev);
    }
    g_strfreev(keys);

    g_object_unref(config);
    locale_mgr_store(self, locale);
    return TRUE;
}

guint
gsw_locale_mgr_load_directory(GswLocaleMgr *self, const gchar *path,
                              GError **error)
{
    GDir *dir;
    const gchar *name;
    guint count = 0;
    GError *local_error = NULL;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), 0);
    g_return_val_if_fail(path != NULL, 0);

    dir = g_dir_open(path, 0, &local_error);
    if (dir == NULL) {
        g_propagate_error(error, local_error);
        return 0;
    }

    while ((name = g_dir_read_name(dir)) != NULL) {
        if (g_str_has_suffix(name, ".conf")) {
            gchar *file = g_build_filename(path, name, NULL);
            GError *file_error = NULL;

            if (gsw_locale_mgr_load_file(self, file, &file_error))
                count++;
            else
                g_clear_error(&file_error);
            g_free(file);
        }
    }
    g_dir_close(dir);
    return count;
}

GPtrArray *
gsw_locale_mgr_get_locales(GswLocaleMgr *self)
{
    GswLocaleMgrPrivate *priv;
    GPtrArray *result;
    guint i;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), NULL);
    gsw_locale_mgr_scan_paths(self);
    priv = gsw_locale_mgr_get_instance_private(self);
    result = g_ptr_array_new_with_free_func(g_free);
    for (i = 0; i < priv->order->len; i++) {
        GswLocale *locale = g_ptr_array_index(priv->order, i);

        g_ptr_array_add(result, g_strdup(locale->name));
    }
    return result;
}

const gchar *
gsw_locale_mgr_get_default_locale(GswLocaleMgr *self)
{
    GswLocaleMgrPrivate *priv;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), NULL);
    priv = gsw_locale_mgr_get_instance_private(self);
    return priv->default_locale;
}

void
gsw_locale_mgr_set_default_locale(GswLocaleMgr *self, const gchar *name)
{
    GswLocaleMgrPrivate *priv;

    g_return_if_fail(GSW_IS_LOCALE_MGR(self));
    gsw_locale_mgr_scan_paths(self);
    priv = gsw_locale_mgr_get_instance_private(self);
    g_free(priv->default_locale);
    priv->default_locale = g_strdup(name != NULL && *name != '\0' ? name : "en");
}

static GswLocale *
locale_mgr_resolve(GswLocaleMgr *self, const gchar *locale_name)
{
    GswLocaleMgrPrivate *priv;
    const gchar *name;
    GswLocale *locale;

    gsw_locale_mgr_scan_paths(self);
    priv = gsw_locale_mgr_get_instance_private(self);
    name = locale_name != NULL && *locale_name != '\0'
         ? locale_name : priv->default_locale;
    if (name == NULL || *name == '\0')
        name = "en";
    locale = g_hash_table_lookup(priv->locales, name);
    if (locale == NULL)
        locale = g_hash_table_lookup(priv->locales, "en");
    return locale;
}

const gchar *
gsw_locale_mgr_translate(GswLocaleMgr *self, const gchar *text,
                         const gchar *locale_name)
{
    GswLocale *locale;
    const gchar *result;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), NULL);
    g_return_val_if_fail(text != NULL, NULL);

    locale = locale_mgr_resolve(self, locale_name);
    if (locale == NULL)
        return text;
    if (g_str_has_prefix(text, "prefAbbr_")) {
        result = locale_lookup_pref_abbrev(locale, text + 9);
        if (result != NULL)
            return result;
    }
    result = g_hash_table_lookup(locale->forward, text);
    return result != NULL ? result : text;
}

const gchar *
gsw_locale_mgr_reverse(GswLocaleMgr *self, const gchar *translated,
                       const gchar *locale_name)
{
    GswLocale *locale;
    const gchar *result;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), NULL);
    g_return_val_if_fail(translated != NULL, NULL);

    locale = locale_mgr_resolve(self, locale_name);
    if (locale == NULL)
        return translated;
    result = g_hash_table_lookup(locale->reverse, translated);
    return result != NULL ? result : translated;
}

const gchar *
gsw_locale_mgr_translate_abbreviation(GswLocaleMgr *self,
                                      const gchar *abbreviation,
                                      const gchar *locale_name)
{
    GswLocale *locale;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), NULL);
    g_return_val_if_fail(abbreviation != NULL, NULL);

    locale = locale_mgr_resolve(self, locale_name);
    return locale_lookup_abbrev(locale, abbreviation);
}

GPtrArray *
gsw_locale_mgr_get_book_candidates(GswLocaleMgr *self, const gchar *text,
                                   const gchar *locale_name)
{
    GswLocale *locale;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), NULL);
    g_return_val_if_fail(text != NULL, NULL);

    locale = locale_mgr_resolve(self, locale_name);
    if (locale == NULL)
        return g_ptr_array_new_with_free_func(g_free);
    return locale_lookup_candidates(locale, text);
}

const gchar *
gsw_locale_mgr_translate_preferred_abbreviation(GswLocaleMgr *self,
                                                const gchar *abbreviation,
                                                const gchar *locale_name)
{
    GswLocale *locale;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), NULL);
    g_return_val_if_fail(abbreviation != NULL, NULL);

    locale = locale_mgr_resolve(self, locale_name);
    return locale_lookup_pref_abbrev(locale, abbreviation);
}
