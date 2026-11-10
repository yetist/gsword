/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
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

#include "gsword-reference-parser.h"

G_BEGIN_DECLS

#define GSWORD_TYPE_REFERENCE_CHOOSER (gsword_reference_chooser_get_type())
G_DECLARE_FINAL_TYPE(GswordReferenceChooser, gsword_reference_chooser,
                     GSWORD, REFERENCE_CHOOSER, GtkBox)

/**
 * gsword_reference_chooser_new:
 * @versification: versification name such as "KJV"
 * @locale: locale name such as "en" or "zh-Hans"
 *
 * Creates a reference chooser with the full path bar and navigation arrows.
 *
 * Returns: (transfer full): a new #GswordReferenceChooser
 */
GtkWidget *gsword_reference_chooser_new(const gchar *versification,
                                        const gchar *locale);

/**
 * gsword_reference_chooser_new_compact:
 * @versification: versification name such as "KJV"
 * @locale: locale name such as "en" or "zh-Hans"
 *
 * Creates a compact chooser for narrow containers.  It keeps the book,
 * chapter, and verse segments clickable but hides the navigation arrows and
 * the text-edit button.
 *
 * Returns: (transfer full): a new #GswordReferenceChooser
 */
GtkWidget *gsword_reference_chooser_new_compact(const gchar *versification,
                                                const gchar *locale);

void gsword_reference_chooser_set_versification(
    GswordReferenceChooser *self, const gchar *versification);
/**
 * gsword_reference_chooser_set_locale:
 * @self: a #GswordReferenceChooser
 * @locale: locale name such as "en" or "zh-Hans"
 *
 * Sets the language used for book names and abbreviations. The widget never
 * reads the process environment; callers choose the module or UI locale
 * explicitly.
 */
void gsword_reference_chooser_set_locale(
    GswordReferenceChooser *self, const gchar *locale);
void gsword_reference_chooser_set_reference(
    GswordReferenceChooser *self, gint book, gint chapter, gint verse);
void gsword_reference_chooser_set_reference_range(
    GswordReferenceChooser *self, gint book, gint chapter, gint verse,
    gint verse_end);
gboolean gsword_reference_chooser_get_reference(
    GswordReferenceChooser *self, gint *book, gint *chapter, gint *verse);
gint gsword_reference_chooser_get_verse_end(GswordReferenceChooser *self);
gchar *gsword_reference_chooser_dup_osis(GswordReferenceChooser *self);

/**
 * gsword_reference_chooser_begin_edit:
 * @self: a #GswordReferenceChooser
 *
 * Switches the full chooser to its text-entry state.  Intended for a window
 * accelerator such as Ctrl+L.
 */
void gsword_reference_chooser_begin_edit(GswordReferenceChooser *self);

G_END_DECLS

#endif /* GSWORD_REFERENCE_CHOOSER_H */
