/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * reference-utils.h: shared UI reference parsing and formatting
 */

#ifndef GSWORD_REFERENCE_UTILS_H
#define GSWORD_REFERENCE_UTILS_H 1

#include "gsword-reference-parser.h"

G_BEGIN_DECLS

gboolean gsword_reference_parse_ui(const gchar *text,
                                   GswordReference *out, GError **error);
gchar *gsword_reference_localize(const gchar *reference,
                                 gboolean include_verse);

G_END_DECLS

#endif /* GSWORD_REFERENCE_UTILS_H */
