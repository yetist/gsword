/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsword-page.h: common base for window pages
 */

#ifndef GSWORD_PAGE_H
#define GSWORD_PAGE_H 1

#include "window.h"

G_BEGIN_DECLS

#define GSWORD_TYPE_PAGE (gsword_page_get_type())
G_DECLARE_DERIVABLE_TYPE(GswordPage, gsword_page, GSWORD, PAGE, GObject)

struct _GswordPageClass {
    GObjectClass parent_class;
};

GswordWindow *gsword_page_get_window(GswordPage *page);
void gsword_page_set_window(GswordPage *page, GswordWindow *window);
GtkWidget *gsword_page_get_widget(GswordPage *page);
void gsword_page_set_widget(GswordPage *page, GtkWidget *widget);
void gsword_page_dispose_popover(GtkPopover **popover);

G_END_DECLS

#endif /* GSWORD_PAGE_H */
