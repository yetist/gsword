/*
 * gsw-install-manager.h: download, install, and remove modules
 *
 * This file is part of gsword.
 */

#ifndef __GSW_INSTALL_MANAGER_H__
#define __GSW_INSTALL_MANAGER_H__ 1

#include <gio/gio.h>
#include <gsword/gsw-install-source.h>
#include <gsword/gsw-manager.h>
#include <gsword/gsw-status-reporter.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

/**
 * GswInstallManager:
 *
 * Downloads, installs, updates, and removes modules from remote SWORD
 * sources, and installs modules from local DIR sources. It reads and writes
 * <filename>InstallMgr.conf</filename>, caches source metadata, and reports
 * transfer progress through a #GswStatusReporter.
 */
#define GSW_TYPE_INSTALL_MANAGER (gsw_install_manager_get_type())
G_DECLARE_FINAL_TYPE(GswInstallManager, gsw_install_manager, GSW,
                     INSTALL_MANAGER, GObject)

/**
 * gsw_install_manager_new:
 * @private_path: directory for cached metadata and installer state
 * @error: return location for a #GError, or %NULL
 *
 * Creates an install manager rooted at @private_path.
 *
 * Returns: (transfer full): a new #GswInstallManager, or %NULL on error
 */
GswInstallManager *gsw_install_manager_new(const gchar *private_path,
                                           GError **error);

/**
 * gsw_install_manager_set_status_reporter:
 * @self: a #GswInstallManager
 * @reporter: the #GswStatusReporter, or %NULL to disable reporting
 *
 * Sets the progress reporter used for downloads and installs.
 */
void              gsw_install_manager_set_status_reporter(GswInstallManager *self,
                                                          GswStatusReporter *reporter);
/**
 * gsw_install_manager_get_status_reporter:
 * @self: a #GswInstallManager
 *
 * Gets the current progress reporter.
 *
 * Returns: (transfer none): the current status reporter, or %NULL
 */
GswStatusReporter *gsw_install_manager_get_status_reporter(GswInstallManager *self);

/**
 * gsw_install_manager_set_cancellable:
 * @self: a #GswInstallManager
 * @cancellable: a #GCancellable, or %NULL
 *
 * Sets the cancellable used to interrupt transfers.
 */
void              gsw_install_manager_set_cancellable(GswInstallManager *self,
                                                      GCancellable *cancellable);
/**
 * gsw_install_manager_get_cancellable:
 * @self: a #GswInstallManager
 *
 * Gets the current cancellable.
 *
 * Returns: (transfer none): the current cancellable, or %NULL
 */
GCancellable     *gsw_install_manager_get_cancellable(GswInstallManager *self);

/**
 * gsw_install_manager_set_user_disclaimer_confirmed:
 * @self: a #GswInstallManager
 * @confirmed: whether the user confirmed the disclaimer
 *
 * Records whether the user accepted the remote-source disclaimer.
 */
void              gsw_install_manager_set_user_disclaimer_confirmed(GswInstallManager *self,
                                                                    gboolean confirmed);
/**
 * gsw_install_manager_is_user_disclaimer_confirmed:
 * @self: a #GswInstallManager
 *
 * Checks whether the user accepted the remote-source disclaimer.
 *
 * Returns: %TRUE if the condition holds, %FALSE otherwise
 */
gboolean          gsw_install_manager_is_user_disclaimer_confirmed(GswInstallManager *self);

/**
 * gsw_install_manager_set_passive_ftp:
 * @self: a #GswInstallManager
 * @passive: whether to use passive FTP
 *
 * Enables or disables passive FTP.
 */
void              gsw_install_manager_set_passive_ftp(GswInstallManager *self,
                                                      gboolean passive);
/**
 * gsw_install_manager_get_passive_ftp:
 * @self: a #GswInstallManager
 *
 * Checks whether passive FTP is enabled.
 *
 * Returns: %TRUE if passive FTP is enabled, %FALSE otherwise
 */
gboolean          gsw_install_manager_get_passive_ftp(GswInstallManager *self);
/**
 * gsw_install_manager_set_timeout:
 * @self: a #GswInstallManager
 * @timeout_msec: timeout in milliseconds
 *
 * Sets the connection timeout and the interval used to detect stalled
 * transfers. It does not limit the total duration of a download, so large
 * modules can still complete on slow connections.
 */
void              gsw_install_manager_set_timeout(GswInstallManager *self,
                                                  guint timeout_msec);
/**
 * gsw_install_manager_get_timeout:
 * @self: a #GswInstallManager
 *
 * Gets the connection timeout.
 *
 * Returns: the timeout in milliseconds
 */
guint             gsw_install_manager_get_timeout(GswInstallManager *self);
/**
 * gsw_install_manager_set_proxy:
 * @self: a #GswInstallManager
 * @url: (nullable): proxy URL
 * @user: (nullable): proxy user name
 * @password: (nullable): proxy password
 *
 * Sets the proxy used for future remote operations.
 */
void              gsw_install_manager_set_proxy(GswInstallManager *self,
                                                const gchar *url,
                                                const gchar *user,
                                                const gchar *password);

/**
 * gsw_install_manager_set_verify_peer:
 * @self: a #GswInstallManager
 * @verify: whether to verify TLS peers
 *
 * Enables or disables TLS peer verification.
 */
void              gsw_install_manager_set_verify_peer(GswInstallManager *self,
                                                      gboolean verify);
/**
 * gsw_install_manager_get_verify_peer:
 * @self: a #GswInstallManager
 *
 * Checks whether TLS peer verification is enabled.
 *
 * Returns: %TRUE if TLS peer verification is enabled, %FALSE otherwise
 */
gboolean          gsw_install_manager_get_verify_peer(GswInstallManager *self);

/**
 * gsw_install_manager_get_private_path:
 * @self: a #GswInstallManager
 *
 * Gets the private directory used for cached metadata and downloads.
 *
 * Returns: (transfer none): the private installer directory
 */
const gchar      *gsw_install_manager_get_private_path(GswInstallManager *self);

/**
 * gsw_install_manager_load_config:
 * @self: a #GswInstallManager
 * @error: return location for a #GError, or %NULL
 *
 * Reloads <filename>InstallMgr.conf</filename>.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean          gsw_install_manager_load_config(GswInstallManager *self,
                                                  GError **error);
/**
 * gsw_install_manager_save_config:
 * @self: a #GswInstallManager
 * @error: return location for a #GError, or %NULL
 *
 * Saves <filename>InstallMgr.conf</filename>.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean          gsw_install_manager_save_config(GswInstallManager *self,
                                                  GError **error);

/**
 * gsw_install_manager_get_sources:
 * @self: a #GswInstallManager
 *
 * Gets all configured installation sources.
 *
 * Returns: (transfer container) (element-type GswInstallSource): a newly allocated array of #GswInstallSource
 */
GPtrArray        *gsw_install_manager_get_sources(GswInstallManager *self);
/**
 * gsw_install_manager_lookup_source:
 * @self: a #GswInstallManager
 * @caption: source caption
 *
 * Looks up an installation source by caption.
 *
 * Returns: (transfer none): the source, or %NULL if not found
 */
GswInstallSource *gsw_install_manager_lookup_source(GswInstallManager *self,
                                                    const gchar *caption);
/**
 * gsw_install_manager_lookup_source_by_uid:
 * @self: a #GswInstallManager
 * @uid: source UID
 *
 * Looks up an installation source by UID.
 *
 * Returns: (transfer none): the source, or %NULL if not found
 */
GswInstallSource *gsw_install_manager_lookup_source_by_uid(GswInstallManager *self,
                                                           const gchar *uid);
/**
 * gsw_install_manager_add_source:
 * @self: a #GswInstallManager
 * @source: the #GswInstallSource
 *
 * Adds or replaces an installation source.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean          gsw_install_manager_add_source(GswInstallManager *self,
                                                 GswInstallSource *source);
/**
 * gsw_install_manager_remove_source:
 * @self: a #GswInstallManager
 * @caption: source caption
 *
 * Removes an installation source by caption.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean          gsw_install_manager_remove_source(GswInstallManager *self,
                                                    const gchar *caption);

/**
 * gsw_install_manager_refresh_remote_source_configuration_async:
 * @self: a #GswInstallManager
 * @cancellable: a #GCancellable, or %NULL
 * @callback: a #GAsyncReadyCallback to invoke when the operation completes
 * @user_data: user data for @callback
 *
 * Asynchronously refreshes the master remote-source list.
 */
void gsw_install_manager_refresh_remote_source_configuration_async(
         GswInstallManager *self, GCancellable *cancellable,
         GAsyncReadyCallback callback, gpointer user_data);
/**
 * gsw_install_manager_refresh_remote_source_configuration_finish:
 * @self: a #GswInstallManager
 * @result: a #GAsyncResult from the corresponding async call
 * @error: return location for a #GError, or %NULL
 *
 * Finishes an asynchronous refresh of the master remote-source list.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean gsw_install_manager_refresh_remote_source_configuration_finish(
         GswInstallManager *self, GAsyncResult *result, GError **error);

/**
 * gsw_install_manager_refresh_remote_source_configuration:
 * @self: a #GswInstallManager
 * @error: return location for a #GError, or %NULL
 *
 * Refreshes the master remote-source list from CrossWire.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean gsw_install_manager_refresh_remote_source_configuration(
             GswInstallManager *self, GError **error);
/**
 * gsw_install_manager_refresh_remote_source_async:
 * @self: a #GswInstallManager
 * @source: the #GswInstallSource
 * @cancellable: a #GCancellable, or %NULL
 * @callback: a #GAsyncReadyCallback to invoke when the operation completes
 * @user_data: user data for @callback
 *
 * Asynchronously refreshes the metadata cache for @source.
 */
void gsw_install_manager_refresh_remote_source_async(
         GswInstallManager *self, GswInstallSource *source,
         GCancellable *cancellable, GAsyncReadyCallback callback,
         gpointer user_data);
/**
 * gsw_install_manager_refresh_remote_source_finish:
 * @self: a #GswInstallManager
 * @result: a #GAsyncResult from the corresponding async call
 * @error: return location for a #GError, or %NULL
 *
 * Finishes an asynchronous remote-source refresh.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean gsw_install_manager_refresh_remote_source_finish(
         GswInstallManager *self, GAsyncResult *result, GError **error);

/**
 * gsw_install_manager_refresh_remote_source:
 * @self: a #GswInstallManager
 * @source: the #GswInstallSource
 * @error: return location for a #GError, or %NULL
 *
 * Refreshes the metadata cache for @source.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean gsw_install_manager_refresh_remote_source(
             GswInstallManager *self, GswInstallSource *source, GError **error);

/**
 * gsw_install_manager_get_remote_module_infos:
 * @self: a #GswInstallManager
 * @source: the #GswInstallSource
 * @error: return location for a #GError, or %NULL
 *
 * Gets metadata for modules available from a refreshed source.
 *
 * Returns: (transfer container) (element-type GswModuleInfo): a newly allocated array of remote #GswModuleInfo
 */
GPtrArray     *gsw_install_manager_get_remote_module_infos(
                  GswInstallManager *self, GswInstallSource *source,
                  GError **error);
/**
 * gsw_install_manager_lookup_remote_module_info:
 * @self: a #GswInstallManager
 * @source: the #GswInstallSource
 * @name: module name
 * @error: return location for a #GError, or %NULL
 *
 * Looks up remote module metadata by name.
 *
 * Returns: (transfer none): the remote metadata, or %NULL if not found
 */
GswModuleInfo *gsw_install_manager_lookup_remote_module_info(
                  GswInstallManager *self, GswInstallSource *source,
                  const gchar *name, GError **error);

/**
 * gsw_install_manager_install_module_async:
 * @self: a #GswInstallManager
 * @destination: the destination #GswManager
 * @source: the #GswInstallSource
 * @module_name: name of the module to install or remove
 * @cancellable: a #GCancellable, or %NULL
 * @callback: a #GAsyncReadyCallback to invoke when the operation completes
 * @user_data: user data for @callback
 *
 * Asynchronously installs a module from @source into @destination.
 */
void gsw_install_manager_install_module_async(GswInstallManager *self,
                                               GswManager *destination,
                                               GswInstallSource *source,
                                               const gchar *module_name,
                                               GCancellable *cancellable,
                                               GAsyncReadyCallback callback,
                                               gpointer user_data);
/**
 * gsw_install_manager_install_module_finish:
 * @self: a #GswInstallManager
 * @result: a #GAsyncResult from the corresponding async call
 * @error: return location for a #GError, or %NULL
 *
 * Finishes an asynchronous module install.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean gsw_install_manager_install_module_finish(GswInstallManager *self,
                                                   GAsyncResult *result,
                                                   GError **error);

/**
 * gsw_install_manager_install_module:
 * @self: a #GswInstallManager
 * @destination: the destination #GswManager
 * @source: the #GswInstallSource
 * @module_name: name of the module to install or remove
 * @error: return location for a #GError, or %NULL
 *
 * Installs a module from @source into @destination.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean gsw_install_manager_install_module(GswInstallManager *self,
                                            GswManager *destination,
                                            GswInstallSource *source,
                                            const gchar *module_name,
                                            GError **error);
/**
 * gsw_install_manager_install_module_from_directory:
 * @self: a #GswInstallManager
 * @destination: the destination #GswManager
 * @directory: local SWORD library directory containing the module
 * @module_name: the module name
 * @error: return location for a #GError, or %NULL
 *
 * Installs a module from a local SWORD library directory without requiring a
 * remote source refresh.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean gsw_install_manager_install_module_from_directory(
                                            GswInstallManager *self,
                                            GswManager *destination,
                                            const gchar *directory,
                                            const gchar *module_name,
                                            GError **error);
/**
 * gsw_install_manager_remove_module_async:
 * @self: a #GswInstallManager
 * @destination: the destination #GswManager
 * @module_name: name of the module to install or remove
 * @cancellable: a #GCancellable, or %NULL
 * @callback: a #GAsyncReadyCallback to invoke when the operation completes
 * @user_data: user data for @callback
 *
 * Asynchronously removes a module from @destination.
 */
void gsw_install_manager_remove_module_async(GswInstallManager *self,
                                              GswManager *destination,
                                              const gchar *module_name,
                                              GCancellable *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer user_data);
/**
 * gsw_install_manager_remove_module_finish:
 * @self: a #GswInstallManager
 * @result: a #GAsyncResult from the corresponding async call
 * @error: return location for a #GError, or %NULL
 *
 * Finishes an asynchronous module removal.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean gsw_install_manager_remove_module_finish(GswInstallManager *self,
                                                  GAsyncResult *result,
                                                  GError **error);

/**
 * gsw_install_manager_remove_module:
 * @self: a #GswInstallManager
 * @destination: the destination #GswManager
 * @module_name: name of the module to install or remove
 * @error: return location for a #GError, or %NULL
 *
 * Removes an installed module from @destination.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean gsw_install_manager_remove_module(GswInstallManager *self,
                                           GswManager *destination,
                                           const gchar *module_name,
                                           GError **error);

/**
 * gsw_install_manager_get_module_status:
 * @local: local module library
 * @remote: remote module metadata
 *
 * Compares a remote module against a local library.
 *
 * Returns: a bitwise OR of #GswModuleStatusFlags values
 */
guint gsw_install_manager_get_module_status(GswManager *local,
                                            GswModuleInfo *remote);

/**
 * gsw_install_manager_cancel:
 * @self: a #GswInstallManager
 *
 * Requests cancellation of the current operation.
 */
void gsw_install_manager_cancel(GswInstallManager *self);

G_END_DECLS

#endif /* __GSW_INSTALL_MANAGER_H__ */
