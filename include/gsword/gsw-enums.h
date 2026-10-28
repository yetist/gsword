/*
 * gsw-enums.h: public enumerations for gsword
 *
 * This file is part of gsword.
 */

#ifndef __GSW_ENUMS_H__
#define __GSW_ENUMS_H__ 1

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
    GSW_MODULE_TYPE_UNKNOWN,
    GSW_MODULE_TYPE_BIBLES,
    GSW_MODULE_TYPE_COMMENTARIES,
    GSW_MODULE_TYPE_LEXDICTS,
    GSW_MODULE_TYPE_GENBOOKS,
    GSW_MODULE_TYPE_DAILY_DEVOS,
    GSW_MODULE_TYPE_MAPS,
    GSW_MODULE_TYPE_OTHER
} GswModuleType;

typedef enum {
    GSW_DRIVER_UNKNOWN,
    GSW_DRIVER_RAW_TEXT,
    GSW_DRIVER_RAW_TEXT4,
    GSW_DRIVER_Z_TEXT,
    GSW_DRIVER_Z_TEXT4,
    GSW_DRIVER_RAW_COM,
    GSW_DRIVER_RAW_COM4,
    GSW_DRIVER_Z_COM,
    GSW_DRIVER_Z_COM4,
    GSW_DRIVER_RAW_LD,
    GSW_DRIVER_RAW_LD4,
    GSW_DRIVER_Z_LD,
    GSW_DRIVER_RAW_GENBOOK,
    GSW_DRIVER_RAW_FILES,
    GSW_DRIVER_HREF_COM
} GswDriver;

typedef enum {
    GSW_SOURCE_FTP,
    GSW_SOURCE_HTTP,
    GSW_SOURCE_HTTPS,
    GSW_SOURCE_SFTP
} GswSourceType;

typedef enum {
    GSW_MODULE_STATUS_NEW          = 1 << 0,
    GSW_MODULE_STATUS_UPDATED      = 1 << 1,
    GSW_MODULE_STATUS_OLDER        = 1 << 2,
    GSW_MODULE_STATUS_SAME_VERSION = 1 << 3,
    GSW_MODULE_STATUS_CIPHERED     = 1 << 4,
    GSW_MODULE_STATUS_KEY_PRESENT  = 1 << 5
} GswModuleStatusFlags;

typedef enum {
    GSW_ENCODING_UNKNOWN,
    GSW_ENCODING_LATIN1,
    GSW_ENCODING_UTF8,
    GSW_ENCODING_SCSU,
    GSW_ENCODING_UTF16
} GswEncoding;

typedef enum {
    GSW_DIRECTION_LTR,
    GSW_DIRECTION_RTL,
    GSW_DIRECTION_BIDI
} GswDirection;

typedef enum {
    GSW_MARKUP_UNKNOWN,
    GSW_MARKUP_PLAIN,
    GSW_MARKUP_THML,
    GSW_MARKUP_GBF,
    GSW_MARKUP_OSIS,
    GSW_MARKUP_TEI
} GswMarkup;

typedef enum {
    GSW_POSITION_TOP,
    GSW_POSITION_BOTTOM
} GswPosition;

typedef enum {
    GSW_SEARCH_PHRASE,
    GSW_SEARCH_MULTIWORD,
    GSW_SEARCH_REGEX
} GswSearchType;

#define GSW_TYPE_MODULE_TYPE (gsw_module_type_get_type())
#define GSW_TYPE_DRIVER (gsw_driver_get_type())
#define GSW_TYPE_SOURCE_TYPE (gsw_source_type_get_type())
#define GSW_TYPE_ENCODING (gsw_encoding_get_type())
#define GSW_TYPE_DIRECTION (gsw_direction_get_type())
#define GSW_TYPE_MARKUP (gsw_markup_get_type())
#define GSW_TYPE_POSITION (gsw_position_get_type())
#define GSW_TYPE_SEARCH_TYPE (gsw_search_type_get_type())

GType gsw_module_type_get_type(void);
GType gsw_driver_get_type(void);
GType gsw_source_type_get_type(void);
GType gsw_encoding_get_type(void);
GType gsw_direction_get_type(void);
GType gsw_markup_get_type(void);
GType gsw_position_get_type(void);
GType gsw_search_type_get_type(void);

const gchar *gsw_source_type_to_string(GswSourceType type);
const gchar *gsw_source_type_to_scheme(GswSourceType type);

G_END_DECLS

#endif /* __GSW_ENUMS_H__ */
