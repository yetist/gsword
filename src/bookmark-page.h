/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * bookmark-page.h: reading-position bookmarks for the gsword window
 */

#ifndef GSWORD_BOOKMARK_PAGE_H
#define GSWORD_BOOKMARK_PAGE_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordBookmarkPage GswordBookmarkPage;

GswordBookmarkPage *gsword_bookmark_page_new(GswordWindow *window);
GtkWidget *gsword_bookmark_page_widget(GswordBookmarkPage *page);
void gsword_bookmark_page_refresh(GswordBookmarkPage *page);

G_END_DECLS

#endif /* GSWORD_BOOKMARK_PAGE_H */
