/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * workspace-page.h: comparison workspace container for the gsword window
 */

#ifndef GSWORD_WORKSPACE_PAGE_H
#define GSWORD_WORKSPACE_PAGE_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordWorkspacePage GswordWorkspacePage;
typedef struct _GswordWorkspacePane GswordWorkspacePane;

GswordWorkspacePage *gsword_workspace_page_new(GswordWindow *window);
GtkWidget *gsword_workspace_page_widget(GswordWorkspacePage *page);
GswordWorkspacePane *gsword_workspace_page_add_pane(
    GswordWorkspacePage *page, const gchar *module_name, const gchar *key,
    gboolean follow, gboolean save);
void gsword_workspace_page_remove_pane(GswordWorkspacePage *page,
                                        GswordWorkspacePane *pane);
void gsword_workspace_page_set_active_pane(GswordWorkspacePage *page,
                                           GswordWorkspacePane *pane);
void gsword_workspace_page_refresh_diff(GswordWorkspacePage *page);
void gsword_workspace_page_scroll_to_reference(GswordWorkspacePage *page,
                                               GswordWorkspacePane *source);
void gsword_workspace_page_reflow(GswordWorkspacePage *page);
void gsword_workspace_page_save(GswordWorkspacePage *page);
void gsword_workspace_page_restore(GswordWorkspacePage *page);
void gsword_workspace_page_sync_from_reader(GswordWorkspacePage *page);
void gsword_workspace_page_apply_parallel_template(GswordWorkspacePage *page);
void gsword_workspace_page_apply_book_name_policy(GswordWorkspacePage *page);
void gsword_workspace_page_sync_reference(GswordWorkspacePage *page,
                                           const gchar *reference,
                                           GswordWorkspacePane *source);
const gchar *gsword_workspace_page_get_reference(GswordWorkspacePage *page);
void gsword_workspace_page_sync_scroll(GswordWorkspacePage *page,
                                        GswordWorkspacePane *source,
                                        GtkAdjustment *adjustment);
void gsword_workspace_page_reorder_from_drag(GswordWorkspacePage *page,
                                              GswordWorkspacePane *source,
                                              gdouble dx, gdouble dy);

G_END_DECLS

#endif /* GSWORD_WORKSPACE_PAGE_H */
