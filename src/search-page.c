/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * search-page.c: search page for the gsword window
 */

#include "search-page.h"
#include "app-paths.h"
#include "window-private.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

struct _GswordSearchPage {
    GswordWindow *window;
    GtkWidget *root;
    GtkSearchEntry *entry;
    GtkDropDown *module_selector;
    GtkStringList *module_names;
    GtkDropDown *scope_selector;
    GtkDropDown *type;
    GtkCheckButton *case_sensitive;
    GtkEntry *attribute1;
    GtkEntry *attribute2;
    GtkMenuButton *history_button;
    GtkListBox *history_list;
    GtkStringList *history;
    GtkListBox *results;
    GtkLabel *results_label;
    GtkLabel *empty_label;
    GtkStack *results_stack;
    GtkButton *search_button;
    GtkButton *cancel_button;
    GtkButton *build_button;
    GtkButton *delete_button;
    GtkLabel *index_label;
    guint results_count;
    GCancellable *search_cancellable;
    GTask *search_task;
    GtkPopover *context_menu;
    GtkPopover *list_menu;
    gchar *context_module;
    gchar *context_key;
    gchar *context_display;
    gchar *context_storage;
    gchar *context_text;
    gboolean context_is_bible;
    gint context_x;
    gint context_y;
};

typedef struct {
    GswordWindow *window;
    GswordSearchPage *page;
    GswModule *module;
    GswSearchOptions *options;
} SearchJob;

typedef struct {
    GswordWindow *window;
    gchar *message;
} SearchProgressData;

static void run_search(GswordSearchPage *page);
static void clear_context(GswordSearchPage *page);
static void ensure_result_popovers(GswordSearchPage *page);

static void
search_job_free(gpointer data)
{
    SearchJob *job = data;

    g_clear_object(&job->window);
    g_clear_object(&job->module);
    g_clear_pointer(&job->options, gsw_search_options_unref);
    g_free(job);
}

static gboolean
search_progress_update(gpointer data)
{
    SearchProgressData *progress = data;

    gsword_window_set_status(progress->window, progress->message);
    g_clear_object(&progress->window);
    g_free(progress->message);
    g_free(progress);
    return G_SOURCE_REMOVE;
}

static void
search_progress(const gchar *module_name, guint processed, guint total,
                gpointer user_data)
{
    SearchJob *job = user_data;
    SearchProgressData *progress;
    gchar *message;

    if (processed % 500 != 0 && (total == 0 || processed != total))
        return;
    if (total > 0)
        message = g_strdup_printf(_("Searching %s: %u/%u"),
                                  module_name, processed, total);
    else
        message = g_strdup_printf(_("Searching %s: %u"),
                                  module_name, processed);
    progress = g_new0(SearchProgressData, 1);
    progress->window = g_object_ref(job->window);
    progress->message = message;
    g_main_context_invoke(NULL, search_progress_update, progress);
}

static void
search_thread(GTask *task, gpointer source_object, gpointer task_data,
              GCancellable *cancellable)
{
    SearchJob *job = task_data;
    GError *error = NULL;
    GPtrArray *results;

    (void)source_object;
    results = gsw_module_search_full(job->module, job->options, cancellable,
                                     search_progress, job, &error);
    if (results == NULL)
        g_task_return_error(task, error);
    else
        g_task_return_pointer(task, results,
                              (GDestroyNotify)g_ptr_array_unref);
}

static const gchar *
selected_module_name(GswordSearchPage *page)
{
    guint selected;

    if (page->module_selector == NULL || page->module_names == NULL)
        return NULL;
    selected = gtk_drop_down_get_selected(page->module_selector);
    if (selected == GTK_INVALID_LIST_POSITION)
        return NULL;
    return gtk_string_list_get_string(page->module_names, selected);
}

static void
refresh_modules(GswordSearchPage *page, const gchar *preferred_module)
{
    GswManager *manager = gsword_window_get_manager(page->window);
    GswModule *current = gsword_window_get_module(page->window);
    GPtrArray *infos;
    GtkStringList *names;
    gchar *wanted = g_strdup(preferred_module != NULL
                             ? preferred_module
                             : selected_module_name(page));
    const gchar *current_name = current != NULL
                              ? gsw_module_get_name(current) : NULL;
    guint i;
    guint selected = GTK_INVALID_LIST_POSITION;

    if (manager == NULL || page->module_selector == NULL) {
        g_free(wanted);
        return;
    }
    if (wanted == NULL)
        wanted = g_strdup(current_name);
    infos = gsw_manager_get_module_infos(manager);
    names = gtk_string_list_new(NULL);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);
        const gchar *name = gsw_module_info_get_name(info);

        if (name != NULL && *name != '\0')
            gtk_string_list_append(names, name);
    }
    g_ptr_array_unref(infos);
    if (wanted == NULL && gtk_string_list_get_string(names, 0) != NULL)
        wanted = g_strdup(gtk_string_list_get_string(names, 0));
    gtk_drop_down_set_model(page->module_selector, NULL);
    g_clear_object(&page->module_names);
    page->module_names = names;
    gtk_drop_down_set_model(page->module_selector, G_LIST_MODEL(names));
    for (i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(names)); i++) {
        if (g_strcmp0(gtk_string_list_get_string(names, i), wanted) == 0) {
            selected = i;
            break;
        }
    }
    if (selected != GTK_INVALID_LIST_POSITION)
        gtk_drop_down_set_selected(page->module_selector, selected);
    g_free(wanted);
}

static void
configure_search_index(GswordWindow *window, GswModule *module,
                       const gchar *module_name)
{
    gchar *index_path = gsword_app_search_index_path(module_name);

    gsword_window_apply_book_name_locale(window, module);
    gsw_module_set_search_index_path(module, index_path);
    g_free(index_path);
}

static GswModule *
create_selected_module(GswordSearchPage *page)
{
    GswManager *manager = gsword_window_get_manager(page->window);
    GswModule *current = gsword_window_get_module(page->window);
    const gchar *name = selected_module_name(page);
    GswModule *module;

    if (manager == NULL)
        return NULL;
    if (name == NULL && current != NULL)
        name = gsw_module_get_name(current);
    if (name == NULL)
        return NULL;
    module = gsw_manager_create_module(manager, name);
    if (module != NULL)
        configure_search_index(page->window, module, name);
    return module;
}

static gboolean
search_can_use_index(GswordSearchPage *page)
{
    guint type = gtk_drop_down_get_selected(page->type);
    guint scope = gtk_drop_down_get_selected(page->scope_selector);

    const gchar *attribute1 = page->attribute1 != NULL
                            ? gtk_editable_get_text(GTK_EDITABLE(
                                  page->attribute1)) : "";
    const gchar *attribute2 = page->attribute2 != NULL
                            ? gtk_editable_get_text(GTK_EDITABLE(
                                  page->attribute2)) : "";

    return !gtk_check_button_get_active(page->case_sensitive) &&
           scope == 0 && *attribute1 == '\0' && *attribute2 == '\0' &&
           (type == 0 || type == 1);
}

static void
refresh_index_status(GswordSearchPage *page)
{
    GswModule *module = create_selected_module(page);
    gboolean built = module != NULL && gsw_module_has_search_index(module);

    if (module == NULL)
        gtk_label_set_text(page->index_label, _("Index: no module"));
    else if (!built)
        gtk_label_set_text(page->index_label,
                           _("Index: not built (scan used)"));
    else if (search_can_use_index(page))
        gtk_label_set_text(page->index_label,
                           _("Index: built (used for this search)"));
    else
        gtk_label_set_text(page->index_label,
                           _("Index: built (scan used for this search)"));
    if (page->build_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->build_button),
                                 module != NULL && !built);
    if (page->delete_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->delete_button), built);
    g_clear_object(&module);
}

static GswVerseList *
build_scope(GswordSearchPage *page, GError **error)
{
    GswModule *module = gsword_window_get_module(page->window);
    GswVerseKey *key;
    gint selected;

    if (page->scope_selector == NULL)
        return NULL;
    selected = gtk_drop_down_get_selected(page->scope_selector);
    if (selected <= 0)
        return NULL;
    if (module == NULL || gsw_key_get_kind(gsw_module_get_key(module)) !=
        GSW_KEY_KIND_VERSE) {
        g_set_error_literal(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                            "No active verse reference");
        return NULL;
    }
    key = (GswVerseKey *)gsw_module_get_key(module);
    if (selected == 1)
        return gsw_verse_list_parse("Gen 1:1-Mal 4:6", key, error);
    if (selected == 2)
        return gsw_verse_list_parse("Matt 1:1-Rev 22:21", key, error);
    {
        const gchar *book = gsw_verse_key_get_book_name(key);
        gint chapter = gsw_verse_key_get_chapter(key);
        gint chapter_max = gsw_verse_key_get_chapter_max(key);
        GswVerseKey *last = (GswVerseKey *)gsw_key_clone((GswKey *)key);
        gint last_verse;
        gchar *text;
        GswVerseList *scope;

        gsw_verse_key_set_chapter(last, chapter_max);
        last_verse = gsw_verse_key_get_verse_max(last);
        if (selected == 3) {
            text = g_strdup_printf("%s 1:1-%s %d:%d", book, book,
                                   chapter_max, last_verse);
        } else {
            text = g_strdup_printf("%s %d:1-%s %d:%d", book, chapter, book,
                                   chapter, gsw_verse_key_get_verse_max(key));
        }
        scope = gsw_verse_list_parse(text, key, error);
        g_free(text);
        gsw_key_unref((GswKey *)last);
        return scope;
    }
}

static void
on_history_clicked(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;
    const gchar *query = g_object_get_data(G_OBJECT(button), "history-query");
    GtkPopover *popover;

    if (query == NULL)
        return;
    gtk_editable_set_text(GTK_EDITABLE(page->entry), query);
    popover = gtk_menu_button_get_popover(page->history_button);
    if (popover != NULL)
        gtk_popover_popdown(popover);
    run_search(page);
}

static void
rebuild_history_list(GswordSearchPage *page)
{
    GtkWidget *child;
    guint i;

    if (page->history_list == NULL)
        return;
    while ((child = gtk_widget_get_first_child(
                GTK_WIDGET(page->history_list))) != NULL)
        gtk_list_box_remove(page->history_list, child);
    for (i = 0; page->history != NULL &&
                i < g_list_model_get_n_items(G_LIST_MODEL(page->history));
         i++) {
        const gchar *text = gtk_string_list_get_string(page->history, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *button = gtk_button_new_with_label(text);

        gtk_widget_add_css_class(button, "flat");
        gtk_widget_set_halign(button, GTK_ALIGN_START);
        g_object_set_data_full(G_OBJECT(button), "history-query",
                               g_strdup(text), g_free);
        g_signal_connect(button, "clicked",
                         G_CALLBACK(on_history_clicked), page);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), button);
        gtk_list_box_append(page->history_list, row);
    }
    if (page->history_button != NULL)
        gtk_widget_set_sensitive(
            GTK_WIDGET(page->history_button),
            page->history != NULL &&
            g_list_model_get_n_items(G_LIST_MODEL(page->history)) > 0);
}

static void
remember_search(GswordSearchPage *page, const gchar *text)
{
    GtkStringList *history;
    guint i;
    guint count = 0;

    if (*text == '\0')
        return;
    history = gtk_string_list_new(NULL);
    gtk_string_list_append(history, text);
    if (page->history != NULL) {
        for (i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(page->history));
             i++) {
            const gchar *item = gtk_string_list_get_string(page->history, i);

            if (g_strcmp0(item, text) == 0)
                continue;
            gtk_string_list_append(history, item);
            if (++count >= 19)
                break;
        }
    }
    g_clear_object(&page->history);
    page->history = history;
    rebuild_history_list(page);
}

static gchar *
highlight_matches(GswordSearchPage *page, const gchar *text,
                  const gchar *query)
{
    gboolean case_sensitive = gtk_check_button_get_active(page->case_sensitive);
    guint type = gtk_drop_down_get_selected(page->type);
    gchar *trimmed;
    gchar *pattern;
    GRegex *regex;
    GMatchInfo *match = NULL;
    GString *markup;
    gint last = 0;

    if (text == NULL || query == NULL || (trimmed = g_strdup(query)) == NULL)
        return g_markup_escape_text(text != NULL ? text : "", -1);
    g_strstrip(trimmed);
    if (*trimmed == '\0' || type == 2) {
        g_free(trimmed);
        return g_markup_escape_text(text, -1);
    }
    if (type == 1) {
        gchar **words = g_strsplit_set(trimmed, " \t\n", -1);
        GString *alternatives = g_string_new(NULL);
        guint i;

        for (i = 0; words[i] != NULL; i++) {
            gchar *escaped;

            if (*words[i] == '\0')
                continue;
            escaped = g_regex_escape_string(words[i], -1);
            if (alternatives->len > 0)
                g_string_append_c(alternatives, '|');
            g_string_append(alternatives, escaped);
            g_free(escaped);
        }
        pattern = g_string_free(alternatives, FALSE);
        g_strfreev(words);
    } else {
        pattern = g_regex_escape_string(trimmed, -1);
    }
    g_free(trimmed);
    regex = g_regex_new(pattern,
                        G_REGEX_OPTIMIZE |
                        (case_sensitive ? 0 : G_REGEX_CASELESS),
                        G_REGEX_MATCH_DEFAULT, NULL);
    g_free(pattern);
    if (regex == NULL)
        return g_markup_escape_text(text, -1);

    markup = g_string_new(NULL);
    g_regex_match(regex, text, 0, &match);
    while (match != NULL && g_match_info_matches(match)) {
        gint start;
        gint end;
        gchar *before;
        gchar *hit;

        g_match_info_fetch_pos(match, 0, &start, &end);
        if (end <= start)
            break;
        before = g_markup_escape_text(text + last, start - last);
        hit = g_markup_escape_text(text + start, end - start);
        g_string_append(markup, before);
        g_string_append(markup, "<b>");
        g_string_append(markup, hit);
        g_string_append(markup, "</b>");
        g_free(before);
        g_free(hit);
        last = end;
        g_match_info_next(match, NULL);
    }
    if (last < (gint)strlen(text)) {
        gchar *tail = g_markup_escape_text(text + last, -1);

        g_string_append(markup, tail);
        g_free(tail);
    }
    if (match != NULL)
        g_match_info_free(match);
    g_regex_unref(regex);
    return g_string_free(markup, FALSE);
}

static void
clear_results(GswordSearchPage *page)
{
    while (gtk_widget_get_first_child(GTK_WIDGET(page->results)) != NULL)
        gtk_list_box_remove(
            page->results,
            gtk_widget_get_first_child(GTK_WIDGET(page->results)));
}

static void
clear_context(GswordSearchPage *page)
{
    g_clear_pointer(&page->context_module, g_free);
    g_clear_pointer(&page->context_key, g_free);
    g_clear_pointer(&page->context_display, g_free);
    g_clear_pointer(&page->context_storage, g_free);
    g_clear_pointer(&page->context_text, g_free);
    page->context_is_bible = FALSE;
}

static void
prepare_context(GswordSearchPage *page, GtkListBoxRow *row)
{
    GswManager *manager = gsword_window_get_manager(page->window);
    GswModule *module = NULL;
    GswModuleInfo *info;
    const gchar *module_name;
    const gchar *key;

    clear_context(page);
    if (row == NULL)
        return;
    module_name = g_object_get_data(G_OBJECT(row), "result-module");
    key = g_object_get_data(G_OBJECT(row), "result-key");
    page->context_module = g_strdup(module_name);
    page->context_key = g_strdup(key);
    page->context_display = g_strdup(key);
    page->context_storage = g_strdup(key);
    if (module_name == NULL || *module_name == '\0' ||
        key == NULL || *key == '\0' || manager == NULL)
        return;

    module = gsw_manager_create_module(manager, module_name);
    info = gsw_manager_lookup_module_info(manager, module_name);
    if (module == NULL || !gsw_module_set_key_text(module, key, NULL)) {
        g_clear_object(&module);
        return;
    }
    if (gsword_window_module_effective_type(info) == GSW_MODULE_TYPE_BIBLES &&
        gsw_key_get_kind(gsw_module_get_key(module)) == GSW_KEY_KIND_VERSE) {
        GswVerseKey *verse_key = (GswVerseKey *)gsw_module_get_key(module);

        page->context_is_bible = TRUE;
        g_free(page->context_display);
        page->context_display = g_strdup(gsw_module_get_key_text(module));
        g_free(page->context_storage);
        page->context_storage = gsw_verse_key_get_osis_ref(verse_key);
        if (page->context_storage == NULL)
            page->context_storage = g_strdup(page->context_display);
    }
    page->context_text = gsw_module_get_text(module, NULL);
    g_clear_object(&module);
}

static void
copy_context_text(GswordSearchPage *page, const gchar *text)
{
    GdkClipboard *clipboard =
        gtk_widget_get_clipboard(GTK_WIDGET(page->window));

    gdk_clipboard_set_text(clipboard, text != NULL ? text : "");
    gsword_window_set_status(page->window, _("Copied to clipboard"));
}

static void
on_context_copy_text(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    copy_context_text(page, page->context_text);
}

static void
on_context_copy_reference(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    copy_context_text(page, page->context_display);
}

static void
on_context_copy_both(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;
    gchar *text;

    (void)button;
    text = g_strdup_printf("%s\n\n%s",
                           page->context_display != NULL
                           ? page->context_display : "",
                           page->context_text != NULL
                           ? page->context_text : "");
    gtk_popover_popdown(page->context_menu);
    copy_context_text(page, text);
    g_free(text);
}

static void
on_context_open_reader(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    gsword_window_open_module_reference(page->window, page->context_module,
                                        page->context_key);
}

static void
on_context_open_compare(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    gsword_window_open_compare_reference(page->window, page->context_module,
                                         page->context_key);
}

static void
on_context_open_note(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    gsword_window_open_note_for(page->window, page->context_module,
                                page->context_storage, page->context_display);
}

static void
on_context_add_prayer(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    if (!page->context_is_bible || page->context_module == NULL ||
        page->context_storage == NULL || page->context_display == NULL) {
        gsword_window_set_status(
            page->window, _("This result cannot be added to prayer"));
        return;
    }
    gsword_window_open_prayer_for(page->window, page->context_module,
                                  page->context_storage,
                                  page->context_display);
}

static void
on_context_add_memory(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;
    GswordPlanStore *plans = gsword_window_get_plan_store(page->window);
    GError *error = NULL;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    if (plans == NULL || !page->context_is_bible ||
        page->context_text == NULL) {
        gsword_window_set_status(page->window,
                                 _("This result cannot be added to memory review"));
        return;
    }
    if (!gsword_plan_store_add_memory_verse(
            plans, page->context_module, page->context_storage,
            page->context_display, page->context_text, &error)) {
        gsword_window_show_error(page->window,
                                 _("Cannot add memory verse"), error);
        return;
    }
    gsword_window_set_status(page->window,
                             _("Verse added to memory review"));
}

static void
on_list_menu_choice(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;
    GswordNotesStore *store = gsword_window_get_notes_store(page->window);
    gint64 list_id = (gint64)GPOINTER_TO_SIZE(
        g_object_get_data(G_OBJECT(button), "list-id"));
    GError *error = NULL;

    gtk_popover_popdown(page->list_menu);
    if (store == NULL)
        return;
    if (!gsword_notes_store_add_to_list(
            store, list_id, page->context_module, page->context_storage,
            page->context_display, &error)) {
        gsword_window_show_error(page->window,
                                 _("Cannot add verse to list"), error);
        return;
    }
    gsword_window_set_status(page->window, _("Verse added to list"));
}

static void
on_list_menu_open_notes(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->list_menu);
    gsword_window_show_personal(page->window,
                                GSWORD_PERSONAL_SECTION_COLLECTIONS);
}

static void
rebuild_list_menu(GswordSearchPage *page)
{
    GswordNotesStore *store;
    GtkWidget *child;
    GtkWidget *box;
    GtkWidget *title;
    GPtrArray *lists;
    gboolean has_normal = FALSE;

    ensure_result_popovers(page);
    if (page->list_menu == NULL)
        return;
    store = gsword_window_get_notes_store(page->window);
    child = NULL;
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    title = gtk_label_new(_("Add to verse list"));
    lists = NULL;
    gtk_widget_add_css_class(title, "heading");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_box_append(GTK_BOX(box), title);

    if (store == NULL) {
        gtk_box_append(GTK_BOX(box),
                       gtk_label_new(_("Verse lists are unavailable.")));
    } else {
        lists = gsword_notes_store_list_lists(store);
        for (guint i = 0; i < lists->len; i++) {
            GswordVerseList *list = g_ptr_array_index(lists, i);
            GtkWidget *button;

            if (g_strcmp0(list->kind, "favorites") == 0)
                continue;
            has_normal = TRUE;
            button = gtk_button_new_with_label(list->name);
            gtk_widget_set_halign(button, GTK_ALIGN_FILL);
            g_object_set_data(G_OBJECT(button), "list-id",
                              GSIZE_TO_POINTER((gsize)list->id));
            g_signal_connect(button, "clicked",
                             G_CALLBACK(on_list_menu_choice), page);
            gtk_box_append(GTK_BOX(box), button);
        }
        if (!has_normal) {
            GtkWidget *label = gtk_label_new(_("No verse lists yet"));
            GtkWidget *open = gtk_button_new_with_label(
                _("Open collections"));

            gtk_widget_add_css_class(label, "dim-label");
            gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
            g_signal_connect(open, "clicked",
                             G_CALLBACK(on_list_menu_open_notes), page);
            gtk_box_append(GTK_BOX(box), label);
            gtk_box_append(GTK_BOX(box), open);
        }
        g_ptr_array_unref(lists);
    }

    child = gtk_popover_get_child(page->list_menu);
    if (child != NULL)
        gtk_popover_set_child(page->list_menu, NULL);
    gtk_popover_set_child(page->list_menu, box);
}

static void
on_context_add_list(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;
    GdkRectangle rect = { page->context_x, page->context_y, 1, 1 };

    (void)button;
    gtk_popover_popdown(page->context_menu);
    rebuild_list_menu(page);
    gtk_popover_set_pointing_to(page->list_menu, &rect);
    gtk_popover_popup(page->list_menu);
}

static void
on_context_module_info(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;
    GswManager *manager = gsword_window_get_manager(page->window);
    GswModuleInfo *info;
    AdwDialog *dialog;
    gchar *body;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    info = manager != NULL && page->context_module != NULL
         ? gsw_manager_lookup_module_info(manager, page->context_module) : NULL;
    if (info == NULL) {
        gsword_window_set_status(page->window, _("Module information unavailable"));
        return;
    }
    body = g_strdup_printf(
        "%s\n\n%s\n%s\n\n%s: %s\n%s: %s\n%s: %s",
        gsw_module_info_get_description(info) != NULL
        ? gsw_module_info_get_description(info) : "",
        gsword_window_module_type_display(info),
        gsw_module_info_get_name(info),
        _("Language"),
        gsw_module_info_get_language(info) != NULL
        ? gsw_module_info_get_language(info) : "?",
        _("Version"),
        gsw_module_info_get_version(info) != NULL
        ? gsw_module_info_get_version(info) : "?",
        _("Path"),
        gsw_module_info_get_absolute_data_path(info) != NULL
        ? gsw_module_info_get_absolute_data_path(info) : "?");
    dialog = adw_alert_dialog_new(_("Module information"), NULL);
    adw_alert_dialog_format_body(ADW_ALERT_DIALOG(dialog), "%s", body);
    adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), "close",
                                  _("Close"));
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "close");
    adw_dialog_present(dialog, GTK_WIDGET(page->window));
    g_free(body);
}

static GtkWidget *
add_context_button(GtkWidget *box, const gchar *label, GCallback callback,
                   GswordSearchPage *page, gboolean sensitive)
{
    GtkWidget *button = gtk_button_new_with_label(label);

    gtk_widget_add_css_class(button, "flat");
    gtk_widget_set_halign(button, GTK_ALIGN_FILL);
    gtk_widget_set_sensitive(button, sensitive);
    g_signal_connect(button, "clicked", callback, page);
    gtk_box_append(GTK_BOX(box), button);
    return button;
}

static void
on_results_unrealize(GtkWidget *widget, gpointer user_data)
{
    GswordSearchPage *page = user_data;

    (void)widget;
    if (page->context_menu != NULL &&
        gtk_widget_get_parent(GTK_WIDGET(page->context_menu)) != NULL)
        gtk_widget_unparent(GTK_WIDGET(page->context_menu));
    if (page->list_menu != NULL &&
        gtk_widget_get_parent(GTK_WIDGET(page->list_menu)) != NULL)
        gtk_widget_unparent(GTK_WIDGET(page->list_menu));
    page->context_menu = NULL;
    page->list_menu = NULL;
}

static void
ensure_result_popovers(GswordSearchPage *page)
{
    GtkWidget *scrolled = gtk_widget_get_ancestor(GTK_WIDGET(page->results),
                                                  GTK_TYPE_SCROLLED_WINDOW);

    if (scrolled == NULL)
        return;
    if (page->context_menu == NULL) {
        page->context_menu = GTK_POPOVER(gtk_popover_new());
        gtk_popover_set_autohide(page->context_menu, TRUE);
        gtk_popover_set_has_arrow(page->context_menu, TRUE);
        gtk_widget_set_parent(GTK_WIDGET(page->context_menu), scrolled);
    }
    if (page->list_menu == NULL) {
        page->list_menu = GTK_POPOVER(gtk_popover_new());
        gtk_popover_set_autohide(page->list_menu, TRUE);
        gtk_popover_set_has_arrow(page->list_menu, TRUE);
        gtk_widget_set_parent(GTK_WIDGET(page->list_menu), scrolled);
    }
}

static void
show_context_menu(GswordSearchPage *page, GtkListBoxRow *row,
                  gint x, gint y)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *header;
    GtkWidget *child;
    GtkWidget *scrolled;
    graphene_point_t point;
    graphene_point_t translated;
    gchar *module_escaped;
    gchar *reference_escaped;
    gchar *header_text;
    GdkRectangle rect = { x, y, 1, 1 };

    if (page == NULL || row == NULL)
        return;
    ensure_result_popovers(page);
    if (page->context_menu == NULL)
        return;
    prepare_context(page, row);
    scrolled = gtk_widget_get_ancestor(GTK_WIDGET(page->results),
                                       GTK_TYPE_SCROLLED_WINDOW);
    if (scrolled != NULL) {
        point = GRAPHENE_POINT_INIT((gfloat)x, (gfloat)y);
        if (gtk_widget_compute_point(GTK_WIDGET(page->results), scrolled,
                                     &point, &translated)) {
            rect.x = (gint)translated.x;
            rect.y = (gint)translated.y;
        }
    }
    page->context_x = rect.x;
    page->context_y = rect.y;

    module_escaped = g_markup_escape_text(
        page->context_module != NULL ? page->context_module : "", -1);
    reference_escaped = g_markup_escape_text(
        page->context_display != NULL ? page->context_display : "", -1);
    header_text = g_strdup_printf("<b>%s</b> · %s", module_escaped,
                                  reference_escaped);
    header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header), header_text);
    gtk_label_set_xalign(GTK_LABEL(header), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(header), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_margin_top(header, 8);
    gtk_widget_set_margin_bottom(header, 6);
    gtk_widget_set_margin_start(header, 10);
    gtk_widget_set_margin_end(header, 10);
    gtk_box_append(GTK_BOX(box), header);
    g_free(header_text);
    g_free(module_escaped);
    g_free(reference_escaped);

    add_context_button(box, _("Copy verse"), G_CALLBACK(on_context_copy_text),
                       page, page->context_text != NULL);
    add_context_button(box, _("Copy reference"),
                       G_CALLBACK(on_context_copy_reference), page,
                       page->context_display != NULL);
    add_context_button(box, _("Copy verse and reference"),
                       G_CALLBACK(on_context_copy_both), page,
                       page->context_text != NULL &&
                       page->context_display != NULL);
    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    add_context_button(box, _("Open in reader"),
                       G_CALLBACK(on_context_open_reader), page,
                       page->context_module != NULL &&
                       page->context_key != NULL);
    add_context_button(box, _("Open in compare"),
                       G_CALLBACK(on_context_open_compare), page,
                       page->context_is_bible);
    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    add_context_button(box, _("Add note"), G_CALLBACK(on_context_open_note),
                       page, page->context_is_bible);
    add_context_button(box, _("Add to prayer"),
                       G_CALLBACK(on_context_add_prayer), page,
                       page->context_is_bible &&
                       page->context_storage != NULL &&
                       page->context_display != NULL);
    add_context_button(box, _("Add to memory review"),
                       G_CALLBACK(on_context_add_memory), page,
                       page->context_is_bible && page->context_text != NULL);
    add_context_button(box, _("Add to verse list"),
                       G_CALLBACK(on_context_add_list), page,
                       page->context_is_bible);
    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    add_context_button(box, _("Module information"),
                       G_CALLBACK(on_context_module_info), page,
                       page->context_module != NULL);

    child = gtk_popover_get_child(page->context_menu);
    if (child != NULL)
        gtk_popover_set_child(page->context_menu, NULL);
    gtk_box_set_spacing(GTK_BOX(box), 2);
    gtk_widget_set_margin_top(box, 4);
    gtk_widget_set_margin_bottom(box, 4);
    gtk_widget_set_margin_start(box, 4);
    gtk_widget_set_margin_end(box, 4);
    gtk_popover_set_child(page->context_menu, box);
    gtk_popover_set_pointing_to(page->context_menu, &rect);
    gtk_popover_popup(page->context_menu);
}

static void
on_result_menu_clicked(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;
    GtkListBoxRow *row = g_object_get_data(G_OBJECT(button), "result-row");
    graphene_rect_t bounds;

    if (row == NULL)
        return;
    if (gtk_widget_compute_bounds(GTK_WIDGET(button),
                                  GTK_WIDGET(page->results), &bounds))
        show_context_menu(page, row,
                          (gint)(bounds.origin.x + bounds.size.width),
                          (gint)(bounds.origin.y + bounds.size.height));
}

static void
on_results_context_clicked(GtkGestureClick *gesture, gint n_press,
                           gdouble x, gdouble y, gpointer user_data)
{
    GswordSearchPage *page = user_data;
    GtkListBoxRow *row;

    (void)n_press;
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture)) !=
        GDK_BUTTON_SECONDARY)
        return;
    row = gtk_list_box_get_row_at_y(page->results, (gint)y);
    if (row != NULL) {
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
        show_context_menu(page, row, (gint)x, (gint)y);
    }
}

static void
populate_results(GswordSearchPage *page, GPtrArray *results)
{
    guint i;

    clear_results(page);
    for (i = 0; i < results->len; i++) {
        GswSearchResult *result = g_ptr_array_index(results, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label;
        const gchar *query = gtk_editable_get_text(GTK_EDITABLE(page->entry));
        gchar *key = g_markup_escape_text(gsw_search_result_get_key(result),
                                          -1);
        gchar *snippet = highlight_matches(
            page, gsw_search_result_get_snippet(result), query);
        gchar *text = g_strdup_printf("<b>%s</b>\n%s", key, snippet);

        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_set_hexpand(label, TRUE);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        {
            GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
            GtkWidget *more = gtk_button_new_from_icon_name(
                "view-more-symbolic");

            gtk_widget_add_css_class(more, "flat");
            gtk_widget_set_valign(more, GTK_ALIGN_CENTER);
            gtk_widget_set_tooltip_text(more, _("Result actions"));
            g_object_set_data(G_OBJECT(more), "result-row", row);
            g_signal_connect(more, "clicked",
                             G_CALLBACK(on_result_menu_clicked), page);
            gtk_box_append(GTK_BOX(row_box), label);
            gtk_box_append(GTK_BOX(row_box), more);
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
        }
        g_object_set_data_full(G_OBJECT(row), "result-module",
                               g_strdup(gsw_search_result_get_module_name(result)),
                               g_free);
        g_object_set_data_full(G_OBJECT(row), "result-key",
                               g_strdup(gsw_search_result_get_key(result)),
                               g_free);
        gtk_list_box_append(page->results, row);
        g_free(key);
        g_free(snippet);
        g_free(text);
    }
    {
        gchar *message = g_strdup_printf(_("Search: %u result(s)"),
                                         results->len);

        gtk_label_set_text(page->results_label, message);
        gsword_window_set_status(page->window, message);
        g_free(message);
    }
    page->results_count = results->len;
    if (results->len == 0)
        gtk_label_set_text(page->empty_label, _("No results."));
    gtk_stack_set_visible_child_name(page->results_stack,
                                     results->len > 0 ? "results" : "empty");
}

static void
restore_results_label(GswordSearchPage *page)
{
    if (page->results_count > 0) {
        gchar *message = g_strdup_printf(_("Search: %u result(s)"),
                                         page->results_count);

        gtk_label_set_text(page->results_label, message);
        g_free(message);
    } else {
        gtk_label_set_text(page->empty_label, _("No results."));
        gtk_stack_set_visible_child_name(page->results_stack, "empty");
    }
}

static void
set_searching(GswordSearchPage *page, gboolean searching)
{
    gtk_widget_set_sensitive(GTK_WIDGET(page->search_button), !searching);
    gtk_widget_set_visible(GTK_WIDGET(page->cancel_button), searching);
    if (searching) {
        gtk_label_set_text(page->results_label, _("Searching..."));
        if (page->results_count == 0) {
            gtk_label_set_text(page->empty_label, _("Searching..."));
            gtk_stack_set_visible_child_name(page->results_stack, "empty");
        }
    } else {
        gtk_widget_set_sensitive(
            GTK_WIDGET(page->search_button),
            *gtk_editable_get_text(GTK_EDITABLE(page->entry)) != '\0');
    }
}

static void
on_search_done(GObject *source_object, GAsyncResult *result,
               gpointer user_data)
{
    GTask *task = G_TASK(result);
    SearchJob *job = g_task_get_task_data(task);
    GswordSearchPage *page = job != NULL ? job->page : NULL;
    GError *error = NULL;
    GPtrArray *results;

    (void)source_object;
    (void)user_data;
    results = g_task_propagate_pointer(task, &error);
    if (page == NULL) {
        g_clear_pointer(&results, g_ptr_array_unref);
        g_clear_error(&error);
        return;
    }

    if (page->search_task == task)
        g_clear_object(&page->search_task);
    g_clear_object(&page->search_cancellable);
    set_searching(page, FALSE);

    if (results == NULL) {
        restore_results_label(page);
        if (g_error_matches(error, GSW_ERROR, GSW_ERROR_CANCELLED) ||
            g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            gsword_window_set_status(page->window, _("Search cancelled"));
        } else {
            gsword_window_show_error(page->window, _("Search failed"), error);
        }
        g_clear_error(&error);
        return;
    }

    g_clear_error(&error);
    populate_results(page, results);
    g_ptr_array_unref(results);
}

static void
run_search(GswordSearchPage *page)
{
    GswordWindow *window = page->window;
    GswModule *current = gsword_window_get_module(window);
    GswManager *manager;
    GswModule *module;
    GswSearchOptions *options;
    GswVerseList *scope;
    GError *scope_error = NULL;
    SearchJob *job;
    GTask *task;
    const gchar *text = gtk_editable_get_text(GTK_EDITABLE(page->entry));
    const gchar *attribute1 = gtk_editable_get_text(
        GTK_EDITABLE(page->attribute1));
    const gchar *attribute2 = gtk_editable_get_text(
        GTK_EDITABLE(page->attribute2));
    const gchar *module_name = selected_module_name(page);
    guint selected = gtk_drop_down_get_selected(page->type);

    if (*text == '\0')
        return;
    if ((*attribute1 == '\0') != (*attribute2 == '\0')) {
        gsword_window_set_status(
            window, _("Enter both attribute levels or leave both empty"));
        return;
    }
    if (module_name == NULL && current != NULL)
        module_name = gsw_module_get_name(current);
    if (module_name == NULL) {
        gsword_window_set_status(window, _("Open a module before searching"));
        return;
    }
    scope = build_scope(page, &scope_error);
    if (scope == NULL && scope_error != NULL) {
        gsword_window_set_status(window, _("No active verse reference"));
        g_clear_error(&scope_error);
        return;
    }
    if (page->search_task != NULL) {
        SearchJob *old_job = g_task_get_task_data(page->search_task);

        if (old_job != NULL)
            old_job->page = NULL;
        g_cancellable_cancel(page->search_cancellable);
        g_clear_object(&page->search_task);
        g_clear_object(&page->search_cancellable);
    }

    manager = gsword_window_get_manager(window);
    module = manager != NULL
           ? gsw_manager_create_module(manager, module_name)
           : NULL;
    if (module != NULL)
        configure_search_index(window, module, module_name);
    if (module == NULL) {
        gsword_window_set_status(window, _("Cannot open module"));
        return;
    }
    options = gsw_search_options_new();
    gsw_search_options_set_text(options, text);
    gsw_search_options_set_search_type(
        options, selected == 1 ? GSW_SEARCH_MULTIWORD
                             : selected == 2 ? GSW_SEARCH_REGEX
                                             : GSW_SEARCH_PHRASE);
    gsw_search_options_set_case_sensitive(
        options, gtk_check_button_get_active(page->case_sensitive));
    if (*attribute1 != '\0')
        gsw_search_options_set_attribute(options, attribute1, attribute2);
    gsw_search_options_set_scope(options, scope);
    g_clear_pointer(&scope, gsw_verse_list_unref);
    gsw_search_options_set_max_results(options, 200);

    job = g_new0(SearchJob, 1);
    job->window = g_object_ref(window);
    job->page = page;
    job->module = module;
    job->options = options;
    page->search_cancellable = g_cancellable_new();
    task = g_task_new(NULL, page->search_cancellable, on_search_done, NULL);
    g_task_set_task_data(task, job, search_job_free);
    page->search_task = task;
    remember_search(page, text);
    set_searching(page, TRUE);
    gsword_window_set_status(window, _("Searching..."));
    g_task_run_in_thread(task, search_thread);
}

static void
on_search_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    run_search(user_data);
}

static void
on_search_changed(GtkSearchEntry *entry, gpointer user_data)
{
    GswordSearchPage *page = user_data;

    gtk_widget_set_sensitive(
        GTK_WIDGET(page->search_button),
        *gtk_editable_get_text(GTK_EDITABLE(entry)) != '\0');
}

static void
on_search_stop(GtkSearchEntry *entry, gpointer user_data)
{
    GswordSearchPage *page = user_data;

    (void)entry;
    gsword_window_close_search(page->window);
}

static void
on_cancel_clicked(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;

    (void)button;
    if (page->search_cancellable != NULL)
        g_cancellable_cancel(page->search_cancellable);
}

static void
on_back_clicked(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;

    (void)button;
    gsword_window_close_search(page->window);
}

static void
on_search_activate(GtkEntry *entry, gpointer user_data)
{
    (void)entry;
    run_search(user_data);
}

static void
on_result_selected(GtkListBox *list, GtkListBoxRow *row,
                   gpointer user_data)
{
    GswordSearchPage *page = user_data;
    const gchar *module_name;
    const gchar *key;

    (void)list;
    if (row == NULL)
        return;
    module_name = g_object_get_data(G_OBJECT(row), "result-module");
    key = g_object_get_data(G_OBJECT(row), "result-key");
    gsword_window_open_module_reference(page->window, module_name, key);
}

static void
on_index_progress(const gchar *module_name, guint processed, guint total,
                  gpointer user_data)
{
    GswordSearchPage *page = user_data;
    gchar *message = g_strdup_printf(_("Indexing %s: %u/%u"),
                                     module_name, processed, total);

    gsword_window_set_status(page->window, message);
    g_free(message);
    while (g_main_context_iteration(NULL, FALSE))
        ;
}

static void
on_index_selection_changed(GObject *object, GParamSpec *pspec,
                           gpointer user_data)
{
    (void)object;
    (void)pspec;
    refresh_index_status(user_data);
}

static void
on_index_option_changed(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    refresh_index_status(user_data);
}

static void
on_attribute_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    refresh_index_status(user_data);
}

void
gsword_search_page_refresh_index(GswordSearchPage *page)
{
    g_return_if_fail(page != NULL);
    refresh_modules(page, NULL);
    refresh_index_status(page);
}

void
gsword_search_page_select_current(GswordSearchPage *page)
{
    GswModule *current;

    g_return_if_fail(page != NULL);
    current = gsword_window_get_module(page->window);
    refresh_modules(page, current != NULL
                          ? gsw_module_get_name(current) : NULL);
    refresh_index_status(page);
}

static void
on_build_index(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;
    GswModule *module = create_selected_module(page);
    GError *error = NULL;

    (void)button;
    if (module == NULL)
        return;
    if (!gsw_module_build_search_index(module, NULL, on_index_progress, page,
                                       &error)) {
        gsword_window_show_error(page->window, _("Cannot build index"), error);
        g_clear_object(&module);
        return;
    }
    gsword_window_set_status(page->window, _("Search index built"));
    g_clear_object(&module);
    gsword_search_page_refresh_index(page);
}

static void
on_delete_index(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;
    GswModule *module = create_selected_module(page);
    GError *error = NULL;

    (void)button;
    if (module == NULL)
        return;
    if (!gsw_module_delete_search_index(module, &error)) {
        gsword_window_show_error(page->window, _("Cannot delete index"), error);
        g_clear_object(&module);
        return;
    }
    gsword_window_set_status(page->window, _("Search index deleted"));
    g_clear_object(&module);
    gsword_search_page_refresh_index(page);
}

static void
search_page_free(gpointer data)
{
    GswordSearchPage *page = data;

    if (page == NULL)
        return;
    if (page->search_task != NULL) {
        SearchJob *job = g_task_get_task_data(page->search_task);

        if (job != NULL)
            job->page = NULL;
        if (page->search_cancellable != NULL)
            g_cancellable_cancel(page->search_cancellable);
        g_clear_object(&page->search_task);
    }
    g_clear_object(&page->search_cancellable);
    g_clear_object(&page->history);
    clear_context(page);
    g_free(page);
}

GswordSearchPage *
gsword_search_page_new(GswordWindow *window)
{
    GswordSearchPage *page = g_new0(GswordSearchPage, 1);
    GtkWidget *box;
    GtkWidget *controls;
    GtkWidget *search_row;
    GtkWidget *range_row;
    GtkWidget *options_row;
    GtkWidget *index_row;
    GtkWidget *advanced;
    GtkWidget *advanced_box;
    GtkWidget *module_label;
    GtkWidget *scope_label;
    GtkWidget *attribute_label1;
    GtkWidget *attribute_label2;
    GtkWidget *search;
    GtkWidget *cancel;
    GtkWidget *history;
    GtkWidget *back;
    GtkWidget *build;
    GtkWidget *delete;
    GtkWidget *scrolled;
    GtkStringList *types;

    page->window = window;
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    search_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    range_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    options_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    index_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    search = gtk_button_new_with_label(_("Search"));
    cancel = gtk_button_new_with_label(_("Cancel"));
    history = gtk_menu_button_new();
    back = gtk_button_new_from_icon_name("go-previous-symbolic");
    build = gtk_button_new_with_label(_("Build index"));
    delete = gtk_button_new_with_label(_("Delete index"));
    scrolled = gtk_scrolled_window_new();

    gtk_widget_set_margin_top(controls, 8);
    gtk_widget_set_margin_bottom(controls, 8);
    gtk_widget_set_margin_start(controls, 12);
    gtk_widget_set_margin_end(controls, 12);
    gtk_widget_add_css_class(back, "flat");
    gtk_widget_add_css_class(cancel, "flat");
    gtk_widget_add_css_class(search, "suggested-action");
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(history),
                                  "document-open-recent-symbolic");
    gtk_widget_add_css_class(history, "flat");
    gtk_widget_set_sensitive(history, FALSE);
    gtk_widget_set_tooltip_text(history, _("Recent searches"));
    gtk_widget_set_tooltip_text(back, _("Back"));
    gtk_widget_set_visible(cancel, FALSE);
    gtk_widget_set_tooltip_text(build, _("Build search index"));
    gtk_widget_set_tooltip_text(delete, _("Delete search index"));

    page->entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_search_entry_set_placeholder_text(page->entry, _("Search text"));
    gtk_widget_set_hexpand(GTK_WIDGET(page->entry), TRUE);
    page->history = gtk_string_list_new(NULL);
    page->module_selector = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    page->scope_selector = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    {
        GtkStringList *scopes = gtk_string_list_new(
            (const gchar *[]) { _("Whole module"), _("Old Testament"),
                                _("New Testament"), _("Current book"),
                                _("Current chapter"), NULL });

        gtk_drop_down_set_model(page->scope_selector, G_LIST_MODEL(scopes));
        g_object_unref(scopes);
    }
    module_label = gtk_label_new(_("Module:"));
    scope_label = gtk_label_new(_("Range:"));
    page->type = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    types = gtk_string_list_new(
        (const gchar *[]) { _("Phrase"), _("Multiword"), _("Regex"), NULL });
    gtk_drop_down_set_model(page->type, G_LIST_MODEL(types));
    g_object_unref(types);
    page->case_sensitive = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label(_("Case sensitive")));
    page->attribute1 = GTK_ENTRY(gtk_entry_new());
    page->attribute2 = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->attribute1, _("Word"));
    gtk_entry_set_placeholder_text(page->attribute2, _("Lemma"));
    gtk_widget_set_size_request(GTK_WIDGET(page->attribute1), 110, -1);
    gtk_widget_set_size_request(GTK_WIDGET(page->attribute2), 110, -1);
    attribute_label1 = gtk_label_new(_("Attribute 1:"));
    attribute_label2 = gtk_label_new(_("Attribute 2:"));
    advanced = gtk_expander_new(_("Advanced"));
    gtk_widget_set_tooltip_text(advanced,
        _("Search tags such as Word / Lemma. Leave both fields empty for "
          "normal text search."));
    advanced_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_top(advanced_box, 6);
    gtk_box_append(GTK_BOX(advanced_box), attribute_label1);
    gtk_box_append(GTK_BOX(advanced_box), GTK_WIDGET(page->attribute1));
    gtk_box_append(GTK_BOX(advanced_box), attribute_label2);
    gtk_box_append(GTK_BOX(advanced_box), GTK_WIDGET(page->attribute2));
    gtk_expander_set_child(GTK_EXPANDER(advanced), advanced_box);
    page->history_button = GTK_MENU_BUTTON(history);
    page->history_list = GTK_LIST_BOX(gtk_list_box_new());
    {
        GtkWidget *popover = gtk_popover_new();
        GtkWidget *history_scrolled = gtk_scrolled_window_new();

        gtk_widget_set_size_request(history_scrolled, 280, 300);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(history_scrolled),
                                      GTK_WIDGET(page->history_list));
        gtk_popover_set_child(GTK_POPOVER(popover), history_scrolled);
        gtk_menu_button_set_popover(GTK_MENU_BUTTON(history), popover);
    }
    page->index_label = GTK_LABEL(gtk_label_new(_("Index: no module")));
    gtk_label_set_xalign(page->index_label, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->index_label), "dim-label");
    gtk_widget_add_css_class(GTK_WIDGET(page->index_label), "caption");
    gtk_widget_set_tooltip_text(GTK_WIDGET(page->index_label),
        _("The index accelerates case-insensitive phrase and multiword "
          "searches without a range."));

    page->search_button = GTK_BUTTON(search);
    page->cancel_button = GTK_BUTTON(cancel);
    page->build_button = GTK_BUTTON(build);
    page->delete_button = GTK_BUTTON(delete);
    gtk_widget_set_sensitive(search, FALSE);
    g_signal_connect(back, "clicked", G_CALLBACK(on_back_clicked), page);
    g_signal_connect(search, "clicked", G_CALLBACK(on_search_clicked), page);
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel_clicked), page);
    g_signal_connect(page->entry, "search-changed",
                     G_CALLBACK(on_search_changed), page);
    g_signal_connect(page->entry, "stop-search",
                     G_CALLBACK(on_search_stop), page);
    g_signal_connect(page->entry, "activate",
                     G_CALLBACK(on_search_activate), page);
    g_signal_connect(build, "clicked", G_CALLBACK(on_build_index), page);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_delete_index), page);
    g_signal_connect(page->module_selector, "notify::selected",
                     G_CALLBACK(on_index_selection_changed), page);
    g_signal_connect(page->scope_selector, "notify::selected",
                     G_CALLBACK(on_index_selection_changed), page);
    g_signal_connect(page->type, "notify::selected",
                     G_CALLBACK(on_index_selection_changed), page);
    g_signal_connect(page->case_sensitive, "toggled",
                     G_CALLBACK(on_index_option_changed), page);
    g_signal_connect(page->attribute1, "changed",
                     G_CALLBACK(on_attribute_changed), page);
    g_signal_connect(page->attribute2, "changed",
                     G_CALLBACK(on_attribute_changed), page);

    gtk_box_append(GTK_BOX(search_row), back);
    gtk_box_append(GTK_BOX(search_row), GTK_WIDGET(page->entry));
    gtk_box_append(GTK_BOX(search_row), history);
    gtk_box_append(GTK_BOX(search_row), search);
    gtk_box_append(GTK_BOX(search_row), cancel);
    gtk_box_append(GTK_BOX(range_row), module_label);
    gtk_box_append(GTK_BOX(range_row), GTK_WIDGET(page->module_selector));
    gtk_box_append(GTK_BOX(range_row), scope_label);
    gtk_box_append(GTK_BOX(range_row), GTK_WIDGET(page->scope_selector));
    gtk_box_append(GTK_BOX(options_row), GTK_WIDGET(page->type));
    gtk_box_append(GTK_BOX(options_row), GTK_WIDGET(page->case_sensitive));
    gtk_box_append(GTK_BOX(index_row), GTK_WIDGET(page->index_label));
    gtk_widget_set_hexpand(GTK_WIDGET(page->index_label), TRUE);
    gtk_box_append(GTK_BOX(index_row), build);
    gtk_box_append(GTK_BOX(index_row), delete);
    gtk_box_append(GTK_BOX(controls), search_row);
    gtk_box_append(GTK_BOX(controls), range_row);
    gtk_box_append(GTK_BOX(controls), options_row);
    gtk_box_append(GTK_BOX(controls), advanced);
    gtk_box_append(GTK_BOX(controls), index_row);

    page->results_label = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->results_label, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->results_label), "caption");
    gtk_widget_add_css_class(GTK_WIDGET(page->results_label), "dim-label");
    gtk_widget_set_margin_start(GTK_WIDGET(page->results_label), 12);
    gtk_widget_set_margin_end(GTK_WIDGET(page->results_label), 12);
    page->empty_label = GTK_LABEL(gtk_label_new(_("Enter a search term.")));
    gtk_label_set_wrap(page->empty_label, TRUE);
    gtk_label_set_xalign(page->empty_label, 0.0f);
    gtk_widget_set_margin_top(GTK_WIDGET(page->empty_label), 24);
    gtk_widget_set_margin_start(GTK_WIDGET(page->empty_label), 12);
    gtk_widget_set_margin_end(GTK_WIDGET(page->empty_label), 12);
    gtk_widget_add_css_class(GTK_WIDGET(page->empty_label), "dim-label");

    page->results = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(page->results, "row-selected",
                     G_CALLBACK(on_result_selected), page);
    page->context_menu = GTK_POPOVER(gtk_popover_new());
    gtk_popover_set_autohide(page->context_menu, TRUE);
    gtk_popover_set_has_arrow(page->context_menu, TRUE);
    gtk_widget_set_parent(GTK_WIDGET(page->context_menu), scrolled);
    page->list_menu = GTK_POPOVER(gtk_popover_new());
    gtk_popover_set_autohide(page->list_menu, TRUE);
    gtk_popover_set_has_arrow(page->list_menu, TRUE);
    gtk_widget_set_parent(GTK_WIDGET(page->list_menu), scrolled);
    {
        GtkGesture *context = gtk_gesture_click_new();

        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(context),
                                      GDK_BUTTON_SECONDARY);
        g_signal_connect(context, "pressed",
                         G_CALLBACK(on_results_context_clicked), page);
        gtk_widget_add_controller(GTK_WIDGET(page->results),
                                  GTK_EVENT_CONTROLLER(context));
    }
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(page->results));
    g_signal_connect(scrolled, "unrealize",
                     G_CALLBACK(on_results_unrealize), page);
    g_signal_connect(scrolled, "destroy",
                     G_CALLBACK(on_results_unrealize), page);
    gtk_widget_set_vexpand(scrolled, TRUE);
    page->results_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_add_named(page->results_stack, GTK_WIDGET(page->empty_label),
                        "empty");
    gtk_stack_add_named(page->results_stack, scrolled, "results");
    gtk_stack_set_visible_child_name(page->results_stack, "empty");

    gtk_box_append(GTK_BOX(box), controls);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(page->results_label));
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(page->results_stack));
    page->root = box;
    g_object_set_data_full(G_OBJECT(box), "gsword-search-page", page,
                           search_page_free);
    gsword_search_page_refresh_index(page);
    return page;
}

GtkWidget *
gsword_search_page_widget(GswordSearchPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}

void
gsword_search_page_focus(GswordSearchPage *page)
{
    if (page == NULL || page->entry == NULL)
        return;
    gtk_widget_grab_focus(GTK_WIDGET(page->entry));
    gtk_editable_select_region(GTK_EDITABLE(page->entry), 0, -1);
}
