/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * window-pages.c: page construction and ownership for the gsword window
 */

#include "window-internal.h"

#include <glib/gi18n.h>

void
gsword_window_build_pages(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_private(self);

    priv->search_page = gsword_search_page_new(self);
    priv->teaching_page = gsword_teaching_page_new(self);
    priv->resource_page = gsword_resource_page_new(self);
    priv->notes_page = gsword_notes_page_new(self);
    priv->plan_page = gsword_plan_page_new(self);
    priv->today_page = gsword_today_page_new(self);
    priv->prayer_page = gsword_prayer_page_new(self);
    priv->collections_page = gsword_collections_page_new(self);
    priv->personal_page = gsword_personal_page_new(
        self, gsword_today_page_widget(priv->today_page),
        gsword_notes_page_widget(priv->notes_page),
        gsword_prayer_page_widget(priv->prayer_page),
        gsword_plan_page_widget(priv->plan_page),
        gsword_collections_page_widget(priv->collections_page));
    priv->library_page = gsword_library_page_new(self);

    priv->content_stack = ADW_VIEW_STACK(adw_view_stack_new());
    adw_view_stack_set_hhomogeneous(priv->content_stack, FALSE);
    adw_view_stack_set_vhomogeneous(priv->content_stack, FALSE);

    priv->reader_page = gsword_reader_page_new(self);
    adw_view_stack_add_titled_with_icon(
        priv->content_stack, gsword_reader_page_widget(priv->reader_page),
        "reader", _("Reading"), "x-office-document-symbolic");

    priv->workspace_page = gsword_workspace_page_new(self);
    adw_view_stack_add_titled_with_icon(
        priv->content_stack,
        gsword_workspace_page_widget(priv->workspace_page),
        "compare", _("Compare"), "view-dual-symbolic");

    {
        AdwViewStackPage *page;

        page = adw_view_stack_add_named(
            priv->content_stack,
            gsword_search_page_widget(priv->search_page), "search");
        adw_view_stack_page_set_visible(page, FALSE);
    }

    adw_view_stack_add_titled_with_icon(
        priv->content_stack,
        gsword_personal_page_widget(priv->personal_page),
        "personal", _("Personal"), "avatar-default-symbolic");

    {
        AdwViewStackPage *page;

        page = adw_view_stack_add_named(
            priv->content_stack,
            gsword_teaching_page_widget(priv->teaching_page), "teaching");
        adw_view_stack_page_set_visible(page, FALSE);
        page = adw_view_stack_add_named(
            priv->content_stack,
            gsword_resource_page_widget(priv->resource_page), "resource");
        adw_view_stack_page_set_visible(page, FALSE);
    }

    {
        AdwViewStackPage *page;

        page = adw_view_stack_add_named(
            priv->content_stack,
            gsword_library_page_widget(priv->library_page), "library");
        adw_view_stack_page_set_visible(page, FALSE);
    }

    adw_view_stack_set_visible_child_name(priv->content_stack, "reader");
}

void
gsword_window_dispose_pages(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_private(self);

    g_clear_object(&priv->today_page);
    g_clear_object(&priv->search_page);
    g_clear_object(&priv->teaching_page);
    g_clear_object(&priv->personal_page);
    g_clear_object(&priv->prayer_page);
    g_clear_object(&priv->resource_page);
    g_clear_object(&priv->plan_page);
    g_clear_object(&priv->notes_page);
    g_clear_object(&priv->collections_page);
    g_clear_object(&priv->workspace_page);
    g_clear_object(&priv->reader_page);
    g_clear_object(&priv->library_page);
}
