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
#include <versekey.h>
#include <treekeyidx.h>
#include <swbuf.h>
#include "webmgr.hpp"
#include "gsw-module.h"

// should clean begin
#define SWHANDLE intptr_t
// should clean end

struct _GswSearchHit {
	const gchar *modName;
	gchar *key;
	long  score;
};

GswSearchHit* gsw_search_hit_new (const char *modName, char *key, long  score)
{
	GswSearchHit *hit;
	hit = g_new0(GswSearchHit, 1);
	hit->modName     = modName;
	hit->key = g_strdup(key);
	hit->score = score;
	return hit;
}

const char* gsw_search_hit_get_name  (GswSearchHit *hit)
{
	g_return_val_if_fail(hit, NULL);
	return hit->modName;
}

char* gsw_search_hit_get_key   (GswSearchHit *hit)
{
	g_return_val_if_fail(hit, NULL);
	return hit->key;
}

long gsw_search_hit_get_score (GswSearchHit *hit)
{
	g_return_val_if_fail(hit, 0);
	return hit->score;
}

void gsw_search_hit_free      (GswSearchHit *hit)
{
	if (hit->key != NULL) g_free(hit->key);
}

#define GETSWMODULE(handle, failReturn) HandleSWModule *hmod = (HandleSWModule *)handle; if (!hmod) return failReturn; SWModule *module = hmod->mod; if (!module) return failReturn;

using sword::VerseKey;
using sword::SWBuf;
using sword::TreeKeyIdx;

namespace {
	void clearStringArray(const char ***stringArray) {
		if (*stringArray) {
			for (int i = 0; true; ++i) {
				if ((*stringArray)[i]) {
					delete [] (*stringArray)[i];
				}
				else break;
			}
			free((*stringArray));
			(*stringArray) = 0;
		}
	}


	struct pu {
		char last;
		SWHANDLE progressReporter;

		void init(SWHANDLE pr) { progressReporter = pr; last = 0; }
	};
	void percentUpdate(char percent, void *userData) {
		struct pu *p = (struct pu *)userData;

		if (percent != p->last) {
			p->last = percent;
		}
	}

	class HandleSWModule {
		public:
			SWModule *mod;
			char *renderBuf;
			char *stripBuf;
			char *renderHeader;
			char *rawEntry;
			char *configEntry;
			struct pu peeuuu;
			// making searchHits cache static saves memory only having a single
			// outstanding copy, but also is not threadsafe.  Remove static here
			// and fix compiling bugs and add clearSearchHits() to d-tor to change
			static gsw_SearchHit *searchHits;
			static const char **entryAttributes;
			static const char **parseKeyList;
			static const char **keyChildren;

			HandleSWModule(SWModule *mod) {
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

			static void clearSearchHits() {
				if (searchHits) {
					for (int i = 0; true; ++i) {
						if (searchHits[i].modName) {
							delete [] searchHits[i].key;
						}
						else break;
					}
					free(searchHits);
					searchHits = 0;
				}
			}
			static void clearEntryAttributes() {
				clearStringArray(&entryAttributes);
			}
			static void clearParseKeyList() {
				clearStringArray(&parseKeyList);
			}
			static void clearKeyChildren() {
				clearStringArray(&keyChildren);
			}
	};


	gsw_SearchHit *HandleSWModule::searchHits = 0;
	const char **HandleSWModule::entryAttributes = 0;
	const char **HandleSWModule::parseKeyList = 0;
	const char **HandleSWModule::keyChildren = 0;
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

#if 0
const struct gsw_SearchHit *  gsw_SWModule_search
(SWHANDLE hSWModule, const char *searchString, int searchType, long flags, const char *scope, SWHANDLE progressReporter)
{

	GETSWMODULE(hSWModule, 0);

	hmod->clearSearchHits();

	sword::ListKey lscope;
	sword::ListKey result;


	hmod->peeuuu.init(progressReporter);
	if ((scope) && (strlen(scope)) > 0) {
		sword::SWKey *p = module->createKey();
		sword::VerseKey *parser = SWDYNAMIC_CAST(VerseKey, p);
		if (!parser) {
			delete p;
			parser = new VerseKey();
		}
		*parser = module->getKeyText();
		lscope = parser->parseVerseList(scope, *parser, true);
		result = module->search(searchString, searchType, flags, &lscope, 0, &percentUpdate, &(hmod->peeuuu));
		delete parser;
	}
	else	result = module->search(searchString, searchType, flags, 0, 0, &percentUpdate, &(hmod->peeuuu));

	int count = 0;
	for (result = sword::TOP; !result.popError(); result++) count++;

	// if we're sorted by score, let's re-sort by verse, because Java can always re-sort by score
	result = sword::TOP;
	if ((count) && (long)result.getElement()->userData)
		result.sort();

	struct gsw_SearchHit *retVal = (struct gsw_SearchHit *)calloc(count+1, sizeof(struct gsw_SearchHit));

	int i = 0;
	for (result = sword::TOP; !result.popError(); result++) {
		// don't alloc this; we have a persistent const char * in SWModule we can just reference
		retVal[i].modName = module->getName();
		stdstr(&(retVal[i].key), assureValidUTF8(result.getShortText()));
		retVal[i++].score = (long)result.getElement()->userData;
		// in case we limit count to a max number of hits
		if (i >= count) break;
	}
	hmod->searchHits = retVal;
	return retVal;
}
#endif

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

#if 0
const char **  gsw_SWModule_getEntryAttribute
(SWHANDLE hSWModule, const char *level1, const char *level2, const char *level3, char filteredBool)
{

	GETSWMODULE(hSWModule, 0);

	hmod->clearEntryAttributes();

	module->renderText();	// force parse
	std::vector<SWBuf> results;

	sword::AttributeTypeList &entryAttribs = module->getEntryAttributes();
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

	const char **retVal = (const char **)calloc(results.size()+1, sizeof(const char *));
	for (int i = 0; i < (int)results.size(); i++) {
		if (filteredBool) {
			stdstr((char **)&(retVal[i]), assureValidUTF8(module->renderText(results[i].c_str())));
		}
		else {
			stdstr((char **)&(retVal[i]), assureValidUTF8(results[i].c_str()));
		}
	}

	hmod->entryAttributes = retVal;
	return retVal;
}

const char **  gsw_SWModule_parseKeyList
(SWHANDLE hSWModule, const char *keyText)
{

	GETSWMODULE(hSWModule, 0);

	hmod->clearParseKeyList();

	sword::VerseKey *parser = dynamic_cast<VerseKey *>(module->getKey());
	const char **retVal = 0;
	if (parser) {
		sword::ListKey result;
		result = parser->parseVerseList(keyText, *parser, true);
		int count = 0;
		for (result = sword::TOP; !result.popError(); result++) {
			count++;
		}
		retVal = (const char **)calloc(count+1, sizeof(const char *));
		count = 0;
		for (result = sword::TOP; !result.popError(); result++) {
			stdstr((char **)&(retVal[count++]), assureValidUTF8(VerseKey(result).getOSISRef()));
		}
	}
	else	{
		retVal = (const char **)calloc(2, sizeof(const char *));
		stdstr((char **)&(retVal[0]), assureValidUTF8(keyText));
	}

	hmod->parseKeyList = retVal;
	return retVal;
}
#endif

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

#if 0
// This method returns child nodes for a genbook,
// but has special handling if called on a VerseKey module:
//  [0..7] [testament, book, chapter, verse, chapterMax, verseMax, bookName, osisRef]
const char ** gsw_SWModule_getKeyChildren (SWHANDLE hSWModule)
{
	GETSWMODULE(hSWModule, 0);

	hmod->clearKeyChildren();

	sword::SWKey *key = module->getKey();
	const char **retVal = 0;
	int count = 0;

	sword::VerseKey *vkey = SWDYNAMIC_CAST(VerseKey, key);
	if (vkey) {
		retVal = (const char **)calloc(9, sizeof(const char *));
		SWBuf num;
		num.appendFormatted("%d", vkey->getTestament());
		stdstr((char **)&(retVal[0]), num.c_str());
		num = "";
		num.appendFormatted("%d", vkey->getBook());
		stdstr((char **)&(retVal[1]), num.c_str());
		num = "";
		num.appendFormatted("%d", vkey->getChapter());
		stdstr((char **)&(retVal[2]), num.c_str());
		num = "";
		num.appendFormatted("%d", vkey->getVerse());
		stdstr((char **)&(retVal[3]), num.c_str());
		num = "";
		num.appendFormatted("%d", vkey->getChapterMax());
		stdstr((char **)&(retVal[4]), num.c_str());
		num = "";
		num.appendFormatted("%d", vkey->getVerseMax());
		stdstr((char **)&(retVal[5]), num.c_str());
		stdstr((char **)&(retVal[6]), vkey->getBookName());
		stdstr((char **)&(retVal[7]), vkey->getOSISRef());
	} else {
		TreeKeyIdx *tkey = SWDYNAMIC_CAST(TreeKeyIdx, key);
		if (tkey) {
			if (tkey->firstChild()) {
				do {
					count++;
				}
				while (tkey->nextSibling());
				tkey->parent();
			}
			retVal = (const char **)calloc(count+1, sizeof(const char *));
			count = 0;
			if (tkey->firstChild()) {
				do {
					stdstr((char **)&(retVal[count++]), assureValidUTF8(tkey->getLocalName()));
				}
				while (tkey->nextSibling());
				tkey->parent();
			}
		}
	}

	hmod->keyChildren = retVal;
	return retVal;
}
#endif

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
