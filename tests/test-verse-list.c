/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include <gsword/gsword.h>

static void
assert_reference(GswVerseKey *key, gint book, gint chapter, gint verse)
{
    g_assert_nonnull(key);
    g_assert_cmpint(gsw_verse_key_get_book(key), ==, book);
    g_assert_cmpint(gsw_verse_key_get_chapter(key), ==, chapter);
    g_assert_cmpint(gsw_verse_key_get_verse(key), ==, verse);
}

static void
test_parse_list(void)
{
    GError *error = NULL;
    GswVerseKey *context = gsw_verse_key_new("Gen 1:1", "KJV");
    GswVerseList *list = gsw_verse_list_parse(
        "John 3:16-18; Rom 8:1", context, &error);
    GswVerseKey *range;
    GswVerseKey *lower;
    GswVerseKey *upper;
    GPtrArray *expanded;

    g_assert_no_error(error);
    g_assert_nonnull(list);
    g_assert_cmpuint(gsw_verse_list_get_length(list), ==, 2);

    range = gsw_verse_list_get_range(list, 0);
    lower = gsw_verse_key_get_lower_bound(range);
    upper = gsw_verse_key_get_upper_bound(range);
    assert_reference(lower, 4, 3, 16);
    assert_reference(upper, 4, 3, 18);
    gsw_key_unref(lower);
    gsw_key_unref(upper);

    range = gsw_verse_list_get_range(list, 1);
    lower = gsw_verse_key_get_lower_bound(range);
    upper = gsw_verse_key_get_upper_bound(range);
    assert_reference(lower, 6, 8, 1);
    assert_reference(upper, 6, 8, 1);
    gsw_key_unref(lower);
    gsw_key_unref(upper);

    expanded = gsw_verse_list_expand(list, &error);
    g_assert_no_error(error);
    g_assert_nonnull(expanded);
    g_assert_cmpuint(expanded->len, ==, 4);
    g_assert_cmpstr(gsw_key_get_text(g_ptr_array_index(expanded, 0)),
                    ==, "John 3:16");
    g_assert_cmpstr(gsw_key_get_text(g_ptr_array_index(expanded, 3)),
                    ==, "Romans 8:1");
    g_ptr_array_unref(expanded);
    gsw_verse_list_unref(list);
    gsw_key_unref(context);
}

static void
test_relative_ranges(void)
{
    GError *error = NULL;
    GswVerseKey *context = gsw_verse_key_new("Gen 1:1", "KJV");
    GswVerseList *list = gsw_verse_list_parse(
        "Gen 1:31-2:2; John 3:16-18", context, &error);
    GswVerseKey *range;
    GswVerseKey *upper;
    GPtrArray *expanded;

    g_assert_no_error(error);
    g_assert_nonnull(list);
    g_assert_cmpuint(gsw_verse_list_get_length(list), ==, 2);

    range = gsw_verse_list_get_range(list, 0);
    upper = gsw_verse_key_get_upper_bound(range);
    assert_reference(upper, 1, 2, 2);
    gsw_key_unref(upper);

    expanded = gsw_verse_list_expand(list, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(expanded->len, ==, 6);
    g_assert_cmpstr(gsw_key_get_text(g_ptr_array_index(expanded, 0)),
                    ==, "Genesis 1:31");
    g_assert_cmpstr(gsw_key_get_text(g_ptr_array_index(expanded, 1)),
                    ==, "Genesis 2:1");
    g_assert_cmpstr(gsw_key_get_text(g_ptr_array_index(expanded, 2)),
                    ==, "Genesis 2:2");
    g_ptr_array_unref(expanded);
    gsw_verse_list_unref(list);
    gsw_key_unref(context);
}

static void
test_invalid_range(void)
{
    GError *error = NULL;
    GswVerseKey *context = gsw_verse_key_new("Gen 1:1", "KJV");
    GswVerseList *list = gsw_verse_list_parse(
        "John 3:18-16", context, &error);

    g_assert_null(list);
    g_assert_nonnull(error);
    g_clear_error(&error);
    gsw_key_unref(context);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/verse-list/parse", test_parse_list);
    g_test_add_func("/verse-list/relative-ranges", test_relative_ranges);
    g_test_add_func("/verse-list/invalid-range", test_invalid_range);
    return g_test_run();
}
