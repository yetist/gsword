/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-module.c: This file is part of ____
 *
 * Copyright (C) 2017 yetist <yetist@yetipc>
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


enum {
    LAST_SIGNAL
};

enum {
    PROP_0,
};

struct _GswModule
{
  GObject      object;
};

static GParamSpec *properties[LAST_PROP] = { NULL };
G_DEFINE_TYPE (GswModule, gsw_module, G_TYPE_OBJECT);

static void
gsw_module_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    GswModule *module;

    module = GSW_MODULE (object);

    switch (prop_id)
    {

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gsw_module_get_property (GObject      *object,
                            guint         prop_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
    GswModule *module;

    module = GSW_MODULE (object);

    switch (prop_id)
    {

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gsw_module_class_init (GswModuleClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

    gobject_class->set_property = gsw_module_set_property;
    gobject_class->get_property = gsw_module_get_property;

    //g_type_class_add_private (class, sizeof (GswModulePrivate));
    g_object_class_install_properties (gobject_class, LAST_PROP, properties);
}

static void
gsw_module_init (GswModule *module)
{
    GswModulePrivate *priv;

    priv = GSW_MODULE_GET_PRIVATE (module);

}

GswModule*
gsw_module_new (void)
{
    return g_object_new (GSW_TYPE_MODULE, NULL);
}
