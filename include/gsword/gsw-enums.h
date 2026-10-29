/*
 * gsw-enums.h: public enumerations for gsword
 *
 * This file is part of gsword.
 */

#ifndef __GSW_ENUMS_H__
#define __GSW_ENUMS_H__ 1

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * GswModuleType:
 * @GSW_MODULE_TYPE_UNKNOWN: the module type could not be determined
 * @GSW_MODULE_TYPE_BIBLES: a Bible text module
 * @GSW_MODULE_TYPE_COMMENTARIES: a commentary module
 * @GSW_MODULE_TYPE_LEXDICTS: a lexicon or dictionary module
 * @GSW_MODULE_TYPE_GENBOOKS: a general book module
 * @GSW_MODULE_TYPE_DAILY_DEVOS: a daily devotional module
 * @GSW_MODULE_TYPE_MAPS: a map module
 * @GSW_MODULE_TYPE_OTHER: any other module type
 *
 * The broad category of a SWORD module.
 */
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

/**
 * GswDriver:
 * @GSW_DRIVER_UNKNOWN: the driver is unknown or unsupported
 * @GSW_DRIVER_RAW_TEXT: uncompressed verse-keyed text
 * @GSW_DRIVER_RAW_TEXT4: uncompressed verse-keyed text with four-byte offsets
 * @GSW_DRIVER_Z_TEXT: zlib-compressed verse-keyed text
 * @GSW_DRIVER_Z_TEXT4: zlib-compressed verse-keyed text with four-byte offsets
 * @GSW_DRIVER_RAW_COM: uncompressed commentary text
 * @GSW_DRIVER_RAW_COM4: uncompressed commentary text with four-byte offsets
 * @GSW_DRIVER_Z_COM: zlib-compressed commentary text
 * @GSW_DRIVER_Z_COM4: zlib-compressed commentary text with four-byte offsets
 * @GSW_DRIVER_RAW_LD: uncompressed lexicon or dictionary data
 * @GSW_DRIVER_RAW_LD4: uncompressed lexicon data with four-byte offsets
 * @GSW_DRIVER_Z_LD: zlib-compressed lexicon or dictionary data
 * @GSW_DRIVER_RAW_GENBOOK: general book storage
 * @GSW_DRIVER_RAW_FILES: raw file-backed storage
 * @GSW_DRIVER_HREF_COM: commentary with hyperlinked references
 *
 * The storage driver used by a SWORD module.
 */
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

/**
 * GswSourceType:
 * @GSW_SOURCE_FTP: FTP
 * @GSW_SOURCE_HTTP: unencrypted HTTP
 * @GSW_SOURCE_HTTPS: HTTP over TLS
 * @GSW_SOURCE_SFTP: SSH File Transfer Protocol
 *
 * The protocol used by an installation source.
 */
typedef enum {
    GSW_SOURCE_FTP,
    GSW_SOURCE_HTTP,
    GSW_SOURCE_HTTPS,
    GSW_SOURCE_SFTP
} GswSourceType;

/**
 * GswModuleStatusFlags:
 * @GSW_MODULE_STATUS_NEW: the module is not installed locally
 * @GSW_MODULE_STATUS_UPDATED: a newer version is available remotely
 * @GSW_MODULE_STATUS_OLDER: the locally installed version is newer
 * @GSW_MODULE_STATUS_SAME_VERSION: local and remote versions are identical
 * @GSW_MODULE_STATUS_CIPHERED: the module is encrypted
 * @GSW_MODULE_STATUS_KEY_PRESENT: a decryption key is available
 *
 * Bit flags returned by gsw_install_manager_get_module_status().
 */
typedef enum {
    GSW_MODULE_STATUS_NEW          = 1 << 0,
    GSW_MODULE_STATUS_UPDATED      = 1 << 1,
    GSW_MODULE_STATUS_OLDER        = 1 << 2,
    GSW_MODULE_STATUS_SAME_VERSION = 1 << 3,
    GSW_MODULE_STATUS_CIPHERED     = 1 << 4,
    GSW_MODULE_STATUS_KEY_PRESENT  = 1 << 5
} GswModuleStatusFlags;

/**
 * GswEncoding:
 * @GSW_ENCODING_UNKNOWN: the encoding is unknown
 * @GSW_ENCODING_LATIN1: ISO-8859-1
 * @GSW_ENCODING_UTF8: UTF-8
 * @GSW_ENCODING_SCSU: Standard Compression Scheme for Unicode
 * @GSW_ENCODING_UTF16: UTF-16
 *
 * Character encoding used by module text.
 */
typedef enum {
    GSW_ENCODING_UNKNOWN,
    GSW_ENCODING_LATIN1,
    GSW_ENCODING_UTF8,
    GSW_ENCODING_SCSU,
    GSW_ENCODING_UTF16
} GswEncoding;

/**
 * GswDirection:
 * @GSW_DIRECTION_LTR: left-to-right text
 * @GSW_DIRECTION_RTL: right-to-left text
 * @GSW_DIRECTION_BIDI: bidirectional text
 *
 * Writing direction of module text.
 */
typedef enum {
    GSW_DIRECTION_LTR,
    GSW_DIRECTION_RTL,
    GSW_DIRECTION_BIDI
} GswDirection;

/**
 * GswMarkup:
 * @GSW_MARKUP_UNKNOWN: the markup format is unknown
 * @GSW_MARKUP_PLAIN: plain text without markup
 * @GSW_MARKUP_THML: Theologically Markup Language
 * @GSW_MARKUP_GBF: General Bible Format
 * @GSW_MARKUP_OSIS: Open Scripture Information Standard
 * @GSW_MARKUP_TEI: Text Encoding Initiative markup
 *
 * Markup format used by module entries.
 */
typedef enum {
    GSW_MARKUP_UNKNOWN,
    GSW_MARKUP_PLAIN,
    GSW_MARKUP_THML,
    GSW_MARKUP_GBF,
    GSW_MARKUP_OSIS,
    GSW_MARKUP_TEI
} GswMarkup;

/**
 * GswPosition:
 * @GSW_POSITION_TOP: the first entry
 * @GSW_POSITION_BOTTOM: the last entry
 *
 * A coarse position within a module.
 */
typedef enum {
    GSW_POSITION_TOP,
    GSW_POSITION_BOTTOM
} GswPosition;

/**
 * GswSearchType:
 * @GSW_SEARCH_PHRASE: match the entire query as a phrase
 * @GSW_SEARCH_MULTIWORD: match entries containing all query words
 * @GSW_SEARCH_REGEX: treat the query as a regular expression
 *
 * Search algorithm used by gsw_module_search().
 */
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

/**
 * gsw_module_type_get_type:
 *
 * Gets the #GType for #GswModuleType.
 *
 * Returns: the #GType for #GswModuleType
 */
GType gsw_module_type_get_type(void);
/**
 * gsw_driver_get_type:
 *
 * Gets the #GType for #GswDriver.
 *
 * Returns: the #GType for #GswDriver
 */
GType gsw_driver_get_type(void);
/**
 * gsw_source_type_get_type:
 *
 * Gets the #GType for #GswSourceType.
 *
 * Returns: the #GType for #GswSourceType
 */
GType gsw_source_type_get_type(void);
/**
 * gsw_encoding_get_type:
 *
 * Gets the #GType for #GswEncoding.
 *
 * Returns: the #GType for #GswEncoding
 */
GType gsw_encoding_get_type(void);
/**
 * gsw_direction_get_type:
 *
 * Gets the #GType for #GswDirection.
 *
 * Returns: the #GType for #GswDirection
 */
GType gsw_direction_get_type(void);
/**
 * gsw_markup_get_type:
 *
 * Gets the #GType for #GswMarkup.
 *
 * Returns: the #GType for #GswMarkup
 */
GType gsw_markup_get_type(void);
/**
 * gsw_position_get_type:
 *
 * Gets the #GType for #GswPosition.
 *
 * Returns: the #GType for #GswPosition
 */
GType gsw_position_get_type(void);
/**
 * gsw_search_type_get_type:
 *
 * Gets the #GType for #GswSearchType.
 *
 * Returns: the #GType for #GswSearchType
 */
GType gsw_search_type_get_type(void);

/**
 * gsw_source_type_to_string:
 * @type: a #GswSourceType
 *
 * Gets the human-readable name of a source protocol.
 *
 * Returns: (transfer none): the display name of @type
 */
const gchar *gsw_source_type_to_string(GswSourceType type);
/**
 * gsw_source_type_to_scheme:
 * @type: a #GswSourceType
 *
 * Gets the URL scheme used by a source protocol.
 *
 * Returns: (transfer none): the URL scheme for @type
 */
const gchar *gsw_source_type_to_scheme(GswSourceType type);

G_END_DECLS

#endif /* __GSW_ENUMS_H__ */
