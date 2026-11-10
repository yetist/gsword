/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * window-session.c: page restore and window session signals
 */

#include "window-internal.h"

static void
on_workspace_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_private(self);
    const gchar *name = adw_view_stack_get_visible_child_name(
        ADW_VIEW_STACK(object));

    (void)pspec;
    if (priv->settings != NULL && name != NULL && priv->page_restored) {
        gchar *page_id = gsword_window_current_page_id(self);

        g_settings_set_string(priv->settings, "active-page", page_id);
        g_free(page_id);
    }
    if (priv->reader_page != NULL && g_strcmp0(name, "reader") != 0)
        gsword_reader_page_set_focus_mode(priv->reader_page, FALSE);
    if (priv->reader_page != NULL && g_strcmp0(name, "reader") == 0)
        gsword_reader_page_update_favorite(priv->reader_page);
    if (g_strcmp0(name, "compare") == 0) {
        gsword_workspace_page_sync_from_reader(priv->workspace_page);
    } else if (g_strcmp0(name, "search") == 0) {
        if (priv->search_page != NULL)
            gsword_search_page_select_current(priv->search_page);
    } else if (g_strcmp0(name, "personal") == 0 &&
               priv->personal_page != NULL) {
        gsword_window_refresh_personal_section(
            self, gsword_personal_page_get_section(priv->personal_page));
    } else if (g_strcmp0(name, "teaching") == 0) {
        if (priv->teaching_page != NULL)
            gsword_teaching_page_load(priv->teaching_page);
    } else if (g_strcmp0(name, "library") == 0) {
        if (priv->library_page != NULL) {
            gsword_library_page_refresh_sources(priv->library_page);
            gsword_library_page_refresh_installed(priv->library_page);
            gsword_library_page_refresh_remote(priv->library_page);
        }
    }
}

void
gsword_window_restore_page(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_private(self);
    gchar *page = NULL;

    if (priv->settings != NULL)
        page = g_settings_get_string(priv->settings, "active-page");
    if (g_strcmp0(page, "parallel") == 0) {
        gsword_workspace_page_apply_parallel_template(priv->workspace_page);
        gsword_window_show_page(self, "compare");
    } else if (page != NULL &&
               (g_strcmp0(page, "reader") == 0 ||
                g_strcmp0(page, "compare") == 0 ||
                g_strcmp0(page, "panes") == 0 ||
                g_strcmp0(page, "search") == 0 ||
                g_strcmp0(page, "notes") == 0 ||
                g_strcmp0(page, "plan") == 0 ||
                g_strcmp0(page, "personal") == 0 ||
                g_str_has_prefix(page, "personal/") ||
                g_strcmp0(page, "library") == 0 ||
                g_strcmp0(page, "teaching") == 0)) {
        gsword_window_show_page(self, page);
    }
    priv->page_restored = TRUE;
    if (priv->settings != NULL) {
        gchar *page_id = gsword_window_current_page_id(self);

        g_settings_set_string(priv->settings, "active-page", page_id);
        g_free(page_id);
    }
    g_free(page);
}

static void
on_book_name_language_changed(GSettings *settings, const gchar *key,
                              gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_private(self);

    (void)settings;
    (void)key;
    if (priv->module != NULL)
        gsword_window_apply_book_name_locale(self, priv->module);
    if (priv->reader_page != NULL) {
        gsword_reader_page_sync_context(priv->reader_page);
        gsword_reader_page_refresh(priv->reader_page);
    }
    if (priv->workspace_page != NULL)
        gsword_workspace_page_apply_book_name_policy(priv->workspace_page);
}

void
gsword_window_connect_session(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_private(self);

    g_signal_connect(priv->content_stack, "notify::visible-child",
                     G_CALLBACK(on_workspace_changed), self);
    if (priv->settings != NULL)
        g_signal_connect(priv->settings, "changed::book-name-language",
                         G_CALLBACK(on_book_name_language_changed), self);
}
