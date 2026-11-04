/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gtk.c: minimal GTK4 frontend prototype for gsword
 *
 * Builds when GTK4 is available. It demonstrates module listing, reading,
 * parallel reading, search/index management, local installation and a simple
 * RawGenBook notes page.
 */

#include <gtk/gtk.h>
#include <gsword/gsword.h>
#include <string.h>

typedef struct {
    GtkApplication *app;
    GtkWidget *window;
    GtkListBox *module_list;
    GtkListBox *result_list;
    GtkTextView *reader;
    GtkEntry *key_entry;
    GtkSearchEntry *search_entry;
    GtkLabel *status;
    GtkLabel *index_status;
    GtkComboBoxText *parallel_a;
    GtkComboBoxText *parallel_b;
    GtkEntry *parallel_key;
    GtkTextView *parallel_view_a;
    GtkTextView *parallel_view_b;
    GtkTextView *notes_view;
    GtkEntry *install_dir;
    GtkEntry *install_name;
    GswManager *manager;
    GswInstallManager *installer;
    GswModule *current;
    GswModule *notes_module;
    GswModule *parallel_module_a;
    GswModule *parallel_module_b;
} App;

static App app;

static void
set_status(const gchar *message)
{
    gtk_label_set_text(app.status, message != NULL ? message : "");
}

static void
set_text_view(GtkTextView *view, const gchar *text)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);

    gtk_text_buffer_set_text(buffer, text != NULL ? text : "", -1);
}

static void
clear_list_box(GtkListBox *list)
{
    GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(list));

    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);

        gtk_list_box_remove(list, child);
        child = next;
    }
}

static gchar *
text_view_contents(GtkTextView *view)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter start, end;

    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

static void
update_index_status(void)
{
    if (app.current == NULL) {
        gtk_label_set_text(app.index_status, "Index: no module");
        return;
    }
    gtk_label_set_text(app.index_status,
                       gsw_module_has_search_index(app.current)
                       ? "Index: available" : "Index: not built");
}

static void
show_error(const gchar *prefix, GError *error)
{
    gchar *message = g_strdup_printf("%s: %s", prefix,
                                     error != NULL ? error->message : "unknown");
    set_status(message);
    g_free(message);
    g_clear_error(&error);
}

static void
read_current_key(void)
{
    GError *error = NULL;
    gchar *text;

    if (app.current == NULL)
        return;
    text = gsw_module_get_text(app.current, &error);
    if (text == NULL) {
        show_error("Cannot read entry", error);
        return;
    }
    set_text_view(app.reader, text);
    g_free(text);
    {
        const gchar *key = gsw_module_get_key_text(app.current);
        if (key != NULL)
            gtk_editable_set_text(GTK_EDITABLE(app.key_entry), key);
    }
    update_index_status();
}

static void
load_module(const gchar *name)
{
    GError *error = NULL;
    const gchar *key_text;

    g_clear_object(&app.current);
    app.current = gsw_manager_create_module(app.manager, name);
    if (app.current == NULL) {
        set_status("Cannot open module");
        return;
    }
    key_text = gtk_editable_get_text(GTK_EDITABLE(app.key_entry));
    if (key_text != NULL && *key_text != '\0' &&
        !gsw_module_set_key_text(app.current, key_text, NULL))
        gsw_module_set_position(app.current, GSW_POSITION_TOP, NULL);
    else if (key_text == NULL || *key_text == '\0')
        gsw_module_set_position(app.current, GSW_POSITION_TOP, NULL);
    (void)error;
    read_current_key();
}

static void
on_module_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    const gchar *name;

    (void)list;
    (void)user_data;
    if (row == NULL)
        return;
    name = g_object_get_data(G_OBJECT(row), "module-name");
    if (name != NULL)
        load_module(name);
}

static void
on_key_activate(GtkEntry *entry, gpointer user_data)
{
    GError *error = NULL;
    (void)user_data;

    if (app.current == NULL)
        return;
    if (!gsw_module_set_key_text(app.current,
                                 gtk_editable_get_text(GTK_EDITABLE(entry)),
                                 &error)) {
        show_error("Invalid key", error);
        return;
    }
    read_current_key();
}

static void
on_go_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    on_key_activate(app.key_entry, NULL);
}

static void
on_result_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    const gchar *key;
    GError *error = NULL;
    (void)list;
    (void)user_data;

    if (row == NULL || app.current == NULL)
        return;
    key = g_object_get_data(G_OBJECT(row), "result-key");
    if (key == NULL)
        return;
    gtk_editable_set_text(GTK_EDITABLE(app.key_entry), key);
    if (!gsw_module_set_key_text(app.current, key, &error)) {
        show_error("Cannot open search result", error);
        return;
    }
    read_current_key();
}

static void
on_search_clicked(GtkButton *button, gpointer user_data)
{
    GswSearchOptions *options;
    GPtrArray *results;
    GError *error = NULL;
    guint i;
    (void)button;
    (void)user_data;

    clear_list_box(app.result_list);
    if (app.current == NULL)
        return;
    options = gsw_search_options_new();
    gsw_search_options_set_text(options,
        gtk_editable_get_text(GTK_EDITABLE(app.search_entry)));
    gsw_search_options_set_max_results(options, 100);
    results = gsw_module_search_full(app.current, options, NULL, NULL, NULL,
                                     &error);
    gsw_search_options_unref(options);
    if (results == NULL) {
        show_error("Search failed", error);
        return;
    }
    for (i = 0; i < results->len; i++) {
        GswSearchResult *result = g_ptr_array_index(results, i);
        gchar *label = g_strdup_printf("%s\n%s",
            gsw_search_result_get_key(result),
            gsw_search_result_get_snippet(result));
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *text = gtk_label_new(label);

        gtk_label_set_xalign(GTK_LABEL(text), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(text), TRUE);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), text);
        g_object_set_data_full(G_OBJECT(row), "result-key",
                               g_strdup(gsw_search_result_get_key(result)),
                               g_free);
        gtk_list_box_append(app.result_list, row);
        g_free(label);
    }
    {
        gchar *message = g_strdup_printf("Search: %u result(s)", results->len);
        set_status(message);
        g_free(message);
    }
    g_ptr_array_unref(results);
}

static void
on_index_progress(const gchar *module_name, guint processed, guint total,
                  gpointer user_data)
{
    gchar *message;
    (void)module_name;
    (void)user_data;

    message = g_strdup_printf("Indexing: %u/%u", processed, total);
    set_status(message);
    g_free(message);
    while (g_main_context_iteration(NULL, FALSE))
        ;
}

static void
on_build_index_clicked(GtkButton *button, gpointer user_data)
{
    GError *error = NULL;
    (void)button;
    (void)user_data;

    if (app.current == NULL)
        return;
    if (!gsw_module_build_search_index(app.current, NULL, on_index_progress,
                                       NULL, &error)) {
        show_error("Cannot build index", error);
        return;
    }
    set_status("Search index built");
    update_index_status();
}

static void
on_delete_index_clicked(GtkButton *button, gpointer user_data)
{
    GError *error = NULL;
    (void)button;
    (void)user_data;

    if (app.current == NULL)
        return;
    if (!gsw_module_delete_search_index(app.current, &error)) {
        show_error("Cannot delete index", error);
        return;
    }
    set_status("Search index deleted");
    update_index_status();
}

static void
refresh_modules(void)
{
    GPtrArray *infos = gsw_manager_get_module_infos(app.manager);
    guint i;

    clear_list_box(app.module_list);
    gtk_combo_box_text_remove_all(app.parallel_a);
    gtk_combo_box_text_remove_all(app.parallel_b);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);
        const gchar *name = gsw_module_info_get_name(info);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(name);

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "module-name", g_strdup(name),
                               g_free);
        gtk_list_box_append(app.module_list, row);
        gtk_combo_box_text_append_text(app.parallel_a, name);
        gtk_combo_box_text_append_text(app.parallel_b, name);
    }
    if (infos->len > 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(app.parallel_a), 0);
        gtk_combo_box_set_active(GTK_COMBO_BOX(app.parallel_b), 0);
    }
    g_ptr_array_unref(infos);
}

static void
load_parallel_side(GtkComboBoxText *combo, GswModule **module,
                   GtkTextView *view)
{
    gchar *name = gtk_combo_box_text_get_active_text(combo);
    GError *error = NULL;
    gchar *text;

    if (name == NULL)
        return;
    g_clear_object(module);
    *module = gsw_manager_create_module(app.manager, name);
    g_free(name);
    if (*module == NULL)
        return;
    {
        const gchar *key = gtk_editable_get_text(GTK_EDITABLE(app.parallel_key));
        if (key != NULL && *key != '\0')
            gsw_module_set_key_text(*module, key, NULL);
        else
            gsw_module_set_position(*module, GSW_POSITION_TOP, NULL);
    }
    text = gsw_module_get_text(*module, &error);
    if (text == NULL) {
        show_error("Parallel read failed", error);
        return;
    }
    set_text_view(view, text);
    g_free(text);
}

static void
on_parallel_load(GtkButton *button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    load_parallel_side(app.parallel_a, &app.parallel_module_a,
                       app.parallel_view_a);
    load_parallel_side(app.parallel_b, &app.parallel_module_b,
                       app.parallel_view_b);
}

static void
on_install_clicked(GtkButton *button, gpointer user_data)
{
    const gchar *path;
    const gchar *name;
    GError *error = NULL;
    (void)button;
    (void)user_data;

    if (app.installer == NULL) {
        set_status("Installer is not available");
        return;
    }
    name = gtk_editable_get_text(GTK_EDITABLE(app.install_name));
    path = gtk_editable_get_text(GTK_EDITABLE(app.install_dir));
    if (name == NULL || *name == '\0' || path == NULL || *path == '\0') {
        set_status("Enter a local SWORD directory and module name");
        return;
    }
    if (!gsw_install_manager_install_module_from_directory(app.installer,
                                                           app.manager,
                                                           path, name,
                                                           &error)) {
        show_error("Install failed", error);
        return;
    }
    if (!gsw_manager_reload(app.manager, &error)) {
        show_error("Reload failed", error);
        return;
    }
    g_clear_object(&app.current);
    g_clear_object(&app.parallel_module_a);
    g_clear_object(&app.parallel_module_b);
    app.notes_module = NULL;
    refresh_modules();
    set_status("Module installed");
}

static void
open_notes(void)
{
    GError *error = NULL;
    GswKey *key;
    GswTreeKey *tree;
    gchar *text;

    if (!gsw_manager_has_module(app.manager, "GTKNotes")) {
        if (!gsw_manager_create_genbook(app.manager, "GTKNotes",
                                        "GTK frontend notes", &error)) {
            show_error("Cannot create notes module", error);
            return;
        }
        refresh_modules();
    }
    app.notes_module = gsw_manager_get_module(app.manager, "GTKNotes");
    if (app.notes_module == NULL)
        return;
    key = gsw_module_create_key(app.notes_module);
    tree = (GswTreeKey *)key;
    if (!gsw_tree_key_set_text(tree, "Notes")) {
        gsw_key_unref(key);
        key = gsw_module_create_key(app.notes_module);
        tree = (GswTreeKey *)key;
        if (!gsw_tree_key_append_child(tree, "Notes", &error)) {
            show_error("Cannot create notes entry", error);
            gsw_key_unref(key);
            return;
        }
    }
    gsw_module_set_key(app.notes_module, key, NULL);
    gsw_key_unref(key);
    text = gsw_module_get_text(app.notes_module, &error);
    if (text == NULL) {
        show_error("Cannot read notes", error);
        return;
    }
    set_text_view(app.notes_view, text);
    g_free(text);
    set_status("Notes opened");
}

static void
save_notes(void)
{
    GError *error = NULL;
    gchar *text;

    if (app.notes_module == NULL ||
        !gsw_tree_key_set_text((GswTreeKey *)gsw_module_get_key(app.notes_module),
                               "Notes")) {
        set_status("Open notes before saving");
        return;
    }
    text = text_view_contents(app.notes_view);
    if (!gsw_module_set_entry(app.notes_module, text, &error)) {
        show_error("Cannot save notes", error);
        g_free(text);
        return;
    }
    g_free(text);
    set_status("Notes saved");
}

static GtkWidget *
make_scrolled_text_view(GtkTextView **view)
{
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkWidget *text = gtk_text_view_new();

    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text);
    *view = GTK_TEXT_VIEW(text);
    return scrolled;
}

static GtkWidget *
make_library_page(void)
{
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *left = gtk_scrolled_window_new();
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *key_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *go = gtk_button_new_with_label("Go");
    GtkWidget *search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *search = gtk_button_new_with_label("Search");
    GtkWidget *build = gtk_button_new_with_label("Build index");
    GtkWidget *drop = gtk_button_new_with_label("Delete index");
    GtkWidget *reader = make_scrolled_text_view(&app.reader);
    GtkWidget *results_scroll = gtk_scrolled_window_new();

    app.module_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(app.module_list, "row-selected",
                     G_CALLBACK(on_module_selected), NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(left),
                                  GTK_WIDGET(app.module_list));

    app.key_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(app.key_entry, "Genesis 1:1 / dictionary key");
    g_signal_connect(app.key_entry, "activate", G_CALLBACK(on_key_activate),
                     NULL);
    g_signal_connect(go, "clicked", G_CALLBACK(on_go_clicked), NULL);
    gtk_box_append(GTK_BOX(key_box), gtk_label_new("Key"));
    gtk_widget_set_hexpand(GTK_WIDGET(app.key_entry), TRUE);
    gtk_box_append(GTK_BOX(key_box), GTK_WIDGET(app.key_entry));
    gtk_box_append(GTK_BOX(key_box), go);

    app.search_entry = GTK_SEARCH_ENTRY(gtk_search_entry_new());
    gtk_search_entry_set_placeholder_text(app.search_entry, "Search module");
    g_signal_connect(search, "clicked", G_CALLBACK(on_search_clicked), NULL);
    g_signal_connect(build, "clicked", G_CALLBACK(on_build_index_clicked),
                     NULL);
    g_signal_connect(drop, "clicked", G_CALLBACK(on_delete_index_clicked),
                     NULL);
    gtk_widget_set_hexpand(GTK_WIDGET(app.search_entry), TRUE);
    gtk_box_append(GTK_BOX(search_box), GTK_WIDGET(app.search_entry));
    gtk_box_append(GTK_BOX(search_box), search);
    gtk_box_append(GTK_BOX(search_box), build);
    gtk_box_append(GTK_BOX(search_box), drop);

    app.result_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(app.result_list, "row-selected",
                     G_CALLBACK(on_result_selected), NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(results_scroll),
                                  GTK_WIDGET(app.result_list));
    gtk_widget_set_size_request(results_scroll, -1, 140);

    app.index_status = GTK_LABEL(gtk_label_new("Index: not built"));
    gtk_label_set_xalign(app.index_status, 0.0f);

    gtk_widget_set_vexpand(reader, TRUE);
    gtk_box_append(GTK_BOX(right), key_box);
    gtk_box_append(GTK_BOX(right), search_box);
    gtk_box_append(GTK_BOX(right), GTK_WIDGET(app.index_status));
    gtk_box_append(GTK_BOX(right), reader);
    gtk_box_append(GTK_BOX(right), results_scroll);

    gtk_paned_set_start_child(GTK_PANED(paned), left);
    gtk_paned_set_end_child(GTK_PANED(paned), right);
    gtk_paned_set_position(GTK_PANED(paned), 220);
    return paned;
}

static GtkWidget *
make_parallel_page(void)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *load = gtk_button_new_with_label("Load both");
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *left = make_scrolled_text_view(&app.parallel_view_a);
    GtkWidget *right = make_scrolled_text_view(&app.parallel_view_b);

    app.parallel_a = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new());
    app.parallel_b = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new());
    app.parallel_key = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(app.parallel_key, "Shared key");
    gtk_widget_set_hexpand(GTK_WIDGET(app.parallel_key), TRUE);
    g_signal_connect(load, "clicked", G_CALLBACK(on_parallel_load), NULL);

    gtk_box_append(GTK_BOX(controls), GTK_WIDGET(app.parallel_a));
    gtk_box_append(GTK_BOX(controls), GTK_WIDGET(app.parallel_b));
    gtk_box_append(GTK_BOX(controls), GTK_WIDGET(app.parallel_key));
    gtk_box_append(GTK_BOX(controls), load);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_paned_set_start_child(GTK_PANED(paned), left);
    gtk_paned_set_end_child(GTK_PANED(paned), right);
    gtk_box_append(GTK_BOX(box), controls);
    gtk_box_append(GTK_BOX(box), paned);
    return box;
}

static GtkWidget *
make_notes_page(void)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *open = gtk_button_new_with_label("Open notes");
    GtkWidget *save = gtk_button_new_with_label("Save notes");
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkWidget *text = gtk_text_view_new();

    app.notes_view = GTK_TEXT_VIEW(text);
    gtk_text_view_set_wrap_mode(app.notes_view, GTK_WRAP_WORD_CHAR);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text);
    g_signal_connect(open, "clicked", G_CALLBACK(open_notes), NULL);
    g_signal_connect(save, "clicked", G_CALLBACK(save_notes), NULL);
    gtk_box_append(GTK_BOX(controls), open);
    gtk_box_append(GTK_BOX(controls), save);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(box), controls);
    gtk_box_append(GTK_BOX(box), scrolled);
    return box;
}

static GtkWidget *
make_install_page(void)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *install = gtk_button_new_with_label("Install module");
    GtkWidget *hint = gtk_label_new(
        "Choose a local SWORD library directory and enter the module name.");

    app.install_dir = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(app.install_dir,
                                   "/path/to/local/sword/library");
    app.install_name = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(app.install_name, "Module name");
    g_signal_connect(install, "clicked", G_CALLBACK(on_install_clicked), NULL);
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_box_append(GTK_BOX(box), hint);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(app.install_dir));
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(app.install_name));
    gtk_box_append(GTK_BOX(box), install);
    return box;
}

static void
on_activate(GtkApplication *application, gpointer user_data)
{
    GError *error = NULL;
    const gchar *prefix = g_getenv("GSWORD_LIBRARY");
    gchar *default_prefix = NULL;
    gchar *installmgr_config;
    gchar *installmgr_cache;
    GtkWidget *notebook;
    GtkWidget *box;
    (void)user_data;

    if (prefix == NULL || *prefix == '\0') {
        default_prefix = g_build_filename(g_get_user_data_dir(), "gsword",
                                          "library", NULL);
        prefix = default_prefix;
    }
    app.manager = gsw_manager_new_for_path(prefix, &error);
    if (app.manager == NULL) {
        g_printerr("Cannot open module library %s: %s\n", prefix,
                   error->message);
        g_clear_error(&error);
        g_free(default_prefix);
        return;
    }
    g_free(default_prefix);

    installmgr_config = g_build_filename(g_get_user_config_dir(), "gsword",
                                         "installmgr", NULL);
    installmgr_cache = g_build_filename(g_get_user_cache_dir(), "gsword",
                                        "installmgr", NULL);
    app.installer = gsw_install_manager_new(installmgr_config,
                                            installmgr_cache, &error);
    g_free(installmgr_cache);
    g_free(installmgr_config);
    if (app.installer == NULL) {
        g_printerr("Installer unavailable: %s\n", error->message);
        g_clear_error(&error);
    } else {
        gsw_install_manager_set_user_disclaimer_confirmed(app.installer, TRUE);
    }

    app.app = application;
    app.window = gtk_application_window_new(application);
    gtk_window_set_title(GTK_WINDOW(app.window), "gsword GTK prototype");
    gtk_window_set_default_size(GTK_WINDOW(app.window), 1100, 760);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    notebook = gtk_notebook_new();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), make_library_page(),
                             gtk_label_new("Library"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), make_parallel_page(),
                             gtk_label_new("Parallel"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), make_notes_page(),
                             gtk_label_new("Notes"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), make_install_page(),
                             gtk_label_new("Install"));
    gtk_widget_set_vexpand(notebook, TRUE);
    app.status = GTK_LABEL(gtk_label_new("Ready"));
    gtk_label_set_xalign(app.status, 0.0f);
    gtk_box_append(GTK_BOX(box), notebook);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(app.status));
    gtk_window_set_child(GTK_WINDOW(app.window), box);
    gtk_window_present(GTK_WINDOW(app.window));

    refresh_modules();
}

static void
on_shutdown(GApplication *application, gpointer user_data)
{
    (void)application;
    (void)user_data;

    g_clear_object(&app.current);
    app.notes_module = NULL;
    g_clear_object(&app.parallel_module_a);
    g_clear_object(&app.parallel_module_b);
    g_clear_object(&app.installer);
    g_clear_object(&app.manager);
}

int
main(int argc, char **argv)
{
    GtkApplication *application;
    int status;

    application = gtk_application_new("org.gsword.GtkPrototype",
                                      G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(application, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(application, "shutdown", G_CALLBACK(on_shutdown), NULL);
    status = g_application_run(G_APPLICATION(application), argc, argv);
    g_object_unref(application);
    return status;
}
