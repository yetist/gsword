/*
 * gsw-backend-utils.h: path helpers shared by storage backends
 */

#ifndef __GSW_BACKEND_UTILS_H__
#define __GSW_BACKEND_UTILS_H__ 1

#include <gsword/gsw-module-info.h>

G_BEGIN_DECLS

/* Directory containing the module data. */
gchar *gsw_module_data_dir(GswModuleInfo *info);

/* Base path without extension for RawLD/zLD/RawGenBook. */
gchar *gsw_module_data_base(GswModuleInfo *info);

G_END_DECLS

#endif /* __GSW_BACKEND_UTILS_H__ */
