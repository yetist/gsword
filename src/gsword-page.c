/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsword-page.c: common base for window pages
 */

#include "gsword-page.h"

typedef struct {
    GswordWindow *window;
    GtkWidget *widget;
} GswordPagePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(GswordPage, gsword_page, G_TYPE_OBJECT)

static void
gsword_page_class_init(GswordPageClass *klass)
{
    (void)klass;
}

static void
gsword_page_init(GswordPage *page)
{
    (void)page;
}

GswordWindow *
gsword_page_get_window(GswordPage *page)
{
    GswordPagePrivate *priv;

    g_return_val_if_fail(GSWORD_IS_PAGE(page), NULL);
    priv = gsword_page_get_instance_private(page);
    return priv->window;
}

void
gsword_page_set_window(GswordPage *page, GswordWindow *window)
{
    GswordPagePrivate *priv;

    g_return_if_fail(GSWORD_IS_PAGE(page));
    g_return_if_fail(GSWORD_IS_WINDOW(window));
    priv = gsword_page_get_instance_private(page);
    priv->window = window;
}

GtkWidget *
gsword_page_get_widget(GswordPage *page)
{
    GswordPagePrivate *priv;

    g_return_val_if_fail(GSWORD_IS_PAGE(page), NULL);
    priv = gsword_page_get_instance_private(page);
    return priv->widget;
}

void
gsword_page_set_widget(GswordPage *page, GtkWidget *widget)
{
    GswordPagePrivate *priv;

    g_return_if_fail(GSWORD_IS_PAGE(page));
    priv = gsword_page_get_instance_private(page);
    priv->widget = widget;
}

void
gsword_page_dispose_popover(GtkPopover **popover)
{
    if (popover == NULL || *popover == NULL)
        return;
    if (gtk_widget_get_parent(GTK_WIDGET(*popover)) != NULL)
        gtk_widget_unparent(GTK_WIDGET(*popover));
    *popover = NULL;
}
