/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-installer.h: This file is part of gsword.
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

#ifndef __GSW_INSTALLER_H__ 
#define __GSW_INSTALLER_H__  1

#include <glib.h>
#include "gsw-status-reporter.h"
#include "gsw-manager.h"

G_BEGIN_DECLS

typedef void GswInstaller;

GswInstaller* gsw_installer_new                            (const char *baseDir, GswStatusReporter *statusReporter);
void          gsw_installer_delete                         (GswInstaller* installer);
void          gsw_installer_set_user_disclaimer_confirmed  (GswInstaller* installer);
int           gsw_installer_sync_config                    (GswInstaller* installer);
int           gsw_installer_uninstall_module               (GswInstaller* installer, GswManager* manager, const char *modName);
GList*        gsw_installer_get_remote_sources             (GswInstaller* installer);
int           gsw_installer_refresh_remote_source          (GswInstaller* installer, const char *sourceName);
GList*        gsw_installer_get_remote_modinfo_list        (GswInstaller* installer, GswManager* manager, const char *sourceName);
int           gsw_installer_remote_install_module          (GswInstaller* installer, GswManager* manager, const char *sourceName, const char *modName);
GswModule*    gsw_installer_get_remote_module_by_name      (GswInstaller* installer, const char *sourceName, const char *modName);
void          gsw_installer_terminate                      (GswInstaller* installer);
void          gsw_installer_reset_config                   (GswInstaller *installer, const gchar *baseDir);
void          gsw_installer_reload_config                  (GswInstaller *installer);

G_END_DECLS

#endif /* __GSW_INSTALLER_H__ */
