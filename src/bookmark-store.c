/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * bookmark-store.c: SQLite-backed reading-position bookmarks
 */

#include "bookmark-store.h"
#include "app-db.h"

#include <sqlite3.h>

struct _GswordBookmarkStore {
    GswordAppDb *app_db;
    sqlite3 *db;
};

static void
bookmark_store_error(GswordBookmarkStore *self, GError **error,
                     const gchar *context)
{
    gsword_app_db_set_error(self->app_db, error, context);
}

void
gsword_bookmark_free(GswordBookmark *bookmark)
{
    if (bookmark == NULL)
        return;
    g_free(bookmark->title);
    g_free(bookmark->module);
    g_free(bookmark->key);
    g_free(bookmark->display_key);
    g_free(bookmark->note);
    g_free(bookmark);
}

GswordBookmarkStore *
gsword_bookmark_store_new(GError **error)
{
    GswordBookmarkStore *self = g_new0(GswordBookmarkStore, 1);

    self->app_db = gsword_app_db_open(error);
    if (self->app_db == NULL) {
        g_free(self);
        return NULL;
    }
    self->db = gsword_app_db_get_sqlite(self->app_db);
    return self;
}

void
gsword_bookmark_store_free(GswordBookmarkStore *self)
{
    if (self == NULL)
        return;
    gsword_app_db_close(self->app_db);
    g_free(self);
}

gint64
gsword_bookmark_store_create(GswordBookmarkStore *self, const gchar *title,
                             const gchar *module, const gchar *key,
                             const gchar *display_key, const gchar *note,
                             GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gint64 id = 0;

    if (module == NULL || *module == '\0' || key == NULL || *key == '\0') {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Bookmark module and key are required");
        return 0;
    }
    if (sqlite3_prepare_v2(self->db,
            "INSERT INTO bookmarks(title, module, key, display_key, note,"
            " created) VALUES(?, ?, ?, ?, ?, strftime('%s','now'))",
            -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, title != NULL ? title : "", -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, module, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 3, key, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 4,
                          display_key != NULL ? display_key : key, -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 5, note != NULL ? note : "", -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        bookmark_store_error(self, error, "Cannot create bookmark");
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    id = sqlite3_last_insert_rowid(self->db);
    return id;
}

gboolean
gsword_bookmark_store_update(GswordBookmarkStore *self, gint64 id,
                             const gchar *title, const gchar *note,
                             GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (sqlite3_prepare_v2(self->db,
            "UPDATE bookmarks SET title = ?, note = ? WHERE id = ?",
            -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, title != NULL ? title : "", -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, note != NULL ? note : "", -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 3, id) != SQLITE_OK) {
        bookmark_store_error(self, error, "Cannot prepare bookmark update");
        sqlite3_finalize(stmt);
        return FALSE;
    }
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok)
        bookmark_store_error(self, error, "Cannot update bookmark");
    sqlite3_finalize(stmt);
    return ok;
}

gboolean
gsword_bookmark_store_delete(GswordBookmarkStore *self, gint64 id,
                             GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (sqlite3_prepare_v2(self->db, "DELETE FROM bookmarks WHERE id = ?",
                           -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, id) != SQLITE_OK) {
        bookmark_store_error(self, error, "Cannot prepare bookmark deletion");
        sqlite3_finalize(stmt);
        return FALSE;
    }
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok)
        bookmark_store_error(self, error, "Cannot delete bookmark");
    sqlite3_finalize(stmt);
    return ok;
}

GswordBookmark *
gsword_bookmark_store_get(GswordBookmarkStore *self, gint64 id)
{
    sqlite3_stmt *stmt = NULL;
    GswordBookmark *bookmark = NULL;
    const gchar *sql =
        "SELECT id, title, module, key, display_key, note, created"
        " FROM bookmarks WHERE id = ?";

    if (sqlite3_prepare_v2(self->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            bookmark = g_new0(GswordBookmark, 1);
            bookmark->id = sqlite3_column_int64(stmt, 0);
            bookmark->title = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 1));
            bookmark->module = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 2));
            bookmark->key = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 3));
            bookmark->display_key = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 4));
            bookmark->note = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 5));
            bookmark->created = sqlite3_column_int64(stmt, 6);
        }
    }
    sqlite3_finalize(stmt);
    return bookmark;
}

GPtrArray *
gsword_bookmark_store_list(GswordBookmarkStore *self)
{
    GPtrArray *result = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_bookmark_free);
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(self->db,
            "SELECT id, title, module, key, display_key, note, created"
            " FROM bookmarks ORDER BY created DESC, id DESC",
            -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            GswordBookmark *bookmark = g_new0(GswordBookmark, 1);

            bookmark->id = sqlite3_column_int64(stmt, 0);
            bookmark->title = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 1));
            bookmark->module = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 2));
            bookmark->key = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 3));
            bookmark->display_key = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 4));
            bookmark->note = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 5));
            bookmark->created = sqlite3_column_int64(stmt, 6);
            g_ptr_array_add(result, bookmark);
        }
    }
    sqlite3_finalize(stmt);
    return result;
}
