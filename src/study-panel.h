/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * study-panel.h: commentary, Strong's, cross-reference and lexicon panel
 */

#ifndef GSWORD_STUDY_PANEL_H
#define GSWORD_STUDY_PANEL_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordStudyPanel GswordStudyPanel;

GswordStudyPanel *gsword_study_panel_new(GswordWindow *window);
GtkWidget *gsword_study_panel_widget(GswordStudyPanel *panel);
void gsword_study_panel_refresh(GswordStudyPanel *panel);
void gsword_study_panel_refresh_lexicons(GswordStudyPanel *panel);
void gsword_study_panel_load_settings(GswordStudyPanel *panel);
void gsword_study_panel_bind_toggle(GswordStudyPanel *panel,
                                    GtkToggleButton *toggle);
void gsword_study_panel_set_visible(GswordStudyPanel *panel, gboolean visible);
void gsword_study_panel_set_follow(GswordStudyPanel *panel, gboolean follow);
void gsword_study_panel_show_commentary(GswordStudyPanel *panel);
void gsword_study_panel_show_lexicon(GswordStudyPanel *panel);
void gsword_study_panel_set_commentary_module(GswordStudyPanel *panel,
                                               const gchar *name);
void gsword_study_panel_set_lexicon_module(GswordStudyPanel *panel,
                                            const gchar *name);

G_END_DECLS

#endif /* GSWORD_STUDY_PANEL_H */
