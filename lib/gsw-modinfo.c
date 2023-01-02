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

struct _GswModinfo {
	gint   refcount;
	gchar *name;
	gchar *description;
	gchar *category;
	gchar *language;
	gchar *version;
	gchar *delta;
};

static void _gsw_modinfo_ref (GswModinfo *modinfo)
{
    g_atomic_int_inc (&modinfo->refcount);
}

static GswModinfo* gsw_modinfo_copy (GswModinfo *modinfo)
{
    _gsw_modinfo_ref (modinfo);
    return modinfo;
}

void gsw_modinfo_unref (GswModinfo *modinfo)
{
	if (!g_atomic_int_dec_and_test (&modinfo->refcount))
		return;

	if (modinfo->name != NULL) g_free(modinfo->name);
	if (modinfo->description != NULL) g_free(modinfo->description);
	if (modinfo->category != NULL) g_free(modinfo->category);
	if (modinfo->language != NULL) g_free(modinfo->language);
	if (modinfo->version != NULL) g_free(modinfo->version);
	if (modinfo->delta != NULL) g_free(modinfo->delta);

	if (modinfo != NULL) g_free (modinfo);
}


GType gsw_modinfo_get_type (void)
{
    static GType the_type = 0;

    if (G_UNLIKELY (!the_type))
        the_type = g_boxed_type_register_static (
                    "GswModinfo",
                    (GBoxedCopyFunc) gsw_modinfo_copy,
                    (GBoxedFreeFunc) gsw_modinfo_unref);

    return the_type;
}

GswModinfo*  gsw_modinfo_new  (const gchar* name,
		const gchar* description,
		const gchar* category,
		const gchar* language,
		const gchar* version,
		const gchar* delta)
{
	GswModinfo *info;
	info = g_new0(GswModinfo, 1);
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
	g_return_val_if_fail(modinfo != NULL, NULL);
	return modinfo->name;
}

const gchar* gsw_modinfo_get_description (GswModinfo *modinfo)
{
	g_return_val_if_fail(modinfo != NULL, NULL);
	return modinfo->description;
}

const gchar* gsw_modinfo_get_category    (GswModinfo *modinfo)
{
	g_return_val_if_fail(modinfo != NULL, NULL);
	return modinfo->category;
}

const gchar* gsw_modinfo_get_language    (GswModinfo *modinfo)
{
	g_return_val_if_fail(modinfo != NULL, NULL);
	return modinfo->language;
}

const gchar* gsw_modinfo_get_version     (GswModinfo *modinfo)
{
	g_return_val_if_fail(modinfo != NULL, NULL);
	return modinfo->version;
}

const gchar* gsw_modinfo_get_delta       (GswModinfo *modinfo)
{
	g_return_val_if_fail(modinfo != NULL, NULL);
	return modinfo->delta;
}
