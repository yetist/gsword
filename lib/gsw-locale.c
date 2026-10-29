/*
 * gsw-locale.c: SWORD locale/book-name translation
 */

#include <gsword/gsw-locale.h>
#include <gsword/gsw-config.h>
#include <gsword/gsw-error.h>

#include <string.h>

typedef struct {
    gchar *name;
    GHashTable *forward; /* English -> localized */
    GHashTable *reverse; /* localized -> English */
    GHashTable *abbrevs; /* localized abbreviation -> OSIS */
    GHashTable *abbrevs_folded; /* case-folded abbreviation -> OSIS */
    GHashTable *pref_abbrevs; /* OSIS abbreviation -> localized */
    GHashTable *pref_abbrevs_folded; /* case-folded OSIS -> localized */
} GswLocale;

struct _GswLocaleMgr {
    GObject parent_instance;
};

typedef struct {
    GHashTable *locales; /* name -> GswLocale* */
    GPtrArray *order;
    gchar *default_locale;
    gboolean scanned_system;
} GswLocaleMgrPrivate;

static GswLocaleMgr *default_manager = NULL;

G_DEFINE_TYPE_WITH_PRIVATE(GswLocaleMgr, gsw_locale_mgr, G_TYPE_OBJECT)

static void
gsw_locale_free(gpointer data)
{
    GswLocale *locale = data;

    if (locale == NULL)
        return;
    g_free(locale->name);
    g_clear_pointer(&locale->forward, g_hash_table_unref);
    g_clear_pointer(&locale->reverse, g_hash_table_unref);
    g_clear_pointer(&locale->abbrevs, g_hash_table_unref);
    g_clear_pointer(&locale->abbrevs_folded, g_hash_table_unref);
    g_clear_pointer(&locale->pref_abbrevs, g_hash_table_unref);
    g_clear_pointer(&locale->pref_abbrevs_folded, g_hash_table_unref);
    g_free(locale);
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
    locale->abbrevs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                            g_free);
    locale->abbrevs_folded = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                   g_free, g_free);
    locale->pref_abbrevs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                 g_free, g_free);
    locale->pref_abbrevs_folded = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                        g_free, g_free);
    return locale;
}

static void
locale_add_abbrev(GswLocale *locale, const gchar *abbrev, const gchar *osis)
{
    gchar *folded;

    if (abbrev == NULL || *abbrev == '\0' || osis == NULL || *osis == '\0')
        return;
    g_hash_table_replace(locale->abbrevs, g_strdup(abbrev), g_strdup(osis));
    folded = g_utf8_casefold(abbrev, -1);
    g_hash_table_replace(locale->abbrevs_folded, folded, g_strdup(osis));
}

static void
locale_add_pref_abbrev(GswLocale *locale, const gchar *osis,
                       const gchar *abbrev)
{
    gchar *folded;

    if (osis == NULL || *osis == '\0' || abbrev == NULL || *abbrev == '\0')
        return;
    g_hash_table_replace(locale->pref_abbrevs, g_strdup(osis),
                         g_strdup(abbrev));
    folded = g_utf8_casefold(osis, -1);
    g_hash_table_replace(locale->pref_abbrevs_folded, folded,
                         g_strdup(abbrev));
}

static const gchar *
locale_lookup_abbrev(GswLocale *locale, const gchar *abbrev)
{
    const gchar *result;
    gchar *folded;

    if (locale == NULL || abbrev == NULL || *abbrev == '\0')
        return NULL;
    result = g_hash_table_lookup(locale->abbrevs, abbrev);
    if (result != NULL)
        return result;
    folded = g_utf8_casefold(abbrev, -1);
    result = g_hash_table_lookup(locale->abbrevs_folded, folded);
    g_free(folded);
    return result;
}

static const gchar *
locale_lookup_pref_abbrev(GswLocale *locale, const gchar *osis)
{
    const gchar *result;
    gchar *folded;

    if (locale == NULL || osis == NULL || *osis == '\0')
        return NULL;
    result = g_hash_table_lookup(locale->pref_abbrevs, osis);
    if (result != NULL)
        return result;
    folded = g_utf8_casefold(osis, -1);
    result = g_hash_table_lookup(locale->pref_abbrevs_folded, folded);
    g_free(folded);
    return result;
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
    priv->default_locale = NULL;
    priv->scanned_system = FALSE;
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
gsw_locale_mgr_scan_system(GswLocaleMgr *self)
{
    GswLocaleMgrPrivate *priv = gsw_locale_mgr_get_instance_private(self);
    const gchar *env;
    const gchar *dirs[4] = { NULL, NULL, NULL, NULL };
    guint i;

    if (priv->scanned_system)
        return;
    priv->scanned_system = TRUE;

    env = g_getenv("SWORD_PATH");
    if (env != NULL && *env != '\0') {
        gchar *path = g_build_filename(env, "locales.d", NULL);
        dirs[0] = path;
    }
    dirs[1] = "/usr/share/sword/locales.d";
    dirs[2] = "/usr/local/share/sword/locales.d";
    {
        gchar *home = g_build_filename(g_get_home_dir(), ".sword", "locales.d", NULL);
        dirs[3] = home;
    }

    for (i = 0; i < 4; i++) {
        if (dirs[i] != NULL && g_file_test(dirs[i], G_FILE_TEST_IS_DIR))
            gsw_locale_mgr_load_directory(self, dirs[i], NULL);
    }

    /* dirs[0] and dirs[3] are allocated; avoid freeing static strings. */
    g_free((gpointer)dirs[0]);
    g_free((gpointer)dirs[3]);
}

gboolean
gsw_locale_mgr_load_file(GswLocaleMgr *self, const gchar *path, GError **error)
{
    GswLocaleMgrPrivate *priv;
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

    priv = gsw_locale_mgr_get_instance_private(self);
    {
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
    }
    g_hash_table_replace(priv->locales, g_strdup(locale->name), locale);
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
    gsw_locale_mgr_scan_system(self);
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
    gsw_locale_mgr_scan_system(self);
    priv = gsw_locale_mgr_get_instance_private(self);
    g_free(priv->default_locale);
    priv->default_locale = g_strdup(name);
}

static GswLocale *
locale_mgr_resolve(GswLocaleMgr *self, const gchar *locale_name)
{
    GswLocaleMgrPrivate *priv;
    const gchar *name;

    priv = gsw_locale_mgr_get_instance_private(self);
    if (locale_name == NULL && priv->default_locale == NULL)
        return NULL;
    gsw_locale_mgr_scan_system(self);
    priv = gsw_locale_mgr_get_instance_private(self);
    name = locale_name != NULL ? locale_name : priv->default_locale;
    if (name == NULL)
        return NULL;
    return g_hash_table_lookup(priv->locales, name);
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
