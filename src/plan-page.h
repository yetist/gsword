/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * plan-page.h: reading plan container for the gsword window
 */

#ifndef GSWORD_PLAN_PAGE_H
#define GSWORD_PLAN_PAGE_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordPlanPage GswordPlanPage;

GswordPlanPage *gsword_plan_page_new(GswordWindow *window);
GtkWidget *gsword_plan_page_widget(GswordPlanPage *page);
void gsword_plan_page_refresh(GswordPlanPage *page);
void gsword_plan_page_refresh_devotion(GswordPlanPage *page);
void gsword_plan_page_refresh_memory(GswordPlanPage *page);
void gsword_plan_page_show_devotion(GswordPlanPage *page);
void gsword_plan_page_show_memory(GswordPlanPage *page);

G_END_DECLS

#endif /* GSWORD_PLAN_PAGE_H */
