/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-manager.c: This file is part of ____
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

struct _GswManager
{
  GObject      object;
};

static GParamSpec *properties[LAST_PROP] = { NULL };
G_DEFINE_TYPE (GswManager, gsw_manager, G_TYPE_OBJECT);

static void
gsw_manager_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    GswManager *manager;

    manager = GSW_MANAGER (object);

    switch (prop_id)
    {

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gsw_manager_get_property (GObject      *object,
                            guint         prop_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
    GswManager *manager;

    manager = GSW_MANAGER (object);

    switch (prop_id)
    {

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gsw_manager_class_init (GswManagerClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

    gobject_class->set_property = gsw_manager_set_property;
    gobject_class->get_property = gsw_manager_get_property;

    //g_type_class_add_private (class, sizeof (GswManagerPrivate));
    g_object_class_install_properties (gobject_class, LAST_PROP, properties);
}

static void
gsw_manager_init (GswManager *manager)
{
    GswManagerPrivate *priv;

    priv = GSW_MANAGER_GET_PRIVATE (manager);

}

GswManager*
gsw_manager_new (void)
{
    return g_object_new (GSW_TYPE_MANAGER, NULL);
}
