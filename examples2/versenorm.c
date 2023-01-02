#include <stdio.h>
#include <stdlib.h>
#include "gsword.h"

int main(int argc, char **argv)
{
        const char *modName = "KJV";
        const char *keyTextRegular = "1Sam1.20";
        const char *keyTextNeedsNormalization = "1Sam1.200";

	GswManager* manager;
	GswModule* module;

	manager = gsw_manager_new();
	module = gsw_manager_get_module_by_name (manager, modName);

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

	// get two VerseKey objects for a module
	GswVerseKey *verse = gsw_module_create_key(module);
	GswVerseKey *verseNormalized = gsw_module_create_key(module);

	// turn off autonormalization for one VerseKey reference
	gsw_verse_key_set_auto_normalize(verse, FALSE);

	// set a reference not requiring normalization to both VerseKey objects
	gsw_verse_key_set_text(verse, keyTextRegular);
	gsw_verse_key_set_text(verseNormalized, keyTextRegular);

	// check to see if they are equal
	if (gsw_verse_key_get_text(verse) == gsw_verse_key_get_text(verseNormalized)) {
		printf("%s == %s\n", gsw_verse_key_get_text(verse), gsw_verse_key_get_text(verseNormalized));
	} else {
		printf("%s != %s\n", gsw_verse_key_get_text(verse), gsw_verse_key_get_text(verseNormalized));
	}
	gsw_verse_key_set_text(verse, keyTextNeedsNormalization);
	gsw_verse_key_set_text(verseNormalized, keyTextNeedsNormalization);

	// now do the same for a reference which requires normalization in this module's v11n system
	if (gsw_verse_key_get_text(verse) == gsw_verse_key_get_text(verseNormalized)) {
		printf("%s == %s\n", gsw_verse_key_get_text(verse), gsw_verse_key_get_text(verseNormalized));
	} else {
		printf("%s != %s\n", gsw_verse_key_get_text(verse), gsw_verse_key_get_text(verseNormalized));
	}

        return 0;
}
