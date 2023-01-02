/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-module.h: This file is part of gsword.
 *
 * Copyright (C) 2017 yetist <yetist@gmail.com>
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
#include "gsw-verse-key.h"
#include "gsw-enum.h"

G_BEGIN_DECLS

typedef void GswModule;

typedef enum
{
	GSW_SEARCH_TYPE_REGEX     = 1L,
	GSW_SEARCH_TYPE_PHRASE    = -1L,
	GSW_SEARCH_TYPE_MULTIWORD = -2L,
	GSW_SEARCH_TYPE_ENTRYATTR = -3L,
	GSW_SEARCH_TYPE_LUCENE    = -4L,
} GswSearchType;

typedef void (*PercentCallback) (gchar, gpointer);

GswModule*   gsw_module_new                     (gpointer data);
void         gsw_module_terminate_search        (GswModule *module);
void         gsw_module_set_percent_callback    (GswModule *module, PercentCallback func, gpointer userdata);
GList*       gsw_module_search                  (GswModule *module, const gchar *searchString, GswSearchType searchType, glong flags, const gchar *scope);
gboolean     gsw_module_pop_error               (GswModule *module);
long         gsw_module_get_entry_size          (GswModule *module);
GList*       gsw_module_get_entry_attributes    (GswModule *module, const gchar *level1, const gchar *level2,
		const gchar *level3, gboolean filteredBool);
GList*       gsw_module_parse_key_list          (GswModule *module, const gchar *keyText);
void         gsw_module_set_key_text            (GswModule *module, const gchar *key);
const gchar* gsw_module_get_key_text            (GswModule *module);
gboolean     gsw_module_has_key_children        (GswModule *module);
GList*       gsw_module_get_key_children        (GswModule *module);
const gchar* gsw_module_get_name                (GswModule *module);
const gchar* gsw_module_get_type                (GswModule *module);
const gchar* gsw_module_get_description         (GswModule *module);
const gchar* gsw_module_get_category            (GswModule *module);
const gchar* gsw_module_get_key_parent          (GswModule *module);
GswVerseKey* gsw_module_get_verse_key           (GswModule *module);
void         gsw_module_previous                (GswModule *module);
void         gsw_module_next                    (GswModule *module);
void         gsw_module_begin                   (GswModule *module);
gchar*       gsw_module_strip_text              (GswModule *module);
const gchar* gsw_module_render_text             (GswModule *module);
const gchar* gsw_module_get_render_header       (GswModule *module);
const gchar* gsw_module_get_raw_entry           (GswModule *module);
void         gsw_module_set_raw_entry           (GswModule *module, const gchar *entryBuffer);
const gchar* gsw_module_get_config_entry        (GswModule *module, const gchar *key);
void         gsw_module_delete_search_framework (GswModule *module);
gboolean     gsw_module_has_search_framework    (GswModule *module);
GswVerseKey* gsw_module_create_key              (GswModule *module);
void         gsw_module_set_skip_consecutive_links (GswModule *module, gboolean val);
void         gsw_module_set_position            (GswModule *module, GswPosition pos);

G_END_DECLS

#endif /* __GSW_MODULE_H__ */
