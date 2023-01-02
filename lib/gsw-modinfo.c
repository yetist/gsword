/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-modinfo.c: This file is part of ____
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

#include "gsw-modinfo.h"

struct _GswModInfo {
	gchar *name;
	gchar *description;
	gchar *category;
	gchar *language;
	gchar *version;
	gchar *delta;
};

GswModinfo*  gsw_modinfo_new             (const gchar* name, const gchar* description,
		const gchar* category, const gchar* language,
		const gchar* version, const gchar* delta)
{
	GswModinfo *modinfo;
	modinfo = g_new0(GswModinfo);
	modinfo->name        = g_strdup(name);
	modinfo->description = g_strdup(description);
	modinfo->category    = g_strdup(category);
	modinfo->language    = g_strdup(language);
	modinfo->version     = g_strdup(version);
	modinfo->delta       = g_strdup(delta);
	return modinfo;
}

const gchar* gsw_modinfo_get_name        (GswModinfo *modinfo)
{
	g_return_val_if_fail(modinfo, NULL);
	return modinfo->name;
}

const gchar* gsw_modinfo_get_description (GswModinfo *modinfo)
{
	g_return_val_if_fail(modinfo, NULL);
	return modinfo->description;
}

const gchar* gsw_modinfo_get_category    (GswModinfo *modinfo)
{
	g_return_val_if_fail(modinfo, NULL);
	return modinfo->category;
}

const gchar* gsw_modinfo_get_language    (GswModinfo *modinfo)
{
	g_return_val_if_fail(modinfo, NULL);
	return modinfo->language;
}

const gchar* gsw_modinfo_get_version     (GswModinfo *modinfo)
{
	g_return_val_if_fail(modinfo, NULL);
	return modinfo->version;
}

const gchar* gsw_modinfo_get_delta       (GswModinfo *modinfo)
{
	g_return_val_if_fail(modinfo, NULL);
	return modinfo->delta;
}

void gsw_modinfo_free (GswModinfo *modinfo)
{
	if (modinfo->name != NULL) g_free(modinfo->name);
	if (modinfo->description != NULL) g_free(modinfo->description);
	if (modinfo->category != NULL) g_free(modinfo->category);
	if (modinfo->language != NULL) g_free(modinfo->language);
	if (modinfo->version != NULL) g_free(modinfo->version);
	if (modinfo->delta != NULL) g_free(modinfo->delta);
	g_free(modinfo);
}
