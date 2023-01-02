#include <stdio.h>
#include <stdlib.h>
#include <gsword.h>

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "\nusage: %s <modname>\n"
				"\tExample: %s KJV \n\n", argv[0], argv[0]);
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
		g_object_unref(manager);
		exit(-1);
	}

	GswVerseKey *key;
	key = gsw_module_get_verse_key(module);

	for (; !gsw_verse_key_pop_error (key); gsw_verse_key_set_chapter(key, gsw_verse_key_get_chapter(key) +1)) {
		gsw_verse_key_set_verse(key, gsw_verse_key_get_verse_max (key));
		gchar* text =  gsw_module_strip_text (module);
		gchar* t = g_strstrip(text);
		if (g_str_has_suffix (t, ".") || g_str_has_suffix (t, "。") || g_str_has_suffix (t, "！")) {
			printf("%s:\n\n%s\n", gsw_verse_key_get_text(key), t);
		}
	}
	return 0;
}
