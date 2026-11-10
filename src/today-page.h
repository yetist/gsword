/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * today-page.h: personal center daily overview for the gsword window
 */

#ifndef GSWORD_TODAY_PAGE_H
#define GSWORD_TODAY_PAGE_H 1

#include "gsword-page.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(GswordTodayPage, gsword_today_page, GSWORD,
                     TODAY_PAGE, GswordPage)

GswordTodayPage *gsword_today_page_new(GswordWindow *window);
GtkWidget *gsword_today_page_widget(GswordTodayPage *page);
void gsword_today_page_refresh(GswordTodayPage *page);

G_END_DECLS

#endif /* GSWORD_TODAY_PAGE_H */
