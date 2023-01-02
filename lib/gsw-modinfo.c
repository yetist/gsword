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

typedef struct _ModInfo ModInfo;
struct _ModInfo {
	gchar *name;
	gchar *description;
	gchar *category;
	gchar *language;
	gchar *version;
	gchar *delta;
};

GswModinfo*  gsw_modinfo_new  (const gchar* name,
		const gchar* description,
		const gchar* category,
		const gchar* language,
		const gchar* version,
		const gchar* delta)
{
	ModInfo *info;
	info = g_new0(ModInfo, 1);
	info->name        = g_strdup(name);
	info->description = g_strdup(description);
	info->category    = g_strdup(category);
	info->language    = g_strdup(language);
	info->version     = g_strdup(version);
	info->delta       = g_strdup(delta);
	return info;
}

const gchar* gsw_modinfo_get_name (GswModinfo *modinfo)
{
	ModInfo *info;
	g_return_val_if_fail(modinfo, NULL);
	info = (ModInfo*)modinfo;
	return info->name;
}

const gchar* gsw_modinfo_get_description (GswModinfo *modinfo)
{
	ModInfo *info;
	g_return_val_if_fail(modinfo, NULL);
	info = (ModInfo*)modinfo;

	return info->description;
}

const gchar* gsw_modinfo_get_category    (GswModinfo *modinfo)
{
	ModInfo *info;
	g_return_val_if_fail(modinfo, NULL);
	info = (ModInfo*)modinfo;

	return info->category;
}

const gchar* gsw_modinfo_get_language    (GswModinfo *modinfo)
{
	ModInfo *info;
	g_return_val_if_fail(modinfo, NULL);
	info = (ModInfo*)modinfo;

	return info->language;
}

const gchar* gsw_modinfo_get_version     (GswModinfo *modinfo)
{
	ModInfo *info;
	g_return_val_if_fail(modinfo, NULL);
	info = (ModInfo*)modinfo;

	return info->version;
}

const gchar* gsw_modinfo_get_delta       (GswModinfo *modinfo)
{
	ModInfo *info;
	g_return_val_if_fail(modinfo, NULL);
	info = (ModInfo*)modinfo;

	return info->delta;
}

void gsw_modinfo_free (GswModinfo *modinfo)
{
	ModInfo *info;
	info = (ModInfo*) modinfo;

	if (info->name != NULL) g_free(info->name);
	if (info->description != NULL) g_free(info->description);
	if (info->category != NULL) g_free(info->category);
	if (info->language != NULL) g_free(info->language);
	if (info->version != NULL) g_free(info->version);
	if (info->delta != NULL) g_free(info->delta);
	g_free(info);
}
