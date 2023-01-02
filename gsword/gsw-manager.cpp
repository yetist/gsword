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
#include "webmgr.hpp"

#include "gsw-manager.h"
#include "gsw-modinfo.h"
#include "gsw-private.h"

typedef struct _GswManager   GswManager;

struct _GswManager
{
  GObject       object;
  gchar*        path;
  WebMgr* mgr;
};

enum {
    PROP_0,
	PROP_PATH,
	NUM_PROPERTIES
};

static GParamSpec *manager_props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GswManager, gsw_manager, G_TYPE_OBJECT);

static void gsw_manager_set_path(GswManager *manager, const gchar* path)
{
	manager->path = g_strdup(path);
}

static void gsw_manager_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    GswManager *manager;
    manager = GSW_MANAGER (object);

    switch (prop_id)
	{
		case PROP_PATH:
			gsw_manager_set_path(manager, g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void gsw_manager_dispose (GObject *object)
{
	GswManager* manager = GSW_MANAGER(object);
	if (manager->mgr != NULL) {
		delete manager->mgr;
		manager->mgr = NULL;
	}
	if (manager->path != NULL) {
		g_free(manager->path);
		manager->path = NULL;
	}
//	if (manager->osisWordJS != NULL) {
//		delete manager->osisWordJS;
//		manager->osisWordJS = NULL;
//	}
//	if (manager->thmlWordJS != NULL) {
//		delete manager->thmlWordJS;
//		manager->thmlWordJS = NULL;
//	}
//	if (manager->gbfWordJS != NULL) {
//		delete manager->gbfWordJS;
//		manager->gbfWordJS = NULL;
//	}
	G_OBJECT_CLASS (gsw_manager_parent_class)->dispose (object);
}

static void gsw_manager_class_init (GswManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->dispose = gsw_manager_dispose;
	gobject_class->set_property = gsw_manager_set_property;
	manager_props[PROP_PATH] =
		g_param_spec_string ("path",
				"Install path",
				"Path to install module",
				"",
				G_PARAM_WRITABLE);
	g_object_class_install_properties (gobject_class, NUM_PROPERTIES, manager_props);
}

static void gsw_manager_init (GswManager *manager)
{
	g_return_if_fail(GSW_IS_MANAGER(manager));
	if (manager->path != NULL) {
		g_free(manager->path);
	}
//	manager->osisWordJS = NULL;
//	manager->thmlWordJS = NULL;
//	manager->gbfWordJS = NULL;
	manager->path = g_build_filename(g_get_home_dir(), ".sword", NULL);
}

static void gsw_manager_init_config (GswManager *manager)
{
	g_return_if_fail(GSW_IS_MANAGER(manager));
	gchar* modsd = g_build_filename(manager->path, "mods.d", NULL);
	gchar* confpath = g_build_filename(modsd, "globals.conf", NULL);

	// be sure we have at least some config file already out there
	if (!g_file_test(modsd, G_FILE_TEST_IS_DIR)) {
		g_mkdir_with_parents (modsd, 0755);
		sword::SWConfig config(confpath);
		config["Globals"]["HiAndroid"] = "weeee";
		config.Save();
	}
	g_free(modsd);
	g_free(confpath);
}

static void gsw_manager_initialize (GswManager *manager)
{
	g_return_if_fail(GSW_IS_MANAGER(manager));
	//gsw_manager_init_config(manager);

//	sword::SWModule *defaultGreekLex = NULL;
//	sword::SWModule *defaultHebLex = NULL;
//	sword::SWModule *defaultGreekParse = NULL;
//	sword::SWModule *defaultHebParse = NULL;

//	manager->osisWordJS = new sword::OSISWordJS();
//	manager->thmlWordJS = new sword::ThMLWordJS();
//	manager->gbfWordJS = new  sword::GBFWordJS();
//	manager->mgr->Load();
//	manager->osisWordJS->setDefaultModules(defaultGreekLex, defaultHebLex, defaultGreekParse, defaultHebParse);
//	manager->thmlWordJS->setDefaultModules(defaultGreekLex, defaultHebLex, defaultGreekParse, defaultHebParse);
//	manager->gbfWordJS->setDefaultModules(defaultGreekLex, defaultHebLex, defaultGreekParse, defaultHebParse);
//
//	manager->osisWordJS->setMgr(manager->mgr);
//	manager->thmlWordJS->setMgr(manager->mgr);
//	manager->gbfWordJS->setMgr(manager->mgr);
//
//	manager->mgr->setGlobalOption("Textual Variants", "Primary Reading");
}

GswManager* gsw_manager_new (void)
{
	GswManager *manager;
	sword::SWConfig *sysConf = NULL;
    manager = (GswManager*) g_object_new (GSW_TYPE_MANAGER, NULL);
	manager->mgr = new WebMgr(sysConf);
	//gsw_manager_initialize(manager);
	return manager;
}

GswManager* gsw_manager_new_with_path (const gchar *path)
{
	g_print("call here, path=%s\n", path);
	GswManager *manager;
	sword::SWBuf confPath = path;
    manager = (GswManager*) g_object_new (GSW_TYPE_MANAGER, "path", path, NULL);
	gsw_manager_init_config(manager);
	manager->mgr = new WebMgr(confPath.c_str());
	//gsw_manager_initialize(manager);
	g_print("prefix:%s, config:%s\n", manager->mgr->prefixPath, manager->mgr->configPath);
	return manager;
}

#if 0
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

void gsw_manager_delete (GswManager *manager)
{
	WebMgr *mgr;
	mgr = (WebMgr *)manager;
	if (mgr)
		delete mgr;
}
#endif

const gchar* gsw_manager_get_version (GswManager *manager)
{
	// we don't actually need an SWMgr to get version
	static sword::SWVersion v;
	return v.currentVersion;
}

GList* gsw_manager_get_modules (GswManager *manager)
{
	GList *modlist = NULL;

	for (sword::ModMap::iterator it = manager->mgr->Modules.begin(); it != manager->mgr->Modules.end(); ++it) {
		sword::SWModule  *mod  = it->second;
		GswModule *module = gsw_module_new(mod);
		modlist = g_list_append(modlist, module);
	}
	return modlist;
}

GList* gsw_manager_get_modinfo_list (GswManager *manager)
{
	sword::SWModule *module = NULL;
	GList *milist = NULL;

	for (sword::ModMap::iterator it = manager->mgr->Modules.begin(); it != manager->mgr->Modules.end(); ++it) {
		module = it->second;
		if ((!(module->getConfigEntry("CipherKey"))) || (*(module->getConfigEntry("CipherKey")))) {
			GswModinfo*  modinfo;
			sword::SWBuf type = module->getType();
			sword::SWBuf cat = module->getConfigEntry("Category");
			if (cat.length() > 0) type = cat;
			sword::SWBuf version = module->getConfigEntry("Version");

			modinfo = gsw_modinfo_new (
					sword::assureValidUTF8(module->getName()),
					sword::assureValidUTF8(module->getDescription()),
					sword::assureValidUTF8(type.c_str()),
					sword::assureValidUTF8(module->getLanguage()),
					sword::assureValidUTF8(version.c_str()),
					"");
			milist = g_list_append(milist, modinfo);
		}
	}
	return milist;
}

GswModule* gsw_manager_get_module_by_name (GswManager *manager, const gchar *moduleName)
{
	sword::SWModule *mod;
	GswModule *module;

	mod = manager->mgr->getModule(moduleName);
	if (mod == NULL) {
		return NULL;
	} else {
		module = gsw_module_new(mod);
		return module;
	}
}

const gchar* gsw_manager_get_prefix_path (GswManager *manager)
{
	return manager->mgr->prefixPath;
}

const gchar*  gsw_manager_get_config_path (GswManager *manager)
{
	return manager->mgr->configPath;
}

void  gsw_manager_set_global_option (GswManager *manager, const gchar *option, const gchar *value)
{
	manager->mgr->setGlobalOption(option, value);
}

const gchar*  gsw_manager_get_global_option (GswManager *manager, const gchar *option)
{
	return manager->mgr->getGlobalOption(option);
}

const gchar*  gsw_manager_get_global_option_tip (GswManager *manager, const gchar *option)
{
	return manager->mgr->getGlobalOptionTip(option);
}

const gchar*  gsw_manager_filter_text (GswManager *manager, const gchar *filterName, const gchar *text)
{
	sword::SWBuf filterBuf = text;

	// why was this in bindings/corba/omniorb?
	//	mgr->setGlobalOption("Greek Accents", "Off");
	gboolean errStatus = manager->mgr->filterText(filterName, filterBuf);
	(void)errStatus;
	return filterBuf.c_str();
}

GList* gsw_manager_get_global_options (GswManager *manager)
{
	GList *list = NULL;

	sword::StringList options = manager->mgr->getGlobalOptions();
	for (sword::StringList::iterator it = options.begin(); it != options.end(); ++it) {
		list = g_list_append(list, g_strdup(it->c_str()));
	}
	return list;
}

GList* gsw_manager_get_global_option_values (GswManager *manager, const gchar *option)
{
	GList *list = NULL;

	sword::StringList options = manager->mgr->getGlobalOptionValues(option);
	for (sword::StringList::iterator it = options.begin(); it != options.end(); ++it) {
		list = g_list_append(list, g_strdup(it->c_str()));
	}
	return list;
}

void gsw_manager_set_cipherkey (GswManager *manager, const gchar *modName, const gchar *key)
{
	manager->mgr->setCipherKey(modName, key);
}

void gsw_manager_set_javascript (GswManager *manager, gboolean value)
{
	manager->mgr->setJavascript(value);
	//osisWordJS->setOptionValue((value)?"On":"Off");
	//manager->thmlWordJS->setOptionValue((value)?"On":"Off");
	//manager->gbfWordJS->setOptionValue((value)?"On":"Off");
}

GList* gsw_manager_get_available_locales (GswManager *manager)
{
	GList *list = NULL;

	sword::StringList localeNames = sword::LocaleMgr::getSystemLocaleMgr()->getAvailableLocales();
	for (sword::StringList::iterator it = localeNames.begin(); it != localeNames.end(); ++it) {
		list = g_list_append(list, g_strdup(it->c_str()));
	}

	return list;
}

void gsw_manager_set_default_locale (GswManager *manager, const gchar *name)
{
	// we don't actually need an SWMgr instance for this
	sword::LocaleMgr::getSystemLocaleMgr()->setDefaultLocaleName(name);
}

const gchar*  gsw_manager_translate (GswManager *manager, const gchar *text, const gchar *localeName)
{
	return sword::LocaleMgr::getSystemLocaleMgr()->translate(text, localeName);
}

gpointer      gsw_manager_get_internal (GswManager *manager)
{
	return manager->mgr;
}
