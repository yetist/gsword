/*
 * search-page.c: search page for the gsword window
 */

#include "search-page.h"
#include "window-private.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

struct _GswordSearchPage {
    GswordWindow *window;
    GtkWidget *root;
    GtkSearchEntry *entry;
    GtkDropDown *type;
    GtkCheckButton *case_sensitive;
    GtkListBox *results;
    GtkLabel *results_label;
    GtkLabel *empty_label;
    GtkStack *results_stack;
    GtkButton *search_button;
    GtkButton *cancel_button;
    GtkLabel *index_label;
    GCancellable *search_cancellable;
    GTask *search_task;
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

static void
clear_results(GswordSearchPage *page)
{
    while (gtk_widget_get_first_child(GTK_WIDGET(page->results)) != NULL)
        gtk_list_box_remove(
            page->results,
            gtk_widget_get_first_child(GTK_WIDGET(page->results)));
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
        gchar *text = g_strdup_printf("%s\n%s",
            gsw_search_result_get_key(result),
            gsw_search_result_get_snippet(result));

        label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "result-module",
                               g_strdup(gsw_search_result_get_module_name(result)),
                               g_free);
        g_object_set_data_full(G_OBJECT(row), "result-key",
                               g_strdup(gsw_search_result_get_key(result)),
                               g_free);
        gtk_list_box_append(page->results, row);
        g_free(text);
    }
    {
        gchar *message = g_strdup_printf(_("Search: %u result(s)"),
                                         results->len);

        gtk_label_set_text(page->results_label, message);
        gsword_window_set_status(page->window, message);
        g_free(message);
    }
    if (results->len == 0)
        gtk_label_set_text(page->empty_label, _("No results."));
    gtk_stack_set_visible_child_name(page->results_stack,
                                     results->len > 0 ? "results" : "empty");
}

static void
set_searching(GswordSearchPage *page, gboolean searching)
{
    gtk_widget_set_sensitive(GTK_WIDGET(page->search_button), !searching);
    gtk_widget_set_visible(GTK_WIDGET(page->cancel_button), searching);
    if (searching) {
        gtk_label_set_text(page->results_label, _("Searching..."));
        gtk_label_set_text(page->empty_label, _("Searching..."));
        gtk_stack_set_visible_child_name(page->results_stack, "empty");
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
    SearchJob *job;
    GTask *task;
    const gchar *text = gtk_editable_get_text(GTK_EDITABLE(page->entry));
    guint selected = gtk_drop_down_get_selected(page->type);

    if (*text == '\0')
        return;
    if (current == NULL) {
        gsword_window_set_status(window, _("Open a module before searching"));
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
           ? gsw_manager_create_module(manager, gsw_module_get_name(current))
           : NULL;
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

void
gsword_search_page_refresh_index(GswordSearchPage *page)
{
    GswModule *module;

    g_return_if_fail(page != NULL);
    module = gsword_window_get_module(page->window);
    if (module == NULL)
        gtk_label_set_text(page->index_label, _("Index: no module"));
    else
        gtk_label_set_text(page->index_label,
            gsw_module_has_search_index(module)
            ? _("Index: available") : _("Index: not built"));
}

static void
on_build_index(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;
    GswModule *module = gsword_window_get_module(page->window);
    GError *error = NULL;

    (void)button;
    if (module == NULL)
        return;
    if (!gsw_module_build_search_index(module, NULL, on_index_progress, page,
                                       &error)) {
        gsword_window_show_error(page->window, _("Cannot build index"), error);
        return;
    }
    gsword_window_set_status(page->window, _("Search index built"));
    gsword_search_page_refresh_index(page);
}

static void
on_delete_index(GtkButton *button, gpointer user_data)
{
    GswordSearchPage *page = user_data;
    GswModule *module = gsword_window_get_module(page->window);
    GError *error = NULL;

    (void)button;
    if (module == NULL)
        return;
    if (!gsw_module_delete_search_index(module, &error)) {
        gsword_window_show_error(page->window, _("Cannot delete index"), error);
        return;
    }
    gsword_window_set_status(page->window, _("Search index deleted"));
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
    g_free(page);
}

GswordSearchPage *
gsword_search_page_new(GswordWindow *window)
{
    GswordSearchPage *page = g_new0(GswordSearchPage, 1);
    GtkWidget *box;
    GtkWidget *controls;
    GtkWidget *search_row;
    GtkWidget *options_row;
    GtkWidget *search;
    GtkWidget *cancel;
    GtkWidget *back;
    GtkWidget *build;
    GtkWidget *delete;
    GtkWidget *scrolled;
    GtkStringList *types;

    page->window = window;
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    search_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    options_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    search = gtk_button_new_with_label(_("Search"));
    cancel = gtk_button_new_with_label(_("Cancel"));
    back = gtk_button_new_from_icon_name("go-previous-symbolic");
    build = gtk_button_new_from_icon_name("document-save-symbolic");
    delete = gtk_button_new_from_icon_name("edit-delete-symbolic");
    scrolled = gtk_scrolled_window_new();

    gtk_widget_set_margin_top(controls, 8);
    gtk_widget_set_margin_bottom(controls, 8);
    gtk_widget_set_margin_start(controls, 12);
    gtk_widget_set_margin_end(controls, 12);
    gtk_widget_add_css_class(back, "flat");
    gtk_widget_add_css_class(cancel, "flat");
    gtk_widget_set_tooltip_text(back, _("Back"));
    gtk_widget_set_visible(cancel, FALSE);
    gtk_widget_set_tooltip_text(build, _("Build search index"));
    gtk_widget_set_tooltip_text(delete, _("Delete search index"));

    page->entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_search_entry_set_placeholder_text(page->entry, _("Search text"));
    gtk_widget_set_hexpand(GTK_WIDGET(page->entry), TRUE);
    page->type = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    types = gtk_string_list_new(
        (const gchar *[]) { _("Phrase"), _("Multiword"), _("Regex"), NULL });
    gtk_drop_down_set_model(page->type, G_LIST_MODEL(types));
    g_object_unref(types);
    page->case_sensitive = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label(_("Case sensitive")));
    page->index_label = GTK_LABEL(gtk_label_new(_("Index: no module")));
    gtk_label_set_xalign(page->index_label, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->index_label), "dim-label");
    gtk_widget_add_css_class(GTK_WIDGET(page->index_label), "caption");

    page->search_button = GTK_BUTTON(search);
    page->cancel_button = GTK_BUTTON(cancel);
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

    gtk_box_append(GTK_BOX(search_row), back);
    gtk_box_append(GTK_BOX(search_row), GTK_WIDGET(page->entry));
    gtk_box_append(GTK_BOX(search_row), search);
    gtk_box_append(GTK_BOX(search_row), cancel);
    gtk_box_append(GTK_BOX(options_row), GTK_WIDGET(page->type));
    gtk_box_append(GTK_BOX(options_row), GTK_WIDGET(page->case_sensitive));
    gtk_box_append(GTK_BOX(options_row), build);
    gtk_box_append(GTK_BOX(options_row), delete);
    gtk_box_append(GTK_BOX(controls), search_row);
    gtk_box_append(GTK_BOX(controls), options_row);
    gtk_box_append(GTK_BOX(controls), GTK_WIDGET(page->index_label));

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
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(page->results));
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
