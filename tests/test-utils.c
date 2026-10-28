#include <gsword/gsw-error.h>
#include "gsw-utils.h"
#include <glib/gstdio.h>

static void
test_safe_relative(void)
{
    g_assert_true(gsw_path_is_safe_relative("modules/texts/ztext/kjv"));
    g_assert_true(gsw_path_is_safe_relative("./modules/kjv"));
    g_assert_true(gsw_path_is_safe_relative("a/./b"));
    g_assert_false(gsw_path_is_safe_relative(""));
    g_assert_false(gsw_path_is_safe_relative(NULL));
    g_assert_false(gsw_path_is_safe_relative("/etc/passwd"));
    g_assert_false(gsw_path_is_safe_relative("../etc/passwd"));
    g_assert_false(gsw_path_is_safe_relative("a/../../b"));
    g_assert_false(gsw_path_is_safe_relative("C:\\Windows\\system.ini"));
}

static void
test_join_under(void)
{
    GError *error = NULL;
    gchar *path;

    path = gsw_path_join_under("/tmp/gsword-root", "modules/kjv", &error);
    g_assert_no_error(error);
    g_assert_nonnull(path);
    g_assert_cmpstr(path, ==, "/tmp/gsword-root/modules/kjv");
    g_free(path);

    path = gsw_path_join_under("/tmp/gsword-root", "../other", &error);
    g_assert_null(path);
    g_assert_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA);
    g_clear_error(&error);

    path = gsw_path_join_under("/tmp/gsword-root", "/etc/passwd", &error);
    g_assert_null(path);
    g_assert_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA);
    g_clear_error(&error);

    g_assert_true(gsw_path_is_within("/tmp/gsword-root",
                                     "/tmp/gsword-root/modules/kjv"));
    g_assert_false(gsw_path_is_within("/tmp/gsword-root",
                                      "/tmp/gsword-root2/modules"));
}

static void
test_misc(void)
{
    g_assert_cmpstr(gsw_driver_to_string(GSW_DRIVER_Z_TEXT), ==, "zText");
    g_assert_cmpint(gsw_driver_from_string("zText4"), ==, GSW_DRIVER_Z_TEXT4);
    g_assert_cmpint(gsw_module_type_from_string("Commentaries"), ==,
                    GSW_MODULE_TYPE_COMMENTARIES);
    g_assert_cmpint(gsw_source_type_from_string("HTTPS"), ==,
                    GSW_SOURCE_HTTPS);
    g_assert_cmpstr(gsw_source_type_to_scheme(GSW_SOURCE_HTTPS), ==, "https");
    g_assert_true(gsw_str_is_true("TRUE"));
    g_assert_false(gsw_str_is_true("false"));
    g_assert_cmpuint(gsw_str_parse_u64("3199007", 0), ==, 3199007);
    g_assert_cmpuint(gsw_str_parse_u64("bad", 7), ==, 7);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/utils/safe-relative", test_safe_relative);
    g_test_add_func("/utils/join-under", test_join_under);
    g_test_add_func("/utils/misc", test_misc);
    return g_test_run();
}
