/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsword-reference-parser.c: reference parsing and book candidate lookup
 */

#include "gsword-reference-parser.h"

#include <glib/gi18n.h>

#include <string.h>

GQuark
gsword_reference_error_quark(void)
{
    return g_quark_from_static_string("gsword-reference-error-quark");
}

void
gsword_reference_clear(GswordReference *reference)
{
    if (reference == NULL)
        return;
    g_free(reference->osis);
    reference->osis = NULL;
    reference->book = 0;
    reference->chapter = 0;
    reference->verse = 0;
    reference->verse_end = 0;
}

static GPtrArray *
find_book_candidates(const gchar *text, const gchar *locale,
                     GswVersification *v11n)
{
    GswLocaleMgr *locale_mgr = gsw_locale_mgr_get_default();
    GPtrArray *raw;
    GPtrArray *result;
    guint i;

    result = g_ptr_array_new_with_free_func(g_free);
    if (text == NULL || *text == '\0' || v11n == NULL)
        return result;
    raw = gsw_locale_mgr_get_book_candidates(locale_mgr, text,
                                             locale != NULL && *locale != '\0'
                                             ? locale : "en");
    for (i = 0; i < raw->len; i++) {
        const gchar *osis = g_ptr_array_index(raw, i);
        gint index = gsw_versification_get_book_number_by_osis(v11n, osis);
        guint j;
        gboolean duplicate = FALSE;

        if (index < 0)
            continue;
        for (j = 0; j < result->len; j++) {
            if (g_strcmp0(g_ptr_array_index(result, j), osis) == 0) {
                duplicate = TRUE;
                break;
            }
        }
        if (!duplicate)
            g_ptr_array_add(result, g_strdup(osis));
    }
    g_ptr_array_unref(raw);
    return result;
}

gboolean
gsword_reference_parse_with_candidates(const gchar *text, const gchar *locale,
                                       GswVersification *v11n,
                                       GswordReference *out,
                                       GPtrArray **candidates_out,
                                       GError **error)
{
    gchar *work;
    gchar *left;
    gchar *right = NULL;
    gchar *book_text;
    gchar *chapter_text = NULL;
    const gchar *colon;
    GPtrArray *candidates = NULL;
    gint chapter = 0;
    gint verse = 1;
    gint verse_end = 0;
    gint book_index;
    gint chapter_max;
    gint verse_max;
    gsize len;
    gsize pos;
    g_autofree gchar *result_osis = NULL;

    if (out == NULL)
        return FALSE;
    memset(out, 0, sizeof(*out));
    if (text == NULL || *text == '\0') {
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_EMPTY,
                            _("Reference is empty"));
        return FALSE;
    }
    work = g_strdup(text);
    g_strstrip(work);
    if (*work == '\0') {
        g_free(work);
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_EMPTY,
                            _("Reference is empty"));
        return FALSE;
    }
    colon = strchr(work, ':');
    if (colon != NULL) {
        left = g_strndup(work, (gsize)(colon - work));
        right = g_strdup(colon + 1);
        g_strstrip(right);
        if (*right == '\0') {
            g_free(work);
            g_free(left);
            g_free(right);
            g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                                GSWORD_REFERENCE_ERROR_INVALID_VERSE,
                                _("Verse is empty"));
            return FALSE;
        }
    } else {
        left = g_strdup(work);
    }
    g_free(work);
    g_strstrip(left);

    len = strlen(left);
    pos = len;
    while (pos > 0 && g_ascii_isdigit(left[pos - 1]))
        pos--;
    if (pos == len) {
        book_text = g_strdup(left);
    } else if (pos == 0) {
        /* A bare number can match several numbered books.  Keep it as a
         * book token so the caller can present the ambiguity list. */
        book_text = g_strdup(left);
    } else {
        book_text = g_strndup(left, pos);
        chapter_text = g_strdup(left + pos);
        g_strstrip(book_text);
        chapter = (gint)g_ascii_strtoll(chapter_text, NULL, 10);
        if (chapter <= 0) {
            g_free(left);
            g_free(book_text);
            g_free(chapter_text);
            g_free(right);
            g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                                GSWORD_REFERENCE_ERROR_INVALID_CHAPTER,
                                _("Chapter is invalid"));
            return FALSE;
        }
    }
    g_free(left);
    if (*book_text == '\0') {
        g_free(book_text);
        g_free(chapter_text);
        g_free(right);
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_UNKNOWN_BOOK,
                            _("Book is missing"));
        return FALSE;
    }

    candidates = find_book_candidates(book_text, locale, v11n);
    g_free(book_text);
    g_free(chapter_text);
    if (candidates_out != NULL)
        *candidates_out = g_ptr_array_ref(candidates);
    if (candidates->len == 0) {
        g_ptr_array_unref(candidates);
        g_free(right);
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_UNKNOWN_BOOK,
                            _("No matching book"));
        return FALSE;
    }
    if (candidates->len > 1) {
        g_ptr_array_unref(candidates);
        g_free(right);
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_AMBIGUOUS_BOOK,
                            _("Book abbreviation is ambiguous"));
        return FALSE;
    }
    {
        const gchar *osis = g_ptr_array_index(candidates, 0);

        book_index = gsw_versification_get_book_number_by_osis(v11n, osis);
        result_osis = g_strdup(osis);
    }
    g_ptr_array_unref(candidates);
    if (book_index < 0) {
        g_free(right);
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_UNKNOWN_BOOK,
                            _("Book is not in this versification"));
        return FALSE;
    }
    {
        GswBook *book = gsw_versification_get_book(v11n, book_index);

        chapter_max = gsw_book_get_chapter_max(book);
    }
    if (chapter < 1)
        chapter = 1;
    if (chapter > chapter_max) {
        g_free(right);
        g_set_error(error, GSWORD_REFERENCE_ERROR,
                    GSWORD_REFERENCE_ERROR_INVALID_CHAPTER,
                    _("Chapter %d is out of range"), chapter);
        return FALSE;
    }
    if (right != NULL) {
        gchar *dash = strchr(right, '-');
        gchar *end = NULL;

        if (dash != NULL) {
            gchar *end_text;

            *dash = '\0';
            end_text = g_strdup(dash + 1);
            g_strstrip(right);
            g_strstrip(end_text);
            verse = (gint)g_ascii_strtoll(right, &end, 10);
            if (end == right || (end != NULL && *end != '\0') || verse < 1) {
                g_free(end_text);
                g_free(right);
                g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                                    GSWORD_REFERENCE_ERROR_INVALID_VERSE,
                                    _("Verse is invalid"));
                return FALSE;
            }
            end = NULL;
            verse_end = (gint)g_ascii_strtoll(end_text, &end, 10);
            if (end == end_text || (end != NULL && *end != '\0') ||
                verse_end < 1) {
                g_free(end_text);
                g_free(right);
                g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                                    GSWORD_REFERENCE_ERROR_INVALID_VERSE,
                                    _("Verse is invalid"));
                return FALSE;
            }
            g_free(end_text);
            if (verse_end < verse) {
                gint swap = verse;

                verse = verse_end;
                verse_end = swap;
            }
        } else {
            verse = (gint)g_ascii_strtoll(right, &end, 10);
            if (end == right || (end != NULL && *end != '\0') ||
                verse < 1) {
                g_free(right);
                g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                                    GSWORD_REFERENCE_ERROR_INVALID_VERSE,
                                    _("Verse is invalid"));
                return FALSE;
            }
        }
    }
    {
        GswBook *book = gsw_versification_get_book(v11n, book_index);

        verse_max = gsw_book_get_verse_max(book, chapter);
    }
    if (verse < 1)
        verse = 1;
    if (verse > verse_max) {
        g_free(right);
        g_set_error(error, GSWORD_REFERENCE_ERROR,
                    GSWORD_REFERENCE_ERROR_INVALID_VERSE,
                    _("Verse %d is out of range"), verse);
        return FALSE;
    }
    if (verse_end > verse_max) {
        g_free(right);
        g_set_error(error, GSWORD_REFERENCE_ERROR,
                    GSWORD_REFERENCE_ERROR_INVALID_VERSE,
                    _("Verse %d is out of range"), verse_end);
        return FALSE;
    }
    if (verse_end <= verse)
        verse_end = 0;
    g_free(right);
    out->osis = g_steal_pointer(&result_osis);
    out->book = book_index;
    out->chapter = chapter;
    out->verse = verse;
    out->verse_end = verse_end;
    return TRUE;
}

gboolean
gsword_reference_parse(const gchar *text, const gchar *locale,
                       const gchar *versification, GswordReference *out,
                       GError **error)
{
    GswVersification *v11n;
    GError *local_error = NULL;
    gboolean result;

    v11n = gsw_versification_mgr_get_system(
        gsw_versification_mgr_get_default(),
        versification != NULL && *versification != '\0'
        ? versification : "KJV");
    if (v11n == NULL) {
        g_set_error_literal(error, GSWORD_REFERENCE_ERROR,
                            GSWORD_REFERENCE_ERROR_UNKNOWN_BOOK,
                            _("Unknown versification"));
        return FALSE;
    }
    result = gsword_reference_parse_with_candidates(text, locale, v11n, out,
                                                    NULL, &local_error);
    if (!result) {
        g_propagate_error(error, local_error);
        return FALSE;
    }
    g_clear_error(&local_error);
    return TRUE;
}
