/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "gsw-encoding.h"
#include <glib.h>
#include <string.h>

static void
test_scsu(void)
{
    const gchar input[] = { 0x0f, 0x79, 0x5e, 0x72, 0x31, 0x4e, 0x16, 0x4e, (gchar)0xba };
    const gchar *expected = "神爱世人";
    gchar *out = NULL;
    gsize len = 0;
    GError *error = NULL;

    g_assert_true(gsw_scsu_to_utf8(input, sizeof(input), &out, &len, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(len, ==, strlen(expected));
    g_assert_cmpstr(out, ==, expected);
    g_free(out);
}

static void
test_ascii(void)
{
    gchar *out = NULL;
    gsize len = 0;
    GError *error = NULL;

    g_assert_true(gsw_scsu_to_utf8("hello", 5, &out, &len, &error));
    g_assert_no_error(error);
    g_assert_cmpstr(out, ==, "hello");
    g_assert_cmpuint(len, ==, 5);
    g_free(out);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/encoding/scsu", test_scsu);
    g_test_add_func("/encoding/ascii", test_ascii);
    return g_test_run();
}
