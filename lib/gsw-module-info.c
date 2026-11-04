/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-module-info.c: immutable module metadata
 */

#include <gsword/gsw-module-info.h>
#include <gsword/gsw-error.h>
#include "gsw-utils.h"

#include <string.h>

struct _GswModuleInfo {
    GObject parent_instance;
};

typedef struct {
    gchar *name;
    gchar *description;
    gchar *type_string;
    gchar *driver_name;
    gchar *language;
    gchar *version;
    gchar *minimum_version;
    gchar *data_path;
    gchar *absolute_data_path;
    gchar *relative_data_dir;
    gchar *prefix_path;
    gchar *config_file;
    gchar *encoding;
    gchar *source_type;
    gchar *versification;
    gchar *copyright;
    gchar *distribution_license;
    gchar *about;
    guint64 install_size;
    GswModuleType module_type;
    GswDriver driver;
    gboolean ciphered;
    gboolean supported;
    GHashTable *values; /* key -> GPtrArray of gchar* */
} GswModuleInfoPrivate;

enum {
    PROP_0,
    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_TYPE_STRING,
    PROP_MODULE_TYPE,
    PROP_DRIVER,
    PROP_DRIVER_NAME,
    PROP_LANGUAGE,
    PROP_VERSION,
    PROP_MINIMUM_VERSION,
    PROP_INSTALL_SIZE,
    PROP_DATA_PATH,
    PROP_ABSOLUTE_DATA_PATH,
    PROP_RELATIVE_DATA_DIR,
    PROP_PREFIX_PATH,
    PROP_CONFIG_FILE,
    PROP_ENCODING,
    PROP_SOURCE_TYPE,
    PROP_VERSIFICATION,
    PROP_COPYRIGHT,
    PROP_DISTRIBUTION_LICENSE,
    PROP_ABOUT,
    PROP_CIPHERED,
    PROP_SUPPORTED,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE_WITH_PRIVATE(GswModuleInfo, gsw_module_info, G_TYPE_OBJECT)

static GswModuleInfoPrivate *
gsw_module_info_priv(GswModuleInfo *self)
{
    return gsw_module_info_get_instance_private(self);
}

static void
gsw_module_info_finalize(GObject *object)
{
    GswModuleInfo *self = GSW_MODULE_INFO(object);
    GswModuleInfoPrivate *priv = gsw_module_info_priv(self);

    g_free(priv->name);
    g_free(priv->description);
    g_free(priv->type_string);
    g_free(priv->driver_name);
    g_free(priv->language);
    g_free(priv->version);
    g_free(priv->minimum_version);
    g_free(priv->data_path);
    g_free(priv->absolute_data_path);
    g_free(priv->relative_data_dir);
    g_free(priv->prefix_path);
    g_free(priv->config_file);
    g_free(priv->encoding);
    g_free(priv->source_type);
    g_free(priv->versification);
    g_free(priv->copyright);
    g_free(priv->distribution_license);
    g_free(priv->about);
    g_clear_pointer(&priv->values, g_hash_table_unref);

    G_OBJECT_CLASS(gsw_module_info_parent_class)->finalize(object);
}

static void
gsw_module_info_get_property(GObject *object, guint prop_id,
                             GValue *value, GParamSpec *pspec)
{
    GswModuleInfo *self = GSW_MODULE_INFO(object);
    GswModuleInfoPrivate *priv = gsw_module_info_priv(self);

    switch (prop_id) {
    case PROP_NAME:               g_value_set_string(value, priv->name); break;
    case PROP_DESCRIPTION:        g_value_set_string(value, priv->description); break;
    case PROP_TYPE_STRING:        g_value_set_string(value, priv->type_string); break;
    case PROP_MODULE_TYPE:        g_value_set_enum(value, priv->module_type); break;
    case PROP_DRIVER:             g_value_set_enum(value, priv->driver); break;
    case PROP_DRIVER_NAME:        g_value_set_string(value, priv->driver_name); break;
    case PROP_LANGUAGE:           g_value_set_string(value, priv->language); break;
    case PROP_VERSION:            g_value_set_string(value, priv->version); break;
    case PROP_MINIMUM_VERSION:    g_value_set_string(value, priv->minimum_version); break;
    case PROP_INSTALL_SIZE:       g_value_set_uint64(value, priv->install_size); break;
    case PROP_DATA_PATH:          g_value_set_string(value, priv->data_path); break;
    case PROP_ABSOLUTE_DATA_PATH: g_value_set_string(value, priv->absolute_data_path); break;
    case PROP_RELATIVE_DATA_DIR:  g_value_set_string(value, priv->relative_data_dir); break;
    case PROP_PREFIX_PATH:        g_value_set_string(value, priv->prefix_path); break;
    case PROP_CONFIG_FILE:        g_value_set_string(value, priv->config_file); break;
    case PROP_ENCODING:           g_value_set_string(value, priv->encoding); break;
    case PROP_SOURCE_TYPE:        g_value_set_string(value, priv->source_type); break;
    case PROP_VERSIFICATION:      g_value_set_string(value, priv->versification); break;
    case PROP_COPYRIGHT:          g_value_set_string(value, priv->copyright); break;
    case PROP_DISTRIBUTION_LICENSE: g_value_set_string(value, priv->distribution_license); break;
    case PROP_ABOUT:              g_value_set_string(value, priv->about); break;
    case PROP_CIPHERED:           g_value_set_boolean(value, priv->ciphered); break;
    case PROP_SUPPORTED:          g_value_set_boolean(value, priv->supported); break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gsw_module_info_class_init(GswModuleInfoClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = gsw_module_info_finalize;
    object_class->get_property = gsw_module_info_get_property;

    properties[PROP_NAME] =
        g_param_spec_string("name", "Name", "Internal module name",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_DESCRIPTION] =
        g_param_spec_string("description", "Description", "Module description",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_TYPE_STRING] =
        g_param_spec_string("type-string", "Type string", "SWORD module type string",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_MODULE_TYPE] =
        g_param_spec_enum("module-type", "Module type", "Module category",
                          GSW_TYPE_MODULE_TYPE, GSW_MODULE_TYPE_UNKNOWN,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_DRIVER] =
        g_param_spec_enum("driver", "Driver", "Module storage driver",
                          GSW_TYPE_DRIVER, GSW_DRIVER_UNKNOWN,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_DRIVER_NAME] =
        g_param_spec_string("driver-name", "Driver name", "Module storage driver name",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_LANGUAGE] =
        g_param_spec_string("language", "Language", "Module language code",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_VERSION] =
        g_param_spec_string("version", "Version", "Module version",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_MINIMUM_VERSION] =
        g_param_spec_string("minimum-version", "Minimum version",
                            "Minimum SWORD version", NULL,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_INSTALL_SIZE] =
        g_param_spec_uint64("install-size", "Install size", "Installed size in bytes",
                            0, G_MAXUINT64, 0,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_DATA_PATH] =
        g_param_spec_string("data-path", "Data path", "DataPath from configuration",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_ABSOLUTE_DATA_PATH] =
        g_param_spec_string("absolute-data-path", "Absolute data path",
                            "Absolute data path", NULL,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_RELATIVE_DATA_DIR] =
        g_param_spec_string("relative-data-dir", "Relative data directory",
                            "Directory to copy when installing", NULL,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_PREFIX_PATH] =
        g_param_spec_string("prefix-path", "Prefix path", "Library prefix path",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_CONFIG_FILE] =
        g_param_spec_string("config-file", "Config file", "Configuration file path",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_ENCODING] =
        g_param_spec_string("encoding", "Encoding", "Text encoding",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_SOURCE_TYPE] =
        g_param_spec_string("source-type", "Source type", "Source markup type",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_VERSIFICATION] =
        g_param_spec_string("versification", "Versification",
                            "Versification system", NULL,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_COPYRIGHT] =
        g_param_spec_string("copyright", "Copyright", "Copyright holder",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_DISTRIBUTION_LICENSE] =
        g_param_spec_string("distribution-license", "Distribution license",
                            "Distribution license", NULL,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_ABOUT] =
        g_param_spec_string("about", "About", "Long description",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_CIPHERED] =
        g_param_spec_boolean("ciphered", "Ciphered", "Module is encrypted",
                             FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_SUPPORTED] =
        g_param_spec_boolean("supported", "Supported", "Module can be read",
                             FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties(object_class, N_PROPERTIES, properties);
}

static void
gsw_module_info_init(GswModuleInfo *self)
{
    GswModuleInfoPrivate *priv = gsw_module_info_priv(self);

    priv->values = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                         (GDestroyNotify)g_ptr_array_unref);
    priv->module_type = GSW_MODULE_TYPE_UNKNOWN;
    priv->driver = GSW_DRIVER_UNKNOWN;
}

static void
gsw_module_info_copy_values(GswModuleInfoPrivate *priv, GswConfig *config,
                            const gchar *name)
{
    gchar **keys;
    guint i;

    keys = gsw_config_get_keys(config, name);
    if (keys == NULL)
        return;

    for (i = 0; keys[i] != NULL; i++) {
        gchar **values = gsw_config_get_all(config, name, keys[i]);
        GPtrArray *array;

        if (values == NULL)
            continue;

        array = g_ptr_array_new_with_free_func(g_free);
        for (guint j = 0; values[j] != NULL; j++)
            g_ptr_array_add(array, g_strdup(values[j]));
        g_hash_table_insert(priv->values, g_strdup(keys[i]), array);
        g_strfreev(values);
    }
    g_strfreev(keys);
}

static const gchar *
gsw_module_info_value(GswModuleInfoPrivate *priv, const gchar *key)
{
    GPtrArray *values = g_hash_table_lookup(priv->values, key);

    if (values == NULL || values->len == 0)
        return NULL;
    return g_ptr_array_index(values, 0);
}

static gboolean
gsw_module_info_compression_supported(const gchar *compression)
{
    if (compression == NULL || *compression == '\0')
        return TRUE;
    if (g_ascii_strcasecmp(compression, "LZSS") == 0)
        return TRUE;
    if (g_ascii_strcasecmp(compression, "ZIP") == 0)
        return TRUE;
    if (g_ascii_strcasecmp(compression, "BZIP2") == 0) {
#ifdef HAVE_BZIP2
        return TRUE;
#else
        return FALSE;
#endif
    }
    if (g_ascii_strcasecmp(compression, "XZ") == 0) {
#ifdef HAVE_LZMA
        return TRUE;
#else
        return FALSE;
#endif
    }
    return FALSE;
}

GswModuleInfo *
gsw_module_info_new_from_config(const gchar *name, GswConfig *config,
                                const gchar *config_file,
                                const gchar *prefix_path,
                                GError **error)
{
    GswModuleInfo *self;
    GswModuleInfoPrivate *priv;
    const gchar *driver_name;
    const gchar *data_path;
    const gchar *type_string;
    gchar *prefix;

    g_return_val_if_fail(GSW_IS_CONFIG(config), NULL);
    g_return_val_if_fail(name != NULL, NULL);

    if (!gsw_config_has_section(config, name)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "Module section [%s] not found", name);
        return NULL;
    }

    self = g_object_new(GSW_TYPE_MODULE_INFO, NULL);
    priv = gsw_module_info_priv(self);

    priv->name = g_strdup(name);
    gsw_module_info_copy_values(priv, config, name);

    driver_name = gsw_module_info_value(priv, "ModDrv");
    priv->driver = gsw_driver_from_string(driver_name);
    priv->driver_name = g_strdup(driver_name != NULL ? driver_name : "");

    type_string = gsw_module_info_value(priv, "Type");
    if (type_string != NULL && *type_string != '\0')
        priv->module_type = gsw_module_type_from_string(type_string);
    else
        priv->module_type = gsw_module_type_default_for_driver(priv->driver);

    priv->type_string = g_strdup(type_string != NULL && *type_string != '\0'
                                 ? type_string
                                 : gsw_module_type_to_string(priv->module_type));

    data_path = gsw_module_info_value(priv, "DataPath");
    priv->data_path = g_strdup(data_path != NULL ? data_path : "");
    while (g_str_has_prefix(priv->data_path, "./"))
        memmove(priv->data_path, priv->data_path + 2,
                strlen(priv->data_path + 2) + 1);

    prefix = gsw_str_remove_trailing_slashes(prefix_path != NULL ? prefix_path : "");
    if (prefix == NULL || *prefix == '\0') {
        g_free(prefix);
        prefix = g_strdup(".");
    }
    {
        gchar *absolute_prefix = g_canonicalize_filename(prefix, NULL);
        g_free(prefix);
        prefix = absolute_prefix;
    }
    priv->prefix_path = g_strconcat(prefix, G_DIR_SEPARATOR_S, NULL);

    if (g_path_is_absolute(priv->data_path)) {
        priv->relative_data_dir = g_strdup("");
        priv->absolute_data_path = g_canonicalize_filename(priv->data_path, NULL);
    } else {
        if (priv->driver == GSW_DRIVER_RAW_LD ||
            priv->driver == GSW_DRIVER_RAW_LD4 ||
            priv->driver == GSW_DRIVER_Z_LD ||
            priv->driver == GSW_DRIVER_RAW_GENBOOK) {
            gchar *dir = g_path_get_dirname(priv->data_path);
            if (g_strcmp0(dir, ".") == 0)
                priv->relative_data_dir = g_strdup("");
            else
                priv->relative_data_dir = g_strdup(dir);
            g_free(dir);
        } else {
            priv->relative_data_dir = g_strdup(priv->data_path);
        }

        if (priv->relative_data_dir[0] != '\0')
            priv->absolute_data_path = gsw_path_join_under(prefix, priv->relative_data_dir, error);
        else
            priv->absolute_data_path = g_strdup(prefix);

        if (priv->absolute_data_path == NULL) {
            g_free(prefix);
            g_object_unref(self);
            return NULL;
        }
    }
    g_free(prefix);

    priv->config_file = g_strdup(config_file != NULL ? config_file : "");
    priv->description = g_strdup(gsw_module_info_value(priv, "Description"));
    if (priv->description == NULL)
        priv->description = g_strdup(priv->name);
    priv->language = g_strdup(gsw_module_info_value(priv, "Lang"));
    if (priv->language == NULL)
        priv->language = g_strdup("en");
    priv->version = g_strdup(gsw_module_info_value(priv, "Version"));
    priv->minimum_version = g_strdup(gsw_module_info_value(priv, "MinimumVersion"));
    priv->encoding = g_strdup(gsw_module_info_value(priv, "Encoding"));
    priv->source_type = g_strdup(gsw_module_info_value(priv, "SourceType"));
    priv->versification = g_strdup(gsw_module_info_value(priv, "Versification"));
    priv->copyright = g_strdup(gsw_module_info_value(priv, "CopyrightHolder"));
    priv->distribution_license = g_strdup(gsw_module_info_value(priv, "DistributionLicense"));
    priv->about = g_strdup(gsw_module_info_value(priv, "About"));
    priv->install_size = gsw_str_parse_u64(gsw_module_info_value(priv, "InstallSize"), 0);

    if (priv->description == NULL) priv->description = g_strdup("");
    if (priv->language == NULL) priv->language = g_strdup("en");
    if (priv->version == NULL) priv->version = g_strdup("");
    if (priv->minimum_version == NULL) priv->minimum_version = g_strdup("");
    if (priv->encoding == NULL) priv->encoding = g_strdup("");
    if (priv->source_type == NULL) priv->source_type = g_strdup("");
    if (priv->versification == NULL) priv->versification = g_strdup("KJV");
    if (priv->copyright == NULL) priv->copyright = g_strdup("");
    if (priv->distribution_license == NULL) priv->distribution_license = g_strdup("");
    if (priv->about == NULL) priv->about = g_strdup("");

    priv->ciphered = g_hash_table_contains(priv->values, "CipherKey");
    priv->supported = gsw_driver_is_supported(priv->driver);
    if (priv->supported &&
        (priv->driver == GSW_DRIVER_Z_TEXT ||
         priv->driver == GSW_DRIVER_Z_TEXT4 ||
         priv->driver == GSW_DRIVER_Z_COM ||
         priv->driver == GSW_DRIVER_Z_COM4 ||
         priv->driver == GSW_DRIVER_Z_LD)) {
        priv->supported = gsw_module_info_compression_supported(
            gsw_module_info_value(priv, "CompressType"));
    }

    return self;
}

const gchar *
gsw_module_info_get_name(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->name;
}

const gchar *
gsw_module_info_get_description(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->description;
}

const gchar *
gsw_module_info_get_type_string(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->type_string;
}

GswModuleType
gsw_module_info_get_module_type(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->module_type;
}

GswDriver
gsw_module_info_get_driver(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->driver;
}

const gchar *
gsw_module_info_get_driver_name(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->driver_name;
}

const gchar *
gsw_module_info_get_language(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->language;
}

const gchar *
gsw_module_info_get_version(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->version;
}

const gchar *
gsw_module_info_get_minimum_version(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->minimum_version;
}

guint64
gsw_module_info_get_install_size(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->install_size;
}

const gchar *
gsw_module_info_get_data_path(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->data_path;
}

const gchar *
gsw_module_info_get_absolute_data_path(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->absolute_data_path;
}

const gchar *
gsw_module_info_get_relative_data_dir(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->relative_data_dir;
}

const gchar *
gsw_module_info_get_prefix_path(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->prefix_path;
}

const gchar *
gsw_module_info_get_config_file(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->config_file;
}

const gchar *
gsw_module_info_get_encoding(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->encoding;
}

const gchar *
gsw_module_info_get_source_type(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->source_type;
}

const gchar *
gsw_module_info_get_versification(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->versification;
}

const gchar *
gsw_module_info_get_copyright(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->copyright;
}

const gchar *
gsw_module_info_get_distribution_license(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->distribution_license;
}

const gchar *
gsw_module_info_get_about(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->about;
}

gboolean
gsw_module_info_is_ciphered(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->ciphered;
}

gboolean
gsw_module_info_is_supported(GswModuleInfo *self)
{
    return gsw_module_info_priv(self)->supported;
}

const gchar *
gsw_module_info_get_value(GswModuleInfo *self, const gchar *key)
{
    GswModuleInfoPrivate *priv;

    g_return_val_if_fail(GSW_IS_MODULE_INFO(self), NULL);
    g_return_val_if_fail(key != NULL, NULL);

    priv = gsw_module_info_priv(self);
    return gsw_module_info_value(priv, key);
}

gchar **
gsw_module_info_get_keys(GswModuleInfo *self)
{
    GswModuleInfoPrivate *priv;
    GList *keys;
    GList *item;
    gchar **result;
    guint i;

    g_return_val_if_fail(GSW_IS_MODULE_INFO(self), NULL);

    priv = gsw_module_info_priv(self);
    keys = g_hash_table_get_keys(priv->values);
    result = g_new0(gchar *, g_list_length(keys) + 1);
    for (item = keys, i = 0; item != NULL; item = item->next, i++)
        result[i] = g_strdup(item->data);
    g_list_free(keys);
    return result;
}

gchar **
gsw_module_info_get_values(GswModuleInfo *self, const gchar *key)
{
    GswModuleInfoPrivate *priv;
    GPtrArray *values;
    gchar **result;
    guint i;

    g_return_val_if_fail(GSW_IS_MODULE_INFO(self), NULL);
    g_return_val_if_fail(key != NULL, NULL);

    priv = gsw_module_info_priv(self);
    values = g_hash_table_lookup(priv->values, key);
    if (values == NULL)
        return NULL;

    result = g_new0(gchar *, values->len + 1);
    for (i = 0; i < values->len; i++)
        result[i] = g_strdup(g_ptr_array_index(values, i));
    return result;
}
