/*
 * gsw-manager.h: an installed SWORD module library
 *
 * This file is part of gsword.
 */

#ifndef __GSW_MANAGER_H__
#define __GSW_MANAGER_H__ 1

#include <glib-object.h>
#include <gsword/gsw-config.h>
#include <gsword/gsw-module-info.h>
#include <gsword/gsw-module.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

/**
 * GswManager:
 *
 * An installed SWORD module library. It merges <filename>mods.conf</filename>
 * files from <filename>mods.d/</filename> and optional augmentation paths,
 * provides module metadata, and creates lazily opened #GswModule readers.
 */
#define GSW_TYPE_MANAGER (gsw_manager_get_type())
G_DECLARE_FINAL_TYPE(GswManager, gsw_manager, GSW, MANAGER, GObject)

/**
 * gsw_manager_new:
 * @error: return location for a #GError, or %NULL
 *
 * Discovers and opens the default SWORD module library.
 *
 * Returns: (transfer full): a new #GswManager, or %NULL on error
 */
GswManager *gsw_manager_new(GError **error);
/**
 * gsw_manager_new_for_path:
 * @prefix_path: prefix path of the module library
 * @error: return location for a #GError, or %NULL
 *
 * Opens the module library rooted at @prefix_path.
 *
 * Returns: (transfer full): a new #GswManager, or %NULL on error
 */
GswManager *gsw_manager_new_for_path(const gchar *prefix_path, GError **error);
/**
 * gsw_manager_new_for_config:
 * @config_path: path to the module configuration file
 * @error: return location for a #GError, or %NULL
 *
 * Opens the module library described by @config_path.
 *
 * Returns: (transfer full): a new #GswManager, or %NULL on error
 */
GswManager *gsw_manager_new_for_config(const gchar *config_path, GError **error);

/**
 * gsw_manager_load:
 * @self: a #GswManager
 * @error: return location for a #GError, or %NULL
 *
 * Loads module configuration and metadata.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean gsw_manager_load(GswManager *self, GError **error);
/**
 * gsw_manager_reload:
 * @self: a #GswManager
 * @error: return location for a #GError, or %NULL
 *
 * Reloads module configuration and invalidates cached modules.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean gsw_manager_reload(GswManager *self, GError **error);

/**
 * gsw_manager_add_augment_path:
 * @self: a #GswManager
 * @path: directory containing additional module configuration files
 *
 * Adds a directory to load additional module configuration from.
 */
void         gsw_manager_add_augment_path(GswManager *self, const gchar *path);
/**
 * gsw_manager_get_prefix_path:
 * @self: a #GswManager
 *
 * Gets the prefix path of the module library.
 *
 * Returns: (transfer none): the module library prefix path
 */
const gchar *gsw_manager_get_prefix_path(GswManager *self);
/**
 * gsw_manager_get_config_path:
 * @self: a #GswManager
 *
 * Gets the primary module configuration path.
 *
 * Returns: (transfer none): the module configuration path
 */
const gchar *gsw_manager_get_config_path(GswManager *self);

/**
 * gsw_manager_get_module_infos:
 * @self: a #GswManager
 *
 * Gets metadata for all available modules.
 *
 * Returns: (transfer container) (element-type GswModuleInfo): a newly allocated array of #GswModuleInfo
 */
GPtrArray     *gsw_manager_get_module_infos(GswManager *self);
/**
 * gsw_manager_lookup_module_info:
 * @self: a #GswManager
 * @name: module name
 *
 * Looks up metadata for a module by name.
 *
 * Returns: (transfer none): the module metadata, or %NULL if not found
 */
GswModuleInfo *gsw_manager_lookup_module_info(GswManager *self, const gchar *name);
/**
 * gsw_manager_has_module:
 * @self: a #GswManager
 * @name: module name
 *
 * Checks whether a module is available.
 *
 * Returns: %TRUE if the condition holds, %FALSE otherwise
 */
gboolean       gsw_manager_has_module(GswManager *self, const gchar *name);

/**
 * gsw_manager_get_module:
 * @self: a #GswManager
 * @name: module name
 *
 * Gets a module, opening its storage backend lazily.
 *
 * Returns: (transfer none): the module, or %NULL if not found
 */
GswModule     *gsw_manager_get_module(GswManager *self, const gchar *name);
/**
 * gsw_manager_get_config:
 * @self: a #GswManager
 *
 * Gets the merged module configuration.
 *
 * Returns: (transfer none): the merged module configuration
 */
GswConfig     *gsw_manager_get_config(GswManager *self);
/**
 * gsw_manager_get_config_file_for_module:
 * @self: a #GswManager
 * @name: module name
 *
 * Gets the configuration file that defines a module.
 *
 * Returns: (transfer none): the configuration file that defines @name, or %NULL
 */
const gchar   *gsw_manager_get_config_file_for_module(GswManager *self,
                                                      const gchar *name);

G_END_DECLS

#endif /* __GSW_MANAGER_H__ */
