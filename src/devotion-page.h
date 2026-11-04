/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * devotion-page.h: daily devotion page for the gsword window
 */

#ifndef GSWORD_DEVOTION_PAGE_H
#define GSWORD_DEVOTION_PAGE_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordDevotionPage GswordDevotionPage;

GswordDevotionPage *gsword_devotion_page_new(GswordWindow *window);
GtkWidget *gsword_devotion_page_widget(GswordDevotionPage *page);
void gsword_devotion_page_refresh(GswordDevotionPage *page);

G_END_DECLS

#endif /* GSWORD_DEVOTION_PAGE_H */
