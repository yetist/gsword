/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-config.h: SWORD-style INI configuration
 *
 * This file is part of gsword.
 */

#ifndef __GSW_CONFIG_H__
#define __GSW_CONFIG_H__ 1

#include <glib-object.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

/**
 * GswConfig:
 *
 * A SWORD-style INI configuration file. Values are grouped into sections and
 * keys; a key may have multiple values. The object preserves definition order
 * for sections and keys.
 */
#define GSW_TYPE_CONFIG (gsw_config_get_type())
G_DECLARE_FINAL_TYPE(GswConfig, gsw_config, GSW, CONFIG, GObject)

/**
 * gsw_config_new:
 *
 * Creates a new, empty configuration object.
 *
 * Returns: (transfer full): a new empty #GswConfig
 */
GswConfig *gsw_config_new(void);
/**
 * gsw_config_new_from_file:
 * @path: path to a configuration file
 * @error: return location for a #GError, or %NULL
 *
 * Creates a configuration object by loading a configuration file.
 *
 * Returns: (transfer full): a new #GswConfig, or %NULL on error
 */
GswConfig *gsw_config_new_from_file(const gchar *path, GError **error);
/**
 * gsw_config_new_from_data:
 * @data: configuration data to parse
 * @len: length of @data in bytes
 * @error: return location for a #GError, or %NULL
 *
 * Creates a configuration object from in-memory configuration data.
 *
 * Returns: (transfer full): a new #GswConfig, or %NULL on error
 */
GswConfig *gsw_config_new_from_data(const gchar *data, gssize len, GError **error);
/**
 * gsw_config_copy:
 * @self: a #GswConfig
 *
 * Creates a deep copy of @self.
 *
 * Returns: (transfer full): a copy of @self
 */
GswConfig *gsw_config_copy(GswConfig *self);

/**
 * gsw_config_load:
 * @self: a #GswConfig
 * @path: path to a configuration file
 * @error: return location for a #GError, or %NULL
 *
 * Loads configuration data from @path, replacing the current contents.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean gsw_config_load(GswConfig *self, const gchar *path, GError **error);
/**
 * gsw_config_save:
 * @self: a #GswConfig
 * @path: path to a configuration file
 * @error: return location for a #GError, or %NULL
 *
 * Saves the configuration to @path in SWORD INI format.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean gsw_config_save(GswConfig *self, const gchar *path, GError **error);

/**
 * gsw_config_get_path:
 * @self: a #GswConfig
 *
 * Gets the file path associated with @self.
 *
 * Returns: (transfer none): the path associated with @self, or %NULL
 */
const gchar *gsw_config_get_path(GswConfig *self);

/**
 * gsw_config_has_section:
 * @self: a #GswConfig
 * @section: configuration section name
 *
 * Checks whether @section exists in @self.
 *
 * Returns: %TRUE if the condition holds, %FALSE otherwise
 */
gboolean     gsw_config_has_section(GswConfig *self, const gchar *section);
/**
 * gsw_config_get_sections:
 * @self: a #GswConfig
 *
 * Gets all section names in definition order.
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8): a newly allocated %NULL-terminated array of strings
 */
gchar      **gsw_config_get_sections(GswConfig *self);
/**
 * gsw_config_remove_section:
 * @self: a #GswConfig
 * @section: configuration section name
 *
 * Removes @section and all of its keys.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_config_remove_section(GswConfig *self, const gchar *section);

/**
 * gsw_config_has_key:
 * @self: a #GswConfig
 * @section: configuration section name
 * @key: key name
 *
 * Checks whether @key exists in @section.
 *
 * Returns: %TRUE if the condition holds, %FALSE otherwise
 */
gboolean     gsw_config_has_key(GswConfig *self, const gchar *section, const gchar *key);
/**
 * gsw_config_get:
 * @self: a #GswConfig
 * @section: configuration section name
 * @key: key name
 *
 * Gets the first value for @key in @section.
 *
 * Returns: (transfer none): the value, or %NULL if not found
 */
const gchar *gsw_config_get(GswConfig *self, const gchar *section, const gchar *key);
/**
 * gsw_config_get_all:
 * @self: a #GswConfig
 * @section: configuration section name
 * @key: key name
 *
 * Gets all values for a multi-valued key.
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8): a newly allocated %NULL-terminated array of strings
 */
gchar      **gsw_config_get_all(GswConfig *self, const gchar *section, const gchar *key);
/**
 * gsw_config_get_keys:
 * @self: a #GswConfig
 * @section: configuration section name
 *
 * Gets all key names in @section.
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8): a newly allocated %NULL-terminated array of strings
 */
gchar      **gsw_config_get_keys(GswConfig *self, const gchar *section);

/**
 * gsw_config_set:
 * @self: a #GswConfig
 * @section: configuration section name
 * @key: key name
 * @value: value to store
 *
 * Sets a single value for @key in @section.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_config_set(GswConfig *self, const gchar *section,
                            const gchar *key, const gchar *value);
/**
 * gsw_config_add:
 * @self: a #GswConfig
 * @section: configuration section name
 * @key: key name
 * @value: value to store
 *
 * Appends a value to the multi-valued key @section/@key.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_config_add(GswConfig *self, const gchar *section,
                            const gchar *key, const gchar *value);
/**
 * gsw_config_remove_key:
 * @self: a #GswConfig
 * @section: configuration section name
 * @key: key name
 *
 * Removes @key from @section.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_config_remove_key(GswConfig *self, const gchar *section, const gchar *key);

/**
 * gsw_config_augment:
 * @self: a #GswConfig
 * @other: another #GswConfig to merge
 *
 * Merges @other into @self, preserving multi-valued keys.
 */
void         gsw_config_augment(GswConfig *self, GswConfig *other);

G_END_DECLS

#endif /* __GSW_CONFIG_H__ */
