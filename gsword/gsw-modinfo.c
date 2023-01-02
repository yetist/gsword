/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-modinfo.c: This file is part of gsword.
 *
 * Copyright (C) 2017 yetist <yetist@gmail.com>
 *
 * 
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

enum {
    PROP_0,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_CATEGORY,
    PROP_LANGUAGE,
    PROP_VERSION,
    PROP_DELTA,
    LAST_PROP
};

typedef struct _GswModinfo GswModinfo;

struct _GswModinfo {
	GObject  object;
	gint     refcount;
	gchar   *name;
	gchar   *description;
	gchar   *category;
	gchar   *language;
	gchar   *version;
	gchar   *delta;
};

static GParamSpec *properties[LAST_PROP] = { NULL };
G_DEFINE_TYPE (GswModinfo, gsw_modinfo, G_TYPE_OBJECT);

static void gsw_modinfo_set_name(GswModinfo *modinfo, const gchar* value);
static void gsw_modinfo_set_description(GswModinfo *modinfo, const gchar* value);
static void gsw_modinfo_set_category(GswModinfo *modinfo, const gchar* value);
static void gsw_modinfo_set_language(GswModinfo *modinfo, const gchar* value);
static void gsw_modinfo_set_version(GswModinfo *modinfo, const gchar* value);
static void gsw_modinfo_set_delta(GswModinfo *modinfo, const gchar* value);
 
static void gsw_modinfo_dispose(GObject *object)
{
	GswModinfo *modinfo = GSW_MODINFO(object);

	if (modinfo->name != NULL){
		g_free(modinfo->name);
		modinfo->name = NULL;
	}

	if (modinfo->description != NULL){
		g_free(modinfo->description);
		modinfo->description = NULL;
	}

	if (modinfo->category != NULL){
		g_free(modinfo->category);
		modinfo->category = NULL;
	}

	if (modinfo->language != NULL){
		g_free(modinfo->language);
		modinfo->language = NULL;
	}

	if (modinfo->version != NULL){
		g_free(modinfo->version);
		modinfo->version = NULL;
	}
	if (modinfo->delta != NULL){
		g_free(modinfo->delta);
		modinfo->delta= NULL;
	}

	G_OBJECT_CLASS (gsw_modinfo_parent_class)->dispose (object);
}

static void gsw_modinfo_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    GswModinfo *modinfo;
    modinfo = GSW_MODINFO (object);

    switch (prop_id)
    {
		case PROP_NAME:
			gsw_modinfo_set_name(modinfo, g_value_get_string (value));
			break;
		case PROP_DESCRIPTION:
			gsw_modinfo_set_description(modinfo, g_value_get_string (value));
		case PROP_CATEGORY:
			gsw_modinfo_set_category(modinfo, g_value_get_string (value));
		case PROP_LANGUAGE:
			gsw_modinfo_set_language(modinfo, g_value_get_string (value));
		case PROP_VERSION:
			gsw_modinfo_set_version(modinfo, g_value_get_string (value));
		case PROP_DELTA:
			gsw_modinfo_set_delta(modinfo, g_value_get_string (value));
			break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gsw_modinfo_get_property (GObject      *object,
                            guint         prop_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
    GswModinfo *modinfo;

    modinfo = GSW_MODINFO (object);

    switch (prop_id)
    {
		case PROP_NAME:
			g_value_set_string(value, modinfo->name);
			break;
		case PROP_DESCRIPTION:
			g_value_set_string(value, modinfo->description);
			break;
		case PROP_CATEGORY:
			g_value_set_string(value, modinfo->category);
			break;
		case PROP_LANGUAGE:
			g_value_set_string(value, modinfo->language);
			break;
		case PROP_VERSION:
			g_value_set_string(value, modinfo->version);
			break;
		case PROP_DELTA:
			g_value_set_string(value, modinfo->delta);
			break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gsw_modinfo_class_init (GswModinfoClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);

    gobject_class->dispose = gsw_modinfo_dispose;
    gobject_class->set_property = gsw_modinfo_set_property;
    gobject_class->get_property = gsw_modinfo_get_property;

	properties[PROP_NAME] =
		g_param_spec_string ("name",
				"name",
				"module name",
				"",
				G_PARAM_READWRITE);
	properties[PROP_DESCRIPTION] =
		g_param_spec_string ("description",
				"description",
				"module description",
				"",
				G_PARAM_READWRITE);
	properties[PROP_CATEGORY] =
		g_param_spec_string ("category",
				"category",
				"module category",
				"",
				G_PARAM_READWRITE);
	properties[PROP_LANGUAGE] =
		g_param_spec_string ("language",
				"language",
				"module language",
				"",
				G_PARAM_READWRITE);
	properties[PROP_VERSION] =
		g_param_spec_string ("version",
				"version",
				"module version",
				"",
				G_PARAM_READWRITE);
	properties[PROP_DELTA] =
		g_param_spec_string ("delta",
				"delta",
				"module delta",
				"",
				G_PARAM_READWRITE);
    g_object_class_install_properties (gobject_class, LAST_PROP, properties);
}

static void
gsw_modinfo_init (GswModinfo *modinfo)
{
	modinfo->name = NULL;
	modinfo->description = NULL;
	modinfo->category = NULL;
	modinfo->language = NULL;
	modinfo->version = NULL;
	modinfo->delta = NULL;
}

GswModinfo*  gsw_modinfo_new  (const gchar* name,
		const gchar* description,
		const gchar* category,
		const gchar* language,
		const gchar* version,
		const gchar* delta)
{
	GswModinfo *info;
    info = (GswModinfo *) g_object_new (GSW_TYPE_MODINFO, "name", name, "description", description, "category", category, "version", version, "delta", delta, NULL);
	return info;
}

const gchar* gsw_modinfo_get_name (GswModinfo *modinfo)
{
	g_return_val_if_fail(GSW_IS_MODINFO(modinfo), NULL);
	return modinfo->name;
}

const gchar* gsw_modinfo_get_description (GswModinfo *modinfo)
{
	g_return_val_if_fail(GSW_IS_MODINFO(modinfo), NULL);
	return modinfo->description;
}

const gchar* gsw_modinfo_get_category    (GswModinfo *modinfo)
{
	g_return_val_if_fail(GSW_IS_MODINFO(modinfo), NULL);
	return modinfo->category;
}

const gchar* gsw_modinfo_get_language    (GswModinfo *modinfo)
{
	g_return_val_if_fail(GSW_IS_MODINFO(modinfo), NULL);
	return modinfo->language;
}

const gchar* gsw_modinfo_get_version     (GswModinfo *modinfo)
{
	g_return_val_if_fail(GSW_IS_MODINFO(modinfo), NULL);
	return modinfo->version;
}

const gchar* gsw_modinfo_get_delta       (GswModinfo *modinfo)
{
	g_return_val_if_fail(GSW_IS_MODINFO(modinfo), NULL);
	return modinfo->delta;
}

static void gsw_modinfo_set_name(GswModinfo *modinfo, const gchar* value)
{
	g_return_if_fail(GSW_IS_MODINFO(modinfo));
	modinfo->name = g_strdup(value);
}
static void gsw_modinfo_set_description(GswModinfo *modinfo, const gchar* value)
{
	g_return_if_fail(GSW_IS_MODINFO(modinfo));
	modinfo->description = g_strdup(value);
}
static void gsw_modinfo_set_category(GswModinfo *modinfo, const gchar* value)
{
	g_return_if_fail(GSW_IS_MODINFO(modinfo));
	modinfo->category = g_strdup(value);
}
static void gsw_modinfo_set_language(GswModinfo *modinfo, const gchar* value)
{
	g_return_if_fail(GSW_IS_MODINFO(modinfo));
	modinfo->language = g_strdup(value);
}
static void gsw_modinfo_set_version(GswModinfo *modinfo, const gchar* value)
{
	g_return_if_fail(GSW_IS_MODINFO(modinfo));
	modinfo->version = g_strdup(value);
}
static void gsw_modinfo_set_delta(GswModinfo *modinfo, const gchar* value)
{
	g_return_if_fail(GSW_IS_MODINFO(modinfo));
	modinfo->delta = g_strdup(value);
}
