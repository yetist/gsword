/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-error.h: error domain for gsword
 *
 * This file is part of gsword.
 */

#ifndef __GSW_ERROR_H__
#define __GSW_ERROR_H__ 1

#include <glib.h>

G_BEGIN_DECLS

/**
 * GswError:
 * @GSW_ERROR_CONFIG: configuration syntax or value error
 * @GSW_ERROR_IO: local file I/O error
 * @GSW_ERROR_NETWORK: DNS, connection, TLS, or transfer error
 * @GSW_ERROR_PROTOCOL: unexpected HTTP/FTP response
 * @GSW_ERROR_NOT_FOUND: module, entry, file, or source not found
 * @GSW_ERROR_INVALID_DATA: corrupt or inconsistent module data
 * @GSW_ERROR_UNSUPPORTED: driver, encoding, or feature not supported
 * @GSW_ERROR_CANCELLED: operation cancelled through #GCancellable
 * @GSW_ERROR_CIPHERED: module is encrypted and cannot be read
 * @GSW_ERROR_PERMISSION: permission denied
 * @GSW_ERROR_INSTALL: install/remove operation failed
 * @GSW_ERROR_OUT_OF_BOUNDS: key outside module bounds
 */
typedef enum {
    GSW_ERROR_CONFIG,
    GSW_ERROR_IO,
    GSW_ERROR_NETWORK,
    GSW_ERROR_PROTOCOL,
    GSW_ERROR_NOT_FOUND,
    GSW_ERROR_INVALID_DATA,
    GSW_ERROR_UNSUPPORTED,
    GSW_ERROR_CANCELLED,
    GSW_ERROR_CIPHERED,
    GSW_ERROR_PERMISSION,
    GSW_ERROR_INSTALL,
    GSW_ERROR_OUT_OF_BOUNDS
} GswError;

#define GSW_ERROR (gsw_error_quark())

/**
 * gsw_error_quark:
 *
 * Gets the #GQuark identifying the gsword error domain.
 *
 * Returns: the #GQuark identifying the gsword error domain
 */
GQuark gsw_error_quark(void);

G_END_DECLS

#endif /* __GSW_ERROR_H__ */
