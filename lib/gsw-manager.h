SWHANDLE  gsw_manager_new ();

SWHANDLE  gsw_manager_newWithPath (const char *path);

void  gsw_manager_delete (SWHANDLE hSWMgr);

const char *  gsw_manager_version (SWHANDLE hSWMgr);

const struct org_crosswire_sword_ModInfo *  gsw_manager_getModInfoList (SWHANDLE hSWMgr);

SWHANDLE  gsw_manager_getModuleByName (SWHANDLE hSWMgr, const char *moduleName);

const char *  gsw_manager_getPrefixPath (SWHANDLE hSWMgr);

const char *  gsw_manager_getConfigPath (SWHANDLE hSWMgr);

void  gsw_manager_setGlobalOption (SWHANDLE hSWMgr, const char *option, const char *value);

const char *  gsw_manager_getGlobalOption (SWHANDLE hSWMgr, const char *option);

const char *  gsw_manager_getGlobalOptionTip (SWHANDLE hSWMgr, const char *option);

const char *  gsw_manager_filterText (SWHANDLE hSWMgr, const char *filterName, const char *text);

const char **  gsw_manager_getGlobalOptions (SWHANDLE hSWMgr);

const char **  gsw_manager_getGlobalOptionValues (SWHANDLE hSWMgr, const char *option);

void  gsw_manager_setCipherKey (SWHANDLE hSWMgr, const char *modName, const char *key);

void  gsw_manager_setJavascript (SWHANDLE hSWMgr, char valueBool);

const char **  gsw_manager_getAvailableLocales (SWHANDLE hSWMgr);

void  gsw_manager_setDefaultLocale (SWHANDLE hSWMgr, const char *name);

const char *  gsw_manager_translate (SWHANDLE hSWMgr, const char *text, const char *localeName);
