/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-manager.h: This file is part of ____
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

G_BEGIN_DECLS

typedef void GswManager;

GswManager* gsw_manager_new (void);

/*
SWHANDLE  gsw_manager_newWithPath (const char *path);
void          gsw_manager_delete (SWHANDLE hSWMgr);
const char *  gsw_manager_version (SWHANDLE hSWMgr);
const struct org_crosswire_sword_ModInfo *  gsw_manager_getModInfoList (SWHANDLE hSWMgr);
SWHANDLE      gsw_manager_getModuleByName (SWHANDLE hSWMgr, const char *moduleName);
const char *  gsw_manager_getPrefixPath (SWHANDLE hSWMgr);
const char *  gsw_manager_getConfigPath (SWHANDLE hSWMgr);
void          gsw_manager_setGlobalOption (SWHANDLE hSWMgr, const char *option, const char *value);
const char *  gsw_manager_getGlobalOption (SWHANDLE hSWMgr, const char *option);
const char *  gsw_manager_getGlobalOptionTip (SWHANDLE hSWMgr, const char *option);
const char *  gsw_manager_filterText (SWHANDLE hSWMgr, const char *filterName, const char *text);
const char **  gsw_manager_getGlobalOptions (SWHANDLE hSWMgr);
const char **  gsw_manager_getGlobalOptionValues (SWHANDLE hSWMgr, const char *option);
void           gsw_manager_setCipherKey (SWHANDLE hSWMgr, const char *modName, const char *key);
void           gsw_manager_setJavascript (SWHANDLE hSWMgr, char valueBool);
const char **  gsw_manager_getAvailableLocales (SWHANDLE hSWMgr);
void           gsw_manager_setDefaultLocale (SWHANDLE hSWMgr, const char *name);
const gchar *  gsw_manager_translate (SWHANDLE hSWMgr, const char *text, const char *localeName);
*/

G_END_DECLS

#endif /* __GSW_MANAGER_H__ */
