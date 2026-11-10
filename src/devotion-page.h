/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * devotion-page.h: daily devotion page for the gsword window
 */

#ifndef GSWORD_DEVOTION_PAGE_H
#define GSWORD_DEVOTION_PAGE_H 1

#include "gsword-page.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(GswordDevotionPage, gsword_devotion_page, GSWORD,
                     DEVOTION_PAGE, GswordPage)

GswordDevotionPage *gsword_devotion_page_new(GswordWindow *window);
GtkWidget *gsword_devotion_page_widget(GswordDevotionPage *page);
void gsword_devotion_page_refresh(GswordDevotionPage *page);

G_END_DECLS

#endif /* GSWORD_DEVOTION_PAGE_H */
