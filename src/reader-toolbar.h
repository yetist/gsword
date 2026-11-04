/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * reader-toolbar.h: reader toolbar for the gsword window
 */

#ifndef GSWORD_READER_TOOLBAR_H
#define GSWORD_READER_TOOLBAR_H 1

#include "window.h"
#include "gsword-reference-chooser.h"

G_BEGIN_DECLS

typedef struct _GswordReaderPage GswordReaderPage;
typedef struct _GswordReaderToolbar GswordReaderToolbar;

GswordReaderToolbar *gsword_reader_toolbar_new(GswordWindow *window,
                                                GswordReaderPage *page);
GtkWidget *gsword_reader_toolbar_widget(GswordReaderToolbar *toolbar);
GswordReferenceChooser *gsword_reader_toolbar_get_reference_chooser(
    GswordReaderToolbar *toolbar);
GtkToggleButton *gsword_reader_toolbar_get_study_toggle(
    GswordReaderToolbar *toolbar);
void gsword_reader_toolbar_update_history(GswordReaderToolbar *toolbar);
void gsword_reader_toolbar_set_focus_mode(GswordReaderToolbar *toolbar,
                                          gboolean focus);
void gsword_reader_toolbar_set_available(GswordReaderToolbar *toolbar,
                                         gboolean available);

G_END_DECLS

#endif /* GSWORD_READER_TOOLBAR_H */
