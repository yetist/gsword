#include <stdio.h>
#include <stdlib.h>

#include <gsword.h>

void search_percent (GswModule *module, guint per, gpointer user_data)
{
	g_print("Searching %d%%, %s\n", per, (gchar*) user_data);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "\nusage: %s <modname> <\"search terms\">\n"
				"\tExample: %s KJV \"God love world\"\n\n", argv[0], argv[0]);
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
		g_object_unref (manager);
		exit(-1);
	}

	gsw_module_set_key_text (module, argv[2]);

	g_signal_connect(G_OBJECT(module), "searching", G_CALLBACK(search_percent), "searching...");
	GList *list, *l;
	list = gsw_module_search (module, argv[2], GSW_SEARCH_TYPE_MULTIWORD, 0, NULL);
	printf("==========================\n");
	printf("Parsing: %s\n", argv[2]);
	printf("==========================\n");
	for (l = list; l !=NULL; l=l->next) {
		GswSearchHit *hit;
		hit = l->data;
		printf("%s, %s\n", gsw_search_hit_get_name(hit), gsw_search_hit_get_key(hit));
	}
	printf("==========================\n");

	g_object_unref (manager);

	return 0;
}
