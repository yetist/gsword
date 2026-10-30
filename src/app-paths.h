#ifndef GSWORD_APP_PATHS_H
#define GSWORD_APP_PATHS_H 1

#include <glib.h>

G_BEGIN_DECLS

gchar *gsword_app_data_dir(void);
void gsword_app_migrate_legacy_data(void);

G_END_DECLS

#endif /* GSWORD_APP_PATHS_H */
