/*
 * gsw-locale.c: native libgsword locale and book-name translation
 */

#include <gsword/gsw-locale.h>
#include <gsword/gsw-error.h>
#include "gsw-versification-data.h"

#include <string.h>

typedef struct {
    gchar *name;
    gchar *fallback;
    GPtrArray *locale_aliases;
    GHashTable *forward; /* English -> localized */
    GHashTable *reverse; /* localized -> English */
    GHashTable *aliases; /* normalized alias -> GPtrArray of OSIS names */
    GHashTable *pref_abbrevs; /* normalized preferred abbreviation -> localized */
} GswLocale;

typedef struct {
    const gchar *alias;
    const gchar *osis;
} GswAliasMatch;

struct _GswLocaleMgr {
    GObject parent_instance;
};

typedef struct {
    GHashTable *locales; /* canonical name -> GswLocale* */
    GHashTable *locale_aliases; /* normalized alias -> canonical name */
    GPtrArray *order;
    gchar *default_locale;
    gboolean scanned_paths;
    GMutex scan_mutex;
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

static gchar *
locale_id_normalize(const gchar *text)
{
    gchar *normalized = locale_normalize(text);

    g_strdelimit(normalized, "_", '-');
    return normalized;
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
    g_free(locale->fallback);
    g_clear_pointer(&locale->locale_aliases, g_ptr_array_unref);
    g_clear_pointer(&locale->forward, g_hash_table_unref);
    g_clear_pointer(&locale->reverse, g_hash_table_unref);
    g_clear_pointer(&locale->aliases, g_hash_table_unref);
    g_clear_pointer(&locale->pref_abbrevs, g_hash_table_unref);
    g_free(locale);
}

static const GswV11nBook *
locale_find_kjv_book(const gchar *osis)
{
    const GswV11nBook *books[2] = { gsw_kjv_ot, gsw_kjv_nt };
    guint i;

    for (i = 0; i < G_N_ELEMENTS(books); i++) {
        const GswV11nBook *book;

        for (book = books[i]; book->name != NULL && *book->name != '\0';
             book++) {
            if (g_strcmp0(book->osis, osis) == 0)
                return book;
        }
    }
    return NULL;
}

static const gchar *
locale_english_name(const gchar *osis)
{
    const GswV11nBook *book = locale_find_kjv_book(osis);
    const gchar *best = NULL;
    guint i;

    if (book != NULL)
        return book->name;

    for (i = 0; i < gsw_builtin_abbrevs_count; i++) {
        const GswV11nAbbrev *entry = &gsw_builtin_abbrevs[i];

        if (g_strcmp0(entry->osis, osis) != 0)
            continue;
        if (best == NULL || strlen(entry->abbrev) > strlen(best))
            best = entry->abbrev;
    }
    return best != NULL ? best : osis;
}

static const gchar *
locale_english_pref(const gchar *osis)
{
    const GswV11nBook *book = locale_find_kjv_book(osis);

    return book != NULL ? book->abbrev : osis;
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
     * Locale data can override an exact alias key with its own mapping. */
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

static GswLocale *
gsw_locale_new(const gchar *name)
{
    GswLocale *locale = g_new0(GswLocale, 1);

    locale->name = g_strdup(name);
    locale->locale_aliases = g_ptr_array_new_with_free_func(g_free);
    locale->forward = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                            g_free);
    locale->reverse = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                            g_free);
    locale->aliases = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free,
        (GDestroyNotify)locale_alias_list_free);
    locale->pref_abbrevs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                 g_free, g_free);
    locale_seed_builtin_aliases(locale);
    return locale;
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

static void
locale_mgr_store(GswLocaleMgr *self, GswLocale *locale)
{
    GswLocaleMgrPrivate *priv = gsw_locale_mgr_get_instance_private(self);
    gchar *canonical;
    guint i;
    gboolean replaced = FALSE;

    canonical = locale_id_normalize(locale->name);
    g_hash_table_replace(priv->locale_aliases, canonical,
                         g_strdup(locale->name));
    for (i = 0; i < locale->locale_aliases->len; i++) {
        gchar *alias = locale_id_normalize(
            g_ptr_array_index(locale->locale_aliases, i));

        g_hash_table_replace(priv->locale_aliases, alias,
                             g_strdup(locale->name));
    }

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
    g_clear_pointer(&priv->locale_aliases, g_hash_table_unref);
    g_clear_pointer(&priv->order, g_ptr_array_unref);
    g_free(priv->default_locale);
    g_mutex_clear(&priv->scan_mutex);

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
    priv->locale_aliases = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                 g_free, g_free);
    priv->order = g_ptr_array_new();
    priv->default_locale = g_strdup("en");
    priv->scanned_paths = FALSE;
    g_mutex_init(&priv->scan_mutex);

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
    if (path != NULL && *path != '\0' &&
        g_file_test(path, G_FILE_TEST_IS_DIR))
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

    g_mutex_lock(&priv->scan_mutex);
    if (priv->scanned_paths) {
        g_mutex_unlock(&priv->scan_mutex);
        return;
    }
    priv->scanned_paths = TRUE;

    /* Installed data, XDG system data, user data, then explicit override. */
#ifdef GSW_LOCALE_DIR
    locale_mgr_load_dir_if_exists(self, GSW_LOCALE_DIR);
#endif

    system_dirs = g_get_system_data_dirs();
    for (i = 0; system_dirs != NULL && system_dirs[i] != NULL; i++) {
        dir = g_build_filename(system_dirs[i], "libgsword", "locales.d", NULL);
        locale_mgr_load_dir_if_exists(self, dir);
        g_free(dir);
    }

    dir = g_build_filename(g_get_user_data_dir(), "libgsword", "locales.d",
                           NULL);
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
    g_mutex_unlock(&priv->scan_mutex);
}

static gchar *
locale_file_basename(const gchar *path)
{
    gchar *base = g_path_get_basename(path);
    gchar *dot = strrchr(base, '.');

    if (dot != NULL)
        *dot = '\0';
    return base;
}

gboolean
gsw_locale_mgr_load_file(GswLocaleMgr *self, const gchar *path, GError **error)
{
    GKeyFile *key_file;
    GswLocale *locale = NULL;
    gchar *id = NULL;
    gchar *fallback = NULL;
    gchar **aliases = NULL;
    gchar **groups = NULL;
    gchar **keys = NULL;
    gsize count = 0;
    gint format = 0;
    GError *local_error = NULL;
    gboolean ok = FALSE;
    guint i;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), FALSE);
    g_return_val_if_fail(path != NULL, FALSE);

    key_file = g_key_file_new();
    if (!g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, error))
        goto out;

    format = g_key_file_get_integer(key_file, "Locale", "Format",
                                    &local_error);
    if (local_error != NULL || format != 1) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "%s: unsupported locale Format", path);
        g_clear_error(&local_error);
        goto out;
    }

    id = g_key_file_get_string(key_file, "Locale", "Id", NULL);
    if (id == NULL || *id == '\0') {
        g_free(id);
        id = locale_file_basename(path);
    }
    locale = gsw_locale_new(id);
    g_free(id);
    id = NULL;

    fallback = g_key_file_get_string(key_file, "Locale", "Fallback", NULL);
    if (fallback != NULL && *fallback != '\0')
        locale->fallback = g_strdup(fallback);
    g_free(fallback);
    fallback = NULL;

    aliases = g_key_file_get_string_list(key_file, "Locale", "Aliases",
                                         &count, NULL);
    for (i = 0; aliases != NULL && aliases[i] != NULL; i++)
        g_ptr_array_add(locale->locale_aliases, g_strdup(aliases[i]));
    g_strfreev(aliases);
    aliases = NULL;

    keys = g_key_file_get_keys(key_file, "Strings", &count, NULL);
    for (i = 0; keys != NULL && keys[i] != NULL; i++) {
        gchar *translated = g_key_file_get_string(key_file, "Strings",
                                                  keys[i], NULL);

        if (translated == NULL)
            continue;
        g_hash_table_replace(locale->forward, g_strdup(keys[i]),
                             g_strdup(translated));
        g_hash_table_replace(locale->reverse, g_strdup(translated),
                             g_strdup(keys[i]));
        g_free(translated);
    }
    g_strfreev(keys);
    keys = NULL;

    groups = g_key_file_get_groups(key_file, &count);
    for (i = 0; groups != NULL && groups[i] != NULL; i++) {
        const gchar *group = groups[i];
        const gchar *osis;
        gchar *name;
        gchar *abbrev;
        gchar **book_aliases;

        if (!g_str_has_prefix(group, "Book "))
            continue;
        osis = group + 5;
        if (*osis == '\0')
            continue;

        name = g_key_file_get_string(key_file, group, "Name", NULL);
        abbrev = g_key_file_get_string(key_file, group, "Abbrev", NULL);
        book_aliases = g_key_file_get_string_list(key_file, group, "Aliases",
                                                  NULL, NULL);

        if (name != NULL && *name != '\0') {
            const gchar *english = locale_english_name(osis);

            locale_add_abbrev(locale, name, osis);
            if (english != NULL && *english != '\0') {
                g_hash_table_replace(locale->forward, g_strdup(english),
                                     g_strdup(name));
                g_hash_table_replace(locale->reverse, g_strdup(name),
                                     g_strdup(english));
            }
        }
        if (abbrev != NULL && *abbrev != '\0') {
            const gchar *english_pref = locale_english_pref(osis);

            locale_add_abbrev(locale, abbrev, osis);
            locale_add_pref_abbrev(locale, osis, abbrev);
            if (g_strcmp0(english_pref, osis) != 0)
                locale_add_pref_abbrev(locale, english_pref, abbrev);
        }
        for (guint j = 0; book_aliases != NULL && book_aliases[j] != NULL; j++)
            locale_add_abbrev(locale, book_aliases[j], osis);
        locale_add_abbrev(locale, osis, osis);

        g_strfreev(book_aliases);
        g_free(name);
        g_free(abbrev);
    }
    g_strfreev(groups);
    groups = NULL;

    locale_mgr_store(self, locale);
    locale = NULL;
    ok = TRUE;

out:
    g_strfreev(groups);
    g_strfreev(keys);
    g_strfreev(aliases);
    g_free(fallback);
    g_free(id);
    g_clear_pointer(&locale, gsw_locale_free);
    g_key_file_unref(key_file);
    return ok;
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
locale_mgr_lookup(GswLocaleMgr *self, const gchar *name)
{
    GswLocaleMgrPrivate *priv = gsw_locale_mgr_get_instance_private(self);
    GswLocale *locale;
    gchar *normalized;
    const gchar *canonical;

    if (name == NULL || *name == '\0')
        return NULL;
    locale = g_hash_table_lookup(priv->locales, name);
    if (locale != NULL)
        return locale;

    normalized = locale_id_normalize(name);
    canonical = g_hash_table_lookup(priv->locale_aliases, normalized);
    if (canonical != NULL)
        locale = g_hash_table_lookup(priv->locales, canonical);
    g_free(normalized);
    return locale;
}

static gboolean
locale_id_matches_language(const gchar *id, const gchar *language)
{
    gsize len;

    if (id == NULL || language == NULL || *id == '\0' || *language == '\0')
        return FALSE;
    if (g_strcmp0(id, language) == 0)
        return TRUE;
    len = strlen(language);
    if (g_str_has_prefix(id, language) && id[len] == '-')
        return TRUE;
    len = strlen(id);
    if (g_str_has_prefix(language, id) && language[len] == '-')
        return TRUE;
    return FALSE;
}

static GswLocale *
locale_mgr_lookup_language(GswLocaleMgr *self, const gchar *name)
{
    GswLocaleMgrPrivate *priv = gsw_locale_mgr_get_instance_private(self);
    GswLocale *best = NULL;
    gchar *best_id = NULL;
    gchar *normalized;
    guint i;

    if (name == NULL || *name == '\0')
        return NULL;
    normalized = locale_id_normalize(name);
    if (strlen(normalized) < 2) {
        g_free(normalized);
        return NULL;
    }

    /* A bare language such as "zh" can match several scripts. Prefer the
     * shortest, then lexicographically first locale so the result does not
     * depend on locale load order. */
    for (i = 0; i < priv->order->len; i++) {
        GswLocale *locale = g_ptr_array_index(priv->order, i);
        gchar *candidate = locale_id_normalize(locale->name);
        gsize candidate_len = strlen(candidate);

        if (locale_id_matches_language(candidate, normalized) &&
            (best == NULL || candidate_len < strlen(best_id) ||
             (candidate_len == strlen(best_id) &&
              strcmp(candidate, best_id) < 0))) {
            g_free(best_id);
            best_id = g_strdup(candidate);
            best = locale;
        }
        g_free(candidate);
    }
    g_free(best_id);
    g_free(normalized);
    return best;
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
    locale = locale_mgr_lookup(self, name);
    if (locale == NULL)
        locale = locale_mgr_lookup_language(self, name);
    if (locale == NULL)
        locale = locale_mgr_lookup(self, "en");
    return locale;
}

static GswLocale *
locale_mgr_next_fallback(GswLocaleMgr *self, GswLocale *locale)
{
    if (locale == NULL || locale->fallback == NULL ||
        g_strcmp0(locale->fallback, locale->name) == 0)
        return NULL;
    return locale_mgr_lookup(self, locale->fallback);
}

const gchar *
gsw_locale_mgr_translate(GswLocaleMgr *self, const gchar *text,
                         const gchar *locale_name)
{
    GswLocale *locale;
    GHashTable *seen;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), NULL);
    g_return_val_if_fail(text != NULL, NULL);

    locale = locale_mgr_resolve(self, locale_name);
    if (locale == NULL)
        return text;

    seen = g_hash_table_new(g_str_hash, g_str_equal);
    while (locale != NULL && !g_hash_table_contains(seen, locale->name)) {
        const gchar *result = NULL;

        g_hash_table_add(seen, locale->name);
        if (g_str_has_prefix(text, "prefAbbr_"))
            result = locale_lookup_pref_abbrev(locale, text + 9);
        if (result == NULL)
            result = g_hash_table_lookup(locale->forward, text);
        if (result != NULL) {
            g_hash_table_unref(seen);
            return result;
        }
        locale = locale_mgr_next_fallback(self, locale);
    }
    g_hash_table_unref(seen);
    return text;
}

const gchar *
gsw_locale_mgr_reverse(GswLocaleMgr *self, const gchar *translated,
                       const gchar *locale_name)
{
    GswLocale *locale;
    GHashTable *seen;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), NULL);
    g_return_val_if_fail(translated != NULL, NULL);

    locale = locale_mgr_resolve(self, locale_name);
    if (locale == NULL)
        return translated;

    seen = g_hash_table_new(g_str_hash, g_str_equal);
    while (locale != NULL && !g_hash_table_contains(seen, locale->name)) {
        const gchar *result;

        g_hash_table_add(seen, locale->name);
        result = g_hash_table_lookup(locale->reverse, translated);
        if (result != NULL) {
            g_hash_table_unref(seen);
            return result;
        }
        locale = locale_mgr_next_fallback(self, locale);
    }
    g_hash_table_unref(seen);
    return translated;
}

const gchar *
gsw_locale_mgr_translate_abbreviation(GswLocaleMgr *self,
                                      const gchar *abbreviation,
                                      const gchar *locale_name)
{
    GswLocale *locale;
    GHashTable *seen;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), NULL);
    g_return_val_if_fail(abbreviation != NULL, NULL);

    locale = locale_mgr_resolve(self, locale_name);
    if (locale == NULL)
        return NULL;

    seen = g_hash_table_new(g_str_hash, g_str_equal);
    while (locale != NULL && !g_hash_table_contains(seen, locale->name)) {
        const gchar *result;

        g_hash_table_add(seen, locale->name);
        result = locale_lookup_abbrev(locale, abbreviation);
        if (result != NULL) {
            g_hash_table_unref(seen);
            return result;
        }
        locale = locale_mgr_next_fallback(self, locale);
    }
    g_hash_table_unref(seen);
    return NULL;
}

GPtrArray *
gsw_locale_mgr_get_book_candidates(GswLocaleMgr *self, const gchar *text,
                                   const gchar *locale_name)
{
    GswLocaleMgrPrivate *priv;
    GswLocale *locale;
    GPtrArray *result;
    GHashTable *seen_locales;
    GHashTable *seen_osis;
    guint i;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), NULL);
    g_return_val_if_fail(text != NULL, NULL);

    result = g_ptr_array_new_with_free_func(g_free);
    locale = locale_mgr_resolve(self, locale_name);
    if (locale == NULL)
        return result;

    seen_locales = g_hash_table_new(g_str_hash, g_str_equal);
    seen_osis = g_hash_table_new(g_str_hash, g_str_equal);

    while (locale != NULL &&
           !g_hash_table_contains(seen_locales, locale->name)) {
        GPtrArray *candidates;

        g_hash_table_add(seen_locales, locale->name);
        candidates = locale_lookup_candidates(locale, text);
        for (i = 0; i < candidates->len; i++) {
            const gchar *osis = g_ptr_array_index(candidates, i);

            if (!g_hash_table_contains(seen_osis, osis)) {
                g_hash_table_add(seen_osis, (gpointer)osis);
                g_ptr_array_add(result, g_strdup(osis));
            }
        }
        g_ptr_array_unref(candidates);
        if (result->len > 0)
            break;
        locale = locale_mgr_next_fallback(self, locale);
    }

    /* Last resort for user-entered localized names: search every loaded
     * locale, not just the selected one. This keeps input such as
     * "启示录 1:1" usable while the UI default locale is still English. */
    if (result->len == 0) {
        priv = gsw_locale_mgr_get_instance_private(self);
        for (i = 0; i < priv->order->len; i++) {
            GPtrArray *candidates;
            guint j;

            locale = g_ptr_array_index(priv->order, i);
            if (g_hash_table_contains(seen_locales, locale->name))
                continue;
            g_hash_table_add(seen_locales, locale->name);
            candidates = locale_lookup_candidates(locale, text);
            for (j = 0; j < candidates->len; j++) {
                const gchar *osis = g_ptr_array_index(candidates, j);

                if (!g_hash_table_contains(seen_osis, osis)) {
                    g_hash_table_add(seen_osis, (gpointer)osis);
                    g_ptr_array_add(result, g_strdup(osis));
                }
            }
            g_ptr_array_unref(candidates);
            if (result->len > 0)
                break;
        }
    }

    g_hash_table_unref(seen_osis);
    g_hash_table_unref(seen_locales);
    return result;
}

const gchar *
gsw_locale_mgr_translate_preferred_abbreviation(GswLocaleMgr *self,
                                                const gchar *abbreviation,
                                                const gchar *locale_name)
{
    GswLocale *locale;
    GHashTable *seen;

    g_return_val_if_fail(GSW_IS_LOCALE_MGR(self), NULL);
    g_return_val_if_fail(abbreviation != NULL, NULL);

    locale = locale_mgr_resolve(self, locale_name);
    if (locale == NULL)
        return NULL;

    seen = g_hash_table_new(g_str_hash, g_str_equal);
    while (locale != NULL && !g_hash_table_contains(seen, locale->name)) {
        const gchar *result;

        g_hash_table_add(seen, locale->name);
        result = locale_lookup_pref_abbrev(locale, abbreviation);
        if (result != NULL) {
            g_hash_table_unref(seen);
            return result;
        }
        locale = locale_mgr_next_fallback(self, locale);
    }
    g_hash_table_unref(seen);
    return NULL;
}
