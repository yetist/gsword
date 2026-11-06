/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * image-view.h: zoomable map image view
 */

#ifndef GSWORD_IMAGE_VIEW_H
#define GSWORD_IMAGE_VIEW_H 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GSWORD_IMAGE_MIN_ZOOM 0.1
#define GSWORD_IMAGE_MAX_ZOOM 4.0

#define GSWORD_TYPE_IMAGE_VIEW (gsword_image_view_get_type())
G_DECLARE_FINAL_TYPE(GswordImageView, gsword_image_view, GSWORD, IMAGE_VIEW,
                     GtkWidget)

GtkWidget *gsword_image_view_new(void);
gboolean gsword_image_view_load(GswordImageView *self, const gchar *path,
                                GError **error);
void gsword_image_view_clear(GswordImageView *self);
void gsword_image_view_fit_width(GswordImageView *self);
void gsword_image_view_fit_window(GswordImageView *self);
void gsword_image_view_actual_size(GswordImageView *self);
void gsword_image_view_zoom_in(GswordImageView *self);
void gsword_image_view_zoom_out(GswordImageView *self);
gdouble gsword_image_view_get_zoom(GswordImageView *self);
GdkTexture *gsword_image_view_get_texture(GswordImageView *self);
const gchar *gsword_image_view_get_path(GswordImageView *self);
gboolean gsword_image_view_has_image(GswordImageView *self);

G_END_DECLS

#endif /* GSWORD_IMAGE_VIEW_H */
