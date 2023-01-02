#include <stdio.h>
#include <stdlib.h>
#include <gsword.h>

void  upfunc (gulong total, gulong completed)
{
	g_print("up: %ld, %ld\n", total, completed);
}

void  prefunc (glong total, glong completed, const gchar* message)
{
	g_print("pre: %s, %ld, %ld\n", message, total, completed);
}

void test_modinfo(GList *list)
{
	GList *l;
	for (l = list; l != NULL; l = l->next)
	{
		GswModinfo *info = (GswModinfo*)l->data;
		g_print("modinfo:name=%s\n", gsw_modinfo_get_name(info));
		g_print("        desc=%s\n", gsw_modinfo_get_description(info));
		g_print("        cate=%s\n", gsw_modinfo_get_category(info));
		g_print("        lang=%s\n", gsw_modinfo_get_language(info));
		g_print("        vern=%s\n", gsw_modinfo_get_version(info));
		g_print("        delt=%s\n", gsw_modinfo_get_delta(info));
	}
}

int main(int argc, char **argv)
{
	if (argc != 4) {
		fprintf(stderr, "\nusage: %s \"<sourceName>\" <\"modName\"> \"<destPath>\"\n"
				"\tExample: CrossWire KJV ~/library\n\n", argv[0]);
		exit(-1);
	}

	GswManager* manager;
	GswInstaller* installer;
	GswStatusReporter *reporter;

	// status reporter
	reporter = gsw_status_reporter_new ();
	gsw_status_reporter_set_update_callback (reporter, upfunc);
	gsw_status_reporter_set_prestatus_callback (reporter, prefunc);

	manager = gsw_manager_new_with_path(argv[3]);
	gchar* path = g_build_filename(g_get_home_dir(), ".sword", "InstallMgr", NULL);
	installer = gsw_installer_new (path, reporter);
	gsw_installer_set_user_disclaimer_confirmed (installer);
	gsw_installer_sync_config (installer);

	gsw_installer_refresh_remote_source (installer, "CrossWire");

        GList *list = gsw_installer_get_remote_modinfo_list (installer, manager, "CrossWire");
	test_modinfo(list);

	fprintf(stdout, "Install returned: %d\n", gsw_installer_remote_install_module (installer, manager, argv[1], argv[2]));
	gsw_manager_delete (manager);
	gsw_installer_delete (installer);

	return 0;
}
