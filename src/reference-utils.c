/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * reference-utils.c: shared UI reference parsing and formatting
 */

#include "reference-utils.h"
#include "app-locale.h"

gboolean
gsword_reference_parse_ui(const gchar *text, GswordReference *out,
                          GError **error)
{
    gchar *locale;
    gboolean valid;

    locale = gsword_app_locale_name();
    valid = gsword_reference_parse(text, locale, "KJV", out, error);
    g_free(locale);
    return valid;
}

gchar *
gsword_reference_localize(const gchar *reference, gboolean include_verse)
{
    GswVerseKey *key;
    const gchar *book;
    gchar *locale;
    gchar *result;
    gint chapter;
    gint verse;

    if (reference == NULL || *reference == '\0')
        return g_strdup("");
    key = gsw_verse_key_new(reference, NULL);
    locale = gsword_app_locale_name();
    if (key != NULL)
        gsw_key_set_locale((GswKey *)key, locale);
    g_free(locale);
    if (key == NULL || gsw_verse_key_get_book(key) <= 0 ||
        gsw_verse_key_get_chapter(key) <= 0) {
        g_clear_pointer(&key, gsw_key_unref);
        return g_strdup(reference);
    }
    book = gsw_verse_key_get_book_name(key);
    chapter = gsw_verse_key_get_chapter(key);
    verse = gsw_verse_key_get_verse(key);
    if (include_verse || verse > 1)
        result = g_strdup_printf("%s %d:%d", book, chapter, verse);
    else
        result = g_strdup_printf("%s %d", book, chapter);
    gsw_key_unref((GswKey *)key);
    return result;
}
