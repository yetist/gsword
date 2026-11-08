/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * verse-preview.h: hover preview widget for saved verse references
 */

#ifndef GSWORD_VERSE_PREVIEW_H
#define GSWORD_VERSE_PREVIEW_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordVersePreview GswordVersePreview;

GswordVersePreview *gsword_verse_preview_new(GswordWindow *window);
GtkWidget *gsword_verse_preview_widget(GswordVersePreview *preview);
void gsword_verse_preview_reset(GswordVersePreview *preview);
void gsword_verse_preview_show(GswordVersePreview *preview,
                               const gchar *module_name, const gchar *key,
                               const gchar *display_key);
void gsword_verse_preview_free(GswordVersePreview *preview);

G_END_DECLS

#endif /* GSWORD_VERSE_PREVIEW_H */
