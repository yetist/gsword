/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-module.c: This file is part of gsword.
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
#include <vector>
#include <versekey.h>
#include <treekeyidx.h>
#include <swmodule.h>
#include "gsw-module.h"
#include "gsw-result.h"
#include "gsw-private.h"

enum {
	SEARCHING,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _GswModulePrivate GswModulePrivate;

struct _GswModulePrivate
{
	sword::SWModule *module;
};

G_DEFINE_TYPE_WITH_PRIVATE (GswModule, gsw_module, G_TYPE_OBJECT);

static void gsw_module_searching(gchar now, gpointer data) {
	GswModule *module = (GswModule*) data;
	g_signal_emit (module, signals[SEARCHING], 0, now);
}

static void gsw_module_dispose (GObject *object)
{
	GswModule* module = GSW_MODULE(object);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);

	if (priv->module != NULL)
	{
		delete priv->module;
		priv->module = NULL;
	}
	G_OBJECT_CLASS (gsw_module_parent_class)->dispose (object);
}

static void gsw_module_class_init (GswModuleClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->dispose = gsw_module_dispose;
	signals[SEARCHING] =
		g_signal_new ("searching",
				G_TYPE_FROM_CLASS (gobject_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (GswModuleClass, searching),
				NULL, NULL,
				g_cclosure_marshal_VOID__UINT,
				G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void gsw_module_init (GswModule *module)
{
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	priv->module = NULL;
}

GswModule* gsw_module_new (gpointer data)
{
	GswModule *module;
	GswModulePrivate *priv;

	module = (GswModule*) g_object_new (GSW_TYPE_MODULE, NULL);
	priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	priv->module = (sword::SWModule*) data;
	return module;
}

void gsw_module_terminate_search (GswModule *module)
{
	g_return_if_fail(GSW_IS_MODULE(module));
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	priv->module->terminateSearch = TRUE;
}

GList* gsw_module_search (GswModule *module, const gchar *searchString, GswSearchType searchType, glong flags, const gchar *scope)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);

	sword::ListKey lscope;
	sword::ListKey result;
	if (scope != NULL && strlen(scope) > 0) {
		sword::SWKey *p = priv->module->createKey();
		sword::VerseKey *parser = SWDYNAMIC_CAST(sword::VerseKey, p);
		if (!parser) {
			delete p;
			parser = new sword::VerseKey();
		}
		*parser = priv->module->getKeyText();
		lscope = parser->parseVerseList(scope, *parser, TRUE);
		result = priv->module->search(searchString, searchType, flags, &lscope, 0, gsw_module_searching, module);
		delete parser;
	} else {
		result = priv->module->search(searchString, searchType, flags, 0, 0, gsw_module_searching, module);
	}

	int count = 0;
	for (result = sword::TOP; !result.popError(); result++) count++;

	// if we're sorted by score, let's re-sort by verse, because Java can always re-sort by score
	result = sword::TOP;
	if ((count) && (long)result.getElement()->userData)
		result.sort();

	int i = 0;
	GList *results = NULL;
	for (result = sword::TOP; !result.popError(); result++) {
		GswResult *hit;
		hit = gsw_result_new(priv->module->getName(), sword::assureValidUTF8(result.getShortText()), (long)result.getElement()->userData);
		results = g_list_append(results, hit);
		i++;
		if (i >= count) break;
	}
	return results;
}

gboolean gsw_module_pop_error (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), FALSE);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	return priv->module->popError();
}

long gsw_module_get_entry_size (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), 0);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	return priv->module->getEntrySize();
}

GList* gsw_module_get_entry_attributes (GswModule *module, const gchar *level1, const gchar *level2,
		const gchar *level3, gboolean filteredBool)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);

	priv->module->renderText();	// force parse
	std::vector<sword::SWBuf> results;

	sword::AttributeTypeList &entryAttribs = priv->module->getEntryAttributes();
	sword::AttributeTypeList::iterator i1Start, i1End;
	sword::AttributeList::iterator i2Start, i2End;
	sword::AttributeValue::iterator i3Start, i3End;

	if ((level1) && (*level1)) {
		i1Start = entryAttribs.find(level1);
		i1End = i1Start;
		if (i1End != entryAttribs.end())
			++i1End;
	}
	else {
		i1Start = entryAttribs.begin();
		i1End   = entryAttribs.end();
	}
	for (;i1Start != i1End; ++i1Start) {
		if ((level2) && (*level2)) {
			i2Start = i1Start->second.find(level2);
			i2End = i2Start;
			if (i2End != i1Start->second.end())
				++i2End;
		}
		else {
			i2Start = i1Start->second.begin();
			i2End   = i1Start->second.end();
		}
		for (;i2Start != i2End; ++i2Start) {
			if ((level3) && (*level3)) {
				i3Start = i2Start->second.find(level3);
				i3End = i3Start;
				if (i3End != i2Start->second.end())
					++i3End;
			}
			else {
				i3Start = i2Start->second.begin();
				i3End   = i2Start->second.end();
			}
			for (;i3Start != i3End; ++i3Start) {
				results.push_back(i3Start->second);
			}
			if (i3Start != i3End)
				break;
		}
		if (i2Start != i2End)
			break;
	}

	GList *attributes = NULL;
	for (int i = 0; i < (int)results.size(); i++) {
		if (filteredBool) {
			attributes = g_list_append(attributes, g_strdup(sword::assureValidUTF8(priv->module->renderText(results[i].c_str())).c_str()));
		}
		else {
			attributes = g_list_append(attributes, g_strdup(sword::assureValidUTF8(results[i].c_str()).c_str()));
		}
	}

	return attributes;
}

GList* gsw_module_parse_key_list (GswModule *module, const gchar *keyText)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);

	GList *list = NULL;
	sword::VerseKey *parser = dynamic_cast<sword::VerseKey *>(priv->module->getKey());
	if (parser) {
		sword::ListKey result;
		result = parser->parseVerseList(keyText, *parser, true);
		for (result = sword::TOP; !result.popError(); result++) {
			list = g_list_append(list, g_strdup(sword::assureValidUTF8(sword::VerseKey(result).getOSISRef())));
		}
	} else {
		list = g_list_append(list, g_strdup(sword::assureValidUTF8(keyText)));
	}

	return list;
}

// Special values handled for VerseKey modules:
//	[+-][book|chapter]	- [de|in]crement by chapter or book
//	(e.g.	"+chapter" will increment the VerseKey 1 chapter)
//	[=][key]		- position absolutely and don't normalize
//	(e.g.	"jn.1.0" for John Chapter 1 intro; "jn.0.0" For Book of John Intro)
void gsw_module_set_key_text (GswModule *module, const gchar *keyText)
{
	g_return_if_fail(GSW_IS_MODULE(module));
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);

	sword::SWKey *key = priv->module->getKey();
	sword::VerseKey *vkey = SWDYNAMIC_CAST(sword::VerseKey, key);
	if (vkey) {
		if ((*keyText=='+' || *keyText=='-')) {
			if (!sword::stricmp(keyText+1, "book")) {
				vkey->setBook(vkey->getBook() + ((*keyText=='+')?1:-1));
				return;
			}
			else if (!sword::stricmp(keyText+1, "chapter")) {
				vkey->setChapter(vkey->getChapter() + ((*keyText=='+')?1:-1));
				return;
			}
		}
		else if (*keyText=='=') {
			vkey->setIntros(TRUE);
			vkey->setAutoNormalize(FALSE);
			vkey->setText(keyText+1);
			return;
		}
	}
	priv->module->setKey(keyText);
}

const gchar* gsw_module_get_key_text (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	return priv->module->getKeyText();
}

gboolean gsw_module_has_key_children (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), FALSE);
	gboolean retVal = FALSE;
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);

	sword::SWKey *key = priv->module->getKey();

	sword::TreeKeyIdx *tkey = SWDYNAMIC_CAST(sword::TreeKeyIdx, key);
	if (tkey) {
		retVal = tkey->hasChildren() ? TRUE : FALSE;
	}
	return retVal;
}

// This method returns child nodes for a genbook,
// but has special handling if called on a VerseKey module:
//  [0..7] [testament, book, chapter, verse, chapterMax, verseMax, bookName, osisRef]
GList* gsw_module_get_key_children (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);

	GList *list = NULL;
	sword::SWKey *key = priv->module->getKey();

	sword::VerseKey *vkey = SWDYNAMIC_CAST(sword::VerseKey, key);
	if (vkey) {
		sword::SWBuf num;
		num.appendFormatted("%d", vkey->getTestament());
		list = g_list_append(list, (gpointer) num.c_str());
		num = "";
		num.appendFormatted("%d", vkey->getBook());
		list = g_list_append(list, (gpointer) num.c_str());
		num = "";
		num.appendFormatted("%d", vkey->getChapter());
		list = g_list_append(list, (gpointer) num.c_str());
		num = "";
		num.appendFormatted("%d", vkey->getVerse());
		list = g_list_append(list, (gpointer) num.c_str());
		num = "";
		num.appendFormatted("%d", vkey->getChapterMax());
		list = g_list_append(list, (gpointer) num.c_str());
		num = "";
		num.appendFormatted("%d", vkey->getVerseMax());
		list = g_list_append(list, (gpointer) num.c_str());
		list = g_list_append(list, g_strdup(vkey->getBookName()));
		list = g_list_append(list, g_strdup(vkey->getOSISRef()));
	} else {
		sword::TreeKeyIdx *tkey = SWDYNAMIC_CAST(sword::TreeKeyIdx, key);
		if (tkey) {
			if (tkey->firstChild()) {
				do {
					list = g_list_append(list,  g_strdup(sword::assureValidUTF8(tkey->getLocalName())));
				} while (tkey->nextSibling());
				tkey->parent();
			}
		}
	}
	return list;
}

const gchar* gsw_module_get_mod_name (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	return priv->module->getName();
}

const gchar* gsw_module_get_mod_type (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	return priv->module->getType();
}

const gchar* gsw_module_get_mod_description (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	return priv->module->getDescription();
}

gchar* gsw_module_get_mod_category (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	const gchar* type;
	gchar* category;

	type = gsw_module_get_mod_type(module);
	category =  gsw_module_get_config_entry (module, "Category");
	if (category != NULL) {
		return category;
	} else {
		return g_strdup(type);
	}
}

const gchar* gsw_module_get_key_parent  (GswModule *module)
{
	static sword::SWBuf retVal;
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);

	sword::SWKey *key = priv->module->getKey();

	retVal = "";

	sword::TreeKeyIdx *tkey = SWDYNAMIC_CAST(sword::TreeKeyIdx, key);
	if (tkey) {
		if (tkey->parent()) {
			retVal = tkey->getText();
		}
	}
	return sword::assureValidUTF8(retVal.c_str());
}

void gsw_module_previous (GswModule *module)
{
	g_return_if_fail(GSW_IS_MODULE(module));
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	priv->module->decrement();
}

void gsw_module_next (GswModule *module)
{
	g_return_if_fail(GSW_IS_MODULE(module));
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	priv->module->increment();
}

void gsw_module_begin (GswModule *module)
{
	g_return_if_fail(GSW_IS_MODULE(module));
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	priv->module->setPosition(sword::TOP);
}

gchar* gsw_module_strip_text (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	return g_strdup(sword::assureValidUTF8((const char *)priv->module->stripText()));
}

gchar* gsw_module_render_text (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	return g_strdup(sword::assureValidUTF8((const char *)priv->module->renderText().c_str()));
}

// CSS styles associated with this text
gchar* gsw_module_get_render_header (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	return g_strdup(sword::assureValidUTF8(((const char *)(priv->module->getRenderHeader() ? priv->module->getRenderHeader():""))));
}

gchar* gsw_module_get_raw_entry (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	return g_strdup(sword::assureValidUTF8(((const char *)priv->module->getRawEntry())));
}

void gsw_module_set_raw_entry (GswModule *module, const gchar *entryBuffer)
{
	g_return_if_fail(GSW_IS_MODULE(module));
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	priv->module->setEntry(entryBuffer);
}

gchar* gsw_module_get_config_entry (GswModule *module, const gchar *key)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);

	if (priv->module->getConfigEntry(key) != NULL)
		return g_strdup(sword::assureValidUTF8(priv->module->getConfigEntry(key)).c_str());
	else
		return NULL;
}

void gsw_module_delete_search_framework (GswModule *module)
{
	g_return_if_fail(GSW_IS_MODULE(module));
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	priv->module->deleteSearchFramework(); 
}

gboolean gsw_module_has_search_framework (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), FALSE);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	return (priv->module->hasSearchFramework() && priv->module->isSearchOptimallySupported("God", -4, 0, 0));
}

GswVerseKey* gsw_module_get_verse_key (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	return gsw_verse_key_new (priv->module->getKey());
}

GswVerseKey* gsw_module_create_key (GswModule *module)
{
	g_return_val_if_fail(GSW_IS_MODULE(module), NULL);
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	return gsw_verse_key_new (priv->module->createKey());
}

void gsw_module_set_skip_consecutive_links (GswModule *module, gboolean val)
{
	g_return_if_fail(GSW_IS_MODULE(module));
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	priv->module -> setSkipConsecutiveLinks(val);
}

void gsw_module_set_position (GswModule *module, GswPosition pos)
{
	g_return_if_fail(GSW_IS_MODULE(module));
	GswModulePrivate *priv = (GswModulePrivate *) gsw_module_get_instance_private(module);
	priv->module->setPosition(sword::SW_POSITION((char) pos));
}
