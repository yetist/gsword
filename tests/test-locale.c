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
    g_assert_cmpstr(gsw_locale_mgr_get_default_locale(mgr), ==, "en");
    g_assert_cmpstr(gsw_locale_mgr_translate(mgr, "prefAbbr_Gen", NULL), ==,
                    "Gen");
    g_assert_cmpstr(gsw_locale_mgr_translate_abbreviation(mgr, "Genesis",
                                                        NULL), ==, "Gen");
    {
        GPtrArray *candidates = gsw_locale_mgr_get_book_candidates(mgr, "1",
                                                                   NULL);
        gboolean found = FALSE;
        guint i;

        for (i = 0; i < candidates->len; i++) {
            if (g_strcmp0(g_ptr_array_index(candidates, i), "1Cor") == 0)
                found = TRUE;
        }
        g_assert_true(found);
        g_ptr_array_unref(candidates);
    }
    path = g_build_filename(dir, "zh_Test.conf", NULL);
    g_assert_true(g_file_set_contents(path,
        "[Meta]\n"
        "Name=zh_Test\n"
        "Encoding=UTF-8\n"
        "[Text]\n"
        "Genesis=创世记\n"
        "John=约翰福音\n"
        "[Book Abbrevs]\n"
        "創=Gen\n"
        "出=Exod\n"
        "XGen=Gen\n"
        "[Pref Abbrevs]\n"
        "Gen=創\n"
        "Exod=出\n", -1, NULL));

    g_assert_true(gsw_locale_mgr_load_file(mgr, path, &error));
    g_assert_no_error(error);
    {
        GPtrArray *locales = gsw_locale_mgr_get_locales(mgr);
        gboolean has_en = FALSE;
        gboolean has_test = FALSE;
        guint i;

        for (i = 0; i < locales->len; i++) {
            const gchar *name = g_ptr_array_index(locales, i);

            if (g_strcmp0(name, "en") == 0)
                has_en = TRUE;
            if (g_strcmp0(name, "zh_Test") == 0)
                has_test = TRUE;
        }
        g_assert_true(has_en);
        g_assert_true(has_test);
        g_ptr_array_unref(locales);
    }
    g_assert_cmpstr(gsw_locale_mgr_translate_abbreviation(mgr, "Genesis",
                                                        NULL), ==, "Gen");

    gsw_locale_mgr_set_default_locale(mgr, "zh_Test");
    g_assert_cmpstr(gsw_locale_mgr_get_default_locale(mgr), ==, "zh_Test");
    g_assert_cmpstr(gsw_locale_mgr_translate(mgr, "Genesis", NULL), ==,
                    "创世记");
    g_assert_cmpstr(gsw_locale_mgr_reverse(mgr, "创世记", NULL), ==,
                    "Genesis");
    g_assert_cmpstr(gsw_locale_mgr_translate_abbreviation(mgr, "創", NULL),
                    ==, "Gen");
    g_assert_cmpstr(gsw_locale_mgr_translate_abbreviation(mgr, "xgen", NULL),
                    ==, "Gen");
    g_assert_cmpstr(gsw_locale_mgr_translate_preferred_abbreviation(
                        mgr, "Gen", NULL), ==, "創");
    g_assert_cmpstr(gsw_locale_mgr_translate(mgr, "prefAbbr_Gen", NULL), ==,
                    "創");
    g_assert_null(gsw_locale_mgr_translate_abbreviation(mgr, "unknown",
                                                        NULL));

    key = gsw_verse_key_new("Genesis 1:1", "KJV");
    g_assert_cmpstr(gsw_key_get_text((GswKey *)key), ==, "创世记 1:1");
    g_assert_cmpstr(gsw_verse_key_get_book_name(key), ==, "创世记");
    {
        gchar *short_text = gsw_verse_key_get_short_text(key);

        g_assert_cmpstr(short_text, ==, "創 1:1");
        g_free(short_text);
    }
    gsw_key_unref((GswKey *)key);

    key = gsw_verse_key_new("出 2:3", "KJV");
    g_assert_cmpint(gsw_verse_key_get_book(key), ==, 2);
    g_assert_cmpint(gsw_verse_key_get_chapter(key), ==, 2);
    g_assert_cmpint(gsw_verse_key_get_verse(key), ==, 3);
    gsw_key_unref((GswKey *)key);

    key = gsw_verse_key_new("xgen 1:2", "KJV");
    g_assert_cmpint(gsw_verse_key_get_book(key), ==, 1);
    g_assert_cmpint(gsw_verse_key_get_chapter(key), ==, 1);
    g_assert_cmpint(gsw_verse_key_get_verse(key), ==, 2);
    gsw_key_unref((GswKey *)key);

    key = gsw_verse_key_new("约翰福音 3:16", "KJV");
    g_assert_cmpint(gsw_verse_key_get_testament(key), ==, 2);
    g_assert_cmpint(gsw_verse_key_get_book(key), ==, 4);
    g_assert_cmpint(gsw_verse_key_get_verse(key), ==, 16);
    gsw_key_unref((GswKey *)key);

    gsw_locale_mgr_set_default_locale(mgr, NULL);
    g_assert_cmpstr(gsw_locale_mgr_get_default_locale(mgr), ==, "en");
    g_remove(path);
    g_rmdir(dir);
    g_free(path);
    g_free(dir);
}

static gboolean
locales_contain(GPtrArray *locales, const gchar *name)
{
    guint i;

    for (i = 0; i < locales->len; i++) {
        if (g_strcmp0(g_ptr_array_index(locales, i), name) == 0)
            return TRUE;
    }
    return FALSE;
}

static void
test_locale_paths(void)
{
    GError *error = NULL;
    gchar *root = g_dir_make_tmp("gsword-locale-paths-XXXXXX", &error);
    gchar *path_dir = g_build_filename(root, "gsword-locales", NULL);
    gchar *sword_root = g_build_filename(root, "sword", NULL);
    gchar *sword_dir = g_build_filename(sword_root, "locales.d", NULL);
    gchar *path_conf = g_build_filename(path_dir, "path_Test.conf", NULL);
    gchar *sword_conf = g_build_filename(sword_dir, "sword_Test.conf", NULL);
    GswLocaleMgr *mgr;
    GPtrArray *locales;

    g_assert_no_error(error);
    g_mkdir_with_parents(path_dir, 0755);
    g_mkdir_with_parents(sword_dir, 0755);
    g_assert_true(g_file_set_contents(path_conf,
        "[Meta]\n"
        "Name=path_Test\n"
        "Encoding=UTF-8\n"
        "[Book Abbrevs]\n"
        "XPath=Gen\n", -1, NULL));
    g_assert_true(g_file_set_contents(sword_conf,
        "[Meta]\n"
        "Name=sword_Test\n"
        "Encoding=UTF-8\n"
        "[Book Abbrevs]\n"
        "XSword=Gen\n", -1, NULL));

    g_setenv("GSW_LOCALE_PATH", path_dir, TRUE);
    g_setenv("SWORD_PATH", sword_root, TRUE);
    mgr = g_object_new(GSW_TYPE_LOCALE_MGR, NULL);
    locales = gsw_locale_mgr_get_locales(mgr);
    g_assert_true(locales_contain(locales, "en"));
    g_assert_true(locales_contain(locales, "path_Test"));
    g_assert_false(locales_contain(locales, "sword_Test"));
    g_ptr_array_unref(locales);
    g_assert_cmpstr(gsw_locale_mgr_translate_abbreviation(
                        mgr, "xpath", "path_Test"), ==, "Gen");
    g_object_unref(mgr);

    g_unsetenv("GSW_LOCALE_PATH");
    g_unsetenv("SWORD_PATH");
    g_remove(path_conf);
    g_remove(sword_conf);
    g_rmdir(path_dir);
    g_rmdir(sword_dir);
    g_rmdir(sword_root);
    g_rmdir(root);
    g_free(path_conf);
    g_free(sword_conf);
    g_free(path_dir);
    g_free(sword_dir);
    g_free(sword_root);
    g_free(root);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/locale/translate", test_locale);
    g_test_add_func("/locale/paths", test_locale_paths);
    return g_test_run();
}
