/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * library-module-row.h: module row model and display helpers
 */

#ifndef GSWORD_LIBRARY_MODULE_ROW_H
#define GSWORD_LIBRARY_MODULE_ROW_H 1

#include <glib-object.h>

#include "gsword/gsw-module-info.h"

G_BEGIN_DECLS

typedef struct _GswordModuleRow {
    GObject parent_instance;
    gchar *name;
    gchar *description;
    gchar *detail;
    gchar *language;
    GswModuleType type;
    guint status;
    gboolean remote;
} GswordModuleRow;

GType gsword_module_row_get_type(void);
GswordModuleRow *gsword_module_row_new(GswModuleInfo *info, guint status,
                                       gboolean remote);
gboolean gsword_module_row_matches_query(GswordModuleRow *row,
                                         const gchar *folded_query);
const gchar *gsword_library_module_status_text(guint status);
gchar *gsword_library_language_display_name(const gchar *code);
GswModuleType gsword_library_filter_type_for_info(GswModuleInfo *info);

G_END_DECLS

#endif /* GSWORD_LIBRARY_MODULE_ROW_H */
