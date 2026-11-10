/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * reference-selection.h: shared module-to-chooser synchronization
 */

#ifndef GSWORD_REFERENCE_SELECTION_H
#define GSWORD_REFERENCE_SELECTION_H 1

#include "gsword-reference-chooser.h"
#include "window.h"

G_BEGIN_DECLS

typedef struct {
    gchar *versification;
    gchar *locale;
} GswordReferenceContext;

void gsword_reference_context_sync(GswordReferenceContext *context,
                                   GswordReferenceChooser *chooser,
                                   GswordWindow *window, GswModule *module);
void gsword_reference_context_clear(GswordReferenceContext *context);
void gsword_reference_chooser_sync_from_module(
    GswordReferenceChooser *chooser, GswordReferenceContext *context,
    GswordWindow *window, GswModule *module, gint range_start,
    gint range_end);

G_END_DECLS

#endif /* GSWORD_REFERENCE_SELECTION_H */
