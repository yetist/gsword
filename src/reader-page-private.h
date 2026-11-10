/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * reader-page-private.h: internal reader page state shared by reader modules
 */

#ifndef GSWORD_READER_PAGE_PRIVATE_H
#define GSWORD_READER_PAGE_PRIVATE_H 1

#include "reader-page.h"
#include "reader-history.h"
#include "reader-toolbar.h"
#include "reference-selection.h"
#include "gsword-reference-chooser.h"

G_BEGIN_DECLS

struct _GswordReaderPage {
    GswordPage parent_instance;

    GswordReaderToolbar *toolbar;
    GswordStudyPanel *study_panel;
    GtkTextView *view;
    GtkWidget *scrolled;
    GtkWidget *empty_bar;
    GtkPaned *paned;
    GtkWidget *study_widget;
    GswordReferenceChooser *reference_chooser;
    GswordReferenceChooser *compact_reference_chooser;
    GswordReferenceContext reference_context;
    GswordReferenceContext compact_reference_context;
    GtkTextTag *highlight_tag;
    GtkTextTag *active_verse_tag;
    GtkTextTag *range_tag;
    GtkTextTag *spacing_tag;
    GtkPopover *context_menu;
    GtkBox *context_list_box;
    gboolean updating_context_list;
    gint active_verse;
    gint range_start;
    gint range_end;
    gboolean range_active;
    GswordReaderHistory *history;
    gboolean history_navigating;
    gboolean focus_mode;
    gboolean focus_sidebar_visible;
    gboolean focus_study_visible;
    gboolean updating_study_width;
    gboolean study_width_ready;
    gint study_width;
    gint paragraph_spacing;
    gint reading_width;
    guint margin_idle;
    guint scroll_idle;
    guint compact_timeout_id;
    guint compact_idle_id;
};

#define READER_WINDOW(page) \
    gsword_page_get_window(GSWORD_PAGE(page))

void gsword_reader_page_build_context_menu(GswordReaderPage *page);
void gsword_reader_page_apply_highlights(GswordReaderPage *page);
void gsword_reader_page_clear_range(GswordReaderPage *page);
void gsword_reader_page_apply_range(GswordReaderPage *page);
gboolean gsword_reader_page_verse_at_position(GtkTextView *view, gdouble x,
                                              gdouble y, gint *verse);
void gsword_reader_page_sync_reference_chooser(GswordReaderPage *page);
void gsword_reader_page_update_reference_status(GswordReaderPage *page);

G_END_DECLS

#endif /* GSWORD_READER_PAGE_PRIVATE_H */
