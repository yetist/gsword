/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * insight-page.h: free-form insight management page for the gsword window
 */

#ifndef GSWORD_INSIGHT_PAGE_H
#define GSWORD_INSIGHT_PAGE_H 1

#include "gsword-page.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(GswordInsightPage, gsword_insight_page, GSWORD,
                     INSIGHT_PAGE, GswordPage)

GswordInsightPage *gsword_insight_page_new(GswordWindow *window);
GtkWidget *gsword_insight_page_widget(GswordInsightPage *page);
void gsword_insight_page_refresh(GswordInsightPage *page);

G_END_DECLS

#endif /* GSWORD_INSIGHT_PAGE_H */
