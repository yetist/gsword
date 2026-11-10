/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * notes-list.c: notes list helper functions
 */

#include "notes-page-private.h"

#include <glib/gi18n.h>

void
gsword_notes_clear_list(GtkListBox *list)
{
    while (gtk_widget_get_first_child(GTK_WIDGET(list)) != NULL)
        gtk_list_box_remove(list,
                            gtk_widget_get_first_child(GTK_WIDGET(list)));
}

GtkWidget *
gsword_notes_make_placeholder(const gchar *text)
{
    GtkWidget *label = gtk_label_new(text);

    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_margin_top(label, 12);
    gtk_widget_set_margin_bottom(label, 12);
    gtk_widget_set_margin_start(label, 8);
    gtk_widget_set_margin_end(label, 8);
    gtk_widget_add_css_class(label, "dim-label");
    gtk_widget_add_css_class(label, "caption");
    return label;
}

gchar *
gsword_notes_format_time(gint64 timestamp)
{
    GDateTime *time;
    gchar *result;

    if (timestamp <= 0)
        return g_strdup("");
    time = g_date_time_new_from_unix_local(timestamp);
    if (time == NULL)
        return g_strdup("");
    result = g_date_time_format(time, "%Y-%m-%d %H:%M");
    g_date_time_unref(time);
    return result;
}

void
gsword_notes_select_row(GswordNotesPage *page)
{
    GtkWidget *child;

    if (page->storage_key == NULL)
        return;
    for (child = gtk_widget_get_first_child(GTK_WIDGET(page->notes_list));
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        const gchar *module = g_object_get_data(G_OBJECT(child), "note-module");
        const gchar *key = g_object_get_data(G_OBJECT(child), "note-key");

        if (g_strcmp0(module, page->module) == 0 &&
            g_strcmp0(key, page->storage_key) == 0) {
            gtk_list_box_select_row(page->notes_list,
                                    GTK_LIST_BOX_ROW(child));
            return;
        }
    }
}
