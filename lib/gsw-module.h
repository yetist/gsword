/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-module.h: This file is part of ____
 *
 * Copyright (C) 2017 yetist <yetist@yetipc>
 *
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * */

#ifndef __GSW_MODULE_H__ 
#define __GSW_MODULE_H__  1

#include <glib-object.h>

G_BEGIN_DECLS


#define GSW_TYPE_MODULE              (gsw_module_get_type ())
G_DECLARE_FINAL_TYPE (GswModule, gsw_module, GSW, MODULE, GObject);

typedef struct _gsw_search_hit gst_search_hit;

struct _gsw_search_hit {
	const char *modName;
	char *key;
	long  score;
};

GswModule*     gsw_module_new                (void);
/*
 
void  gsw_module_terminate_search (SWHANDLE hSWModule);

const struct gSearchHit *  gsw_module_search (SWHANDLE hSWModule, const char *searchString, int searchType, long flags, const char *scope, SWHANDLE progressReporter);
char  gsw_module_pop_error (SWHANDLE hSWModule);
long  gsw_module_get_entry_size (SWHANDLE hSWModule);

const char **  gsw_module_getEntryAttribute (SWHANDLE hSWModule, const char *level1, const char *level2, const char *level3, char filteredBool);

const char **  gsw_module_parseKeyList (SWHANDLE hSWModule, const char *keyText);

// Special values handled for VerseKey modules:
//	[+-][book|chapter]	- [de|in]crement by chapter or book
//	(e.g.	"+chapter" will increment the VerseKey 1 chapter)
//	[=][key]		- position absolutely and don't normalize
//	(e.g.	"jn.1.0" for John Chapter 1 intro; "jn.0.0" For Book of John Intro)
void  gsw_module_setKeyText (SWHANDLE hSWModule, const char *key);

const char *  gsw_module_getKeyText (SWHANDLE hSWModule);

char  gsw_module_hasKeyChildren (SWHANDLE hSWModule);

// This method returns child nodes for a genbook,
// but has special handling if called on a VerseKey module:
//  [0..7] [testament, book, chapter, verse, chapterMax, verseMax, bookName, osisRef]
const char **  gsw_module_getKeyChildren (SWHANDLE hSWModule);

const char *  gsw_module_getName (SWHANDLE hSWModule);

const char *  gsw_module_getDescription (SWHANDLE hSWModule);

const char *  gsw_module_getCategory (SWHANDLE hSWModule);

const char *  gsw_module_getKeyParent (SWHANDLE hSWModule);

void  gsw_module_previous (SWHANDLE hSWModule);

void  gsw_module_next (SWHANDLE hSWModule);

void  gsw_module_begin (SWHANDLE hSWModule);

const char *  gsw_module_stripText (SWHANDLE hSWModule);

const char *  gsw_module_renderText (SWHANDLE hSWModule);

// CSS styles associated with this text
const char *  gsw_module_getRenderHeader (SWHANDLE hSWModule);

const char *  gsw_module_getRawEntry (SWHANDLE hSWModule);

void  gsw_module_setRawEntry (SWHANDLE hSWModule, const char *entryBuffer);

const char *  gsw_module_getConfigEntry (SWHANDLE hSWModule, const char *key);
void  gsw_module_deleteSearchFramework (SWHANDLE hSWModule);

char  gsw_module_hasSearchFramework (SWHANDLE hSWModule);
 */

G_END_DECLS

#endif /* __GSW_MODULE_H__ */
