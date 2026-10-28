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
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

#define GSW_TYPE_MANAGER (gsw_manager_get_type())
G_DECLARE_FINAL_TYPE(GswManager, gsw_manager, GSW, MANAGER, GObject)

GswManager *gsw_manager_new(GError **error);
GswManager *gsw_manager_new_for_path(const gchar *prefix_path, GError **error);
GswManager *gsw_manager_new_for_config(const gchar *config_path, GError **error);

gboolean gsw_manager_load(GswManager *self, GError **error);
gboolean gsw_manager_reload(GswManager *self, GError **error);

void         gsw_manager_add_augment_path(GswManager *self, const gchar *path);
const gchar *gsw_manager_get_prefix_path(GswManager *self);
const gchar *gsw_manager_get_config_path(GswManager *self);

/**
 * gsw_manager_get_module_infos:
 * @self: a #GswManager
 *
 * Returns: (transfer full) (element-type GswModuleInfo): a new array of
 *   referenced module metadata objects. Free with g_ptr_array_unref().
 */
GPtrArray     *gsw_manager_get_module_infos(GswManager *self);
GswModuleInfo *gsw_manager_lookup_module_info(GswManager *self, const gchar *name);
gboolean       gsw_manager_has_module(GswManager *self, const gchar *name);

GswConfig     *gsw_manager_get_config(GswManager *self);
const gchar   *gsw_manager_get_config_file_for_module(GswManager *self,
                                                      const gchar *name);

G_END_DECLS

#endif /* __GSW_MANAGER_H__ */
