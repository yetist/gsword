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

#define GSW_TYPE_MODULE (gsw_module_get_type())
G_DECLARE_FINAL_TYPE(GswModule, gsw_module, GSW, MODULE, GObject)

/**
 * gsw_module_get_info:
 *
 * Returns: (transfer none): result
 */
GswModuleInfo *gsw_module_get_info(GswModule *self);
const gchar   *gsw_module_get_name(GswModule *self);
gboolean       gsw_module_is_open(GswModule *self);
gboolean       gsw_module_is_supported(GswModule *self);

/**
 * gsw_module_create_key:
 *
 * Returns: (transfer full): result
 */
GswKey        *gsw_module_create_key(GswModule *self);
GswKey        *gsw_module_get_key(GswModule *self);
gboolean       gsw_module_set_key(GswModule *self, GswKey *key, GError **error);
gboolean       gsw_module_set_key_text(GswModule *self, const gchar *text,
                                       GError **error);
const gchar   *gsw_module_get_key_text(GswModule *self);

/**
 * gsw_module_read_raw:
 *
 * Returns: (transfer full): result
 */
GBytes        *gsw_module_read_raw(GswModule *self, GError **error);
const gchar   *gsw_module_peek_raw(GswModule *self, GError **error);
/**
 * gsw_module_get_text:
 *
 * Returns: (transfer full): result
 */
gchar         *gsw_module_get_text(GswModule *self, GError **error);

gboolean       gsw_module_next(GswModule *self, gint steps, GError **error);
gboolean       gsw_module_prev(GswModule *self, gint steps, GError **error);
gboolean       gsw_module_set_position(GswModule *self, GswPosition pos,
                                       GError **error);

gboolean       gsw_module_has_entry(GswModule *self);
gsize          gsw_module_get_entry_size(GswModule *self);
gboolean       gsw_module_is_writable(GswModule *self);

G_END_DECLS

#endif /* __GSW_MODULE_H__ */
