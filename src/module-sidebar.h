/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * module-sidebar.h: grouped module list for the gsword window
 */

#ifndef GSWORD_MODULE_SIDEBAR_H
#define GSWORD_MODULE_SIDEBAR_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordModuleSidebar GswordModuleSidebar;

GswordModuleSidebar *gsword_module_sidebar_new(GswordWindow *window);
GtkWidget *gsword_module_sidebar_widget(GswordModuleSidebar *sidebar);
void gsword_module_sidebar_populate(GswordModuleSidebar *sidebar);
void gsword_module_sidebar_select(GswordModuleSidebar *sidebar,
                                  const gchar *name);

G_END_DECLS

#endif /* GSWORD_MODULE_SIDEBAR_H */
