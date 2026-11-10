/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * collections-page.h: verse lists, bookmarks and favorites placeholder
 */

#ifndef GSWORD_COLLECTIONS_PAGE_H
#define GSWORD_COLLECTIONS_PAGE_H 1

#include "gsword-page.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(GswordCollectionsPage, gsword_collections_page, GSWORD,
                     COLLECTIONS_PAGE, GswordPage)

GswordCollectionsPage *gsword_collections_page_new(GswordWindow *window);
GtkWidget *gsword_collections_page_widget(GswordCollectionsPage *page);
void gsword_collections_page_refresh(GswordCollectionsPage *page);
void gsword_collections_page_show_bookmarks(GswordCollectionsPage *page);

G_END_DECLS

#endif /* GSWORD_COLLECTIONS_PAGE_H */
