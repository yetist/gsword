/*
 * gsw-install-source.h: a remote SWORD installation source
 *
 * This file is part of gsword.
 */

#ifndef __GSW_INSTALL_SOURCE_H__
#define __GSW_INSTALL_SOURCE_H__ 1

#include <glib-object.h>
#include <gsword/gsw-enums.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

#define GSW_TYPE_INSTALL_SOURCE (gsw_install_source_get_type())
G_DECLARE_FINAL_TYPE(GswInstallSource, gsw_install_source, GSW,
                     INSTALL_SOURCE, GObject)

GswInstallSource *gsw_install_source_new(GswSourceType type,
                                         const gchar *caption,
                                         const gchar *host,
                                         const gchar *directory,
                                         const gchar *user,
                                         const gchar *password,
                                         const gchar *uid);
GswInstallSource *gsw_install_source_new_from_config(GswSourceType type,
                                                     const gchar *value,
                                                     GError **error);

gchar            *gsw_install_source_to_config(GswInstallSource *self);

GswSourceType  gsw_install_source_get_source_type(GswInstallSource *self);
const gchar   *gsw_install_source_get_caption(GswInstallSource *self);
const gchar   *gsw_install_source_get_host(GswInstallSource *self);
const gchar   *gsw_install_source_get_directory(GswInstallSource *self);
const gchar   *gsw_install_source_get_user(GswInstallSource *self);
const gchar   *gsw_install_source_get_password(GswInstallSource *self);
const gchar   *gsw_install_source_get_uid(GswInstallSource *self);

G_END_DECLS

#endif /* __GSW_INSTALL_SOURCE_H__ */
