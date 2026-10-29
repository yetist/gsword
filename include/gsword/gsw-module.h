/*
 * gsw-module.h: a readable SWORD module
 *
 * This file is part of gsword.
 */

#ifndef __GSW_MODULE_H__
#define __GSW_MODULE_H__ 1

#include <gio/gio.h>
#include <gsword/gsw-key.h>
#include <gsword/gsw-module-info.h>
#include <gsword/gsw-verse-key.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

/**
 * GswModule:
 *
 * A readable SWORD module. A module owns a key and opens its storage backend
 * lazily on first access. Text can be read as raw bytes, plain text, or
 * rendered text with options such as footnotes and strong's markup enabled.
 */
#define GSW_TYPE_MODULE (gsw_module_get_type())
G_DECLARE_FINAL_TYPE(GswModule, gsw_module, GSW, MODULE, GObject)

/**
 * gsw_module_get_info:
 * @self: a #GswModule
 *
 * Gets the metadata for @self.
 *
 * Returns: (transfer none): the module metadata
 */
GswModuleInfo *gsw_module_get_info(GswModule *self);
/**
 * gsw_module_get_name:
 * @self: a #GswModule
 *
 * Gets the module name.
 *
 * Returns: (transfer none): the module name
 */
const gchar   *gsw_module_get_name(GswModule *self);
/**
 * gsw_module_is_open:
 * @self: a #GswModule
 *
 * Checks whether the module storage backend is open.
 *
 * Returns: %TRUE if the condition holds, %FALSE otherwise
 */
gboolean       gsw_module_is_open(GswModule *self);
/**
 * gsw_module_is_supported:
 * @self: a #GswModule
 *
 * Checks whether the module driver is supported.
 *
 * Returns: %TRUE if the condition holds, %FALSE otherwise
 */
gboolean       gsw_module_is_supported(GswModule *self);

/**
 * gsw_module_create_key:
 * @self: a #GswModule
 *
 * Creates a new key compatible with this module.
 *
 * Returns: (transfer full): a new #GswKey
 */
GswKey        *gsw_module_create_key(GswModule *self);
/**
 * gsw_module_get_key:
 * @self: a #GswModule
 *
 * Gets the current key.
 *
 * Returns: (transfer none): the current key
 */
GswKey        *gsw_module_get_key(GswModule *self);
/**
 * gsw_module_set_key:
 * @self: a #GswModule
 * @key: a #GswKey
 * @error: return location for a #GError, or %NULL
 *
 * Sets the current key from another key.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean       gsw_module_set_key(GswModule *self, GswKey *key, GError **error);
/**
 * gsw_module_set_key_text:
 * @self: a #GswModule
 * @text: key text
 * @error: return location for a #GError, or %NULL
 *
 * Sets the current key from text.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean       gsw_module_set_key_text(GswModule *self, const gchar *text,
                                       GError **error);
/**
 * gsw_module_get_key_text:
 * @self: a #GswModule
 *
 * Gets the current key as text.
 *
 * Returns: (transfer none): the current key as text
 */
const gchar   *gsw_module_get_key_text(GswModule *self);

/**
 * gsw_module_read_raw:
 * @self: a #GswModule
 * @error: return location for a #GError, or %NULL
 *
 * Reads the current entry as raw bytes.
 *
 * Returns: (transfer full): a #GBytes containing the raw entry, or %NULL on error
 */
GBytes        *gsw_module_read_raw(GswModule *self, GError **error);
/**
 * gsw_module_peek_raw:
 * @self: a #GswModule
 * @error: return location for a #GError, or %NULL
 *
 * Gets the current raw entry as a borrowed string.
 *
 * Returns: (transfer none): the current raw entry, or %NULL on error
 */
const gchar   *gsw_module_peek_raw(GswModule *self, GError **error);
/**
 * gsw_module_get_text:
 * @self: a #GswModule
 * @error: return location for a #GError, or %NULL
 *
 * Gets the current entry as rendered plain text.
 *
 * Returns: (transfer full): the entry as plain UTF-8 text, or %NULL on error
 */
gchar         *gsw_module_get_text(GswModule *self, GError **error);
/**
 * gsw_module_get_render_text:
 * @self: a #GswModule
 * @error: return location for a #GError, or %NULL
 *
 * Gets the current entry with render options applied.
 *
 * Returns: (transfer full): the entry with render options applied, or %NULL on error
 */
gchar         *gsw_module_get_render_text(GswModule *self, GError **error);

/**
 * gsw_module_set_option:
 * @self: a #GswModule
 * @name: option name
 * @value: option value
 *
 * Sets a render or search option such as <literal>Footnotes</literal>.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean       gsw_module_set_option(GswModule *self, const gchar *name,
                                     const gchar *value);
/**
 * gsw_module_get_option:
 * @self: a #GswModule
 * @name: option name
 *
 * Gets the value of a render option.
 *
 * Returns: (transfer none): the option value, or %NULL if unset
 */
const gchar   *gsw_module_get_option(GswModule *self, const gchar *name);
/**
 * gsw_module_get_options:
 * @self: a #GswModule
 *
 * Gets the available render option names.
 *
 * Returns: (transfer container) (element-type utf8): a newly allocated array of option names
 */
GPtrArray     *gsw_module_get_options(GswModule *self);
/**
 * gsw_module_get_option_values:
 * @self: a #GswModule
 * @name: option name
 *
 * Gets the allowed values for a render option.
 *
 * Returns: (transfer container) (element-type utf8): a newly allocated array of allowed values
 */
GPtrArray     *gsw_module_get_option_values(GswModule *self, const gchar *name);

/**
 * gsw_module_next:
 * @self: a #GswModule
 * @steps: number of entries to move; may be negative
 * @error: return location for a #GError, or %NULL
 *
 * Moves the module key forward.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean       gsw_module_next(GswModule *self, gint steps, GError **error);
/**
 * gsw_module_prev:
 * @self: a #GswModule
 * @steps: number of entries to move; may be negative
 * @error: return location for a #GError, or %NULL
 *
 * Moves the module key backward.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean       gsw_module_prev(GswModule *self, gint steps, GError **error);
/**
 * gsw_module_set_position:
 * @self: a #GswModule
 * @pos: a #GswPosition
 * @error: return location for a #GError, or %NULL
 *
 * Moves the module key to the top or bottom.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean       gsw_module_set_position(GswModule *self, GswPosition pos,
                                       GError **error);

/**
 * gsw_module_has_entry:
 * @self: a #GswModule
 *
 * Checks whether the current key has a non-empty entry.
 *
 * Returns: %TRUE if the condition holds, %FALSE otherwise
 */
gboolean       gsw_module_has_entry(GswModule *self);
/**
 * gsw_module_get_entry_size:
 * @self: a #GswModule
 *
 * Gets the size of the current entry in bytes.
 *
 * Returns: the size of the current entry in bytes
 */
gsize          gsw_module_get_entry_size(GswModule *self);
/**
 * gsw_module_is_writable:
 * @self: a #GswModule
 *
 * Checks whether the module supports writing.
 *
 * Returns: %TRUE if the condition holds, %FALSE otherwise
 */
gboolean       gsw_module_is_writable(GswModule *self);

/**
 * gsw_module_search:
 * @self: a #GswModule
 * @text: search query
 * @type: a #GswSearchType
 * @error: return location for a #GError, or %NULL
 *
 * Searches all module entries for @text.
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8): a newly allocated %NULL-terminated array of strings
 */
gchar        **gsw_module_search(GswModule *self, const gchar *text,
                                 GswSearchType type, GError **error);

G_END_DECLS

#endif /* __GSW_MODULE_H__ */
