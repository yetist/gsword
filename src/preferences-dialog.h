/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * preferences-dialog.h: application appearance preferences
 */

#ifndef GSWORD_PREFERENCES_DIALOG_H
#define GSWORD_PREFERENCES_DIALOG_H 1

#include "window.h"

G_BEGIN_DECLS

void gsword_preferences_init(GswordWindow *window);
void gsword_preferences_show(GswordWindow *window);
gdouble gsword_preferences_get_font_scale(GswordWindow *window);
void gsword_preferences_set_font_scale(GswordWindow *window,
                                       gdouble font_scale);

G_END_DECLS

#endif /* GSWORD_PREFERENCES_DIALOG_H */
