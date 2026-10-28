#include <gsword/gsword.h>
#include "gsw-key-internal.h"

static void
check_key(const gchar *text, gint testament, gint book, gint chapter,
          gint verse, glong index)
{
    GswVerseKey *key = gsw_verse_key_new(text, "KJV");

    g_assert_nonnull(key);
    g_assert_cmpint(gsw_verse_key_get_testament(key), ==, testament);
    g_assert_cmpint(gsw_verse_key_get_book(key), ==, book);
    g_assert_cmpint(gsw_verse_key_get_chapter(key), ==, chapter);
    g_assert_cmpint(gsw_verse_key_get_verse(key), ==, verse);
    if (index >= 0)
        g_assert_cmpint(gsw_verse_key_get_index(key), ==, index);
    gsw_key_unref(key);
}

static void
test_parse(void)
{
    GswVerseKey *key;
    GswVersification *v11n;
    gint book, chapter, verse;

    check_key("Gen 1:1", 1, 1, 1, 1, 4);
    check_key("Genesis 1:1", 1, 1, 1, 1, 4);
    check_key("Matt 1:1", 2, 1, 1, 1, 24118);
    check_key("1Sam1.20", 1, 9, 1, 20, -1);
    check_key("Ps 119:176", 1, 19, 119, 176, -1);
    check_key("Jn.3.16", 2, 4, 3, 16, -1);

    key = gsw_verse_key_new("Matt 1:1", "KJV");
    g_assert_cmpint(gsw_verse_key_get_testament_index(key), ==, 4);
    g_assert_cmpstr(gsw_verse_key_get_book_name(key), ==, "Matthew");
    g_assert_cmpint(gsw_verse_key_get_chapter_max(key), ==, 28);
    g_assert_cmpint(gsw_verse_key_get_verse_max(key), ==, 25);
    gsw_key_unref(key);

    key = gsw_verse_key_new("Gen 1:200", "KJV");
    g_assert_false(gsw_verse_key_is_valid(key));
    g_assert_cmpint(gsw_verse_key_get_verse(key), ==, 31);
    g_assert_cmpstr(gsw_key_get_text(key), ==, "Genesis 1:31");
    gsw_key_unref(key);

    key = gsw_verse_key_new("[ Module Heading ]", "KJV");
    g_assert_cmpint(gsw_verse_key_get_testament(key), ==, 0);
    g_assert_cmpint(gsw_verse_key_get_index(key), ==, 0);
    gsw_key_unref(key);

    key = gsw_verse_key_new("[ Testament 2 Heading ]", "KJV");
    g_assert_cmpint(gsw_verse_key_get_testament(key), ==, 2);
    g_assert_cmpint(gsw_verse_key_get_book(key), ==, 0);
    g_assert_cmpint(gsw_verse_key_get_testament_index(key), ==, 1);
    gsw_key_unref(key);

    v11n = gsw_versification_mgr_get_system(gsw_versification_mgr_get_default(),
                                            "KJV");
    g_assert_nonnull(v11n);
    g_assert_cmpint(gsw_versification_get_nt_start_offset(v11n), ==, 24114);
    g_assert_cmpint(gsw_versification_get_offset(v11n, 0, 1, 1), ==, 4);
    g_assert_cmpint(gsw_versification_get_offset(v11n, 39, 1, 1), ==, 24118);

    g_assert_true(gsw_versification_get_verse(v11n, 24118, &book, &chapter, &verse));
    g_assert_cmpint(book, ==, 39);
    g_assert_cmpint(chapter, ==, 1);
    g_assert_cmpint(verse, ==, 1);

    {
        gint ot = 0, nt = 0;
        gsw_versification_get_bmax(v11n, &ot, &nt);
        g_assert_cmpint(ot, ==, 39);
        g_assert_cmpint(nt, ==, 27);
    }
}

static void
test_iteration(void)
{
    GswVerseKey *key = gsw_verse_key_new("Gen 1:1", "KJV");

    g_assert_true(gsw_key_next(key, 1));
    g_assert_cmpint(gsw_verse_key_get_verse(key), ==, 2);
    g_assert_true(gsw_key_prev(key, 1));
    g_assert_cmpint(gsw_verse_key_get_verse(key), ==, 1);

    gsw_verse_key_set_index(key, 24118);
    g_assert_cmpint(gsw_verse_key_get_testament(key), ==, 2);
    g_assert_cmpstr(gsw_verse_key_get_book_name(key), ==, "Matthew");
    gsw_key_unref(key);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/verse-key/parse", test_parse);
    g_test_add_func("/verse-key/iteration", test_iteration);
    return g_test_run();
}
