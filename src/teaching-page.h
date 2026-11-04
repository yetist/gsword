/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * teaching-page.h: teaching workspace for the gsword window
 */

#ifndef GSWORD_TEACHING_PAGE_H
#define GSWORD_TEACHING_PAGE_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordTeachingPage GswordTeachingPage;

GswordTeachingPage *gsword_teaching_page_new(GswordWindow *window);
GtkWidget *gsword_teaching_page_widget(GswordTeachingPage *page);
void gsword_teaching_page_load(GswordTeachingPage *page);

G_END_DECLS

#endif /* GSWORD_TEACHING_PAGE_H */
