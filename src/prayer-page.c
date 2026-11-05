/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * prayer-page.c: prayer management page for the gsword window
 */

#include "prayer-page.h"
#include "prayer-store.h"
#include "window-private.h"

#include <glib/gi18n.h>

struct _GswordPrayerPage {
    GswordWindow *window;
    GtkWidget *root;
    GtkEntry *search;
    GtkDropDown *filter;
    GtkLabel *count_label;
    GtkListBox *list;
    GtkStack *editor_stack;
    GtkEntry *title;
    GtkDropDown *status;
    GtkTextView *body;
    GtkWidget *answered_box;
    GtkTextView *answered_note;
    GtkListBox *verse_list;
    GtkWidget *log_box;
    GtkListBox *entry_list;
    GtkEntry *entry_input;
    GtkLabel *updated_label;
    GtkButton *delete_button;
    gint64 selected_id;
    GPtrArray *verses;
    gboolean loading;
};

static void refresh_list(GswordPrayerPage *page);
static void load_prayer(GswordPrayerPage *page, gint64 id);
static gboolean save_prayer(GswordPrayerPage *page, gint status_override);
static void rebuild_verse_list(GswordPrayerPage *page);
static void rebuild_entry_list(GswordPrayerPage *page);

static const gchar *
status_label(GswordPrayerStatus status)
{
    switch (status) {
    case GSWORD_PRAYER_STATUS_ANSWERED:
        return _("Answered");
    case GSWORD_PRAYER_STATUS_ARCHIVED:
        return _("Archived");
    case GSWORD_PRAYER_STATUS_ACTIVE:
    default:
        return _("Active");
    }
}

static GswordPrayerStatus
status_from_index(guint index)
{
    switch (index) {
    case 1:
        return GSWORD_PRAYER_STATUS_ANSWERED;
    case 2:
        return GSWORD_PRAYER_STATUS_ARCHIVED;
    default:
        return GSWORD_PRAYER_STATUS_ACTIVE;
    }
}

static guint
index_from_status(GswordPrayerStatus status)
{
    switch (status) {
    case GSWORD_PRAYER_STATUS_ANSWERED:
        return 1;
    case GSWORD_PRAYER_STATUS_ARCHIVED:
        return 2;
    default:
        return 0;
    }
}

static GswordPrayerStatus
selected_filter(GswordPrayerPage *page)
{
    switch (gtk_drop_down_get_selected(page->filter)) {
    case 1:
        return GSWORD_PRAYER_STATUS_ACTIVE;
    case 2:
        return GSWORD_PRAYER_STATUS_ANSWERED;
    case 3:
        return GSWORD_PRAYER_STATUS_ARCHIVED;
    default:
        return GSWORD_PRAYER_STATUS_ANY;
    }
}

static gchar *
view_text(GtkTextView *view)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter start;
    GtkTextIter end;

    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

static void
set_view_text(GtkTextView *view, const gchar *text)
{
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(view),
                             text != NULL ? text : "", -1);
}

static void
update_answered_visibility(GswordPrayerPage *page)
{
    gboolean answered = status_from_index(
        gtk_drop_down_get_selected(page->status)) ==
        GSWORD_PRAYER_STATUS_ANSWERED;

    gtk_widget_set_visible(page->answered_box, answered);
}

static void
on_status_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    update_answered_visibility(user_data);
}

static void
free_verse_array(GswordPrayerPage *page)
{
    g_clear_pointer(&page->verses, g_ptr_array_unref);
}

static void
copy_verses(GswordPrayerPage *page, GPtrArray *verses)
{
    guint i;

    free_verse_array(page);
    page->verses = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_prayer_verse_free);
    for (i = 0; verses != NULL && i < verses->len; i++) {
        GswordPrayerVerse *verse = g_ptr_array_index(verses, i);

        g_ptr_array_add(page->verses,
                        gsword_prayer_verse_new(verse->module, verse->key,
                                                verse->display_key));
    }
}

static gboolean
verse_exists(GswordPrayerPage *page, const gchar *module, const gchar *key)
{
    guint i;

    for (i = 0; i < page->verses->len; i++) {
        GswordPrayerVerse *verse = g_ptr_array_index(page->verses, i);

        if (g_strcmp0(verse->module, module) == 0 &&
            g_strcmp0(verse->key, key) == 0)
            return TRUE;
    }
    return FALSE;
}

static void
on_verse_open(GtkButton *button, gpointer user_data)
{
    GswordPrayerVerse *verse = user_data;
    GswordPrayerPage *page = g_object_get_data(G_OBJECT(button),
                                               "gsword-prayer-page");

    if (verse != NULL && page != NULL)
        gsword_window_open_module_reference(page->window, verse->module,
                                            verse->key);
}

static void
on_verse_remove(GtkButton *button, gpointer user_data)
{
    GswordPrayerPage *page = user_data;
    guint index = GPOINTER_TO_UINT(
        g_object_get_data(G_OBJECT(button), "verse-index"));

    if (index > 0 && index <= page->verses->len)
        g_ptr_array_remove_index(page->verses, index - 1);
    rebuild_verse_list(page);
    gsword_window_set_status(page->window, _("Verse removed from prayer"));
}

static void
rebuild_verse_list(GswordPrayerPage *page)
{
    guint i;

    while (gtk_widget_get_first_child(GTK_WIDGET(page->verse_list)) != NULL) {
        gtk_list_box_remove(
            page->verse_list,
            gtk_widget_get_first_child(GTK_WIDGET(page->verse_list)));
    }
    for (i = 0; i < page->verses->len; i++) {
        GswordPrayerVerse *verse = g_ptr_array_index(page->verses, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *labels = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        GtkWidget *open = gtk_button_new_with_label(
            verse->display_key != NULL ? verse->display_key : verse->key);
        GtkWidget *module = gtk_label_new(verse->module);
        GtkWidget *remove = gtk_button_new_from_icon_name(
            "list-remove-symbolic");

        gtk_widget_set_margin_top(box, 4);
        gtk_widget_set_margin_bottom(box, 4);
        gtk_widget_set_margin_start(box, 6);
        gtk_widget_set_margin_end(box, 6);
        gtk_widget_add_css_class(open, "flat");
        gtk_widget_set_halign(open, GTK_ALIGN_START);
        gtk_widget_set_hexpand(open, TRUE);
        g_object_set_data(G_OBJECT(open), "gsword-prayer-page", page);
        g_signal_connect(open, "clicked", G_CALLBACK(on_verse_open), verse);
        gtk_label_set_xalign(GTK_LABEL(module), 0.0f);
        gtk_widget_add_css_class(module, "caption");
        gtk_widget_add_css_class(module, "dim-label");
        gtk_box_append(GTK_BOX(labels), open);
        gtk_box_append(GTK_BOX(labels), module);
        gtk_widget_add_css_class(remove, "flat");
        gtk_widget_set_tooltip_text(remove, _("Remove verse"));
        g_object_set_data(G_OBJECT(remove), "verse-index",
                          GUINT_TO_POINTER(i + 1));
        g_signal_connect(remove, "clicked", G_CALLBACK(on_verse_remove), page);
        gtk_box_append(GTK_BOX(box), labels);
        gtk_box_append(GTK_BOX(box), remove);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        gtk_list_box_append(page->verse_list, row);
    }
}

static void
add_verse(GswordPrayerPage *page, const gchar *module, const gchar *key,
          const gchar *display_key)
{
    if (module == NULL || *module == '\0' || key == NULL || *key == '\0')
        return;
    if (verse_exists(page, module, key))
        return;
    g_ptr_array_add(page->verses,
                    gsword_prayer_verse_new(module, key, display_key));
    rebuild_verse_list(page);
}

static void
on_add_current_clicked(GtkButton *button, gpointer user_data)
{
    GswordPrayerPage *page = user_data;
    GswModule *module = gsword_window_get_module(page->window);
    gchar *storage_key;
    gchar *display_key;

    (void)button;
    if (module == NULL) {
        gsword_window_set_status(page->window, _("No active verse"));
        return;
    }
    storage_key = gsword_window_current_storage_key(page->window);
    display_key = gsword_window_current_display_key(page->window);
    if (storage_key == NULL || *storage_key == '\0') {
        gsword_window_set_status(page->window, _("No active verse"));
    } else {
        add_verse(page, gsw_module_get_name(module), storage_key, display_key);
        gsword_window_set_status(page->window, _("Verse added to prayer"));
    }
    g_free(storage_key);
    g_free(display_key);
}

static void
on_new_clicked(GtkButton *button, gpointer user_data)
{
    GswordPrayerPage *page = user_data;

    (void)button;
    load_prayer(page, 0);
    gtk_widget_grab_focus(GTK_WIDGET(page->title));
}

static void
on_row_activated(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordPrayerPage *page = user_data;
    const gchar *id_text;

    (void)list;
    if (row == NULL)
        return;
    id_text = g_object_get_data(G_OBJECT(row), "prayer-id");
    if (id_text != NULL)
        load_prayer(page, g_ascii_strtoll(id_text, NULL, 10));
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
on_entry_remove(GtkButton *button, gpointer user_data)
{
    GswordPrayerPage *page = user_data;
    GswordPrayerStore *store = gsword_window_get_prayer_store(page->window);
    const gchar *id_text = g_object_get_data(G_OBJECT(button), "entry-id");
    GError *error = NULL;

    if (store == NULL || id_text == NULL)
        return;
    if (!gsword_prayer_store_delete_entry(
            store, g_ascii_strtoll(id_text, NULL, 10), &error)) {
        gsword_window_show_error(page->window,
                                 _("Cannot remove prayer update"), error);
        return;
    }
    rebuild_entry_list(page);
    refresh_list(page);
    gsword_window_set_status(page->window, _("Prayer update removed"));
}

static void
rebuild_entry_list(GswordPrayerPage *page)
{
    GswordPrayerStore *store = gsword_window_get_prayer_store(page->window);
    GPtrArray *entries;
    guint i;

    while (gtk_widget_get_first_child(GTK_WIDGET(page->entry_list)) != NULL) {
        gtk_list_box_remove(
            page->entry_list,
            gtk_widget_get_first_child(GTK_WIDGET(page->entry_list)));
    }
    gtk_widget_set_visible(page->log_box, page->selected_id > 0);
    if (page->selected_id <= 0 || store == NULL)
        return;

    entries = gsword_prayer_store_list_entries(store, page->selected_id);
    if (entries->len == 0) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(_("No prayer updates yet."));

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_add_css_class(label, "dim-label");
        gtk_widget_set_margin_top(label, 6);
        gtk_widget_set_margin_bottom(label, 6);
        gtk_widget_set_margin_start(label, 8);
        gtk_widget_set_margin_end(label, 8);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_append(page->entry_list, row);
    }
    for (i = 0; i < entries->len; i++) {
        GswordPrayerEntry *entry = g_ptr_array_index(entries, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *labels = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *body = gtk_label_new(entry->body);
        gchar *date = format_date(entry->created);
        GtkWidget *date_label = gtk_label_new(date);
        GtkWidget *remove = gtk_button_new_from_icon_name(
            "list-remove-symbolic");

        gtk_widget_set_margin_top(box, 4);
        gtk_widget_set_margin_bottom(box, 4);
        gtk_widget_set_margin_start(box, 6);
        gtk_widget_set_margin_end(box, 6);
        gtk_label_set_xalign(GTK_LABEL(body), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(body), TRUE);
        gtk_label_set_selectable(GTK_LABEL(body), TRUE);
        gtk_label_set_xalign(GTK_LABEL(date_label), 0.0f);
        gtk_widget_add_css_class(date_label, "caption");
        gtk_widget_add_css_class(date_label, "dim-label");
        gtk_widget_add_css_class(remove, "flat");
        gtk_widget_set_tooltip_text(remove, _("Remove prayer update"));
        g_object_set_data_full(G_OBJECT(remove), "entry-id",
                               g_strdup_printf("%" G_GINT64_FORMAT,
                                               entry->id),
                               g_free);
        g_signal_connect(remove, "clicked", G_CALLBACK(on_entry_remove),
                         page);
        gtk_box_append(GTK_BOX(labels), body);
        gtk_box_append(GTK_BOX(labels), date_label);
        gtk_box_append(GTK_BOX(box), labels);
        gtk_box_append(GTK_BOX(box), remove);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        gtk_list_box_append(page->entry_list, row);
        g_free(date);
    }
    g_ptr_array_unref(entries);
}

static void
on_add_entry_clicked(GtkButton *button, gpointer user_data)
{
    GswordPrayerPage *page = user_data;
    GswordPrayerStore *store = gsword_window_get_prayer_store(page->window);
    gchar *body = g_strdup(
        gtk_editable_get_text(GTK_EDITABLE(page->entry_input)));
    GError *error = NULL;

    (void)button;
    g_strstrip(body);
    if (body == NULL || *body == '\0') {
        gsword_window_set_status(page->window,
                                 _("Prayer update text is required"));
        g_free(body);
        return;
    }
    if (store == NULL || page->selected_id <= 0 ||
        gsword_prayer_store_add_entry(store, page->selected_id, body,
                                      &error) == 0) {
        gsword_window_show_error(page->window,
                                 _("Cannot add prayer update"), error);
        g_free(body);
        return;
    }
    gtk_editable_set_text(GTK_EDITABLE(page->entry_input), "");
    rebuild_entry_list(page);
    refresh_list(page);
    gsword_window_set_status(page->window, _("Prayer update added"));
    g_free(body);
}

static void
refresh_list(GswordPrayerPage *page)
{
    GswordPrayerStore *store = gsword_window_get_prayer_store(page->window);
    const gchar *query = gtk_editable_get_text(GTK_EDITABLE(page->search));
    GPtrArray *items;
    gchar *count;
    guint i;
    GtkListBoxRow *selected_row = NULL;

    while (gtk_widget_get_first_child(GTK_WIDGET(page->list)) != NULL) {
        gtk_list_box_remove(
            page->list, gtk_widget_get_first_child(GTK_WIDGET(page->list)));
    }
    if (store == NULL) {
        gtk_label_set_text(page->count_label, _("Prayer storage unavailable"));
        return;
    }

    items = gsword_prayer_store_list(store, selected_filter(page),
                                     query != NULL && *query != '\0'
                                         ? query : NULL);
    for (i = 0; i < items->len; i++) {
        GswordPrayer *prayer = g_ptr_array_index(items, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *title = gtk_label_new(prayer->title);
        gchar *date = format_date(prayer->updated > 0
                                  ? prayer->updated : prayer->created);
        gchar *meta_text = g_strdup_printf("%s · %s",
                                           status_label(prayer->status), date);
        GtkWidget *meta = gtk_label_new(meta_text);

        gtk_widget_set_margin_top(box, 6);
        gtk_widget_set_margin_bottom(box, 6);
        gtk_widget_set_margin_start(box, 8);
        gtk_widget_set_margin_end(box, 8);
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
        gtk_label_set_xalign(GTK_LABEL(meta), 0.0f);
        gtk_widget_add_css_class(meta, "caption");
        gtk_widget_add_css_class(meta, "dim-label");
        gtk_box_append(GTK_BOX(box), title);
        gtk_box_append(GTK_BOX(box), meta);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data_full(G_OBJECT(row), "prayer-id",
                               g_strdup_printf("%" G_GINT64_FORMAT,
                                               prayer->id),
                               g_free);
        gtk_list_box_append(page->list, row);
        if (prayer->id == page->selected_id)
            selected_row = GTK_LIST_BOX_ROW(row);
        g_free(meta_text);
        g_free(date);
    }
    if (selected_row != NULL)
        gtk_list_box_select_row(page->list, selected_row);
    count = g_strdup_printf(
        g_dngettext(GETTEXT_PACKAGE, "%d prayer", "%d prayers", items->len),
        items->len);
    gtk_label_set_text(page->count_label, count);
    g_free(count);
    g_ptr_array_unref(items);
}

static void
on_search_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    refresh_list(user_data);
}

static void
on_filter_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    refresh_list(user_data);
}

static void
load_prayer(GswordPrayerPage *page, gint64 id)
{
    GswordPrayerStore *store = gsword_window_get_prayer_store(page->window);
    GswordPrayer *prayer = NULL;
    gchar *date;

    page->loading = TRUE;
    page->selected_id = id;
    prayer = id > 0 && store != NULL
           ? gsword_prayer_store_get(store, id) : NULL;
    if (id > 0 && prayer == NULL)
        page->selected_id = 0;

    if (page->selected_id == 0) {
        gtk_editable_set_text(GTK_EDITABLE(page->title), "");
        set_view_text(page->body, "");
        set_view_text(page->answered_note, "");
        gtk_drop_down_set_selected(page->status, 0);
        copy_verses(page, NULL);
        gtk_label_set_text(page->updated_label, _("New prayer"));
        gtk_widget_set_sensitive(GTK_WIDGET(page->delete_button), FALSE);
        gtk_stack_set_visible_child_name(page->editor_stack, "editor");
    } else {
        gtk_editable_set_text(GTK_EDITABLE(page->title), prayer->title);
        set_view_text(page->body, prayer->body);
        set_view_text(page->answered_note, prayer->answered_note);
        gtk_drop_down_set_selected(page->status,
                                   index_from_status(prayer->status));
        copy_verses(page, prayer->verses);
        date = format_date(prayer->updated > 0
                           ? prayer->updated : prayer->created);
        if (prayer->status == GSWORD_PRAYER_STATUS_ANSWERED &&
            prayer->answered_at > 0) {
            gchar *answered = format_date(prayer->answered_at);
            gchar *text = g_strdup_printf(_("Answered %s · updated %s"),
                                          answered, date);

            gtk_label_set_text(page->updated_label, text);
            g_free(text);
            g_free(answered);
        } else {
            gchar *text = g_strdup_printf(_("Updated %s"), date);

            gtk_label_set_text(page->updated_label, text);
            g_free(text);
        }
        g_free(date);
        gtk_widget_set_sensitive(GTK_WIDGET(page->delete_button), TRUE);
        gtk_stack_set_visible_child_name(page->editor_stack, "editor");
    }
    update_answered_visibility(page);
    rebuild_verse_list(page);
    rebuild_entry_list(page);
    page->loading = FALSE;
    g_clear_pointer(&prayer, gsword_prayer_free);
    refresh_list(page);
}

static gboolean
save_prayer(GswordPrayerPage *page, gint status_override)
{
    GswordPrayerStore *store = gsword_window_get_prayer_store(page->window);
    gchar *title = g_strdup(
        gtk_editable_get_text(GTK_EDITABLE(page->title)));
    gchar *body;
    gchar *answered_note;
    GswordPrayerStatus status;
    GError *error = NULL;
    gint64 id;
    gboolean ok = FALSE;

    g_strstrip(title);
    if (title == NULL || *title == '\0') {
        gsword_window_set_status(page->window, _("Prayer title is required"));
        gtk_widget_grab_focus(GTK_WIDGET(page->title));
        g_free(title);
        return FALSE;
    }
    if (store == NULL) {
        gsword_window_set_status(page->window,
                                 _("Prayer storage unavailable"));
        g_free(title);
        return FALSE;
    }

    body = view_text(page->body);
    answered_note = view_text(page->answered_note);
    status = status_override >= 0
           ? (GswordPrayerStatus)status_override
           : status_from_index(gtk_drop_down_get_selected(page->status));

    id = page->selected_id;
    if (id == 0) {
        id = gsword_prayer_store_create(store, title, body, &error);
        ok = id != 0;
    } else {
        ok = gsword_prayer_store_update(store, id, title, body, &error);
    }
    if (ok)
        ok = gsword_prayer_store_set_status(store, id, status,
                                            answered_note, &error);
    if (ok)
        ok = gsword_prayer_store_set_verses(store, id, page->verses, &error);
    if (!ok) {
        gsword_window_show_error(page->window, _("Cannot save prayer"), error);
        g_free(title);
        g_free(body);
        g_free(answered_note);
        return FALSE;
    }

    page->selected_id = id;
    gsword_window_set_status(page->window, _("Prayer saved"));
    gsword_window_toast(page->window, _("Prayer saved"));
    g_free(title);
    g_free(body);
    g_free(answered_note);
    load_prayer(page, id);
    return TRUE;
}

static void
on_save_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    save_prayer(user_data, -1);
}

static void
on_answered_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    save_prayer(user_data, GSWORD_PRAYER_STATUS_ANSWERED);
}

static void
on_archive_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    save_prayer(user_data, GSWORD_PRAYER_STATUS_ARCHIVED);
}

static void
on_delete_response(AdwAlertDialog *dialog, const gchar *response,
                   gpointer user_data)
{
    GswordPrayerPage *page = user_data;
    GswordPrayerStore *store = gsword_window_get_prayer_store(page->window);
    GError *error = NULL;

    (void)dialog;
    if (g_strcmp0(response, "delete") != 0)
        return;
    if (store == NULL || page->selected_id == 0)
        return;
    if (!gsword_prayer_store_delete(store, page->selected_id, &error)) {
        gsword_window_show_error(page->window, _("Cannot delete prayer"),
                                 error);
        return;
    }
    page->selected_id = 0;
    load_prayer(page, 0);
    gtk_stack_set_visible_child_name(page->editor_stack, "empty");
    gsword_window_set_status(page->window, _("Prayer deleted"));
    refresh_list(page);
}

static void
on_delete_clicked(GtkButton *button, gpointer user_data)
{
    GswordPrayerPage *page = user_data;
    AdwDialog *dialog = adw_alert_dialog_new(_("Delete prayer"), NULL);

    (void)button;
    if (page->selected_id == 0)
        return;
    adw_alert_dialog_format_body(ADW_ALERT_DIALOG(dialog),
                                 _("Delete this prayer and its related verses?"));
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

static GtkWidget *
build_editor(GswordPrayerPage *page)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *status_label_widget = gtk_label_new(_("Status"));
    GtkWidget *body_label = gtk_label_new(_("Prayer"));
    GtkWidget *body_scroll = gtk_scrolled_window_new();
    GtkWidget *answered_label = gtk_label_new(_("Answered note"));
    GtkWidget *answered_scroll = gtk_scrolled_window_new();
    GtkWidget *verse_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *verse_title = gtk_label_new(_("Related verses"));
    GtkWidget *add_current = gtk_button_new_with_label(
        _("Add current verse"));
    GtkWidget *verse_scroll = gtk_scrolled_window_new();
    GtkWidget *log_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *log_title = gtk_label_new(_("Prayer log"));
    GtkWidget *entry_scroll = gtk_scrolled_window_new();
    GtkWidget *entry_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *entry_add = gtk_button_new_with_label(_("Add update"));
    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *save = gtk_button_new_with_label(_("Save"));
    GtkWidget *answered = gtk_button_new_with_label(_("Mark answered"));
    GtkWidget *archive = gtk_button_new_with_label(_("Archive"));
    GtkWidget *delete = gtk_button_new_with_label(_("Delete"));
    GtkStringList *statuses;

    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    page->title = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->title, _("Prayer title"));
    gtk_widget_set_hexpand(GTK_WIDGET(page->title), TRUE);
    page->status = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    statuses = gtk_string_list_new(
        (const gchar *[]) { _("Active"), _("Answered"), _("Archived"), NULL });
    gtk_drop_down_set_model(page->status, G_LIST_MODEL(statuses));
    g_object_unref(statuses);
    g_signal_connect(page->status, "notify::selected",
                     G_CALLBACK(on_status_changed), page);
    gtk_label_set_xalign(GTK_LABEL(status_label_widget), 0.0f);
    gtk_box_append(GTK_BOX(status_box), status_label_widget);
    gtk_box_append(GTK_BOX(status_box), GTK_WIDGET(page->status));
    gtk_box_append(GTK_BOX(header), GTK_WIDGET(page->title));
    gtk_box_append(GTK_BOX(header), status_box);

    page->updated_label = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->updated_label, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->updated_label), "caption");
    gtk_widget_add_css_class(GTK_WIDGET(page->updated_label), "dim-label");

    gtk_label_set_xalign(GTK_LABEL(body_label), 0.0f);
    gtk_widget_add_css_class(body_label, "heading");
    page->body = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_wrap_mode(page->body, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_top_margin(page->body, 8);
    gtk_text_view_set_bottom_margin(page->body, 8);
    gtk_text_view_set_left_margin(page->body, 8);
    gtk_text_view_set_right_margin(page->body, 8);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(body_scroll),
                                  GTK_WIDGET(page->body));
    gtk_widget_set_vexpand(body_scroll, TRUE);
    gtk_widget_set_size_request(body_scroll, -1, 160);

    gtk_label_set_xalign(GTK_LABEL(verse_title), 0.0f);
    gtk_widget_set_hexpand(verse_title, TRUE);
    gtk_widget_add_css_class(verse_title, "heading");
    gtk_widget_add_css_class(add_current, "flat");
    g_signal_connect(add_current, "clicked",
                     G_CALLBACK(on_add_current_clicked), page);
    gtk_box_append(GTK_BOX(verse_header), verse_title);
    gtk_box_append(GTK_BOX(verse_header), add_current);
    page->verse_list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(page->verse_list, GTK_SELECTION_NONE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(verse_scroll),
                                  GTK_WIDGET(page->verse_list));
    gtk_widget_set_size_request(verse_scroll, -1, 100);

    page->answered_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_label_set_xalign(GTK_LABEL(answered_label), 0.0f);
    gtk_widget_add_css_class(answered_label, "heading");
    page->answered_note = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_wrap_mode(page->answered_note, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_top_margin(page->answered_note, 6);
    gtk_text_view_set_bottom_margin(page->answered_note, 6);
    gtk_text_view_set_left_margin(page->answered_note, 6);
    gtk_text_view_set_right_margin(page->answered_note, 6);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(answered_scroll),
                                  GTK_WIDGET(page->answered_note));
    gtk_widget_set_size_request(answered_scroll, -1, 80);
    gtk_box_append(GTK_BOX(page->answered_box), answered_label);
    gtk_box_append(GTK_BOX(page->answered_box), answered_scroll);

    page->log_box = log_box;
    gtk_label_set_xalign(GTK_LABEL(log_title), 0.0f);
    gtk_widget_add_css_class(log_title, "heading");
    page->entry_list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(page->entry_list, GTK_SELECTION_NONE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(entry_scroll),
                                  GTK_WIDGET(page->entry_list));
    gtk_widget_set_size_request(entry_scroll, -1, 120);
    page->entry_input = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->entry_input,
                                   _("Add a prayer update…"));
    gtk_widget_set_hexpand(GTK_WIDGET(page->entry_input), TRUE);
    g_signal_connect(page->entry_input, "activate",
                     G_CALLBACK(on_add_entry_clicked), page);
    g_signal_connect(entry_add, "clicked",
                     G_CALLBACK(on_add_entry_clicked), page);
    gtk_box_append(GTK_BOX(entry_row), GTK_WIDGET(page->entry_input));
    gtk_box_append(GTK_BOX(entry_row), entry_add);
    gtk_box_append(GTK_BOX(log_box), log_title);
    gtk_box_append(GTK_BOX(log_box), entry_row);
    gtk_box_append(GTK_BOX(log_box), entry_scroll);

    page->delete_button = GTK_BUTTON(delete);
    gtk_widget_add_css_class(save, "suggested-action");
    gtk_widget_add_css_class(delete, "destructive-action");
    g_signal_connect(save, "clicked", G_CALLBACK(on_save_clicked), page);
    g_signal_connect(answered, "clicked",
                     G_CALLBACK(on_answered_clicked), page);
    g_signal_connect(archive, "clicked", G_CALLBACK(on_archive_clicked), page);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_delete_clicked), page);
    gtk_box_append(GTK_BOX(footer), save);
    gtk_box_append(GTK_BOX(footer), answered);
    gtk_box_append(GTK_BOX(footer), archive);
    gtk_box_append(GTK_BOX(footer), delete);
    gtk_widget_set_halign(footer, GTK_ALIGN_END);

    gtk_box_append(GTK_BOX(box), header);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(page->updated_label));
    gtk_box_append(GTK_BOX(box), body_label);
    gtk_box_append(GTK_BOX(box), body_scroll);
    gtk_box_append(GTK_BOX(box), verse_header);
    gtk_box_append(GTK_BOX(box), verse_scroll);
    gtk_box_append(GTK_BOX(box), log_box);
    gtk_box_append(GTK_BOX(box), page->answered_box);
    gtk_box_append(GTK_BOX(box), footer);
    return box;
}

static void
prayer_page_free(gpointer data)
{
    GswordPrayerPage *page = data;

    if (page == NULL)
        return;
    free_verse_array(page);
    g_free(page);
}

GswordPrayerPage *
gsword_prayer_page_new(GswordWindow *window)
{
    GswordPrayerPage *page = g_new0(GswordPrayerPage, 1);
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *left_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *left_title = gtk_label_new(_("Prayer"));
    GtkWidget *new_button = gtk_button_new_with_label(_("New prayer"));
    GtkWidget *search_scroll;
    GtkWidget *list_scroll;
    GtkWidget *empty;
    GtkWidget *editor;
    GtkStringList *filters;

    g_return_val_if_fail(GSWORD_IS_WINDOW(window), NULL);
    page->window = window;
    page->verses = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_prayer_verse_free);

    gtk_widget_set_margin_top(left, 10);
    gtk_widget_set_margin_bottom(left, 10);
    gtk_widget_set_margin_start(left, 10);
    gtk_widget_set_margin_end(left, 10);
    gtk_label_set_xalign(GTK_LABEL(left_title), 0.0f);
    gtk_widget_set_hexpand(left_title, TRUE);
    gtk_widget_add_css_class(left_title, "title-2");
    gtk_widget_add_css_class(new_button, "suggested-action");
    g_signal_connect(new_button, "clicked", G_CALLBACK(on_new_clicked), page);
    gtk_box_append(GTK_BOX(left_header), left_title);
    gtk_box_append(GTK_BOX(left_header), new_button);

    page->search = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->search, _("Search prayers…"));
    g_signal_connect(page->search, "changed",
                     G_CALLBACK(on_search_changed), page);
    page->filter = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    filters = gtk_string_list_new(
        (const gchar *[]) { _("All"), _("Active"), _("Answered"),
                            _("Archived"), NULL });
    gtk_drop_down_set_model(page->filter, G_LIST_MODEL(filters));
    g_object_unref(filters);
    g_signal_connect(page->filter, "notify::selected",
                     G_CALLBACK(on_filter_changed), page);
    gtk_widget_set_hexpand(GTK_WIDGET(page->filter), TRUE);

    page->list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(page->list, GTK_SELECTION_SINGLE);
    g_signal_connect(page->list, "row-activated",
                     G_CALLBACK(on_row_activated), page);
    list_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(list_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(list_scroll),
                                  GTK_WIDGET(page->list));
    gtk_widget_set_vexpand(list_scroll, TRUE);
    page->count_label = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->count_label, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->count_label), "caption");
    gtk_widget_add_css_class(GTK_WIDGET(page->count_label), "dim-label");

    gtk_box_append(GTK_BOX(left), left_header);
    gtk_box_append(GTK_BOX(left), GTK_WIDGET(page->search));
    gtk_box_append(GTK_BOX(left), GTK_WIDGET(page->filter));
    gtk_box_append(GTK_BOX(left), list_scroll);
    gtk_box_append(GTK_BOX(left), GTK_WIDGET(page->count_label));

    page->editor_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_set_transition_type(page->editor_stack,
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    empty = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_valign(empty, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(empty, GTK_ALIGN_CENTER);
    {
        GtkWidget *empty_title = gtk_label_new(_("Select a prayer"));
        GtkWidget *empty_hint = gtk_label_new(
            _("Choose a prayer from the list or create a new one."));
        GtkWidget *empty_new = gtk_button_new_with_label(_("New prayer"));

        gtk_widget_add_css_class(empty_title, "title-2");
        gtk_label_set_wrap(GTK_LABEL(empty_hint), TRUE);
        gtk_widget_add_css_class(empty_hint, "dim-label");
        g_signal_connect(empty_new, "clicked", G_CALLBACK(on_new_clicked),
                         page);
        gtk_box_append(GTK_BOX(empty), empty_title);
        gtk_box_append(GTK_BOX(empty), empty_hint);
        gtk_box_append(GTK_BOX(empty), empty_new);
    }
    editor = build_editor(page);
    gtk_stack_add_named(page->editor_stack, empty, "empty");
    gtk_stack_add_named(page->editor_stack, editor, "editor");
    gtk_stack_set_visible_child_name(page->editor_stack, "empty");
    search_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(search_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(search_scroll),
                                  GTK_WIDGET(page->editor_stack));
    gtk_widget_set_vexpand(search_scroll, TRUE);

    gtk_paned_set_start_child(GTK_PANED(paned), left);
    gtk_paned_set_end_child(GTK_PANED(paned), search_scroll);
    gtk_paned_set_position(GTK_PANED(paned), 320);
    page->root = paned;
    g_object_set_data_full(G_OBJECT(paned), "gsword-prayer-page", page,
                           prayer_page_free);
    refresh_list(page);
    return page;
}

GtkWidget *
gsword_prayer_page_widget(GswordPrayerPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}

void
gsword_prayer_page_refresh(GswordPrayerPage *page)
{
    g_return_if_fail(page != NULL);
    if (page->selected_id > 0)
        load_prayer(page, page->selected_id);
    else
        refresh_list(page);
}

void
gsword_prayer_page_open_for_reference(GswordPrayerPage *page,
                                      const gchar *module,
                                      const gchar *storage_key,
                                      const gchar *display_key)
{
    g_return_if_fail(page != NULL);

    load_prayer(page, 0);
    add_verse(page, module, storage_key, display_key);
    gtk_widget_grab_focus(GTK_WIDGET(page->title));
    gsword_window_set_status(page->window, _("New prayer draft"));
}
