/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * search-page.h: search page for the gsword window
 */

#ifndef GSWORD_SEARCH_PAGE_H
#define GSWORD_SEARCH_PAGE_H 1

#include "gsword-page.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(GswordSearchPage, gsword_search_page, GSWORD,
                     SEARCH_PAGE, GswordPage)

GswordSearchPage *gsword_search_page_new(GswordWindow *window);
GtkWidget *gsword_search_page_widget(GswordSearchPage *page);
void gsword_search_page_refresh_index(GswordSearchPage *page);
void gsword_search_page_select_current(GswordSearchPage *page);
void gsword_search_page_focus(GswordSearchPage *page);

G_END_DECLS

#endif /* GSWORD_SEARCH_PAGE_H */
