/*
 * notes-page.c: notes, tags and verse lists for the gsword window
 */

#include "notes-page.h"
#include "window-private.h"

#include <glib/gi18n.h>

#ifdef HAVE_GTKSOURCEVIEW
#include <gtksourceview/gtksource.h>
#endif

#include <string.h>

struct _GswordNotesPage {
    GswordWindow *window;
    GtkWidget *root;
    GtkEntry *search;
    GtkLabel *key_label;
    GtkWidget *context_box;
    GtkLabel *context_title;
    GtkTextView *context_text;
    GtkTextView *text;
    GtkEntry *tags_entry;
    GtkListBox *notes_list;
    GtkListBox *tags_list;
    GtkListBox *lists_list;
    GtkListBox *list_items;
    GtkEntry *new_list_entry;
    GtkButton *delete_list_button;
    gchar *module;
    gchar *storage_key;
    gchar *display_key;
    gint64 note_id;
    gint64 selected_list_id;
};

static void
clear_list(GtkListBox *list)
{
    while (gtk_widget_get_first_child(GTK_WIDGET(list)) != NULL)
        gtk_list_box_remove(list,
                            gtk_widget_get_first_child(GTK_WIDGET(list)));
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
    notes = gsword_notes_store_list_notes(notes_store, query);
    tags = gsword_notes_store_list_tags(notes_store);
    lists = gsword_notes_store_list_lists(notes_store);

    clear_list(page->notes_list);
    clear_list(page->tags_list);
    clear_list(page->lists_list);

    for (i = 0; i < notes->len; i++) {
        GswordNote *note = g_ptr_array_index(notes, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label;
        gchar *text = g_strdup_printf("%s\n%s",
            note->display_key, note->text != NULL ? note->text : "");

        label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 32);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
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
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "tag-name",
                               g_strdup(g_ptr_array_index(tags, i)), g_free);
        gtk_list_box_append(page->tags_list, row);
    }
    for (i = 0; i < lists->len; i++) {
        GswordVerseList *list = g_ptr_array_index(lists, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(list->name);
        gchar *id = g_strdup_printf("%" G_GINT64_FORMAT, list->id);

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "list-id", id, g_free);
        g_object_set_data_full(G_OBJECT(row), "list-name",
                               g_strdup(list->name), g_free);
        gtk_list_box_append(page->lists_list, row);
    }

    g_ptr_array_unref(notes);
    g_ptr_array_unref(tags);
    g_ptr_array_unref(lists);
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
load_note(GswordNotesPage *page, const gchar *module, const gchar *key,
          const gchar *display_key)
{
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GswordNote *note = NULL;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(page->text);

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
    update_scripture_context(page);
    gsword_note_free(note);
}

gboolean
gsword_notes_page_has_note_key(GswordNotesPage *page)
{
    g_return_val_if_fail(page != NULL, FALSE);
    return page->storage_key != NULL;
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
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(page->text);
    GtkTextIter start;
    GtkTextIter end;
    gchar *text;
    const gchar *tags;
    GswordNote *saved;
    GError *error = NULL;

    (void)button;
    if (notes_store == NULL || page->storage_key == NULL)
        return;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    tags = gtk_editable_get_text(GTK_EDITABLE(page->tags_entry));
    if (!gsword_notes_store_save_note(
            notes_store, page->module, page->storage_key, page->display_key,
            text, tags, &error)) {
        gsword_window_show_error(page->window, _("Cannot save note"), error);
        g_free(text);
        return;
    }
    g_free(text);
    saved = gsword_notes_store_get_note(
        notes_store, page->module, page->storage_key);
    if (saved != NULL) {
        page->note_id = saved->id;
        gsword_note_free(saved);
    }
    gsword_notes_page_refresh(page);
    gsword_window_set_status(page->window, _("Note saved"));
}

static void
on_note_delete(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GError *error = NULL;

    (void)button;
    if (notes_store == NULL || page->note_id == 0)
        return;
    if (!gsword_notes_store_delete_note(notes_store, page->note_id, &error)) {
        gsword_window_show_error(page->window, _("Cannot delete note"), error);
        return;
    }
    page->note_id = 0;
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(page->text), "", -1);
    gtk_editable_set_text(GTK_EDITABLE(page->tags_entry), "");
    gsword_notes_page_refresh(page);
    gsword_window_set_status(page->window, _("Note deleted"));
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
    g_object_set_data_full(G_OBJECT(dialog), "export-content", content, g_free);
    gtk_file_dialog_save(dialog, GTK_WINDOW(page->window), NULL,
                         on_export_save, page);
    g_object_unref(dialog);
}

static void
on_tag_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    const gchar *tag;
    const gchar *current;

    (void)list;
    if (row == NULL)
        return;
    tag = g_object_get_data(G_OBJECT(row), "tag-name");
    if (tag == NULL)
        return;
    current = gtk_editable_get_text(GTK_EDITABLE(page->tags_entry));
    if (current == NULL || *current == '\0') {
        gtk_editable_set_text(GTK_EDITABLE(page->tags_entry), tag);
    } else if (strstr(current, tag) == NULL) {
        gchar *combined = g_strdup_printf("%s, %s", current, tag);

        gtk_editable_set_text(GTK_EDITABLE(page->tags_entry), combined);
        g_free(combined);
    }
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
    if (row == NULL)
        return;
    module = g_object_get_data(G_OBJECT(row), "item-module");
    key = g_object_get_data(G_OBJECT(row), "item-display");
    if (module == NULL || key == NULL)
        return;
    gsword_window_open_module_reference(page->window, module, key);
}

static void
on_list_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    const gchar *id;

    (void)list;
    if (row == NULL) {
        page->selected_list_id = 0;
        if (page->delete_list_button != NULL)
            gtk_widget_set_sensitive(GTK_WIDGET(page->delete_list_button),
                                     FALSE);
        clear_list(page->list_items);
        return;
    }
    id = g_object_get_data(G_OBJECT(row), "list-id");
    page->selected_list_id = id != NULL ? g_ascii_strtoll(id, NULL, 10) : 0;
    if (page->delete_list_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->delete_list_button),
                                 page->selected_list_id != 0);
    clear_list(page->list_items);
    if (notes_store != NULL && page->selected_list_id != 0) {
        GPtrArray *items = gsword_notes_store_list_items(
            notes_store, page->selected_list_id);
        guint i;

        for (i = 0; i < items->len; i++) {
            GswordVerseListItem *item = g_ptr_array_index(items, i);
            GtkWidget *item_row = gtk_list_box_row_new();
            GtkWidget *label = gtk_label_new(item->display_key);

            gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(item_row), label);
            g_object_set_data_full(G_OBJECT(item_row), "item-module",
                                   g_strdup(item->module), g_free);
            g_object_set_data_full(G_OBJECT(item_row), "item-display",
                                   g_strdup(item->display_key), g_free);
            gtk_list_box_append(page->list_items, item_row);
        }
        g_ptr_array_unref(items);
    }
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
on_add_to_list(GtkButton *button, gpointer user_data)
{
    GswordNotesPage *page = user_data;
    GswordNotesStore *notes_store = gsword_window_get_notes_store(page->window);
    GError *error = NULL;

    (void)button;
    if (notes_store == NULL || page->selected_list_id == 0 ||
        page->storage_key == NULL) {
        gsword_window_set_status(page->window,
                                 _("Select a verse list first"));
        return;
    }
    if (!gsword_notes_store_add_to_list(
            notes_store, page->selected_list_id, page->module,
            page->storage_key, page->display_key, &error)) {
        gsword_window_show_error(page->window,
                                 _("Cannot add verse to list"), error);
        return;
    }
    gsword_notes_page_refresh(page);
    gsword_window_set_status(page->window, _("Verse added to list"));
}

static void
notes_page_free(gpointer data)
{
    GswordNotesPage *page = data;

    if (page == NULL)
        return;
    g_free(page->module);
    g_free(page->storage_key);
    g_free(page->display_key);
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
    GtkWidget *notes_label;
    GtkWidget *tags_label;
    GtkWidget *lists_label;
    GtkWidget *list_items_label;
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
    GtkWidget *add_to_list;
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
    notes_label = gtk_label_new(_("Notes"));
    tags_label = gtk_label_new(_("Tags"));
    lists_label = gtk_label_new(_("Verse lists"));
    list_items_label = gtk_label_new(_("List verses"));
    editor_scroll = gtk_scrolled_window_new();
    buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    save = gtk_button_new_with_label(_("Save"));
    delete = gtk_button_new_with_label(_("Delete"));
    export = gtk_button_new_with_label(_("Export"));
    add_to_list = gtk_button_new_with_label(_("Add to list"));
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
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(notes_scroll),
                                  GTK_WIDGET(page->notes_list));
    gtk_widget_set_vexpand(notes_scroll, TRUE);

    page->tags_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(page->tags_list, "row-selected",
                     G_CALLBACK(on_tag_selected), page);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tags_scroll),
                                  GTK_WIDGET(page->tags_list));
    gtk_widget_set_size_request(tags_scroll, -1, 120);

    page->lists_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(page->lists_list, "row-selected",
                     G_CALLBACK(on_list_selected), page);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(lists_scroll),
                                  GTK_WIDGET(page->lists_list));
    gtk_widget_set_size_request(lists_scroll, -1, 90);

    page->list_items = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(page->list_items, "row-selected",
                     G_CALLBACK(on_list_item_selected), page);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(list_items_scroll),
                                  GTK_WIDGET(page->list_items));
    gtk_widget_set_size_request(list_items_scroll, -1, 100);
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
    gtk_box_append(GTK_BOX(new_list_row), delete_list);

    gtk_label_set_xalign(GTK_LABEL(notes_label), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(tags_label), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(lists_label), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(list_items_label), 0.0f);
    gtk_widget_add_css_class(notes_label, "heading");
    gtk_widget_add_css_class(tags_label, "heading");
    gtk_widget_add_css_class(lists_label, "heading");
    gtk_widget_add_css_class(list_items_label, "heading");
    gtk_box_append(GTK_BOX(left), GTK_WIDGET(page->search));
    gtk_box_append(GTK_BOX(left), notes_label);
    gtk_box_append(GTK_BOX(left), notes_scroll);
    gtk_box_append(GTK_BOX(left), tags_label);
    gtk_box_append(GTK_BOX(left), tags_scroll);
    gtk_box_append(GTK_BOX(left), lists_label);
    gtk_box_append(GTK_BOX(left), lists_scroll);
    gtk_box_append(GTK_BOX(left), list_items_label);
    gtk_box_append(GTK_BOX(left), list_items_scroll);
    gtk_box_append(GTK_BOX(left), new_list_row);

    page->key_label = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->key_label, 0.0f);
    gtk_label_set_selectable(page->key_label, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(page->key_label), "heading");
    page->tags_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->tags_entry,
                                   _("Tags, separated by commas"));
    gtk_widget_set_hexpand(GTK_WIDGET(page->tags_entry), TRUE);
    gtk_box_append(GTK_BOX(tags_row), gtk_label_new(_("Tags:")));
    gtk_box_append(GTK_BOX(tags_row), GTK_WIDGET(page->tags_entry));

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

    g_signal_connect(save, "clicked", G_CALLBACK(on_note_save), page);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_note_delete), page);
    g_signal_connect(export, "clicked", G_CALLBACK(on_note_export), page);
    g_signal_connect(add_to_list, "clicked", G_CALLBACK(on_add_to_list), page);
    gtk_box_append(GTK_BOX(buttons), save);
    gtk_box_append(GTK_BOX(buttons), delete);
    gtk_box_append(GTK_BOX(buttons), add_to_list);
    gtk_box_append(GTK_BOX(buttons), export);

    gtk_box_append(GTK_BOX(right), GTK_WIDGET(page->key_label));
    gtk_box_append(GTK_BOX(right), page->context_box);
    gtk_box_append(GTK_BOX(right), tags_row);
    gtk_box_append(GTK_BOX(right), markdown_hint);
    gtk_box_append(GTK_BOX(right), format_bar);
    gtk_box_append(GTK_BOX(right), editor_scroll);
    gtk_box_append(GTK_BOX(right), buttons);

    gtk_paned_set_start_child(GTK_PANED(paned), left);
    gtk_paned_set_end_child(GTK_PANED(paned), right);
    gtk_paned_set_position(GTK_PANED(paned), 280);
    page->root = paned;
    g_object_set_data_full(G_OBJECT(paned), "gsword-notes-page", page,
                           notes_page_free);
    gsword_notes_page_refresh(page);
    return page;
}

GtkWidget *
gsword_notes_page_widget(GswordNotesPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}
