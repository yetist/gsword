/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-search-hit.h: This file is part of gsword.
 *
 * Copyright (C) 2017 yetist <yetist@gmail.com>
 *
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * */

#ifndef __GSW_SEARCH_HIT_H__ 
#define __GSW_SEARCH_HIT_H__  1

#include <glib-object.h>

G_BEGIN_DECLS

#define GSW_TYPE_SEARCH_HIT              (gsw_search_hit_get_type ())
G_DECLARE_FINAL_TYPE (GswSearchHit, gsw_search_hit, GSW, SEARCH_HIT, GObject);

GswSearchHit*     gsw_search_hit_new       (const gchar *name, const gchar *key, glong  score);
const gchar*      gsw_search_hit_get_name  (GswSearchHit *search_hit);
const gchar*      gsw_search_hit_get_key   (GswSearchHit *search_hit);
glong             gsw_search_hit_get_score (GswSearchHit *search_hit);

G_END_DECLS

#endif /* __GSW_SEARCH_HIT_H__ */
