/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * app-db.c: shared SQLite database access for GSword application stores
 */

#include "app-db.h"
#include "app-paths.h"

#include <glib/gstdio.h>

struct _GswordAppDb {
    sqlite3 *db;
    gchar *path;
};

GswordAppDb *
gsword_app_db_open(GError **error)
{
    GswordAppDb *self = g_new0(GswordAppDb, 1);
    gchar *directory = gsword_app_data_dir();

    self->path = g_build_filename(directory, "app.db", NULL);
    g_mkdir_with_parents(directory, 0755);
    if (sqlite3_open_v2(self->path, &self->db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        NULL) != SQLITE_OK) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                    "Cannot open application database %s: %s", self->path,
                    self->db != NULL ? sqlite3_errmsg(self->db) : "unknown");
        sqlite3_close(self->db);
        g_free(self->path);
        g_free(self);
        g_free(directory);
        return NULL;
    }
    if (!gsword_app_db_exec(
            self,
            "PRAGMA foreign_keys = ON;"
            "PRAGMA busy_timeout = 2000;",
            error)) {
        sqlite3_close(self->db);
        g_free(self->path);
        g_free(self);
        g_free(directory);
        return NULL;
    }
    g_free(directory);
    return self;
}

void
gsword_app_db_close(GswordAppDb *self)
{
    if (self == NULL)
        return;
    sqlite3_close(self->db);
    g_free(self->path);
    g_free(self);
}

sqlite3 *
gsword_app_db_get_sqlite(GswordAppDb *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->db;
}

gboolean
gsword_app_db_exec(GswordAppDb *self, const gchar *sql, GError **error)
{
    gchar *message = NULL;

    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(sql != NULL, FALSE);
    if (sqlite3_exec(self->db, sql, NULL, NULL, &message) != SQLITE_OK) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                    "SQLite error: %s", message != NULL ? message : "unknown");
        sqlite3_free(message);
        return FALSE;
    }
    return TRUE;
}

void
gsword_app_db_set_error(GswordAppDb *self, GError **error,
                        const gchar *context)
{
    const gchar *detail = self != NULL && self->db != NULL
                        ? sqlite3_errmsg(self->db) : "unknown";

    g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                "%s: %s", context, detail);
}

gboolean
gsword_app_db_begin(GswordAppDb *self, GError **error)
{
    return gsword_app_db_exec(self, "BEGIN IMMEDIATE", error);
}

gboolean
gsword_app_db_commit(GswordAppDb *self, GError **error)
{
    return gsword_app_db_exec(self, "COMMIT", error);
}

void
gsword_app_db_rollback(GswordAppDb *self)
{
    gsword_app_db_exec(self, "ROLLBACK", NULL);
}
