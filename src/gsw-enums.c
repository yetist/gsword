/*
 * gsw-enums.c: GType registration for public enumerations
 */

#include <glib-object.h>
#include <gsword/gsw-enums.h>

#define GSW_DEFINE_ENUM_TYPE(function_name, type_name, values)               \
    GType function_name(void)                                                \
    {                                                                        \
        static gsize gtype = 0;                                              \
        if (g_once_init_enter(&gtype)) {                                     \
            GType registered = g_enum_register_static(type_name, values);    \
            g_once_init_leave(&gtype, registered);                           \
        }                                                                    \
        return (GType)gtype;                                                 \
    }

static const GEnumValue module_type_values[] = {
    { GSW_MODULE_TYPE_UNKNOWN,      "GSW_MODULE_TYPE_UNKNOWN",      "Unknown" },
    { GSW_MODULE_TYPE_BIBLES,       "GSW_MODULE_TYPE_BIBLES",       "Biblical Texts" },
    { GSW_MODULE_TYPE_COMMENTARIES, "GSW_MODULE_TYPE_COMMENTARIES", "Commentaries" },
    { GSW_MODULE_TYPE_LEXDICTS,     "GSW_MODULE_TYPE_LEXDICTS",     "Lexicons / Dictionaries" },
    { GSW_MODULE_TYPE_GENBOOKS,     "GSW_MODULE_TYPE_GENBOOKS",     "Generic Books" },
    { GSW_MODULE_TYPE_DAILY_DEVOS,  "GSW_MODULE_TYPE_DAILY_DEVOS",  "Daily Devotional" },
    { GSW_MODULE_TYPE_MAPS,         "GSW_MODULE_TYPE_MAPS",         "Maps" },
    { GSW_MODULE_TYPE_OTHER,        "GSW_MODULE_TYPE_OTHER",        "Other" },
    { 0, NULL, NULL }
};

static const GEnumValue driver_values[] = {
    { GSW_DRIVER_UNKNOWN,      "GSW_DRIVER_UNKNOWN",      "Unknown" },
    { GSW_DRIVER_RAW_TEXT,     "GSW_DRIVER_RAW_TEXT",     "RawText" },
    { GSW_DRIVER_RAW_TEXT4,    "GSW_DRIVER_RAW_TEXT4",    "RawText4" },
    { GSW_DRIVER_Z_TEXT,       "GSW_DRIVER_Z_TEXT",       "zText" },
    { GSW_DRIVER_Z_TEXT4,      "GSW_DRIVER_Z_TEXT4",      "zText4" },
    { GSW_DRIVER_RAW_COM,      "GSW_DRIVER_RAW_COM",      "RawCom" },
    { GSW_DRIVER_RAW_COM4,     "GSW_DRIVER_RAW_COM4",     "RawCom4" },
    { GSW_DRIVER_Z_COM,        "GSW_DRIVER_Z_COM",        "zCom" },
    { GSW_DRIVER_Z_COM4,       "GSW_DRIVER_Z_COM4",       "zCom4" },
    { GSW_DRIVER_RAW_LD,       "GSW_DRIVER_RAW_LD",       "RawLD" },
    { GSW_DRIVER_RAW_LD4,      "GSW_DRIVER_RAW_LD4",      "RawLD4" },
    { GSW_DRIVER_Z_LD,         "GSW_DRIVER_Z_LD",         "zLD" },
    { GSW_DRIVER_RAW_GENBOOK,  "GSW_DRIVER_RAW_GENBOOK",  "RawGenBook" },
    { GSW_DRIVER_RAW_FILES,    "GSW_DRIVER_RAW_FILES",    "RawFiles" },
    { GSW_DRIVER_HREF_COM,     "GSW_DRIVER_HREF_COM",     "HREFCom" },
    { 0, NULL, NULL }
};

static const GEnumValue source_type_values[] = {
    { GSW_SOURCE_FTP,   "GSW_SOURCE_FTP",   "FTP" },
    { GSW_SOURCE_HTTP,  "GSW_SOURCE_HTTP",  "HTTP" },
    { GSW_SOURCE_HTTPS, "GSW_SOURCE_HTTPS", "HTTPS" },
    { GSW_SOURCE_SFTP,  "GSW_SOURCE_SFTP",  "SFTP" },
    { 0, NULL, NULL }
};

static const GEnumValue encoding_values[] = {
    { GSW_ENCODING_UNKNOWN, "GSW_ENCODING_UNKNOWN", "Unknown" },
    { GSW_ENCODING_LATIN1,  "GSW_ENCODING_LATIN1",  "Latin-1" },
    { GSW_ENCODING_UTF8,    "GSW_ENCODING_UTF8",    "UTF-8" },
    { GSW_ENCODING_SCSU,    "GSW_ENCODING_SCSU",    "SCSU" },
    { GSW_ENCODING_UTF16,   "GSW_ENCODING_UTF16",   "UTF-16" },
    { 0, NULL, NULL }
};

static const GEnumValue direction_values[] = {
    { GSW_DIRECTION_LTR,  "GSW_DIRECTION_LTR",  "Left-to-right" },
    { GSW_DIRECTION_RTL,  "GSW_DIRECTION_RTL",  "Right-to-left" },
    { GSW_DIRECTION_BIDI, "GSW_DIRECTION_BIDI", "Bidirectional" },
    { 0, NULL, NULL }
};

static const GEnumValue markup_values[] = {
    { GSW_MARKUP_UNKNOWN, "GSW_MARKUP_UNKNOWN", "Unknown" },
    { GSW_MARKUP_PLAIN,   "GSW_MARKUP_PLAIN",   "Plain" },
    { GSW_MARKUP_THML,    "GSW_MARKUP_THML",    "ThML" },
    { GSW_MARKUP_GBF,     "GSW_MARKUP_GBF",     "GBF" },
    { GSW_MARKUP_OSIS,    "GSW_MARKUP_OSIS",    "OSIS" },
    { GSW_MARKUP_TEI,     "GSW_MARKUP_TEI",     "TEI" },
    { 0, NULL, NULL }
};

static const GEnumValue position_values[] = {
    { GSW_POSITION_TOP,    "GSW_POSITION_TOP",    "Top" },
    { GSW_POSITION_BOTTOM, "GSW_POSITION_BOTTOM", "Bottom" },
    { 0, NULL, NULL }
};

GSW_DEFINE_ENUM_TYPE(gsw_module_type_get_type, "GswModuleType", module_type_values)
GSW_DEFINE_ENUM_TYPE(gsw_driver_get_type, "GswDriver", driver_values)
GSW_DEFINE_ENUM_TYPE(gsw_source_type_get_type, "GswSourceType", source_type_values)
GSW_DEFINE_ENUM_TYPE(gsw_encoding_get_type, "GswEncoding", encoding_values)
GSW_DEFINE_ENUM_TYPE(gsw_direction_get_type, "GswDirection", direction_values)
GSW_DEFINE_ENUM_TYPE(gsw_markup_get_type, "GswMarkup", markup_values)
GSW_DEFINE_ENUM_TYPE(gsw_position_get_type, "GswPosition", position_values)
