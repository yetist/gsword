/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-install-source.c: a remote SWORD installation source
 */

#include <gsword/gsw-install-source.h>
#include <gsword/gsw-error.h>
#include "gsw-utils.h"

#include <string.h>

struct _GswInstallSource {
    GObject parent_instance;
};

typedef struct {
    GswSourceType type;
    gchar *caption;
    gchar *host;
    gchar *directory;
    gchar *user;
    gchar *password;
    gchar *uid;
} GswInstallSourcePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GswInstallSource, gsw_install_source, G_TYPE_OBJECT)

static GswInstallSourcePrivate *
gsw_install_source_priv(GswInstallSource *self)
{
    return gsw_install_source_get_instance_private(self);
}

static void
gsw_install_source_finalize(GObject *object)
{
    GswInstallSource *self = GSW_INSTALL_SOURCE(object);
    GswInstallSourcePrivate *priv = gsw_install_source_priv(self);

    g_free(priv->caption);
    g_free(priv->host);
    g_free(priv->directory);
    g_free(priv->user);
    g_free(priv->password);
    g_free(priv->uid);

    G_OBJECT_CLASS(gsw_install_source_parent_class)->finalize(object);
}

static void
gsw_install_source_class_init(GswInstallSourceClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = gsw_install_source_finalize;
}

static void
gsw_install_source_init(GswInstallSource *self)
{
    GswInstallSourcePrivate *priv = gsw_install_source_priv(self);

    priv->type = GSW_SOURCE_FTP;
    priv->caption = g_strdup("");
    priv->host = g_strdup("");
    priv->directory = g_strdup("");
    priv->user = g_strdup("");
    priv->password = g_strdup("");
    priv->uid = g_strdup("");
}

static gboolean
gsw_install_source_uid_valid(const gchar *uid)
{
    if (uid == NULL || *uid == '\0')
        return FALSE;
    if (g_strcmp0(uid, ".") == 0 || g_strcmp0(uid, "..") == 0)
        return FALSE;
    return gsw_path_is_safe_relative(uid);
}

GswInstallSource *
gsw_install_source_new(GswSourceType type, const gchar *caption,
                       const gchar *host, const gchar *directory,
                       const gchar *user, const gchar *password,
                       const gchar *uid)
{
    GswInstallSource *self;
    GswInstallSourcePrivate *priv;

    if (caption == NULL || *caption == '\0' || host == NULL || *host == '\0')
        return NULL;

    if (uid != NULL && *uid != '\0' && !gsw_install_source_uid_valid(uid))
        return NULL;

    self = g_object_new(GSW_TYPE_INSTALL_SOURCE, NULL);
    priv = gsw_install_source_priv(self);

    priv->type = type;
    g_free(priv->caption);
    priv->caption = g_strdup(caption);
    g_free(priv->host);
    priv->host = g_strdup(host);
    g_free(priv->directory);
    priv->directory = gsw_str_remove_trailing_slashes(directory != NULL ? directory : "");
    g_free(priv->user);
    priv->user = g_strdup(user != NULL ? user : "");
    g_free(priv->password);
    priv->password = g_strdup(password != NULL ? password : "");
    g_free(priv->uid);
    priv->uid = g_strdup(uid != NULL && *uid != '\0' ? uid : host);

    return self;
}

GswInstallSource *
gsw_install_source_new_from_config(GswSourceType type, const gchar *value,
                                   GError **error)
{
    gchar **parts;
    guint n;
    GswInstallSource *source;

    if (value == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CONFIG,
                    "Empty installation source configuration");
        return NULL;
    }

    parts = g_strsplit(value, "|", -1);
    n = g_strv_length(parts);

    if (n < 3 || parts[0][0] == '\0' || parts[1][0] == '\0') {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CONFIG,
                    "Malformed installation source configuration: %s", value);
        g_strfreev(parts);
        return NULL;
    }

    source = gsw_install_source_new(type, parts[0], parts[1], parts[2],
                                    n > 3 ? parts[3] : "",
                                    n > 4 ? parts[4] : "",
                                    n > 5 ? parts[5] : "");
    if (source == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CONFIG,
                    "Invalid installation source UID: %s",
                    n > 5 && parts[5][0] != '\0' ? parts[5] : parts[1]);
    }

    g_strfreev(parts);
    return source;
}

gchar *
gsw_install_source_to_config(GswInstallSource *self)
{
    GswInstallSourcePrivate *priv;

    g_return_val_if_fail(GSW_IS_INSTALL_SOURCE(self), NULL);
    priv = gsw_install_source_priv(self);

    return g_strdup_printf("%s|%s|%s|%s|%s|%s",
                           priv->caption, priv->host, priv->directory,
                           priv->user, priv->password, priv->uid);
}

GswSourceType
gsw_install_source_get_source_type(GswInstallSource *self)
{
    return gsw_install_source_priv(self)->type;
}

const gchar *
gsw_install_source_get_caption(GswInstallSource *self)
{
    return gsw_install_source_priv(self)->caption;
}

const gchar *
gsw_install_source_get_host(GswInstallSource *self)
{
    return gsw_install_source_priv(self)->host;
}

const gchar *
gsw_install_source_get_directory(GswInstallSource *self)
{
    return gsw_install_source_priv(self)->directory;
}

const gchar *
gsw_install_source_get_user(GswInstallSource *self)
{
    return gsw_install_source_priv(self)->user;
}

const gchar *
gsw_install_source_get_password(GswInstallSource *self)
{
    return gsw_install_source_priv(self)->password;
}

const gchar *
gsw_install_source_get_uid(GswInstallSource *self)
{
    return gsw_install_source_priv(self)->uid;
}
