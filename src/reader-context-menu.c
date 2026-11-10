/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * reader-context-menu.c: reader verse context menu
 */

#include "reader-page-private.h"
#include "window-private.h"

#include <glib/gi18n.h>

static void
on_context_copy(GtkButton *button, gpointer user_data)
{
    GswordReaderPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    gsword_reader_page_copy(page);
}

static void
on_context_note(GtkButton *button, gpointer user_data)
{
    GswordReaderPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    gsword_window_open_current_note(READER_WINDOW(page));
}

static void
on_context_highlight(GtkButton *button, gpointer user_data)
{
    GswordReaderPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    gsword_reader_page_toggle_highlight(page);
}

static void
on_context_prayer(GtkButton *button, gpointer user_data)
{
    GswordReaderPage *page = user_data;
    GswModule *module = gsword_window_get_module(READER_WINDOW(page));
    gchar *storage_key;
    gchar *display_key;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    if (module == NULL)
        return;
    storage_key = gsword_window_current_storage_key(READER_WINDOW(page));
    display_key = gsword_window_current_display_key(READER_WINDOW(page));
    gsword_window_open_prayer_for(READER_WINDOW(page),
                                  gsw_module_get_name(module),
                                  storage_key, display_key);
    g_free(storage_key);
    g_free(display_key);
}

static void
on_context_list_check_toggled(GtkCheckButton *button, gpointer user_data)
{
    GswordReaderPage *page = user_data;
    GswordNotesStore *store = gsword_window_get_notes_store(
        READER_WINDOW(page));
    GswModule *module = gsword_window_get_module(READER_WINDOW(page));
    const gchar *id_text = g_object_get_data(G_OBJECT(button), "list-id");
    gchar *storage_key = gsword_window_current_storage_key(
        READER_WINDOW(page));
    gchar *display_key = gsword_window_current_display_key(
        READER_WINDOW(page));
    GError *error = NULL;
    gint64 list_id;
    gboolean active = gtk_check_button_get_active(button);

    if (page->updating_context_list || store == NULL || module == NULL ||
        storage_key == NULL || id_text == NULL) {
        g_free(storage_key);
        g_free(display_key);
        return;
    }
    list_id = g_ascii_strtoll(id_text, NULL, 10);
    if (active) {
        if (!gsword_notes_store_add_to_list(
                store, list_id, gsw_module_get_name(module), storage_key,
                display_key, &error)) {
            gsword_window_show_error(READER_WINDOW(page),
                                     _("Cannot add verse to list"), error);
            g_free(storage_key);
            g_free(display_key);
            return;
        }
    } else {
        if (!gsword_notes_store_remove_from_list(
                store, list_id, gsw_module_get_name(module), storage_key,
                &error)) {
            gsword_window_show_error(READER_WINDOW(page),
                                     _("Cannot remove verse from list"), error);
            g_free(storage_key);
            g_free(display_key);
            return;
        }
    }
    gsword_window_set_status(READER_WINDOW(page), active
                             ? _("Verse added to list")
                             : _("Verse removed from list"));
    g_free(storage_key);
    g_free(display_key);
}

static void
on_context_manage_lists(GtkButton *button, gpointer user_data)
{
    GswordReaderPage *page = user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(button),
                                                 GTK_TYPE_POPOVER);

    if (popover != NULL)
        gtk_popover_popdown(GTK_POPOVER(popover));
    gtk_popover_popdown(page->context_menu);
    gsword_window_show_personal(READER_WINDOW(page),
                                GSWORD_PERSONAL_SECTION_COLLECTIONS);
}

static void
rebuild_context_list_menu(GswordReaderPage *page)
{
    GswordNotesStore *store;
    GswModule *module;
    gchar *storage_key;
    GPtrArray *lists;
    GtkWidget *child;
    GtkWidget *manage;
    gboolean has_normal = FALSE;
    guint i;

    if (page->context_list_box == NULL)
        return;
    page->updating_context_list = TRUE;
    while ((child = gtk_widget_get_first_child(
                GTK_WIDGET(page->context_list_box))) != NULL)
        gtk_box_remove(page->context_list_box, child);
    store = gsword_window_get_notes_store(READER_WINDOW(page));
    module = gsword_window_get_module(READER_WINDOW(page));
    storage_key = gsword_window_current_storage_key(READER_WINDOW(page));
    if (store == NULL || module == NULL || storage_key == NULL) {
        GtkWidget *label = gtk_label_new(_("Open a verse first"));

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_set_margin_top(label, 6);
        gtk_widget_set_margin_bottom(label, 6);
        gtk_widget_add_css_class(label, "dim-label");
        gtk_box_append(page->context_list_box, label);
        g_free(storage_key);
        page->updating_context_list = FALSE;
        return;
    }
    lists = gsword_notes_store_list_lists(store);
    for (i = 0; i < lists->len; i++) {
        GswordVerseList *list = g_ptr_array_index(lists, i);
        GtkWidget *check;
        gchar *id;

        if (g_strcmp0(list->kind, "favorites") == 0)
            continue;
        has_normal = TRUE;
        check = gtk_check_button_new_with_label(list->name);
        id = g_strdup_printf("%" G_GINT64_FORMAT, list->id);
        g_object_set_data_full(G_OBJECT(check), "list-id", id, g_free);
        gtk_check_button_set_active(
            GTK_CHECK_BUTTON(check),
            gsword_notes_store_is_in_list(store, list->id,
                                          gsw_module_get_name(module),
                                          storage_key));
        g_signal_connect(check, "toggled",
                         G_CALLBACK(on_context_list_check_toggled), page);
        gtk_box_append(page->context_list_box, check);
    }
    if (!has_normal) {
        GtkWidget *label = gtk_label_new(_("No verse lists yet"));

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_set_margin_top(label, 6);
        gtk_widget_set_margin_bottom(label, 6);
        gtk_widget_add_css_class(label, "dim-label");
        gtk_box_append(page->context_list_box, label);
    }
    gtk_box_append(page->context_list_box,
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    manage = gtk_button_new_with_label(_("Verse lists"));
    gtk_widget_add_css_class(manage, "flat");
    g_signal_connect(manage, "clicked",
                     G_CALLBACK(on_context_manage_lists), page);
    gtk_box_append(page->context_list_box, manage);
    g_ptr_array_unref(lists);
    g_free(storage_key);
    page->updating_context_list = FALSE;
}

static void
on_context_list_show(GtkPopover *popover, gpointer user_data)
{
    (void)popover;
    rebuild_context_list_menu(user_data);
}

static void
on_context_bookmark(GtkButton *button, gpointer user_data)
{
    GswordReaderPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    gsword_reader_toolbar_add_bookmark(page->toolbar);
}

void
gsword_reader_page_build_context_menu(GswordReaderPage *page)
{
    GtkWidget *popover = gtk_popover_new();
    GtkWidget *menu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *copy_item = gtk_button_new_with_label(_("Copy verse"));
    GtkWidget *note_item = gtk_button_new_with_label(_("Open note"));
    GtkWidget *highlight_item = gtk_button_new_with_label(
        _("Toggle highlight"));
    GtkWidget *prayer_item = gtk_button_new_with_label(_("Add to prayer"));
    GtkWidget *list_item = gtk_menu_button_new();
    GtkWidget *list_popover = gtk_popover_new();
    GtkWidget *list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *bookmark_item = gtk_button_new_with_label(_("Add bookmark"));

    gtk_menu_button_set_label(GTK_MENU_BUTTON(list_item),
                              _("Add to verse list"));
    gtk_widget_set_halign(list_item, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(list_item, TRUE);
    gtk_widget_set_margin_top(list_box, 6);
    gtk_widget_set_margin_bottom(list_box, 6);
    gtk_widget_set_margin_start(list_box, 6);
    gtk_widget_set_margin_end(list_box, 6);
    gtk_widget_set_size_request(list_box, 220, -1);
    gtk_popover_set_child(GTK_POPOVER(list_popover), list_box);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(list_item), list_popover);
    g_signal_connect(list_popover, "show",
                     G_CALLBACK(on_context_list_show), page);
    page->context_list_box = GTK_BOX(list_box);

    page->context_menu = GTK_POPOVER(popover);
    gtk_popover_set_autohide(page->context_menu, TRUE);
    gtk_popover_set_has_arrow(page->context_menu, TRUE);
    gtk_widget_set_focusable(GTK_WIDGET(popover), FALSE);
    gtk_widget_set_margin_top(menu, 6);
    gtk_widget_set_margin_bottom(menu, 6);
    gtk_widget_set_margin_start(menu, 6);
    gtk_widget_set_margin_end(menu, 6);
    gtk_widget_add_css_class(copy_item, "flat");
    gtk_widget_add_css_class(note_item, "flat");
    gtk_widget_add_css_class(highlight_item, "flat");
    gtk_widget_add_css_class(list_item, "flat");
    gtk_widget_add_css_class(bookmark_item, "flat");
    gtk_widget_add_css_class(prayer_item, "flat");
    g_signal_connect(copy_item, "clicked",
                     G_CALLBACK(on_context_copy), page);
    g_signal_connect(note_item, "clicked",
                     G_CALLBACK(on_context_note), page);
    g_signal_connect(highlight_item, "clicked",
                     G_CALLBACK(on_context_highlight), page);
    g_signal_connect(bookmark_item, "clicked",
                     G_CALLBACK(on_context_bookmark), page);
    g_signal_connect(prayer_item, "clicked",
                     G_CALLBACK(on_context_prayer), page);
    gtk_box_append(GTK_BOX(menu), copy_item);
    gtk_box_append(GTK_BOX(menu), note_item);
    gtk_box_append(GTK_BOX(menu), highlight_item);
    gtk_box_append(GTK_BOX(menu), list_item);
    gtk_box_append(GTK_BOX(menu), bookmark_item);
    gtk_box_append(GTK_BOX(menu), prayer_item);
    gtk_popover_set_child(page->context_menu, menu);
}
