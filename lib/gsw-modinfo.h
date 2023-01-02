/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-modinfo.h: This file is part of ____
 *
 * Copyright (C) 2017 yetist <yetist@yetibook>
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

#ifndef __GSW_MODINFO_H__ 
#define __GSW_MODINFO_H__  1

#include <glib-object.h>

G_BEGIN_DECLS

#define GSW_TYPE_MODINFO              (gsw_modinfo_get_type ())
#define GSW_MODINFO(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSW_TYPE_MODINFO, GswModinfo))

typedef struct _GswModinfo GswModinfo;

GType        gsw_modinfo_get_type   (void) G_GNUC_CONST;
GswModinfo*  gsw_modinfo_new (const gchar* name, const gchar* description,
		const gchar* category, const gchar* language,
		const gchar* version, const gchar* delta);
const gchar* gsw_modinfo_get_name        (GswModinfo *modinfo);
const gchar* gsw_modinfo_get_description (GswModinfo *modinfo);
const gchar* gsw_modinfo_get_category    (GswModinfo *modinfo);
const gchar* gsw_modinfo_get_language    (GswModinfo *modinfo);
const gchar* gsw_modinfo_get_version     (GswModinfo *modinfo);
const gchar* gsw_modinfo_get_delta       (GswModinfo *modinfo);
void         gsw_modinfo_free            (GswModinfo *modinfo);
void         gsw_modinfo_unref           (GswModinfo *modinfo);

G_END_DECLS

#endif /* __GSW_MODINFO_H__ */
