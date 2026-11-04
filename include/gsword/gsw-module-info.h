/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-module-info.h: immutable module metadata
 *
 * This file is part of gsword.
 */

#ifndef __GSW_MODULE_INFO_H__
#define __GSW_MODULE_INFO_H__ 1

#include <glib-object.h>
#include <gsword/gsw-config.h>
#include <gsword/gsw-enums.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

/**
 * GswModuleInfo:
 *
 * Immutable metadata parsed from a module configuration section. It exposes
 * the module name, description, type, driver, paths, encoding, and raw
 * configuration keys and values.
 */
#define GSW_TYPE_MODULE_INFO (gsw_module_info_get_type())
G_DECLARE_FINAL_TYPE(GswModuleInfo, gsw_module_info, GSW, MODULE_INFO, GObject)

/**
 * gsw_module_info_new_from_config:
 * @name: module name
 * @config: a #GswConfig containing the module section
 * @config_file: configuration file that defines the module, or %NULL
 * @prefix_path: prefix path of the module library
 * @error: return location for a #GError, or %NULL
 *
 * Creates module metadata from a module configuration section.
 *
 * Returns: (transfer full): a new #GswModuleInfo, or %NULL on error
 */
GswModuleInfo *gsw_module_info_new_from_config(const gchar *name,
                                               GswConfig *config,
                                               const gchar *config_file,
                                               const gchar *prefix_path,
                                               GError **error);

/**
 * gsw_module_info_get_name:
 * @self: a #GswModuleInfo
 *
 * Gets the module name.
 *
 * Returns: (transfer none): the module name
 */
const gchar   *gsw_module_info_get_name(GswModuleInfo *self);
/**
 * gsw_module_info_get_description:
 * @self: a #GswModuleInfo
 *
 * Gets the module description.
 *
 * Returns: (transfer none): the module description
 */
const gchar   *gsw_module_info_get_description(GswModuleInfo *self);
/**
 * gsw_module_info_get_type_string:
 * @self: a #GswModuleInfo
 *
 * Gets the SWORD type string, such as <literal>Biblical Texts</literal>.
 *
 * Returns: (transfer none): the SWORD type string
 */
const gchar   *gsw_module_info_get_type_string(GswModuleInfo *self);
/**
 * gsw_module_info_get_module_type:
 * @self: a #GswModuleInfo
 *
 * Gets the parsed module type.
 *
 * Returns: the module type
 */
GswModuleType  gsw_module_info_get_module_type(GswModuleInfo *self);
/**
 * gsw_module_info_get_driver:
 * @self: a #GswModuleInfo
 *
 * Gets the parsed storage driver.
 *
 * Returns: the storage driver
 */
GswDriver      gsw_module_info_get_driver(GswModuleInfo *self);
/**
 * gsw_module_info_get_driver_name:
 * @self: a #GswModuleInfo
 *
 * Gets the SWORD driver name.
 *
 * Returns: (transfer none): the SWORD driver name
 */
const gchar   *gsw_module_info_get_driver_name(GswModuleInfo *self);
/**
 * gsw_module_info_get_language:
 * @self: a #GswModuleInfo
 *
 * Gets the module language code.
 *
 * Returns: (transfer none): the language code
 */
const gchar   *gsw_module_info_get_language(GswModuleInfo *self);
/**
 * gsw_module_info_get_version:
 * @self: a #GswModuleInfo
 *
 * Gets the module version string.
 *
 * Returns: (transfer none): the version string
 */
const gchar   *gsw_module_info_get_version(GswModuleInfo *self);
/**
 * gsw_module_info_get_minimum_version:
 * @self: a #GswModuleInfo
 *
 * Gets the minimum SWORD version required by the module.
 *
 * Returns: (transfer none): the minimum SWORD version
 */
const gchar   *gsw_module_info_get_minimum_version(GswModuleInfo *self);
/**
 * gsw_module_info_get_install_size:
 * @self: a #GswModuleInfo
 *
 * Gets the installed size in bytes.
 *
 * Returns: the installed size in bytes
 */
guint64        gsw_module_info_get_install_size(GswModuleInfo *self);
/**
 * gsw_module_info_get_data_path:
 * @self: a #GswModuleInfo
 *
 * Gets the path to the module data file or directory.
 *
 * Returns: (transfer none): the module data path
 */
const gchar   *gsw_module_info_get_data_path(GswModuleInfo *self);
/**
 * gsw_module_info_get_absolute_data_path:
 * @self: a #GswModuleInfo
 *
 * Gets the absolute path to the module data.
 *
 * Returns: (transfer none): the absolute module data path
 */
const gchar   *gsw_module_info_get_absolute_data_path(GswModuleInfo *self);
/**
 * gsw_module_info_get_relative_data_dir:
 * @self: a #GswModuleInfo
 *
 * Gets the module data directory relative to the library prefix.
 *
 * Returns: (transfer none): the relative module data directory
 */
const gchar   *gsw_module_info_get_relative_data_dir(GswModuleInfo *self);
/**
 * gsw_module_info_get_prefix_path:
 * @self: a #GswModuleInfo
 *
 * Gets the module library prefix path.
 *
 * Returns: (transfer none): the module library prefix path
 */
const gchar   *gsw_module_info_get_prefix_path(GswModuleInfo *self);
/**
 * gsw_module_info_get_config_file:
 * @self: a #GswModuleInfo
 *
 * Gets the configuration file that defines the module.
 *
 * Returns: (transfer none): the module configuration file
 */
const gchar   *gsw_module_info_get_config_file(GswModuleInfo *self);
/**
 * gsw_module_info_get_encoding:
 * @self: a #GswModuleInfo
 *
 * Gets the declared module encoding.
 *
 * Returns: (transfer none): the declared encoding
 */
const gchar   *gsw_module_info_get_encoding(GswModuleInfo *self);
/**
 * gsw_module_info_get_source_type:
 * @self: a #GswModuleInfo
 *
 * Gets the declared module source type.
 *
 * Returns: (transfer none): the declared source type
 */
const gchar   *gsw_module_info_get_source_type(GswModuleInfo *self);
/**
 * gsw_module_info_get_versification:
 * @self: a #GswModuleInfo
 *
 * Gets the versification system name.
 *
 * Returns: (transfer none): the versification system name
 */
const gchar   *gsw_module_info_get_versification(GswModuleInfo *self);
/**
 * gsw_module_info_get_copyright:
 * @self: a #GswModuleInfo
 *
 * Gets the copyright notice.
 *
 * Returns: (transfer none): the copyright notice
 */
const gchar   *gsw_module_info_get_copyright(GswModuleInfo *self);
/**
 * gsw_module_info_get_distribution_license:
 * @self: a #GswModuleInfo
 *
 * Gets the distribution license.
 *
 * Returns: (transfer none): the distribution license
 */
const gchar   *gsw_module_info_get_distribution_license(GswModuleInfo *self);
/**
 * gsw_module_info_get_about:
 * @self: a #GswModuleInfo
 *
 * Gets the module description shown in about dialogs.
 *
 * Returns: (transfer none): the about text
 */
const gchar   *gsw_module_info_get_about(GswModuleInfo *self);
/**
 * gsw_module_info_is_ciphered:
 * @self: a #GswModuleInfo
 *
 * Checks whether the module data is encrypted.
 *
 * Returns: %TRUE if the condition holds, %FALSE otherwise
 */
gboolean       gsw_module_info_is_ciphered(GswModuleInfo *self);
/**
 * gsw_module_info_is_supported:
 * @self: a #GswModuleInfo
 *
 * Checks whether gsword can read the module type and driver.
 *
 * Returns: %TRUE if the condition holds, %FALSE otherwise
 */
gboolean       gsw_module_info_is_supported(GswModuleInfo *self);

/**
 * gsw_module_info_get_value:
 * @self: a #GswModuleInfo
 * @key: configuration key name
 *
 * Gets a raw configuration value from the module metadata.
 *
 * Returns: (transfer none): the value for @key, or %NULL if not found
 */
const gchar   *gsw_module_info_get_value(GswModuleInfo *self, const gchar *key);
/**
 * gsw_module_info_get_keys:
 * @self: a #GswModuleInfo
 *
 * Gets all configuration keys present in the module metadata.
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8): a newly allocated %NULL-terminated array of strings
 */
gchar        **gsw_module_info_get_keys(GswModuleInfo *self);
/**
 * gsw_module_info_get_values:
 * @self: a #GswModuleInfo
 * @key: configuration key name
 *
 * Gets all values for a configuration key in the module metadata.
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8): a newly allocated %NULL-terminated array of strings
 */
gchar        **gsw_module_info_get_values(GswModuleInfo *self, const gchar *key);

G_END_DECLS

#endif /* __GSW_MODULE_INFO_H__ */
