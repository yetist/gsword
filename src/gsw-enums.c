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
    { GSW_MODULE_TYPE_BIBLES,       "GSW_MODULE_TYPE_BIBLES",       "bibles" },
    { GSW_MODULE_TYPE_COMMENTARIES, "GSW_MODULE_TYPE_COMMENTARIES", "commentaries" },
    { GSW_MODULE_TYPE_LEXDICTS,     "GSW_MODULE_TYPE_LEXDICTS",     "lexdicts" },
    { GSW_MODULE_TYPE_GENBOOKS,     "GSW_MODULE_TYPE_GENBOOKS",     "genbooks" },
    { GSW_MODULE_TYPE_DAILY_DEVOS,  "GSW_MODULE_TYPE_DAILY_DEVOS",  "daily_devos" },
    { GSW_MODULE_TYPE_MAPS,         "GSW_MODULE_TYPE_MAPS",         "maps" },
    { GSW_MODULE_TYPE_OTHER,        "GSW_MODULE_TYPE_OTHER",        "other" },
    { 0, NULL, NULL }
};

static const GEnumValue driver_values[] = {
    { GSW_DRIVER_UNKNOWN,      "GSW_DRIVER_UNKNOWN",      "Unknown" },
    { GSW_DRIVER_RAW_TEXT,     "GSW_DRIVER_RAW_TEXT",     "raw_text" },
    { GSW_DRIVER_RAW_TEXT4,    "GSW_DRIVER_RAW_TEXT4",    "raw_text4" },
    { GSW_DRIVER_Z_TEXT,       "GSW_DRIVER_Z_TEXT",       "z_text" },
    { GSW_DRIVER_Z_TEXT4,      "GSW_DRIVER_Z_TEXT4",      "z_text4" },
    { GSW_DRIVER_RAW_COM,      "GSW_DRIVER_RAW_COM",      "raw_com" },
    { GSW_DRIVER_RAW_COM4,     "GSW_DRIVER_RAW_COM4",     "raw_com4" },
    { GSW_DRIVER_Z_COM,        "GSW_DRIVER_Z_COM",        "z_com" },
    { GSW_DRIVER_Z_COM4,       "GSW_DRIVER_Z_COM4",       "z_com4" },
    { GSW_DRIVER_RAW_LD,       "GSW_DRIVER_RAW_LD",       "raw_ld" },
    { GSW_DRIVER_RAW_LD4,      "GSW_DRIVER_RAW_LD4",      "raw_ld4" },
    { GSW_DRIVER_Z_LD,         "GSW_DRIVER_Z_LD",         "z_ld" },
    { GSW_DRIVER_RAW_GENBOOK,  "GSW_DRIVER_RAW_GENBOOK",  "raw_genbook" },
    { GSW_DRIVER_RAW_FILES,    "GSW_DRIVER_RAW_FILES",    "raw_files" },
    { GSW_DRIVER_HREF_COM,     "GSW_DRIVER_HREF_COM",     "href_com" },
    { 0, NULL, NULL }
};

static const GEnumValue source_type_values[] = {
    { GSW_SOURCE_FTP,   "GSW_SOURCE_FTP",   "ftp" },
    { GSW_SOURCE_HTTP,  "GSW_SOURCE_HTTP",  "http" },
    { GSW_SOURCE_HTTPS, "GSW_SOURCE_HTTPS", "https" },
    { GSW_SOURCE_SFTP,  "GSW_SOURCE_SFTP",  "sftp" },
    { 0, NULL, NULL }
};

static const GEnumValue encoding_values[] = {
    { GSW_ENCODING_UNKNOWN, "GSW_ENCODING_UNKNOWN", "Unknown" },
    { GSW_ENCODING_LATIN1,  "GSW_ENCODING_LATIN1",  "latin1" },
    { GSW_ENCODING_UTF8,    "GSW_ENCODING_UTF8",    "utf8" },
    { GSW_ENCODING_SCSU,    "GSW_ENCODING_SCSU",    "scsu" },
    { GSW_ENCODING_UTF16,   "GSW_ENCODING_UTF16",   "utf16" },
    { 0, NULL, NULL }
};

static const GEnumValue direction_values[] = {
    { GSW_DIRECTION_LTR,  "GSW_DIRECTION_LTR",  "ltr" },
    { GSW_DIRECTION_RTL,  "GSW_DIRECTION_RTL",  "rtl" },
    { GSW_DIRECTION_BIDI, "GSW_DIRECTION_BIDI", "bidi" },
    { 0, NULL, NULL }
};

static const GEnumValue markup_values[] = {
    { GSW_MARKUP_UNKNOWN, "GSW_MARKUP_UNKNOWN", "Unknown" },
    { GSW_MARKUP_PLAIN,   "GSW_MARKUP_PLAIN",   "plain" },
    { GSW_MARKUP_THML,    "GSW_MARKUP_THML",    "thml" },
    { GSW_MARKUP_GBF,     "GSW_MARKUP_GBF",     "gbf" },
    { GSW_MARKUP_OSIS,    "GSW_MARKUP_OSIS",    "osis" },
    { GSW_MARKUP_TEI,     "GSW_MARKUP_TEI",     "tei" },
    { 0, NULL, NULL }
};

static const GEnumValue position_values[] = {
    { GSW_POSITION_TOP,    "GSW_POSITION_TOP",    "top" },
    { GSW_POSITION_BOTTOM, "GSW_POSITION_BOTTOM", "bottom" },
    { 0, NULL, NULL }
};

GSW_DEFINE_ENUM_TYPE(gsw_module_type_get_type, "GswModuleType", module_type_values)
GSW_DEFINE_ENUM_TYPE(gsw_driver_get_type, "GswDriver", driver_values)
GSW_DEFINE_ENUM_TYPE(gsw_source_type_get_type, "GswSourceType", source_type_values)
GSW_DEFINE_ENUM_TYPE(gsw_encoding_get_type, "GswEncoding", encoding_values)
GSW_DEFINE_ENUM_TYPE(gsw_direction_get_type, "GswDirection", direction_values)
GSW_DEFINE_ENUM_TYPE(gsw_markup_get_type, "GswMarkup", markup_values)
GSW_DEFINE_ENUM_TYPE(gsw_position_get_type, "GswPosition", position_values)

const gchar *
gsw_source_type_to_string(GswSourceType type)
{
    switch (type) {
    case GSW_SOURCE_HTTP:  return "HTTP";
    case GSW_SOURCE_HTTPS: return "HTTPS";
    case GSW_SOURCE_SFTP:  return "SFTP";
    case GSW_SOURCE_FTP:
    default:               return "FTP";
    }
}

const gchar *
gsw_source_type_to_scheme(GswSourceType type)
{
    switch (type) {
    case GSW_SOURCE_HTTP:  return "http";
    case GSW_SOURCE_HTTPS: return "https";
    case GSW_SOURCE_SFTP:  return "sftp";
    case GSW_SOURCE_FTP:
    default:               return "ftp";
    }
}
