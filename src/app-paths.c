/*
 * app-paths.c: XDG paths owned by the gsword application
 */

#include "app-paths.h"

#include <glib/gstdio.h>

gchar *
gsword_app_data_dir(void)
{
    return g_build_filename(g_get_user_data_dir(), "gsword", NULL);
}

gchar *
gsword_app_library_dir(void)
{
    gchar *data_dir = gsword_app_data_dir();
    gchar *path = g_build_filename(data_dir, "library", NULL);

    g_free(data_dir);
    return path;
}

gchar *
gsword_app_config_dir(void)
{
    return g_build_filename(g_get_user_config_dir(), "gsword", NULL);
}

gchar *
gsword_app_cache_dir(void)
{
    return g_build_filename(g_get_user_cache_dir(), "gsword", NULL);
}

gchar *
gsword_app_default_library_path(void)
{
    const gchar *env = g_getenv("GSWORD_LIBRARY");

    if (env != NULL && *env != '\0')
        return g_strdup(env);
    return gsword_app_library_dir();
}

gchar *
gsword_app_exports_dir(void)
{
    gchar *data_dir = gsword_app_data_dir();
    gchar *path = g_build_filename(data_dir, "exports", NULL);

    g_free(data_dir);
    return path;
}

gchar *
gsword_app_search_index_path(const gchar *module_name)
{
    gchar *cache_dir = gsword_app_cache_dir();
    gchar *search_dir = g_build_filename(cache_dir, "search", NULL);
    gchar *escaped = g_uri_escape_string(
        module_name != NULL ? module_name : "module", "._-", TRUE);
    gchar *filename = g_strdup_printf("%s.idx", escaped);
    gchar *path = g_build_filename(search_dir, filename, NULL);

    g_free(filename);
    g_free(escaped);
    g_free(search_dir);
    g_free(cache_dir);
    return path;
}

GPtrArray *
gsword_app_system_library_dirs(void)
{
    const gchar * const *data_dirs = g_get_system_data_dirs();
    GPtrArray *paths = g_ptr_array_new_with_free_func(g_free);
    guint i;

    for (i = 0; data_dirs != NULL && data_dirs[i] != NULL; i++) {
        gchar *path = g_build_filename(data_dirs[i], "sword", NULL);

        if (g_file_test(path, G_FILE_TEST_IS_DIR))
            g_ptr_array_add(paths, path);
        else
            g_free(path);
    }
    return paths;
}
