/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * app-schema.h: versioned application database schema
 */

#ifndef GSWORD_APP_SCHEMA_H
#define GSWORD_APP_SCHEMA_H 1

#include "app-db.h"

G_BEGIN_DECLS

guint gsword_app_schema_version(void);
gboolean gsword_app_schema_migrate(GswordAppDb *db, GError **error);

G_END_DECLS

#endif /* GSWORD_APP_SCHEMA_H */
