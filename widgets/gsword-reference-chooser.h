/*
 * gsword-reference-chooser.h: standalone book/chapter/verse reference chooser
 *
 * This widget is intentionally independent of the application window.  It can
 * be embedded in any GTK4 program that links libgsword.
 */

#ifndef GSWORD_REFERENCE_CHOOSER_H
#define GSWORD_REFERENCE_CHOOSER_H 1

#include <gtk/gtk.h>
#include <gsword/gsword.h>

G_BEGIN_DECLS

#define GSWORD_TYPE_REFERENCE_CHOOSER (gsword_reference_chooser_get_type())
G_DECLARE_FINAL_TYPE(GswordReferenceChooser, gsword_reference_chooser,
                     GSWORD, REFERENCE_CHOOSER, GtkBox)

/**
 * GswordReference:
 * @book: global book index in the selected versification, 0-based
 * @chapter: chapter number, 1-based
 * @verse: verse number, 1-based
 * @osis: OSIS book name
 *
 * A parsed passage reference.
 */
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

GtkWidget *gsword_reference_chooser_new(const gchar *versification,
                                        const gchar *locale);

void gsword_reference_chooser_set_versification(
    GswordReferenceChooser *self, const gchar *versification);
void gsword_reference_chooser_set_locale(
    GswordReferenceChooser *self, const gchar *locale);
void gsword_reference_chooser_set_reference(
    GswordReferenceChooser *self, gint book, gint chapter, gint verse);
gboolean gsword_reference_chooser_get_reference(
    GswordReferenceChooser *self, gint *book, gint *chapter, gint *verse);
gchar *gsword_reference_chooser_dup_osis(GswordReferenceChooser *self);

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

G_END_DECLS

#endif /* GSWORD_REFERENCE_CHOOSER_H */
