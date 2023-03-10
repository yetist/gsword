#include <stdio.h>
#include <locale.h>
#include "gsword.h"

void  prefunc (glong total, glong completed, const gchar* message)
{
	g_print("pre: %s, %ld, %ld\n", message, total, completed);
}

void search_percent (GswModule *module, guint per, gpointer user_data)
{
	g_print("Searching %d%%, %s\n", per, (gchar*) user_data);
}

void test_module(GswModule *module)
{
	if (module == NULL)
		return;
	GList *list, *l;
	g_print("module size = %ld\n", gsw_module_get_entry_size(module));
	g_signal_connect(G_OBJECT(module), "searching", G_CALLBACK(search_percent), "searching...");
	list = gsw_module_search (module, "God", GSW_SEARCH_TYPE_REGEX, 0, "Rev");
	for (l = list; l != NULL; l = l->next) {
		GswResult *result;
		result = l->data;
		g_print("name =%s\n", gsw_result_get_name  (result));
		g_print("key =%s\n", gsw_result_get_key (result));
		g_print("score =%ld\n", gsw_result_get_score (result));
	}
	g_print("render header=%s\n", gsw_module_get_render_header (module));
	gsw_module_set_key_text(module, "jn.2.3");
	g_print("render text=%s\n", gsw_module_render_text(module));
	g_print("strip text =%s\n", gsw_module_strip_text (module));
	g_print("raw entry=%s\n",  gsw_module_get_raw_entry (module));
	//const gchar* gsw_module_get_config_entry (GswModule *module, const char *key)
	//gsw_result_unref(result);
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
	//gsw_installer_refresh_remote_source (installer, "CrossWare");
}

void updating   (GswStatusReporter *reporter, gulong total, gulong completed, gpointer data)
{
	g_print("...updating, %ld of %ld (%s)\n", completed, total, (char*) data);
}
void pre_update (GswStatusReporter *reporter, gulong total, gulong completed, const gchar* message, gpointer data)
{
	g_print("...pre update, %ld of %ld (%s) %s\n", completed, total, message, (char*) data);
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
	g_signal_connect(G_OBJECT(reporter), "updating", G_CALLBACK(updating), "hi");
	g_signal_connect(G_OBJECT(reporter), "pre-update", G_CALLBACK(pre_update), "hi");

	// installer
	installer = gsw_installer_new("here", reporter);
	test_installer(installer);

	gsw_installer_delete (installer);
	g_object_unref(manager);
}
