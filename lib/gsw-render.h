/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-render.h: markup rendering and entry attribute extraction
 */

#ifndef __GSW_RENDER_H__
#define __GSW_RENDER_H__ 1

#include <glib.h>

G_BEGIN_DECLS

gboolean    gsw_render_option_on(GHashTable *options, const gchar *name);
gboolean    gsw_render_option_any(GHashTable *options,
                                  const gchar *const *names);
gchar      *gsw_render_strip_markup(const gchar *markup);
gchar      *gsw_render_markup_html(GHashTable *options, const gchar *markup);
GHashTable *gsw_render_collect_attributes(const gchar *markup);

G_END_DECLS

#endif /* __GSW_RENDER_H__ */
