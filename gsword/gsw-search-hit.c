/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-search-hit.c: This file is part of gsword.
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

#include "gsw-search-hit.h"

enum {
    PROP_0,
	PROP_NAME,
	PROP_KEY,
	PROP_SCORE,
    LAST_PROP
};

typedef struct _GswSearchHit GswSearchHit;

struct _GswSearchHit
{
	GObject      object;
	gchar       *name;
	gchar       *key;
	glong        score;
};

static void gsw_search_hit_set_name (GswSearchHit *search_hit, const gchar* name);
static void gsw_search_hit_set_key (GswSearchHit *search_hit, const gchar* key);
static void gsw_search_hit_set_score (GswSearchHit *search_hit, glong score);

static GParamSpec *properties[LAST_PROP] = { NULL };
G_DEFINE_TYPE (GswSearchHit, gsw_search_hit, G_TYPE_OBJECT);

static void gsw_search_hit_dispose(GObject *object)
{
    GswSearchHit *search_hit = GSW_SEARCH_HIT (object);
	if (search_hit->name != NULL){
		g_free(search_hit->name);
		search_hit->name = NULL;
	}
	if (search_hit->key != NULL) {
		g_free(search_hit->key);
		search_hit->key = NULL;
	}
	G_OBJECT_CLASS (gsw_search_hit_parent_class)->dispose (object);
}

static void gsw_search_hit_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GswSearchHit *search_hit;

    search_hit = GSW_SEARCH_HIT (object);

    switch (prop_id)
    {
		case PROP_NAME:
			gsw_search_hit_set_name(search_hit, g_value_get_string (value));
            break;
		case PROP_KEY:
			gsw_search_hit_set_key(search_hit, g_value_get_string (value));
            break;
		case PROP_SCORE:
			gsw_search_hit_set_score(search_hit, g_value_get_long(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gsw_search_hit_get_property (GObject      *object,
                            guint         prop_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
    GswSearchHit *search_hit;

    search_hit = GSW_SEARCH_HIT (object);

    switch (prop_id)
    {
		case PROP_NAME:
			g_value_set_string(value, search_hit->name);
            break;
		case PROP_KEY:
			g_value_set_string(value, search_hit->key);
            break;
		case PROP_SCORE:
			g_value_set_long(value, search_hit->score);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gsw_search_hit_class_init (GswSearchHitClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);

    gobject_class->dispose = gsw_search_hit_dispose;
    gobject_class->set_property = gsw_search_hit_set_property;
    gobject_class->get_property = gsw_search_hit_get_property;

	properties[PROP_NAME] =
		g_param_spec_string ("name",
				"name",
				"search hit name",
				"",
				G_PARAM_READWRITE);
	properties[PROP_KEY] =
		g_param_spec_string ("key",
				"key",
				"search hit key",
				"",
				G_PARAM_READWRITE);
	properties[PROP_SCORE] =
		g_param_spec_long("score",
				"score",
				"search hit score",
				G_MINLONG,
				G_MAXLONG,
				0,
				G_PARAM_READWRITE);
    g_object_class_install_properties (gobject_class, LAST_PROP, properties);
}

static void
gsw_search_hit_init (GswSearchHit *search_hit)
{
	search_hit->name = NULL;
	search_hit->key = NULL;
	search_hit->score = 0;
}

GswSearchHit* gsw_search_hit_new (const gchar *name, const gchar *key, glong  score)
{
	GswSearchHit *search_hit;
    search_hit = (GswSearchHit *)g_object_new (GSW_TYPE_SEARCH_HIT, "name", name, "key", key, "score", score,  NULL);
	return search_hit;
}

const gchar* gsw_search_hit_get_name  (GswSearchHit *search_hit)
{
	g_return_val_if_fail(GSW_IS_SEARCH_HIT(search_hit), NULL);
	return search_hit->name;
}

const gchar* gsw_search_hit_get_key   (GswSearchHit *search_hit)
{
	g_return_val_if_fail(GSW_IS_SEARCH_HIT(search_hit), NULL);
	return search_hit->key;
}

glong gsw_search_hit_get_score (GswSearchHit *search_hit)
{
	g_return_val_if_fail(GSW_IS_SEARCH_HIT(search_hit), 0);
	return search_hit->score;
}

static void gsw_search_hit_set_name (GswSearchHit *search_hit, const gchar* name)
{
	g_return_if_fail(GSW_IS_SEARCH_HIT(search_hit));
	search_hit->name = g_strdup(name);
}

static void gsw_search_hit_set_key (GswSearchHit *search_hit, const gchar* key)
{
	g_return_if_fail(GSW_IS_SEARCH_HIT(search_hit));
	search_hit->key = g_strdup(key);
}

static void gsw_search_hit_set_score (GswSearchHit *search_hit, glong score)
{
	g_return_if_fail(GSW_IS_SEARCH_HIT(search_hit));
	search_hit->score = score;
}
