/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * memory-page.h: memory verse review page for the gsword window
 */

#ifndef GSWORD_MEMORY_PAGE_H
#define GSWORD_MEMORY_PAGE_H 1

#include "gsword-page.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(GswordMemoryPage, gsword_memory_page, GSWORD,
                     MEMORY_PAGE, GswordPage)

GswordMemoryPage *gsword_memory_page_new(GswordWindow *window);
GtkWidget *gsword_memory_page_widget(GswordMemoryPage *page);
void gsword_memory_page_refresh(GswordMemoryPage *page);

G_END_DECLS

#endif /* GSWORD_MEMORY_PAGE_H */
