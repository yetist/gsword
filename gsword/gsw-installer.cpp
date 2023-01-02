/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-installer.c: This file is part of gsword.
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

#include <iostream>
#include <vector>
#include <map>

#include <swversion.h>
#include <swmgr.h>
#include <installmgr.h>
#include <remotetrans.h>
#include <swmodule.h>
#include <versekey.h>
#include <treekeyidx.h>
#include <filemgr.h>
#include <swbuf.h>
#include <localemgr.h>
#include <utilstr.h>

#include <glib/gstdio.h>
#include "gsw-installer.h"
#include "gsw-modinfo.h"

using namespace sword;

namespace {
	class HandleInstMgr {
		public:
			InstallMgr *installMgr;
			HandleInstMgr() : installMgr(0) {}
			HandleInstMgr(InstallMgr *mgr) {
				this->installMgr = installMgr;
			}

			~HandleInstMgr() {
				delete installMgr;
			}

	};
}

GswInstaller* gsw_installer_new (const gchar *baseDir, GswStatusReporter *statusReporter)
{
	gchar *conf_path;
	conf_path = g_build_filename(baseDir, "InstallMgr.conf", NULL);
	if (!g_file_test(conf_path, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (baseDir, 0755);
		SWConfig config(conf_path);
		config["General"]["PassiveFTP"] = "true";

		InstallSource is("FTP");
		is.caption = "CrossWire";
		is.source = "ftp.crosswire.org";
		is.directory = "/pub/sword/raw";

		config["General"]["PassiveFTP"] = "true";
		config["Sources"]["FTPSource"] = is.getConfEnt();
		config.Save();

		InstallSource is_local("DIR");
		is_local.caption = "cdrom";
		is_local.source = "[local]";
		is_local.directory = "/mnt/cdrom";
		config["Sources"]["DIRSource"] = is_local.getConfEnt();
		config.Save();
	}
	g_free(conf_path);

	HandleInstMgr *hinstmgr = new HandleInstMgr();
	StatusReporter* reporter = (StatusReporter*) gsw_status_reporter_get_class(statusReporter);
	hinstmgr->installMgr = new InstallMgr(baseDir, reporter);
	return (GswInstaller*) hinstmgr;
}

void  gsw_installer_delete (GswInstaller* installer)
{
	HandleInstMgr *hinstMgr = (HandleInstMgr *) installer;
	if (hinstMgr)
		delete hinstMgr;
}

void gsw_installer_set_user_disclaimer_confirmed  (GswInstaller* installer)
{
	HandleInstMgr *hinstmgr = (HandleInstMgr *) installer;
	if (!hinstmgr)
		return;
	InstallMgr *installMgr = hinstmgr->installMgr;
	if (!installMgr)
		return;

	installMgr->setUserDisclaimerConfirmed(TRUE);
}

int gsw_installer_sync_config (GswInstaller* installer)
{
	HandleInstMgr *hinstmgr = (HandleInstMgr *) installer;
	if (!hinstmgr)
		return -1;
	InstallMgr *installMgr = hinstmgr->installMgr;
	if (!installMgr)
		return -1;
	return installMgr->refreshRemoteSourceConfiguration();
}

int gsw_installer_uninstall_module (GswInstaller* installer, GswManager* manager, const char *modName)
{
	HandleInstMgr *hinstmgr = (HandleInstMgr *) installer;
	if (!hinstmgr)
		return -1;
	InstallMgr *installMgr = hinstmgr->installMgr;
	if (!installMgr)
		return -1;

	SWMgr *mgr;
	mgr = (SWMgr*)manager;
	if (!mgr)
		return -1;

	SWModule *module;
	ModMap::iterator it = mgr->Modules.find(modName);
	if (it == mgr->Modules.end()) {
		return -2;
	}
	module = it->second;
	return installMgr->removeModule(mgr, module->getName());
}

GList* gsw_installer_get_remote_sources (GswInstaller* installer)
{
	HandleInstMgr *hinstmgr = (HandleInstMgr *) installer;
	if (!hinstmgr)
		return NULL;
	InstallMgr *installMgr = hinstmgr->installMgr;
	if (!installMgr)
		return NULL;
	GList *list = NULL;

	sword::StringList vals = LocaleMgr::getSystemLocaleMgr()->getAvailableLocales();

	for (InstallSourceMap::iterator it = installMgr->sources.begin(); it != installMgr->sources.end(); ++it) {
		list = g_list_append(list, (gpointer) it->second->caption.c_str());
	}

	return list;
}

int  gsw_installer_refresh_remote_source (GswInstaller* installer, const char *sourceName)
{

	HandleInstMgr *hinstmgr = (HandleInstMgr *) installer;
	if (!hinstmgr)
		return -1;
	InstallMgr *installMgr = hinstmgr->installMgr;
	if (!installMgr)
		return -1;

	InstallSourceMap::iterator source = installMgr->sources.find(sourceName);
	if (source == installMgr->sources.end()) {
		return -3;
	}

	return installMgr->refreshRemoteSource(source->second);
}

GList* gsw_installer_get_remote_modinfo_list (GswInstaller* installer, GswManager* manager, const char *sourceName)
{
	HandleInstMgr *hinstmgr = (HandleInstMgr *) installer;
	if (!hinstmgr)
		return NULL;
	InstallMgr *installMgr = hinstmgr->installMgr;
	if (!installMgr)
		return NULL;

	SWMgr *mgr;
	mgr = (SWMgr*)manager;
	if (!mgr)
		return NULL;

	GList *list = NULL;

	InstallSourceMap::iterator source = installMgr->sources.find(sourceName);

	std::map<SWModule *, int> modStats = installMgr->getModuleStatus(*mgr, *source->second->getMgr());

	for (std::map<SWModule *, int>::iterator it = modStats.begin(); it != modStats.end(); ++it) {
		SWModule *module = it->first;
		int status = it->second;

		SWBuf version = module->getConfigEntry("Version");
		SWBuf statusString = " ";
		if (status & InstallMgr::MODSTAT_NEW) statusString = "*";
		if (status & InstallMgr::MODSTAT_OLDER) statusString = "-";
		if (status & InstallMgr::MODSTAT_UPDATED) statusString = "+";

		SWBuf type = module->getType();
		SWBuf cat = module->getConfigEntry("Category");
		if (cat.length() > 0) type = cat;

		GswModinfo* modinfo;
		modinfo = gsw_modinfo_new (
				g_strdup(assureValidUTF8(module->getName())),
				g_strdup(assureValidUTF8(module->getDescription())),
				g_strdup(assureValidUTF8(type.c_str())),
				g_strdup(assureValidUTF8(module->getLanguage())),
				g_strdup(assureValidUTF8(version.c_str())),
				g_strdup(assureValidUTF8(statusString.c_str()))
				);
		list = g_list_append(list, modinfo);

	}
	return list;
}

int gsw_installer_remote_install_module (GswInstaller* installer, GswManager* manager, const char *sourceName, const char *modName)
{
	HandleInstMgr *hinstmgr = (HandleInstMgr *) installer;
	if (!hinstmgr)
		return -1;
	InstallMgr *installMgr = hinstmgr->installMgr;
	if (!installMgr)
		return -1;

	SWMgr *mgr;
	mgr = (SWMgr*)manager;
	if (!mgr)
		return -1;

	InstallSourceMap::iterator source = installMgr->sources.find(sourceName);

	if (source == installMgr->sources.end()) {
		return -3;
	}

	InstallSource *is = source->second;
	SWMgr *rmgr = is->getMgr();
	SWModule *module;

	ModMap::iterator it = rmgr->Modules.find(modName);

	if (it == rmgr->Modules.end()) {
		return -4;
	}

	module = it->second;

	int error = installMgr->installModule(mgr, 0, module->getName(), is);

	return error;
}

GswModule* gsw_installer_get_remote_module_by_name (GswInstaller* installer, const char *sourceName, const char *modName)
{
	HandleInstMgr *hinstmgr = (HandleInstMgr *) installer;
	if (!hinstmgr)
		return NULL;
	InstallMgr *installMgr = hinstmgr->installMgr;
	if (!installMgr)
		return NULL;

	InstallSourceMap::iterator source = installMgr->sources.find(sourceName);

	if (source == installMgr->sources.end()) {
		return NULL;
	}

	SWMgr *mgr = source->second->getMgr();

	sword::SWModule *module = mgr->getModule(modName);

	if (!module) {
		return NULL;
	}

	return  gsw_module_new(module);

}

void gsw_installer_terminate (GswInstaller* installer)
{
	HandleInstMgr *hinstmgr = (HandleInstMgr *) installer;
	if (!hinstmgr)
		return;
	InstallMgr *installMgr = hinstmgr->installMgr;
	if (!installMgr)
		return;
	installMgr->terminate();
}


void gsw_installer_reset_config (GswInstaller *installer, const gchar *baseDir)
{
	HandleInstMgr *hinstmgr = (HandleInstMgr *) installer;
	if (!hinstmgr)
		return;
	InstallMgr *installMgr = hinstmgr->installMgr;
	if (!installMgr)
		return;

	gchar *conf_path;
	conf_path = g_build_filename(baseDir, "InstallMgr.conf", NULL);
	if (g_file_test(conf_path, G_FILE_TEST_EXISTS)) {
		g_remove(conf_path);
	}
	SWConfig config(conf_path);
	config["General"]["PassiveFTP"] = "true";
	config.Save();
	gsw_installer_reload_config(installer);
}

void gsw_installer_reload_config(GswInstaller *installer)
{
	HandleInstMgr *hinstmgr = (HandleInstMgr *) installer;
	if (!hinstmgr)
		return;
	InstallMgr *installMgr = hinstmgr->installMgr;
	if (!installMgr)
		return;
	installMgr->readInstallConf();
}
