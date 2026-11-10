/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * library-page-private.h: internal helpers shared by library modules
 */

#ifndef GSWORD_LIBRARY_PAGE_PRIVATE_H
#define GSWORD_LIBRARY_PAGE_PRIVATE_H 1

#include "library-page.h"

G_BEGIN_DECLS

#define LIBRARY_WINDOW(page) \
    gsword_page_get_window(GSWORD_PAGE(page))

GswordLibraryPage *library_page_get(GswordWindow *self);
GswInstallManager *gsword_library_page_get_installer(
    GswordLibraryPage *page);
void gsword_window_refresh_install_sources(GswordWindow *self);
void gsword_library_page_show_sources_dialog(GtkButton *button,
                                             gpointer user_data);

G_END_DECLS

#endif /* GSWORD_LIBRARY_PAGE_PRIVATE_H */
