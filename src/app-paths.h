#ifndef GSWORD_APP_PATHS_H
#define GSWORD_APP_PATHS_H 1

#include <glib.h>

G_BEGIN_DECLS

gchar *gsword_app_data_dir(void);
gchar *gsword_app_library_dir(void);
gchar *gsword_app_config_dir(void);
gchar *gsword_app_cache_dir(void);
gchar *gsword_app_default_library_path(void);
gchar *gsword_app_exports_dir(void);
gchar *gsword_app_search_index_path(const gchar *module_name);
GPtrArray *gsword_app_system_library_dirs(void);

G_END_DECLS

#endif /* GSWORD_APP_PATHS_H */
