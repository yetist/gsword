#include <gsword/gsword.h>
#include "gsw-utils.h"
#include <glib/gstdio.h>

static void
test_source_config(void)
{
    GError *error = NULL;
    GswInstallSource *source;
    gchar *value;

    source = gsw_install_source_new_from_config(
        GSW_SOURCE_HTTPS,
        "CrossWire|example.org|/pub/sword/|user|pass|20260101000000",
        &error);
    g_assert_no_error(error);
    g_assert_nonnull(source);
    g_assert_cmpint(gsw_install_source_get_source_type(source), ==,
                    GSW_SOURCE_HTTPS);
    g_assert_cmpstr(gsw_install_source_get_caption(source), ==, "CrossWire");
    g_assert_cmpstr(gsw_install_source_get_host(source), ==, "example.org");
    g_assert_cmpstr(gsw_install_source_get_directory(source), ==, "/pub/sword");
    g_assert_cmpstr(gsw_install_source_get_user(source), ==, "user");
    g_assert_cmpstr(gsw_install_source_get_password(source), ==, "pass");
    g_assert_cmpstr(gsw_install_source_get_uid(source), ==, "20260101000000");

    value = gsw_install_source_to_config(source);
    g_assert_cmpstr(value, ==,
                    "CrossWire|example.org|/pub/sword|user|pass|20260101000000");
    g_free(value);
    g_object_unref(source);

    source = gsw_install_source_new_from_config(
        GSW_SOURCE_DIR, "Local|/tmp/sword-library||||local-dir", &error);
    g_assert_no_error(error);
    g_assert_nonnull(source);
    g_assert_cmpint(gsw_install_source_get_source_type(source), ==,
                    GSW_SOURCE_DIR);
    g_assert_cmpstr(gsw_install_source_get_host(source), ==,
                    "/tmp/sword-library");
    value = gsw_install_source_to_config(source);
    g_assert_cmpstr(value, ==, "Local|/tmp/sword-library||||local-dir");
    g_free(value);
    g_object_unref(source);

    source = gsw_install_source_new_from_config(GSW_SOURCE_FTP,
                                                "bad", &error);
    g_assert_null(source);
    g_assert_error(error, GSW_ERROR, GSW_ERROR_CONFIG);
    g_clear_error(&error);

    source = gsw_install_source_new_from_config(GSW_SOURCE_FTP,
                                                "X|host|/dir|||../evil",
                                                &error);
    g_assert_null(source);
    g_assert_error(error, GSW_ERROR, GSW_ERROR_CONFIG);
    g_clear_error(&error);
}

static void
test_manager_config(void)
{
    GError *error = NULL;
    gchar *private_path = g_dir_make_tmp("gsword-install-XXXXXX", &error);
    gchar *cache_path = g_dir_make_tmp("gsword-install-cache-XXXXXX", &error);
    gchar *conf_path;
    GswInstallManager *manager;
    GswInstallSource *source;
    GPtrArray *sources;

    g_assert_no_error(error);
    g_assert_nonnull(private_path);
    g_assert_nonnull(cache_path);

    conf_path = g_build_filename(private_path, "InstallMgr.conf", NULL);
    g_assert_true(g_file_set_contents(conf_path,
        "[General]\n"
        "PassiveFTP=true\n"
        "TimeoutMillis=12345\n"
        "[Sources]\n"
        "FTPSource=CrossWire|ftp.example.org|/pub/sword|||uid1\n"
        "HTTPSource=Secure|secure.example.org|/sword|||uid2\n",
        -1, NULL));

    manager = gsw_install_manager_new(private_path, cache_path, &error);
    g_assert_no_error(error);
    g_assert_nonnull(manager);
    g_assert_cmpstr(gsw_install_manager_get_config_dir(manager), ==,
                    private_path);
    g_assert_cmpstr(gsw_install_manager_get_cache_dir(manager), ==,
                    cache_path);
    g_assert_cmpuint(gsw_install_manager_get_timeout(manager), ==, 12345);
    g_assert_true(gsw_install_manager_get_passive_ftp(manager));

    sources = gsw_install_manager_get_sources(manager);
    g_assert_cmpuint(sources->len, ==, 2);
    g_ptr_array_unref(sources);

    source = gsw_install_manager_lookup_source(manager, "CrossWire");
    g_assert_nonnull(source);
    g_assert_cmpint(gsw_install_source_get_source_type(source), ==,
                    GSW_SOURCE_FTP);
    g_assert_cmpstr(gsw_install_source_get_uid(source), ==, "uid1");

    source = gsw_install_manager_lookup_source(manager, "Secure");
    g_assert_nonnull(source);
    g_assert_cmpint(gsw_install_source_get_source_type(source), ==,
                    GSW_SOURCE_HTTP);

    source = gsw_install_source_new(GSW_SOURCE_FTP, "Added", "new.example.org",
                                    "/sword", "", "", "uid3");
    g_assert_nonnull(source);
    g_assert_true(gsw_install_manager_add_source(manager, source));
    g_object_unref(source);

    source = gsw_install_source_new(GSW_SOURCE_DIR, "Local",
                                    "/tmp/sword-library", "", "", "", "uid4");
    g_assert_nonnull(source);
    g_assert_true(gsw_install_manager_add_source(manager, source));
    g_object_unref(source);
    g_assert_true(gsw_install_manager_save_config(manager, &error));
    g_assert_no_error(error);
    g_object_unref(manager);

    manager = gsw_install_manager_new(private_path, cache_path, &error);
    g_assert_no_error(error);
    g_assert_nonnull(manager);
    g_assert_nonnull(gsw_install_manager_lookup_source(manager, "Added"));
    source = gsw_install_manager_lookup_source(manager, "Local");
    g_assert_nonnull(source);
    g_assert_cmpint(gsw_install_source_get_source_type(source), ==,
                    GSW_SOURCE_DIR);
    sources = gsw_install_manager_get_sources(manager);
    g_assert_cmpuint(sources->len, ==, 4);
    g_ptr_array_unref(sources);
    g_object_unref(manager);

    gsw_dir_remove_recursive(cache_path, NULL);
    gsw_dir_remove_recursive(private_path, NULL);
    g_free(cache_path);
    g_free(conf_path);
    g_free(private_path);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/install-source/parse", test_source_config);
    g_test_add_func("/install-source/manager", test_manager_config);
    return g_test_run();
}
