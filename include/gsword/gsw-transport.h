/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-transport.h: remote transfer abstraction
 *
 * This file is part of gsword.
 */

#ifndef __GSW_TRANSPORT_H__
#define __GSW_TRANSPORT_H__ 1

#include <gio/gio.h>
#include <gsword/gsw-enums.h>
#include <gsword/gsw-status-reporter.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

/**
 * GswDirEntry:
 *
 * A name, size, and directory flag returned by gsw_transport_list_dir().
 * Reference-counted with gsw_dir_entry_ref() and gsw_dir_entry_unref().
 */
typedef struct _GswDirEntry GswDirEntry;

#define GSW_TYPE_DIR_ENTRY (gsw_dir_entry_get_type())

/**
 * gsw_dir_entry_get_type:
 *
 * Gets the #GType for #GswDirEntry.
 *
 * Returns: the #GType for #GswDirEntry
 */
GType        gsw_dir_entry_get_type(void);
/**
 * gsw_dir_entry_new:
 * @name: entry file name
 * @size: entry size in bytes
 * @is_directory: whether the entry is a directory
 *
 * Creates a directory entry.
 *
 * Returns: (transfer full): a new #GswDirEntry
 */
GswDirEntry *gsw_dir_entry_new(const gchar *name, guint64 size, gboolean is_directory);
/**
 * gsw_dir_entry_ref:
 * @self: a #GswDirEntry
 *
 * Increases the reference count of a directory entry.
 *
 * Returns: (transfer full): @self with an increased reference count
 */
GswDirEntry *gsw_dir_entry_ref(GswDirEntry *self);
/**
 * gsw_dir_entry_unref:
 * @self: a #GswDirEntry
 *
 * Decreases the reference count of a directory entry.
 */
void         gsw_dir_entry_unref(GswDirEntry *self);
/**
 * gsw_dir_entry_get_name:
 * @self: a #GswDirEntry
 *
 * Gets the entry name.
 *
 * Returns: (transfer none): the entry name
 */
const gchar *gsw_dir_entry_get_name(GswDirEntry *self);
/**
 * gsw_dir_entry_get_size:
 * @self: a #GswDirEntry
 *
 * Gets the entry size in bytes.
 *
 * Returns: the entry size in bytes
 */
guint64      gsw_dir_entry_get_size(GswDirEntry *self);
/**
 * gsw_dir_entry_is_directory:
 * @self: a #GswDirEntry
 *
 * Checks whether the entry is a directory.
 *
 * Returns: %TRUE if the condition holds, %FALSE otherwise
 */
gboolean     gsw_dir_entry_is_directory(GswDirEntry *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswDirEntry, gsw_dir_entry_unref)

/**
 * GswTransport:
 *
 * A protocol-independent remote transfer abstraction. The libcurl-backed
 * implementation supports HTTP, HTTPS, FTP, and SFTP, including progress
 * reporting through #GswStatusReporter.
 */
#define GSW_TYPE_TRANSPORT (gsw_transport_get_type())
G_DECLARE_DERIVABLE_TYPE(GswTransport, gsw_transport, GSW, TRANSPORT, GObject)

/**
 * GswTransportClass:
 * @parent_class: the parent class
 * @get: downloads @url into @out
 * @list_dir: lists the contents of the remote directory at @url
 *
 * Class structure for #GswTransport. Subclasses implement the actual
 * protocol operations by overriding @get and @list_dir.
 */
struct _GswTransportClass {
    GObjectClass parent_class;

    gboolean   (*get)(GswTransport *self, const gchar *url, GOutputStream *out,
                      GCancellable *cancellable, GError **error);
    GPtrArray *(*list_dir)(GswTransport *self, const gchar *url,
                           GCancellable *cancellable, GError **error);
};

/**
 * gsw_transport_new_curl:
 * @type: a #GswSourceType
 * @host: remote host name
 * @error: return location for a #GError, or %NULL
 *
 * Creates a libcurl-backed transport.
 *
 * Returns: (transfer full): a new #GswTransport, or %NULL on error
 */
GswTransport *gsw_transport_new_curl(GswSourceType type,
                                     const gchar *host,
                                     GError **error);

/**
 * gsw_transport_get_source_type:
 * @self: a #GswTransport
 *
 * Gets the transport protocol type.
 *
 * Returns: the transport protocol type
 */
GswSourceType     gsw_transport_get_source_type(GswTransport *self);
/**
 * gsw_transport_get_host:
 * @self: a #GswTransport
 *
 * Gets the remote host.
 *
 * Returns: (transfer none): the remote host
 */
const gchar      *gsw_transport_get_host(GswTransport *self);
/**
 * gsw_transport_set_credentials:
 * @self: a #GswTransport
 * @user: user name, or %NULL
 * @password: password, or %NULL
 *
 * Sets the user name and password.
 */
void              gsw_transport_set_credentials(GswTransport *self,
                                                const gchar *user,
                                                const gchar *password);
/**
 * gsw_transport_set_passive:
 * @self: a #GswTransport
 * @passive: whether to use passive FTP
 *
 * Enables or disables passive FTP.
 */
void              gsw_transport_set_passive(GswTransport *self, gboolean passive);
/**
 * gsw_transport_set_timeout:
 * @self: a #GswTransport
 * @timeout_msec: timeout in milliseconds
 *
 * Sets the connection timeout and the interval used to detect stalled
 * transfers. It is not a limit on the total duration of a download.
 */
void              gsw_transport_set_timeout(GswTransport *self, guint timeout_msec);
/**
 * gsw_transport_set_verify_peer:
 * @self: a #GswTransport
 * @verify: whether to verify TLS peers
 *
 * Enables or disables TLS peer verification.
 */
void              gsw_transport_set_verify_peer(GswTransport *self, gboolean verify);
/**
 * gsw_transport_set_proxy:
 * @self: a #GswTransport
 * @url: (nullable): proxy URL, or %NULL to use libcurl's default
 * @user: (nullable): proxy user name
 * @password: (nullable): proxy password
 *
 * Configures an HTTP/HTTPS proxy for the transport.
 */
void              gsw_transport_set_proxy(GswTransport *self,
                                          const gchar *url,
                                          const gchar *user,
                                          const gchar *password);
/**
 * gsw_transport_set_status_reporter:
 * @self: a #GswTransport
 * @reporter: the #GswStatusReporter, or %NULL to disable reporting
 *
 * Sets the progress reporter.
 */
void              gsw_transport_set_status_reporter(GswTransport *self,
                                                    GswStatusReporter *reporter);

/**
 * gsw_transport_get:
 * @self: a #GswTransport
 * @url: URL to download or list
 * @out: output stream that receives the response body
 * @cancellable: a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Downloads a URL into an output stream.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean   gsw_transport_get(GswTransport *self, const gchar *url,
                             GOutputStream *out, GCancellable *cancellable,
                             GError **error);
/**
 * gsw_transport_get_bytes:
 * @self: a #GswTransport
 * @url: URL to download or list
 * @cancellable: a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Downloads a URL into a #GBytes.
 *
 * Returns: (transfer full): a #GBytes containing the response body
 */
GBytes    *gsw_transport_get_bytes(GswTransport *self, const gchar *url,
                                   GCancellable *cancellable, GError **error);
/**
 * gsw_transport_list_dir:
 * @self: a #GswTransport
 * @url: URL to download or list
 * @cancellable: a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Lists a remote directory.
 *
 * Returns: (transfer container) (element-type GswDirEntry): a newly allocated array of #GswDirEntry
 */
GPtrArray *gsw_transport_list_dir(GswTransport *self, const gchar *url,
                                  GCancellable *cancellable, GError **error);

G_END_DECLS

#endif /* __GSW_TRANSPORT_H__ */
