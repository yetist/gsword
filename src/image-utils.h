/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * image-utils.h: helpers for map image paths and sizing
 */

#ifndef GSWORD_IMAGE_UTILS_H
#define GSWORD_IMAGE_UTILS_H 1

#include <glib.h>

G_BEGIN_DECLS

gchar *gsword_image_extract_src(const gchar *html);
gchar *gsword_image_resolve_path(const gchar *data_dir, const gchar *source,
                                 GError **error);
gdouble gsword_image_fit_width(gint image_width, gint view_width,
                               gint scrollbar_width);
gdouble gsword_image_fit_window(gint image_width, gint image_height,
                                gint view_width, gint view_height);

G_END_DECLS

#endif /* GSWORD_IMAGE_UTILS_H */
