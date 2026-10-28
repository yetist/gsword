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

#define GSW_TYPE_CONFIG (gsw_config_get_type())
G_DECLARE_FINAL_TYPE(GswConfig, gsw_config, GSW, CONFIG, GObject)

GswConfig *gsw_config_new(void);
GswConfig *gsw_config_new_from_file(const gchar *path, GError **error);
GswConfig *gsw_config_new_from_data(const gchar *data, gssize len, GError **error);
GswConfig *gsw_config_copy(GswConfig *self);

gboolean gsw_config_load(GswConfig *self, const gchar *path, GError **error);
gboolean gsw_config_save(GswConfig *self, const gchar *path, GError **error);

const gchar *gsw_config_get_path(GswConfig *self);

gboolean     gsw_config_has_section(GswConfig *self, const gchar *section);
gchar      **gsw_config_get_sections(GswConfig *self);
gboolean     gsw_config_remove_section(GswConfig *self, const gchar *section);

gboolean     gsw_config_has_key(GswConfig *self, const gchar *section, const gchar *key);
const gchar *gsw_config_get(GswConfig *self, const gchar *section, const gchar *key);
gchar      **gsw_config_get_all(GswConfig *self, const gchar *section, const gchar *key);
gchar      **gsw_config_get_keys(GswConfig *self, const gchar *section);

gboolean     gsw_config_set(GswConfig *self, const gchar *section,
                            const gchar *key, const gchar *value);
gboolean     gsw_config_add(GswConfig *self, const gchar *section,
                            const gchar *key, const gchar *value);
gboolean     gsw_config_remove_key(GswConfig *self, const gchar *section, const gchar *key);

void         gsw_config_augment(GswConfig *self, GswConfig *other);

G_END_DECLS

#endif /* __GSW_CONFIG_H__ */
