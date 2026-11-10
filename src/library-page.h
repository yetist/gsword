/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * library-page.h: module library and install page for the gsword window
 */

#ifndef GSWORD_LIBRARY_PAGE_H
#define GSWORD_LIBRARY_PAGE_H 1

#include "gsword-page.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(GswordLibraryPage, gsword_library_page, GSWORD,
                     LIBRARY_PAGE, GswordPage)

GswordLibraryPage *gsword_library_page_new(GswordWindow *window);
GtkWidget *gsword_library_page_widget(GswordLibraryPage *page);
void gsword_library_page_refresh_sources(GswordLibraryPage *page);
void gsword_library_page_refresh_installed(GswordLibraryPage *page);
void gsword_library_page_refresh_remote(GswordLibraryPage *page);

G_END_DECLS

#endif /* GSWORD_LIBRARY_PAGE_H */
