/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-module-backend.c: backend factory
 */

#include "gsw-module-backend.h"
#include <gsword/gsw-error.h>

GswModuleBackend *
gsw_module_backend_new(GswModuleInfo *info, GError **error)
{
    GswDriver driver;

    g_return_val_if_fail(GSW_IS_MODULE_INFO(info), NULL);
    driver = gsw_module_info_get_driver(info);

    switch (driver) {
    case GSW_DRIVER_RAW_TEXT:
    case GSW_DRIVER_RAW_TEXT4:
    case GSW_DRIVER_RAW_COM:
    case GSW_DRIVER_RAW_COM4:
        return gsw_rawverse_backend_new(info);
    case GSW_DRIVER_Z_TEXT:
    case GSW_DRIVER_Z_TEXT4:
    case GSW_DRIVER_Z_COM:
    case GSW_DRIVER_Z_COM4:
        return gsw_zverse_backend_new(info);
    case GSW_DRIVER_RAW_LD:
    case GSW_DRIVER_RAW_LD4:
        return gsw_rawstr_backend_new(info);
    case GSW_DRIVER_Z_LD:
        return gsw_zstr_backend_new(info);
    case GSW_DRIVER_RAW_GENBOOK:
        return gsw_rawgenbook_backend_new(info);
    case GSW_DRIVER_RAW_FILES:
        return gsw_rawfiles_backend_new(info);
    case GSW_DRIVER_HREF_COM:
        return gsw_hrefcom_backend_new(info);
    default:
        g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                    "Module driver %s is not supported",
                    gsw_module_info_get_driver_name(info));
        return NULL;
    }
}

void
gsw_module_backend_free(GswModuleBackend *backend)
{
    if (backend == NULL)
        return;
    if (backend->ops != NULL && backend->ops->free != NULL)
        backend->ops->free(backend);
    g_object_unref(backend->info);
    g_free(backend);
}
