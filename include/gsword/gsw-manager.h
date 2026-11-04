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
 * Discovers and opens a system SWORD module library. It checks the current
 * directory, `SWORD_PATH`, `sword.conf`, and the `sword` directory below the
 * XDG system data directories. It does not use `~/.sword` or the gsword
 * application library; applications should call gsw_manager_new_for_path()
 * with their own writable library path.
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
 * Adds a directory to load after the primary library. Modules from augment
 * paths override modules with the same name from the primary library. This
 * matches SWORD augment path behavior.
 */
void         gsw_manager_add_augment_path(GswManager *self, const gchar *path);
/**
 * gsw_manager_add_fallback_path:
 * @self: a #GswManager
 * @path: directory containing read-only fallback module configuration files
 *
 * Adds a directory to load before the primary library. The primary library
 * overrides modules with the same name from a fallback path. Earlier fallback
 * paths take precedence over later ones. This is intended for system-wide
 * read-only module libraries.
 */
void         gsw_manager_add_fallback_path(GswManager *self,
                                            const gchar *path);
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
 * gsw_manager_create_module:
 * @self: a #GswManager
 * @name: module name
 *
 * Creates a new module instance that is not stored in the manager's module
 * cache. Use this when the same module must be displayed in several views at
 * different positions.
 *
 * Returns: (transfer full): a new #GswModule, or %NULL if @name is not found
 */
GswModule     *gsw_manager_create_module(GswManager *self, const gchar *name);
/**
 * gsw_manager_create_genbook:
 * @self: a #GswManager
 * @name: new module name
 * @description: module description
 * @error: return location for a #GError, or %NULL
 *
 * Creates an empty RawGenBook module and its configuration file, then reloads
 * the module library. The new module has an empty root node.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean       gsw_manager_create_genbook(GswManager *self, const gchar *name,
                                          const gchar *description,
                                          GError **error);
/**
 * gsw_manager_set_option:
 * @self: a #GswManager
 * @name: render option name
 * @value: option value
 *
 * Sets a global render option default. Existing modules that have not
 * explicitly overridden @name are updated immediately; modules created later
 * inherit the new value.
 *
 * Returns: %TRUE if the option is known, %FALSE otherwise
 */
gboolean       gsw_manager_set_option(GswManager *self, const gchar *name,
                                      const gchar *value);
/**
 * gsw_manager_get_option:
 * @self: a #GswManager
 * @name: render option name
 *
 * Gets the global value of a render option.
 *
 * Returns: (transfer none): the option value, or %NULL if unknown
 */
const gchar   *gsw_manager_get_option(GswManager *self, const gchar *name);
/**
 * gsw_manager_get_options:
 * @self: a #GswManager
 *
 * Gets all known global render option names.
 *
 * Returns: (transfer container) (element-type utf8): an array of option names
 */
GPtrArray     *gsw_manager_get_options(GswManager *self);
/**
 * gsw_manager_get_option_values:
 * @self: a #GswManager
 * @name: render option name
 *
 * Gets the allowed values for a global render option.
 *
 * Returns: (transfer container) (element-type utf8): an array of allowed
 *   values, or %NULL if the option is unknown
 */
GPtrArray     *gsw_manager_get_option_values(GswManager *self,
                                              const gchar *name);
/**
 * gsw_manager_load_render_options:
 * @self: a #GswManager
 * @path: path to a render option configuration file
 * @error: return location for a #GError, or %NULL
 *
 * Loads global render options from the `RenderOptions` section of an INI file.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean       gsw_manager_load_render_options(GswManager *self,
                                               const gchar *path,
                                               GError **error);
/**
 * gsw_manager_save_render_options:
 * @self: a #GswManager
 * @path: path to a render option configuration file
 * @error: return location for a #GError, or %NULL
 *
 * Saves global render options to the `RenderOptions` section of an INI file.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean       gsw_manager_save_render_options(GswManager *self,
                                               const gchar *path,
                                               GError **error);
/**
 * gsw_manager_set_module_config_value:
 * @self: a #GswManager
 * @name: module name
 * @key: configuration key
 * @value: (nullable): new value, or %NULL to remove the key
 * @error: return location for a #GError, or %NULL
 *
 * Updates a value in the module's configuration file and reloads the module
 * library.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean       gsw_manager_set_module_config_value(GswManager *self,
                                                   const gchar *name,
                                                   const gchar *key,
                                                   const gchar *value,
                                                   GError **error);
/**
 * gsw_manager_set_module_cipher_key:
 * @self: a #GswManager
 * @name: module name
 * @key: (nullable): cipher key, or %NULL to remove it
 * @error: return location for a #GError, or %NULL
 *
 * Updates the module's `CipherKey`, saves its configuration file, and reloads
 * the module library. Existing #GswModule instances are invalidated.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean       gsw_manager_set_module_cipher_key(GswManager *self,
                                                 const gchar *name,
                                                 const gchar *key,
                                                 GError **error);
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
