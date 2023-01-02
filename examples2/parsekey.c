#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include "gsword.h"

int main(int argc, char **argv)
{
	g_setenv("LC_ALL", "zh_CN.utf8", TRUE);
	setlocale (LC_ALL, "");
	if (argc != 3) {
		fprintf(stderr, "\nusage: %s <modname> <\"keyText\">\n"
				"\tExample: %s KJV \"James 1:19-30,34\"\n\n", argv[0], argv[0]);
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
			info = (GswModinfo*) l->data;
			fprintf(stderr, "[%s]\t - %s\n", gsw_modinfo_get_name (info), gsw_modinfo_get_description (info));
		}
		gsw_manager_delete (manager);
		exit(-1);
	}

	GList *li, *l;
	li = gsw_module_parse_key_list (module, argv[2]);
	printf("==========================\n");
	printf("Parsing: %s\n", argv[2]);
	printf("==========================\n");
	for (l=li; l != NULL; l = l->next) {
		printf("%s\n", (gchar*) l->data);
	}
	printf("==========================\n");

	gsw_manager_delete (manager);

	return 0;
}
