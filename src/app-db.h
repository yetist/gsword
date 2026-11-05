/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * app-db.h: shared SQLite database access for GSword application stores
 */

#ifndef GSWORD_APP_DB_H
#define GSWORD_APP_DB_H 1

#include <glib.h>
#include <sqlite3.h>

G_BEGIN_DECLS

typedef struct _GswordAppDb GswordAppDb;

GswordAppDb *gsword_app_db_open(GError **error);
void gsword_app_db_close(GswordAppDb *self);
sqlite3 *gsword_app_db_get_sqlite(GswordAppDb *self);

gboolean gsword_app_db_exec(GswordAppDb *self, const gchar *sql,
                            GError **error);
void gsword_app_db_set_error(GswordAppDb *self, GError **error,
                             const gchar *context);
gboolean gsword_app_db_begin(GswordAppDb *self, GError **error);
gboolean gsword_app_db_commit(GswordAppDb *self, GError **error);
void gsword_app_db_rollback(GswordAppDb *self);

G_END_DECLS

#endif /* GSWORD_APP_DB_H */
