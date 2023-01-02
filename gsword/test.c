/* vi: set sw=4 ts=4 wrap ai: */
/*
 * test.c: This file is part of gsword.
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
#include <stdio.h>
#include <locale.h>
#include "gsw-manager.h"
#include "gsw-modinfo.h"
#include "gsw-search-hit.h"
#include "gsw-status-reporter.h"
#include "gsw-installer.h"

void percent(gchar i, gpointer d)
{
	g_print("%d, %s\n", i, (gchar*) d);
}

void  upfunc (gulong total, gulong completed)
{
	g_print("up: %ld, %ld\n", total, completed);
}

void  prefunc (glong total, glong completed, const gchar* message)
{
	g_print("pre: %s, %ld, %ld\n", message, total, completed);
}

void test_module(GswModule *module)
{
	GList *list, *l;
	g_print("module size = %ld\n", gsw_module_get_entry_size(module));
	gsw_module_set_percent_callback(module, percent, "searching...");
	list = gsw_module_search (module, "God", GSW_SEARCH_TYPE_REGEX, 0, "Rev");
	for (l = list; l != NULL; l = l->next) {
		GswSearchHit *search_hit;
		search_hit = l->data;
		g_print("name =%s\n", gsw_search_hit_get_name  (search_hit));
		g_print("key =%s\n", gsw_search_hit_get_key (search_hit));
		g_print("score =%ld\n", gsw_search_hit_get_score (search_hit));
	}
	g_print("render header=%s\n", gsw_module_get_render_header (module));
	gsw_module_set_key_text(module, "jn.2.3");
	g_print("render text=%s\n", gsw_module_render_text(module));
	g_print("strip text =%s\n", gsw_module_strip_text (module));
	g_print("raw entry=%s\n",  gsw_module_get_raw_entry (module));
	//const gchar* gsw_module_get_config_entry (GswModule *module, const char *key)
	//gsw_search_hit_unref(search_hit);
}

void test_modinfo(GList *list)
{
	GList *l;
	for (l = list; l != NULL; l = l->next)
	{
		GswModinfo *info = (GswModinfo*)l->data;
		g_print("modinfo:name=%s\n", gsw_modinfo_get_name(info));
		g_print("       :desc=%s\n", gsw_modinfo_get_description(info));
		g_print("       :cate=%s\n", gsw_modinfo_get_category(info));
		g_print("       :lang=%s\n", gsw_modinfo_get_language(info));
		g_print("       :vern=%s\n", gsw_modinfo_get_version(info));
		g_print("       :delt=%s\n", gsw_modinfo_get_delta(info));
	}
}

void test_manager(GswManager *manager)
{
	GList *list, *l;
	g_print("manager version=%s\n", gsw_manager_get_version(manager));

	list = gsw_manager_get_modinfo_list(manager);
	test_modinfo(list);
	GList *list2, *l2;

	gsw_manager_set_default_locale (manager, "zh_Hans"); // "zh_CN");
	list = gsw_manager_get_global_options (manager);
	g_print("==== global option: ====\n");
	for (l = list; l != NULL; l = l->next)
	{
		gchar* option = l->data;
		g_print("[%s] = ", option);
		list2 = gsw_manager_get_global_option_values (manager, (gchar*) l->data);
		for (l2 = list2; l2 != NULL; l2 = l2->next)
		{
			g_print("[%s]", (gchar*) l2->data);
		}
		g_print("\t# %s", gsw_manager_get_global_option_tip (manager, option));
		g_print("\n");
	}

	list = gsw_manager_get_available_locales (manager);
	g_print("==== Manager Locales: ====\n");
	for (l = list; l != NULL; l = l->next)
	{
		g_print("%s\n", (gchar*) l->data);
	}
	g_print("\n");

}

void test_installer(GswInstaller *installer)
{
	gsw_installer_sync_config (installer);
	//	gsw_installer_refresh_remote_source (installer, const char *sourceName);
}

int main(int argc, char** argv)
{
	g_setenv("LC_ALL", "zh_CN.utf8", TRUE);
	setlocale (LC_ALL, "");

	GswManager* manager;
	GswModule *module;
	GswStatusReporter *reporter;
	GswInstaller *installer;

	// manager
	manager = gsw_manager_new ();
	test_manager(manager);

	// module
	module = gsw_manager_get_module_by_name(manager, "ChiUns");
	test_module(module);

	// status reporter
	reporter = gsw_status_reporter_new ();
	gsw_status_reporter_set_update_callback (reporter, upfunc);
	gsw_status_reporter_set_prestatus_callback (reporter, prefunc);

	// installer
	installer = gsw_installer_new("here", reporter);
	test_installer(installer);

	gsw_installer_delete (installer);
	gsw_manager_delete(manager);
}
