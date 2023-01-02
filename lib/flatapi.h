/******************************************************************************
 *
 *  flatapi.h -	This file contains an api usable by non-C++ environments
 *
 * $Id$
 *
 * Copyright 2002-2014 CrossWire Bible Society (http://www.crosswire.org)
 *	CrossWire Bible Society
 *	P. O. Box 2528
 *	Tempe, AZ  85280-2528
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#ifndef SWORDFLATAPI_H
#define SWORDFLATAPI_H

#include <inttypes.h>
#include <defs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SWHANDLE intptr_t

struct gsw_ModInfo {
	char *name;
	char *description;
	char *category;
	char *language;
	char *version;
	char *delta;
};


struct gsw_SearchHit {
	const char *modName;
	char *key;
	long  score;
};


#undef gsw_SWModule_SEARCHTYPE_REGEX
#define gsw_SWModule_SEARCHTYPE_REGEX 1L
#undef gsw_SWModule_SEARCHTYPE_PHRASE
#define gsw_SWModule_SEARCHTYPE_PHRASE -1L
#undef gsw_SWModule_SEARCHTYPE_MULTIWORD
#define gsw_SWModule_SEARCHTYPE_MULTIWORD -2L
#undef gsw_SWModule_SEARCHTYPE_ENTRYATTR
#define gsw_SWModule_SEARCHTYPE_ENTRYATTR -3L
#undef gsw_SWModule_SEARCHTYPE_LUCENE
#define gsw_SWModule_SEARCHTYPE_LUCENE -4L

// ====================== SWModule ======================
/*
 * Class:     gsw_SWModule
 * Method:    terminateSearch
 * Signature: ()V
 */
void  gsw_SWModule_terminateSearch
  (SWHANDLE hSWModule);

const struct gsw_SearchHit *  gsw_SWModule_search
  (SWHANDLE hSWModule, const char *searchString, int searchType, long flags, const char *scope, SWHANDLE progressReporter);

char  gsw_SWModule_popError
  (SWHANDLE hSWModule);

long  gsw_SWModule_getEntrySize
  (SWHANDLE hSWModule);

const char **  gsw_SWModule_getEntryAttribute
  (SWHANDLE hSWModule, const char *level1, const char *level2, const char *level3, char filteredBool);

const char **  gsw_SWModule_parseKeyList
  (SWHANDLE hSWModule, const char *keyText);

// Special values handled for VerseKey modules:
//	[+-][book|chapter]	- [de|in]crement by chapter or book
//	(e.g.	"+chapter" will increment the VerseKey 1 chapter)
//	[=][key]		- position absolutely and don't normalize
//	(e.g.	"jn.1.0" for John Chapter 1 intro; "jn.0.0" For Book of John Intro)
void  gsw_SWModule_setKeyText
  (SWHANDLE hSWModule, const char *key);

const char *  gsw_SWModule_getKeyText
  (SWHANDLE hSWModule);

char  gsw_SWModule_hasKeyChildren
  (SWHANDLE hSWModule);

// This method returns child nodes for a genbook,
// but has special handling if called on a VerseKey module:
//  [0..7] [testament, book, chapter, verse, chapterMax, verseMax, bookName, osisRef]
const char **  gsw_SWModule_getKeyChildren
  (SWHANDLE hSWModule);

const char *  gsw_SWModule_getName
  (SWHANDLE hSWModule);

const char *  gsw_SWModule_getDescription
  (SWHANDLE hSWModule);

const char *  gsw_SWModule_getCategory
  (SWHANDLE hSWModule);

const char *  gsw_SWModule_getKeyParent
  (SWHANDLE hSWModule);

void  gsw_SWModule_previous
  (SWHANDLE hSWModule);

void  gsw_SWModule_next
  (SWHANDLE hSWModule);

void  gsw_SWModule_begin
  (SWHANDLE hSWModule);

const char *  gsw_SWModule_stripText
  (SWHANDLE hSWModule);

const char *  gsw_SWModule_renderText
  (SWHANDLE hSWModule);

// CSS styles associated with this text
const char *  gsw_SWModule_getRenderHeader
  (SWHANDLE hSWModule);

const char *  gsw_SWModule_getRawEntry
  (SWHANDLE hSWModule);

void  gsw_SWModule_setRawEntry
  (SWHANDLE hSWModule, const char *entryBuffer);

const char *  gsw_SWModule_getConfigEntry
  (SWHANDLE hSWModule, const char *key);

void  gsw_SWModule_deleteSearchFramework
  (SWHANDLE hSWModule);

char  gsw_SWModule_hasSearchFramework
  (SWHANDLE hSWModule);

// ====================== SWMgr ======================

SWHANDLE  gsw_SWMgr_new ();


SWHANDLE  gsw_SWMgr_newWithPath (const char *path);

void  gsw_SWMgr_delete (SWHANDLE hSWMgr);

const char *  gsw_SWMgr_version
  (SWHANDLE hSWMgr);

const struct gsw_ModInfo *  gsw_SWMgr_getModInfoList
  (SWHANDLE hSWMgr);

SWHANDLE  gsw_SWMgr_getModuleByName
  (SWHANDLE hSWMgr, const char *moduleName);

const char *  gsw_SWMgr_getPrefixPath
  (SWHANDLE hSWMgr);

const char *  gsw_SWMgr_getConfigPath
  (SWHANDLE hSWMgr);

void  gsw_SWMgr_setGlobalOption
  (SWHANDLE hSWMgr, const char *option, const char *value);

const char *  gsw_SWMgr_getGlobalOption
  (SWHANDLE hSWMgr, const char *option);

const char *  gsw_SWMgr_getGlobalOptionTip
  (SWHANDLE hSWMgr, const char *option);

const char *  gsw_SWMgr_filterText
  (SWHANDLE hSWMgr, const char *filterName, const char *text);

const char **  gsw_SWMgr_getGlobalOptions
  (SWHANDLE hSWMgr);

const char **  gsw_SWMgr_getGlobalOptionValues
  (SWHANDLE hSWMgr, const char *option);

void  gsw_SWMgr_setCipherKey
  (SWHANDLE hSWMgr, const char *modName, const char *key);

void  gsw_SWMgr_setJavascript
  (SWHANDLE hSWMgr, char valueBool);

const char **  gsw_SWMgr_getAvailableLocales
  (SWHANDLE hSWMgr);

void  gsw_SWMgr_setDefaultLocale
  (SWHANDLE hSWMgr, const char *name);

const char *  gsw_SWMgr_translate
  (SWHANDLE hSWMgr, const char *text, const char *localeName);




// ====================== InstallMgr ======================

//
// InstallMgr methods
//
//


SWHANDLE  gsw_InstallMgr_new
  (const char *baseDir, SWHANDLE statusReporter);

void  gsw_InstallMgr_delete
  (SWHANDLE hInstallMgr);

void  gsw_InstallMgr_setUserDisclaimerConfirmed
  (SWHANDLE hInstallMgr);

int  gsw_InstallMgr_syncConfig
  (SWHANDLE hInstallMgr);

int  gsw_InstallMgr_uninstallModule
  (SWHANDLE hInstallMgr, SWHANDLE hSWMgr_removeFrom, const char *modName);

const char **  gsw_InstallMgr_getRemoteSources
  (SWHANDLE hInstallMgr);

int  gsw_InstallMgr_refreshRemoteSource
  (SWHANDLE hInstallMgr, const char *sourceName);

const struct gsw_ModInfo *  gsw_InstallMgr_getRemoteModInfoList
  (SWHANDLE hInstallMgr, SWHANDLE hSWMgr_deltaCompareTo, const char *sourceName);

int  gsw_InstallMgr_remoteInstallModule
  (SWHANDLE hInstallMgr_from, SWHANDLE hSWMgr_to, const char *sourceName, const char *modName);

SWHANDLE  gsw_InstallMgr_getRemoteModuleByName
  (SWHANDLE hInstallMgr, const char *sourceName, const char *modName);

#ifdef __cplusplus
}
#endif
#endif
