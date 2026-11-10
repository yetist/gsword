/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * notes-page.c: notes, tags and verse lists for the gsword window
 */

#include "notes-page.h"
#include "app-paths.h"
#include "insight-page.h"
#include "window-private.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_GTKSOURCEVIEW
#include <gtksourceview/gtksource.h>
#endif

#include <string.h>

struct _GswordNotesPage {
    GswordWindow *window;
    GtkWidget *root;
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
    GtkListBox *lists_list;
    GtkButton *rename_list_button;
    GtkListBox *list_items;
    GtkLabel *list_items_label;
    GtkButton *open_list_item_button;
    GtkButton *remove_list_item_button;
    gchar *selected_item_module;
    gchar *selected_item_key;
    GtkEntry *new_list_entry;
    GtkButton *delete_list_button;
    gchar *module;
    gchar *storage_key;
    gchar *display_key;
    gint64 note_id;
    gint64 selected_list_id;
};

static gboolean save_note_page(GswordNotesPage *page, gboolean refresh);
static void update_note_actions(GswordNotesPage *page);
static void update_tag_actions(GswordNotesPage *page);
static void update_list_item_actions(GswordNotesPage *page);
static void clear_selected_item(GswordNotesPage *page);
static void load_list_items(GswordNotesPage *page);
static void on_note_selected(GtkListBox *list, GtkListBoxRow *row,
                             gpointer user_data);

static void
clear_list(GtkListBox *list)
{
    while (gtk_widget_get_first_child(GTK_WIDGET(list)) != NULL)
        gtk_list_box_remove(list,
                            gtk_widget_get_first_child(GTK_WIDGET(list)));
}

static GtkWidget *
make_placeholder(const gchar *text)
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

static gchar *
format_note_time(gint64 timestamp)
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

static void
on_clear_tag_filter(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;

    (void)button;
    g_clear_pointer(&page->tag_filter, g_free);
    gsword_notes_page_refresh(page);
}

static gboolean
on_notes_key_pressed(GtkEventControllerKey *controller, guint keyval,
                     guint keycode, GdkModifierType state, gpointer user_data)
{
    GswordNotesPage *page = user_data;

    (void)controller;
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Escape && page->tag_filter != NULL) {
        g_clear_pointer(&page->tag_filter, g_free);
        gsword_notes_page_refresh(page);
        return TRUE;
    }
    return FALSE;
}

void
gsword_notes_page_refresh(GswordNotesPage *page)
{
    GswordNotesStore *notes_store;
    GPtrArray *notes;
    GPtrArray *tags;
    GPtrArray *lists;
    guint i;
    const gchar *query;

    g_return_if_fail(page != NULL);
    notes_store = gsword_window_get_notes_store(page->window);
    if (notes_store == NULL)
        return;
    query = page->search != NULL
          ? gtk_editable_get_text(GTK_EDITABLE(page->search)) : "";
    notes = gsword_notes_store_list_notes(notes_store, query,
                                           page->tag_filter);
    tags = gsword_notes_store_list_tags(notes_store);
    lists = gsword_notes_store_list_lists(notes_store);

    gtk_list_box_set_placeholder(page->notes_list,
        make_placeholder(*query != '\0' || page->tag_filter != NULL
                         ? _("No matching notes") : _("No notes yet")));
    if (page->tag_filter_bar != NULL) {
        gboolean filtered = page->tag_filter != NULL &&
                            *page->tag_filter != '\0';

        gtk_widget_set_visible(page->tag_filter_bar, filtered);
        if (filtered && page->tag_filter_label != NULL) {
            gchar *text = g_strdup_printf(_("Tag: %s"), page->tag_filter);

            gtk_label_set_text(page->tag_filter_label, text);
            g_free(text);
        }
        if (page->left_stack != NULL) {
            GtkWidget *notes_page = gtk_stack_get_child_by_name(
                page->left_stack, "notes");

            if (notes_page != NULL) {
                GtkStackPage *stack_page = gtk_stack_get_page(
                    page->left_stack, notes_page);

                if (stack_page != NULL) {
                    if (filtered) {
                        gchar *title = g_strdup_printf(_("Notes · %s"),
                                                       page->tag_filter);

                        gtk_stack_page_set_title(stack_page, title);
                        g_free(title);
                    } else {
                        gtk_stack_page_set_title(stack_page, _("Notes"));
                    }
                }
            }
        }
    }
    if (page->notes_count_label != NULL) {
        gchar *text = g_strdup_printf(_("%u note(s)"), notes->len);

        gtk_label_set_text(page->notes_count_label, text);
        g_free(text);
    }

    clear_list(page->notes_list);
    clear_list(page->tags_list);
    clear_list(page->lists_list);
    if (page->rename_tag_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->rename_tag_button), FALSE);
    if (page->delete_tag_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->delete_tag_button), FALSE);
    if (page->show_tagged_notes_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->show_tagged_notes_button),
                                 FALSE);
    if (page->rename_list_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->rename_list_button), FALSE);
    if (page->delete_list_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->delete_list_button), FALSE);

    for (i = 0; i < notes->len; i++) {
        GswordNote *note = g_ptr_array_index(notes, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *title = gtk_label_new(note->display_key);
        GtkWidget *preview;
        GtkWidget *meta;
        const gchar *note_text = note->text != NULL ? note->text : "";
        gchar *text = g_strdup(note_text);
        gchar *cursor;
        gchar *time_text;

        for (cursor = text; *cursor != '\0'; cursor++) {
            if (*cursor == '\n' || *cursor == '\r' || *cursor == '\t')
                *cursor = ' ';
        }
        g_strstrip(text);
        preview = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(title), 24);
        gtk_label_set_single_line_mode(GTK_LABEL(title), TRUE);
        gtk_widget_add_css_class(title, "heading");
        gtk_label_set_xalign(GTK_LABEL(preview), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(preview), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(preview), 24);
        gtk_label_set_lines(GTK_LABEL(preview), 2);
        gtk_widget_add_css_class(preview, "dim-label");
        gtk_widget_add_css_class(preview, "caption");
        meta = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        if (note->tags != NULL && *note->tags != '\0') {
            GtkWidget *tags = gtk_label_new(note->tags);

            gtk_label_set_xalign(GTK_LABEL(tags), 0.0f);
            gtk_label_set_ellipsize(GTK_LABEL(tags), PANGO_ELLIPSIZE_END);
            gtk_label_set_max_width_chars(GTK_LABEL(tags), 14);
            gtk_widget_add_css_class(tags, "dim-label");
            gtk_widget_add_css_class(tags, "caption");
            gtk_box_append(GTK_BOX(meta), tags);
        }
        time_text = format_note_time(note->updated);
        if (*time_text != '\0') {
            GtkWidget *updated = gtk_label_new(time_text);

            gtk_widget_set_hexpand(updated, TRUE);
            gtk_widget_set_halign(updated, GTK_ALIGN_END);
            gtk_widget_add_css_class(updated, "dim-label");
            gtk_widget_add_css_class(updated, "caption");
            gtk_box_append(GTK_BOX(meta), updated);
        }
        g_free(time_text);
        gtk_widget_set_margin_top(box, 4);
        gtk_widget_set_margin_bottom(box, 4);
        gtk_box_append(GTK_BOX(box), title);
        gtk_box_append(GTK_BOX(box), preview);
        gtk_box_append(GTK_BOX(box), meta);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data_full(G_OBJECT(row), "note-module",
                               g_strdup(note->module), g_free);
        g_object_set_data_full(G_OBJECT(row), "note-key",
                               g_strdup(note->key), g_free);
        g_object_set_data_full(G_OBJECT(row), "note-display",
                               g_strdup(note->display_key), g_free);
        gtk_list_box_append(page->notes_list, row);
        g_free(text);
    }
    for (i = 0; i < tags->len; i++) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(g_ptr_array_index(tags, i));

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 24);
        if (g_strcmp0(g_ptr_array_index(tags, i), page->tag_filter) == 0)
            gtk_widget_add_css_class(label, "accent");
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "tag-name",
                               g_strdup(g_ptr_array_index(tags, i)), g_free);
        gtk_list_box_append(page->tags_list, row);
    }
    update_tag_actions(page);
    for (i = 0; i < lists->len; i++) {
        GswordVerseList *list = g_ptr_array_index(lists, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(list->name);
        gchar *id = g_strdup_printf("%" G_GINT64_FORMAT, list->id);

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 24);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "list-id", id, g_free);
        g_object_set_data_full(G_OBJECT(row), "list-name",
                               g_strdup(list->name), g_free);
        gtk_list_box_append(page->lists_list, row);
    }

    g_ptr_array_unref(notes);
    g_ptr_array_unref(tags);
    g_ptr_array_unref(lists);
    if (page->insight_page != NULL)
        gsword_insight_page_refresh(page->insight_page);
}

static void
update_scripture_context(GswordNotesPage *page)
{
    GswordWindow *window = page->window;
    GswManager *manager;
    GswModule *module = NULL;
    GError *error = NULL;
    gchar *reference;
    gchar *text = NULL;

    if (page->context_box == NULL)
        return;
    if (page->module == NULL || page->storage_key == NULL) {
        gtk_widget_set_visible(page->context_box, FALSE);
        return;
    }

    reference = g_strdup_printf("%s · %s", page->module,
                                page->display_key != NULL
                                ? page->display_key : page->storage_key);
    gtk_label_set_text(page->context_title, reference);
    g_free(reference);

    manager = gsword_window_get_manager(window);
    if (manager != NULL)
        module = gsw_manager_create_module(manager, page->module);
    if (module != NULL &&
        gsw_module_set_key_text(module, page->storage_key, &error)) {
        text = gsw_module_get_text(module, &error);
    }
    if (error != NULL) {
        g_clear_error(&error);
        g_clear_object(&module);
    }
    if (text == NULL || *text == '\0') {
        g_free(text);
        text = g_strdup(_("Verse text unavailable"));
    }
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(page->context_text),
                             text, -1);
    g_free(text);
    g_clear_object(&module);
    gtk_widget_set_visible(page->context_box, TRUE);
}

static void
on_open_context(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;

    (void)button;
    if (page->module != NULL && page->storage_key != NULL)
        gsword_window_open_module_reference(page->window, page->module,
                                             page->storage_key);
}

static void
set_save_state(GswordNotesPage *page, const gchar *text)
{
    if (page->save_state != NULL)
        gtk_label_set_text(page->save_state, text != NULL ? text : "");
}

static void
cancel_autosave(GswordNotesPage *page)
{
    if (page->autosave_source != 0) {
        g_source_remove(page->autosave_source);
        page->autosave_source = 0;
    }
}

static gboolean
autosave_note(gpointer data)
{
    GswordNotesPage *page = data;

    page->autosave_source = 0;
    save_note_page(page, TRUE);
    return G_SOURCE_REMOVE;
}

static void
update_note_actions(GswordNotesPage *page)
{
    gboolean has_key = page->storage_key != NULL;

    if (page->save_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->save_button), has_key);
    if (page->delete_note_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->delete_note_button),
                                 has_key && page->note_id != 0);
    if (page->add_to_list_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->add_to_list_button),
                                 has_key);
    if (page->tag_add_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->tag_add_button), has_key);
}

static void
select_note_row(GswordNotesPage *page)
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

static gboolean
save_note_page(GswordNotesPage *page, gboolean refresh)
{
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(page->text);
    GtkTextIter start;
    GtkTextIter end;
    gchar *text;
    const gchar *tags;
    GswordNote *saved;
    GError *error = NULL;

    if (notes_store == NULL || page->module == NULL ||
        page->storage_key == NULL)
        return FALSE;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    tags = gtk_editable_get_text(GTK_EDITABLE(page->tags_entry));
    if (!gsword_notes_store_save_note(
            notes_store, page->module, page->storage_key, page->display_key,
            text, tags, &error)) {
        gsword_window_show_error(page->window, _("Cannot save note"), error);
        g_free(text);
        return FALSE;
    }
    g_free(text);
    cancel_autosave(page);
    page->dirty = FALSE;
    saved = gsword_notes_store_get_note(
        notes_store, page->module, page->storage_key);
    if (saved != NULL) {
        page->note_id = saved->id;
        gsword_note_free(saved);
    }
    update_note_actions(page);
    set_save_state(page, _("Saved"));
    if (refresh) {
        gsword_notes_page_refresh(page);
        if (page->notes_list != NULL) {
            g_signal_handlers_block_by_func(
                page->notes_list, G_CALLBACK(on_note_selected), page);
            select_note_row(page);
            g_signal_handlers_unblock_by_func(
                page->notes_list, G_CALLBACK(on_note_selected), page);
        }
    }
    return TRUE;
}

static void
mark_note_dirty(GswordNotesPage *page)
{
    if (page->loading || page->storage_key == NULL)
        return;
    page->dirty = TRUE;
    set_save_state(page, _("Unsaved changes"));
    cancel_autosave(page);
    page->autosave_source = g_timeout_add(1000, autosave_note, page);
}

static void
load_note(GswordNotesPage *page, const gchar *module, const gchar *key,
          const gchar *display_key)
{
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GswordNote *note = NULL;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(page->text);

    if (page->dirty)
        save_note_page(page, FALSE);
    cancel_autosave(page);
    page->loading = TRUE;
    g_free(page->module);
    g_free(page->storage_key);
    g_free(page->display_key);
    page->module = g_strdup(module);
    page->storage_key = g_strdup(key);
    page->display_key = g_strdup(display_key);
    page->note_id = 0;

    if (notes_store != NULL)
        note = gsword_notes_store_get_note(notes_store, module, key);
    if (note != NULL) {
        page->note_id = note->id;
        gtk_text_buffer_set_text(buffer, note->text, -1);
        gtk_editable_set_text(GTK_EDITABLE(page->tags_entry), note->tags);
    } else {
        gtk_text_buffer_set_text(buffer, "", -1);
        gtk_editable_set_text(GTK_EDITABLE(page->tags_entry), "");
    }
    gtk_label_set_text(page->key_label,
                       display_key != NULL ? display_key : "");
    page->loading = FALSE;
    page->dirty = FALSE;
    update_scripture_context(page);
    update_note_actions(page);
    set_save_state(page, note != NULL ? _("Saved") : _("Not saved"));
    gsword_note_free(note);
}

gboolean
gsword_notes_page_has_note_key(GswordNotesPage *page)
{
    g_return_val_if_fail(page != NULL, FALSE);
    return page->storage_key != NULL;
}

void
gsword_notes_page_show_insights(GswordNotesPage *page)
{
    g_return_if_fail(page != NULL);
    if (page->notebook != NULL)
        gtk_notebook_set_current_page(page->notebook, 1);
}

void
gsword_notes_page_open_reference(GswordNotesPage *page,
                                 const gchar *module,
                                 const gchar *storage_key,
                                 const gchar *display_key)
{
    if (page == NULL || module == NULL || storage_key == NULL)
        return;
    load_note(page, module, storage_key,
              display_key != NULL ? display_key : storage_key);
    if (page->left_stack != NULL)
        gtk_stack_set_visible_child_name(page->left_stack, "notes");
    if (page->notebook != NULL)
        gtk_notebook_set_current_page(page->notebook, 0);
    gsword_window_show_page(page->window, "notes");
    gtk_widget_grab_focus(GTK_WIDGET(page->text));
}

void
gsword_notes_page_open_current(GswordNotesPage *page)
{
    GswModule *module;
    gchar *storage_key;
    gchar *display_key;

    g_return_if_fail(page != NULL);
    module = gsword_window_get_module(page->window);
    if (module == NULL)
        return;
    storage_key = gsword_window_current_storage_key(page->window);
    display_key = gsword_window_current_display_key(page->window);
    if (storage_key == NULL || display_key == NULL) {
        g_free(storage_key);
        g_free(display_key);
        return;
    }
    load_note(page, gsw_module_get_name(module), storage_key, display_key);
    g_free(storage_key);
    g_free(display_key);
    if (page->left_stack != NULL)
        gtk_stack_set_visible_child_name(page->left_stack, "notes");
    if (page->notebook != NULL)
        gtk_notebook_set_current_page(page->notebook, 0);
    gsword_window_show_page(page->window, "notes");
    gtk_widget_grab_focus(GTK_WIDGET(page->text));
}

static void
on_search_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    gsword_notes_page_refresh(user_data);
}

static void
on_editor_changed(GtkTextBuffer *buffer, gpointer user_data)
{
    (void)buffer;
    mark_note_dirty(user_data);
}

static void
on_tags_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    mark_note_dirty(user_data);
}

static void
on_note_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    const gchar *module;
    const gchar *key;
    const gchar *display;

    (void)list;
    if (row == NULL)
        return;
    module = g_object_get_data(G_OBJECT(row), "note-module");
    key = g_object_get_data(G_OBJECT(row), "note-key");
    display = g_object_get_data(G_OBJECT(row), "note-display");
    if (module != NULL && key != NULL)
        load_note(page, module, key, display);
}

static void
on_note_save(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;

    (void)button;
    if (save_note_page(page, TRUE))
        gsword_window_set_status(page->window, _("Note saved"));
}

static void
delete_note_now(GswordNotesPage *page)
{
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GError *error = NULL;

    if (notes_store == NULL || page->note_id == 0)
        return;
    if (!gsword_notes_store_delete_note(notes_store, page->note_id, &error)) {
        gsword_window_show_error(page->window, _("Cannot delete note"), error);
        return;
    }
    cancel_autosave(page);
    page->note_id = 0;
    page->loading = TRUE;
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(page->text), "", -1);
    gtk_editable_set_text(GTK_EDITABLE(page->tags_entry), "");
    page->loading = FALSE;
    page->dirty = FALSE;
    update_note_actions(page);
    set_save_state(page, _("Not saved"));
    gsword_notes_page_refresh(page);
    gsword_window_set_status(page->window, _("Note deleted"));
}

static void
on_delete_note_response(AdwAlertDialog *dialog, const gchar *response,
                        gpointer user_data)
{
    (void)dialog;
    if (g_strcmp0(response, "delete") == 0)
        delete_note_now(user_data);
}

static void
on_note_delete(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    AdwDialog *dialog;

    (void)button;
    if (page->note_id == 0)
        return;
    dialog = adw_alert_dialog_new(_("Delete note"), NULL);
    adw_alert_dialog_format_body(ADW_ALERT_DIALOG(dialog),
        _("Delete the note for %s? This cannot be undone."),
        page->display_key != NULL ? page->display_key : "");
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "cancel", _("Cancel"),
                                   "delete", _("Delete"), NULL);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    g_signal_connect(dialog, "response",
                     G_CALLBACK(on_delete_note_response), page);
    adw_dialog_present(dialog, GTK_WIDGET(page->window));
}

static void
on_export_save(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result,
                                              &error);
    const gchar *content = g_object_get_data(source, "export-content");

    (void)user_data;
    if (file == NULL) {
        g_clear_error(&error);
        return;
    }
    if (content != NULL)
        g_file_replace_contents(file, content, strlen(content), NULL, FALSE,
                                G_FILE_CREATE_REPLACE_DESTINATION, NULL, NULL,
                                &error);
    g_clear_error(&error);
    g_object_unref(file);
}

static void
on_note_export(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(page->text);
    GtkTextIter start;
    GtkTextIter end;
    gchar *text;
    gchar *content;
    GtkFileDialog *dialog;

    (void)button;
    if (page->display_key == NULL ||
        gsword_window_get_notes_store(page->window) == NULL)
        return;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    content = g_strdup_printf("# %s\n\n%s\n\nTags: %s\n",
                              page->display_key, text,
                              gtk_editable_get_text(
                                  GTK_EDITABLE(page->tags_entry)));
    g_free(text);

    dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, _("Export note"));
    gtk_file_dialog_set_initial_name(dialog, "gsword-note.md");
    {
        gchar *exports_dir = gsword_app_exports_dir();
        GFile *folder;

        g_mkdir_with_parents(exports_dir, 0755);
        folder = g_file_new_for_path(exports_dir);
        gtk_file_dialog_set_initial_folder(dialog, folder);
        g_object_unref(folder);
        g_free(exports_dir);
    }
    g_object_set_data_full(G_OBJECT(dialog), "export-content", content, g_free);
    gtk_file_dialog_save(dialog, GTK_WINDOW(page->window), NULL,
                         on_export_save, page);
    g_object_unref(dialog);
}

static void
on_tag_name_clicked(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    const gchar *tag = g_object_get_data(G_OBJECT(button), "tag-name");
    const gchar *current;
    gchar **parts;
    gboolean found = FALSE;
    gchar *combined;
    guint i;

    if (tag == NULL || *tag == '\0')
        return;
    current = gtk_editable_get_text(GTK_EDITABLE(page->tags_entry));
    parts = g_strsplit(current != NULL ? current : "", ",", -1);
    for (i = 0; parts[i] != NULL; i++) {
        gchar *part = g_strstrip(parts[i]);

        if (g_strcmp0(part, tag) == 0) {
            found = TRUE;
            break;
        }
    }
    g_strfreev(parts);
    if (!found) {
        if (current == NULL || *current == '\0')
            combined = g_strdup(tag);
        else
            combined = g_strdup_printf("%s, %s", current, tag);
        gtk_editable_set_text(GTK_EDITABLE(page->tags_entry), combined);
        g_free(combined);
    }
    gtk_popover_popdown(gtk_menu_button_get_popover(page->tag_add_button));
}

static void
rebuild_tag_menu(GswordNotesPage *page)
{
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GPtrArray *tags;
    GtkWidget *child;
    guint i;

    if (page->tag_menu_box == NULL)
        return;
    while ((child = gtk_widget_get_first_child(
                GTK_WIDGET(page->tag_menu_box))) != NULL)
        gtk_box_remove(page->tag_menu_box, child);
    if (notes_store == NULL)
        return;
    tags = gsword_notes_store_list_tags(notes_store);
    if (tags->len == 0) {
        GtkWidget *label = make_placeholder(_("No tags yet"));

        gtk_widget_set_margin_top(label, 6);
        gtk_widget_set_margin_bottom(label, 6);
        gtk_box_append(page->tag_menu_box, label);
    }
    for (i = 0; i < tags->len; i++) {
        const gchar *tag = g_ptr_array_index(tags, i);
        GtkWidget *button = gtk_button_new_with_label(tag);

        gtk_widget_add_css_class(button, "flat");
        gtk_widget_set_halign(button, GTK_ALIGN_START);
        g_object_set_data_full(G_OBJECT(button), "tag-name",
                               g_strdup(tag), g_free);
        g_signal_connect(button, "clicked",
                         G_CALLBACK(on_tag_name_clicked), page);
        gtk_box_append(page->tag_menu_box, button);
    }
    g_ptr_array_unref(tags);
}

static void
on_tag_menu_active(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordNotesPage *page = user_data;

    (void)pspec;
    if (gtk_menu_button_get_active(GTK_MENU_BUTTON(object)))
        rebuild_tag_menu(page);
}

static void
update_tag_actions(GswordNotesPage *page)
{
    gboolean active = page->tag_filter != NULL && *page->tag_filter != '\0';

    if (page->rename_tag_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->rename_tag_button), active);
    if (page->delete_tag_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->delete_tag_button), active);
    if (page->show_tagged_notes_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->show_tagged_notes_button),
                                 active);
}

static void
on_show_tagged_notes(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GtkListBoxRow *first;

    (void)button;
    if (page->tag_filter == NULL || page->left_stack == NULL)
        return;
    gtk_stack_set_visible_child_name(page->left_stack, "notes");
    first = gtk_list_box_get_row_at_index(page->notes_list, 0);
    if (first != NULL)
        gtk_list_box_select_row(page->notes_list, first);
}

static void
on_tag_activated(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    const gchar *tag;

    (void)list;
    if (row == NULL)
        return;
    tag = g_object_get_data(G_OBJECT(row), "tag-name");
    if (tag == NULL)
        return;
    if (g_strcmp0(page->tag_filter, tag) == 0) {
        g_clear_pointer(&page->tag_filter, g_free);
    } else {
        g_free(page->tag_filter);
        page->tag_filter = g_strdup(tag);
    }
    gsword_notes_page_refresh(page);
}

static void
focus_note_editor(GswordNotesPage *page)
{
    if (page->text != NULL)
        gtk_widget_grab_focus(GTK_WIDGET(page->text));
}

static void
wrap_selection(GswordNotesPage *page, const gchar *prefix,
               const gchar *suffix)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(page->text);
    GtkTextIter start;
    GtkTextIter end;
    gboolean has_selection = gtk_text_buffer_get_selection_bounds(
        buffer, &start, &end);

    gtk_text_buffer_begin_user_action(buffer);
    if (has_selection) {
        gchar *selected = gtk_text_buffer_get_text(buffer, &start, &end,
                                                   FALSE);
        gchar *replacement = g_strdup_printf("%s%s%s", prefix, selected,
                                             suffix);

        gtk_text_buffer_delete(buffer, &start, &end);
        gtk_text_buffer_insert(buffer, &start, replacement, -1);
        g_free(replacement);
        g_free(selected);
    } else {
        GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
        GtkTextIter iter;
        gchar *replacement = g_strdup_printf("%s%s", prefix, suffix);

        gtk_text_buffer_insert_at_cursor(buffer, replacement, -1);
        gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
        gtk_text_iter_backward_chars(
            &iter, (gint)g_utf8_strlen(suffix, -1));
        gtk_text_buffer_place_cursor(buffer, &iter);
        g_free(replacement);
    }
    gtk_text_buffer_end_user_action(buffer);
    focus_note_editor(page);
}

static void
prefix_selected_lines(GswordNotesPage *page, const gchar *prefix)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(page->text);
    GtkTextIter start;
    GtkTextIter end;
    gboolean has_selection = gtk_text_buffer_get_selection_bounds(
        buffer, &start, &end);

    gtk_text_buffer_begin_user_action(buffer);
    if (has_selection) {
        gint start_line = gtk_text_iter_get_line(&start);
        gint end_line = gtk_text_iter_get_line(&end);
        gint line;

        for (line = end_line; line >= start_line; line--) {
            GtkTextIter iter;
            gtk_text_buffer_get_iter_at_line(buffer, &iter, line);
            gtk_text_buffer_insert(buffer, &iter, prefix, -1);
        }
    } else {
        GtkTextIter iter;
        gtk_text_buffer_get_iter_at_mark(
            buffer, &iter, gtk_text_buffer_get_insert(buffer));
        gtk_text_iter_set_line_offset(&iter, 0);
        gtk_text_buffer_insert(buffer, &iter, prefix, -1);
    }
    gtk_text_buffer_end_user_action(buffer);
    focus_note_editor(page);
}

static void
on_markdown_bold(GtkButton *button, gpointer user_data)
{
    (void)button;
    wrap_selection(user_data, "**", "**");
}

static void
on_markdown_italic(GtkButton *button, gpointer user_data)
{
    (void)button;
    wrap_selection(user_data, "*", "*");
}

static void
on_markdown_heading(GtkButton *button, gpointer user_data)
{
    (void)button;
    prefix_selected_lines(user_data, "# ");
}

static void
on_markdown_bullet(GtkButton *button, gpointer user_data)
{
    (void)button;
    prefix_selected_lines(user_data, "- ");
}

static void
on_markdown_numbered(GtkButton *button, gpointer user_data)
{
    (void)button;
    prefix_selected_lines(user_data, "1. ");
}

static void
on_markdown_quote(GtkButton *button, gpointer user_data)
{
    (void)button;
    prefix_selected_lines(user_data, "> ");
}

static void
on_markdown_link(GtkButton *button, gpointer user_data)
{
    (void)button;
    wrap_selection(user_data, "[", "](url)");
}

static void
on_markdown_code(GtkButton *button, gpointer user_data)
{
    (void)button;
    wrap_selection(user_data, "`", "`");
}

static GtkWidget *
make_markdown_button(const gchar *label, const gchar *tooltip,
                     GCallback callback, GswordNotesPage *page)
{
    GtkWidget *button = gtk_button_new_with_label(label);

    gtk_widget_add_css_class(button, "flat");
    gtk_widget_set_tooltip_text(button, tooltip);
    g_signal_connect(button, "clicked", callback, page);
    return button;
}

static void
on_list_item_selected(GtkListBox *list, GtkListBoxRow *row,
                      gpointer user_data)
{
    GswordNotesPage *page = user_data;
    const gchar *module;
    const gchar *key;

    (void)list;
    clear_selected_item(page);
    if (row == NULL)
        return;
    module = g_object_get_data(G_OBJECT(row), "item-module");
    key = g_object_get_data(G_OBJECT(row), "item-key");
    if (module == NULL || key == NULL)
        return;
    page->selected_item_module = g_strdup(module);
    page->selected_item_key = g_strdup(key);
    update_list_item_actions(page);
}

static void
open_selected_list_item(GswordNotesPage *page)
{
    if (page->selected_item_module != NULL && page->selected_item_key != NULL)
        gsword_window_open_module_reference(page->window,
                                            page->selected_item_module,
                                            page->selected_item_key);
}

static void
on_list_item_activated(GtkListBox *list, GtkListBoxRow *row,
                       gpointer user_data)
{
    (void)list;
    if (row != NULL)
        on_list_item_selected(list, row, user_data);
    open_selected_list_item(user_data);
}

static void
on_open_list_item(GtkButton *button, gpointer user_data)
{
    (void)button;
    open_selected_list_item(user_data);
}

static void
on_remove_list_item(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GError *error = NULL;

    (void)button;
    if (notes_store == NULL || page->selected_list_id == 0 ||
        page->selected_item_module == NULL || page->selected_item_key == NULL)
        return;
    if (!gsword_notes_store_remove_from_list(
            notes_store, page->selected_list_id, page->selected_item_module,
            page->selected_item_key, &error)) {
        gsword_window_show_error(page->window,
                                 _("Cannot remove verse from list"), error);
        return;
    }
    load_list_items(page);
    gsword_window_set_status(page->window, _("Verse removed from list"));
}

static void
update_list_item_actions(GswordNotesPage *page)
{
    gboolean selected = page->selected_item_module != NULL &&
                        page->selected_item_key != NULL;

    if (page->open_list_item_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->open_list_item_button),
                                 selected);
    if (page->remove_list_item_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->remove_list_item_button),
                                 selected);
}

static void
clear_selected_item(GswordNotesPage *page)
{
    g_clear_pointer(&page->selected_item_module, g_free);
    g_clear_pointer(&page->selected_item_key, g_free);
    update_list_item_actions(page);
}

static void
load_list_items(GswordNotesPage *page)
{
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);

    clear_list(page->list_items);
    clear_selected_item(page);
    if (page->selected_list_id == 0) {
        gtk_list_box_set_placeholder(page->list_items,
            make_placeholder(_("Select a verse list")));
        return;
    }
    if (notes_store == NULL)
        return;
    {
        GPtrArray *items = gsword_notes_store_list_items(
            notes_store, page->selected_list_id);
        guint i;

        if (items->len == 0)
            gtk_list_box_set_placeholder(page->list_items,
                make_placeholder(_("This list is empty")));
        for (i = 0; i < items->len; i++) {
            GswordVerseListItem *item = g_ptr_array_index(items, i);
            GtkWidget *item_row = gtk_list_box_row_new();
            GtkWidget *label = gtk_label_new(item->display_key);

            gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
            gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
            gtk_label_set_max_width_chars(GTK_LABEL(label), 24);
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(item_row), label);
            g_object_set_data_full(G_OBJECT(item_row), "item-module",
                                   g_strdup(item->module), g_free);
            g_object_set_data_full(G_OBJECT(item_row), "item-key",
                                   g_strdup(item->key), g_free);
            g_object_set_data_full(G_OBJECT(item_row), "item-display",
                                   g_strdup(item->display_key), g_free);
            gtk_list_box_append(page->list_items, item_row);
        }
        g_ptr_array_unref(items);
    }
}

static void
on_list_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    const gchar *id;

    (void)list;
    if (row == NULL) {
        page->selected_list_id = 0;
        if (page->rename_list_button != NULL)
            gtk_widget_set_sensitive(GTK_WIDGET(page->rename_list_button),
                                     FALSE);
        if (page->delete_list_button != NULL)
            gtk_widget_set_sensitive(GTK_WIDGET(page->delete_list_button),
                                     FALSE);
        load_list_items(page);
        return;
    }
    id = g_object_get_data(G_OBJECT(row), "list-id");
    page->selected_list_id = id != NULL ? g_ascii_strtoll(id, NULL, 10) : 0;
    if (page->rename_list_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->rename_list_button),
                                 page->selected_list_id != 0);
    if (page->delete_list_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->delete_list_button),
                                 page->selected_list_id != 0);
    load_list_items(page);
}

static void
on_list_membership_toggled(GtkCheckButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    const gchar *id_text;
    gint64 list_id;
    GError *error = NULL;
    gboolean active = gtk_check_button_get_active(button);

    if (page->updating_list_menu || notes_store == NULL ||
        page->module == NULL || page->storage_key == NULL)
        return;
    id_text = g_object_get_data(G_OBJECT(button), "list-id");
    if (id_text == NULL)
        return;
    list_id = g_ascii_strtoll(id_text, NULL, 10);
    if (active) {
        if (!gsword_notes_store_add_to_list(
                notes_store, list_id, page->module, page->storage_key,
                page->display_key, &error)) {
            gsword_window_show_error(page->window,
                                     _("Cannot add verse to list"), error);
            return;
        }
    } else {
        if (!gsword_notes_store_remove_from_list(
                notes_store, list_id, page->module, page->storage_key,
                &error)) {
            gsword_window_show_error(page->window,
                                     _("Cannot remove verse from list"),
                                     error);
            return;
        }
    }
    if (page->selected_list_id == list_id)
        load_list_items(page);
    gsword_window_set_status(page->window, active
                             ? _("Verse added to list")
                             : _("Verse removed from list"));
}

static void
rebuild_list_menu(GswordNotesPage *page)
{
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GPtrArray *lists;
    GtkWidget *child;
    guint i;

    if (page->list_menu_box == NULL)
        return;
    page->updating_list_menu = TRUE;
    while ((child = gtk_widget_get_first_child(
                GTK_WIDGET(page->list_menu_box))) != NULL)
        gtk_box_remove(page->list_menu_box, child);
    if (page->storage_key == NULL) {
        GtkWidget *label = make_placeholder(_("Select a note first"));

        gtk_box_append(page->list_menu_box, label);
        page->updating_list_menu = FALSE;
        return;
    }
    if (notes_store == NULL) {
        page->updating_list_menu = FALSE;
        return;
    }
    lists = gsword_notes_store_list_lists(notes_store);
    if (lists->len == 0) {
        GtkWidget *label = make_placeholder(_("No verse lists yet"));

        gtk_box_append(page->list_menu_box, label);
    }
    for (i = 0; i < lists->len; i++) {
        GswordVerseList *list = g_ptr_array_index(lists, i);
        GtkWidget *check = gtk_check_button_new_with_label(list->name);
        gchar *id = g_strdup_printf("%" G_GINT64_FORMAT, list->id);

        g_object_set_data_full(G_OBJECT(check), "list-id", id, g_free);
        gtk_check_button_set_active(
            GTK_CHECK_BUTTON(check),
            gsword_notes_store_is_in_list(notes_store, list->id,
                                          page->module, page->storage_key));
        g_signal_connect(check, "toggled",
                         G_CALLBACK(on_list_membership_toggled), page);
        gtk_box_append(page->list_menu_box, check);
    }
    g_ptr_array_unref(lists);
    page->updating_list_menu = FALSE;
}

static void
on_list_menu_active(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordNotesPage *page = user_data;

    (void)pspec;
    if (gtk_menu_button_get_active(GTK_MENU_BUTTON(object)))
        rebuild_list_menu(page);
}

static void
on_rename_entry_changed(GtkEditable *editable, gpointer user_data)
{
    AdwAlertDialog *dialog = user_data;
    gchar *text = g_strdup(gtk_editable_get_text(editable));

    g_strstrip(text);
    adw_alert_dialog_set_response_enabled(dialog, "rename", *text != '\0');
    g_free(text);
}

static void
update_tags_entry_after_rename(GswordNotesPage *page, const gchar *old_name,
                               const gchar *new_name)
{
    const gchar *current;
    gchar **parts;
    GString *result;
    guint i;

    if (page->tags_entry == NULL)
        return;
    current = gtk_editable_get_text(GTK_EDITABLE(page->tags_entry));
    parts = g_strsplit(current, ",", -1);
    result = g_string_new(NULL);
    for (i = 0; parts[i] != NULL; i++) {
        gchar *tag = g_strstrip(parts[i]);

        if (*tag == '\0')
            continue;
        if (g_strcmp0(tag, old_name) == 0)
            tag = (gchar *)new_name;
        if (result->len > 0)
            g_string_append(result, ", ");
        g_string_append(result, tag);
    }
    gtk_editable_set_text(GTK_EDITABLE(page->tags_entry), result->str);
    g_strfreev(parts);
    g_string_free(result, TRUE);
}

static void
on_rename_tag_response(AdwAlertDialog *dialog, const gchar *response,
                       gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GtkEntry *entry = g_object_get_data(G_OBJECT(dialog), "rename-entry");
    const gchar *old_name = g_object_get_data(G_OBJECT(dialog), "old-name");
    GError *error = NULL;
    gchar *new_name;

    if (g_strcmp0(response, "rename") != 0 || notes_store == NULL ||
        entry == NULL || old_name == NULL)
        return;
    new_name = g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry)));
    g_strstrip(new_name);
    if (*new_name == '\0' || g_strcmp0(new_name, old_name) == 0) {
        g_free(new_name);
        return;
    }
    if (!gsword_notes_store_rename_tag(notes_store, old_name, new_name,
                                       &error)) {
        gsword_window_show_error(page->window, _("Cannot rename tag"), error);
        g_free(new_name);
        return;
    }
    if (g_strcmp0(page->tag_filter, old_name) == 0) {
        g_free(page->tag_filter);
        page->tag_filter = g_strdup(new_name);
    }
    update_tags_entry_after_rename(page, old_name, new_name);
    g_free(new_name);
    gsword_notes_page_refresh(page);
    gsword_window_set_status(page->window, _("Tag renamed"));
}

static void
on_rename_tag(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GtkListBoxRow *row;
    const gchar *name;
    GtkWidget *entry;
    AdwDialog *dialog;

    (void)button;
    row = gtk_list_box_get_selected_row(page->tags_list);
    name = row != NULL ? g_object_get_data(G_OBJECT(row), "tag-name")
                       : page->tag_filter;
    if (name == NULL)
        return;
    dialog = adw_alert_dialog_new(_("Rename tag"), NULL);
    entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(entry), name);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    gtk_widget_set_hexpand(entry, TRUE);
    adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(dialog), entry);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "cancel", _("Cancel"),
                                   "rename", _("Rename"), NULL);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "rename");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    g_object_set_data(G_OBJECT(dialog), "rename-entry", entry);
    g_object_set_data_full(G_OBJECT(dialog), "old-name", g_strdup(name), g_free);
    g_signal_connect(entry, "changed", G_CALLBACK(on_rename_entry_changed),
                     dialog);
    g_signal_connect(dialog, "response",
                     G_CALLBACK(on_rename_tag_response), page);
    adw_dialog_present(dialog, GTK_WIDGET(page->window));
}

static void
on_rename_list_response(AdwAlertDialog *dialog, const gchar *response,
                        gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GtkEntry *entry = g_object_get_data(G_OBJECT(dialog), "rename-entry");
    const gchar *id_text = g_object_get_data(G_OBJECT(dialog), "list-id");
    GError *error = NULL;
    gchar *new_name;

    if (g_strcmp0(response, "rename") != 0 || notes_store == NULL ||
        entry == NULL || id_text == NULL)
        return;
    new_name = g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry)));
    g_strstrip(new_name);
    if (*new_name == '\0') {
        g_free(new_name);
        return;
    }
    if (!gsword_notes_store_rename_list(notes_store,
                                        g_ascii_strtoll(id_text, NULL, 10),
                                        new_name, &error)) {
        gsword_window_show_error(page->window,
                                 _("Cannot rename verse list"), error);
        g_free(new_name);
        return;
    }
    g_free(new_name);
    gsword_notes_page_refresh(page);
    gsword_window_set_status(page->window, _("Verse list renamed"));
}

static void
on_rename_list(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GtkListBoxRow *row;
    const gchar *name;
    const gchar *id;
    GtkWidget *entry;
    AdwDialog *dialog;

    (void)button;
    row = gtk_list_box_get_selected_row(page->lists_list);
    name = row != NULL ? g_object_get_data(G_OBJECT(row), "list-name") : NULL;
    id = row != NULL ? g_object_get_data(G_OBJECT(row), "list-id") : NULL;
    if (name == NULL || id == NULL)
        return;
    dialog = adw_alert_dialog_new(_("Rename verse list"), NULL);
    entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(entry), name);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    gtk_widget_set_hexpand(entry, TRUE);
    adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(dialog), entry);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "cancel", _("Cancel"),
                                   "rename", _("Rename"), NULL);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "rename");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    g_object_set_data(G_OBJECT(dialog), "rename-entry", entry);
    g_object_set_data_full(G_OBJECT(dialog), "list-id", g_strdup(id), g_free);
    g_signal_connect(entry, "changed", G_CALLBACK(on_rename_entry_changed),
                     dialog);
    g_signal_connect(dialog, "response",
                     G_CALLBACK(on_rename_list_response), page);
    adw_dialog_present(dialog, GTK_WIDGET(page->window));
}

static void
on_delete_tag_response(AdwAlertDialog *dialog, const gchar *response,
                       gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    const gchar *name = g_object_get_data(G_OBJECT(dialog), "tag-name");
    GError *error = NULL;

    if (g_strcmp0(response, "delete") != 0 || notes_store == NULL ||
        name == NULL)
        return;
    if (!gsword_notes_store_delete_tag(notes_store, name, &error)) {
        gsword_window_show_error(page->window, _("Cannot delete tag"), error);
        return;
    }
    if (g_strcmp0(page->tag_filter, name) == 0)
        g_clear_pointer(&page->tag_filter, g_free);
    gsword_notes_page_refresh(page);
    gsword_window_set_status(page->window, _("Tag deleted"));
}

static void
on_delete_tag(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GtkListBoxRow *row;
    const gchar *name;
    AdwDialog *dialog;

    (void)button;
    row = gtk_list_box_get_selected_row(page->tags_list);
    name = row != NULL ? g_object_get_data(G_OBJECT(row), "tag-name")
                       : page->tag_filter;
    if (name == NULL)
        return;
    dialog = adw_alert_dialog_new(_("Delete tag"), NULL);
    adw_alert_dialog_format_body(ADW_ALERT_DIALOG(dialog),
                                 _("Delete tag “%s”? Notes will remain."),
                                 name);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "cancel", _("Cancel"),
                                   "delete", _("Delete"), NULL);
    adw_alert_dialog_set_response_appearance(
        ADW_ALERT_DIALOG(dialog), "delete", ADW_RESPONSE_DESTRUCTIVE);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    g_object_set_data_full(G_OBJECT(dialog), "tag-name", g_strdup(name), g_free);
    g_signal_connect(dialog, "response",
                     G_CALLBACK(on_delete_tag_response), page);
    adw_dialog_present(dialog, GTK_WIDGET(page->window));
}

static void
on_create_list(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GError *error = NULL;
    const gchar *name = gtk_editable_get_text(
        GTK_EDITABLE(page->new_list_entry));

    (void)button;
    if (notes_store == NULL || name == NULL || *name == '\0')
        return;
    if (!gsword_notes_store_create_list(notes_store, name, &error)) {
        gsword_window_show_error(page->window, _("Cannot create list"), error);
        return;
    }
    gtk_editable_set_text(GTK_EDITABLE(page->new_list_entry), "");
    gsword_notes_page_refresh(page);
    gsword_window_set_status(page->window, _("Verse list created"));
}

static void
on_delete_list_response(AdwAlertDialog *dialog, const gchar *response,
                        gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GError *error = NULL;

    (void)dialog;
    if (g_strcmp0(response, "delete") != 0 || notes_store == NULL ||
        page->selected_list_id == 0)
        return;
    if (!gsword_notes_store_delete_list(notes_store, page->selected_list_id,
                                        &error)) {
        gsword_window_show_error(page->window,
                                 _("Cannot delete verse list"), error);
        return;
    }
    page->selected_list_id = 0;
    clear_list(page->list_items);
    if (page->rename_list_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->rename_list_button), FALSE);
    if (page->delete_list_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->delete_list_button), FALSE);
    gsword_notes_page_refresh(page);
    gsword_window_set_status(page->window, _("Verse list deleted"));
}

static void
on_delete_list(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GtkListBoxRow *row;
    const gchar *name;
    AdwDialog *dialog;

    (void)button;
    if (page->selected_list_id == 0)
        return;
    row = gtk_list_box_get_selected_row(page->lists_list);
    name = row != NULL ? g_object_get_data(G_OBJECT(row), "list-name") : NULL;
    dialog = adw_alert_dialog_new(_("Delete verse list"), NULL);
    adw_alert_dialog_format_body(ADW_ALERT_DIALOG(dialog),
                                 _("Delete “%s” and all its verses?"),
                                 name != NULL ? name : "");
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "cancel", _("Cancel"),
                                   "delete", _("Delete"), NULL);
    adw_alert_dialog_set_response_appearance(
        ADW_ALERT_DIALOG(dialog), "delete", ADW_RESPONSE_DESTRUCTIVE);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    g_signal_connect(dialog, "response",
                     G_CALLBACK(on_delete_list_response), page);
    adw_dialog_present(dialog, GTK_WIDGET(page->window));
}

static void
notes_page_free(gpointer data)
{
    GswordNotesPage *page = data;

    if (page == NULL)
        return;
    cancel_autosave(page);
    g_free(page->module);
    g_free(page->storage_key);
    g_free(page->display_key);
    g_free(page->selected_item_module);
    g_free(page->selected_item_key);
    g_free(page->tag_filter);
    g_free(page);
}

GswordNotesPage *
gsword_notes_page_new(GswordWindow *window)
{
    GswordNotesPage *page = g_new0(GswordNotesPage, 1);
    GtkWidget *paned;
    GtkWidget *left;
    GtkWidget *right;
    GtkWidget *notes_scroll;
    GtkWidget *tags_scroll;
    GtkWidget *lists_scroll;
    GtkWidget *list_items_scroll;
    GtkWidget *new_list_row;
    GtkWidget *new_list_button;
    GtkWidget *delete_list;
    GtkWidget *notes_page;
    GtkWidget *tags_page;
    GtkWidget *lists_page;
    GtkWidget *lists_paned;
    GtkWidget *list_items_box;
    GtkWidget *stack_switcher;
    GtkWidget *filter_clear;
    GtkWidget *tags_label;
    GtkWidget *tags_header;
    GtkWidget *show_tagged_notes;
    GtkWidget *rename_tag;
    GtkWidget *delete_tag;
    GtkWidget *lists_header;
    GtkWidget *rename_list;
    GtkWidget *lists_label;
    GtkWidget *list_items_header;
    GtkWidget *open_list_item;
    GtkWidget *remove_list_item;
    GtkWidget *editor_scroll;
    GtkWidget *context_scroll;
    GtkWidget *context_header;
    GtkWidget *context_open;
    GtkWidget *markdown_hint;
    GtkWidget *format_bar;
    GtkWidget *buttons;
    GtkWidget *save;
    GtkWidget *delete;
    GtkWidget *export;
    GtkMenuButton *add_to_list;
    GtkMenuButton *tag_add;
    GtkWidget *editor_header;
    GtkWidget *tags_row;

    page->window = window;
    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    notes_scroll = gtk_scrolled_window_new();
    tags_scroll = gtk_scrolled_window_new();
    lists_scroll = gtk_scrolled_window_new();
    list_items_scroll = gtk_scrolled_window_new();
    new_list_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    new_list_button = gtk_button_new_with_label(_("Add list"));
    delete_list = gtk_button_new_from_icon_name("edit-delete-symbolic");
    page->delete_list_button = GTK_BUTTON(delete_list);
    tags_label = gtk_label_new(_("Tags"));
    tags_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    show_tagged_notes = gtk_button_new_from_icon_name("go-next-symbolic");
    rename_tag = gtk_button_new_from_icon_name("document-edit-symbolic");
    page->rename_tag_button = GTK_BUTTON(rename_tag);
    page->show_tagged_notes_button = GTK_BUTTON(show_tagged_notes);
    delete_tag = gtk_button_new_from_icon_name("edit-delete-symbolic");
    page->delete_tag_button = GTK_BUTTON(delete_tag);
    lists_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    rename_list = gtk_button_new_from_icon_name("document-edit-symbolic");
    page->rename_list_button = GTK_BUTTON(rename_list);
    lists_label = gtk_label_new(_("Verse lists"));
    page->list_items_label = GTK_LABEL(gtk_label_new(_("List verses")));
    open_list_item = gtk_button_new_from_icon_name("document-open-symbolic");
    remove_list_item = gtk_button_new_from_icon_name("edit-delete-symbolic");
    page->open_list_item_button = GTK_BUTTON(open_list_item);
    page->remove_list_item_button = GTK_BUTTON(remove_list_item);
    editor_scroll = gtk_scrolled_window_new();
    buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    save = gtk_button_new_with_label(_("Save"));
    delete = gtk_button_new_with_label(_("Delete"));
    export = gtk_button_new_with_label(_("Export"));
    add_to_list = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_menu_button_set_label(add_to_list, _("Add to list"));
    tag_add = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_menu_button_set_icon_name(tag_add, "list-add-symbolic");
    gtk_widget_set_tooltip_text(GTK_WIDGET(tag_add), _("Add tag"));
    tags_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    page->context_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    context_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    context_open = gtk_button_new_with_label(_("Open in reader"));

    gtk_widget_set_margin_top(left, 8);
    gtk_widget_set_margin_bottom(left, 8);
    gtk_widget_set_margin_start(left, 10);
    gtk_widget_set_margin_end(left, 10);
    gtk_widget_set_margin_top(right, 8);
    gtk_widget_set_margin_bottom(right, 8);
    gtk_widget_set_margin_start(right, 10);
    gtk_widget_set_margin_end(right, 10);
    gtk_widget_set_size_request(left, 260, -1);

    page->search = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->search, _("Search notes"));
    g_signal_connect(page->search, "changed",
                     G_CALLBACK(on_search_changed), page);

    page->notes_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(page->notes_list, "row-selected",
                     G_CALLBACK(on_note_selected), page);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(notes_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(notes_scroll),
                                  GTK_WIDGET(page->notes_list));
    gtk_widget_set_vexpand(notes_scroll, TRUE);
    gtk_list_box_set_placeholder(page->notes_list,
                                 make_placeholder(_("No notes yet")));

    page->tags_list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(page->tags_list, GTK_SELECTION_NONE);
    gtk_list_box_set_activate_on_single_click(page->tags_list, TRUE);
    g_signal_connect(page->tags_list, "row-activated",
                     G_CALLBACK(on_tag_activated), page);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tags_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tags_scroll),
                                  GTK_WIDGET(page->tags_list));
    gtk_widget_set_vexpand(tags_scroll, TRUE);
    gtk_list_box_set_placeholder(page->tags_list,
                                 make_placeholder(_("No tags yet")));

    page->lists_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(page->lists_list, "row-selected",
                     G_CALLBACK(on_list_selected), page);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(lists_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(lists_scroll),
                                  GTK_WIDGET(page->lists_list));
    gtk_widget_set_size_request(lists_scroll, -1, 100);
    gtk_list_box_set_placeholder(page->lists_list,
                                 make_placeholder(_("No verse lists yet")));

    page->list_items = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_activate_on_single_click(page->list_items, FALSE);
    g_signal_connect(page->list_items, "row-selected",
                     G_CALLBACK(on_list_item_selected), page);
    g_signal_connect(page->list_items, "row-activated",
                     G_CALLBACK(on_list_item_activated), page);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(list_items_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(list_items_scroll),
                                  GTK_WIDGET(page->list_items));
    gtk_widget_set_vexpand(list_items_scroll, TRUE);
    gtk_list_box_set_placeholder(page->list_items,
                                 make_placeholder(_("Select a verse list")));

    page->new_list_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->new_list_entry, _("New list"));
    gtk_widget_set_hexpand(GTK_WIDGET(page->new_list_entry), TRUE);
    g_signal_connect(new_list_button, "clicked",
                     G_CALLBACK(on_create_list), page);
    gtk_widget_add_css_class(delete_list, "flat");
    gtk_widget_add_css_class(delete_list, "destructive-action");
    gtk_widget_set_tooltip_text(delete_list, _("Delete verse list"));
    gtk_widget_set_sensitive(delete_list, FALSE);
    g_signal_connect(delete_list, "clicked",
                     G_CALLBACK(on_delete_list), page);
    gtk_box_append(GTK_BOX(new_list_row), GTK_WIDGET(page->new_list_entry));
    gtk_box_append(GTK_BOX(new_list_row), new_list_button);

    gtk_label_set_xalign(GTK_LABEL(tags_label), 0.0f);
    gtk_widget_set_hexpand(tags_label, TRUE);
    gtk_widget_add_css_class(rename_tag, "flat");
    gtk_widget_set_tooltip_text(rename_tag, _("Rename tag"));
    gtk_widget_set_sensitive(rename_tag, FALSE);
    g_signal_connect(rename_tag, "clicked", G_CALLBACK(on_rename_tag), page);
    gtk_widget_add_css_class(delete_tag, "flat");
    gtk_widget_add_css_class(delete_tag, "destructive-action");
    gtk_widget_set_tooltip_text(delete_tag, _("Delete tag"));
    gtk_widget_set_sensitive(delete_tag, FALSE);
    g_signal_connect(delete_tag, "clicked", G_CALLBACK(on_delete_tag), page);
    gtk_widget_add_css_class(show_tagged_notes, "flat");
    gtk_widget_set_tooltip_text(show_tagged_notes, _("Show filtered notes"));
    gtk_widget_set_sensitive(show_tagged_notes, FALSE);
    g_signal_connect(show_tagged_notes, "clicked",
                     G_CALLBACK(on_show_tagged_notes), page);
    gtk_box_append(GTK_BOX(tags_header), tags_label);
    gtk_box_append(GTK_BOX(tags_header), show_tagged_notes);
    gtk_box_append(GTK_BOX(tags_header), rename_tag);
    gtk_box_append(GTK_BOX(tags_header), delete_tag);
    gtk_label_set_xalign(GTK_LABEL(lists_label), 0.0f);
    gtk_widget_set_hexpand(lists_label, TRUE);
    gtk_widget_add_css_class(rename_list, "flat");
    gtk_widget_set_tooltip_text(rename_list, _("Rename verse list"));
    gtk_widget_set_sensitive(rename_list, FALSE);
    g_signal_connect(rename_list, "clicked", G_CALLBACK(on_rename_list), page);
    gtk_box_append(GTK_BOX(lists_header), lists_label);
    gtk_box_append(GTK_BOX(lists_header), rename_list);
    gtk_box_append(GTK_BOX(lists_header), delete_list);
    gtk_label_set_xalign(page->list_items_label, 0.0f);
    gtk_widget_set_hexpand(GTK_WIDGET(page->list_items_label), TRUE);
    gtk_widget_add_css_class(tags_label, "heading");
    gtk_widget_add_css_class(lists_label, "heading");
    gtk_widget_add_css_class(GTK_WIDGET(page->list_items_label), "heading");
    gtk_widget_add_css_class(open_list_item, "flat");
    gtk_widget_add_css_class(remove_list_item, "flat");
    gtk_widget_add_css_class(remove_list_item, "destructive-action");
    gtk_widget_set_tooltip_text(open_list_item, _("Open verse"));
    gtk_widget_set_tooltip_text(remove_list_item, _("Remove from list"));
    gtk_widget_set_sensitive(open_list_item, FALSE);
    gtk_widget_set_sensitive(remove_list_item, FALSE);
    g_signal_connect(open_list_item, "clicked",
                     G_CALLBACK(on_open_list_item), page);
    g_signal_connect(remove_list_item, "clicked",
                     G_CALLBACK(on_remove_list_item), page);

    page->tag_filter_label = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->tag_filter_label, 0.0f);
    gtk_widget_set_hexpand(GTK_WIDGET(page->tag_filter_label), TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(page->tag_filter_label), "caption");
    gtk_widget_add_css_class(GTK_WIDGET(page->tag_filter_label), "accent");
    filter_clear = gtk_button_new_with_label(_("All notes"));
    gtk_widget_add_css_class(filter_clear, "flat");
    gtk_widget_add_css_class(filter_clear, "pill");
    gtk_widget_set_tooltip_text(filter_clear, _("Clear tag filter"));
    g_signal_connect(filter_clear, "clicked",
                     G_CALLBACK(on_clear_tag_filter), page);
    page->tag_filter_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(page->tag_filter_bar, "card");
    gtk_widget_set_margin_top(page->tag_filter_bar, 2);
    gtk_widget_set_margin_bottom(page->tag_filter_bar, 2);
    gtk_box_append(GTK_BOX(page->tag_filter_bar),
                   GTK_WIDGET(page->tag_filter_label));
    gtk_box_append(GTK_BOX(page->tag_filter_bar), filter_clear);
    gtk_widget_set_visible(page->tag_filter_bar, FALSE);
    page->notes_count_label = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->notes_count_label, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->notes_count_label), "caption");
    gtk_widget_add_css_class(GTK_WIDGET(page->notes_count_label), "dim-label");

    notes_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(notes_page), GTK_WIDGET(page->search));
    gtk_box_append(GTK_BOX(notes_page), GTK_WIDGET(page->notes_count_label));
    gtk_box_append(GTK_BOX(notes_page), page->tag_filter_bar);
    gtk_box_append(GTK_BOX(notes_page), notes_scroll);

    tags_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(tags_page), tags_header);
    gtk_box_append(GTK_BOX(tags_page), tags_scroll);

    lists_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(lists_page), lists_header);
    lists_paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_start_child(GTK_PANED(lists_paned), lists_scroll);
    list_items_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    list_items_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(list_items_header),
                   GTK_WIDGET(page->list_items_label));
    gtk_box_append(GTK_BOX(list_items_header), open_list_item);
    gtk_box_append(GTK_BOX(list_items_header), remove_list_item);
    gtk_box_append(GTK_BOX(list_items_box), list_items_header);
    gtk_box_append(GTK_BOX(list_items_box), list_items_scroll);
    gtk_box_append(GTK_BOX(list_items_box), new_list_row);
    gtk_paned_set_end_child(GTK_PANED(lists_paned), list_items_box);
    gtk_paned_set_position(GTK_PANED(lists_paned), 170);
    gtk_widget_set_vexpand(lists_paned, TRUE);
    gtk_box_append(GTK_BOX(lists_page), lists_paned);

    stack_switcher = gtk_stack_switcher_new();
    page->left_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_set_hhomogeneous(page->left_stack, FALSE);
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(stack_switcher),
                                 page->left_stack);
    gtk_widget_set_halign(stack_switcher, GTK_ALIGN_CENTER);
    gtk_stack_add_titled(page->left_stack, notes_page, "notes", _("Notes"));
    gtk_stack_add_titled(page->left_stack, tags_page, "tags", _("Tags"));
    gtk_stack_add_titled(page->left_stack, lists_page, "lists",
                         _("Verse lists"));
    {
        GtkStackPage *lists_stack_page = gtk_stack_get_page(
            page->left_stack, lists_page);

        if (lists_stack_page != NULL)
            gtk_stack_page_set_visible(lists_stack_page, FALSE);
    }
    gtk_widget_set_vexpand(GTK_WIDGET(page->left_stack), TRUE);

    gtk_box_append(GTK_BOX(left), stack_switcher);
    gtk_box_append(GTK_BOX(left), GTK_WIDGET(page->left_stack));

    page->key_label = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->key_label, 0.0f);
    gtk_label_set_selectable(page->key_label, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(page->key_label), "heading");
    gtk_widget_set_hexpand(GTK_WIDGET(page->key_label), TRUE);
    page->save_state = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->save_state, 1.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->save_state), "dim-label");
    gtk_widget_add_css_class(GTK_WIDGET(page->save_state), "caption");
    editor_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(editor_header), GTK_WIDGET(page->key_label));
    gtk_box_append(GTK_BOX(editor_header), GTK_WIDGET(page->save_state));
    page->tags_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->tags_entry,
                                   _("Tags, separated by commas"));
    gtk_widget_set_hexpand(GTK_WIDGET(page->tags_entry), TRUE);
    gtk_box_append(GTK_BOX(tags_row), gtk_label_new(_("Tags:")));
    gtk_box_append(GTK_BOX(tags_row), GTK_WIDGET(page->tags_entry));
    gtk_box_append(GTK_BOX(tags_row), GTK_WIDGET(tag_add));
    page->tag_add_button = tag_add;
    page->tag_menu_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
    {
        GtkWidget *popover = gtk_popover_new();

        gtk_popover_set_child(GTK_POPOVER(popover),
                              GTK_WIDGET(page->tag_menu_box));
        gtk_menu_button_set_popover(tag_add, popover);
        g_signal_connect(tag_add, "notify::active",
                         G_CALLBACK(on_tag_menu_active), page);
    }
    page->list_menu_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
    {
        GtkWidget *popover = gtk_popover_new();

        gtk_popover_set_child(GTK_POPOVER(popover),
                              GTK_WIDGET(page->list_menu_box));
        gtk_menu_button_set_popover(add_to_list, popover);
        g_signal_connect(add_to_list, "notify::active",
                         G_CALLBACK(on_list_menu_active), page);
    }
    gtk_widget_set_sensitive(GTK_WIDGET(add_to_list), FALSE);

    page->context_title = GTK_LABEL(gtk_label_new(_("Scripture")));
    gtk_label_set_xalign(page->context_title, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->context_title), "heading");
    gtk_widget_set_hexpand(GTK_WIDGET(page->context_title), TRUE);
    gtk_box_append(GTK_BOX(context_header), GTK_WIDGET(page->context_title));
    gtk_widget_add_css_class(context_open, "flat");
    gtk_widget_set_tooltip_text(context_open, _("Open in reader"));
    g_signal_connect(context_open, "clicked",
                     G_CALLBACK(on_open_context), page);
    gtk_box_append(GTK_BOX(context_header), context_open);
    page->context_text = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(page->context_text, FALSE);
    gtk_text_view_set_cursor_visible(page->context_text, FALSE);
    gtk_text_view_set_wrap_mode(page->context_text, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(page->context_text, 8);
    gtk_text_view_set_right_margin(page->context_text, 8);
    gtk_text_view_set_top_margin(page->context_text, 6);
    gtk_text_view_set_bottom_margin(page->context_text, 6);
    context_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(context_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(context_scroll),
                                  GTK_WIDGET(page->context_text));
    gtk_widget_set_size_request(context_scroll, -1, 110);
    gtk_widget_set_vexpand(context_scroll, FALSE);
    gtk_box_append(GTK_BOX(page->context_box), context_header);
    gtk_box_append(GTK_BOX(page->context_box), context_scroll);
    gtk_widget_set_visible(page->context_box, FALSE);

    markdown_hint = gtk_label_new(
        _("Markdown: **bold**, *italic*, # heading, - list, > quote"));
    gtk_label_set_xalign(GTK_LABEL(markdown_hint), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(markdown_hint), TRUE);
    gtk_widget_add_css_class(markdown_hint, "dim-label");
    gtk_widget_add_css_class(markdown_hint, "caption");
    format_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(format_bar),
                   make_markdown_button("B", _("Bold"),
                                        G_CALLBACK(on_markdown_bold), page));
    gtk_box_append(GTK_BOX(format_bar),
                   make_markdown_button("I", _("Italic"),
                                        G_CALLBACK(on_markdown_italic), page));
    gtk_box_append(GTK_BOX(format_bar),
                   make_markdown_button("H", _("Heading"),
                                        G_CALLBACK(on_markdown_heading), page));
    gtk_box_append(GTK_BOX(format_bar),
                   make_markdown_button("•", _("Bullet list"),
                                        G_CALLBACK(on_markdown_bullet), page));
    gtk_box_append(GTK_BOX(format_bar),
                   make_markdown_button("1.", _("Numbered list"),
                                        G_CALLBACK(on_markdown_numbered), page));
    gtk_box_append(GTK_BOX(format_bar),
                   make_markdown_button(">", _("Quote"),
                                        G_CALLBACK(on_markdown_quote), page));
    gtk_box_append(GTK_BOX(format_bar),
                   make_markdown_button("[]", _("Link"),
                                        G_CALLBACK(on_markdown_link), page));
    gtk_box_append(GTK_BOX(format_bar),
                   make_markdown_button("`", _("Inline code"),
                                        G_CALLBACK(on_markdown_code), page));

#ifdef HAVE_GTKSOURCEVIEW
    {
        GtkSourceLanguageManager *language_manager =
            gtk_source_language_manager_get_default();
        GtkSourceLanguage *language = gtk_source_language_manager_get_language(
            language_manager, "markdown");
        GtkSourceBuffer *buffer = language != NULL
            ? gtk_source_buffer_new_with_language(language)
            : gtk_source_buffer_new(NULL);

        if (language != NULL)
            gtk_source_buffer_set_highlight_syntax(buffer, TRUE);
        page->text = GTK_TEXT_VIEW(gtk_source_view_new_with_buffer(buffer));
        g_object_unref(buffer);
        gtk_source_view_set_show_line_numbers(
            GTK_SOURCE_VIEW(page->text), FALSE);
        gtk_source_view_set_auto_indent(GTK_SOURCE_VIEW(page->text), TRUE);
        gtk_source_view_set_insert_spaces_instead_of_tabs(
            GTK_SOURCE_VIEW(page->text), TRUE);
        gtk_source_view_set_tab_width(GTK_SOURCE_VIEW(page->text), 4);
    }
#else
    page->text = GTK_TEXT_VIEW(gtk_text_view_new());
#endif
    gtk_text_view_set_wrap_mode(page->text, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(page->text, 12);
    gtk_text_view_set_right_margin(page->text, 12);
    gtk_text_view_set_top_margin(page->text, 12);
    gtk_text_view_set_bottom_margin(page->text, 12);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(editor_scroll),
                                  GTK_WIDGET(page->text));
    gtk_widget_set_vexpand(editor_scroll, TRUE);

    page->save_button = GTK_BUTTON(save);
    page->delete_note_button = GTK_BUTTON(delete);
    page->add_to_list_button = add_to_list;
    g_signal_connect(save, "clicked", G_CALLBACK(on_note_save), page);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_note_delete), page);
    g_signal_connect(export, "clicked", G_CALLBACK(on_note_export), page);
    g_signal_connect(gtk_text_view_get_buffer(page->text), "changed",
                     G_CALLBACK(on_editor_changed), page);
    g_signal_connect(page->tags_entry, "changed",
                     G_CALLBACK(on_tags_changed), page);
    gtk_box_append(GTK_BOX(buttons), save);
    gtk_box_append(GTK_BOX(buttons), delete);
    gtk_box_append(GTK_BOX(buttons), GTK_WIDGET(add_to_list));
    gtk_box_append(GTK_BOX(buttons), export);

    gtk_box_append(GTK_BOX(right), editor_header);
    gtk_box_append(GTK_BOX(right), page->context_box);
    gtk_box_append(GTK_BOX(right), tags_row);
    gtk_box_append(GTK_BOX(right), markdown_hint);
    gtk_box_append(GTK_BOX(right), format_bar);
    gtk_box_append(GTK_BOX(right), editor_scroll);
    gtk_box_append(GTK_BOX(right), buttons);

    gtk_paned_set_start_child(GTK_PANED(paned), left);
    gtk_paned_set_end_child(GTK_PANED(paned), right);
    gtk_paned_set_position(GTK_PANED(paned), 280);
    {
        GtkEventController *keys = gtk_event_controller_key_new();

        gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
        g_signal_connect(keys, "key-pressed",
                         G_CALLBACK(on_notes_key_pressed), page);
        gtk_widget_add_controller(paned, keys);
    }
    page->notebook = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_notebook_append_page(page->notebook, paned,
                             gtk_label_new(_("Notes")));
    page->insight_page = gsword_insight_page_new(window);
    gtk_notebook_append_page(
        page->notebook, gsword_insight_page_widget(page->insight_page),
        gtk_label_new(_("Insights")));
    gtk_notebook_set_scrollable(page->notebook, TRUE);
    page->root = GTK_WIDGET(page->notebook);
    g_object_set_data_full(G_OBJECT(page->notebook), "gsword-notes-page",
                           page, notes_page_free);
    update_note_actions(page);
    gsword_notes_page_refresh(page);
    return page;
}

GtkWidget *
gsword_notes_page_widget(GswordNotesPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}
