/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * notes-page-private.h: internal notes page state shared by notes modules
 */

#ifndef GSWORD_NOTES_PAGE_PRIVATE_H
#define GSWORD_NOTES_PAGE_PRIVATE_H 1

#include "notes-page.h"
#include "insight-page.h"

G_BEGIN_DECLS

struct _GswordNotesPage {
    GswordPage parent_instance;

    GtkNotebook *notebook;
    GswordInsightPage *insight_page;
    GtkEntry *search;
    GtkStack *left_stack;
    GtkWidget *tag_filter_bar;
    GtkLabel *tag_filter_label;
    GtkLabel *notes_count_label;
    gchar *tag_filter;
    GtkLabel *key_label;
    GtkWidget *context_box;
    GtkLabel *context_title;
    GtkTextView *context_text;
    GtkTextView *text;
    GtkEntry *tags_entry;
    GtkMenuButton *tag_add_button;
    GtkBox *tag_menu_box;
    GtkMenuButton *add_to_list_button;
    GtkBox *list_menu_box;
    GtkLabel *save_state;
    GtkButton *save_button;
    GtkButton *delete_note_button;
    gboolean loading;
    gboolean dirty;
    gboolean updating_list_menu;
    guint autosave_source;
    GtkListBox *notes_list;
    GtkListBox *tags_list;
    GtkButton *rename_tag_button;
    GtkButton *delete_tag_button;
    GtkButton *show_tagged_notes_button;
    gchar *module;
    gchar *storage_key;
    gchar *display_key;
    gint64 note_id;
};

void gsword_notes_editor_add_format_buttons(GswordNotesPage *page,
                                            GtkWidget *bar);

G_END_DECLS

#endif /* GSWORD_NOTES_PAGE_PRIVATE_H */
