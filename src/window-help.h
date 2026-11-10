/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * window-help.h: window menu, shortcuts, and about dialogs
 */

#ifndef GSWORD_WINDOW_HELP_H
#define GSWORD_WINDOW_HELP_H 1

#include "window.h"

G_BEGIN_DECLS

GMenuModel *gsword_window_build_main_menu(void);
void gsword_window_show_shortcuts(GswordWindow *window);
void gsword_window_show_about(GswordWindow *window);

G_END_DECLS

#endif /* GSWORD_WINDOW_HELP_H */
