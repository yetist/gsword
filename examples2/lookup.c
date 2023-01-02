#include <stdio.h>
#include <stdlib.h>
#include <gsword.h>

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "\nusage: %s <modname> <\"lookup key\">\n"
				"\tExample: lookup KJV \"James 1:19\"\n\n", argv[0]);
		exit(-1);
	}

	GswManager* manager;
	GswModule* module;

	manager = gsw_manager_new();
	module = gsw_manager_get_module_by_name (manager, argv[1]);

	if (module == NULL) {
		fprintf(stderr, "Could not find module [%s].  Available modules:\n", argv[1]);
		GList *list, *l;
	 	list = gsw_manager_get_modinfo_list(manager);
		for (l=list; l != NULL; l=l->next) {
			GswModinfo *info;
			info = l->data;
			fprintf(stderr, "[%s]\t - %s\n", gsw_modinfo_get_name (info), gsw_modinfo_get_description (info));
		}
		gsw_manager_delete (manager);
		exit(-1);
	}

	gsw_module_set_key_text (module, argv[2]);

	// we render before we print keyText so our keyText snaps to the closest entry 
	//const char *renderText = org_crosswire_sword_SWModule_renderText(module);
	//const char *renderText = org_crosswire_sword_SWModule_getRenderHeader(module);
	const gchar* renderText = gsw_module_get_raw_entry (module);
	printf("==Render=Entry============\n");
	printf("%s\n", gsw_module_get_key_text (module));
	printf("%s\n", renderText);
	printf("==========================\n");

	gsw_manager_delete (manager);

	return 0;
}
