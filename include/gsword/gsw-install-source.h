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

/**
 * GswInstallSource:
 *
 * A local or remote SWORD installation source: its protocol, caption, host or
 * local path, module directory, optional credentials, and local
 * shadow-directory UID.
 */
#define GSW_TYPE_INSTALL_SOURCE (gsw_install_source_get_type())
G_DECLARE_FINAL_TYPE(GswInstallSource, gsw_install_source, GSW,
                     INSTALL_SOURCE, GObject)

/**
 * gsw_install_source_new:
 * @type: a #GswSourceType
 * @caption: source caption
 * @host: remote host name
 * @directory: remote module directory
 * @user: user name, or %NULL
 * @password: password, or %NULL
 * @uid: source UID
 *
 * Creates an installation source from its connection parameters.
 *
 * Returns: (transfer full): a new #GswInstallSource
 */
GswInstallSource *gsw_install_source_new(GswSourceType type,
                                         const gchar *caption,
                                         const gchar *host,
                                         const gchar *directory,
                                         const gchar *user,
                                         const gchar *password,
                                         const gchar *uid);
/**
 * gsw_install_source_new_from_config:
 * @type: a #GswSourceType
 * @value: value to store
 * @error: return location for a #GError, or %NULL
 *
 * Parses an installation source from its configuration string.
 *
 * Returns: (transfer full): a new #GswInstallSource, or %NULL on error
 */
GswInstallSource *gsw_install_source_new_from_config(GswSourceType type,
                                                     const gchar *value,
                                                     GError **error);

/**
 * gsw_install_source_to_config:
 * @self: a #GswInstallSource
 *
 * Serializes the source to its configuration string.
 *
 * Returns: (transfer full): the serialized configuration string
 */
gchar            *gsw_install_source_to_config(GswInstallSource *self);

/**
 * gsw_install_source_get_source_type:
 * @self: a #GswInstallSource
 *
 * Gets the protocol type of the source.
 *
 * Returns: the source protocol type
 */
GswSourceType  gsw_install_source_get_source_type(GswInstallSource *self);
/**
 * gsw_install_source_get_caption:
 * @self: a #GswInstallSource
 *
 * Gets the user-visible caption.
 *
 * Returns: (transfer none): the source caption
 */
const gchar   *gsw_install_source_get_caption(GswInstallSource *self);
/**
 * gsw_install_source_get_host:
 * @self: a #GswInstallSource
 *
 * Gets the host name.
 *
 * Returns: (transfer none): the host name
 */
const gchar   *gsw_install_source_get_host(GswInstallSource *self);
/**
 * gsw_install_source_get_directory:
 * @self: a #GswInstallSource
 *
 * Gets the remote module directory.
 *
 * Returns: (transfer none): the remote module directory
 */
const gchar   *gsw_install_source_get_directory(GswInstallSource *self);
/**
 * gsw_install_source_get_user:
 * @self: a #GswInstallSource
 *
 * Gets the user name.
 *
 * Returns: (transfer none): the user name
 */
const gchar   *gsw_install_source_get_user(GswInstallSource *self);
/**
 * gsw_install_source_get_password:
 * @self: a #GswInstallSource
 *
 * Gets the password.
 *
 * Returns: (transfer none): the password
 */
const gchar   *gsw_install_source_get_password(GswInstallSource *self);
/**
 * gsw_install_source_get_uid:
 * @self: a #GswInstallSource
 *
 * Gets the local shadow-directory UID.
 *
 * Returns: (transfer none): the local shadow-directory UID
 */
const gchar   *gsw_install_source_get_uid(GswInstallSource *self);

G_END_DECLS

#endif /* __GSW_INSTALL_SOURCE_H__ */
