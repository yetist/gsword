/*
 * gsw-utils.h: internal helpers
 *
 * This file is part of gsword. It is not installed.
 */

#ifndef __GSW_UTILS_H__
#define __GSW_UTILS_H__ 1

#include <glib.h>
#include <gsword/gsw-enums.h>

G_BEGIN_DECLS

gchar   *gsw_strdup_trim(const gchar *s);
gchar   *gsw_str_remove_trailing_slashes(const gchar *s);
gboolean gsw_str_ends_with_ci(const gchar *s, const gchar *suffix);
gboolean gsw_str_is_true(const gchar *s);
guint64  gsw_str_parse_u64(const gchar *s, guint64 fallback);
gchar   *gsw_strongs_pad(const gchar *key);
gchar   *gsw_utf8_strup(const gchar *s);

gboolean gsw_path_is_safe_relative(const gchar *path);
gchar   *gsw_path_join_under(const gchar *root, const gchar *relative,
                             GError **error);
gchar   *gsw_path_make_absolute(const gchar *path, const gchar *base);
gboolean gsw_path_is_within(const gchar *root, const gchar *path);
gchar   *gsw_path_make_temp_dir(const gchar *parent, const gchar *prefix,
                                GError **error);

gboolean gsw_file_copy(const gchar *src, const gchar *dst, GError **error);
gboolean gsw_dir_copy_recursive(const gchar *src, const gchar *dst,
                                GError **error);
gboolean gsw_dir_remove_recursive(const gchar *path, GError **error);
gboolean gsw_mkdir_with_parents(const gchar *path, GError **error);

gchar   *gsw_build_url(const gchar *scheme, const gchar *host,
                       const gchar *directory, const gchar *path);

GswDriver      gsw_driver_from_string(const gchar *name);
const gchar   *gsw_driver_to_string(GswDriver driver);
gboolean       gsw_driver_is_supported(GswDriver driver);

GswModuleType  gsw_module_type_from_string(const gchar *name);
const gchar   *gsw_module_type_to_string(GswModuleType type);
GswModuleType  gsw_module_type_default_for_driver(GswDriver driver);

GswSourceType  gsw_source_type_from_string(const gchar *name);
const gchar   *gsw_source_type_to_string(GswSourceType type);
const gchar   *gsw_source_type_to_scheme(GswSourceType type);

G_END_DECLS

#endif /* __GSW_UTILS_H__ */
