/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <gsword/gsword.h>

static void
test_compare(void)
{
    g_assert_cmpint(gsw_version_compare("1.0", "1.0"), ==, 0);
    g_assert_cmpint(gsw_version_compare("1.0", "1.0.0"), <, 0);
    g_assert_cmpint(gsw_version_compare("2.9", "2.8.1"), >, 0);
    g_assert_cmpint(gsw_version_compare("1.2.3.4", "1.2.3.4"), ==, 0);
    g_assert_cmpint(gsw_version_compare("1.2.3.5", "1.2.3.4"), >, 0);
    g_assert_cmpint(gsw_version_compare("", "0"), <, 0);
    g_assert_cmpint(gsw_version_compare("1", "1.0"), <, 0);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/version/compare", test_compare);
    return g_test_run();
}
