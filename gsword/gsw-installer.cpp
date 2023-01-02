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
#include "gsw-private.h"

typedef struct _GswInstaller             GswInstaller;

struct _GswInstaller
{
  GObject            object;
  gchar             *path;
  sword::InstallMgr *mgr;
};

enum {
    PROP_0,
	PROP_PATH,
	NUM_PROPERTIES
};

static GParamSpec *installer_props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GswInstaller, gsw_installer, G_TYPE_OBJECT);

static void gsw_installer_set_path(GswInstaller *installer, const gchar* path)
{
	installer->path = g_strdup(path);
}

static void gsw_installer_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    GswInstaller *installer;
    installer = GSW_INSTALLER (object);

    switch (prop_id)
	{
		case PROP_PATH:
			gsw_installer_set_path(installer, g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void gsw_installer_dispose (GObject *object)
{
	GswInstaller* installer = GSW_INSTALLER(object);
	if (installer->mgr != NULL) {
		delete installer->mgr;
		installer->mgr = NULL;
	}
	if (installer->path != NULL) {
		g_free(installer->path);
		installer->path = NULL;
	}
	G_OBJECT_CLASS (gsw_installer_parent_class)->dispose (object);
}

static void gsw_installer_class_init (GswInstallerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->dispose = gsw_installer_dispose;
	gobject_class->set_property = gsw_installer_set_property;
	installer_props[PROP_PATH] =
		g_param_spec_string ("path",
				"Install path",
				"Path to install module",
				"",
				G_PARAM_WRITABLE);
	g_object_class_install_properties (gobject_class, NUM_PROPERTIES, installer_props);
}

static void gsw_installer_init (GswInstaller *installer)
{
	g_return_if_fail(GSW_IS_INSTALLER(installer));
	if (installer->path != NULL) {
		g_free(installer->path);
	}
	installer->path = g_build_filename(g_get_home_dir(), ".sword", "InstallMgr", NULL);
}

static void gsw_installer_init_config(GswInstaller *installer)
{
	g_return_if_fail(GSW_IS_INSTALLER(installer));
	gchar *conf_path;
	conf_path = g_build_filename(installer->path, "InstallMgr.conf", NULL);
	if (!g_file_test(installer->path, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (installer->path, 0755);
		sword::SWConfig config(conf_path);
		config["General"]["PassiveFTP"] = "true";

		sword::InstallSource is("FTP");
		is.caption = "CrossWire";
		is.source = "ftp.crosswire.org";
		is.directory = "/pub/sword/raw";

		config["General"]["PassiveFTP"] = "true";
		config["Sources"]["FTPSource"] = is.getConfEnt();
		config.Save();

		sword::InstallSource is_local("DIR");
		is_local.caption = "cdrom";
		is_local.source = "[local]";
		is_local.directory = "/mnt/cdrom";
		config["Sources"]["DIRSource"] = is_local.getConfEnt();
		config.Save();
	}
	g_free(conf_path);
}

GswInstaller* gsw_installer_new (const gchar *path, GswStatusReporter *statusReporter)
{
	GswInstaller *installer;
	sword::StatusReporter *reporter;
    installer = (GswInstaller*) g_object_new (GSW_TYPE_INSTALLER, "path", path, NULL);
	gsw_installer_init_config(installer);
	reporter = (sword::StatusReporter*) gsw_status_reporter_get_internal(statusReporter);
	installer->mgr = new sword::InstallMgr(path, reporter);

	return installer;
}

void gsw_installer_delete (GswInstaller* installer)
{
	g_return_if_fail(GSW_IS_INSTALLER(installer));
	g_object_unref(installer);
}

void gsw_installer_set_user_disclaimer_confirmed  (GswInstaller* installer)
{
	g_return_if_fail(GSW_IS_INSTALLER(installer));
	installer->mgr->setUserDisclaimerConfirmed(TRUE);
}

int gsw_installer_sync_config (GswInstaller* installer)
{
	g_return_val_if_fail(GSW_IS_INSTALLER(installer), -1);
	return installer->mgr->refreshRemoteSourceConfiguration();
}

int gsw_installer_uninstall_module (GswInstaller* installer, GswManager* manager, const char *modName)
{
	g_return_val_if_fail(GSW_IS_INSTALLER(installer), -1);
	sword::SWMgr *mgr;
	mgr = (sword::SWMgr*)manager;
	if (!mgr)
		return -1;

	sword::SWModule *module;
	sword::ModMap::iterator it = mgr->Modules.find(modName);
	if (it == mgr->Modules.end()) {
		return -2;
	}
	module = it->second;
	return installer->mgr->removeModule(mgr, module->getName());
}

GList* gsw_installer_get_remote_sources (GswInstaller* installer)
{
	GList *list = NULL;

	sword::StringList vals = sword::LocaleMgr::getSystemLocaleMgr()->getAvailableLocales();

	for (sword::InstallSourceMap::iterator it = installer->mgr->sources.begin(); it != installer->mgr->sources.end(); ++it) {
		list = g_list_append(list, (gpointer) it->second->caption.c_str());
	}

	return list;
}

int  gsw_installer_refresh_remote_source (GswInstaller* installer, const char *sourceName)
{
	g_return_val_if_fail(GSW_IS_INSTALLER(installer), -1);
	sword::InstallSourceMap::iterator source = installer->mgr->sources.find(sourceName);
	if (source == installer->mgr->sources.end()) {
		return -3;
	}

	return installer->mgr->refreshRemoteSource(source->second);
}

GList* gsw_installer_get_remote_modinfo_list (GswInstaller* installer, GswManager* manager, const char *sourceName)
{
	g_return_val_if_fail(GSW_IS_INSTALLER(installer), NULL);
	sword::SWMgr *mgr;
	mgr = (sword::SWMgr*)manager;
	if (!mgr)
		return NULL;

	GList *list = NULL;

	sword::InstallSourceMap::iterator source = installer->mgr->sources.find(sourceName);

	std::map<sword::SWModule *, int> modStats = installer->mgr->getModuleStatus(*mgr, *source->second->getMgr());

	for (std::map<sword::SWModule *, int>::iterator it = modStats.begin(); it != modStats.end(); ++it) {
		sword::SWModule *module = it->first;
		int status = it->second;

		sword::SWBuf version = module->getConfigEntry("Version");
		sword::SWBuf statusString = " ";
		if (status & sword::InstallMgr::MODSTAT_NEW) statusString = "*";
		if (status & sword::InstallMgr::MODSTAT_OLDER) statusString = "-";
		if (status & sword::InstallMgr::MODSTAT_UPDATED) statusString = "+";

		sword::SWBuf type = module->getType();
		sword::SWBuf cat = module->getConfigEntry("Category");
		if (cat.length() > 0) type = cat;

		GswModinfo* modinfo;
		modinfo = gsw_modinfo_new (
				g_strdup(sword::assureValidUTF8(module->getName())),
				g_strdup(sword::assureValidUTF8(module->getDescription())),
				g_strdup(sword::assureValidUTF8(type.c_str())),
				g_strdup(sword::assureValidUTF8(module->getLanguage())),
				g_strdup(sword::assureValidUTF8(version.c_str())),
				g_strdup(sword::assureValidUTF8(statusString.c_str()))
				);
		list = g_list_append(list, modinfo);

	}
	return list;
}

int gsw_installer_remote_install_module (GswInstaller* installer, GswManager* manager, const char *sourceName, const char *modName)
{
	g_return_val_if_fail(GSW_IS_INSTALLER(installer), -1);
	sword::SWMgr *mgr;
	mgr = (sword::SWMgr*)manager;
	if (!mgr)
		return -1;

	sword::InstallSourceMap::iterator source = installer->mgr->sources.find(sourceName);

	if (source == installer->mgr->sources.end()) {
		return -3;
	}

	sword::InstallSource *is = source->second;
	sword::SWMgr *rmgr = is->getMgr();
	sword::SWModule *module;

	sword::ModMap::iterator it = rmgr->Modules.find(modName);

	if (it == rmgr->Modules.end()) {
		return -4;
	}

	module = it->second;

	int error = installer->mgr->installModule(mgr, 0, module->getName(), is);

	return error;
}

GswModule* gsw_installer_get_remote_module_by_name (GswInstaller* installer, const char *sourceName, const char *modName)
{
	g_return_val_if_fail(GSW_IS_INSTALLER(installer), NULL);
	sword::InstallSourceMap::iterator source = installer->mgr->sources.find(sourceName);

	if (source == installer->mgr->sources.end()) {
		return NULL;
	}

	sword::SWMgr *mgr = source->second->getMgr();

	sword::SWModule *module = mgr->getModule(modName);

	if (!module) {
		return NULL;
	}

	return  gsw_module_new(module);
}

void gsw_installer_terminate (GswInstaller* installer)
{
	g_return_if_fail(GSW_IS_INSTALLER(installer));
	installer->mgr->terminate();
}


void gsw_installer_reset_config (GswInstaller *installer, const gchar *baseDir)
{
	g_return_if_fail(GSW_IS_INSTALLER(installer));
	gchar *conf_path;
	conf_path = g_build_filename(baseDir, "InstallMgr.conf", NULL);
	if (g_file_test(conf_path, G_FILE_TEST_EXISTS)) {
		g_remove(conf_path);
	}
	sword::SWConfig config(conf_path);
	config["General"]["PassiveFTP"] = "true";
	config.Save();
	gsw_installer_reload_config(installer);
}

void gsw_installer_reload_config(GswInstaller *installer)
{
	g_return_if_fail(GSW_IS_INSTALLER(installer));
	installer->mgr->readInstallConf();
}
