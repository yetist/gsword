/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * personal-page.c: personal center shell for the gsword window
 */

#include "personal-page.h"

#include <glib/gi18n.h>

struct _GswordPersonalPage {
    GswordWindow *window;
    GtkWidget *root;
    AdwViewStack *stack;
    GtkWidget *switcher;
    GtkWidget *switcher_bar;
    GswordPersonalSectionChangedFunc section_changed;
    gpointer section_changed_data;
    gboolean switching;
};

static const gchar *
section_name(GswordPersonalSection section)
{
    switch (section) {
    case GSWORD_PERSONAL_SECTION_NOTES:
        return "notes";
    case GSWORD_PERSONAL_SECTION_PRAYER:
        return "prayer";
    case GSWORD_PERSONAL_SECTION_PLANS:
        return "plans";
    case GSWORD_PERSONAL_SECTION_COLLECTIONS:
        return "collections";
    case GSWORD_PERSONAL_SECTION_TODAY:
    default:
        return "today";
    }
}

static GswordPersonalSection
section_from_name(const gchar *name)
{
    if (g_strcmp0(name, "notes") == 0)
        return GSWORD_PERSONAL_SECTION_NOTES;
    if (g_strcmp0(name, "prayer") == 0)
        return GSWORD_PERSONAL_SECTION_PRAYER;
    if (g_strcmp0(name, "plans") == 0)
        return GSWORD_PERSONAL_SECTION_PLANS;
    if (g_strcmp0(name, "collections") == 0)
        return GSWORD_PERSONAL_SECTION_COLLECTIONS;
    return GSWORD_PERSONAL_SECTION_TODAY;
}

static void
on_visible_child_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordPersonalPage *page = user_data;
    const gchar *name;
    GswordPersonalSection section;

    (void)object;
    (void)pspec;
    if (page->switching || page->section_changed == NULL)
        return;
    name = adw_view_stack_get_visible_child_name(page->stack);
    section = section_from_name(name);
    page->section_changed(page, section, page->section_changed_data);
}

static void
personal_page_free(gpointer data)
{
    g_free(data);
}

GswordPersonalPage *
gsword_personal_page_new(GswordWindow *window, GtkWidget *today_widget,
                         GtkWidget *notes_widget, GtkWidget *prayer_widget,
                         GtkWidget *plans_widget,
                         GtkWidget *collections_widget)
{
    GswordPersonalPage *page = g_new0(GswordPersonalPage, 1);
    AdwToolbarView *toolbar_view;
    GtkWidget *switcher_box;

    g_return_val_if_fail(GSWORD_IS_WINDOW(window), NULL);
    g_return_val_if_fail(today_widget != NULL && notes_widget != NULL &&
                         prayer_widget != NULL && plans_widget != NULL &&
                         collections_widget != NULL, NULL);

    page->window = window;
    toolbar_view = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
    page->stack = ADW_VIEW_STACK(adw_view_stack_new());

    adw_view_stack_add_titled_with_icon(page->stack, today_widget, "today",
                                        _("Today"), "x-office-calendar-symbolic");
    adw_view_stack_add_titled_with_icon(page->stack, notes_widget, "notes",
                                        _("Notes"),
                                        "accessories-text-editor-symbolic");
    adw_view_stack_add_titled_with_icon(page->stack, prayer_widget, "prayer",
                                        _("Prayer"),
                                        "emblem-favorite-symbolic");
    adw_view_stack_add_titled_with_icon(page->stack, plans_widget, "plans",
                                        _("Plans"),
                                        "x-office-calendar-symbolic");
    adw_view_stack_add_titled_with_icon(page->stack, collections_widget,
                                        "collections", _("Collections"),
                                        "user-bookmarks-symbolic");
    adw_view_stack_set_visible_child_name(page->stack, "today");

    page->switcher = adw_view_switcher_new();
    adw_view_switcher_set_stack(ADW_VIEW_SWITCHER(page->switcher), page->stack);
    adw_view_switcher_set_policy(ADW_VIEW_SWITCHER(page->switcher),
                                 ADW_VIEW_SWITCHER_POLICY_WIDE);
    switcher_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(switcher_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(switcher_box, 6);
    gtk_widget_set_margin_bottom(switcher_box, 6);
    gtk_box_append(GTK_BOX(switcher_box), page->switcher);
    adw_toolbar_view_add_top_bar(toolbar_view, switcher_box);

    page->switcher_bar = adw_view_switcher_bar_new();
    adw_view_switcher_bar_set_stack(ADW_VIEW_SWITCHER_BAR(page->switcher_bar),
                                    page->stack);
    adw_view_switcher_bar_set_reveal(
        ADW_VIEW_SWITCHER_BAR(page->switcher_bar), FALSE);
    adw_toolbar_view_add_bottom_bar(toolbar_view, page->switcher_bar);

    gtk_widget_set_vexpand(GTK_WIDGET(page->stack), TRUE);
    adw_toolbar_view_set_content(toolbar_view, GTK_WIDGET(page->stack));
    page->root = GTK_WIDGET(toolbar_view);

    g_signal_connect(page->stack, "notify::visible-child",
                     G_CALLBACK(on_visible_child_changed), page);
    g_object_set_data_full(G_OBJECT(page->root), "gsword-personal-page", page,
                           personal_page_free);
    return page;
}

GtkWidget *
gsword_personal_page_widget(GswordPersonalPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}

GtkWidget *
gsword_personal_page_switcher(GswordPersonalPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->switcher;
}

GtkWidget *
gsword_personal_page_switcher_bar(GswordPersonalPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->switcher_bar;
}

void
gsword_personal_page_show_section(GswordPersonalPage *page,
                                  GswordPersonalSection section)
{
    g_return_if_fail(page != NULL);

    page->switching = TRUE;
    adw_view_stack_set_visible_child_name(page->stack, section_name(section));
    page->switching = FALSE;
}

GswordPersonalSection
gsword_personal_page_get_section(GswordPersonalPage *page)
{
    g_return_val_if_fail(page != NULL, GSWORD_PERSONAL_SECTION_TODAY);
    return section_from_name(
        adw_view_stack_get_visible_child_name(page->stack));
}

void
gsword_personal_page_set_section_changed_func(
    GswordPersonalPage *page, GswordPersonalSectionChangedFunc func,
    gpointer user_data)
{
    g_return_if_fail(page != NULL);
    page->section_changed = func;
    page->section_changed_data = user_data;
}
