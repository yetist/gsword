/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * workspace-pane.h: a single comparison pane in the gsword comparison workspace
 */

#ifndef GSWORD_WORKSPACE_PANE_H
#define GSWORD_WORKSPACE_PANE_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordWorkspacePage GswordWorkspacePage;
typedef struct _GswordWorkspacePane GswordWorkspacePane;

gboolean gsword_workspace_pane_supports_module(GswModuleInfo *info);
GswordWorkspacePane *gsword_workspace_pane_new(GswordWindow *window,
                                                GswordWorkspacePage *page,
                                                const gchar *module_name,
                                                const gchar *key,
                                                gboolean follow,
                                                gboolean save);
GtkWidget *gsword_workspace_pane_widget(GswordWorkspacePane *pane);
const gchar *gsword_workspace_pane_get_module_name(GswordWorkspacePane *pane);
GswModule *gsword_workspace_pane_get_module(GswordWorkspacePane *pane);
gchar *gsword_workspace_pane_dup_storage_key(GswordWorkspacePane *pane);
gboolean gsword_workspace_pane_get_follow(GswordWorkspacePane *pane);
gint gsword_workspace_pane_get_follow_mode(GswordWorkspacePane *pane);
void gsword_workspace_pane_set_follow_mode(GswordWorkspacePane *pane,
                                           gint mode);
void gsword_workspace_pane_apply_book_name_policy(
    GswordWorkspacePane *pane);
GtkScrolledWindow *gsword_workspace_pane_get_scroll(GswordWorkspacePane *pane);
GHashTable *gsword_workspace_pane_dup_verse_texts(GswordWorkspacePane *pane);
void gsword_workspace_pane_set_diff_verses(GswordWorkspacePane *pane,
                                           const GHashTable *verses);
void gsword_workspace_pane_scroll_to_active(GswordWorkspacePane *pane);
gint gsword_workspace_pane_get_top_verse(GswordWorkspacePane *pane);
void gsword_workspace_pane_scroll_to_verse(GswordWorkspacePane *pane,
                                           gint verse);
void gsword_workspace_pane_set_reference(GswordWorkspacePane *pane,
                                          const gchar *reference);
void gsword_workspace_pane_free(GswordWorkspacePane *pane);

G_END_DECLS

#endif /* GSWORD_WORKSPACE_PANE_H */
