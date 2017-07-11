/*
 * Class:     gsw_manager
 * Method:    new
 * Signature: ()V
 */
SWHANDLE  gsw_manager_new ();


/*
 * Class:     gsw_manager
 * Method:    newWithPath
 * Signature: ()V
 * Signature: (Ljava/lang/String;)V
 */
SWHANDLE  gsw_manager_newWithPath (const char *path);

/*
 * Class:     gsw_manager
 * Method:    delete
 * Signature: ()V
 */
void  gsw_manager_delete (SWHANDLE hSWMgr);

/*
 * Class:     gsw_manager
 * Method:    version
 * Signature: ()Ljava/lang/String;
 */
const char *  gsw_manager_version (SWHANDLE hSWMgr);

/*
 * Class:     gsw_manager
 * Method:    getModInfoList
 * Signature: ()[Lorg/crosswire/android/sword/SWMgr/ModInfo;
 */
const struct org_crosswire_sword_ModInfo *  gsw_manager_getModInfoList (SWHANDLE hSWMgr);

/*
 * Class:     gsw_manager
 * Method:    getModuleByName
 * Signature: (Ljava/lang/String;)Lorg/crosswire/android/sword/SWModule;
 */
SWHANDLE  gsw_manager_getModuleByName (SWHANDLE hSWMgr, const char *moduleName);

/*
 * Class:     gsw_manager
 * Method:    getPrefixPath
 * Signature: ()Ljava/lang/String;
 */
const char *  gsw_manager_getPrefixPath (SWHANDLE hSWMgr);

/*
 * Class:     gsw_manager
 * Method:    getConfigPath
 * Signature: ()Ljava/lang/String;
 */
const char *  gsw_manager_getConfigPath (SWHANDLE hSWMgr);

/*
 * Class:     gsw_manager
 * Method:    setGlobalOption
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
void  gsw_manager_setGlobalOption (SWHANDLE hSWMgr, const char *option, const char *value);

/*
 * Class:     gsw_manager
 * Method:    getGlobalOption
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */
const char *  gsw_manager_getGlobalOption (SWHANDLE hSWMgr, const char *option);

/*
 * Class:     gsw_manager
 * Method:    getGlobalOptionTip
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */
const char *  gsw_manager_getGlobalOptionTip (SWHANDLE hSWMgr, const char *option);

/*
 * Class:     gsw_manager
 * Method:    filterText
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
 */
const char *  gsw_manager_filterText (SWHANDLE hSWMgr, const char *filterName, const char *text);

/*
 * Class:     gsw_manager
 * Method:    getGlobalOptions
 * Signature: ()[Ljava/lang/String;
 */
const char **  gsw_manager_getGlobalOptions (SWHANDLE hSWMgr);

/*
 * Class:     gsw_manager
 * Method:    getGlobalOptionValues
 * Signature: (Ljava/lang/String;)[Ljava/lang/String;
 */
const char **  gsw_manager_getGlobalOptionValues (SWHANDLE hSWMgr, const char *option);

/*
 * Class:     gsw_manager
 * Method:    setCipherKey
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
void  gsw_manager_setCipherKey (SWHANDLE hSWMgr, const char *modName, const char *key);

/*
 * Class:     gsw_manager
 * Method:    setJavascript
 * Signature: (Z)V
 */
void  gsw_manager_setJavascript (SWHANDLE hSWMgr, char valueBool);

/*
 * Class:     gsw_manager
 * Method:    getAvailableLocales
 * Signature: ()[Ljava/lang/String;
 */
const char **  gsw_manager_getAvailableLocales (SWHANDLE hSWMgr);

/*
 * Class:     gsw_manager
 * Method:    setDefaultLocale
 * Signature: (Ljava/lang/String;)V
 */
void  gsw_manager_setDefaultLocale (SWHANDLE hSWMgr, const char *name);

/*
 * Class:     gsw_manager
 * Method:    translate
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
 */
const char *  gsw_manager_translate (SWHANDLE hSWMgr, const char *text, const char *localeName);
