/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-installer.c: This file is part of ____
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

#include "gsw-installer.h"

using namespace sword;
using sword::VerseKey;
using sword::SWVersion;
using sword::SWBuf;
using sword::TreeKeyIdx;
using sword::InstallMgr;
using sword::SWModule;

#define GETINSTMGR(handle, failReturn) HandleInstMgr *hinstmgr = (HandleInstMgr *)handle; if (!hinstmgr) return failReturn; InstallMgr *installMgr = hinstmgr->installMgr; if (!installMgr) return failReturn;

namespace {
	class HandleInstMgr {
		public:
			static const char **remoteSources;
			InstallMgr *installMgr;
//			gsw_ModInfo *modInfo;
//			std::map<SWModule *, HandleSWModule *> moduleHandles;

//			MyStatusReporter statusReporter;
//			HandleInstMgr() : installMgr(0), modInfo(0) {}
			HandleInstMgr() : installMgr(0) {}
			HandleInstMgr(InstallMgr *mgr) {
				this->installMgr = installMgr;
//				this->modInfo = 0;
			}

			~HandleInstMgr() {
//				clearModInfo();
//				for (std::map<SWModule *, HandleSWModule *>::iterator it = moduleHandles.begin(); it != moduleHandles.end(); ++it) {
//					delete it->second;
//				}
				delete installMgr;
			}

//			HandleSWModule *getModuleHandle(SWModule *mod) {
//				if (!mod) return 0;
//				if (moduleHandles.find(mod) == moduleHandles.end()) {
//					moduleHandles[mod] = new HandleSWModule(mod);
//				}
//				return moduleHandles[mod];
//			}
//
//			static void clearRemoteSources() {
//				clearStringArray(&remoteSources);
//			}
//
//			void clearModInfo() {
//				clearModInfoArray(&modInfo);
//			}
	};
	const char **HandleInstMgr::remoteSources = 0;
}

GswInstaller* gsw_installer_new (const gchar *baseDir, GswStatusReporter *statusReporter)
{
	gchar *conf_path;
	conf_path = g_build_path(baseDir, "InstallMgr.conf", NULL);
	if (!g_file_test(conf_path, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (baseDir, 0755);
		SWConfig config(conf_path);
		config["General"]["PassiveFTP"] = "true";
		config.Save();
	}
	g_free(conf_path);

	HandleInstMgr *hinstmgr = new HandleInstMgr();
	StatusReporter* reporter = (StatusReporter*) gsw_status_reporter_get_class(statusReporter);
	//hinstmgr->statusReporter.init(statusReporter);
	//hinstmgr->installMgr = new InstallMgr(baseDir, &(hinstmgr->statusReporter));
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

//	hinstmgr->clearRemoteSources();
	sword::StringList vals = LocaleMgr::getSystemLocaleMgr()->getAvailableLocales();

//	const char **retVal = 0;
//	int count = 0;
//	for (InstallSourceMap::iterator it = installMgr->sources.begin(); it != installMgr->sources.end(); ++it) {
//		count++;
//	}
//	retVal = (const char **)calloc(count+1, sizeof(const char *));
//	count = 0;
	for (InstallSourceMap::iterator it = installMgr->sources.begin(); it != installMgr->sources.end(); ++it) {
		list = g_list_append(list, (gpointer) it->second->caption.c_str());

//		stdstr((char **)&(retVal[count++]), it->second->caption.c_str());
	}

//	hinstmgr->remoteSources = retVal;
	return list;
}
#if 0

/*
 * Class:     org_crosswire_sword_InstallMgr
 * Method:    refreshRemoteSource
 * Signature: (Ljava/lang/String;)I
 */
int  org_crosswire_sword_InstallMgr_refreshRemoteSource
  (SWHANDLE hInstallMgr, const char *sourceName) {

	GETINSTMGR(hInstallMgr, -1);

	InstallSourceMap::iterator source = installMgr->sources.find(sourceName);
	if (source == installMgr->sources.end()) {
		return -3;
	}

	return installMgr->refreshRemoteSource(source->second);
}

/*
 * Class:     org_crosswire_sword_InstallMgr
 * Method:    getRemoteModInfoList
 * Signature: (Lorg/crosswire/android/sword/SWMgr;Ljava/lang/String;)[Lorg/crosswire/android/sword/SWMgr/ModInfo;
 */
const struct org_crosswire_sword_ModInfo *  org_crosswire_sword_InstallMgr_getRemoteModInfoList
  (SWHANDLE hInstallMgr, SWHANDLE hSWMgr_deltaCompareTo, const char *sourceName) {

	GETINSTMGR(hInstallMgr, 0);
	GETSWMGR(hSWMgr_deltaCompareTo, 0);

	struct org_crosswire_sword_ModInfo *retVal = 0;

	hinstmgr->clearModInfo();

	InstallSourceMap::iterator source = installMgr->sources.find(sourceName);
	if (source == installMgr->sources.end()) {
		retVal = (struct org_crosswire_sword_ModInfo *)calloc(1, sizeof(struct org_crosswire_sword_ModInfo));
		hinstmgr->modInfo = retVal;
		return retVal;
	}

	std::map<SWModule *, int> modStats = installMgr->getModuleStatus(*mgr, *source->second->getMgr());

	int size = 0;
	for (std::map<SWModule *, int>::iterator it = modStats.begin(); it != modStats.end(); ++it) {
		size++;
	}
	retVal = (struct org_crosswire_sword_ModInfo *)calloc(size+1, sizeof(struct org_crosswire_sword_ModInfo));
	int i = 0;
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

		stdstr(&(retVal[i].name), assureValidUTF8(module->getName()));
		stdstr(&(retVal[i].description), assureValidUTF8(module->getDescription()));
		stdstr(&(retVal[i].category), assureValidUTF8(type.c_str()));
		stdstr(&(retVal[i].language), assureValidUTF8(module->getLanguage()));
		stdstr(&(retVal[i].version), assureValidUTF8(version.c_str()));
		stdstr(&(retVal[i++].delta), assureValidUTF8(statusString.c_str()));
		if (i >= size) break;
	}
	hinstmgr->modInfo = retVal;
	return retVal;
}

/*
 * Class:     org_crosswire_sword_InstallMgr
 * Method:    remoteInstallModule
 * Signature: (Lorg/crosswire/android/sword/SWMgr;Ljava/lang/String;Ljava/lang/String;)I
 */
int  org_crosswire_sword_InstallMgr_remoteInstallModule
  (SWHANDLE hInstallMgr_from, SWHANDLE hSWMgr_to, const char *sourceName, const char *modName) {

	GETINSTMGR(hInstallMgr_from, -1);
	GETSWMGR(hSWMgr_to, -1);

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

#define GswInstaller intptr_t
/*
 * Class:     org_crosswire_sword_InstallMgr
 * Method:    getRemoteModuleByName
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Lorg/crosswire/android/sword/SWModule;
 */
SWHANDLE  gsw_installer_get_Remote_Module_By_Name
  (SWHANDLE hInstallMgr, const char *sourceName, const char *modName) {

	GETINSTMGR(hInstallMgr, 0);

	InstallSourceMap::iterator source = installMgr->sources.find(sourceName);

	if (source == installMgr->sources.end()) {
		return 0;
	}

	SWMgr *mgr = source->second->getMgr();

	sword::SWModule *module = mgr->getModule(modName);

	if (!module) {
		return 0;
	}

	return (SWHANDLE)hinstmgr->getModuleHandle(module);

}
#endif
