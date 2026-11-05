/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * collections-page.h: verse lists, bookmarks and favorites placeholder
 */

#ifndef GSWORD_COLLECTIONS_PAGE_H
#define GSWORD_COLLECTIONS_PAGE_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordCollectionsPage GswordCollectionsPage;

GswordCollectionsPage *gsword_collections_page_new(GswordWindow *window);
GtkWidget *gsword_collections_page_widget(GswordCollectionsPage *page);
void gsword_collections_page_refresh(GswordCollectionsPage *page);
void gsword_collections_page_show_bookmarks(GswordCollectionsPage *page);

G_END_DECLS

#endif /* GSWORD_COLLECTIONS_PAGE_H */
