/*
 * gsw-module-internal.h: internal module constructor
 */

#ifndef __GSW_MODULE_INTERNAL_H__
#define __GSW_MODULE_INTERNAL_H__ 1

#include <gsword/gsw-module.h>
#include <gsword/gsw-module-info.h>

G_BEGIN_DECLS

GswModule *gsw_module_new_from_info(GswModuleInfo *info);

G_END_DECLS

#endif /* __GSW_MODULE_INTERNAL_H__ */
