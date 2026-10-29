/*
 * gsw-options.h: internal render option registry
 */

#ifndef __GSW_OPTIONS_H__
#define __GSW_OPTIONS_H__ 1

#include <gsword/gsw-module.h>

G_BEGIN_DECLS

void      gsw_options_init(GHashTable *options);
gboolean  gsw_options_known(const gchar *name);
GPtrArray *gsw_options_values(const gchar *name);
void      gsw_module_apply_default_options(GswModule *self,
                                           GHashTable *defaults);

G_END_DECLS

#endif /* __GSW_OPTIONS_H__ */
