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

#include "gsw-result.h"

enum {
    PROP_0,
	PROP_NAME,
	PROP_KEY,
	PROP_SCORE,
    LAST_PROP
};

typedef struct _GswResult GswResult;

struct _GswResult
{
	GObject      object;
	gchar       *name;
	gchar       *key;
	glong        score;
};

static void gsw_result_set_name (GswResult *result, const gchar* name);
static void gsw_result_set_key (GswResult *result, const gchar* key);
static void gsw_result_set_score (GswResult *result, glong score);

static GParamSpec *properties[LAST_PROP] = { NULL };
G_DEFINE_TYPE (GswResult, gsw_result, G_TYPE_OBJECT);

static void gsw_result_dispose(GObject *object)
{
    GswResult *result = GSW_RESULT (object);
	if (result->name != NULL){
		g_free(result->name);
		result->name = NULL;
	}
	if (result->key != NULL) {
		g_free(result->key);
		result->key = NULL;
	}
	G_OBJECT_CLASS (gsw_result_parent_class)->dispose (object);
}

static void gsw_result_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GswResult *result;

    result = GSW_RESULT (object);

    switch (prop_id)
    {
		case PROP_NAME:
			gsw_result_set_name(result, g_value_get_string (value));
            break;
		case PROP_KEY:
			gsw_result_set_key(result, g_value_get_string (value));
            break;
		case PROP_SCORE:
			gsw_result_set_score(result, g_value_get_long(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gsw_result_get_property (GObject      *object,
                            guint         prop_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
    GswResult *result;

    result = GSW_RESULT (object);

    switch (prop_id)
    {
		case PROP_NAME:
			g_value_set_string(value, result->name);
            break;
		case PROP_KEY:
			g_value_set_string(value, result->key);
            break;
		case PROP_SCORE:
			g_value_set_long(value, result->score);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gsw_result_class_init (GswResultClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);

    gobject_class->dispose = gsw_result_dispose;
    gobject_class->set_property = gsw_result_set_property;
    gobject_class->get_property = gsw_result_get_property;

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
gsw_result_init (GswResult *result)
{
	result->name = NULL;
	result->key = NULL;
	result->score = 0;
}

GswResult* gsw_result_new (const gchar *name, const gchar *key, glong  score)
{
	GswResult *result;
    result = (GswResult *)g_object_new (GSW_TYPE_RESULT, "name", name, "key", key, "score", score,  NULL);
	return result;
}

const gchar* gsw_result_get_name  (GswResult *result)
{
	g_return_val_if_fail(GSW_IS_RESULT(result), NULL);
	return result->name;
}

const gchar* gsw_result_get_key   (GswResult *result)
{
	g_return_val_if_fail(GSW_IS_RESULT(result), NULL);
	return result->key;
}

glong gsw_result_get_score (GswResult *result)
{
	g_return_val_if_fail(GSW_IS_RESULT(result), 0);
	return result->score;
}

static void gsw_result_set_name (GswResult *result, const gchar* name)
{
	g_return_if_fail(GSW_IS_RESULT(result));
	result->name = g_strdup(name);
}

static void gsw_result_set_key (GswResult *result, const gchar* key)
{
	g_return_if_fail(GSW_IS_RESULT(result));
	result->key = g_strdup(key);
}

static void gsw_result_set_score (GswResult *result, glong score)
{
	g_return_if_fail(GSW_IS_RESULT(result));
	result->score = score;
}
