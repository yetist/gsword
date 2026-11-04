/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-module-backend.h: internal module storage backend
 */

#ifndef __GSW_MODULE_BACKEND_H__
#define __GSW_MODULE_BACKEND_H__ 1

#include <gio/gio.h>
#include <gsword/gsw-module-info.h>
#include <gsword/gsw-key.h>

typedef struct _GswModuleBackend GswModuleBackend;

typedef struct {
    gboolean (*open)(GswModuleBackend *backend, GError **error);
    void     (*close)(GswModuleBackend *backend);
    GBytes  *(*read_entry)(GswModuleBackend *backend, GswKey *key,
                           GError **error);
    gboolean (*has_entry)(GswModuleBackend *backend, GswKey *key);
    gboolean (*next)(GswModuleBackend *backend, GswKey *key, gint steps,
                     GError **error);
    gboolean (*set_position)(GswModuleBackend *backend, GswKey *key,
                             GswPosition pos, GError **error);
    gboolean (*write_entry)(GswModuleBackend *backend, GswKey *key,
                            const guint8 *data, gsize len, GError **error);
    gboolean (*delete_entry)(GswModuleBackend *backend, GswKey *key,
                             GError **error);
    gboolean (*is_writable)(GswModuleBackend *backend, GswKey *key);
    void     (*free)(GswModuleBackend *backend);
} GswModuleBackendOps;

struct _GswModuleBackend {
    GswModuleInfo *info;
    const GswModuleBackendOps *ops;
    gpointer data;
    gboolean opened;
};

GswModuleBackend *gsw_module_backend_new(GswModuleInfo *info, GError **error);
void              gsw_module_backend_free(GswModuleBackend *backend);

/* Implemented by driver files. */
GswModuleBackend *gsw_rawverse_backend_new(GswModuleInfo *info);
GswModuleBackend *gsw_zverse_backend_new(GswModuleInfo *info);
GswModuleBackend *gsw_rawstr_backend_new(GswModuleInfo *info);
GswModuleBackend *gsw_rawgenbook_backend_new(GswModuleInfo *info);
GswModuleBackend *gsw_zstr_backend_new(GswModuleInfo *info);
GswModuleBackend *gsw_rawfiles_backend_new(GswModuleInfo *info);
GswModuleBackend *gsw_hrefcom_backend_new(GswModuleInfo *info);

#endif /* __GSW_MODULE_BACKEND_H__ */
