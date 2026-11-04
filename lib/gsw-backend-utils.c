/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-backend-utils.c: path helpers shared by storage backends
 */

#include "gsw-backend-utils.h"

#include <string.h>

gchar *
gsw_module_data_dir(GswModuleInfo *info)
{
    const gchar *path = gsw_module_info_get_absolute_data_path(info);
    return g_strdup(path != NULL ? path : "");
}

gchar *
gsw_module_data_base(GswModuleInfo *info)
{
    const gchar *data_path = gsw_module_info_get_data_path(info);
    gchar *dir = gsw_module_data_dir(info);
    gchar *base = NULL;

    if (data_path != NULL && g_path_is_absolute(data_path)) {
        base = g_strdup(data_path);
    } else {
        gchar *name = g_path_get_basename(data_path != NULL ? data_path : "");
        base = g_build_filename(dir, name, NULL);
        g_free(name);
    }
    g_free(dir);
    return base;
}
