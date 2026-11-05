/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * bookmark-page.c: reading-position bookmarks for the gsword window
 */

#include "bookmark-page.h"
#include "bookmark-store.h"
#include "window-private.h"

#include <glib/gi18n.h>

struct _GswordBookmarkPage {
    GswordWindow *window;
    GtkWidget *root;
    GtkListBox *list;
    GtkLabel *count_label;
    GtkButton *open_button;
    GtkButton *edit_button;
    GtkButton *delete_button;
    gint64 selected_id;
    gchar *selected_module;
    gchar *selected_key;
};

static void on_edit_clicked(GtkButton *button, gpointer user_data);
static void on_delete_clicked(GtkButton *button, gpointer user_data);

static void
bookmark_page_free(gpointer data)
{
    GswordBookmarkPage *page = data;

    if (page == NULL)
        return;
    g_free(page->selected_module);
    g_free(page->selected_key);
    g_free(page);
}

static gchar *
format_date(gint64 timestamp)
{
    GDateTime *date = g_date_time_new_from_unix_local(timestamp);
    gchar *text;

    if (date == NULL)
        return g_strdup("");
    text = g_date_time_format(date, "%Y-%m-%d");
    g_date_time_unref(date);
    return text;
}

static void
clear_selected(GswordBookmarkPage *page)
{
    page->selected_id = 0;
    g_clear_pointer(&page->selected_module, g_free);
    g_clear_pointer(&page->selected_key, g_free);
    gtk_widget_set_sensitive(GTK_WIDGET(page->open_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(page->edit_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(page->delete_button), FALSE);
}

static void
on_row_activated(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordBookmarkPage *page = user_data;
    const gchar *module;
    const gchar *key;

    (void)list;
    if (row == NULL)
        return;
    module = g_object_get_data(G_OBJECT(row), "bookmark-module");
    key = g_object_get_data(G_OBJECT(row), "bookmark-key");
    if (module != NULL && key != NULL)
        gsword_window_open_module_reference(page->window, module, key);
}

static void
on_row_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordBookmarkPage *page = user_data;
    const gchar *id_text;
    const gchar *module;
    const gchar *key;

    (void)list;
    if (row == NULL) {
        clear_selected(page);
        return;
    }
    id_text = g_object_get_data(G_OBJECT(row), "bookmark-id");
    module = g_object_get_data(G_OBJECT(row), "bookmark-module");
    key = g_object_get_data(G_OBJECT(row), "bookmark-key");
    page->selected_id = id_text != NULL
                      ? g_ascii_strtoll(id_text, NULL, 10) : 0;
    g_free(page->selected_module);
    g_free(page->selected_key);
    page->selected_module = g_strdup(module);
    page->selected_key = g_strdup(key);
    gtk_widget_set_sensitive(GTK_WIDGET(page->open_button), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(page->edit_button), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(page->delete_button), TRUE);
}

static void
close_row_menu(GtkWidget *button)
{
    GtkWidget *popover = gtk_widget_get_ancestor(button, GTK_TYPE_POPOVER);

    if (popover != NULL)
        gtk_popover_popdown(GTK_POPOVER(popover));
}

static void
on_row_menu_open(GtkButton *button, gpointer user_data)
{
    GswordBookmarkPage *page = user_data;
    const gchar *module = g_object_get_data(G_OBJECT(button),
                                            "bookmark-module");
    const gchar *key = g_object_get_data(G_OBJECT(button), "bookmark-key");

    close_row_menu(GTK_WIDGET(button));
    if (module != NULL && key != NULL)
        gsword_window_open_module_reference(page->window, module, key);
}

static void
on_row_menu_edit(GtkButton *button, gpointer user_data)
{
    GswordBookmarkPage *page = user_data;
    const gchar *id = g_object_get_data(G_OBJECT(button), "bookmark-id");

    close_row_menu(GTK_WIDGET(button));
    if (id != NULL)
        page->selected_id = g_ascii_strtoll(id, NULL, 10);
    on_edit_clicked(NULL, page);
}

static void
on_row_menu_delete(GtkButton *button, gpointer user_data)
{
    GswordBookmarkPage *page = user_data;
    const gchar *id = g_object_get_data(G_OBJECT(button), "bookmark-id");

    close_row_menu(GTK_WIDGET(button));
    if (id != NULL)
        page->selected_id = g_ascii_strtoll(id, NULL, 10);
    on_delete_clicked(NULL, page);
}

static GtkWidget *
build_row_menu(GswordBookmarkPage *page, GswordBookmark *bookmark)
{
    GtkWidget *menu_button = gtk_menu_button_new();
    GtkWidget *popover = gtk_popover_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *open = gtk_button_new_with_label(_("Open verse"));
    GtkWidget *edit = gtk_button_new_with_label(_("Edit"));
    GtkWidget *delete = gtk_button_new_with_label(_("Delete"));
    gchar *id = g_strdup_printf("%" G_GINT64_FORMAT, bookmark->id);

    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button),
                                  "view-more-symbolic");
    gtk_widget_add_css_class(menu_button, "flat");
    gtk_widget_set_tooltip_text(menu_button, _("More actions"));
    gtk_accessible_update_property(
        GTK_ACCESSIBLE(menu_button), GTK_ACCESSIBLE_PROPERTY_LABEL,
        _("More actions"), -1);
    g_object_set_data_full(G_OBJECT(open), "bookmark-module",
                           g_strdup(bookmark->module), g_free);
    g_object_set_data_full(G_OBJECT(open), "bookmark-key",
                           g_strdup(bookmark->key), g_free);
    g_object_set_data_full(G_OBJECT(edit), "bookmark-id", g_strdup(id), g_free);
    g_object_set_data_full(G_OBJECT(delete), "bookmark-id", g_strdup(id),
                           g_free);
    g_free(id);
    g_signal_connect(open, "clicked", G_CALLBACK(on_row_menu_open), page);
    g_signal_connect(edit, "clicked", G_CALLBACK(on_row_menu_edit), page);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_row_menu_delete), page);
    gtk_box_append(GTK_BOX(box), open);
    gtk_box_append(GTK_BOX(box), edit);
    gtk_box_append(GTK_BOX(box), delete);
    gtk_popover_set_child(GTK_POPOVER(popover), box);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu_button), popover);
    return menu_button;
}

static void
refresh_list(GswordBookmarkPage *page)
{
    GswordBookmarkStore *store = gsword_window_get_bookmark_store(
        page->window);
    GPtrArray *items;
    guint i;

    while (gtk_widget_get_first_child(GTK_WIDGET(page->list)) != NULL) {
        gtk_list_box_remove(
            page->list, gtk_widget_get_first_child(GTK_WIDGET(page->list)));
    }
    clear_selected(page);
    if (store == NULL) {
        gtk_label_set_text(page->count_label, _("Bookmark storage unavailable"));
        return;
    }
    items = gsword_bookmark_store_list(store);
    for (i = 0; i < items->len; i++) {
        GswordBookmark *bookmark = g_ptr_array_index(items, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *labels = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        const gchar *title_text = bookmark->title != NULL &&
                                  *bookmark->title != '\0'
                                ? bookmark->title : bookmark->display_key;
        GtkWidget *title = gtk_label_new(title_text);
        gchar *date = format_date(bookmark->created);
        gchar *meta_text = g_strdup_printf("%s · %s · %s",
                                           bookmark->module,
                                           bookmark->display_key, date);
        GtkWidget *meta = gtk_label_new(meta_text);

        gtk_widget_set_margin_top(box, 5);
        gtk_widget_set_margin_bottom(box, 5);
        gtk_widget_set_margin_start(box, 8);
        gtk_widget_set_margin_end(box, 8);
        gtk_widget_set_hexpand(labels, TRUE);
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
        gtk_label_set_xalign(GTK_LABEL(meta), 0.0f);
        gtk_widget_add_css_class(meta, "caption");
        gtk_widget_add_css_class(meta, "dim-label");
        gtk_box_append(GTK_BOX(labels), title);
        gtk_box_append(GTK_BOX(labels), meta);
        if (bookmark->note != NULL && *bookmark->note != '\0') {
            GtkWidget *note = gtk_label_new(bookmark->note);

            gtk_label_set_xalign(GTK_LABEL(note), 0.0f);
            gtk_label_set_wrap(GTK_LABEL(note), TRUE);
            gtk_widget_add_css_class(note, "dim-label");
            gtk_box_append(GTK_BOX(labels), note);
        }
        gtk_box_append(GTK_BOX(box), labels);
        gtk_box_append(GTK_BOX(box),
                       build_row_menu(page, bookmark));
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data_full(G_OBJECT(row), "bookmark-id",
                               g_strdup_printf("%" G_GINT64_FORMAT,
                                               bookmark->id),
                               g_free);
        g_object_set_data_full(G_OBJECT(row), "bookmark-module",
                               g_strdup(bookmark->module), g_free);
        g_object_set_data_full(G_OBJECT(row), "bookmark-key",
                               g_strdup(bookmark->key), g_free);
        gtk_list_box_append(page->list, row);
        g_free(meta_text);
        g_free(date);
    }
    {
        gchar *count = g_strdup_printf(
            g_dngettext(GETTEXT_PACKAGE, "%d bookmark", "%d bookmarks",
                        items->len), items->len);

        gtk_label_set_text(page->count_label, count);
        g_free(count);
    }
    g_ptr_array_unref(items);
}

static void
on_add_current_clicked(GtkButton *button, gpointer user_data)
{
    GswordBookmarkPage *page = user_data;
    GswordBookmarkStore *store = gsword_window_get_bookmark_store(
        page->window);
    GswModule *module = gsword_window_get_module(page->window);
    gchar *storage_key;
    gchar *display_key;
    GError *error = NULL;

    (void)button;
    if (store == NULL || module == NULL) {
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
    if (gsword_bookmark_store_create(
            store, display_key, gsw_module_get_name(module), storage_key,
            display_key, "", &error) == 0) {
        gsword_window_show_error(page->window, _("Cannot add bookmark"),
                                 error);
    } else {
        gsword_window_set_status(page->window, _("Bookmark added"));
        refresh_list(page);
    }
    g_free(storage_key);
    g_free(display_key);
}

static void
on_open_clicked(GtkButton *button, gpointer user_data)
{
    GswordBookmarkPage *page = user_data;

    (void)button;
    if (page->selected_module != NULL && page->selected_key != NULL)
        gsword_window_open_module_reference(page->window,
                                            page->selected_module,
                                            page->selected_key);
}

static void
on_edit_response(AdwAlertDialog *dialog, const gchar *response,
                 gpointer user_data)
{
    GswordBookmarkPage *page = user_data;
    GswordBookmarkStore *store = gsword_window_get_bookmark_store(
        page->window);
    GtkEntry *title = g_object_get_data(G_OBJECT(dialog), "title-entry");
    GtkEntry *note = g_object_get_data(G_OBJECT(dialog), "note-entry");
    GError *error = NULL;

    if (g_strcmp0(response, "save") != 0 || store == NULL ||
        title == NULL || note == NULL)
        return;
    if (!gsword_bookmark_store_update(
            store, page->selected_id,
            gtk_editable_get_text(GTK_EDITABLE(title)),
            gtk_editable_get_text(GTK_EDITABLE(note)), &error)) {
        gsword_window_show_error(page->window, _("Cannot update bookmark"),
                                 error);
        return;
    }
    gsword_window_set_status(page->window, _("Bookmark updated"));
    refresh_list(page);
}

static void
on_edit_clicked(GtkButton *button, gpointer user_data)
{
    GswordBookmarkPage *page = user_data;
    GswordBookmarkStore *store = gsword_window_get_bookmark_store(
        page->window);
    GswordBookmark *bookmark;
    AdwDialog *dialog;
    GtkWidget *grid;
    GtkWidget *title_entry;
    GtkWidget *note_entry;

    (void)button;
    if (store == NULL || page->selected_id == 0)
        return;
    bookmark = gsword_bookmark_store_get(store, page->selected_id);
    if (bookmark == NULL)
        return;
    dialog = adw_alert_dialog_new(_("Edit bookmark"), NULL);
    grid = gtk_grid_new();
    title_entry = gtk_entry_new();
    note_entry = gtk_entry_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_entry_set_placeholder_text(GTK_ENTRY(title_entry),
                                   _("Bookmark title"));
    gtk_entry_set_placeholder_text(GTK_ENTRY(note_entry),
                                   _("Bookmark note"));
    gtk_editable_set_text(GTK_EDITABLE(title_entry), bookmark->title);
    gtk_editable_set_text(GTK_EDITABLE(note_entry), bookmark->note);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(_("Title")), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), title_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(_("Note")), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), note_entry, 1, 1, 1, 1);
    g_object_set_data(G_OBJECT(dialog), "title-entry", title_entry);
    g_object_set_data(G_OBJECT(dialog), "note-entry", note_entry);
    adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(dialog), grid);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "cancel", _("Cancel"),
                                   "save", _("Save"), NULL);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "save");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    g_signal_connect(dialog, "response", G_CALLBACK(on_edit_response), page);
    adw_dialog_present(dialog, GTK_WIDGET(page->window));
    gsword_bookmark_free(bookmark);
}

static void
on_delete_response(AdwAlertDialog *dialog, const gchar *response,
                   gpointer user_data)
{
    GswordBookmarkPage *page = user_data;
    GswordBookmarkStore *store = gsword_window_get_bookmark_store(
        page->window);
    GError *error = NULL;

    (void)dialog;
    if (g_strcmp0(response, "delete") != 0 || store == NULL)
        return;
    if (!gsword_bookmark_store_delete(store, page->selected_id, &error)) {
        gsword_window_show_error(page->window, _("Cannot delete bookmark"),
                                 error);
        return;
    }
    gsword_window_set_status(page->window, _("Bookmark deleted"));
    refresh_list(page);
}

static void
on_delete_clicked(GtkButton *button, gpointer user_data)
{
    GswordBookmarkPage *page = user_data;
    AdwDialog *dialog;

    (void)button;
    if (page->selected_id == 0)
        return;
    dialog = adw_alert_dialog_new(_("Delete bookmark"), NULL);
    adw_alert_dialog_format_body(ADW_ALERT_DIALOG(dialog),
                                 _("Delete this bookmark?"));
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

GswordBookmarkPage *
gsword_bookmark_page_new(GswordWindow *window)
{
    GswordBookmarkPage *page = g_new0(GswordBookmarkPage, 1);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *title = gtk_label_new(_("Bookmarks"));
    GtkWidget *add_current = gtk_button_new_with_label(
        _("Add current verse"));
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *open = gtk_button_new_with_label(_("Open verse"));
    GtkWidget *edit = gtk_button_new_with_label(_("Edit"));
    GtkWidget *delete = gtk_button_new_with_label(_("Delete"));

    g_return_val_if_fail(GSWORD_IS_WINDOW(window), NULL);
    page->window = window;
    gtk_widget_set_margin_top(box, 10);
    gtk_widget_set_margin_bottom(box, 10);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_widget_add_css_class(title, "title-2");
    gtk_widget_add_css_class(add_current, "flat");
    g_signal_connect(add_current, "clicked",
                     G_CALLBACK(on_add_current_clicked), page);
    gtk_box_append(GTK_BOX(header), title);
    gtk_box_append(GTK_BOX(header), add_current);

    page->list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(page->list, GTK_SELECTION_SINGLE);
    gtk_list_box_set_activate_on_single_click(page->list, FALSE);
    gtk_list_box_set_placeholder(page->list,
                                 gtk_label_new(_("No bookmarks yet.")));
    g_signal_connect(page->list, "row-selected",
                     G_CALLBACK(on_row_selected), page);
    g_signal_connect(page->list, "row-activated",
                     G_CALLBACK(on_row_activated), page);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
                                  GTK_WIDGET(page->list));
    gtk_widget_set_vexpand(scroll, TRUE);

    page->count_label = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->count_label, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->count_label), "caption");
    gtk_widget_add_css_class(GTK_WIDGET(page->count_label), "dim-label");
    page->open_button = GTK_BUTTON(open);
    page->edit_button = GTK_BUTTON(edit);
    page->delete_button = GTK_BUTTON(delete);
    g_signal_connect(open, "clicked", G_CALLBACK(on_open_clicked), page);
    g_signal_connect(edit, "clicked", G_CALLBACK(on_edit_clicked), page);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_delete_clicked), page);
    gtk_box_append(GTK_BOX(footer), open);
    gtk_box_append(GTK_BOX(footer), edit);
    gtk_box_append(GTK_BOX(footer), delete);
    gtk_widget_set_halign(footer, GTK_ALIGN_END);
    {
        GtkWidget *hint = gtk_label_new(
            _("Click to select; double-click or press Enter to open"));

        gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
        gtk_widget_add_css_class(hint, "caption");
        gtk_widget_add_css_class(hint, "dim-label");
        gtk_box_append(GTK_BOX(box), header);
        gtk_box_append(GTK_BOX(box), scroll);
        gtk_box_append(GTK_BOX(box), GTK_WIDGET(page->count_label));
        gtk_box_append(GTK_BOX(box), hint);
    }
    gtk_box_append(GTK_BOX(box), footer);
    page->root = box;
    g_object_set_data_full(G_OBJECT(box), "gsword-bookmark-page", page,
                           bookmark_page_free);
    clear_selected(page);
    refresh_list(page);
    return page;
}

GtkWidget *
gsword_bookmark_page_widget(GswordBookmarkPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}

void
gsword_bookmark_page_refresh(GswordBookmarkPage *page)
{
    g_return_if_fail(page != NULL);
    refresh_list(page);
}
