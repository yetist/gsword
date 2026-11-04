#include "app-paths.h"
#include "gsw-utils.h"

#include <glib.h>
#include <glib/gstdio.h>

static void
test_xdg_paths(void)
{
    gchar *data = gsword_app_data_dir();
    gchar *library = gsword_app_library_dir();
    gchar *config = gsword_app_config_dir();
    gchar *cache = gsword_app_cache_dir();
    gchar *exports = gsword_app_exports_dir();
    gchar *default_library = gsword_app_default_library_path();
    gchar *index = gsword_app_search_index_path("KJV");
    gchar *expected_data;
    gchar *expected_library;
    gchar *expected_config;
    gchar *expected_cache;
    gchar *expected_exports;
    gchar *expected_index;

    expected_data = g_build_filename(g_getenv("XDG_DATA_HOME"), "gsword", NULL);
    expected_library = g_build_filename(expected_data, "library", NULL);
    expected_config = g_build_filename(g_getenv("XDG_CONFIG_HOME"), "gsword",
                                       NULL);
    expected_cache = g_build_filename(g_getenv("XDG_CACHE_HOME"), "gsword",
                                      NULL);
    expected_exports = g_build_filename(expected_data, "exports", NULL);
    expected_index = g_build_filename(expected_cache, "search", "KJV.idx",
                                      NULL);

    g_assert_cmpstr(data, ==, expected_data);
    g_assert_cmpstr(library, ==, expected_library);
    g_assert_cmpstr(config, ==, expected_config);
    g_assert_cmpstr(cache, ==, expected_cache);
    g_assert_cmpstr(exports, ==, expected_exports);
    g_assert_cmpstr(default_library, ==, expected_library);
    g_assert_cmpstr(index, ==, expected_index);

    g_free(expected_index);
    g_free(expected_exports);
    g_free(expected_cache);
    g_free(expected_config);
    g_free(expected_library);
    g_free(expected_data);
    g_free(index);
    g_free(default_library);
    g_free(exports);
    g_free(cache);
    g_free(config);
    g_free(library);
    g_free(data);
}

static void
test_library_override(void)
{
    gchar *library;

    g_setenv("GSWORD_LIBRARY", "/tmp/gsword-explicit-library", TRUE);
    library = gsword_app_default_library_path();
    g_assert_cmpstr(library, ==, "/tmp/gsword-explicit-library");
    g_free(library);
    g_unsetenv("GSWORD_LIBRARY");
}

static void
test_system_libraries(void)
{
    GError *error = NULL;
    gchar *root = g_dir_make_tmp("gsword-system-data-XXXXXX", &error);
    gchar *sword = g_build_filename(root, "sword", NULL);
    gchar *data_dirs = g_strconcat(root, G_SEARCHPATH_SEPARATOR_S, "/usr/share",
                                   NULL);
    GPtrArray *dirs;
    gboolean found = FALSE;
    guint i;

    g_assert_no_error(error);
    g_assert_nonnull(root);
    g_mkdir_with_parents(sword, 0755);
    g_setenv("XDG_DATA_DIRS", data_dirs, TRUE);
    dirs = gsword_app_system_library_dirs();
    for (i = 0; i < dirs->len; i++) {
        if (g_strcmp0(g_ptr_array_index(dirs, i), sword) == 0)
            found = TRUE;
    }
    g_assert_true(found);
    g_ptr_array_unref(dirs);
    gsw_dir_remove_recursive(root, NULL);
    g_free(data_dirs);
    g_free(sword);
    g_free(root);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    gchar *root;
    gchar *data_home;
    gchar *config_home;
    gchar *cache_home;

    root = g_dir_make_tmp("gsword-app-paths-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(root);
    data_home = g_build_filename(root, "data", NULL);
    config_home = g_build_filename(root, "config", NULL);
    cache_home = g_build_filename(root, "cache", NULL);
    g_mkdir_with_parents(data_home, 0755);
    g_mkdir_with_parents(config_home, 0755);
    g_mkdir_with_parents(cache_home, 0755);
    g_setenv("XDG_DATA_HOME", data_home, TRUE);
    g_setenv("XDG_CONFIG_HOME", config_home, TRUE);
    g_setenv("XDG_CACHE_HOME", cache_home, TRUE);
    g_unsetenv("GSWORD_LIBRARY");

    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/app-paths/xdg", test_xdg_paths);
    g_test_add_func("/app-paths/library-override", test_library_override);
    g_test_add_func("/app-paths/system-libraries", test_system_libraries);
    {
        int status = g_test_run();

        gsw_dir_remove_recursive(root, NULL);
        g_free(cache_home);
        g_free(config_home);
        g_free(data_home);
        g_free(root);
        return status;
    }
}
