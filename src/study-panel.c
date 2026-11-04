/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * study-panel.c: commentary, Strong's, cross-reference and lexicon panel
 */

#include "study-panel.h"
#include "window-private.h"

#include <glib/gi18n.h>

struct _GswordStudyPanel {
    GswordWindow *window;
    GtkWidget *root;
    GtkToggleButton *toggle;
    GtkCheckButton *follow_toggle;
    GtkNotebook *notebook;
    GtkLabel *commentary_context;
    gboolean commentary_follow;
    gchar *pinned_commentary_key;
    GtkTextView *commentary_view;
    GtkWidget *commentary_action;
    GtkListBox *strongs_list;
    GtkTextView *strongs_view;
    GtkListBox *crossrefs_list;
    GtkTextView *crossref_view;
    GtkTextView *lexicon_view;
    GtkDropDown *lexicon_selector;
    GtkStringList *lexicon_names;
    GtkEntry *lexicon_entry;
    gchar *commentary_name;
    gchar *lexicon_name;
    GswModule *commentary_module;
    GswModule *lexicon_module;
};

static gchar *
find_module_by_type(GswordWindow *self, GswModuleType type)
{
    GswManager *manager = gsword_window_get_manager(self);
    GPtrArray *infos;
    gchar *result = NULL;
    guint i;

    if (manager == NULL)
        return NULL;
    infos = gsw_manager_get_module_infos(manager);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (gsw_module_info_get_module_type(info) == type) {
            result = g_strdup(gsw_module_info_get_name(info));
            break;
        }
    }
    g_ptr_array_unref(infos);
    return result;
}

static void
ensure_study_modules(GswordStudyPanel *panel)
{
    GswManager *manager = gsword_window_get_manager(panel->window);

    if (panel->commentary_name == NULL)
        panel->commentary_name = find_module_by_type(
            panel->window, GSW_MODULE_TYPE_COMMENTARIES);
    if (panel->lexicon_name == NULL)
        panel->lexicon_name = find_module_by_type(
            panel->window, GSW_MODULE_TYPE_LEXDICTS);
    if (panel->commentary_module == NULL && panel->commentary_name != NULL)
        panel->commentary_module = gsw_manager_create_module(
            manager, panel->commentary_name);
    if (panel->lexicon_module == NULL && panel->lexicon_name != NULL)
        panel->lexicon_module = gsw_manager_create_module(
            manager, panel->lexicon_name);
}

static void
update_study(GswordStudyPanel *panel)
{
    GswordWindow *self = panel->window;
    GswModule *module = gsword_window_get_module(self);
    GError *error = NULL;
    const gchar *key_text;
    const gchar *commentary_key;
    GString *text;

    ensure_study_modules(panel);
    if (module == NULL)
        return;
    key_text = gsw_module_get_key_text(module);
    commentary_key = key_text;
    if (!panel->commentary_follow && panel->pinned_commentary_key != NULL &&
        *panel->pinned_commentary_key != '\0')
        commentary_key = panel->pinned_commentary_key;

    if (panel->commentary_context != NULL) {
        gchar *reference = panel->commentary_follow
                         ? gsword_window_current_reference(self)
                         : g_strdup(commentary_key);
        gchar *title = panel->commentary_name != NULL
                     ? g_strdup_printf("%s — %s", panel->commentary_name,
                                       reference != NULL ? reference : "")
                     : g_strdup(_("Commentary"));

        gtk_label_set_text(panel->commentary_context, title);
        g_free(title);
        g_free(reference);
    }

    if (panel->commentary_module != NULL) {
        gchar *commentary = NULL;

        if (gsw_module_set_key_text(panel->commentary_module, commentary_key,
                                    &error)) {
            commentary = gsw_module_get_text(panel->commentary_module, &error);
        }
        gsword_window_set_view_text(panel->commentary_view,
                                    commentary != NULL
                                    ? commentary
                                    : _("No commentary for this verse."));
        g_free(commentary);
        g_clear_error(&error);
    } else {
        gsword_window_set_view_text(panel->commentary_view,
                                    _("No commentary module is installed."));
    }
    if (panel->commentary_action != NULL)
        gtk_widget_set_visible(panel->commentary_action,
                               panel->commentary_module == NULL);

    gsword_window_clear_list(panel->strongs_list);
    text = g_string_new(NULL);
    {
        GPtrArray *keys = gsw_module_get_entry_attribute_keys(
            module, "Word", "Lemma", &error);

        if (keys != NULL) {
            guint i;

            for (i = 0; i < keys->len; i++) {
                const gchar *key = g_ptr_array_index(keys, i);
                gchar *value = gsw_module_get_entry_attribute(
                    module, "Word", "Lemma", key, &error);

                if (value != NULL) {
                    GtkWidget *row = gtk_list_box_row_new();
                    GtkWidget *label = gtk_label_new(key);

                    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
                    gtk_widget_set_margin_top(label, 2);
                    gtk_widget_set_margin_bottom(label, 2);
                    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
                    g_object_set_data_full(G_OBJECT(row), "strongs-key",
                                           g_strdup(key), g_free);
                    gtk_list_box_append(panel->strongs_list, row);
                    g_string_append_printf(text, "%s: %s\n", key, value);
                    g_free(value);
                }
            }
            g_ptr_array_unref(keys);
        }
        g_clear_error(&error);
    }
    if (text->len == 0)
        g_string_append(text, _("No Strong's data for this verse."));
    gsword_window_set_view_text(panel->strongs_view, text->str);
    g_string_free(text, TRUE);

    gsword_window_clear_list(panel->crossrefs_list);
    text = g_string_new(NULL);
    {
        GPtrArray *keys = gsw_module_get_entry_attribute_keys(
            module, "Crossref", "body", &error);

        if (keys != NULL) {
            guint i;

            for (i = 0; i < keys->len; i++) {
                const gchar *key = g_ptr_array_index(keys, i);
                gchar *value = gsw_module_get_entry_attribute(
                    module, "Crossref", "body", key, &error);

                if (value != NULL) {
                    gchar **parts = g_strsplit(value, ";", -1);
                    guint j;

                    for (j = 0; parts[j] != NULL; j++) {
                        gchar *reference = g_strstrip(parts[j]);

                        if (*reference != '\0') {
                            GtkWidget *row = gtk_list_box_row_new();
                            GtkWidget *label = gtk_label_new(reference);

                            gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
                            gtk_label_set_ellipsize(GTK_LABEL(label),
                                                     PANGO_ELLIPSIZE_END);
                            gtk_widget_set_margin_top(label, 2);
                            gtk_widget_set_margin_bottom(label, 2);
                            gtk_list_box_row_set_child(
                                GTK_LIST_BOX_ROW(row), label);
                            g_object_set_data_full(G_OBJECT(row),
                                                   "crossref-key",
                                                   g_strdup(reference), g_free);
                            gtk_list_box_append(panel->crossrefs_list, row);
                        }
                    }
                    g_strfreev(parts);
                    g_string_append_printf(text, "%s\n", value);
                    g_free(value);
                }
            }
            g_ptr_array_unref(keys);
        }
        g_clear_error(&error);
    }
    if (text->len == 0)
        g_string_append(text, _("No cross-references for this verse."));
    gsword_window_set_view_text(panel->crossref_view, text->str);
    g_string_free(text, TRUE);
}

static void
lexicon_lookup(GswordStudyPanel *panel)
{
    GError *error = NULL;
    const gchar *key;
    gchar *text;

    ensure_study_modules(panel);
    if (panel->lexicon_module == NULL) {
        gsword_window_set_view_text(panel->lexicon_view,
                                    _("No lexicon module is installed."));
        return;
    }
    key = gtk_editable_get_text(GTK_EDITABLE(panel->lexicon_entry));
    if (!gsw_module_set_key_text(panel->lexicon_module, key, &error)) {
        gsword_window_show_error(panel->window,
                                 _("Cannot look up lexicon entry"), error);
        return;
    }
    text = gsw_module_get_text(panel->lexicon_module, &error);
    if (text == NULL) {
        gsword_window_show_error(panel->window,
                                 _("Cannot read lexicon entry"), error);
        return;
    }
    gsword_window_set_view_text(panel->lexicon_view, text);
    g_free(text);
}

void
gsword_study_panel_refresh(GswordStudyPanel *panel)
{
    if (panel != NULL)
        update_study(panel);
}

void
gsword_study_panel_show_commentary(GswordStudyPanel *panel)
{
    if (panel != NULL && panel->notebook != NULL)
        gtk_notebook_set_current_page(panel->notebook, 0);
}

void
gsword_study_panel_show_lexicon(GswordStudyPanel *panel)
{
    if (panel != NULL && panel->notebook != NULL)
        gtk_notebook_set_current_page(panel->notebook, 3);
}

void
gsword_study_panel_set_commentary_module(GswordStudyPanel *panel,
                                         const gchar *name)
{
    GswManager *manager;

    if (panel == NULL)
        return;
    manager = gsword_window_get_manager(panel->window);
    g_free(panel->commentary_name);
    panel->commentary_name = g_strdup(name);
    g_clear_object(&panel->commentary_module);
    if (manager != NULL)
        panel->commentary_module = gsw_manager_create_module(manager, name);
}

void
gsword_study_panel_set_lexicon_module(GswordStudyPanel *panel,
                                      const gchar *name)
{
    GswManager *manager;

    if (panel == NULL)
        return;
    manager = gsword_window_get_manager(panel->window);
    g_free(panel->lexicon_name);
    panel->lexicon_name = g_strdup(name);
    g_clear_object(&panel->lexicon_module);
    if (manager != NULL)
        panel->lexicon_module = gsw_manager_create_module(manager, name);
}

static void
on_study_toggled(GtkToggleButton *button, gpointer user_data)
{
    GswordStudyPanel *panel = user_data;
    GSettings *settings = gsword_window_get_settings(panel->window);
    gboolean active = gtk_toggle_button_get_active(button);

    gtk_widget_set_visible(panel->root, active);
    if (settings != NULL)
        g_settings_set_boolean(settings, "study-visible", active);
}

static void
on_study_follow_toggled(GtkCheckButton *button, gpointer user_data)
{
    GswordStudyPanel *panel = user_data;
    GSettings *settings = gsword_window_get_settings(panel->window);
    GswModule *module = gsword_window_get_module(panel->window);
    gboolean follow = gtk_check_button_get_active(button);

    panel->commentary_follow = follow;
    g_clear_pointer(&panel->pinned_commentary_key, g_free);
    if (!follow) {
        if (panel->commentary_module != NULL)
            panel->pinned_commentary_key = g_strdup(
                gsw_module_get_key_text(panel->commentary_module));
        else if (module != NULL)
            panel->pinned_commentary_key = g_strdup(
                gsw_module_get_key_text(module));
    }
    if (settings != NULL) {
        g_settings_set_boolean(settings, "study-follow", follow);
        g_settings_set_string(settings, "study-pinned-key",
                              panel->pinned_commentary_key != NULL
                              ? panel->pinned_commentary_key : "");
    }
    update_study(panel);
}

void
gsword_study_panel_bind_toggle(GswordStudyPanel *panel, GtkToggleButton *toggle)
{
    if (panel == NULL || toggle == NULL)
        return;
    panel->toggle = toggle;
    g_signal_connect(toggle, "toggled", G_CALLBACK(on_study_toggled), panel);
}

void
gsword_study_panel_set_visible(GswordStudyPanel *panel, gboolean visible)
{
    if (panel == NULL)
        return;
    if (panel->toggle != NULL)
        gtk_toggle_button_set_active(panel->toggle, visible);
    gtk_widget_set_visible(panel->root, visible);
}

void
gsword_study_panel_set_follow(GswordStudyPanel *panel, gboolean follow)
{
    if (panel != NULL && panel->follow_toggle != NULL)
        gtk_check_button_set_active(panel->follow_toggle, follow);
}

static void
on_lexicon_lookup(GtkButton *button, gpointer user_data)
{
    (void)button;
    lexicon_lookup(user_data);
}

static void
on_lexicon_selected(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordStudyPanel *panel = user_data;
    GswManager *manager;
    GSettings *settings;
    guint selected = gtk_drop_down_get_selected(panel->lexicon_selector);
    const gchar *name;

    (void)object;
    (void)pspec;
    if (panel->lexicon_names == NULL ||
        selected == GTK_INVALID_LIST_POSITION)
        return;
    name = gtk_string_list_get_string(panel->lexicon_names, selected);
    if (name == NULL || g_strcmp0(name, panel->lexicon_name) == 0)
        return;
    manager = gsword_window_get_manager(panel->window);
    settings = gsword_window_get_settings(panel->window);
    g_free(panel->lexicon_name);
    panel->lexicon_name = g_strdup(name);
    g_clear_object(&panel->lexicon_module);
    if (manager != NULL)
        panel->lexicon_module = gsw_manager_create_module(manager, name);
    if (settings != NULL)
        g_settings_set_string(settings, "study-lexicon", name);
    gsword_window_set_view_text(panel->lexicon_view, "");
}

void
gsword_study_panel_refresh_lexicons(GswordStudyPanel *panel)
{
    GswManager *manager;
    GPtrArray *infos;
    GtkStringList *names;
    guint selected = 0;
    gboolean found = FALSE;
    guint count = 0;
    guint i;

    if (panel == NULL || panel->lexicon_selector == NULL)
        return;
    manager = gsword_window_get_manager(panel->window);
    if (manager == NULL)
        return;
    infos = gsw_manager_get_module_infos(manager);
    names = gtk_string_list_new(NULL);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (gsw_module_info_get_module_type(info) !=
                GSW_MODULE_TYPE_LEXDICTS ||
            gsword_window_module_info_is_devotional(info) ||
            gsword_window_module_info_is_map(info))
            continue;
        gtk_string_list_append(names, gsw_module_info_get_name(info));
        if (panel->lexicon_name != NULL &&
            g_strcmp0(panel->lexicon_name,
                      gsw_module_info_get_name(info)) == 0) {
            selected = count;
            found = TRUE;
        }
        count++;
    }
    g_ptr_array_unref(infos);
    g_clear_object(&panel->lexicon_names);
    panel->lexicon_names = names;
    gtk_drop_down_set_model(panel->lexicon_selector, G_LIST_MODEL(names));
    if (count > 0) {
        if (!found)
            selected = 0;
        gtk_drop_down_set_selected(panel->lexicon_selector, selected);
    }
}

static void
on_strongs_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordStudyPanel *panel = user_data;
    const gchar *key;

    (void)list;
    if (row == NULL)
        return;
    key = g_object_get_data(G_OBJECT(row), "strongs-key");
    if (key == NULL)
        return;
    gtk_editable_set_text(GTK_EDITABLE(panel->lexicon_entry), key);
    lexicon_lookup(panel);
    gsword_study_panel_show_lexicon(panel);
}

static void
on_crossref_selected(GtkListBox *list, GtkListBoxRow *row,
                     gpointer user_data)
{
    GswordStudyPanel *panel = user_data;
    const gchar *reference;

    (void)list;
    if (row == NULL)
        return;
    reference = g_object_get_data(G_OBJECT(row), "crossref-key");
    if (reference != NULL)
        gsword_window_open_reference(panel->window, reference);
}

static void
study_panel_free(gpointer data)
{
    GswordStudyPanel *panel = data;

    g_clear_object(&panel->commentary_module);
    g_clear_object(&panel->lexicon_module);
    g_clear_object(&panel->lexicon_names);
    g_free(panel->commentary_name);
    g_free(panel->lexicon_name);
    g_free(panel->pinned_commentary_key);
    g_free(panel);
}

static GtkWidget *
make_scrolled_text(GtkTextView **view)
{
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkWidget *text = gtk_text_view_new();

    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 10);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(text), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(text), 10);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text);
    *view = GTK_TEXT_VIEW(text);
    return scrolled;
}

GtkWidget *
gsword_study_panel_widget(GswordStudyPanel *panel)
{
    g_return_val_if_fail(panel != NULL, NULL);
    return panel->root;
}

void
gsword_study_panel_load_settings(GswordStudyPanel *panel)
{
    GSettings *settings;
    gchar *study_module;
    gchar *study_lexicon;
    gchar *pinned_key;
    gboolean follow;
    gboolean visible;

    if (panel == NULL)
        return;
    settings = gsword_window_get_settings(panel->window);
    if (settings == NULL)
        return;
    study_module = g_settings_get_string(settings, "study-module");
    if (study_module != NULL && *study_module != '\0') {
        g_free(panel->commentary_name);
        panel->commentary_name = g_strdup(study_module);
    }
    study_lexicon = g_settings_get_string(settings, "study-lexicon");
    if (study_lexicon != NULL && *study_lexicon != '\0') {
        g_free(panel->lexicon_name);
        panel->lexicon_name = g_strdup(study_lexicon);
    }
    follow = g_settings_get_boolean(settings, "study-follow");
    visible = g_settings_get_boolean(settings, "study-visible");
    pinned_key = g_settings_get_string(settings, "study-pinned-key");
    panel->commentary_follow = follow;
    if (panel->follow_toggle != NULL)
        gtk_check_button_set_active(panel->follow_toggle, follow);
    if (panel->toggle != NULL)
        gtk_toggle_button_set_active(panel->toggle, visible);
    if (!follow && pinned_key != NULL && *pinned_key != '\0') {
        g_free(panel->pinned_commentary_key);
        panel->pinned_commentary_key = g_strdup(pinned_key);
        g_settings_set_string(settings, "study-pinned-key", pinned_key);
    }
    g_free(study_module);
    g_free(study_lexicon);
    g_free(pinned_key);
}

static void
on_manage_modules_clicked(GtkButton *button, gpointer user_data)
{
    GswordStudyPanel *panel = user_data;

    (void)button;
    gsword_window_show_page(panel->window, "library");
}

GswordStudyPanel *
gsword_study_panel_new(GswordWindow *window)
{
    GswordStudyPanel *panel = g_new0(GswordStudyPanel, 1);
    GtkWidget *notebook = gtk_notebook_new();
    GtkWidget *commentary = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *commentary_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *study_follow = gtk_check_button_new_with_label(_("Follow"));
    GtkWidget *commentary_action = gtk_button_new_with_label(
        _("Manage modules"));
    GtkWidget *commentary_scroll;
    GtkWidget *strongs = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *strongs_scroll;
    GtkWidget *strongs_list_scroll = gtk_scrolled_window_new();
    GtkWidget *crossrefs = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *crossrefs_scroll;
    GtkWidget *crossrefs_list_scroll = gtk_scrolled_window_new();
    GtkWidget *lexicon_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *lexicon_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *lexicon_module_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *lexicon_button = gtk_button_new_with_label(_("Lookup"));
    GtkWidget *lexicon;

    panel->window = window;
    panel->root = notebook;
    gtk_widget_set_visible(notebook, FALSE);

    commentary_scroll = make_scrolled_text(&panel->commentary_view);
    gtk_widget_set_vexpand(commentary_scroll, TRUE);
    panel->commentary_context = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(panel->commentary_context, 0.0f);
    gtk_label_set_ellipsize(panel->commentary_context, PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(GTK_WIDGET(panel->commentary_context),
                             "dim-label");
    gtk_widget_set_margin_start(GTK_WIDGET(panel->commentary_context), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(panel->commentary_context), 6);
    gtk_widget_set_hexpand(GTK_WIDGET(panel->commentary_context), TRUE);
    panel->follow_toggle = GTK_CHECK_BUTTON(study_follow);
    gtk_check_button_set_active(panel->follow_toggle, TRUE);
    g_signal_connect(study_follow, "toggled",
                     G_CALLBACK(on_study_follow_toggled), panel);
    gtk_box_append(GTK_BOX(commentary_header),
                   GTK_WIDGET(panel->commentary_context));
    gtk_box_append(GTK_BOX(commentary_header), study_follow);
    panel->commentary_action = commentary_action;
    gtk_widget_add_css_class(commentary_action, "suggested-action");
    gtk_widget_set_halign(commentary_action, GTK_ALIGN_START);
    gtk_widget_set_margin_start(commentary_action, 6);
    gtk_widget_set_margin_end(commentary_action, 6);
    gtk_widget_set_visible(commentary_action, FALSE);
    g_signal_connect(commentary_action, "clicked",
                     G_CALLBACK(on_manage_modules_clicked), panel);
    gtk_box_append(GTK_BOX(commentary), commentary_header);
    gtk_box_append(GTK_BOX(commentary), commentary_action);
    gtk_box_append(GTK_BOX(commentary), commentary_scroll);
    strongs_scroll = make_scrolled_text(&panel->strongs_view);
    gtk_widget_set_vexpand(strongs_scroll, TRUE);
    panel->strongs_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(panel->strongs_list, "row-selected",
                     G_CALLBACK(on_strongs_selected), panel);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(strongs_list_scroll),
                                  GTK_WIDGET(panel->strongs_list));
    gtk_widget_set_size_request(strongs_list_scroll, -1, 110);
    gtk_box_append(GTK_BOX(strongs), strongs_list_scroll);
    gtk_box_append(GTK_BOX(strongs), strongs_scroll);
    crossrefs_scroll = make_scrolled_text(&panel->crossref_view);
    gtk_widget_set_vexpand(crossrefs_scroll, TRUE);
    panel->crossrefs_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(panel->crossrefs_list, "row-selected",
                     G_CALLBACK(on_crossref_selected), panel);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(crossrefs_list_scroll),
                                  GTK_WIDGET(panel->crossrefs_list));
    gtk_widget_set_size_request(crossrefs_list_scroll, -1, 110);
    gtk_box_append(GTK_BOX(crossrefs), crossrefs_list_scroll);
    gtk_box_append(GTK_BOX(crossrefs), crossrefs_scroll);
    lexicon = make_scrolled_text(&panel->lexicon_view);
    panel->lexicon_selector = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    gtk_widget_set_hexpand(GTK_WIDGET(panel->lexicon_selector), TRUE);
    g_signal_connect(panel->lexicon_selector, "notify::selected",
                     G_CALLBACK(on_lexicon_selected), panel);
    gtk_box_append(GTK_BOX(lexicon_module_row), gtk_label_new(_("Lexicon")));
    gtk_box_append(GTK_BOX(lexicon_module_row),
                   GTK_WIDGET(panel->lexicon_selector));
    gtk_box_append(GTK_BOX(lexicon_box), lexicon_module_row);
    panel->lexicon_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(panel->lexicon_entry,
                                   _("Strong's / dictionary key"));
    gtk_widget_set_hexpand(GTK_WIDGET(panel->lexicon_entry), TRUE);
    g_signal_connect(panel->lexicon_entry, "activate",
                     G_CALLBACK(on_lexicon_lookup), panel);
    g_signal_connect(lexicon_button, "clicked",
                     G_CALLBACK(on_lexicon_lookup), panel);
    gtk_box_append(GTK_BOX(lexicon_controls),
                   GTK_WIDGET(panel->lexicon_entry));
    gtk_box_append(GTK_BOX(lexicon_controls), lexicon_button);
    gtk_box_append(GTK_BOX(lexicon_box), lexicon_controls);
    gtk_widget_set_vexpand(lexicon, TRUE);
    gtk_box_append(GTK_BOX(lexicon_box), lexicon);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), commentary,
                             gtk_label_new(_("Commentary")));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), strongs,
                             gtk_label_new(_("Strong's")));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), crossrefs,
                             gtk_label_new(_("Cross-refs")));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), lexicon_box,
                             gtk_label_new(_("Lexicon")));
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    gtk_widget_set_size_request(notebook, 300, -1);
    panel->notebook = GTK_NOTEBOOK(notebook);

    g_object_set_data_full(G_OBJECT(notebook), "gsword-study-panel", panel,
                           study_panel_free);
    return panel;
}
