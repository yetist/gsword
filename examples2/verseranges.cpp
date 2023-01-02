#include <stdio.h>
#include <stdlib.h>

#include "gsword.h"

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "\nusage: %s <modname>\n"
				"\tExample: %s KJV\n\n", argv[0], argv[0]);
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

	GswVerseKey *key;
	key = gsw_module_create_key(module);

	// let's set verse ranges for a variety of different contiguous regions

	// set a verse range for the whole Bible
	gsw_verse_key_set_lower_bound(key, key);	// vk initially points to TOP, so we don't need to set position first
	gsw_verse_key_set_position(key, GSW_POSITION_BOTTOM);
	gsw_verse_key_set_upper_bound(key, key);
	g_print("%s\n", gsw_verse_key_get_range_text(key));

	gsw_verse_key_clear_bounds(key);

	// Old Testament
	gsw_verse_key_set_position(key, GSW_POSITION_TOP);
	gsw_verse_key_set_lower_bound(key, key);
	gsw_verse_key_set_testament(key, 2);
	gsw_verse_key_prev(key);
	gsw_verse_key_set_upper_bound(key, key);

	g_print("%s\n", gsw_verse_key_get_range_text(key));
	gsw_verse_key_clear_bounds(key);

	// New Testament
	gsw_verse_key_set_position(key, GSW_POSITION_TOP);
	gsw_verse_key_set_testament(key, 2);
	gsw_verse_key_set_lower_bound(key, key);
	gsw_verse_key_set_position(key, GSW_POSITION_BOTTOM);
	gsw_verse_key_set_upper_bound(key, key);
	g_print("%s\n", gsw_verse_key_get_range_text(key));
	gsw_verse_key_clear_bounds(key);

	// Current Book
	gsw_verse_key_set_text(key, "John 3:16");
	gsw_verse_key_set_chapter(key, 1);
	gsw_verse_key_set_verse(key, 1);
	gsw_verse_key_set_lower_bound(key, key);
	gsw_verse_key_set_chapter(key, gsw_verse_key_get_chapter_max(key));
	gsw_verse_key_set_verse(key, gsw_verse_key_get_verse_max(key));
	gsw_verse_key_set_upper_bound(key, key);
	g_print("%s\n", gsw_verse_key_get_range_text(key));

	g_print(">>>%s\n", gsw_verse_key_get_versification_system (key));
#if 0
	// -------------------------
	// Shorter syntax using the parser and based on book names, and requires intimate knowledge of VersificationMgr
	// You're probably better off using the above code, but this is here for completeness
	//
	const VersificationMgr::System *refSys = VersificationMgr::getSystemVersificationMgr()->getVersificationSystem(vk->getVersificationSystem());


	// whole Bible
	VerseKey vkBible(refSys->getBook(0)->getOSISName(), refSys->getBook(refSys->getBookCount()-1)->getOSISName(), refSys->getName());
	cout << vkBible.getRangeText() << "\n";

	// OT
	VerseKey vkOT(refSys->getBook(0)->getOSISName(), refSys->getBook(refSys->getBMAX()[0]-1)->getOSISName(), refSys->getName());
	cout << vkOT.getRangeText() << "\n";

	// NT
	VerseKey vkNT(refSys->getBook(refSys->getBMAX()[0])->getOSISName(), refSys->getBook(refSys->getBookCount()-1)->getOSISName(), refSys->getName());
	cout << vkNT.getRangeText() << "\n";

	// Current Book
	vk->setText("John 3:16");
	VerseKey vkCurrentBook(vk->getBookName(), vk->getBookName(), refSys->getName());
	cout << vkCurrentBook.getRangeText() << "\n";


	delete vk;

	cout << endl;
#endif
	return 0;
}
