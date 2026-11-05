/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * collections-page.c: verse lists and favorites for the gsword window
 */

#include "collections-page.h"
#include "bookmark-page.h"
#include "notes-store.h"
#include "window-private.h"

#include <glib/gi18n.h>

struct _GswordCollectionsPage {
    GswordWindow *window;
    GtkWidget *root;
    GtkNotebook *notebook;
    GswordBookmarkPage *bookmark_page;
    GtkListBox *lists;
    GtkEntry *new_list;
    GtkButton *rename_button;
    GtkButton *delete_button;
    GtkLabel *title_label;
    GtkLabel *count_label;
    GtkListBox *items;
    GtkButton *open_button;
    GtkButton *remove_button;
    gint64 selected_list_id;
    gboolean selected_is_favorites;
    gchar *selected_item_module;
    gchar *selected_item_key;
};

static void refresh_lists(GswordCollectionsPage *page);
static void load_items(GswordCollectionsPage *page);

static void
collections_page_free(gpointer data)
{
    GswordCollectionsPage *page = data;

    if (page == NULL)
        return;
    g_free(page->selected_item_module);
    g_free(page->selected_item_key);
    g_free(page);
}

static void
clear_list(GtkListBox *list)
{
    while (gtk_widget_get_first_child(GTK_WIDGET(list)) != NULL)
        gtk_list_box_remove(list,
                            gtk_widget_get_first_child(GTK_WIDGET(list)));
}

static void
update_actions(GswordCollectionsPage *page)
{
    gboolean has_item = page->selected_item_module != NULL &&
                        page->selected_item_key != NULL;
    gboolean has_list = page->selected_list_id != 0;

    gtk_widget_set_sensitive(GTK_WIDGET(page->rename_button),
                             has_list && !page->selected_is_favorites);
    gtk_widget_set_sensitive(GTK_WIDGET(page->delete_button),
                             has_list && !page->selected_is_favorites);
    gtk_widget_set_sensitive(GTK_WIDGET(page->open_button), has_item);
    gtk_widget_set_sensitive(GTK_WIDGET(page->remove_button), has_item);
}

static void
clear_selected_item(GswordCollectionsPage *page)
{
    g_clear_pointer(&page->selected_item_module, g_free);
    g_clear_pointer(&page->selected_item_key, g_free);
    update_actions(page);
}

static void
on_item_activated(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordCollectionsPage *page = user_data;
    const gchar *module;
    const gchar *key;

    (void)list;
    if (row == NULL)
        return;
    module = g_object_get_data(G_OBJECT(row), "item-module");
    key = g_object_get_data(G_OBJECT(row), "item-key");
    if (module != NULL && key != NULL)
        gsword_window_open_module_reference(page->window, module, key);
}

static void
load_items(GswordCollectionsPage *page)
{
    GswordNotesStore *store = gsword_window_get_notes_store(page->window);
    GPtrArray *items;
    guint count = 0;
    guint i;

    clear_list(page->items);
    clear_selected_item(page);
    if (store == NULL || page->selected_list_id == 0) {
        gtk_label_set_text(page->count_label, "");
        return;
    }
    items = gsword_notes_store_list_items(store, page->selected_list_id);
    count = items->len;
    for (i = 0; i < items->len; i++) {
        GswordVerseListItem *item = g_ptr_array_index(items, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *verse = gtk_label_new(item->display_key);
        GtkWidget *module = gtk_label_new(item->module);

        gtk_widget_set_margin_top(box, 5);
        gtk_widget_set_margin_bottom(box, 5);
        gtk_widget_set_margin_start(box, 8);
        gtk_widget_set_margin_end(box, 8);
        gtk_label_set_xalign(GTK_LABEL(verse), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(verse), PANGO_ELLIPSIZE_END);
        gtk_label_set_xalign(GTK_LABEL(module), 0.0f);
        gtk_widget_add_css_class(module, "caption");
        gtk_widget_add_css_class(module, "dim-label");
        gtk_box_append(GTK_BOX(box), verse);
        gtk_box_append(GTK_BOX(box), module);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data_full(G_OBJECT(row), "item-module",
                               g_strdup(item->module), g_free);
        g_object_set_data_full(G_OBJECT(row), "item-key",
                               g_strdup(item->key), g_free);
        gtk_list_box_append(page->items, row);
    }
    g_ptr_array_unref(items);
    {
        gchar *text = g_strdup_printf(
            g_dngettext(GETTEXT_PACKAGE, "%d verse", "%d verses", count),
            count);

        gtk_label_set_text(page->count_label, text);
        g_free(text);
    }
}

static void
on_item_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordCollectionsPage *page = user_data;
    const gchar *module;
    const gchar *key;

    (void)list;
    if (row == NULL) {
        clear_selected_item(page);
        return;
    }
    module = g_object_get_data(G_OBJECT(row), "item-module");
    key = g_object_get_data(G_OBJECT(row), "item-key");
    g_free(page->selected_item_module);
    g_free(page->selected_item_key);
    page->selected_item_module = g_strdup(module);
    page->selected_item_key = g_strdup(key);
    update_actions(page);
}

static void
on_list_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordCollectionsPage *page = user_data;
    const gchar *id_text;
    const gchar *kind;
    const gchar *name;

    (void)list;
    if (row == NULL)
        return;
    id_text = g_object_get_data(G_OBJECT(row), "list-id");
    kind = g_object_get_data(G_OBJECT(row), "list-kind");
    name = g_object_get_data(G_OBJECT(row), "list-name");
    page->selected_list_id = id_text != NULL
                           ? g_ascii_strtoll(id_text, NULL, 10) : 0;
    page->selected_is_favorites = g_strcmp0(kind, "favorites") == 0;
    if (page->selected_is_favorites)
        gtk_label_set_text(page->title_label, _("Favorites"));
    else
        gtk_label_set_text(page->title_label, name != NULL ? name : "");
    update_actions(page);
    load_items(page);
}

static gchar *
list_display_name(const gchar *name, const gchar *kind)
{
    return g_strcmp0(kind, "favorites") == 0 ? g_strdup(_("Favorites"))
                                             : g_strdup(name);
}

static void
refresh_lists(GswordCollectionsPage *page)
{
    GswordNotesStore *store = gsword_window_get_notes_store(page->window);
    GPtrArray *lists;
    GtkListBoxRow *selected = NULL;
    gint64 fallback_id = 0;
    guint i;

    clear_list(page->lists);
    if (store == NULL)
        return;
    lists = gsword_notes_store_list_lists(store);
    for (i = 0; i < lists->len; i++) {
        GswordVerseList *list = g_ptr_array_index(lists, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *label;
        gchar *display = list_display_name(list->name, list->kind);

        if (g_strcmp0(list->kind, "favorites") == 0) {
            GtkWidget *icon = gtk_image_new_from_icon_name(
                "starred-symbolic");

            gtk_box_append(GTK_BOX(box), icon);
        }
        label = gtk_label_new(display);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_set_hexpand(label, TRUE);
        gtk_box_append(GTK_BOX(box), label);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data_full(G_OBJECT(row), "list-id",
                               g_strdup_printf("%" G_GINT64_FORMAT,
                                               list->id),
                               g_free);
        g_object_set_data_full(G_OBJECT(row), "list-kind",
                               g_strdup(list->kind), g_free);
        g_object_set_data_full(G_OBJECT(row), "list-name",
                               g_strdup(list->name), g_free);
        gtk_list_box_append(page->lists, row);
        if (list->id == page->selected_list_id)
            selected = GTK_LIST_BOX_ROW(row);
        if (g_strcmp0(list->kind, "normal") == 0 && fallback_id == 0)
            fallback_id = list->id;
        g_free(display);
    }
    g_ptr_array_unref(lists);

    if (selected == NULL) {
        if (fallback_id != 0) {
            page->selected_list_id = fallback_id;
        } else {
            page->selected_list_id =
                gsword_notes_store_get_favorites_list_id(store);
        }
        for (GtkWidget *child = gtk_widget_get_first_child(
                 GTK_WIDGET(page->lists));
             child != NULL; child = gtk_widget_get_next_sibling(child)) {
            const gchar *id_text = g_object_get_data(G_OBJECT(child),
                                                     "list-id");

            if (id_text != NULL &&
                g_ascii_strtoll(id_text, NULL, 10) ==
                    page->selected_list_id) {
                selected = GTK_LIST_BOX_ROW(child);
                break;
            }
        }
    }
    if (selected != NULL)
        gtk_list_box_select_row(page->lists, selected);
    else
        load_items(page);
}

static void
select_list_by_id(GswordCollectionsPage *page, gint64 list_id)
{
    GtkWidget *child;

    page->selected_list_id = list_id;
    for (child = gtk_widget_get_first_child(GTK_WIDGET(page->lists));
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        const gchar *id_text = g_object_get_data(G_OBJECT(child), "list-id");

        if (id_text != NULL &&
            g_ascii_strtoll(id_text, NULL, 10) == list_id) {
            gtk_list_box_select_row(page->lists, GTK_LIST_BOX_ROW(child));
            return;
        }
    }
}

static gint64
find_list_id_by_name(GswordNotesStore *store, const gchar *name)
{
    GPtrArray *lists = gsword_notes_store_list_lists(store);
    gint64 id = 0;
    guint i;

    for (i = 0; i < lists->len; i++) {
        GswordVerseList *list = g_ptr_array_index(lists, i);

        if (g_strcmp0(list->name, name) == 0) {
            id = list->id;
            break;
        }
    }
    g_ptr_array_unref(lists);
    return id;
}

static void
on_add_list_clicked(GtkButton *button, gpointer user_data)
{
    GswordCollectionsPage *page = user_data;
    GswordNotesStore *store = gsword_window_get_notes_store(page->window);
    gchar *name = g_strdup(
        gtk_editable_get_text(GTK_EDITABLE(page->new_list)));
    GError *error = NULL;
    gint64 list_id;

    (void)button;
    g_strstrip(name);
    if (name == NULL || *name == '\0') {
        gsword_window_set_status(page->window,
                                 _("Verse list name is required"));
        g_free(name);
        return;
    }
    if (store == NULL ||
        !gsword_notes_store_create_list(store, name, &error)) {
        gsword_window_show_error(page->window, _("Cannot create verse list"),
                                 error);
        g_free(name);
        return;
    }
    list_id = find_list_id_by_name(store, name);
    gtk_editable_set_text(GTK_EDITABLE(page->new_list), "");
    gsword_window_set_status(page->window, _("Verse list created"));
    refresh_lists(page);
    if (list_id != 0)
        select_list_by_id(page, list_id);
    g_free(name);
}

static void
on_add_current_clicked(GtkButton *button, gpointer user_data)
{
    GswordCollectionsPage *page = user_data;
    GswordNotesStore *store = gsword_window_get_notes_store(page->window);
    GswModule *module = gsword_window_get_module(page->window);
    gchar *storage_key;
    gchar *display_key;
    GError *error = NULL;
    gboolean ok;

    (void)button;
    if (store == NULL || module == NULL || page->selected_list_id == 0) {
        gsword_window_set_status(page->window, _("No active verse"));
        return;
    }
    storage_key = gsword_window_current_storage_key(page->window);
    display_key = gsword_window_current_display_key(page->window);
    if (storage_key == NULL || *storage_key == '\0') {
        gsword_window_set_status(page->window, _("No active verse"));
        g_free(display_key);
        return;
    }
    if (display_key == NULL || *display_key == '\0') {
        g_free(display_key);
        display_key = g_strdup(storage_key);
    }
    if (page->selected_is_favorites) {
        ok = gsword_notes_store_add_favorite(
            store, gsw_module_get_name(module), storage_key, display_key,
            &error);
    } else {
        ok = gsword_notes_store_add_to_list(
            store, page->selected_list_id, gsw_module_get_name(module),
            storage_key, display_key, &error);
    }
    if (!ok) {
        gsword_window_show_error(page->window, _("Cannot add verse to list"),
                                 error);
    } else {
        gsword_window_set_status(
            page->window, page->selected_is_favorites
                          ? _("Verse added to favorites")
                          : _("Verse added to list"));
        load_items(page);
    }
    g_free(storage_key);
    g_free(display_key);
}

static void
on_remove_item_clicked(GtkButton *button, gpointer user_data)
{
    GswordCollectionsPage *page = user_data;
    GswordNotesStore *store = gsword_window_get_notes_store(page->window);
    GError *error = NULL;

    (void)button;
    if (store == NULL || page->selected_list_id == 0 ||
        page->selected_item_module == NULL || page->selected_item_key == NULL)
        return;
    if (!gsword_notes_store_remove_from_list(
            store, page->selected_list_id, page->selected_item_module,
            page->selected_item_key, &error)) {
        gsword_window_show_error(page->window,
                                 _("Cannot remove verse from list"), error);
        return;
    }
    gsword_window_set_status(page->window, _("Verse removed from list"));
    load_items(page);
}

static void
on_open_item_clicked(GtkButton *button, gpointer user_data)
{
    GswordCollectionsPage *page = user_data;

    (void)button;
    if (page->selected_item_module != NULL && page->selected_item_key != NULL)
        gsword_window_open_module_reference(page->window,
                                            page->selected_item_module,
                                            page->selected_item_key);
}

static void
on_rename_response(AdwAlertDialog *dialog, const gchar *response,
                   gpointer user_data)
{
    GswordCollectionsPage *page = user_data;
    GswordNotesStore *store = gsword_window_get_notes_store(page->window);
    GtkEntry *entry = g_object_get_data(G_OBJECT(dialog), "rename-entry");
    GError *error = NULL;
    const gchar *name;

    if (g_strcmp0(response, "rename") != 0 || entry == NULL || store == NULL)
        return;
    name = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (name == NULL || *name == '\0')
        return;
    if (!gsword_notes_store_rename_list(store, page->selected_list_id, name,
                                        &error)) {
        gsword_window_show_error(page->window, _("Cannot rename verse list"),
                                 error);
        return;
    }
    gsword_window_set_status(page->window, _("Verse list renamed"));
    refresh_lists(page);
    select_list_by_id(page, page->selected_list_id);
}

static void
on_rename_clicked(GtkButton *button, gpointer user_data)
{
    GswordCollectionsPage *page = user_data;
    AdwDialog *dialog;
    GtkWidget *entry;

    (void)button;
    if (page->selected_list_id == 0 || page->selected_is_favorites)
        return;
    dialog = adw_alert_dialog_new(_("Rename verse list"), NULL);
    entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(entry),
                          gtk_label_get_text(page->title_label));
    gtk_widget_set_margin_top(entry, 4);
    gtk_widget_set_margin_bottom(entry, 4);
    g_object_set_data(G_OBJECT(dialog), "rename-entry", entry);
    adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(dialog), entry);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "cancel", _("Cancel"),
                                   "rename", _("Rename"), NULL);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "rename");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    g_signal_connect(dialog, "response", G_CALLBACK(on_rename_response), page);
    adw_dialog_present(dialog, GTK_WIDGET(page->window));
}

static void
on_delete_response(AdwAlertDialog *dialog, const gchar *response,
                   gpointer user_data)
{
    GswordCollectionsPage *page = user_data;
    GswordNotesStore *store = gsword_window_get_notes_store(page->window);
    GError *error = NULL;

    (void)dialog;
    if (g_strcmp0(response, "delete") != 0 || store == NULL)
        return;
    if (!gsword_notes_store_delete_list(store, page->selected_list_id,
                                        &error)) {
        gsword_window_show_error(page->window, _("Cannot delete verse list"),
                                 error);
        return;
    }
    gsword_window_set_status(page->window, _("Verse list deleted"));
    page->selected_list_id = 0;
    refresh_lists(page);
}

static void
on_delete_clicked(GtkButton *button, gpointer user_data)
{
    GswordCollectionsPage *page = user_data;
    AdwDialog *dialog;

    (void)button;
    if (page->selected_list_id == 0 || page->selected_is_favorites)
        return;
    dialog = adw_alert_dialog_new(_("Delete verse list"), NULL);
    adw_alert_dialog_format_body(
        ADW_ALERT_DIALOG(dialog),
        _("Delete “%s” and all its verses?"),
        gtk_label_get_text(page->title_label));
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "cancel", _("Cancel"),
                                   "delete", _("Delete"), NULL);
    adw_alert_dialog_set_response_appearance(
        ADW_ALERT_DIALOG(dialog), "delete", ADW_RESPONSE_DESTRUCTIVE);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    g_signal_connect(dialog, "response", G_CALLBACK(on_delete_response), page);
    adw_dialog_present(dialog, GTK_WIDGET(page->window));
}

GswordCollectionsPage *
gsword_collections_page_new(GswordWindow *window)
{
    GswordCollectionsPage *page = g_new0(GswordCollectionsPage, 1);
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *left_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *lists_label = gtk_label_new(_("Verse lists"));
    GtkWidget *lists_scroll = gtk_scrolled_window_new();
    GtkWidget *new_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *add_list = gtk_button_new_with_label(_("Add list"));
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *right_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *add_current = gtk_button_new_with_label(
        _("Add current verse"));
    GtkWidget *items_scroll = gtk_scrolled_window_new();
    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *open = gtk_button_new_with_label(_("Open verse"));
    GtkWidget *remove = gtk_button_new_with_label(_("Remove from list"));
    GtkWidget *rename = gtk_button_new_from_icon_name(
        "document-edit-symbolic");
    GtkWidget *delete = gtk_button_new_from_icon_name("edit-delete-symbolic");

    g_return_val_if_fail(GSWORD_IS_WINDOW(window), NULL);
    page->window = window;

    gtk_widget_set_margin_top(left, 10);
    gtk_widget_set_margin_bottom(left, 10);
    gtk_widget_set_margin_start(left, 10);
    gtk_widget_set_margin_end(left, 10);
    gtk_label_set_xalign(GTK_LABEL(lists_label), 0.0f);
    gtk_widget_set_hexpand(lists_label, TRUE);
    gtk_widget_add_css_class(lists_label, "title-2");
    gtk_box_append(GTK_BOX(left_header), lists_label);
    page->lists = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(page->lists, GTK_SELECTION_SINGLE);
    g_signal_connect(page->lists, "row-selected",
                     G_CALLBACK(on_list_selected), page);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(lists_scroll),
                                  GTK_WIDGET(page->lists));
    gtk_widget_set_vexpand(lists_scroll, TRUE);
    page->new_list = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->new_list, _("List name"));
    gtk_widget_set_hexpand(GTK_WIDGET(page->new_list), TRUE);
    g_signal_connect(page->new_list, "activate",
                     G_CALLBACK(on_add_list_clicked), page);
    g_signal_connect(add_list, "clicked",
                     G_CALLBACK(on_add_list_clicked), page);
    gtk_box_append(GTK_BOX(new_row), GTK_WIDGET(page->new_list));
    gtk_box_append(GTK_BOX(new_row), add_list);
    gtk_box_append(GTK_BOX(left), left_header);
    gtk_box_append(GTK_BOX(left), lists_scroll);
    gtk_box_append(GTK_BOX(left), new_row);

    page->title_label = GTK_LABEL(gtk_label_new(_("Favorites")));
    gtk_label_set_xalign(page->title_label, 0.0f);
    gtk_widget_set_hexpand(GTK_WIDGET(page->title_label), TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(page->title_label), "title-2");
    gtk_widget_add_css_class(add_current, "flat");
    g_signal_connect(add_current, "clicked",
                     G_CALLBACK(on_add_current_clicked), page);
    gtk_box_append(GTK_BOX(right_header), GTK_WIDGET(page->title_label));
    gtk_box_append(GTK_BOX(right_header), add_current);

    page->items = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(page->items, GTK_SELECTION_SINGLE);
    g_signal_connect(page->items, "row-selected",
                     G_CALLBACK(on_item_selected), page);
    g_signal_connect(page->items, "row-activated",
                     G_CALLBACK(on_item_activated), page);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(items_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(items_scroll),
                                  GTK_WIDGET(page->items));
    gtk_widget_set_vexpand(items_scroll, TRUE);

    page->count_label = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->count_label, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->count_label), "caption");
    gtk_widget_add_css_class(GTK_WIDGET(page->count_label), "dim-label");
    page->open_button = GTK_BUTTON(open);
    page->remove_button = GTK_BUTTON(remove);
    page->rename_button = GTK_BUTTON(rename);
    page->delete_button = GTK_BUTTON(delete);
    g_signal_connect(open, "clicked", G_CALLBACK(on_open_item_clicked), page);
    g_signal_connect(remove, "clicked",
                     G_CALLBACK(on_remove_item_clicked), page);
    g_signal_connect(rename, "clicked", G_CALLBACK(on_rename_clicked), page);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_delete_clicked), page);
    gtk_widget_add_css_class(rename, "flat");
    gtk_widget_add_css_class(delete, "flat");
    gtk_widget_set_tooltip_text(rename, _("Rename verse list"));
    gtk_widget_set_tooltip_text(delete, _("Delete verse list"));
    gtk_box_append(GTK_BOX(footer), GTK_WIDGET(rename));
    gtk_box_append(GTK_BOX(footer), GTK_WIDGET(delete));
    gtk_box_append(GTK_BOX(footer), open);
    gtk_box_append(GTK_BOX(footer), remove);

    gtk_box_append(GTK_BOX(right), right_header);
    gtk_box_append(GTK_BOX(right), items_scroll);
    gtk_box_append(GTK_BOX(right), GTK_WIDGET(page->count_label));
    gtk_box_append(GTK_BOX(right), footer);
    gtk_paned_set_start_child(GTK_PANED(paned), left);
    gtk_paned_set_end_child(GTK_PANED(paned), right);
    gtk_paned_set_position(GTK_PANED(paned), 300);
    {
        GtkWidget *notebook = gtk_notebook_new();

        page->bookmark_page = gsword_bookmark_page_new(window);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), paned,
                                 gtk_label_new(_("Lists")));
        gtk_notebook_append_page(
            GTK_NOTEBOOK(notebook),
            gsword_bookmark_page_widget(page->bookmark_page),
            gtk_label_new(_("Bookmarks")));
        gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
        page->notebook = GTK_NOTEBOOK(notebook);
        page->root = notebook;
        g_object_set_data_full(G_OBJECT(notebook),
                               "gsword-collections-page", page,
                               collections_page_free);
    }
    update_actions(page);
    refresh_lists(page);
    return page;
}

GtkWidget *
gsword_collections_page_widget(GswordCollectionsPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}

void
gsword_collections_page_refresh(GswordCollectionsPage *page)
{
    g_return_if_fail(page != NULL);
    refresh_lists(page);
    if (page->bookmark_page != NULL)
        gsword_bookmark_page_refresh(page->bookmark_page);
}

void
gsword_collections_page_show_bookmarks(GswordCollectionsPage *page)
{
    g_return_if_fail(page != NULL);
    if (page->notebook != NULL)
        gtk_notebook_set_current_page(page->notebook, 1);
}
