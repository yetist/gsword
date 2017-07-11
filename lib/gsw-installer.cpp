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

enum {
    LAST_SIGNAL
};

enum {
    PROP_0,
};

struct _GswInstaller
{
  GObject      object;
};

static GParamSpec *properties[LAST_PROP] = { NULL };
G_DEFINE_TYPE (GswInstaller, gsw_installer, G_TYPE_OBJECT);

static void
gsw_installer_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    GswInstaller *installer;

    installer = GSW_INSTALLER (object);

    switch (prop_id)
    {

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gsw_installer_get_property (GObject      *object,
                            guint         prop_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
    GswInstaller *installer;

    installer = GSW_INSTALLER (object);

    switch (prop_id)
    {

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gsw_installer_class_init (GswInstallerClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);

    gobject_class->set_property = gsw_installer_set_property;
    gobject_class->get_property = gsw_installer_get_property;

    //g_type_class_add_private (class, sizeof (GswInstallerPrivate));
    g_object_class_install_properties (gobject_class, LAST_PROP, properties);
}

static void
gsw_installer_init (GswInstaller *installer)
{
    GswInstallerPrivate *priv;

    priv = GSW_INSTALLER_GET_PRIVATE (installer);

}

GswInstaller*
gsw_installer_new (void)
{
    return g_object_new (GSW_TYPE_INSTALLER, NULL);
}

#if 0
class HandleInstMgr {
public:
	static const char **remoteSources;
	InstallMgr *installMgr;
	org_crosswire_sword_ModInfo *modInfo;
	std::map<SWModule *, HandleSWModule *> moduleHandles;

	MyStatusReporter statusReporter;
	HandleInstMgr() : installMgr(0), modInfo(0) {}
	HandleInstMgr(InstallMgr *mgr) {
		this->installMgr = installMgr;
		this->modInfo = 0;
	}

	~HandleInstMgr() {
		clearModInfo();
		for (std::map<SWModule *, HandleSWModule *>::iterator it = moduleHandles.begin(); it != moduleHandles.end(); ++it) {
			delete it->second;
		}
		delete installMgr;
	}

	HandleSWModule *getModuleHandle(SWModule *mod) {
		if (!mod) return 0;
		if (moduleHandles.find(mod) == moduleHandles.end()) {
			moduleHandles[mod] = new HandleSWModule(mod);
		}
		return moduleHandles[mod];
	}

	static void clearRemoteSources() {
		clearStringArray(&remoteSources);
	}

	void clearModInfo() {
		clearModInfoArray(&modInfo);
	}
};

#define GswInstaller intptr_t

GswInstaller*     gsw_installer_new                (void);

SWHANDLE  org_crosswire_sword_InstallMgr_new (const char *baseDir, SWHANDLE statusReporter)
{
	SWBuf confPath = SWBuf(baseDir) + "/InstallMgr.conf";
	// be sure we have at least some config file already out there
	if (!FileMgr::existsFile(confPath.c_str())) {
		FileMgr::createParent(confPath.c_str());
//		remove(confPath.c_str());

		SWConfig config(confPath.c_str());
		config["General"]["PassiveFTP"] = "true";
		config.Save();
	}
	HandleInstMgr *hinstmgr = new HandleInstMgr();
	hinstmgr->statusReporter.init(statusReporter);
	hinstmgr->installMgr = new InstallMgr(baseDir, &(hinstmgr->statusReporter));
	return (SWHANDLE) hinstmgr;
}

/*
 * Class:     org_crosswire_sword_InstallMgr
 * Method:    delete
 * Signature: ()V
 */
void  org_crosswire_sword_InstallMgr_delete
  (SWHANDLE hInstallMgr) {
	HandleInstMgr *hinstMgr = (HandleInstMgr *)hInstallMgr;
	if (hinstMgr) delete hinstMgr;
}

/*
 * Class:     org_crosswire_sword_InstallMgr
 * Method:    setUserDisclaimerConfirmed
 * Signature: ()V
 */
void  org_crosswire_sword_InstallMgr_setUserDisclaimerConfirmed
  (SWHANDLE hInstallMgr) {

	GETINSTMGR(hInstallMgr, );

	installMgr->setUserDisclaimerConfirmed(true);
}

/*
 * Class:     org_crosswire_sword_InstallMgr
 * Method:    syncConfig
 * Signature: ()I
 */
int  org_crosswire_sword_InstallMgr_syncConfig
  (SWHANDLE hInstallMgr) {

	GETINSTMGR(hInstallMgr, -1);

	return installMgr->refreshRemoteSourceConfiguration();
}

/*
 * Class:     org_crosswire_sword_InstallMgr
 * Method:    uninstallModule
 * Signature: (Lorg/crosswire/android/sword/SWMgr;Ljava/lang/String;)I
 */
int  org_crosswire_sword_InstallMgr_uninstallModule
  (SWHANDLE hInstallMgr, SWHANDLE hSWMgr_removeFrom, const char *modName) {

	GETINSTMGR(hInstallMgr, -1);
	GETSWMGR(hSWMgr_removeFrom, -1);

	SWModule *module;
	ModMap::iterator it = mgr->Modules.find(modName);
	if (it == mgr->Modules.end()) {
		return -2;
	}
	module = it->second;
	return installMgr->removeModule(mgr, module->getName());
}

/*
 * Class:     org_crosswire_sword_InstallMgr
 * Method:    getRemoteSources
 * Signature: ()[Ljava/lang/String;
 */
const char **  org_crosswire_sword_InstallMgr_getRemoteSources
  (SWHANDLE hInstallMgr) {

	GETINSTMGR(hInstallMgr, 0);

	hinstmgr->clearRemoteSources();
	sword::StringList vals = LocaleMgr::getSystemLocaleMgr()->getAvailableLocales();
	const char **retVal = 0;
	int count = 0;
	for (InstallSourceMap::iterator it = installMgr->sources.begin(); it != installMgr->sources.end(); ++it) {
		count++;
	}
	retVal = (const char **)calloc(count+1, sizeof(const char *));
	count = 0;
	for (InstallSourceMap::iterator it = installMgr->sources.begin(); it != installMgr->sources.end(); ++it) {
		stdstr((char **)&(retVal[count++]), it->second->caption.c_str());
	}

	hinstmgr->remoteSources = retVal;
	return retVal;
}

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
