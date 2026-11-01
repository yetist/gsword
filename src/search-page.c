/*
 * search-page.c: search page for the gsword window
 */

#include "search-page.h"
#include "window-private.h"

#include <glib/gi18n.h>

struct _GswordSearchPage {
    GswordWindow *window;
    GtkWidget *root;
    GtkEntry *entry;
    GtkDropDown *type;
    GtkCheckButton *case_sensitive;
    GtkListBox *results;
    GtkLabel *index_label;
};

static void
clear_results(GswordSearchPage *page)
{
    while (gtk_widget_get_first_child(GTK_WIDGET(page->results)) != NULL)
        gtk_list_box_remove(
            page->results,
            gtk_widget_get_first_child(GTK_WIDGET(page->results)));
}

static void
run_search(GswordSearchPage *page)
{
    GswModule *module = gsword_window_get_module(page->window);
    GswSearchOptions *options;
    GPtrArray *results;
    GError *error = NULL;
    guint i;
    guint selected = gtk_drop_down_get_selected(page->type);

    if (module == NULL) {
        gsword_window_set_status(page->window,
                                 _("Open a module before searching"));
        return;
    }
    clear_results(page);

    options = gsw_search_options_new();
    gsw_search_options_set_text(
        options, gtk_editable_get_text(GTK_EDITABLE(page->entry)));
    gsw_search_options_set_search_type(
        options, selected == 1 ? GSW_SEARCH_MULTIWORD
                             : selected == 2 ? GSW_SEARCH_REGEX
                                             : GSW_SEARCH_PHRASE);
    gsw_search_options_set_case_sensitive(
        options, gtk_check_button_get_active(page->case_sensitive));
    gsw_search_options_set_max_results(options, 200);
    results = gsw_module_search_full(module, options, NULL, NULL, NULL,
                                     &error);
    gsw_search_options_unref(options);
    if (results == NULL) {
        gsword_window_show_error(page->window, _("Search failed"), error);
        return;
    }

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

        gsword_window_set_status(page->window, message);
        g_free(message);
    }
    g_ptr_array_unref(results);
}

static void
on_search_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    run_search(user_data);
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

GswordSearchPage *
gsword_search_page_new(GswordWindow *window)
{
    GswordSearchPage *page = g_new0(GswordSearchPage, 1);
    GtkWidget *box;
    GtkWidget *controls;
    GtkWidget *search_row;
    GtkWidget *options_row;
    GtkWidget *search;
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
    build = gtk_button_new_from_icon_name("document-save-symbolic");
    delete = gtk_button_new_from_icon_name("edit-delete-symbolic");
    scrolled = gtk_scrolled_window_new();

    gtk_widget_set_margin_top(controls, 8);
    gtk_widget_set_margin_bottom(controls, 8);
    gtk_widget_set_margin_start(controls, 12);
    gtk_widget_set_margin_end(controls, 12);
    gtk_widget_set_tooltip_text(build, _("Build search index"));
    gtk_widget_set_tooltip_text(delete, _("Delete search index"));

    page->entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->entry, _("Search text"));
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

    g_signal_connect(search, "clicked", G_CALLBACK(on_search_clicked), page);
    g_signal_connect(page->entry, "activate",
                     G_CALLBACK(on_search_activate), page);
    g_signal_connect(build, "clicked", G_CALLBACK(on_build_index), page);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_delete_index), page);

    gtk_box_append(GTK_BOX(search_row), GTK_WIDGET(page->entry));
    gtk_box_append(GTK_BOX(search_row), search);
    gtk_box_append(GTK_BOX(options_row), GTK_WIDGET(page->type));
    gtk_box_append(GTK_BOX(options_row), GTK_WIDGET(page->case_sensitive));
    gtk_box_append(GTK_BOX(options_row), build);
    gtk_box_append(GTK_BOX(options_row), delete);
    gtk_box_append(GTK_BOX(controls), search_row);
    gtk_box_append(GTK_BOX(controls), options_row);
    gtk_box_append(GTK_BOX(controls), GTK_WIDGET(page->index_label));

    page->results = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(page->results, "row-selected",
                     G_CALLBACK(on_result_selected), page);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(page->results));
    gtk_widget_set_vexpand(scrolled, TRUE);

    gtk_box_append(GTK_BOX(box), controls);
    gtk_box_append(GTK_BOX(box), scrolled);
    page->root = box;
    g_object_set_data_full(G_OBJECT(box), "gsword-search-page", page, g_free);
    gsword_search_page_refresh_index(page);
    return page;
}

GtkWidget *
gsword_search_page_widget(GswordSearchPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}
