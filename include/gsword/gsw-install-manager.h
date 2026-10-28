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

#define GSW_TYPE_INSTALL_MANAGER (gsw_install_manager_get_type())
G_DECLARE_FINAL_TYPE(GswInstallManager, gsw_install_manager, GSW,
                     INSTALL_MANAGER, GObject)

GswInstallManager *gsw_install_manager_new(const gchar *private_path,
                                           GError **error);

void              gsw_install_manager_set_status_reporter(GswInstallManager *self,
                                                          GswStatusReporter *reporter);
GswStatusReporter *gsw_install_manager_get_status_reporter(GswInstallManager *self);

void              gsw_install_manager_set_cancellable(GswInstallManager *self,
                                                      GCancellable *cancellable);
GCancellable     *gsw_install_manager_get_cancellable(GswInstallManager *self);

void              gsw_install_manager_set_user_disclaimer_confirmed(GswInstallManager *self,
                                                                    gboolean confirmed);
gboolean          gsw_install_manager_is_user_disclaimer_confirmed(GswInstallManager *self);

void              gsw_install_manager_set_passive_ftp(GswInstallManager *self,
                                                      gboolean passive);
gboolean          gsw_install_manager_get_passive_ftp(GswInstallManager *self);
void              gsw_install_manager_set_timeout(GswInstallManager *self,
                                                  guint timeout_msec);
guint             gsw_install_manager_get_timeout(GswInstallManager *self);
void              gsw_install_manager_set_verify_peer(GswInstallManager *self,
                                                      gboolean verify);
gboolean          gsw_install_manager_get_verify_peer(GswInstallManager *self);

const gchar      *gsw_install_manager_get_private_path(GswInstallManager *self);

gboolean          gsw_install_manager_load_config(GswInstallManager *self,
                                                  GError **error);
gboolean          gsw_install_manager_save_config(GswInstallManager *self,
                                                  GError **error);

GPtrArray        *gsw_install_manager_get_sources(GswInstallManager *self);
GswInstallSource *gsw_install_manager_lookup_source(GswInstallManager *self,
                                                    const gchar *caption);
GswInstallSource *gsw_install_manager_lookup_source_by_uid(GswInstallManager *self,
                                                           const gchar *uid);
gboolean          gsw_install_manager_add_source(GswInstallManager *self,
                                                 GswInstallSource *source);
gboolean          gsw_install_manager_remove_source(GswInstallManager *self,
                                                    const gchar *caption);

gboolean gsw_install_manager_refresh_remote_source_configuration(
             GswInstallManager *self, GError **error);
gboolean gsw_install_manager_refresh_remote_source(
             GswInstallManager *self, GswInstallSource *source, GError **error);

GPtrArray     *gsw_install_manager_get_remote_module_infos(
                  GswInstallManager *self, GswInstallSource *source,
                  GError **error);
GswModuleInfo *gsw_install_manager_lookup_remote_module_info(
                  GswInstallManager *self, GswInstallSource *source,
                  const gchar *name, GError **error);

gboolean gsw_install_manager_install_module(GswInstallManager *self,
                                            GswManager *destination,
                                            GswInstallSource *source,
                                            const gchar *module_name,
                                            GError **error);
gboolean gsw_install_manager_remove_module(GswInstallManager *self,
                                           GswManager *destination,
                                           const gchar *module_name,
                                           GError **error);

guint gsw_install_manager_get_module_status(GswManager *local,
                                            GswModuleInfo *remote);

G_END_DECLS

#endif /* __GSW_INSTALL_MANAGER_H__ */
