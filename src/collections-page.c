/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * collections-page.c: verse lists, bookmarks and favorites placeholder
 */

#include "collections-page.h"
#include "window-private.h"

#include <glib/gi18n.h>

struct _GswordCollectionsPage {
    GswordWindow *window;
    GtkWidget *root;
};

static void
collections_page_free(gpointer data)
{
    g_free(data);
}

static void
on_notes_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_show_personal(((GswordCollectionsPage *)user_data)->window,
                                GSWORD_PERSONAL_SECTION_NOTES);
}

GswordCollectionsPage *
gsword_collections_page_new(GswordWindow *window)
{
    GswordCollectionsPage *page = g_new0(GswordCollectionsPage, 1);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *title = gtk_label_new(_("Collections"));
    GtkWidget *description = gtk_label_new(
        _("Verse lists, bookmarks and favorites will be organized here."));
    GtkWidget *hint = gtk_label_new(
        _("For now, verse lists are managed from the Notes section."));
    GtkWidget *button = gtk_button_new_with_label(_("Open notes"));

    g_return_val_if_fail(GSWORD_IS_WINDOW(window), NULL);
    page->window = window;
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);
    gtk_widget_set_valign(box, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_widget_add_css_class(title, "title-1");
    gtk_label_set_xalign(GTK_LABEL(description), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(description), TRUE);
    gtk_widget_add_css_class(description, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_set_halign(button, GTK_ALIGN_START);
    g_signal_connect(button, "clicked", G_CALLBACK(on_notes_clicked), page);
    gtk_box_append(GTK_BOX(box), title);
    gtk_box_append(GTK_BOX(box), description);
    gtk_box_append(GTK_BOX(box), hint);
    gtk_box_append(GTK_BOX(box), button);
    page->root = box;
    g_object_set_data_full(G_OBJECT(box), "gsword-collections-page", page,
                           collections_page_free);
    return page;
}

GtkWidget *
gsword_collections_page_widget(GswordCollectionsPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}
