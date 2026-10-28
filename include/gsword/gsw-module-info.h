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

#define GSW_TYPE_MODULE_INFO (gsw_module_info_get_type())
G_DECLARE_FINAL_TYPE(GswModuleInfo, gsw_module_info, GSW, MODULE_INFO, GObject)

GswModuleInfo *gsw_module_info_new_from_config(const gchar *name,
                                               GswConfig *config,
                                               const gchar *config_file,
                                               const gchar *prefix_path,
                                               GError **error);

const gchar   *gsw_module_info_get_name(GswModuleInfo *self);
const gchar   *gsw_module_info_get_description(GswModuleInfo *self);
const gchar   *gsw_module_info_get_type_string(GswModuleInfo *self);
GswModuleType  gsw_module_info_get_module_type(GswModuleInfo *self);
GswDriver      gsw_module_info_get_driver(GswModuleInfo *self);
const gchar   *gsw_module_info_get_driver_name(GswModuleInfo *self);
const gchar   *gsw_module_info_get_language(GswModuleInfo *self);
const gchar   *gsw_module_info_get_version(GswModuleInfo *self);
const gchar   *gsw_module_info_get_minimum_version(GswModuleInfo *self);
guint64        gsw_module_info_get_install_size(GswModuleInfo *self);
const gchar   *gsw_module_info_get_data_path(GswModuleInfo *self);
const gchar   *gsw_module_info_get_absolute_data_path(GswModuleInfo *self);
const gchar   *gsw_module_info_get_relative_data_dir(GswModuleInfo *self);
const gchar   *gsw_module_info_get_prefix_path(GswModuleInfo *self);
const gchar   *gsw_module_info_get_config_file(GswModuleInfo *self);
const gchar   *gsw_module_info_get_encoding(GswModuleInfo *self);
const gchar   *gsw_module_info_get_source_type(GswModuleInfo *self);
const gchar   *gsw_module_info_get_versification(GswModuleInfo *self);
const gchar   *gsw_module_info_get_copyright(GswModuleInfo *self);
const gchar   *gsw_module_info_get_distribution_license(GswModuleInfo *self);
const gchar   *gsw_module_info_get_about(GswModuleInfo *self);
gboolean       gsw_module_info_is_ciphered(GswModuleInfo *self);
gboolean       gsw_module_info_is_supported(GswModuleInfo *self);

const gchar   *gsw_module_info_get_value(GswModuleInfo *self, const gchar *key);
/**
 * gsw_module_info_get_keys:
 *
 * Returns: (transfer full) (array zero-terminated=1): result
 */
gchar        **gsw_module_info_get_keys(GswModuleInfo *self);
/**
 * gsw_module_info_get_values:
 *
 * Returns: (transfer full) (array zero-terminated=1): result
 */
gchar        **gsw_module_info_get_values(GswModuleInfo *self, const gchar *key);

G_END_DECLS

#endif /* __GSW_MODULE_INFO_H__ */
