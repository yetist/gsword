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

#include <glib.h>

G_BEGIN_DECLS

typedef void GswModule;

typedef struct _GswSearchHit GswSearchHit;
typedef enum
{
	MODULE_SEARCH_TYPE_REGEX     = 1L,
	MODULE_SEARCH_TYPE_PHRASE    = -1L,
	MODULE_SEARCH_TYPE_MULTIWORD = -2L,
	MODULE_SEARCH_TYPE_ENTRYATTR = -3L,
	MODULE_SEARCH_TYPE_LUCENE    = -4L,
}ModuleSearchType;

GswSearchHit* gsw_search_hit_new       (const char *modName, char *key, long  score);
const gchar*   gsw_search_hit_get_name  (GswSearchHit *hit);
gchar*         gsw_search_hit_get_key   (GswSearchHit *hit);
long          gsw_search_hit_get_score (GswSearchHit *hit);
void          gsw_search_hit_free      (GswSearchHit *hit);

GswModule*     gsw_module_new                (void);
void           gsw_module_terminate_search   (GswModule *module);

//const struct gSearchHit *  gsw_module_search (SWHANDLE module, const char *searchString, int searchType, long flags, const char *scope, SWHANDLE progressReporter);
gboolean       gsw_module_pop_error          (GswModule *module);
long           gsw_module_get_entry_size     (GswModule *module);
/*

const char **  gsw_module_getEntryAttribute (SWHANDLE hSWModule, const char *level1, const char *level2, const char *level3, char filteredBool);

const char **  gsw_module_parseKeyList (SWHANDLE hSWModule, const char *keyText);
*/

// Special values handled for VerseKey modules:
//	[+-][book|chapter]	- [de|in]crement by chapter or book
//	(e.g.	"+chapter" will increment the VerseKey 1 chapter)
//	[=][key]		- position absolutely and don't normalize
//	(e.g.	"jn.1.0" for John Chapter 1 intro; "jn.0.0" For Book of John Intro)
void  gsw_module_set_key_text (GswModule *module, const char *key);

const gchar *  gsw_module_get_key_text (GswModule *module);

gboolean gsw_module_has_key_children (GswModule *module);

/*
// This method returns child nodes for a genbook,
// but has special handling if called on a VerseKey module:
//  [0..7] [testament, book, chapter, verse, chapterMax, verseMax, bookName, osisRef]
const char **  gsw_module_getKeyChildren (SWHANDLE hSWModule);
*/
const gchar* gsw_module_get_name        (GswModule *module);
const gchar* gsw_module_get_description (GswModule *module);
const gchar* gsw_module_get_category    (GswModule *module);
const gchar* gsw_module_get_key_parent  (GswModule *module);
void         gsw_module_previous        (GswModule *module);
void         gsw_module_next            (GswModule *module);
void         gsw_module_begin           (GswModule *module);
const gchar* gsw_module_strip_text      (GswModule *module);
const gchar* gsw_module_render_text     (GswModule *module);

const gchar* gsw_module_get_render_header       (GswModule *module);
const gchar* gsw_module_get_raw_entry           (GswModule *module);
void         gsw_module_set_raw_entry           (GswModule *module, const char *entryBuffer);
const gchar* gsw_module_get_config_entry        (GswModule *module, const char *key);
void         gsw_module_delete_search_framework (GswModule *module);
gboolean     gsw_module_has_search_framework    (GswModule *module);

G_END_DECLS

#endif /* __GSW_MODULE_H__ */
