/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * prayer-page.h: prayer management page for the gsword window
 */

#ifndef GSWORD_PRAYER_PAGE_H
#define GSWORD_PRAYER_PAGE_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordPrayerPage GswordPrayerPage;

GswordPrayerPage *gsword_prayer_page_new(GswordWindow *window);
GtkWidget *gsword_prayer_page_widget(GswordPrayerPage *page);
void gsword_prayer_page_refresh(GswordPrayerPage *page);
void gsword_prayer_page_open_for_reference(GswordPrayerPage *page,
                                           const gchar *module,
                                           const gchar *storage_key,
                                           const gchar *display_key);

G_END_DECLS

#endif /* GSWORD_PRAYER_PAGE_H */
