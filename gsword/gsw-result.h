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

#ifndef __GSW_RESULT_H__
#define __GSW_RESULT_H__  1

#include <glib-object.h>

G_BEGIN_DECLS

#define GSW_TYPE_RESULT              (gsw_result_get_type ())
G_DECLARE_FINAL_TYPE (GswResult, gsw_result, GSW, RESULT, GObject);

GswResult*        gsw_result_new       (const gchar *name, const gchar *key, glong  score);
const gchar*      gsw_result_get_name  (GswResult *result);
const gchar*      gsw_result_get_key   (GswResult *result);
glong             gsw_result_get_score (GswResult *result);

G_END_DECLS

#endif /* __GSW_RESULT_H__ */
