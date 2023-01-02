/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-manager.c: This file is part of ____
 *
 * Copyright (C) 2017 yetist <yetist@yetibook>
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

using sword::VerseKey;
using sword::SWVersion;
using sword::SWBuf;
using sword::TreeKeyIdx;


#define GETSWMGR(handle, failReturn) HandleSWMgr *hmgr = (HandleSWMgr *)handle; if (!hmgr) return failReturn; WebMgr *mgr = hmgr->mgr; if (!mgr) return failReturn;

#define GETSWMODULE(handle, failReturn) HandleSWModule *hmod = (HandleSWModule *)handle; if (!hmod) return failReturn; SWModule *module = hmod->mod; if (!module) return failReturn;

#define GETINSTMGR(handle, failReturn) HandleInstMgr *hinstmgr = (HandleInstMgr *)handle; if (!hinstmgr) return failReturn; InstallMgr *installMgr = hinstmgr->installMgr; if (!installMgr) return failReturn;

namespace {
	class HandleSWMgr {
		public:
			WebMgr *mgr;
			gsw_ModInfo *modInfo;
			std::map<SWModule *, HandleSWModule *> moduleHandles;
			SWBuf filterBuf;
			static const char **globalOptions;
			static const char **globalOptionValues;
			static const char **availableLocales;

			HandleSWMgr(WebMgr *mgr) {
				this->mgr = mgr;
				this->modInfo = 0;
			}

			void clearModInfo() {
				clearModInfoArray(&modInfo);
			}

			~HandleSWMgr() {
				clearModInfo();
				for (std::map<SWModule *, HandleSWModule *>::iterator it = moduleHandles.begin(); it != moduleHandles.end(); ++it) {
					delete it->second;
				}
				delete mgr;
			}

			HandleSWModule *getModuleHandle(SWModule *mod) {
				if (!mod) return 0;
				if (moduleHandles.find(mod) == moduleHandles.end()) {
					moduleHandles[mod] = new HandleSWModule(mod);
				}
				return moduleHandles[mod];
			}

			static void clearGlobalOptions() {
				clearStringArray(&globalOptions);
			}

			static void clearGlobalOptionValues() {
				clearStringArray(&globalOptionValues);
			}

			static void clearAvailableLocales() {
				clearStringArray(&availableLocales);
			}
	};
}

SWHANDLE  gsw_SWMgr_new() {
	SWConfig *sysConf = 0;
	return (SWHANDLE) new HandleSWMgr(new WebMgr(sysConf));
}

SWHANDLE  gsw_SWMgr_newWithPath(const char *path) {
	SWBuf confPath = path;
	if (!confPath.endsWith("/")) confPath.append('/');
	SWBuf modsd = confPath + "mods.d";
	// be sure we have at least some config file already out there
	if (!FileMgr::existsFile(modsd.c_str())) {
		modsd.append("/globals.conf");
		FileMgr::createParent(modsd.c_str());
		SWConfig config(modsd.c_str());
		config["Globals"]["HiAndroid"] = "weeee";
		config.Save();
	}
	return (SWHANDLE) new HandleSWMgr(new WebMgr(confPath.c_str()));
}


void  gsw_SWMgr_delete(SWHANDLE hSWMgr) {
	HandleSWMgr *hmgr = (HandleSWMgr *)hSWMgr;
	if (hmgr) delete hmgr;
}

const char *  gsw_SWMgr_version (SWHANDLE hSWMgr)
{
	// we don't actually need an SWMgr to get version
	static SWVersion v;
	return v.currentVersion;
}

const struct gsw_ModInfo *  gsw_SWMgr_getModInfoList (SWHANDLE hSWMgr)
{

	GETSWMGR(hSWMgr, 0);

	sword::SWModule *module = 0;

	hmgr->clearModInfo();

	int size = 0;
	for (sword::ModMap::iterator it = mgr->Modules.begin(); it != mgr->Modules.end(); ++it) {
		if ((!(it->second->getConfigEntry("CipherKey"))) || (*(it->second->getConfigEntry("CipherKey"))))
			size++;
	}

	struct gsw_ModInfo *milist = (struct gsw_ModInfo *)calloc(size+1, sizeof(struct gsw_ModInfo));
	int i = 0;
	for (sword::ModMap::iterator it = mgr->Modules.begin(); it != mgr->Modules.end(); ++it) {
		module = it->second;
		if ((!(module->getConfigEntry("CipherKey"))) || (*(module->getConfigEntry("CipherKey")))) {
			SWBuf type = module->getType();
			SWBuf cat = module->getConfigEntry("Category");
			SWBuf version = module->getConfigEntry("Version");
			if (cat.length() > 0) type = cat;
			stdstr(&(milist[i].name), assureValidUTF8(module->getName()));
			stdstr(&(milist[i].description), assureValidUTF8(module->getDescription()));
			stdstr(&(milist[i].category), assureValidUTF8(type.c_str()));
			stdstr(&(milist[i++].language), assureValidUTF8(module->getLanguage()));
			stdstr(&(milist[i++].version), assureValidUTF8(version.c_str()));
			stdstr(&(milist[i++].delta), "");
			if (i >= size) break;
		}
	}
	hmgr->modInfo = milist;
	return milist;
}

SWHANDLE  gsw_SWMgr_getModuleByName (SWHANDLE hSWMgr, const char *moduleName)
{

	GETSWMGR(hSWMgr, 0);

	return (SWHANDLE)hmgr->getModuleHandle(hmgr->mgr->getModule(moduleName));
}

const char *  gsw_SWMgr_getPrefixPath (SWHANDLE hSWMgr)
{

	GETSWMGR(hSWMgr, 0);

	return mgr->prefixPath;
}

const char *  gsw_SWMgr_getConfigPath (SWHANDLE hSWMgr) 
{

	GETSWMGR(hSWMgr, 0);

	return mgr->configPath;
}

void  gsw_SWMgr_setGlobalOption (SWHANDLE hSWMgr, const char *option, const char *value)
{

	GETSWMGR(hSWMgr, );
	mgr->setGlobalOption(option, value);
}

const char *  gsw_SWMgr_getGlobalOption (SWHANDLE hSWMgr, const char *option)
{

	GETSWMGR(hSWMgr, 0);

	return mgr->getGlobalOption(option);
}

const char *  gsw_SWMgr_getGlobalOptionTip (SWHANDLE hSWMgr, const char *option)
{

	GETSWMGR(hSWMgr, 0);

	return mgr->getGlobalOptionTip(option);
}

const char *  gsw_SWMgr_filterText (SWHANDLE hSWMgr, const char *filterName, const char *text)
{

	GETSWMGR(hSWMgr, 0);

	hmgr->filterBuf = text;

	// why was this in bindings/corba/omniorb?
	//	mgr->setGlobalOption("Greek Accents", "Off");

	char errStatus = mgr->filterText(filterName, hmgr->filterBuf);
	(void)errStatus;
	return hmgr->filterBuf.c_str();
}

const char **  gsw_SWMgr_getGlobalOptions (SWHANDLE hSWMgr)
{

	GETSWMGR(hSWMgr, 0);

	const char **retVal;
	hmgr->clearGlobalOptions();

	sword::StringList options = mgr->getGlobalOptions();
	int count = 0;
	for (sword::StringList::iterator it = options.begin(); it != options.end(); ++it) {
		count++;
	}
	retVal = (const char **)calloc(count+1, sizeof(const char *));
	count = 0;
	for (sword::StringList::iterator it = options.begin(); it != options.end(); ++it) {
		stdstr((char **)&(retVal[count++]), it->c_str());
	}

	hmgr->globalOptions = retVal;
	return retVal;
}

const char **  gsw_SWMgr_getGlobalOptionValues (SWHANDLE hSWMgr, const char *option)
{

	GETSWMGR(hSWMgr, 0);

	const char **retVal = 0;
	hmgr->clearGlobalOptionValues();

	sword::StringList options = mgr->getGlobalOptionValues(option);
	int count = 0;
	for (sword::StringList::iterator it = options.begin(); it != options.end(); ++it) {
		count++;
	}
	retVal = (const char **)calloc(count+1, sizeof(const char *));
	count = 0;
	for (sword::StringList::iterator it = options.begin(); it != options.end(); ++it) {
		stdstr((char **)&(retVal[count++]), it->c_str());
	}

	hmgr->globalOptionValues = retVal;
	return retVal;
}

void  gsw_SWMgr_setCipherKey (SWHANDLE hSWMgr, const char *modName, const char *key)
{

	GETSWMGR(hSWMgr, );

	mgr->setCipherKey(modName, key);
}

void  gsw_SWMgr_setJavascript (SWHANDLE hSWMgr, char valueBool)
{

	GETSWMGR(hSWMgr, );

	mgr->setJavascript(valueBool);
}

const char **  gsw_SWMgr_getAvailableLocales (SWHANDLE hSWMgr)
{
	GETSWMGR(hSWMgr, 0);
	hmgr->clearAvailableLocales();
	sword::StringList localeNames = LocaleMgr::getSystemLocaleMgr()->getAvailableLocales();
	const char **retVal = 0;
	int count = 0;
	for (sword::StringList::iterator it = localeNames.begin(); it != localeNames.end(); ++it) {
		count++;
	}
	retVal = (const char **)calloc(count+1, sizeof(const char *));
	count = 0;
	for (sword::StringList::iterator it = localeNames.begin(); it != localeNames.end(); ++it) {
		stdstr((char **)&(retVal[count++]), it->c_str());
	}

	hmgr->availableLocales = retVal;
	return retVal;
}

void  gsw_SWMgr_setDefaultLocale (SWHANDLE hSWMgr, const char *name)
{
	// we don't actually need an SWMgr instance for this
	GETSWMGR(hSWMgr, );

	LocaleMgr::getSystemLocaleMgr()->setDefaultLocaleName(name);
}

const char *  gsw_SWMgr_translate (SWHANDLE hSWMgr, const char *text, const char *localeName)
{

	GETSWMGR(hSWMgr, 0);

	return LocaleMgr::getSystemLocaleMgr()->translate(text, localeName);
}
