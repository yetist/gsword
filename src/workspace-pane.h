/*
 * workspace-pane.h: a single pane in the gsword multi-pane workspace
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
gboolean gsword_workspace_pane_get_follow(GswordWorkspacePane *pane);
GtkScrolledWindow *gsword_workspace_pane_get_scroll(GswordWorkspacePane *pane);
void gsword_workspace_pane_set_reference(GswordWorkspacePane *pane,
                                          const gchar *reference);
void gsword_workspace_pane_free(GswordWorkspacePane *pane);

G_END_DECLS

#endif /* GSWORD_WORKSPACE_PANE_H */
