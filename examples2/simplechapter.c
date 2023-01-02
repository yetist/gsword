#include <stdio.h>
#include <stdlib.h>

#include "gsword.h"

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "\nusage: %s <modname> <\"VerseKey\">\n"
				"\tExample: %s KJV \"Jn.3.16\"\n\n", argv[0], argv[0]);
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

	gsw_module_set_key_text (module, argc > 2 ? argv[2] : "Jn.3.16");
	GswVerseKey *key;
	key = gsw_module_get_verse_key(module);

	gint curVerse = gsw_verse_key_get_verse (key);
	gint curChapter = gsw_verse_key_get_chapter (key);
	gint curBook = gsw_verse_key_get_book (key);

	for (gsw_verse_key_set_verse(key, 1);
			gsw_verse_key_get_book(key) == curBook &&
			gsw_verse_key_get_chapter(key) == curChapter &&
			!gsw_module_pop_error (module);
			gsw_module_next (module)) {
		if (gsw_verse_key_get_verse(key) == curVerse) {
			printf("* ");
		}
		printf("%d %s\n", gsw_verse_key_get_verse(key), gsw_module_strip_text(module));
	}
	return 0;
}
