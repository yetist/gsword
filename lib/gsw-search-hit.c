/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-search-hit.c: This file is part of ____
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

#include "gsw-search-hit.h"

struct _GswSearchHit {
	gint         refcount;
	gchar       *name;
	gchar       *key;
	glong        score;
};

static void _gsw_search_hit_ref (GswSearchHit *search_hit)
{
	g_atomic_int_inc (&search_hit->refcount);
}

static GswSearchHit* gsw_search_hit_copy (GswSearchHit *search_hit)
{
	_gsw_search_hit_ref (search_hit);
	return search_hit;
}

void gsw_search_hit_unref (gpointer data)
{
	GswSearchHit *search_hit = (GswSearchHit*) data;
	if (!g_atomic_int_dec_and_test (&search_hit->refcount))
		return;

	if (search_hit->key != NULL)
		g_free(search_hit->key);
	if (search_hit->name != NULL)
		g_free(search_hit->name);
	if (search_hit != NULL)
		g_free (search_hit);
}

GType gsw_search_hit_get_type (void)
{
	static GType the_type = 0;

	if (G_UNLIKELY (!the_type))
		the_type = g_boxed_type_register_static (
				"GswSearchHit",
				(GBoxedCopyFunc) gsw_search_hit_copy,
				(GBoxedFreeFunc) gsw_search_hit_unref);
	return the_type;
}


GswSearchHit* gsw_search_hit_new (const gchar *modName, const gchar *key, glong  score)
{
	GswSearchHit *search_hit;
	search_hit = g_new0(GswSearchHit, 1);
	search_hit->name = g_strdup(modName);
	search_hit->key = g_strdup(key);
	search_hit->score = score;
	return search_hit;
}

const gchar* gsw_search_hit_get_name  (GswSearchHit *search_hit)
{
	g_return_val_if_fail(search_hit != NULL, NULL);
	return search_hit->name;
}

const gchar* gsw_search_hit_get_key   (GswSearchHit *search_hit)
{
	g_return_val_if_fail(search_hit != NULL, NULL);
	return search_hit->key;
}

glong gsw_search_hit_get_score (GswSearchHit *search_hit)
{
	g_return_val_if_fail(search_hit != NULL, 0);
	return search_hit->score;
}
