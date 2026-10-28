#include <gsword/gsword.h>
#include "gsw-utils.h"
#include <glib/gstdio.h>

static void
test_locale(void)
{
    GError *error = NULL;
    gchar *dir = g_dir_make_tmp("gsword-locale-XXXXXX", &error);
    gchar *path;
    GswLocaleMgr *mgr = gsw_locale_mgr_get_default();
    GswVerseKey *key;

    g_assert_no_error(error);
    path = g_build_filename(dir, "zh_Test.conf", NULL);
    g_assert_true(g_file_set_contents(path,
        "[Meta]\n"
        "Name=zh_Test\n"
        "Encoding=UTF-8\n"
        "[Text]\n"
        "Genesis=创世记\n"
        "John=约翰福音\n", -1, NULL));

    g_assert_true(gsw_locale_mgr_load_file(mgr, path, &error));
    g_assert_no_error(error);

    gsw_locale_mgr_set_default_locale(mgr, "zh_Test");
    g_assert_cmpstr(gsw_locale_mgr_get_default_locale(mgr), ==, "zh_Test");
    g_assert_cmpstr(gsw_locale_mgr_translate(mgr, "Genesis", NULL), ==,
                    "创世记");
    g_assert_cmpstr(gsw_locale_mgr_reverse(mgr, "创世记", NULL), ==,
                    "Genesis");

    key = gsw_verse_key_new("Genesis 1:1", "KJV");
    g_assert_cmpstr(gsw_key_get_text((GswKey *)key), ==, "创世记 1:1");
    g_assert_cmpstr(gsw_verse_key_get_book_name(key), ==, "创世记");
    gsw_key_unref((GswKey *)key);

    key = gsw_verse_key_new("约翰福音 3:16", "KJV");
    g_assert_cmpint(gsw_verse_key_get_testament(key), ==, 2);
    g_assert_cmpint(gsw_verse_key_get_book(key), ==, 4);
    g_assert_cmpint(gsw_verse_key_get_verse(key), ==, 16);
    gsw_key_unref((GswKey *)key);

    gsw_locale_mgr_set_default_locale(mgr, NULL);
    g_remove(path);
    g_rmdir(dir);
    g_free(path);
    g_free(dir);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/locale/translate", test_locale);
    return g_test_run();
}
