/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * library-module-details.h: module detail formatting helpers
 */

#ifndef GSWORD_LIBRARY_MODULE_DETAILS_H
#define GSWORD_LIBRARY_MODULE_DETAILS_H 1

#include "gsword/gsw-module-info.h"

G_BEGIN_DECLS

gchar *gsword_library_format_size(guint64 bytes);
gchar *gsword_library_format_module_meta(GswModuleInfo *info,
                                         const gchar *version_text,
                                         const gchar *source_text);
gchar *gsword_library_format_module_warning(GswModuleInfo *info);
gchar *gsword_library_format_module_technical(GswModuleInfo *info,
                                              const gchar *name,
                                              gboolean include_minimum);
gchar *gsword_library_format_module_license(GswModuleInfo *info);

G_END_DECLS

#endif /* GSWORD_LIBRARY_MODULE_DETAILS_H */
