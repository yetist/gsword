/*
 * gsw-manager.c: an installed SWORD module library
 */

#include <gsword/gsw-manager.h>
#include <gsword/gsw-error.h>
#include "gsw-utils.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>

struct _GswManager {
    GObject parent_instance;
};

typedef struct {
    gchar *prefix_path;
    gchar *config_path;
    GswConfig *config;
    GHashTable *infos;        /* name -> GswModuleInfo* */
    GHashTable *config_files; /* name -> gchar* */
    GHashTable *modules;      /* name -> GswModule* */
    GPtrArray *info_order;    /* GswModuleInfo* pointers */
    GPtrArray *augment_paths; /* gchar* */
    gboolean config_is_dir;
    gboolean loaded;
} GswManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GswManager, gsw_manager, G_TYPE_OBJECT)

static void
gsw_manager_clear(GswManager *self)
{
    GswManagerPrivate *priv = gsw_manager_get_instance_private(self);

    g_hash_table_remove_all(priv->infos);
    g_hash_table_remove_all(priv->config_files);
    g_hash_table_remove_all(priv->modules);
    g_ptr_array_set_size(priv->info_order, 0);
    g_clear_object(&priv->config);
    priv->config = gsw_config_new();
}

static void
gsw_manager_finalize(GObject *object)
{
    GswManager *self = GSW_MANAGER(object);
    GswManagerPrivate *priv = gsw_manager_get_instance_private(self);

    g_free(priv->prefix_path);
    g_free(priv->config_path);
    g_clear_object(&priv->config);
    g_clear_pointer(&priv->infos, g_hash_table_unref);
    g_clear_pointer(&priv->config_files, g_hash_table_unref);
    g_clear_pointer(&priv->modules, g_hash_table_unref);
    g_clear_pointer(&priv->info_order, g_ptr_array_unref);
    g_clear_pointer(&priv->augment_paths, g_ptr_array_unref);

    G_OBJECT_CLASS(gsw_manager_parent_class)->finalize(object);
}

static void
gsw_manager_class_init(GswManagerClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = gsw_manager_finalize;
}

static void
gsw_manager_init(GswManager *self)
{
    GswManagerPrivate *priv = gsw_manager_get_instance_private(self);

    priv->config = gsw_config_new();
    priv->infos = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                        g_object_unref);
    priv->config_files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    priv->modules = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                          g_object_unref);
    priv->info_order = g_ptr_array_new();
    priv->augment_paths = g_ptr_array_new_with_free_func(g_free);
}

static gboolean
gsw_manager_looks_like_config(GswManager *self, const gchar *path)
{
    GswManagerPrivate *priv = gsw_manager_get_instance_private(self);
    GStatBuf st;

    if (g_stat(path, &st) != 0)
        return FALSE;

    if (S_ISREG(st.st_mode)) {
        g_free(priv->config_path);
        priv->config_path = g_strdup(path);
        priv->config_is_dir = FALSE;
        {
            gchar *dir = g_path_get_dirname(path);
            g_free(priv->prefix_path);
            priv->prefix_path = g_canonicalize_filename(dir, NULL);
            g_free(dir);
        }
        return TRUE;
    }

    if (S_ISDIR(st.st_mode)) {
        gchar *base = g_path_get_basename(path);

        if (g_strcmp0(base, "mods.d") == 0) {
            g_free(priv->config_path);
            priv->config_path = g_strdup(path);
            priv->config_is_dir = TRUE;
            {
                gchar *dir = g_path_get_dirname(path);
                g_free(priv->prefix_path);
                priv->prefix_path = g_canonicalize_filename(dir, NULL);
                g_free(dir);
            }
            g_free(base);
            return TRUE;
        }
        g_free(base);

        {
            gchar *mods_conf = g_build_filename(path, "mods.conf", NULL);
            gchar *mods_dir = g_build_filename(path, "mods.d", NULL);
            gboolean found = FALSE;

            if (g_file_test(mods_conf, G_FILE_TEST_IS_REGULAR)) {
                g_free(priv->config_path);
                priv->config_path = mods_conf;
                mods_conf = NULL;
                priv->config_is_dir = FALSE;
                found = TRUE;
            } else if (g_file_test(mods_dir, G_FILE_TEST_IS_DIR)) {
                g_free(priv->config_path);
                priv->config_path = mods_dir;
                mods_dir = NULL;
                priv->config_is_dir = TRUE;
                found = TRUE;
            }

            g_free(mods_conf);
            g_free(mods_dir);

            if (found) {
                g_free(priv->prefix_path);
                priv->prefix_path = g_canonicalize_filename(path, NULL);
                return TRUE;
            }
        }
    }

    return FALSE;
}

static gboolean
gsw_manager_set_empty_prefix(GswManager *self, const gchar *path)
{
    GswManagerPrivate *priv = gsw_manager_get_instance_private(self);
    gchar *mods_dir;

    g_free(priv->prefix_path);
    priv->prefix_path = g_canonicalize_filename(path, NULL);
    mods_dir = g_build_filename(priv->prefix_path, "mods.d", NULL);
    g_free(priv->config_path);
    priv->config_path = mods_dir;
    priv->config_is_dir = TRUE;
    return TRUE;
}

static gboolean
gsw_manager_set_from_path(GswManager *self, const gchar *path,
                          gboolean allow_empty)
{
    if (path == NULL || *path == '\0')
        return FALSE;

    if (gsw_manager_looks_like_config(self, path))
        return TRUE;

    if (allow_empty)
        return gsw_manager_set_empty_prefix(self, path);

    return FALSE;
}

static void
gsw_manager_load_config_file(GswManager *self, const gchar *path, GError **error)
{
    GswManagerPrivate *priv = gsw_manager_get_instance_private(self);
    GswConfig *file_config;
    GError *local_error = NULL;
    gchar **sections;
    guint i;

    file_config = gsw_config_new_from_file(path, &local_error);
    if (file_config == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CONFIG,
                    "Cannot load module configuration %s: %s",
                    path, local_error != NULL ? local_error->message : "unknown error");
        g_clear_error(&local_error);
        return;
    }

    sections = gsw_config_get_sections(file_config);
    for (i = 0; sections != NULL && sections[i] != NULL; i++) {
        g_hash_table_replace(priv->config_files, g_strdup(sections[i]),
                             g_strdup(path));
    }
    g_strfreev(sections);

    gsw_config_augment(priv->config, file_config);
    g_object_unref(file_config);
}

static gint
gsw_compare_strings(gconstpointer a, gconstpointer b)
{
    return g_strcmp0(*(const gchar * const *)a, *(const gchar * const *)b);
}

static void
gsw_manager_load_config_dir(GswManager *self, const gchar *path, GError **error)
{
    GDir *dir;
    const gchar *name;
    GPtrArray *files;
    GError *local_error = NULL;
    guint i;

    dir = g_dir_open(path, 0, &local_error);
    if (dir == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot open configuration directory %s: %s",
                    path, local_error->message);
        g_error_free(local_error);
        return;
    }

    files = g_ptr_array_new_with_free_func(g_free);
    while ((name = g_dir_read_name(dir)) != NULL) {
        if (gsw_str_ends_with_ci(name, ".conf"))
            g_ptr_array_add(files, g_build_filename(path, name, NULL));
    }
    g_dir_close(dir);

    g_ptr_array_sort(files, gsw_compare_strings);
    for (i = 0; i < files->len; i++)
        gsw_manager_load_config_file(self, g_ptr_array_index(files, i), error);

    g_ptr_array_unref(files);
}

static void
gsw_manager_load_augment_path(GswManager *self, const gchar *path, GError **error)
{
    GswManagerPrivate *priv = gsw_manager_get_instance_private(self);
    gchar *canonical;
    gchar *candidate;
    GStatBuf st;

    if (path == NULL || *path == '\0')
        return;

    canonical = g_canonicalize_filename(path, NULL);
    if (g_stat(canonical, &st) != 0) {
        g_free(canonical);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        gchar *base = g_path_get_basename(canonical);
        if (g_strcmp0(base, "mods.d") == 0) {
            gsw_manager_load_config_dir(self, canonical, error);
            g_free(base);
            g_free(canonical);
            return;
        }
        g_free(base);

        candidate = g_build_filename(canonical, "mods.d", NULL);
        if (g_file_test(candidate, G_FILE_TEST_IS_DIR))
            gsw_manager_load_config_dir(self, candidate, error);
        g_free(candidate);
    } else if (S_ISREG(st.st_mode)) {
        if (gsw_str_ends_with_ci(canonical, ".conf"))
            gsw_manager_load_config_file(self, canonical, error);
    }

    (void)priv;
    g_free(canonical);
}

static void
gsw_manager_build_infos(GswManager *self)
{
    GswManagerPrivate *priv = gsw_manager_get_instance_private(self);
    gchar **sections;
    guint i;

    sections = gsw_config_get_sections(priv->config);
    for (i = 0; sections != NULL && sections[i] != NULL; i++) {
        const gchar *name = sections[i];
        const gchar *driver = gsw_config_get(priv->config, name, "ModDrv");
        const gchar *config_file;
        GswModuleInfo *info;
        GError *local_error = NULL;

        if (driver == NULL || *driver == '\0')
            continue;

        config_file = g_hash_table_lookup(priv->config_files, name);
        info = gsw_module_info_new_from_config(name, priv->config, config_file,
                                               priv->prefix_path, &local_error);
        if (info == NULL) {
            g_warning("gsword: cannot create module info for [%s]: %s",
                      name, local_error != NULL ? local_error->message : "unknown error");
            g_clear_error(&local_error);
            continue;
        }

        g_hash_table_replace(priv->infos, g_strdup(name), info);
        g_ptr_array_add(priv->info_order, info);
    }
    g_strfreev(sections);
}

gboolean
gsw_manager_load(GswManager *self, GError **error)
{
    GswManagerPrivate *priv;
    guint i;

    g_return_val_if_fail(GSW_IS_MANAGER(self), FALSE);

    priv = gsw_manager_get_instance_private(self);
    gsw_manager_clear(self);

    if (priv->config_path == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "No module configuration path configured");
        return FALSE;
    }

    if (priv->config_is_dir) {
        if (g_file_test(priv->config_path, G_FILE_TEST_IS_DIR))
            gsw_manager_load_config_dir(self, priv->config_path, error);
    } else {
        gsw_manager_load_config_file(self, priv->config_path, error);
    }

    if (error != NULL && *error != NULL) {
        priv->loaded = FALSE;
        return FALSE;
    }

    for (i = 0; i < priv->augment_paths->len; i++)
        gsw_manager_load_augment_path(self, g_ptr_array_index(priv->augment_paths, i), error);

    if (error != NULL && *error != NULL) {
        priv->loaded = FALSE;
        return FALSE;
    }

    gsw_manager_build_infos(self);
    priv->loaded = TRUE;
    return TRUE;
}

gboolean
gsw_manager_reload(GswManager *self, GError **error)
{
    return gsw_manager_load(self, error);
}

static gboolean
gsw_manager_try_sys_config(GswManager *self, const gchar *path, GError **error)
{
    GswConfig *sys;
    GError *local_error = NULL;
    const gchar *data_path;
    gchar **augments;
    guint i;
    gboolean found = FALSE;

    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR))
        return FALSE;

    sys = gsw_config_new_from_file(path, &local_error);
    if (sys == NULL) {
        g_clear_error(&local_error);
        return FALSE;
    }

    data_path = gsw_config_get(sys, "Install", "DataPath");
    if (data_path != NULL && *data_path != '\0') {
        gchar *absolute = gsw_path_make_absolute(data_path, NULL);
        if (gsw_manager_set_from_path(self, absolute, FALSE))
            found = TRUE;
        g_free(absolute);
    }

    augments = gsw_config_get_all(sys, "Install", "AugmentPath");
    for (i = 0; augments != NULL && augments[i] != NULL; i++) {
        gchar *absolute = gsw_path_make_absolute(augments[i], NULL);
        gsw_manager_add_augment_path(self, absolute);
        g_free(absolute);
    }
    g_strfreev(augments);

    g_object_unref(sys);

    if (!found) {
        (void)error;
    }
    return found;
}

static gboolean
gsw_manager_discover(GswManager *self, GError **error)
{
    const gchar *env;
    gchar *cwd;
    gchar *candidate;
    gboolean found = FALSE;

    cwd = g_get_current_dir();

    candidate = g_build_filename(cwd, "mods.conf", NULL);
    found = gsw_manager_set_from_path(self, candidate, FALSE);
    g_free(candidate);

    if (!found) {
        candidate = g_build_filename(cwd, "mods.d", NULL);
        found = gsw_manager_set_from_path(self, candidate, FALSE);
        g_free(candidate);
    }

    env = g_getenv("SWORD_PATH");
    if (!found && env != NULL && *env != '\0') {
        gchar *absolute = gsw_path_make_absolute(env, NULL);
        found = gsw_manager_set_from_path(self, absolute, FALSE);
        g_free(absolute);
    }

    if (!found) {
        candidate = g_build_filename(cwd, "sword.conf", NULL);
        found = gsw_manager_try_sys_config(self, candidate, error);
        g_free(candidate);
    }

    if (!found && g_file_test("/etc/sword.conf", G_FILE_TEST_IS_REGULAR))
        found = gsw_manager_try_sys_config(self, "/etc/sword.conf", error);

    if (!found) {
        const gchar *home = g_get_home_dir();
        if (home != NULL) {
            candidate = g_build_filename(home, ".sword", "sword.conf", NULL);
            found = gsw_manager_try_sys_config(self, candidate, error);
            g_free(candidate);
        }
    }

    if (!found) {
        const gchar *home = g_get_home_dir();
        if (home != NULL) {
            candidate = g_build_filename(home, ".sword", "mods.d", NULL);
            found = gsw_manager_set_from_path(self, candidate, FALSE);
            g_free(candidate);
        }
    }

    if (!found) {
        const gchar *home = g_get_home_dir();
        if (home != NULL) {
            candidate = g_build_filename(home, "sword", "mods.d", NULL);
            found = gsw_manager_set_from_path(self, candidate, FALSE);
            g_free(candidate);
        }
    }

    if (!found)
        found = gsw_manager_set_from_path(self, "/usr/share/sword", FALSE);
    if (!found)
        found = gsw_manager_set_from_path(self, "/usr/local/share/sword", FALSE);

    g_free(cwd);

    if (!found) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "Cannot find a SWORD module library (mods.conf or mods.d)");
        return FALSE;
    }

    return TRUE;
}

GswManager *
gsw_manager_new(GError **error)
{
    GswManager *self = g_object_new(GSW_TYPE_MANAGER, NULL);

    if (!gsw_manager_discover(self, error) || !gsw_manager_load(self, error)) {
        g_object_unref(self);
        return NULL;
    }
    return self;
}

GswManager *
gsw_manager_new_for_path(const gchar *prefix_path, GError **error)
{
    GswManager *self;

    g_return_val_if_fail(prefix_path != NULL, NULL);

    self = g_object_new(GSW_TYPE_MANAGER, NULL);
    if (!gsw_manager_set_from_path(self, prefix_path, TRUE)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "Invalid module library path: %s", prefix_path);
        g_object_unref(self);
        return NULL;
    }

    if (!gsw_manager_load(self, error)) {
        g_object_unref(self);
        return NULL;
    }
    return self;
}

GswManager *
gsw_manager_new_for_config(const gchar *config_path, GError **error)
{
    GswManager *self;

    g_return_val_if_fail(config_path != NULL, NULL);

    self = g_object_new(GSW_TYPE_MANAGER, NULL);
    if (!gsw_manager_set_from_path(self, config_path, FALSE)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "Invalid module configuration path: %s", config_path);
        g_object_unref(self);
        return NULL;
    }

    if (!gsw_manager_load(self, error)) {
        g_object_unref(self);
        return NULL;
    }
    return self;
}

void
gsw_manager_add_augment_path(GswManager *self, const gchar *path)
{
    GswManagerPrivate *priv;

    g_return_if_fail(GSW_IS_MANAGER(self));

    if (path == NULL || *path == '\0')
        return;

    priv = gsw_manager_get_instance_private(self);
    g_ptr_array_add(priv->augment_paths, g_strdup(path));
}

const gchar *
gsw_manager_get_prefix_path(GswManager *self)
{
    GswManagerPrivate *priv;

    g_return_val_if_fail(GSW_IS_MANAGER(self), NULL);
    priv = gsw_manager_get_instance_private(self);
    return priv->prefix_path;
}

const gchar *
gsw_manager_get_config_path(GswManager *self)
{
    GswManagerPrivate *priv;

    g_return_val_if_fail(GSW_IS_MANAGER(self), NULL);
    priv = gsw_manager_get_instance_private(self);
    return priv->config_path;
}

GPtrArray *
gsw_manager_get_module_infos(GswManager *self)
{
    GswManagerPrivate *priv;
    GPtrArray *result;
    guint i;

    g_return_val_if_fail(GSW_IS_MANAGER(self), NULL);

    priv = gsw_manager_get_instance_private(self);
    result = g_ptr_array_new_with_free_func(g_object_unref);
    for (i = 0; i < priv->info_order->len; i++)
        g_ptr_array_add(result, g_object_ref(g_ptr_array_index(priv->info_order, i)));
    return result;
}

GswModuleInfo *
gsw_manager_lookup_module_info(GswManager *self, const gchar *name)
{
    GswManagerPrivate *priv;

    g_return_val_if_fail(GSW_IS_MANAGER(self), NULL);
    g_return_val_if_fail(name != NULL, NULL);

    priv = gsw_manager_get_instance_private(self);
    return g_hash_table_lookup(priv->infos, name);
}

gboolean
gsw_manager_has_module(GswManager *self, const gchar *name)
{
    return gsw_manager_lookup_module_info(self, name) != NULL;
}

GswModule *
gsw_manager_get_module(GswManager *self, const gchar *name)
{
    GswManagerPrivate *priv;
    GswModule *module;
    GswModuleInfo *info;

    g_return_val_if_fail(GSW_IS_MANAGER(self), NULL);
    g_return_val_if_fail(name != NULL, NULL);

    priv = gsw_manager_get_instance_private(self);
    module = g_hash_table_lookup(priv->modules, name);
    if (module != NULL)
        return module;

    info = g_hash_table_lookup(priv->infos, name);
    if (info == NULL)
        return NULL;

    module = gsw_module_new_from_info(info);
    if (module == NULL)
        return NULL;

    g_hash_table_insert(priv->modules, g_strdup(name), module);
    return module;
}

GswModule *
gsw_manager_create_module(GswManager *self, const gchar *name)
{
    GswModuleInfo *info;

    g_return_val_if_fail(GSW_IS_MANAGER(self), NULL);
    g_return_val_if_fail(name != NULL, NULL);

    info = gsw_manager_lookup_module_info(self, name);
    if (info == NULL)
        return NULL;

    return gsw_module_new_from_info(info);
}

GswConfig *
gsw_manager_get_config(GswManager *self)
{
    GswManagerPrivate *priv;

    g_return_val_if_fail(GSW_IS_MANAGER(self), NULL);
    priv = gsw_manager_get_instance_private(self);
    return priv->config;
}

const gchar *
gsw_manager_get_config_file_for_module(GswManager *self, const gchar *name)
{
    GswManagerPrivate *priv;

    g_return_val_if_fail(GSW_IS_MANAGER(self), NULL);
    g_return_val_if_fail(name != NULL, NULL);

    priv = gsw_manager_get_instance_private(self);
    return g_hash_table_lookup(priv->config_files, name);
}
