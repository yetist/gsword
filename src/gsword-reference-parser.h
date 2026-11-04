/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsword-reference-parser.h: reference parsing and book candidate lookup
 */

#ifndef GSWORD_REFERENCE_PARSER_H
#define GSWORD_REFERENCE_PARSER_H 1

#include <glib.h>
#include <gsword/gsword.h>

G_BEGIN_DECLS

typedef struct {
    gint book;
    gint chapter;
    gint verse;
    gchar *osis;
} GswordReference;

GQuark gsword_reference_error_quark(void);
#define GSWORD_REFERENCE_ERROR (gsword_reference_error_quark())

typedef enum {
    GSWORD_REFERENCE_ERROR_EMPTY,
    GSWORD_REFERENCE_ERROR_UNKNOWN_BOOK,
    GSWORD_REFERENCE_ERROR_AMBIGUOUS_BOOK,
    GSWORD_REFERENCE_ERROR_INVALID_CHAPTER,
    GSWORD_REFERENCE_ERROR_INVALID_VERSE
} GswordReferenceError;

/**
 * gsword_reference_parse:
 * @text: user input such as "Gen 1", "创1", "1 Cor 13:4"
 * @locale: locale name such as "en" or "zh-Hans"
 * @versification: versification name such as "KJV"
 * @out: parsed reference
 * @error: return location for a #GError
 *
 * Parses a reference using the locale book aliases and the selected
 * versification.  Ambiguous abbreviations return
 * %GSWORD_REFERENCE_ERROR_AMBIGUOUS_BOOK.
 *
 * Returns: %TRUE on success
 */
gboolean gsword_reference_parse(const gchar *text, const gchar *locale,
                                const gchar *versification,
                                GswordReference *out, GError **error);

void gsword_reference_clear(GswordReference *reference);

/*
 * Internal parser entry used by GswordReferenceChooser.  It exposes the
 * candidate book list so the picker can present ambiguous abbreviations.
 */
gboolean gsword_reference_parse_with_candidates(const gchar *text,
                                                const gchar *locale,
                                                GswVersification *v11n,
                                                GswordReference *out,
                                                GPtrArray **candidates,
                                                GError **error);

G_END_DECLS

#endif /* GSWORD_REFERENCE_PARSER_H */
