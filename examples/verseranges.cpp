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
		g_object_unref (manager);
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

	// whole Bible
	GswVersiMgr *mm = gsw_versi_mgr_new();

	GswVersiMgrSystem *refSys = gsw_versi_mgr_get_system(mm, gsw_verse_key_get_versification_system (key));

	GswVersiMgrBook *book =  gsw_versi_mgr_system_get_book(refSys, 0);
	GswVersiMgrBook *book2 =  gsw_versi_mgr_system_get_book(refSys, gsw_versi_mgr_system_get_book_count(refSys) -1);

	GswVerseKey* vkey = gsw_verse_key_new_with_boundary(gsw_versi_mgr_book_get_osis_name(book),
			gsw_versi_mgr_book_get_osis_name(book2), gsw_versi_mgr_system_get_name (refSys));
	g_print("Whole bible: %s\n", gsw_verse_key_get_range_text(vkey));

	// OT
	GArray* array = gsw_versi_mgr_system_get_bmax (refSys);
	GswVersiMgrBook *book3 =  gsw_versi_mgr_system_get_book(refSys, g_array_index (array, gint, 0) -1);
	GswVerseKey* vkey1 = gsw_verse_key_new_with_boundary(gsw_versi_mgr_book_get_osis_name(book),
			gsw_versi_mgr_book_get_osis_name(book3), gsw_versi_mgr_system_get_name (refSys));
	g_print("OT: %s\n", gsw_verse_key_get_range_text(vkey1));

	// NT
	GswVersiMgrBook *book4 =  gsw_versi_mgr_system_get_book(refSys, g_array_index (array, gint, 0));
	GswVerseKey* vkey2 = gsw_verse_key_new_with_boundary(gsw_versi_mgr_book_get_osis_name(book4),
			gsw_versi_mgr_book_get_osis_name(book2), gsw_versi_mgr_system_get_name (refSys));
	g_print("NT: %s\n", gsw_verse_key_get_range_text(vkey2));


	gsw_verse_key_set_text(vkey2, "John 3:16");

	GswVerseKey* vkey4 = gsw_verse_key_new_with_boundary(
	gsw_verse_key_get_book_name (vkey2),
	gsw_verse_key_get_book_name (vkey2),
			gsw_versi_mgr_system_get_name (refSys));
	g_print("Current book: %s\n", gsw_verse_key_get_range_text(vkey4));

	return 0;
}
