/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-manager.h: This file is part of gsword.
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

#ifndef __GSW_MANAGER_H__ 
#define __GSW_MANAGER_H__  1

#include <glib.h>
#include "gsw-module.h"

G_BEGIN_DECLS

typedef void GswManager;

GswManager*   gsw_manager_new              (void);
GswManager*   gsw_manager_new_with_path    (const gchar *path);
void          gsw_manager_delete           (GswManager *manager);
const gchar*  gsw_manager_get_version      (GswManager *manager);
GList *       gsw_manager_get_modinfo_list (GswManager *manager);
GswModule*    gsw_manager_get_module_by_name (GswManager *manager, const gchar *moduleName);
const gchar*  gsw_manager_get_prefix_path (GswManager *manager);
const gchar*  gsw_manager_get_config_path (GswManager *manager);
void          gsw_manager_set_global_option (GswManager *manager, const gchar *option, const gchar *value);
const gchar*  gsw_manager_get_global_option (GswManager *manager, const gchar *option);
const gchar*  gsw_manager_get_global_option_tip (GswManager *manager, const gchar *option);
const gchar*  gsw_manager_filter_text (GswManager *manager, const gchar *filterName, const gchar *text);
GList*        gsw_manager_get_global_options (GswManager *manager);
GList*        gsw_manager_get_global_option_values (GswManager *manager, const gchar *option);
void          gsw_manager_set_cipherkey (GswManager *manager, const gchar *modName, const gchar *key);
void          gsw_manager_set_javascript (GswManager *manager, gboolean valueBool);
GList*        gsw_manager_get_available_locales (GswManager *manager);
void          gsw_manager_set_default_locale (GswManager *manager, const gchar *name);
const gchar*  gsw_manager_translate (GswManager *manager, const char *text, const gchar *localeName);

G_END_DECLS

#endif /* __GSW_MANAGER_H__ */
