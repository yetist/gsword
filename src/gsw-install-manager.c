/*
 * gsw-install-manager.c: download, install, and remove modules
 */

#include <gsword/gsw-install-manager.h>
#include <gsword/gsw-error.h>
#include <gsword/gsw-transport.h>
#include "gsw-utils.h"
#include "gsw-tar.h"

#include <errno.h>
#include <string.h>
#include <glib/gstdio.h>

#define GSW_DEFAULT_TIMEOUT 10000
#define GSW_MAX_DIR_DEPTH 32

struct _GswInstallManager {
    GObject parent_instance;
};

typedef struct {
    gchar *private_path;
    GswConfig *config;
    GHashTable *sources;      /* caption -> GswInstallSource* */
    GPtrArray *source_order;  /* GswInstallSource* pointers */
    GHashTable *shadows;      /* uid -> GswManager* */
    GswStatusReporter *reporter;
    GCancellable *cancellable;
    gchar *default_user;
    gchar *default_password;
    gboolean passive;
    gboolean verify_peer;
    gboolean disclaimer_confirmed;
    guint timeout_msec;
} GswInstallManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GswInstallManager, gsw_install_manager, G_TYPE_OBJECT)

static GswInstallManagerPrivate *
gsw_install_manager_priv(GswInstallManager *self)
{
    return gsw_install_manager_get_instance_private(self);
}

static void
gsw_install_manager_finalize(GObject *object)
{
    GswInstallManager *self = GSW_INSTALL_MANAGER(object);
    GswInstallManagerPrivate *priv = gsw_install_manager_priv(self);

    g_free(priv->private_path);
    g_clear_object(&priv->config);
    g_clear_pointer(&priv->sources, g_hash_table_unref);
    g_clear_pointer(&priv->source_order, g_ptr_array_unref);
    g_clear_pointer(&priv->shadows, g_hash_table_unref);
    g_clear_object(&priv->reporter);
    g_clear_object(&priv->cancellable);
    g_free(priv->default_user);
    g_free(priv->default_password);

    G_OBJECT_CLASS(gsw_install_manager_parent_class)->finalize(object);
}

static void
gsw_install_manager_class_init(GswInstallManagerClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = gsw_install_manager_finalize;
}

static void
gsw_install_manager_init(GswInstallManager *self)
{
    GswInstallManagerPrivate *priv = gsw_install_manager_priv(self);

    priv->config = gsw_config_new();
    priv->sources = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                          g_object_unref);
    priv->source_order = g_ptr_array_new();
    priv->shadows = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                          g_object_unref);
    priv->default_user = g_strdup("ftp");
    priv->default_password = g_strdup("installmgr@user.com");
    priv->passive = TRUE;
    priv->verify_peer = TRUE;
    priv->disclaimer_confirmed = FALSE;
    priv->timeout_msec = GSW_DEFAULT_TIMEOUT;
}

static void
gsw_install_manager_clear_sources(GswInstallManager *self)
{
    GswInstallManagerPrivate *priv = gsw_install_manager_priv(self);

    g_hash_table_remove_all(priv->sources);
    g_ptr_array_set_size(priv->source_order, 0);
}

static gboolean
gsw_install_manager_add_source_internal(GswInstallManager *self,
                                        GswInstallSource *source)
{
    GswInstallManagerPrivate *priv = gsw_install_manager_priv(self);
    const gchar *caption = gsw_install_source_get_caption(source);

    if (g_hash_table_contains(priv->sources, caption)) {
        GswInstallSource *old = g_hash_table_lookup(priv->sources, caption);
        guint i;
        for (i = 0; i < priv->source_order->len; i++) {
            if (g_ptr_array_index(priv->source_order, i) == old) {
                g_ptr_array_index(priv->source_order, i) = source;
                break;
            }
        }
        g_hash_table_replace(priv->sources, g_strdup(caption),
                             g_object_ref(source));
        return TRUE;
    }

    g_hash_table_insert(priv->sources, g_strdup(caption), g_object_ref(source));
    g_ptr_array_add(priv->source_order, source);
    return TRUE;
}

static gboolean
gsw_install_manager_load_config_internal(GswInstallManager *self, GError **error)
{
    GswInstallManagerPrivate *priv = gsw_install_manager_priv(self);
    gchar *conf_path;
    GswConfig *config;
    GError *local_error = NULL;
    gchar **keys;
    guint i;

    conf_path = g_build_filename(priv->private_path, "InstallMgr.conf", NULL);

    if (g_file_test(conf_path, G_FILE_TEST_IS_REGULAR)) {
        config = gsw_config_new_from_file(conf_path, &local_error);
        if (config == NULL) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_CONFIG,
                        "Cannot load %s: %s", conf_path, local_error->message);
            g_error_free(local_error);
            g_free(conf_path);
            return FALSE;
        }
    } else {
        config = gsw_config_new();
    }

    g_free(conf_path);
    g_clear_object(&priv->config);
    priv->config = config;
    gsw_install_manager_clear_sources(self);

    {
        const gchar *passive = gsw_config_get(config, "General", "PassiveFTP");
        const gchar *verify = gsw_config_get(config, "General", "UnverifiedPeerAllowed");
        const gchar *timeout = gsw_config_get(config, "General", "TimeoutMillis");
        const gchar *user = gsw_config_get(config, "General", "User");
        const gchar *password = gsw_config_get(config, "General", "Password");

        if (passive != NULL)
            priv->passive = gsw_str_is_true(passive);
        if (verify != NULL)
            priv->verify_peer = !gsw_str_is_true(verify);
        if (timeout != NULL)
            priv->timeout_msec = (guint)gsw_str_parse_u64(timeout, GSW_DEFAULT_TIMEOUT);
        if (user != NULL) {
            g_free(priv->default_user);
            priv->default_user = g_strdup(user);
        }
        if (password != NULL) {
            g_free(priv->default_password);
            priv->default_password = g_strdup(password);
        }
    }

    keys = gsw_config_get_keys(config, "Sources");
    for (i = 0; keys != NULL && keys[i] != NULL; i++) {
        const gchar *key = keys[i];
        GswSourceType type;
        gchar **values;
        guint j;

        if (!g_str_has_suffix(key, "Source"))
            continue;

        if (g_ascii_strcasecmp(key, "FTPSource") == 0)
            type = GSW_SOURCE_FTP;
        else if (g_ascii_strcasecmp(key, "HTTPSSource") == 0)
            type = GSW_SOURCE_HTTPS;
        else if (g_ascii_strcasecmp(key, "HTTPSource") == 0)
            type = GSW_SOURCE_HTTP;
        else if (g_ascii_strcasecmp(key, "SFTPSource") == 0)
            type = GSW_SOURCE_SFTP;
        else
            continue;

        values = gsw_config_get_all(config, "Sources", key);
        for (j = 0; values != NULL && values[j] != NULL; j++) {
            GswInstallSource *source = gsw_install_source_new_from_config(
                type, values[j], &local_error);
            if (source == NULL) {
                g_warning("gsword: ignoring invalid source [%s]: %s", key,
                          local_error != NULL ? local_error->message : "unknown error");
                g_clear_error(&local_error);
                continue;
            }
            gsw_install_manager_add_source_internal(self, source);
            g_object_unref(source);
        }
        g_strfreev(values);
    }
    g_strfreev(keys);

    return TRUE;
}

gboolean
gsw_install_manager_load_config(GswInstallManager *self, GError **error)
{
    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), FALSE);
    return gsw_install_manager_load_config_internal(self, error);
}

gboolean
gsw_install_manager_save_config(GswInstallManager *self, GError **error)
{
    GswInstallManagerPrivate *priv;
    GswConfig *config;
    gchar *path;
    guint i;
    gboolean ok;

    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), FALSE);

    priv = gsw_install_manager_priv(self);
    config = gsw_config_new();

    gsw_config_set(config, "General", "PassiveFTP", priv->passive ? "true" : "false");
    gsw_config_set(config, "General", "UnverifiedPeerAllowed",
                   priv->verify_peer ? "false" : "true");
    {
        gchar *timeout = g_strdup_printf("%u", priv->timeout_msec);
        gsw_config_set(config, "General", "TimeoutMillis", timeout);
        g_free(timeout);
    }
    if (priv->default_user != NULL && *priv->default_user != '\0')
        gsw_config_set(config, "General", "User", priv->default_user);
    if (priv->default_password != NULL && *priv->default_password != '\0')
        gsw_config_set(config, "General", "Password", priv->default_password);

    for (i = 0; i < priv->source_order->len; i++) {
        GswInstallSource *source = g_ptr_array_index(priv->source_order, i);
        gchar *key = g_strdup_printf("%sSource",
                                     gsw_source_type_to_string(
                                         gsw_install_source_get_source_type(source)));
        gchar *value = gsw_install_source_to_config(source);
        gsw_config_add(config, "Sources", key, value);
        g_free(key);
        g_free(value);
    }

    path = g_build_filename(priv->private_path, "InstallMgr.conf", NULL);
    ok = gsw_config_save(config, path, error);
    g_free(path);

    if (ok) {
        g_clear_object(&priv->config);
        priv->config = config;
    } else {
        g_object_unref(config);
    }
    return ok;
}

GswInstallManager *
gsw_install_manager_new(const gchar *private_path, GError **error)
{
    GswInstallManager *self;
    GswInstallManagerPrivate *priv;

    g_return_val_if_fail(private_path != NULL, NULL);

    self = g_object_new(GSW_TYPE_INSTALL_MANAGER, NULL);
    priv = gsw_install_manager_priv(self);

    priv->private_path = g_canonicalize_filename(private_path, NULL);
    if (!gsw_mkdir_with_parents(priv->private_path, error)) {
        g_object_unref(self);
        return NULL;
    }

    if (!gsw_install_manager_load_config_internal(self, error)) {
        g_object_unref(self);
        return NULL;
    }
    return self;
}

void
gsw_install_manager_set_status_reporter(GswInstallManager *self,
                                        GswStatusReporter *reporter)
{
    GswInstallManagerPrivate *priv;

    g_return_if_fail(GSW_IS_INSTALL_MANAGER(self));
    priv = gsw_install_manager_priv(self);
    g_set_object(&priv->reporter, reporter);
}

GswStatusReporter *
gsw_install_manager_get_status_reporter(GswInstallManager *self)
{
    GswInstallManagerPrivate *priv;

    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), NULL);
    priv = gsw_install_manager_priv(self);
    return priv->reporter;
}

void
gsw_install_manager_set_cancellable(GswInstallManager *self,
                                    GCancellable *cancellable)
{
    GswInstallManagerPrivate *priv;

    g_return_if_fail(GSW_IS_INSTALL_MANAGER(self));
    priv = gsw_install_manager_priv(self);
    g_set_object(&priv->cancellable, cancellable);
}

GCancellable *
gsw_install_manager_get_cancellable(GswInstallManager *self)
{
    GswInstallManagerPrivate *priv;

    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), NULL);
    priv = gsw_install_manager_priv(self);
    return priv->cancellable;
}

void
gsw_install_manager_set_user_disclaimer_confirmed(GswInstallManager *self,
                                                  gboolean confirmed)
{
    GswInstallManagerPrivate *priv;
    g_return_if_fail(GSW_IS_INSTALL_MANAGER(self));
    priv = gsw_install_manager_priv(self);
    priv->disclaimer_confirmed = confirmed;
}

gboolean
gsw_install_manager_is_user_disclaimer_confirmed(GswInstallManager *self)
{
    GswInstallManagerPrivate *priv;
    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), FALSE);
    priv = gsw_install_manager_priv(self);
    return priv->disclaimer_confirmed;
}

void
gsw_install_manager_set_passive_ftp(GswInstallManager *self, gboolean passive)
{
    GswInstallManagerPrivate *priv = gsw_install_manager_priv(self);
    priv->passive = passive;
}

gboolean
gsw_install_manager_get_passive_ftp(GswInstallManager *self)
{
    return gsw_install_manager_priv(self)->passive;
}

void
gsw_install_manager_set_timeout(GswInstallManager *self, guint timeout_msec)
{
    GswInstallManagerPrivate *priv = gsw_install_manager_priv(self);
    priv->timeout_msec = timeout_msec > 0 ? timeout_msec : GSW_DEFAULT_TIMEOUT;
}

guint
gsw_install_manager_get_timeout(GswInstallManager *self)
{
    return gsw_install_manager_priv(self)->timeout_msec;
}

void
gsw_install_manager_set_verify_peer(GswInstallManager *self, gboolean verify)
{
    gsw_install_manager_priv(self)->verify_peer = verify;
}

gboolean
gsw_install_manager_get_verify_peer(GswInstallManager *self)
{
    return gsw_install_manager_priv(self)->verify_peer;
}

const gchar *
gsw_install_manager_get_private_path(GswInstallManager *self)
{
    return gsw_install_manager_priv(self)->private_path;
}

GPtrArray *
gsw_install_manager_get_sources(GswInstallManager *self)
{
    GswInstallManagerPrivate *priv;
    GPtrArray *result;
    guint i;

    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), NULL);
    priv = gsw_install_manager_priv(self);
    result = g_ptr_array_new_with_free_func(g_object_unref);
    for (i = 0; i < priv->source_order->len; i++)
        g_ptr_array_add(result, g_object_ref(g_ptr_array_index(priv->source_order, i)));
    return result;
}

GswInstallSource *
gsw_install_manager_lookup_source(GswInstallManager *self, const gchar *caption)
{
    GswInstallManagerPrivate *priv;

    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), NULL);
    g_return_val_if_fail(caption != NULL, NULL);

    priv = gsw_install_manager_priv(self);
    return g_hash_table_lookup(priv->sources, caption);
}

GswInstallSource *
gsw_install_manager_lookup_source_by_uid(GswInstallManager *self, const gchar *uid)
{
    GswInstallManagerPrivate *priv;
    guint i;

    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), NULL);
    g_return_val_if_fail(uid != NULL, NULL);

    priv = gsw_install_manager_priv(self);
    for (i = 0; i < priv->source_order->len; i++) {
        GswInstallSource *source = g_ptr_array_index(priv->source_order, i);
        if (g_strcmp0(gsw_install_source_get_uid(source), uid) == 0)
            return source;
    }
    return NULL;
}

gboolean
gsw_install_manager_add_source(GswInstallManager *self, GswInstallSource *source)
{
    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), FALSE);
    g_return_val_if_fail(GSW_IS_INSTALL_SOURCE(source), FALSE);
    return gsw_install_manager_add_source_internal(self, source);
}

gboolean
gsw_install_manager_remove_source(GswInstallManager *self, const gchar *caption)
{
    GswInstallManagerPrivate *priv;
    GswInstallSource *source;
    guint i;

    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), FALSE);
    g_return_val_if_fail(caption != NULL, FALSE);

    priv = gsw_install_manager_priv(self);
    source = g_hash_table_lookup(priv->sources, caption);
    if (source == NULL)
        return FALSE;

    for (i = 0; i < priv->source_order->len; i++) {
        if (g_ptr_array_index(priv->source_order, i) == source) {
            g_ptr_array_remove_index(priv->source_order, i);
            break;
        }
    }
    g_hash_table_remove(priv->sources, caption);
    return TRUE;
}

static gboolean
gsw_install_manager_check_disclaimer(GswInstallManager *self, GError **error)
{
    if (!gsw_install_manager_is_user_disclaimer_confirmed(self)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_PERMISSION,
                    "Remote source access requires user disclaimer confirmation");
        return FALSE;
    }
    return TRUE;
}

static GswTransport *
gsw_install_manager_create_transport(GswInstallManager *self,
                                     GswInstallSource *source,
                                     GError **error)
{
    GswInstallManagerPrivate *priv = gsw_install_manager_priv(self);
    GswTransport *transport;
    const gchar *user;
    const gchar *password;

    transport = gsw_transport_new_curl(
        gsw_install_source_get_source_type(source),
        gsw_install_source_get_host(source), error);
    if (transport == NULL)
        return NULL;

    user = gsw_install_source_get_user(source);
    password = gsw_install_source_get_password(source);
    if (user == NULL || *user == '\0')
        user = priv->default_user;
    if (password == NULL || *password == '\0')
        password = priv->default_password;

    gsw_transport_set_credentials(transport, user, password);
    gsw_transport_set_passive(transport, priv->passive);
    gsw_transport_set_timeout(transport, priv->timeout_msec);
    gsw_transport_set_verify_peer(transport, priv->verify_peer);
    gsw_transport_set_status_reporter(transport, priv->reporter);

    return transport;
}

static gchar *
gsw_install_manager_source_url(GswInstallSource *source, const gchar *path)
{
    return gsw_build_url(gsw_source_type_to_scheme(
                             gsw_install_source_get_source_type(source)),
                         gsw_install_source_get_host(source),
                         gsw_install_source_get_directory(source), path);
}

static gboolean
gsw_install_manager_download_to_file(GswTransport *transport, const gchar *url,
                                     const gchar *dest,
                                     GCancellable *cancellable,
                                     GError **error)
{
    GFile *file;
    GFileOutputStream *out;
    gchar *parent;
    GError *local_error = NULL;
    gboolean ok;

    parent = g_path_get_dirname(dest);
    if (!gsw_mkdir_with_parents(parent, error)) {
        g_free(parent);
        return FALSE;
    }
    g_free(parent);

    file = g_file_new_for_path(dest);
    out = g_file_replace(file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION,
                         cancellable, &local_error);
    g_object_unref(file);
    if (out == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot create %s: %s", dest, local_error->message);
        g_error_free(local_error);
        return FALSE;
    }

    ok = gsw_transport_get(transport, url, G_OUTPUT_STREAM(out),
                           cancellable, error);
    if (!g_output_stream_close(G_OUTPUT_STREAM(out), cancellable, &local_error)) {
        if (ok) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                        "Cannot close %s: %s", dest, local_error->message);
            ok = FALSE;
        }
        g_clear_error(&local_error);
    }
    g_object_unref(out);

    if (!ok)
        g_unlink(dest);
    return ok;
}

static gboolean
gsw_install_manager_download_dir(GswInstallManager *self,
                                 GswInstallSource *source,
                                 GswTransport *transport,
                                 const gchar *remote_dir,
                                 const gchar *local_dir,
                                 guint depth,
                                 GCancellable *cancellable,
                                 GError **error)
{
    GswInstallManagerPrivate *priv = gsw_install_manager_priv(self);
    gchar *dir_url;
    GPtrArray *entries;
    guint i;

    if (depth > GSW_MAX_DIR_DEPTH) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Remote directory nesting is too deep: %s", remote_dir);
        return FALSE;
    }

    if (!gsw_mkdir_with_parents(local_dir, error))
        return FALSE;

    dir_url = g_strdup_printf("%s%s", remote_dir,
                              g_str_has_suffix(remote_dir, "/") ? "" : "/");
    entries = gsw_transport_list_dir(transport, dir_url, cancellable, error);
    if (entries == NULL) {
        g_free(dir_url);
        return FALSE;
    }

    for (i = 0; i < entries->len; i++) {
        GswDirEntry *entry = g_ptr_array_index(entries, i);
        const gchar *name = gsw_dir_entry_get_name(entry);
        gchar *child_remote;
        gchar *child_local;

        if (!gsw_path_is_safe_relative(name)) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Unsafe remote file name: %s", name);
            g_ptr_array_unref(entries);
            g_free(dir_url);
            return FALSE;
        }

        child_remote = g_strconcat(dir_url, name, NULL);
        child_local = g_build_filename(local_dir, name, NULL);

        if (gsw_dir_entry_is_directory(entry)) {
            gchar *child_remote_dir = g_strconcat(child_remote, "/", NULL);
            gboolean ok = gsw_install_manager_download_dir(
                self, source, transport, child_remote_dir, child_local,
                depth + 1, cancellable, error);
            g_free(child_remote_dir);
            g_free(child_remote);
            g_free(child_local);
            if (!ok) {
                g_ptr_array_unref(entries);
                g_free(dir_url);
                return FALSE;
            }
        } else {
            if (priv->reporter != NULL)
                gsw_status_reporter_emit_pre_status(priv->reporter, 0, 0, name);
            if (!gsw_install_manager_download_to_file(transport, child_remote,
                                                      child_local, cancellable,
                                                      error)) {
                g_free(child_remote);
                g_free(child_local);
                g_ptr_array_unref(entries);
                g_free(dir_url);
                return FALSE;
            }
            g_free(child_remote);
            g_free(child_local);
        }
    }

    g_ptr_array_unref(entries);
    g_free(dir_url);
    return TRUE;
}

static gboolean
gsw_install_manager_staging_has_mods(GswInstallManager *self,
                                     const gchar *staging, GError **error)
{
    gchar *mods_dir = g_build_filename(staging, "mods.d", NULL);
    GDir *dir;
    const gchar *name;
    gboolean found = FALSE;
    GError *local_error = NULL;

    (void)self;

    dir = g_dir_open(mods_dir, 0, &local_error);
    if (dir == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Downloaded archive has no mods.d directory: %s",
                    local_error->message);
        g_error_free(local_error);
        g_free(mods_dir);
        return FALSE;
    }

    while ((name = g_dir_read_name(dir)) != NULL) {
        if (gsw_str_ends_with_ci(name, ".conf")) {
            found = TRUE;
            break;
        }
    }
    g_dir_close(dir);
    g_free(mods_dir);

    if (!found) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Downloaded source contains no module configuration files");
        return FALSE;
    }
    return TRUE;
}

static gboolean
gsw_install_manager_refresh_with_archive(GswInstallManager *self,
                                         GswInstallSource *source,
                                         GswTransport *transport,
                                         const gchar *staging,
                                         GCancellable *cancellable,
                                         GError **error)
{
    GswInstallManagerPrivate *priv = gsw_install_manager_priv(self);
    gchar *url = gsw_install_manager_source_url(source, "mods.d.tar.gz");
    gchar *archive = g_build_filename(staging, "mods.d.tar.gz", NULL);
    gboolean ok;

    if (priv->reporter != NULL)
        gsw_status_reporter_emit_pre_status(priv->reporter, 0, 0,
                                            "Downloading module list");
    ok = gsw_install_manager_download_to_file(transport, url, archive,
                                              cancellable, error);
    g_free(url);
    if (!ok) {
        g_free(archive);
        return FALSE;
    }

    ok = gsw_tar_extract_gz(archive, staging, error);
    g_unlink(archive);
    g_free(archive);
    if (!ok)
        return FALSE;

    return gsw_install_manager_staging_has_mods(self, staging, error);
}

static gboolean
gsw_install_manager_refresh_with_listing(GswInstallManager *self,
                                         GswInstallSource *source,
                                         GswTransport *transport,
                                         const gchar *staging,
                                         GCancellable *cancellable,
                                         GError **error)
{
    GswInstallManagerPrivate *priv = gsw_install_manager_priv(self);
    gchar *base = gsw_install_manager_source_url(source, "mods.d/");
    gchar *mods_dir = g_build_filename(staging, "mods.d", NULL);
    GPtrArray *entries;
    guint i;
    guint64 total = 0;
    guint64 completed = 0;

    entries = gsw_transport_list_dir(transport, base, cancellable, error);
    if (entries == NULL) {
        g_free(base);
        g_free(mods_dir);
        return FALSE;
    }

    for (i = 0; i < entries->len; i++) {
        GswDirEntry *entry = g_ptr_array_index(entries, i);
        if (!gsw_dir_entry_is_directory(entry) &&
            gsw_str_ends_with_ci(gsw_dir_entry_get_name(entry), ".conf"))
            total += gsw_dir_entry_get_size(entry);
    }

    if (!gsw_mkdir_with_parents(mods_dir, error)) {
        g_ptr_array_unref(entries);
        g_free(base);
        g_free(mods_dir);
        return FALSE;
    }

    for (i = 0; i < entries->len; i++) {
        GswDirEntry *entry = g_ptr_array_index(entries, i);
        const gchar *name = gsw_dir_entry_get_name(entry);
        gchar *url;
        gchar *dest;

        if (gsw_dir_entry_is_directory(entry) ||
            !gsw_str_ends_with_ci(name, ".conf") ||
            !gsw_path_is_safe_relative(name))
            continue;

        url = g_strconcat(base, name, NULL);
        dest = g_build_filename(mods_dir, name, NULL);

        if (priv->reporter != NULL)
            gsw_status_reporter_emit_pre_status(priv->reporter, total, completed,
                                                name);
        if (!gsw_install_manager_download_to_file(transport, url, dest,
                                                  cancellable, error)) {
            g_free(url);
            g_free(dest);
            g_ptr_array_unref(entries);
            g_free(base);
            g_free(mods_dir);
            return FALSE;
        }
        completed += gsw_dir_entry_get_size(entry);
        g_free(url);
        g_free(dest);
    }

    g_ptr_array_unref(entries);
    g_free(base);
    g_free(mods_dir);

    return gsw_install_manager_staging_has_mods(self, staging, error);
}

gboolean
gsw_install_manager_refresh_remote_source(GswInstallManager *self,
                                          GswInstallSource *source,
                                          GError **error)
{
    GswInstallManagerPrivate *priv;
    GswTransport *transport;
    gchar *staging = NULL;
    gchar *final_root = NULL;
    GError *local_error = NULL;

    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), FALSE);
    g_return_val_if_fail(GSW_IS_INSTALL_SOURCE(source), FALSE);

    if (!gsw_install_manager_check_disclaimer(self, error))
        return FALSE;

    priv = gsw_install_manager_priv(self);
    transport = gsw_install_manager_create_transport(self, source, error);
    if (transport == NULL)
        return FALSE;

    staging = gsw_path_make_temp_dir(priv->private_path, ".refresh", error);
    if (staging == NULL)
        goto fail;

    if (!gsw_install_manager_refresh_with_archive(self, source, transport,
                                                  staging, priv->cancellable,
                                                  &local_error)) {
        if (priv->cancellable != NULL &&
            g_cancellable_is_cancelled(priv->cancellable)) {
            if (local_error != NULL)
                g_propagate_error(error, local_error);
            else
                g_set_error(error, GSW_ERROR, GSW_ERROR_CANCELLED,
                            "Refresh cancelled");
            goto fail;
        }

        if (local_error == NULL) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Failed to download module list");
            goto fail;
        }

        /* Fall back to a directory listing for operation errors. */
        if (local_error->code == GSW_ERROR_NETWORK ||
            local_error->code == GSW_ERROR_PERMISSION) {
            g_propagate_error(error, local_error);
            goto fail;
        }

        g_clear_error(&local_error);
        {
            gchar *mods_dir = g_build_filename(staging, "mods.d", NULL);
            gsw_dir_remove_recursive(mods_dir, NULL);
            g_free(mods_dir);
        }

        if (!gsw_install_manager_refresh_with_listing(self, source, transport,
                                                      staging, priv->cancellable,
                                                      error))
            goto fail;
    }

    final_root = g_build_filename(priv->private_path,
                                  gsw_install_source_get_uid(source), NULL);

    if (g_file_test(final_root, G_FILE_TEST_EXISTS)) {
        if (!gsw_dir_remove_recursive(final_root, error))
            goto fail;
    }

    if (g_rename(staging, final_root) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot move refreshed source into place: %s",
                    g_strerror(errno));
        goto fail;
    }
    g_free(staging);
    staging = NULL;

    g_hash_table_remove(priv->shadows, gsw_install_source_get_uid(source));

    g_object_unref(transport);
    g_free(final_root);
    return TRUE;

fail:
    if (staging != NULL) {
        gsw_dir_remove_recursive(staging, NULL);
        g_free(staging);
    }
    g_free(final_root);
    g_object_unref(transport);
    return FALSE;
}

static GswManager *
gsw_install_manager_get_shadow(GswInstallManager *self,
                               GswInstallSource *source, GError **error)
{
    GswInstallManagerPrivate *priv = gsw_install_manager_priv(self);
    const gchar *uid = gsw_install_source_get_uid(source);
    GswManager *manager;
    gchar *root;

    manager = g_hash_table_lookup(priv->shadows, uid);
    if (manager != NULL)
        return manager;

    root = g_build_filename(priv->private_path, uid, NULL);
    if (!g_file_test(root, G_FILE_TEST_IS_DIR)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "Remote source has not been refreshed: %s",
                    gsw_install_source_get_caption(source));
        g_free(root);
        return NULL;
    }

    manager = gsw_manager_new_for_path(root, error);
    g_free(root);
    if (manager == NULL)
        return NULL;

    g_hash_table_insert(priv->shadows, g_strdup(uid), manager);
    return manager;
}

GPtrArray *
gsw_install_manager_get_remote_module_infos(GswInstallManager *self,
                                            GswInstallSource *source,
                                            GError **error)
{
    GswManager *shadow;

    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), NULL);
    g_return_val_if_fail(GSW_IS_INSTALL_SOURCE(source), NULL);

    shadow = gsw_install_manager_get_shadow(self, source, error);
    if (shadow == NULL)
        return NULL;
    return gsw_manager_get_module_infos(shadow);
}

GswModuleInfo *
gsw_install_manager_lookup_remote_module_info(GswInstallManager *self,
                                              GswInstallSource *source,
                                              const gchar *name,
                                              GError **error)
{
    GswManager *shadow;

    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), NULL);
    g_return_val_if_fail(GSW_IS_INSTALL_SOURCE(source), NULL);
    g_return_val_if_fail(name != NULL, NULL);

    shadow = gsw_install_manager_get_shadow(self, source, error);
    if (shadow == NULL)
        return NULL;

    {
        GswModuleInfo *info = gsw_manager_lookup_module_info(shadow, name);
        if (info == NULL)
            g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                        "Module [%s] not found in source %s", name,
                        gsw_install_source_get_caption(source));
        return info;
    }
}

static gboolean
gsw_install_manager_copy_staging_to_prefix(GswInstallManager *self,
                                           const gchar *staging,
                                           const gchar *prefix,
                                           GError **error)
{
    (void)self;
    return gsw_dir_copy_recursive(staging, prefix, error);
}

gboolean
gsw_install_manager_install_module(GswInstallManager *self,
                                   GswManager *destination,
                                   GswInstallSource *source,
                                   const gchar *module_name,
                                   GError **error)
{
    GswInstallManagerPrivate *priv;
    GswModuleInfo *info;
    GswTransport *transport = NULL;
    gchar *staging = NULL;
    gchar **files = NULL;
    gboolean ok = FALSE;

    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), FALSE);
    g_return_val_if_fail(GSW_IS_MANAGER(destination), FALSE);
    g_return_val_if_fail(GSW_IS_INSTALL_SOURCE(source), FALSE);
    g_return_val_if_fail(module_name != NULL, FALSE);

    if (!gsw_install_manager_check_disclaimer(self, error))
        return FALSE;

    priv = gsw_install_manager_priv(self);
    info = gsw_install_manager_lookup_remote_module_info(self, source, module_name, error);
    if (info == NULL)
        return FALSE;

    if (gsw_module_info_is_ciphered(info)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CIPHERED,
                    "Module [%s] is encrypted and cannot be installed by this version",
                    module_name);
        return FALSE;
    }

    transport = gsw_install_manager_create_transport(self, source, error);
    if (transport == NULL)
        return FALSE;

    staging = gsw_path_make_temp_dir(gsw_manager_get_prefix_path(destination),
                                     ".gsw-install", error);
    if (staging == NULL)
        goto out;

    files = gsw_module_info_get_values(info, "File");
    if (files != NULL) {
        guint i;
        for (i = 0; files[i] != NULL; i++) {
            gchar *url;
            gchar *dest;
            gchar *local_rel;

            if (!gsw_path_is_safe_relative(files[i])) {
                g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                            "Unsafe File= entry: %s", files[i]);
                goto out;
            }

            url = gsw_install_manager_source_url(source, files[i]);
            local_rel = g_strdup(files[i]);
            while (g_str_has_prefix(local_rel, "./"))
                memmove(local_rel, local_rel + 2, strlen(local_rel + 2) + 1);
            dest = gsw_path_join_under(staging, local_rel, error);
            g_free(local_rel);
            if (dest == NULL) {
                g_free(url);
                goto out;
            }

            if (priv->reporter != NULL)
                gsw_status_reporter_emit_pre_status(priv->reporter, 0, 0, files[i]);
            if (!gsw_install_manager_download_to_file(transport, url, dest,
                                                      priv->cancellable, error)) {
                g_free(url);
                g_free(dest);
                goto out;
            }
            g_free(url);
            g_free(dest);
        }
    } else {
        const gchar *relative_dir = gsw_module_info_get_relative_data_dir(info);
        gchar *remote_dir;
        gchar *local_dir;

        if (relative_dir == NULL || *relative_dir == '\0') {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Module [%s] has no installable data directory", module_name);
            goto out;
        }

        remote_dir = gsw_install_manager_source_url(source, relative_dir);
        local_dir = gsw_path_join_under(staging, relative_dir, error);
        if (local_dir == NULL) {
            g_free(remote_dir);
            goto out;
        }

        if (!gsw_install_manager_download_dir(self, source, transport,
                                              remote_dir, local_dir, 0,
                                              priv->cancellable, error)) {
            g_free(remote_dir);
            g_free(local_dir);
            goto out;
        }
        g_free(remote_dir);
        g_free(local_dir);
    }

    if (!gsw_install_manager_copy_staging_to_prefix(
            self, staging, gsw_manager_get_prefix_path(destination), error))
        goto out;

    {
        const gchar *config_file = gsw_module_info_get_config_file(info);
        gchar *mods_dir;
        gchar *dest_conf;
        gchar *conf_name;

        if (config_file == NULL || *config_file == '\0') {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Module [%s] has no configuration file", module_name);
            goto out;
        }

        mods_dir = g_build_filename(gsw_manager_get_prefix_path(destination),
                                    "mods.d", NULL);
        if (!gsw_mkdir_with_parents(mods_dir, error)) {
            g_free(mods_dir);
            goto out;
        }
        conf_name = g_path_get_basename(config_file);
        dest_conf = g_build_filename(mods_dir, conf_name, NULL);
        g_free(conf_name);
        g_free(mods_dir);

        if (!gsw_file_copy(config_file, dest_conf, error)) {
            g_free(dest_conf);
            goto out;
        }
        g_free(dest_conf);
    }

    gsw_manager_reload(destination, NULL);
    ok = TRUE;

out:
    g_strfreev(files);
    if (staging != NULL) {
        gsw_dir_remove_recursive(staging, NULL);
        g_free(staging);
    }
    g_object_unref(transport);
    return ok;
}

gboolean
gsw_install_manager_remove_module(GswInstallManager *self,
                                  GswManager *destination,
                                  const gchar *module_name,
                                  GError **error)
{
    GswModuleInfo *info;
    const gchar *prefix;
    const gchar *config_file;
    gboolean ok = TRUE;

    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), FALSE);
    g_return_val_if_fail(GSW_IS_MANAGER(destination), FALSE);
    g_return_val_if_fail(module_name != NULL, FALSE);

    (void)self;
    info = gsw_manager_lookup_module_info(destination, module_name);
    if (info == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "Module [%s] is not installed", module_name);
        return FALSE;
    }

    prefix = gsw_manager_get_prefix_path(destination);
    config_file = gsw_module_info_get_config_file(info);

    {
        gchar **files = gsw_module_info_get_values(info, "File");
        if (files != NULL) {
            guint i;
            for (i = 0; files[i] != NULL; i++) {
                gchar *path = gsw_path_join_under(prefix, files[i], error);
                if (path == NULL) {
                    ok = FALSE;
                    break;
                }
                if (g_unlink(path) != 0 && errno != ENOENT) {
                    g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                                "Cannot remove %s: %s", path, g_strerror(errno));
                    ok = FALSE;
                }
                g_free(path);
                if (!ok)
                    break;
            }
            g_strfreev(files);
        } else {
            const gchar *data_path = gsw_module_info_get_absolute_data_path(info);
            if (data_path != NULL && *data_path != '\0' &&
                g_strcmp0(data_path, prefix) != 0 &&
                gsw_path_is_within(prefix, data_path)) {
                if (!gsw_dir_remove_recursive(data_path, error))
                    ok = FALSE;
            }
        }
    }

    if (ok && config_file != NULL && *config_file != '\0' &&
        gsw_path_is_within(prefix, config_file)) {
        GswConfig *config = gsw_config_new();
        GError *local_error = NULL;

        if (gsw_config_load(config, config_file, &local_error)) {
            gsw_config_remove_section(config, module_name);
            {
                gchar **sections = gsw_config_get_sections(config);
                if (sections == NULL || sections[0] == NULL) {
                    if (g_unlink(config_file) != 0 && errno != ENOENT) {
                        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                                    "Cannot remove %s: %s", config_file,
                                    g_strerror(errno));
                        ok = FALSE;
                    }
                } else if (!gsw_config_save(config, config_file, error)) {
                    ok = FALSE;
                }
                g_strfreev(sections);
            }
        } else {
            g_propagate_error(error, local_error);
            ok = FALSE;
        }
        g_object_unref(config);
    }

    if (ok)
        gsw_manager_reload(destination, NULL);

    return ok;
}

guint
gsw_install_manager_get_module_status(GswManager *local, GswModuleInfo *remote)
{
    guint status = 0;
    GswModuleInfo *local_info;
    const gchar *remote_version;
    const gchar *local_version;

    g_return_val_if_fail(GSW_IS_MANAGER(local), 0);
    g_return_val_if_fail(GSW_IS_MODULE_INFO(remote), 0);

    if (gsw_module_info_is_ciphered(remote))
        status |= GSW_MODULE_STATUS_CIPHERED;
    if (gsw_module_info_get_value(remote, "CipherKey") != NULL)
        status |= GSW_MODULE_STATUS_KEY_PRESENT;

    local_info = gsw_manager_lookup_module_info(local,
                                                gsw_module_info_get_name(remote));
    if (local_info == NULL) {
        status |= GSW_MODULE_STATUS_NEW;
        return status;
    }

    remote_version = gsw_module_info_get_version(remote);
    local_version = gsw_module_info_get_version(local_info);
    if (gsw_version_compare(remote_version, local_version) > 0)
        status |= GSW_MODULE_STATUS_UPDATED;
    else if (gsw_version_compare(remote_version, local_version) < 0)
        status |= GSW_MODULE_STATUS_OLDER;
    else
        status |= GSW_MODULE_STATUS_SAME_VERSION;

    return status;
}

gboolean
gsw_install_manager_refresh_remote_source_configuration(GswInstallManager *self,
                                                        GError **error)
{
    GswInstallManagerPrivate *priv;
    GswInstallSource *master_source;
    GswTransport *transport;
    gchar *master_url;
    gchar *master_path;
    GBytes *bytes;
    GswConfig *config;
    gchar **keys;
    guint i;

    g_return_val_if_fail(GSW_IS_INSTALL_MANAGER(self), FALSE);

    if (!gsw_install_manager_check_disclaimer(self, error))
        return FALSE;

    priv = gsw_install_manager_priv(self);
    master_source = gsw_install_source_new(GSW_SOURCE_FTP, "CrossWire Master",
                                           "ftp.crosswire.org", "/pub/sword",
                                           "", "", "master-repo-list");
    if (master_source == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CONFIG,
                    "Cannot create master source");
        return FALSE;
    }

    transport = gsw_install_manager_create_transport(self, master_source, error);
    if (transport == NULL) {
        g_object_unref(master_source);
        return FALSE;
    }

    master_url = gsw_install_manager_source_url(master_source, "masterRepoList.conf");
    bytes = gsw_transport_get_bytes(transport, master_url, priv->cancellable, error);
    g_free(master_url);
    if (bytes == NULL) {
        g_object_unref(transport);
        g_object_unref(master_source);
        return FALSE;
    }

    master_path = g_build_filename(priv->private_path, "masterRepoList.conf", NULL);
    {
        gsize size;
        const gchar *data = g_bytes_get_data(bytes, &size);
        GError *local_error = NULL;
        if (!g_file_set_contents(master_path, data, (gssize)size, &local_error)) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                        "Cannot save %s: %s", master_path, local_error->message);
            g_error_free(local_error);
            g_bytes_unref(bytes);
            g_free(master_path);
            g_object_unref(transport);
            g_object_unref(master_source);
            return FALSE;
        }
    }
    g_bytes_unref(bytes);

    config = gsw_config_new_from_file(master_path, error);
    g_free(master_path);
    g_object_unref(transport);
    g_object_unref(master_source);
    if (config == NULL)
        return FALSE;

    keys = gsw_config_get_keys(config, "Repos");
    for (i = 0; keys != NULL && keys[i] != NULL; i++) {
        const gchar *uid = keys[i];
        gchar **values = gsw_config_get_all(config, "Repos", uid);

        for (guint j = 0; values != NULL && values[j] != NULL; j++) {
            const gchar *value = values[j];
            const gchar *eq;
            gchar *type_name;
            GswSourceType type;
            GswInstallSource *source;

            if (g_strcmp0(value, "REMOVE") == 0) {
                GswInstallSource *old = gsw_install_manager_lookup_source_by_uid(self, uid);
                if (old != NULL)
                    gsw_install_manager_remove_source(
                        self, gsw_install_source_get_caption(old));
                continue;
            }

            eq = strchr(value, '=');
            if (eq == NULL)
                continue;

            type_name = g_strndup(value, (gsize)(eq - value));
            if (!g_str_has_suffix(type_name, "Source")) {
                g_free(type_name);
                continue;
            }

            if (g_ascii_strcasecmp(type_name, "HTTPSSource") == 0)
                type = GSW_SOURCE_HTTPS;
            else if (g_ascii_strcasecmp(type_name, "HTTPSource") == 0)
                type = GSW_SOURCE_HTTP;
            else if (g_ascii_strcasecmp(type_name, "SFTPSource") == 0)
                type = GSW_SOURCE_SFTP;
            else
                type = GSW_SOURCE_FTP;
            g_free(type_name);

            source = gsw_install_source_new_from_config(type, eq + 1, NULL);
            if (source == NULL)
                continue;

            /* masterRepoList.conf does not carry the UID; use the key. */
            {
                GswInstallSource *with_uid = gsw_install_source_new(
                    type,
                    gsw_install_source_get_caption(source),
                    gsw_install_source_get_host(source),
                    gsw_install_source_get_directory(source),
                    gsw_install_source_get_user(source),
                    gsw_install_source_get_password(source),
                    uid);
                g_object_unref(source);
                source = with_uid;
            }

            if (source != NULL) {
                gsw_install_manager_add_source(self, source);
                g_object_unref(source);
            }
        }
        g_strfreev(values);
    }
    g_strfreev(keys);
    g_object_unref(config);

    return gsw_install_manager_save_config(self, error);
}
