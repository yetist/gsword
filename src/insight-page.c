/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * insight-page.c: free-form insight management page for the gsword window
 */

#include "insight-page.h"
#include "insight-store.h"
#include "window-private.h"

#include <glib/gi18n.h>

#include <string.h>

struct _GswordInsightPage {
    GswordWindow *window;
    GtkWidget *root;
    GtkEntry *search;
    GtkDropDown *tag_filter;
    GtkStringList *tag_model;
    GtkLabel *count_label;
    GtkListBox *list;
    GtkStack *editor_stack;
    GtkEntry *title;
    GtkTextView *body;
    GtkEntry *tags;
    GtkListBox *verse_list;
    GtkLabel *updated_label;
    GtkButton *delete_button;
    gint64 selected_id;
    GPtrArray *verses;
    gboolean updating_tags;
};

static void refresh_list(GswordInsightPage *page);
static void refresh_tags(GswordInsightPage *page);
static void load_insight(GswordInsightPage *page, gint64 id);
static void rebuild_verse_list(GswordInsightPage *page);

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

static gchar *
join_tags(GPtrArray *tags)
{
    GString *text = g_string_new(NULL);
    guint i;

    for (i = 0; tags != NULL && i < tags->len; i++) {
        const gchar *tag = g_ptr_array_index(tags, i);

        if (tag == NULL || *tag == '\0')
            continue;
        if (text->len > 0)
            g_string_append(text, ", ");
        g_string_append(text, tag);
    }
    return g_string_free(text, FALSE);
}

static const gchar *
selected_tag(GswordInsightPage *page)
{
    guint selected = gtk_drop_down_get_selected(page->tag_filter);

    if (selected == GTK_INVALID_LIST_POSITION || selected == 0 ||
        page->tag_model == NULL)
        return NULL;
    return gtk_string_list_get_string(page->tag_model, selected);
}

static void
free_verse_array(GswordInsightPage *page)
{
    g_clear_pointer(&page->verses, g_ptr_array_unref);
}

static void
copy_verses(GswordInsightPage *page, GPtrArray *verses)
{
    guint i;

    free_verse_array(page);
    page->verses = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_insight_verse_free);
    for (i = 0; verses != NULL && i < verses->len; i++) {
        GswordInsightVerse *verse = g_ptr_array_index(verses, i);

        g_ptr_array_add(page->verses,
                        gsword_insight_verse_new(verse->module, verse->key,
                                                 verse->display_key));
    }
}

static gboolean
verse_exists(GswordInsightPage *page, const gchar *module, const gchar *key)
{
    guint i;

    for (i = 0; i < page->verses->len; i++) {
        GswordInsightVerse *verse = g_ptr_array_index(page->verses, i);

        if (g_strcmp0(verse->module, module) == 0 &&
            g_strcmp0(verse->key, key) == 0)
            return TRUE;
    }
    return FALSE;
}

static void
on_verse_open(GtkButton *button, gpointer user_data)
{
    GswordInsightVerse *verse = user_data;
    GswordInsightPage *page = g_object_get_data(G_OBJECT(button),
                                                "gsword-insight-page");

    if (verse != NULL && page != NULL)
        gsword_window_open_module_reference(page->window, verse->module,
                                            verse->key);
}

static void
on_verse_remove(GtkButton *button, gpointer user_data)
{
    GswordInsightPage *page = user_data;
    guint index = GPOINTER_TO_UINT(
        g_object_get_data(G_OBJECT(button), "verse-index"));

    if (index > 0 && index <= page->verses->len)
        g_ptr_array_remove_index(page->verses, index - 1);
    rebuild_verse_list(page);
    gsword_window_set_status(page->window, _("Verse removed from insight"));
}

static void
rebuild_verse_list(GswordInsightPage *page)
{
    guint i;

    while (gtk_widget_get_first_child(GTK_WIDGET(page->verse_list)) != NULL) {
        gtk_list_box_remove(
            page->verse_list,
            gtk_widget_get_first_child(GTK_WIDGET(page->verse_list)));
    }
    for (i = 0; i < page->verses->len; i++) {
        GswordInsightVerse *verse = g_ptr_array_index(page->verses, i);
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
        g_object_set_data(G_OBJECT(open), "gsword-insight-page", page);
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
add_verse(GswordInsightPage *page, const gchar *module, const gchar *key,
          const gchar *display_key)
{
    if (module == NULL || *module == '\0' || key == NULL || *key == '\0')
        return;
    if (verse_exists(page, module, key))
        return;
    g_ptr_array_add(page->verses,
                    gsword_insight_verse_new(module, key, display_key));
    rebuild_verse_list(page);
}

static void
on_add_current_clicked(GtkButton *button, gpointer user_data)
{
    GswordInsightPage *page = user_data;
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
        add_verse(page, gsw_module_get_name(module), storage_key,
                  display_key);
        gsword_window_set_status(page->window, _("Verse added to insight"));
    }
    g_free(storage_key);
    g_free(display_key);
}

static void
on_new_clicked(GtkButton *button, gpointer user_data)
{
    GswordInsightPage *page = user_data;

    (void)button;
    load_insight(page, 0);
    gtk_widget_grab_focus(GTK_WIDGET(page->title));
}

static void
on_row_activated(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordInsightPage *page = user_data;
    const gchar *id_text;

    (void)list;
    if (row == NULL)
        return;
    id_text = g_object_get_data(G_OBJECT(row), "insight-id");
    if (id_text != NULL)
        load_insight(page, g_ascii_strtoll(id_text, NULL, 10));
}

static void
refresh_list(GswordInsightPage *page)
{
    GswordInsightStore *store = gsword_window_get_insight_store(
        page->window);
    const gchar *query = gtk_editable_get_text(GTK_EDITABLE(page->search));
    const gchar *tag = selected_tag(page);
    GPtrArray *items;
    GtkListBoxRow *selected_row = NULL;
    gchar *count;
    guint i;

    while (gtk_widget_get_first_child(GTK_WIDGET(page->list)) != NULL) {
        gtk_list_box_remove(
            page->list, gtk_widget_get_first_child(GTK_WIDGET(page->list)));
    }
    if (store == NULL) {
        gtk_label_set_text(page->count_label,
                           _("Insight storage unavailable"));
        return;
    }

    items = gsword_insight_store_list(store,
                                      query != NULL && *query != '\0'
                                          ? query : NULL,
                                      tag);
    for (i = 0; i < items->len; i++) {
        GswordInsight *insight = g_ptr_array_index(items, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        const gchar *title_text = insight->title != NULL &&
                                  *insight->title != '\0'
                                ? insight->title
                                : (insight->body != NULL ? insight->body : "");
        gchar *title_copy = g_strdup(title_text);
        gchar *newline = strchr(title_copy, '\n');
        gchar *date = format_date(insight->updated > 0
                                  ? insight->updated : insight->created);
        GString *meta = g_string_new(date);

        if (newline != NULL)
            *newline = '\0';
        if (insight->tags != NULL && insight->tags->len > 0) {
            gchar *tag_text = join_tags(insight->tags);

            g_string_append_printf(meta, " · %s", tag_text);
            g_free(tag_text);
        }
        {
            GtkWidget *title = gtk_label_new(title_copy);
            GtkWidget *meta_label = gtk_label_new(meta->str);

            gtk_widget_set_margin_top(box, 6);
            gtk_widget_set_margin_bottom(box, 6);
            gtk_widget_set_margin_start(box, 8);
            gtk_widget_set_margin_end(box, 8);
            gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
            gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
            gtk_label_set_xalign(GTK_LABEL(meta_label), 0.0f);
            gtk_widget_add_css_class(meta_label, "caption");
            gtk_widget_add_css_class(meta_label, "dim-label");
            gtk_box_append(GTK_BOX(box), title);
            gtk_box_append(GTK_BOX(box), meta_label);
        }
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data_full(G_OBJECT(row), "insight-id",
                               g_strdup_printf("%" G_GINT64_FORMAT,
                                               insight->id),
                               g_free);
        gtk_list_box_append(page->list, row);
        if (insight->id == page->selected_id)
            selected_row = GTK_LIST_BOX_ROW(row);
        g_string_free(meta, TRUE);
        g_free(title_copy);
        g_free(date);
    }
    if (selected_row != NULL)
        gtk_list_box_select_row(page->list, selected_row);
    count = g_strdup_printf(
        g_dngettext(GETTEXT_PACKAGE, "%d insight", "%d insights",
                    items->len), items->len);
    gtk_label_set_text(page->count_label, count);
    g_free(count);
    g_ptr_array_unref(items);
}

static void
refresh_tags(GswordInsightPage *page)
{
    GswordInsightStore *store = gsword_window_get_insight_store(
        page->window);
    GtkStringList *model;
    GPtrArray *tags;
    const gchar *previous;
    guint i;
    guint selected = 0;

    if (store == NULL || page->tag_filter == NULL)
        return;
    previous = selected_tag(page);
    model = gtk_string_list_new((const gchar *[]) { _("All tags"), NULL });
    tags = gsword_insight_store_list_tags(store);
    for (i = 0; i < tags->len; i++) {
        const gchar *tag = g_ptr_array_index(tags, i);

        gtk_string_list_append(model, tag);
        if (g_strcmp0(tag, previous) == 0)
            selected = i + 1;
    }
    g_ptr_array_unref(tags);

    page->updating_tags = TRUE;
    if (page->tag_model != NULL)
        g_object_unref(page->tag_model);
    page->tag_model = model;
    gtk_drop_down_set_model(page->tag_filter, G_LIST_MODEL(model));
    gtk_drop_down_set_selected(page->tag_filter, selected);
    page->updating_tags = FALSE;
}

static void
on_search_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    refresh_list(user_data);
}

static void
on_tag_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordInsightPage *page = user_data;

    (void)object;
    (void)pspec;
    if (!page->updating_tags)
        refresh_list(page);
}

static void
load_insight(GswordInsightPage *page, gint64 id)
{
    GswordInsightStore *store = gsword_window_get_insight_store(
        page->window);
    GswordInsight *insight = NULL;
    gchar *tags_text = NULL;
    gchar *date;

    page->selected_id = id;
    insight = id > 0 && store != NULL
            ? gsword_insight_store_get(store, id) : NULL;
    if (id > 0 && insight == NULL)
        page->selected_id = 0;

    if (page->selected_id == 0) {
        gtk_editable_set_text(GTK_EDITABLE(page->title), "");
        set_view_text(page->body, "");
        gtk_editable_set_text(GTK_EDITABLE(page->tags), "");
        copy_verses(page, NULL);
        gtk_label_set_text(page->updated_label, _("New insight"));
        gtk_widget_set_sensitive(GTK_WIDGET(page->delete_button), FALSE);
    } else {
        gtk_editable_set_text(GTK_EDITABLE(page->title), insight->title);
        set_view_text(page->body, insight->body);
        if (insight->tags != NULL && insight->tags->len > 0)
            tags_text = join_tags(insight->tags);
        gtk_editable_set_text(GTK_EDITABLE(page->tags),
                              tags_text != NULL ? tags_text : "");
        g_free(tags_text);
        copy_verses(page, insight->verses);
        date = format_date(insight->updated > 0
                           ? insight->updated : insight->created);
        {
            gchar *text = g_strdup_printf(_("Updated %s"), date);

            gtk_label_set_text(page->updated_label, text);
            g_free(text);
        }
        g_free(date);
        gtk_widget_set_sensitive(GTK_WIDGET(page->delete_button), TRUE);
    }
    rebuild_verse_list(page);
    gtk_stack_set_visible_child_name(page->editor_stack, "editor");
    g_clear_pointer(&insight, gsword_insight_free);
    refresh_list(page);
}

static GPtrArray *
collect_tags(GswordInsightPage *page)
{
    GPtrArray *tags = g_ptr_array_new_with_free_func(g_free);
    gchar **parts = g_strsplit_set(
        gtk_editable_get_text(GTK_EDITABLE(page->tags)), ",，", -1);
    guint i;

    for (i = 0; parts != NULL && parts[i] != NULL; i++) {
        gchar *tag = g_strdup(parts[i]);

        g_strstrip(tag);
        if (*tag != '\0')
            g_ptr_array_add(tags, tag);
        else
            g_free(tag);
    }
    g_strfreev(parts);
    return tags;
}

static gboolean
save_insight(GswordInsightPage *page)
{
    GswordInsightStore *store = gsword_window_get_insight_store(
        page->window);
    gchar *title = g_strdup(
        gtk_editable_get_text(GTK_EDITABLE(page->title)));
    gchar *body;
    GPtrArray *tags;
    GError *error = NULL;
    gint64 id;
    gboolean ok;

    g_strstrip(title);
    body = view_text(page->body);
    if ((title == NULL || *title == '\0') &&
        (body == NULL || *body == '\0')) {
        gsword_window_set_status(page->window,
                                 _("Insight text is required"));
        g_free(title);
        g_free(body);
        return FALSE;
    }
    if (store == NULL) {
        gsword_window_set_status(page->window,
                                 _("Insight storage unavailable"));
        g_free(title);
        g_free(body);
        return FALSE;
    }

    tags = collect_tags(page);
    id = page->selected_id;
    if (id == 0) {
        id = gsword_insight_store_create(store, title, body, page->verses,
                                         tags, &error);
        ok = id != 0;
    } else {
        ok = gsword_insight_store_update(store, id, title, body,
                                         page->verses, tags, &error);
    }
    g_ptr_array_unref(tags);
    if (!ok) {
        gsword_window_show_error(page->window, _("Cannot save insight"),
                                 error);
        g_free(title);
        g_free(body);
        return FALSE;
    }

    page->selected_id = id;
    gsword_window_set_status(page->window, _("Insight saved"));
    gsword_window_toast(page->window, _("Insight saved"));
    g_free(title);
    g_free(body);
    refresh_tags(page);
    load_insight(page, id);
    return TRUE;
}

static void
on_save_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    save_insight(user_data);
}

static void
on_delete_response(AdwAlertDialog *dialog, const gchar *response,
                   gpointer user_data)
{
    GswordInsightPage *page = user_data;
    GswordInsightStore *store = gsword_window_get_insight_store(
        page->window);
    GError *error = NULL;

    (void)dialog;
    if (g_strcmp0(response, "delete") != 0)
        return;
    if (store == NULL || page->selected_id == 0)
        return;
    if (!gsword_insight_store_delete(store, page->selected_id, &error)) {
        gsword_window_show_error(page->window, _("Cannot delete insight"),
                                 error);
        return;
    }
    page->selected_id = 0;
    load_insight(page, 0);
    gtk_stack_set_visible_child_name(page->editor_stack, "empty");
    gsword_window_set_status(page->window, _("Insight deleted"));
    refresh_tags(page);
    refresh_list(page);
}

static void
on_delete_clicked(GtkButton *button, gpointer user_data)
{
    GswordInsightPage *page = user_data;
    AdwDialog *dialog = adw_alert_dialog_new(_("Delete insight"), NULL);

    (void)button;
    if (page->selected_id == 0)
        return;
    adw_alert_dialog_format_body(ADW_ALERT_DIALOG(dialog),
                                 _("Delete this insight?"));
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
build_editor(GswordInsightPage *page)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *body_label = gtk_label_new(_("Insight"));
    GtkWidget *body_scroll = gtk_scrolled_window_new();
    GtkWidget *tags_label = gtk_label_new(_("Tags"));
    GtkWidget *verse_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *verse_title = gtk_label_new(_("Related verses"));
    GtkWidget *add_current = gtk_button_new_with_label(
        _("Add current verse"));
    GtkWidget *verse_scroll = gtk_scrolled_window_new();
    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *save = gtk_button_new_with_label(_("Save"));
    GtkWidget *delete = gtk_button_new_with_label(_("Delete"));

    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    page->title = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->title, _("Insight title"));
    gtk_widget_set_hexpand(GTK_WIDGET(page->title), TRUE);
    gtk_box_append(GTK_BOX(header), GTK_WIDGET(page->title));

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
    gtk_widget_set_size_request(body_scroll, -1, 220);

    gtk_label_set_xalign(GTK_LABEL(tags_label), 0.0f);
    gtk_widget_add_css_class(tags_label, "heading");
    page->tags = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->tags, _("Tags, comma separated"));

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

    page->delete_button = GTK_BUTTON(delete);
    gtk_widget_add_css_class(save, "suggested-action");
    gtk_widget_add_css_class(delete, "destructive-action");
    g_signal_connect(save, "clicked", G_CALLBACK(on_save_clicked), page);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_delete_clicked), page);
    gtk_box_append(GTK_BOX(footer), save);
    gtk_box_append(GTK_BOX(footer), delete);
    gtk_widget_set_halign(footer, GTK_ALIGN_END);

    gtk_box_append(GTK_BOX(box), header);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(page->updated_label));
    gtk_box_append(GTK_BOX(box), body_label);
    gtk_box_append(GTK_BOX(box), body_scroll);
    gtk_box_append(GTK_BOX(box), tags_label);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(page->tags));
    gtk_box_append(GTK_BOX(box), verse_header);
    gtk_box_append(GTK_BOX(box), verse_scroll);
    gtk_box_append(GTK_BOX(box), footer);
    return box;
}

static void
insight_page_free(gpointer data)
{
    GswordInsightPage *page = data;

    if (page == NULL)
        return;
    free_verse_array(page);
    g_clear_object(&page->tag_model);
    g_free(page);
}

GswordInsightPage *
gsword_insight_page_new(GswordWindow *window)
{
    GswordInsightPage *page = g_new0(GswordInsightPage, 1);
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *left_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *left_title = gtk_label_new(_("Insights"));
    GtkWidget *new_button = gtk_button_new_with_label(_("New insight"));
    GtkWidget *list_scroll = gtk_scrolled_window_new();
    GtkWidget *empty;
    GtkWidget *empty_title;
    GtkWidget *empty_hint;
    GtkWidget *empty_new;
    GtkWidget *editor;
    GtkWidget *editor_scroll;

    g_return_val_if_fail(GSWORD_IS_WINDOW(window), NULL);
    page->window = window;
    page->verses = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_insight_verse_free);

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
    gtk_entry_set_placeholder_text(page->search, _("Search insights…"));
    g_signal_connect(page->search, "changed",
                     G_CALLBACK(on_search_changed), page);
    page->tag_filter = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    page->tag_model = gtk_string_list_new(
        (const gchar *[]) { _("All tags"), NULL });
    gtk_drop_down_set_model(page->tag_filter,
                            G_LIST_MODEL(page->tag_model));
    g_signal_connect(page->tag_filter, "notify::selected",
                     G_CALLBACK(on_tag_changed), page);
    gtk_widget_set_hexpand(GTK_WIDGET(page->tag_filter), TRUE);

    page->list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(page->list, GTK_SELECTION_SINGLE);
    gtk_list_box_set_placeholder(page->list,
                                 gtk_label_new(_("No insights yet.")));
    g_signal_connect(page->list, "row-activated",
                     G_CALLBACK(on_row_activated), page);
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
    gtk_box_append(GTK_BOX(left), GTK_WIDGET(page->tag_filter));
    gtk_box_append(GTK_BOX(left), list_scroll);
    gtk_box_append(GTK_BOX(left), GTK_WIDGET(page->count_label));

    page->editor_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_set_transition_type(page->editor_stack,
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    empty = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_valign(empty, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(empty, GTK_ALIGN_CENTER);
    empty_title = gtk_label_new(_("Select an insight"));
    empty_hint = gtk_label_new(
        _("Choose an insight from the list or create a new one."));
    empty_new = gtk_button_new_with_label(_("New insight"));
    gtk_widget_add_css_class(empty_title, "title-2");
    gtk_label_set_wrap(GTK_LABEL(empty_hint), TRUE);
    gtk_widget_add_css_class(empty_hint, "dim-label");
    g_signal_connect(empty_new, "clicked", G_CALLBACK(on_new_clicked), page);
    gtk_box_append(GTK_BOX(empty), empty_title);
    gtk_box_append(GTK_BOX(empty), empty_hint);
    gtk_box_append(GTK_BOX(empty), empty_new);
    editor = build_editor(page);
    gtk_stack_add_named(page->editor_stack, empty, "empty");
    gtk_stack_add_named(page->editor_stack, editor, "editor");
    gtk_stack_set_visible_child_name(page->editor_stack, "empty");
    editor_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(editor_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(editor_scroll),
                                  GTK_WIDGET(page->editor_stack));
    gtk_widget_set_vexpand(editor_scroll, TRUE);

    gtk_paned_set_start_child(GTK_PANED(paned), left);
    gtk_paned_set_end_child(GTK_PANED(paned), editor_scroll);
    gtk_paned_set_position(GTK_PANED(paned), 320);
    page->root = paned;
    g_object_set_data_full(G_OBJECT(paned), "gsword-insight-page", page,
                           insight_page_free);
    refresh_tags(page);
    refresh_list(page);
    return page;
}

GtkWidget *
gsword_insight_page_widget(GswordInsightPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}

void
gsword_insight_page_refresh(GswordInsightPage *page)
{
    g_return_if_fail(page != NULL);

    refresh_tags(page);
    if (page->selected_id > 0)
        load_insight(page, page->selected_id);
    else
        refresh_list(page);
}
