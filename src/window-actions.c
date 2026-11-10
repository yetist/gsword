/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * window-actions.c: window actions, accelerators, and action callbacks
 */

#include "window-internal.h"
#include "window-help.h"
#include "study-panel.h"
#include "preferences-dialog.h"

#include <glib/gi18n.h>

static void
on_preferences_action(GSimpleAction *action, GVariant *parameter,
                      gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_preferences_show(GSWORD_WINDOW(user_data));
}

static void
on_focus_key_action(GSimpleAction *action, GVariant *parameter,
                    gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_focus_reference(GSWORD_WINDOW(user_data));
}

static void
on_history_back_action(GSimpleAction *action, GVariant *parameter,
                       gpointer user_data)
{
    GswordWindowPrivate *priv = gsword_window_get_private(
        GSWORD_WINDOW(user_data));

    (void)action;
    (void)parameter;
    if (priv->reader_page != NULL)
        gsword_reader_page_history_back(priv->reader_page);
}

static void
on_history_forward_action(GSimpleAction *action, GVariant *parameter,
                          gpointer user_data)
{
    GswordWindowPrivate *priv = gsword_window_get_private(
        GSWORD_WINDOW(user_data));

    (void)action;
    (void)parameter;
    if (priv->reader_page != NULL)
        gsword_reader_page_history_forward(priv->reader_page);
}

static void
gsword_window_apply_workspace(GswordWindow *self, const gchar *workspace)
{
    GswordWindowPrivate *priv = gsword_window_get_private(self);
    GswordStudyPanel *panel = priv->reader_page != NULL
                            ? gsword_reader_page_get_study_panel(
                                  priv->reader_page) : NULL;

    if (g_strcmp0(workspace, "reading") == 0) {
        if (panel != NULL)
            gsword_study_panel_set_visible(panel, FALSE);
        gsword_window_show_page(self, "reader");
    } else if (g_strcmp0(workspace, "study") == 0) {
        if (panel != NULL) {
            gsword_study_panel_set_visible(panel, TRUE);
            gsword_study_panel_set_follow(panel, TRUE);
            gsword_study_panel_show_commentary(panel);
            gsword_study_panel_refresh(panel);
        }
        gsword_window_show_page(self, "reader");
    } else if (g_strcmp0(workspace, "panes") == 0) {
        gsword_window_show_page(self, "compare");
        gsword_workspace_page_sync_from_reader(priv->workspace_page);
    } else if (g_strcmp0(workspace, "parallel") == 0) {
        gsword_workspace_page_apply_parallel_template(priv->workspace_page);
    } else if (g_strcmp0(workspace, "teaching") == 0) {
        if (priv->teaching_page != NULL)
            gsword_teaching_page_load(priv->teaching_page);
    }
}

static void
on_study_panel_action(GSimpleAction *action, GVariant *parameter,
                      gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_apply_workspace(GSWORD_WINDOW(user_data), "study");
}

static void
on_compare_action(GSimpleAction *action, GVariant *parameter,
                  gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_apply_workspace(GSWORD_WINDOW(user_data), "panes");
}

static void
on_teaching_action(GSimpleAction *action, GVariant *parameter,
                   gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_apply_workspace(GSWORD_WINDOW(user_data), "teaching");
}

static void
on_open_prayer_action(GSimpleAction *action, GVariant *parameter,
                      gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_show_personal(GSWORD_WINDOW(user_data),
                                GSWORD_PERSONAL_SECTION_PRAYER);
}

static void
on_open_bookmarks_action(GSimpleAction *action, GVariant *parameter,
                         gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_private(self);

    (void)action;
    (void)parameter;
    gsword_window_show_personal(self, GSWORD_PERSONAL_SECTION_COLLECTIONS);
    if (priv->collections_page != NULL)
        gsword_collections_page_show_bookmarks(priv->collections_page);
}

static void
on_open_note_action(GSimpleAction *action, GVariant *parameter,
                    gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_open_current_note(GSWORD_WINDOW(user_data));
}

static void
switch_main_page(GswordWindow *self, gint direction)
{
    GswordWindowPrivate *priv = gsword_window_get_private(self);
    const gchar *pages[] = { "reader", "compare", "personal" };
    const gchar *current = adw_view_stack_get_visible_child_name(
        priv->content_stack);
    gint index = 0;
    guint i;

    for (i = 0; i < G_N_ELEMENTS(pages); i++) {
        if (g_strcmp0(current, pages[i]) == 0) {
            index = (gint)i;
            break;
        }
    }
    index = (index + direction + (gint)G_N_ELEMENTS(pages)) %
            (gint)G_N_ELEMENTS(pages);
    gsword_window_show_page(self, pages[index]);
}

static void
on_next_page_action(GSimpleAction *action, GVariant *parameter,
                    gpointer user_data)
{
    (void)action;
    (void)parameter;
    switch_main_page(GSWORD_WINDOW(user_data), 1);
}

static void
on_previous_page_action(GSimpleAction *action, GVariant *parameter,
                        gpointer user_data)
{
    (void)action;
    (void)parameter;
    switch_main_page(GSWORD_WINDOW(user_data), -1);
}

static void
on_shortcuts_action(GSimpleAction *action, GVariant *parameter,
                    gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_show_shortcuts(GSWORD_WINDOW(user_data));
}

static void
on_about_action(GSimpleAction *action, GVariant *parameter,
                gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_show_about(GSWORD_WINDOW(user_data));
}

void
gsword_window_open_search(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_private(self);

    if (!gsword_window_is_page_visible(self, "search")) {
        g_free(priv->search_return_page);
        priv->search_return_page = gsword_window_current_page_id(self);
    }
    gsword_window_show_page(self, "search");
    if (priv->search_page != NULL)
        gsword_search_page_focus(priv->search_page);
}

static void
on_library_action(GSimpleAction *action, GVariant *parameter,
                  gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_open_library(GSWORD_WINDOW(user_data));
}

static void
on_search_action(GSimpleAction *action, GVariant *parameter,
                 gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_open_search(GSWORD_WINDOW(user_data));
}

static void
on_toggle_sidebar_action(GSimpleAction *action, GVariant *parameter,
                         gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_toggle_sidebar(GSWORD_WINDOW(user_data));
}

static GSimpleAction *
add_action(GswordWindow *self, const gchar *name, GCallback callback)
{
    GSimpleAction *action = g_simple_action_new(name, NULL);

    g_signal_connect(action, "activate", callback, self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(action));
    return action;
}

void
gsword_window_install_actions(GswordWindow *self)
{
    GSimpleAction *action;

    action = add_action(self, "preferences",
                        G_CALLBACK(on_preferences_action));
    g_object_unref(action);
    action = add_action(self, "focus-key", G_CALLBACK(on_focus_key_action));
    g_object_unref(action);
    action = add_action(self, "search", G_CALLBACK(on_search_action));
    g_object_unref(action);
    action = add_action(self, "library", G_CALLBACK(on_library_action));
    g_object_unref(action);

    action = add_action(self, "study-panel",
                        G_CALLBACK(on_study_panel_action));
    g_object_unref(action);
    action = add_action(self, "compare", G_CALLBACK(on_compare_action));
    g_object_unref(action);
    action = add_action(self, "teaching", G_CALLBACK(on_teaching_action));
    g_object_unref(action);
    action = add_action(self, "shortcuts", G_CALLBACK(on_shortcuts_action));
    g_object_unref(action);
    action = add_action(self, "about", G_CALLBACK(on_about_action));
    g_object_unref(action);

    action = add_action(self, "open-prayer",
                        G_CALLBACK(on_open_prayer_action));
    g_object_unref(action);
    action = add_action(self, "open-bookmarks",
                        G_CALLBACK(on_open_bookmarks_action));
    g_object_unref(action);
    action = add_action(self, "open-note", G_CALLBACK(on_open_note_action));
    g_object_unref(action);
    action = add_action(self, "next-page", G_CALLBACK(on_next_page_action));
    g_object_unref(action);
    action = add_action(self, "previous-page",
                        G_CALLBACK(on_previous_page_action));
    g_object_unref(action);

    action = add_action(self, "history-back",
                        G_CALLBACK(on_history_back_action));
    g_object_unref(action);
    action = add_action(self, "history-forward",
                        G_CALLBACK(on_history_forward_action));
    g_object_unref(action);
    action = add_action(self, "toggle-sidebar",
                        G_CALLBACK(on_toggle_sidebar_action));
    g_object_unref(action);

    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.preferences",
        (const gchar * const[]) { "<Control>comma", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.focus-key",
        (const gchar * const[]) { "<Control>l", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.search",
        (const gchar * const[]) { "<Control>f", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.library",
        (const gchar * const[]) { "<Control><Shift>i", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.history-back",
        (const gchar * const[]) { "<Alt>Left", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.history-forward",
        (const gchar * const[]) { "<Alt>Right", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.toggle-sidebar",
        (const gchar * const[]) { "F9", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.open-note",
        (const gchar * const[]) { "<Control><Shift>n", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.open-prayer",
        (const gchar * const[]) { "<Control><Shift>p", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.open-bookmarks",
        (const gchar * const[]) { "<Control><Shift>b", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.previous-page",
        (const gchar * const[]) { "<Control>Page_Up", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.next-page",
        (const gchar * const[]) { "<Control>Page_Down", NULL });
}
