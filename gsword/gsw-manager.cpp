/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-manager.c: This file is part of gsword.
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
#include <swversion.h>
#include <swmgr.h>
#include <swmodule.h>
#include <swbuf.h>
#include <localemgr.h>
#include <utilstr.h>
#include <markupfiltmgr.h>
#include <osiswordjs.h>
#include <thmlwordjs.h>
#include <gbfwordjs.h>

#include "gsw-manager.h"
#include "gsw-modinfo.h"

namespace {
	using namespace sword;
	class WebMgr : public SWMgr {
		OSISWordJS *osisWordJS;
		ThMLWordJS *thmlWordJS;
		GBFWordJS *gbfWordJS;
		SWModule *defaultGreekLex;
		SWModule *defaultHebLex;
		SWModule *defaultGreekParse;
		SWModule *defaultHebParse;

		public:
		WebMgr(const gchar *path) : SWMgr(path, false, new MarkupFilterMgr(FMT_WEBIF)) { init(); }
		WebMgr(SWConfig *sysConf) : SWMgr(0, sysConf, false, new MarkupFilterMgr(FMT_WEBIF)) { init(); }
		void init() {
			defaultGreekLex   = 0;
			defaultHebLex     = 0;
			defaultGreekParse = 0;
			defaultHebParse   = 0;

			osisWordJS = new OSISWordJS();
			thmlWordJS = new ThMLWordJS();
			gbfWordJS = new GBFWordJS();
			Load();
			osisWordJS->setDefaultModules(defaultGreekLex, defaultHebLex, defaultGreekParse, defaultHebParse);
			thmlWordJS->setDefaultModules(defaultGreekLex, defaultHebLex, defaultGreekParse, defaultHebParse);
			gbfWordJS->setDefaultModules(defaultGreekLex, defaultHebLex, defaultGreekParse, defaultHebParse);
			osisWordJS->setMgr(this);
			thmlWordJS->setMgr(this);
			gbfWordJS->setMgr(this);
			setGlobalOption("Textual Variants", "Primary Reading");
		}

		~WebMgr() {
			delete osisWordJS;
			delete thmlWordJS;
			delete gbfWordJS;
		}

		void AddGlobalOptions(SWModule *module, ConfigEntMap &section, ConfigEntMap::iterator start, ConfigEntMap::iterator end) {

			// ThML word stuff needs to process before strongs strip
			if (module->getMarkup() == FMT_THML) {
				module->addOptionFilter(thmlWordJS);
			}

			if (module->getMarkup() == FMT_GBF) {
				module->addOptionFilter(gbfWordJS);
			}

			// add other module filters
			SWMgr::AddGlobalOptions(module, section, start, end);

			// add our special filters
			if (module->getConfig().has("Feature", "GreekDef")) {
				defaultGreekLex = module;
			}
			if (module->getConfig().has("Feature", "HebrewDef")) {
				defaultHebLex = module;
			}
			if (module->getConfig().has("Feature", "GreekParse")) {
				defaultGreekParse = module;
			}
			if (module->getConfig().has("Feature", "HebrewParse")) {
				defaultHebParse = module;
			}
			if (module->getConfig().has("GlobalOptionFilter", "ThMLVariants")) {
				OptionFilterMap::iterator it = optionFilters.find("ThMLVariants");
				if (it != optionFilters.end()) {
					module->addOptionFilter((*it).second);	// add filter to module and option as a valid option
				}
			}

			if (module->getMarkup() == FMT_OSIS) {
				module->addOptionFilter(osisWordJS);
			}
		}
		void setJavascript(bool val) {
			osisWordJS->setOptionValue((val)?"On":"Off");
			thmlWordJS->setOptionValue((val)?"On":"Off");
			gbfWordJS->setOptionValue((val)?"On":"Off");
		}
	};
}

GswManager* gsw_manager_new (void)
{
	SWConfig *sysConf = NULL;
	return (GswManager*) new WebMgr(sysConf);
}

GswManager* gsw_manager_new_with_path (const gchar *path)
{
	GswManager *manager;
	g_autofree gchar* modsd = g_build_path(path, "mods.d", NULL);
	g_autofree gchar* confpath = g_build_path(modsd, "globals.conf", NULL);

	// be sure we have at least some config file already out there
	if (!g_file_test(modsd, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (modsd, 0755);
		SWConfig config(confpath);
		config["Globals"]["HiAndroid"] = "weeee";
		config.Save();
	}

	manager = new WebMgr(confpath);
	return manager;
}

void gsw_manager_delete (GswManager *manager)
{
	WebMgr *mgr;
	mgr = (WebMgr *)manager;
	if (mgr)
		delete mgr;
}

const gchar* gsw_manager_get_version (GswManager *manager)
{
	// we don't actually need an SWMgr to get version
	static SWVersion v;
	return v.currentVersion;
}

GList* gsw_manager_get_modinfo_list (GswManager *manager)
{
	WebMgr *mgr;
	sword::SWModule *module = NULL;
	GList *milist = NULL;

	mgr = (WebMgr *)manager;
	if (!mgr)
		return NULL;

	for (sword::ModMap::iterator it = mgr->Modules.begin(); it != mgr->Modules.end(); ++it) {
		module = it->second;
		if ((!(module->getConfigEntry("CipherKey"))) || (*(module->getConfigEntry("CipherKey")))) {
			GswModinfo*  modinfo;
			SWBuf type = module->getType();
			SWBuf cat = module->getConfigEntry("Category");
			if (cat.length() > 0) type = cat;
			SWBuf version = module->getConfigEntry("Version");

			modinfo = gsw_modinfo_new (
					assureValidUTF8(module->getName()),
					assureValidUTF8(module->getDescription()),
					assureValidUTF8(type.c_str()),
					assureValidUTF8(module->getLanguage()),
					assureValidUTF8(version.c_str()),
					"");
			milist = g_list_append(milist, modinfo);
		}
	}
	return milist;
}

GswModule* gsw_manager_get_module_by_name (GswManager *manager, const gchar *moduleName)
{
	WebMgr *mgr;
	SWModule *mod;
	GswModule *module;

	mgr = (WebMgr *)manager;
	if (!mgr)
		return NULL;

	mod = mgr->getModule(moduleName);
	module = gsw_module_new(mod);
	return module;
}

const gchar* gsw_manager_get_prefix_path (GswManager *manager)
{
	WebMgr *mgr;
	mgr = (WebMgr *)manager;
	if (!mgr)
		return NULL;
	return mgr->prefixPath;
}

const gchar*  gsw_manager_get_config_path (GswManager *manager)
{
	WebMgr *mgr;
	mgr = (WebMgr *)manager;
	if (!mgr)
		return NULL;
	return mgr->configPath;
}

void  gsw_manager_set_global_option (GswManager *manager, const gchar *option, const gchar *value)
{
	WebMgr *mgr;
	mgr = (WebMgr *)manager;
	if (!mgr)
		return;
	mgr->setGlobalOption(option, value);
}

const gchar*  gsw_manager_get_global_option (GswManager *manager, const gchar *option)
{
	WebMgr *mgr;

	mgr = (WebMgr *)manager;
	if (!mgr)
		return NULL;
	return mgr->getGlobalOption(option);
}

const gchar*  gsw_manager_get_global_option_tip (GswManager *manager, const gchar *option)
{
	WebMgr *mgr;
	mgr = (WebMgr *)manager;
	if (!mgr)
		return NULL;

	return mgr->getGlobalOptionTip(option);
}

const gchar*  gsw_manager_filter_text (GswManager *manager, const gchar *filterName, const gchar *text)
{
	WebMgr *mgr;
	mgr = (WebMgr *)manager;
	if (!mgr)
		return NULL;

	SWBuf filterBuf = text;

	// why was this in bindings/corba/omniorb?
	//	mgr->setGlobalOption("Greek Accents", "Off");
	gboolean errStatus = mgr->filterText(filterName, filterBuf);
	(void)errStatus;
	return filterBuf.c_str();
}

GList* gsw_manager_get_global_options (GswManager *manager)
{
	GList *list = NULL;
	WebMgr *mgr;

	mgr = (WebMgr *)manager;
	if (!mgr)
		return NULL;

	sword::StringList options = mgr->getGlobalOptions();
	for (sword::StringList::iterator it = options.begin(); it != options.end(); ++it) {
		list = g_list_append(list, g_strdup(it->c_str()));
	}
	return list;
}

GList* gsw_manager_get_global_option_values (GswManager *manager, const gchar *option)
{
	GList *list = NULL;
	WebMgr *mgr;

	mgr = (WebMgr *)manager;
	if (!mgr)
		return NULL;

	sword::StringList options = mgr->getGlobalOptionValues(option);
	for (sword::StringList::iterator it = options.begin(); it != options.end(); ++it) {
		list = g_list_append(list, g_strdup(it->c_str()));
	}
	return list;
}

void gsw_manager_set_cipherkey (GswManager *manager, const gchar *modName, const gchar *key)
{
	WebMgr *mgr;

	mgr = (WebMgr *)manager;
	if (!mgr)
		return;
	mgr->setCipherKey(modName, key);
}

void gsw_manager_set_javascript (GswManager *manager, gboolean valueBool)
{
	WebMgr *mgr;

	mgr = (WebMgr *)manager;
	if (!mgr)
		return;
	mgr->setJavascript(valueBool);
}

GList* gsw_manager_get_available_locales (GswManager *manager)
{
	GList *list = NULL;
	WebMgr *mgr;

	mgr = (WebMgr *)manager;
	if (!mgr)
		return NULL;

	sword::StringList localeNames = LocaleMgr::getSystemLocaleMgr()->getAvailableLocales();
	for (sword::StringList::iterator it = localeNames.begin(); it != localeNames.end(); ++it) {
		list = g_list_append(list, g_strdup(it->c_str()));
	}

	return list;
}

void gsw_manager_set_default_locale (GswManager *manager, const gchar *name)
{
	// we don't actually need an SWMgr instance for this
	LocaleMgr::getSystemLocaleMgr()->setDefaultLocaleName(name);
}

const gchar*  gsw_manager_translate (GswManager *manager, const gchar *text, const gchar *localeName)
{
	return LocaleMgr::getSystemLocaleMgr()->translate(text, localeName);
}
