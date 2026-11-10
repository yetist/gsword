/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * personal-page.h: personal center shell for the gsword window
 */

#ifndef GSWORD_PERSONAL_PAGE_H
#define GSWORD_PERSONAL_PAGE_H 1

#include "gsword-page.h"

G_BEGIN_DECLS

typedef enum {
    GSWORD_PERSONAL_SECTION_TODAY,
    GSWORD_PERSONAL_SECTION_NOTES,
    GSWORD_PERSONAL_SECTION_PRAYER,
    GSWORD_PERSONAL_SECTION_PLANS,
    GSWORD_PERSONAL_SECTION_COLLECTIONS
} GswordPersonalSection;

G_DECLARE_FINAL_TYPE(GswordPersonalPage, gsword_personal_page, GSWORD,
                     PERSONAL_PAGE, GswordPage)
typedef void (*GswordPersonalSectionChangedFunc)(GswordPersonalPage *page,
                                                 GswordPersonalSection section,
                                                 gpointer user_data);

GswordPersonalPage *gsword_personal_page_new(GswordWindow *window,
                                             GtkWidget *today_widget,
                                             GtkWidget *notes_widget,
                                             GtkWidget *prayer_widget,
                                             GtkWidget *plans_widget,
                                             GtkWidget *collections_widget);
GtkWidget *gsword_personal_page_widget(GswordPersonalPage *page);
GtkWidget *gsword_personal_page_switcher(GswordPersonalPage *page);
GtkWidget *gsword_personal_page_switcher_bar(GswordPersonalPage *page);
void gsword_personal_page_show_section(GswordPersonalPage *page,
                                       GswordPersonalSection section);
GswordPersonalSection gsword_personal_page_get_section(
    GswordPersonalPage *page);
void gsword_personal_page_set_section_changed_func(
    GswordPersonalPage *page, GswordPersonalSectionChangedFunc func,
    gpointer user_data);

G_END_DECLS

#endif /* GSWORD_PERSONAL_PAGE_H */
