/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-module.c: This file is part of ____
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
#include <vector>
#include <versekey.h>
#include <treekeyidx.h>
#include <swbuf.h>
#include <swmodule.h>
#include <utilstr.h>
#include "gsw-module.h"
#include "gsw-search-hit.h"

using namespace sword;
using sword::SWModule;
using sword::VerseKey;
using sword::SWBuf;
using sword::TreeKeyIdx;

namespace {
	class HandleSWModule {
		public:
			SWModule *mod;
			char *renderBuf;
			char *stripBuf;
			char *renderHeader;
			char *rawEntry;
			char *configEntry;
			// making searchHits cache static saves memory only having a single
			// outstanding copy, but also is not threadsafe.  Remove static here
			// and fix compiling bugs and add clearSearchHits() to d-tor to change
			PercentCallback callback;
			void *userdata;

			HandleSWModule(SWModule *mod) {
				this->callback = NULL;
				this->userdata = NULL;
				this->mod = mod;
				this->renderBuf = 0;
				this->stripBuf = 0;
				this->renderHeader = 0;
				this->rawEntry = 0;
				this->configEntry = 0;
			}
			~HandleSWModule() {
				delete [] renderBuf;
				delete [] stripBuf;
				delete [] renderHeader;
				delete [] rawEntry;
				delete [] configEntry;
			}

			void setPercentFunc(PercentCallback func) {
				this->callback = func;
			}

			void setPercentUserdata(void *data) {
				this->userdata = data;
			}
	};
}

GswModule* gsw_module_new (gpointer data)
{
	SWModule *mod;
	GswModule* module;

	mod = (SWModule*) data;
	module = new HandleSWModule(mod);

	return (GswModule*) module;
}

void  gsw_module_terminate_search (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return;
	}
	mod = hmod->mod;
	if (!mod) {
		return;
	}
	mod->terminateSearch = true;
}

void gsw_module_set_percent_callback (GswModule *module, PercentCallback func, gpointer userdata)
{
	HandleSWModule *hmod;
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return;
	}

	hmod->setPercentFunc(func);
	hmod->setPercentUserdata(userdata);
}

GList* gsw_module_search (GswModule *module, const char *searchString, ModuleSearchType searchType, glong flags, const gchar *scope)
//const struct gsw_SearchHit *  gsw_SWModule_search (SWHANDLE hSWModule, const char *searchString, int searchType, long flags, const char *scope, SWHANDLE progressReporter)
{
	HandleSWModule *hmod;
	SWModule *mod;
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return NULL;
	}
	mod = hmod->mod;
	if (!mod) {
		return NULL;
	}

//	hmod->clearSearchHits();

	sword::ListKey lscope;
	sword::ListKey result;

//	hmod->peeuuu.init(progressReporter);
	if ((scope) && (strlen(scope)) > 0) {
		sword::SWKey *p = mod->createKey();
		sword::VerseKey *parser = SWDYNAMIC_CAST(VerseKey, p);
		if (!parser) {
			delete p;
			parser = new VerseKey();
		}
		//ListKey &SWModule::search(const char *istr, int searchType, int flags, SWKey *scope, bool *justCheckIfSupported, void (*percent)(char, void *), void *percentUserData)
		*parser = mod->getKeyText();
		lscope = parser->parseVerseList(scope, *parser, true);
		result = mod->search(searchString, searchType, flags, &lscope, 0, hmod->callback, hmod->userdata); // &percentUpdate, &(hmod->peeuuu));
		delete parser;
	} else {
		result = mod->search(searchString, searchType, flags, 0, 0, hmod->callback, hmod->userdata);
	}

	int count = 0;
	for (result = sword::TOP; !result.popError(); result++) count++;

	// if we're sorted by score, let's re-sort by verse, because Java can always re-sort by score
	result = sword::TOP;
	if ((count) && (long)result.getElement()->userData)
		result.sort();

//	struct gsw_SearchHit *retVal = (struct gsw_SearchHit *)calloc(count+1, sizeof(struct gsw_SearchHit));

	int i = 0;
	GList *results = NULL;
	for (result = sword::TOP; !result.popError(); result++) {
		GswSearchHit *hit;
		hit = gsw_search_hit_new(mod->getName(), assureValidUTF8(result.getShortText()), (long)result.getElement()->userData);
		results = g_list_append(results, hit);
		i++;
		if (i >= count) break;
	}
//	hmod->searchHits = results;
	return results;
}

gboolean gsw_module_pop_error (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return FALSE;
	}
	mod = hmod->mod;
	if (!mod) {
		return FALSE;
	}

	return mod->popError();
}

long gsw_module_get_entry_size (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return 0;
	}
	mod = hmod->mod;
	if (!mod) {
		return 0;
	}

	return mod->getEntrySize();
}

GList* gsw_module_get_entry_attributes (GswModule *module, const gchar *level1, const gchar *level2,
		const gchar *level3, gboolean filteredBool)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return 0;
	}
	mod = hmod->mod;
	if (!mod) {
		return 0;
	}

//	hmod->clearEntryAttributes();

	mod->renderText();	// force parse
	std::vector<SWBuf> results;

	sword::AttributeTypeList &entryAttribs = mod->getEntryAttributes();
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
			attributes = g_list_append(attributes, g_strdup(assureValidUTF8(mod->renderText(results[i].c_str())).c_str()));
		}
		else {
			attributes = g_list_append(attributes, g_strdup(assureValidUTF8(results[i].c_str()).c_str()));
		}
	}

//	hmod->attributes = attributes;
	return attributes;
}

GList* gsw_module_parse_key_list (GswModule *module, const gchar *keyText)
{
	HandleSWModule *hmod;
	SWModule *mod;
	GList *list = NULL;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return NULL;
	}
	mod = hmod->mod;
	if (!mod) {
		return NULL;
	}
	sword::VerseKey *parser = dynamic_cast<VerseKey *>(mod->getKey());
	if (parser) {
		sword::ListKey result;
		result = parser->parseVerseList(keyText, *parser, true);
		for (result = sword::TOP; !result.popError(); result++) {
			list = g_list_append(list, g_strdup(assureValidUTF8(VerseKey(result).getOSISRef())));
		}
	} else {
		list = g_list_append(list, g_strdup(assureValidUTF8(keyText)));
	}

	return list;
}

// Special values handled for VerseKey modules:
//	[+-][book|chapter]	- [de|in]crement by chapter or book
//	(e.g.	"+chapter" will increment the VerseKey 1 chapter)
//	[=][key]		- position absolutely and don't normalize
//	(e.g.	"jn.1.0" for John Chapter 1 intro; "jn.0.0" For Book of John Intro)
void  gsw_module_set_key_text (GswModule *module, const gchar *keyText)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return;
	}
	mod = hmod->mod;
	if (!mod) {
		return;
	}

	sword::SWKey *key = mod->getKey();
	sword::VerseKey *vkey = SWDYNAMIC_CAST(VerseKey, key);
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
			vkey->setIntros(true);
			vkey->setAutoNormalize(false);
			vkey->setText(keyText+1);
			return;
		}
	}
	mod->setKey(keyText);
}

const gchar *  gsw_module_get_key_text (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return NULL;
	}
	mod = hmod->mod;
	if (!mod) {
		return NULL;
	}

	return mod->getKeyText();
}

gboolean gsw_module_has_key_children (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	gboolean retVal = FALSE;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return FALSE;
	}
	mod = hmod->mod;
	if (!mod) {
		return FALSE;
	}

	sword::SWKey *key = mod->getKey();

	TreeKeyIdx *tkey = SWDYNAMIC_CAST(TreeKeyIdx, key);
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
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return NULL;
	}
	mod = hmod->mod;
	if (!mod) {
		return NULL;
	}

	GList *list = NULL;

	sword::SWKey *key = mod->getKey();

	sword::VerseKey *vkey = SWDYNAMIC_CAST(VerseKey, key);
	if (vkey) {
		SWBuf num;
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
		TreeKeyIdx *tkey = SWDYNAMIC_CAST(TreeKeyIdx, key);
		if (tkey) {
			if (tkey->firstChild()) {
				do {
					list = g_list_append(list,  g_strdup(assureValidUTF8(tkey->getLocalName())));
				} while (tkey->nextSibling());
				tkey->parent();
			}
		}
	}
	return list;
}

const gchar* gsw_module_get_name (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return NULL;
	}
	mod = hmod->mod;
	if (!mod) {
		return NULL;
	}
	return mod->getName();
}

const gchar* gsw_module_get_description (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return NULL;
	}
	mod = hmod->mod;
	if (!mod) {
		return NULL;
	}

	return mod->getDescription();
}

const gchar* gsw_module_get_category (GswModule *module)
{
	static SWBuf type;
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return NULL;
	}
	mod = hmod->mod;
	if (!mod) {
		return NULL;
	}

	type = mod->getType();
	SWBuf cat = mod->getConfigEntry("Category");
	if (cat.length() > 0)
		type = cat;

	return type.c_str();
}

const gchar* gsw_module_get_key_parent  (GswModule *module)
{
	static SWBuf retVal;
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return NULL;
	}
	mod = hmod->mod;
	if (!mod) {
		return NULL;
	}

	sword::SWKey *key = mod->getKey();

	retVal = "";

	TreeKeyIdx *tkey = SWDYNAMIC_CAST(TreeKeyIdx, key);
	if (tkey) {
		if (tkey->parent()) {
			retVal = tkey->getText();
		}
	}
	return assureValidUTF8(retVal.c_str());
}

void gsw_module_previous (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return;
	}
	mod = hmod->mod;
	if (!mod) {
		return;
	}
	mod->decrement();
}

void gsw_module_next (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return;
	}
	mod = hmod->mod;
	if (!mod) {
		return;
	}
	mod->increment();
}

void gsw_module_begin (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return;
	}
	mod = hmod->mod;
	if (!mod) {
		return;
	}
	mod->setPosition(sword::TOP);
}


const gchar* gsw_module_strip_text (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return NULL;
	}
	mod = hmod->mod;
	if (!mod) {
		return NULL;
	}
	stdstr(&(hmod->stripBuf), assureValidUTF8((const char *)mod->stripText()));
	return hmod->stripBuf;
}

const gchar* gsw_module_render_text (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return NULL;
	}
	mod = hmod->mod;
	if (!mod) {
		return NULL;
	}

	stdstr(&(hmod->renderBuf), assureValidUTF8((const char *)mod->renderText().c_str()));
	return hmod->renderBuf;
}

// CSS styles associated with this text
const gchar* gsw_module_get_render_header (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return NULL;
	}
	mod = hmod->mod;
	if (!mod) {
		return NULL;
	}

	stdstr(&(hmod->renderHeader), assureValidUTF8(((const char *)(mod->getRenderHeader() ? mod->getRenderHeader():""))));

	return hmod->renderHeader;
}

const gchar* gsw_module_get_raw_entry (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return NULL;
	}
	mod = hmod->mod;
	if (!mod) {
		return NULL;
	}

	stdstr(&(hmod->rawEntry), assureValidUTF8(((const char *)mod->getRawEntry())));
	return hmod->rawEntry;
}

void gsw_module_set_raw_entry (GswModule *module, const char *entryBuffer)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return;
	}
	mod = hmod->mod;
	if (!mod) {
		return;
	}
	mod->setEntry(entryBuffer);
}


const gchar* gsw_module_get_config_entry (GswModule *module, const char *key)
{

	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return NULL;
	}
	mod = hmod->mod;
	if (!mod) {
		return NULL;
	}

	stdstr(&(hmod->configEntry), (mod->getConfigEntry(key) ? assureValidUTF8(mod->getConfigEntry(key)).c_str() : 0));

	return hmod->configEntry;
}

void gsw_module_delete_search_framework (GswModule *module)
{
	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return;
	}
	mod = hmod->mod;
	if (!mod) {
		return;
	}

	mod->deleteSearchFramework(); 
}

gboolean gsw_module_has_search_framework (GswModule *module)
{

	HandleSWModule *hmod;
	SWModule *mod;
	
	hmod = (HandleSWModule *) module;
	if (!hmod) {
		return FALSE;
	}
	mod = hmod->mod;
	if (!mod) {
		return FALSE;
	}

	return (mod->hasSearchFramework() && mod->isSearchOptimallySupported("God", -4, 0, 0));
}
