/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * prayer-store.c: SQLite-backed prayer items for the gsword application
 */

#include "prayer-store.h"
#include "app-db.h"

#include <sqlite3.h>

struct _GswordPrayerStore {
    GswordAppDb *app_db;
    sqlite3 *db;
};

static void
prayer_store_error(GswordPrayerStore *self, GError **error,
                   const gchar *context)
{
    gsword_app_db_set_error(self->app_db, error, context);
}

static gboolean
prayer_store_exec(GswordPrayerStore *self, const gchar *sql, GError **error)
{
    return gsword_app_db_exec(self->app_db, sql, error);
}

static const gchar *
prayer_status_to_string(GswordPrayerStatus status)
{
    switch (status) {
    case GSWORD_PRAYER_STATUS_ANSWERED:
        return "answered";
    case GSWORD_PRAYER_STATUS_ARCHIVED:
        return "archived";
    case GSWORD_PRAYER_STATUS_ACTIVE:
    default:
        return "active";
    }
}

static GswordPrayerStatus
prayer_status_from_string(const gchar *status)
{
    if (g_strcmp0(status, "answered") == 0)
        return GSWORD_PRAYER_STATUS_ANSWERED;
    if (g_strcmp0(status, "archived") == 0)
        return GSWORD_PRAYER_STATUS_ARCHIVED;
    return GSWORD_PRAYER_STATUS_ACTIVE;
}

GswordPrayerVerse *
gsword_prayer_verse_new(const gchar *module, const gchar *key,
                        const gchar *display_key)
{
    GswordPrayerVerse *verse = g_new0(GswordPrayerVerse, 1);

    verse->module = g_strdup(module);
    verse->key = g_strdup(key);
    verse->display_key = g_strdup(display_key != NULL && *display_key != '\0'
                                  ? display_key : key);
    return verse;
}

void
gsword_prayer_verse_free(GswordPrayerVerse *verse)
{
    if (verse == NULL)
        return;
    g_free(verse->module);
    g_free(verse->key);
    g_free(verse->display_key);
    g_free(verse);
}

void
gsword_prayer_free(GswordPrayer *prayer)
{
    if (prayer == NULL)
        return;
    g_free(prayer->title);
    g_free(prayer->body);
    g_free(prayer->answered_note);
    g_clear_pointer(&prayer->verses, g_ptr_array_unref);
    g_free(prayer);
}

void
gsword_prayer_entry_free(GswordPrayerEntry *entry)
{
    if (entry == NULL)
        return;
    g_free(entry->body);
    g_free(entry);
}

GswordPrayerStore *
gsword_prayer_store_new(GError **error)
{
    GswordPrayerStore *self = g_new0(GswordPrayerStore, 1);
    const gchar *schema =
        "CREATE TABLE IF NOT EXISTS prayers ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " title TEXT NOT NULL,"
        " body TEXT NOT NULL DEFAULT '',"
        " status TEXT NOT NULL DEFAULT 'active',"
        " created INTEGER NOT NULL,"
        " updated INTEGER NOT NULL,"
        " answered_at INTEGER,"
        " answered_note TEXT);"
        "CREATE TABLE IF NOT EXISTS prayer_entries ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " prayer_id INTEGER NOT NULL REFERENCES prayers(id) ON DELETE CASCADE,"
        " body TEXT NOT NULL,"
        " created INTEGER NOT NULL);"
        "CREATE INDEX IF NOT EXISTS prayer_entries_prayer"
        " ON prayer_entries(prayer_id, created);"
        "CREATE TABLE IF NOT EXISTS prayer_verses ("
        " prayer_id INTEGER NOT NULL REFERENCES prayers(id) ON DELETE CASCADE,"
        " module TEXT NOT NULL,"
        " key TEXT NOT NULL,"
        " display_key TEXT NOT NULL,"
        " PRIMARY KEY (prayer_id, module, key));";

    self->app_db = gsword_app_db_open(error);
    if (self->app_db == NULL) {
        g_free(self);
        return NULL;
    }
    self->db = gsword_app_db_get_sqlite(self->app_db);
    if (!prayer_store_exec(self, schema, error)) {
        gsword_app_db_close(self->app_db);
        g_free(self);
        return NULL;
    }
    return self;
}

void
gsword_prayer_store_free(GswordPrayerStore *self)
{
    if (self == NULL)
        return;
    gsword_app_db_close(self->app_db);
    g_free(self);
}

static GPtrArray *
load_verses(GswordPrayerStore *self, gint64 id)
{
    GPtrArray *verses = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_prayer_verse_free);
    sqlite3_stmt *stmt = NULL;
    const gchar *sql =
        "SELECT module, key, display_key FROM prayer_verses"
        " WHERE prayer_id = ? ORDER BY module, key";

    if (sqlite3_prepare_v2(self->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            GswordPrayerVerse *verse = g_new0(GswordPrayerVerse, 1);

            verse->module = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 0));
            verse->key = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 1));
            verse->display_key = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 2));
            g_ptr_array_add(verses, verse);
        }
    }
    sqlite3_finalize(stmt);
    return verses;
}

static GswordPrayer *
prayer_from_statement(GswordPrayerStore *self, sqlite3_stmt *stmt)
{
    GswordPrayer *prayer = g_new0(GswordPrayer, 1);

    prayer->id = sqlite3_column_int64(stmt, 0);
    prayer->title = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
    prayer->body = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
    prayer->status = prayer_status_from_string(
        (const gchar *)sqlite3_column_text(stmt, 3));
    prayer->created = sqlite3_column_int64(stmt, 4);
    prayer->updated = sqlite3_column_int64(stmt, 5);
    prayer->answered_at = sqlite3_column_int64(stmt, 6);
    prayer->answered_note = g_strdup(
        (const gchar *)sqlite3_column_text(stmt, 7));
    prayer->verses = load_verses(self, prayer->id);
    return prayer;
}

GswordPrayer *
gsword_prayer_store_get(GswordPrayerStore *self, gint64 id)
{
    sqlite3_stmt *stmt = NULL;
    GswordPrayer *prayer = NULL;
    const gchar *sql =
        "SELECT id, title, body, status, created, updated,"
        " COALESCE(answered_at, 0), answered_note"
        " FROM prayers WHERE id = ?";

    if (sqlite3_prepare_v2(self->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            prayer = prayer_from_statement(self, stmt);
    }
    sqlite3_finalize(stmt);
    return prayer;
}

gint64
gsword_prayer_store_create(GswordPrayerStore *self, const gchar *title,
                           const gchar *body, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gint64 id = 0;

    if (title == NULL || *title == '\0') {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Prayer title must not be empty");
        return 0;
    }
    if (sqlite3_prepare_v2(self->db,
            "INSERT INTO prayers(title, body, status, created, updated)"
            " VALUES(?, ?, 'active', strftime('%s','now'),"
            " strftime('%s','now'))", -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, body != NULL ? body : "", -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        prayer_store_error(self, error, "Cannot create prayer");
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    id = sqlite3_last_insert_rowid(self->db);
    return id;
}

gboolean
gsword_prayer_store_update(GswordPrayerStore *self, gint64 id,
                           const gchar *title, const gchar *body,
                           GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (title == NULL || *title == '\0') {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Prayer title must not be empty");
        return FALSE;
    }
    if (sqlite3_prepare_v2(self->db,
            "UPDATE prayers SET title = ?, body = ?,"
            " updated = strftime('%s','now') WHERE id = ?",
            -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, body != NULL ? body : "", -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 3, id) != SQLITE_OK) {
        prayer_store_error(self, error, "Cannot prepare prayer update");
        sqlite3_finalize(stmt);
        return FALSE;
    }
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok)
        prayer_store_error(self, error, "Cannot update prayer");
    sqlite3_finalize(stmt);
    return ok;
}

gboolean
gsword_prayer_store_delete(GswordPrayerStore *self, gint64 id, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (sqlite3_prepare_v2(self->db, "DELETE FROM prayers WHERE id = ?",
                           -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, id) != SQLITE_OK) {
        prayer_store_error(self, error, "Cannot prepare prayer deletion");
        sqlite3_finalize(stmt);
        return FALSE;
    }
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok)
        prayer_store_error(self, error, "Cannot delete prayer");
    sqlite3_finalize(stmt);
    return ok;
}

GPtrArray *
gsword_prayer_store_list(GswordPrayerStore *self, GswordPrayerStatus status,
                         const gchar *query)
{
    GPtrArray *result = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_prayer_free);
    GArray *ids = g_array_new(FALSE, FALSE, sizeof(gint64));
    sqlite3_stmt *stmt = NULL;
    GString *sql = g_string_new(
        "SELECT id FROM prayers WHERE 1 = 1");
    gboolean has_query = query != NULL && *query != '\0';
    guint i;

    if (status != GSWORD_PRAYER_STATUS_ANY) {
        g_string_append(sql, " AND status = ?");
    }
    if (has_query) {
        g_string_append(sql,
                        " AND (title LIKE ? OR body LIKE ?"
                        " OR COALESCE(answered_note, '') LIKE ?)");
    }
    g_string_append(sql, " ORDER BY updated DESC, id DESC");

    if (sqlite3_prepare_v2(self->db, sql->str, -1, &stmt, NULL) ==
        SQLITE_OK) {
        gint index = 1;

        if (status != GSWORD_PRAYER_STATUS_ANY) {
            sqlite3_bind_text(stmt, index++,
                              prayer_status_to_string(status), -1,
                              SQLITE_STATIC);
        }
        if (has_query) {
            gchar *pattern = g_strdup_printf("%%%s%%", query);

            sqlite3_bind_text(stmt, index++, pattern, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, index++, pattern, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, index++, pattern, -1, SQLITE_TRANSIENT);
            g_free(pattern);
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            gint64 id = sqlite3_column_int64(stmt, 0);

            g_array_append_val(ids, id);
        }
    }
    sqlite3_finalize(stmt);
    g_string_free(sql, TRUE);

    for (i = 0; i < ids->len; i++) {
        gint64 id = g_array_index(ids, gint64, i);
        GswordPrayer *prayer = gsword_prayer_store_get(self, id);

        if (prayer != NULL)
            g_ptr_array_add(result, prayer);
    }
    g_array_unref(ids);
    return result;
}

gint
gsword_prayer_store_count(GswordPrayerStore *self, GswordPrayerStatus status)
{
    sqlite3_stmt *stmt = NULL;
    gint count = 0;

    if (status == GSWORD_PRAYER_STATUS_ANY) {
        if (sqlite3_prepare_v2(self->db,
                "SELECT count(*) FROM prayers", -1, &stmt, NULL) ==
            SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);
    } else if (sqlite3_prepare_v2(self->db,
                   "SELECT count(*) FROM prayers WHERE status = ?",
                   -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, prayer_status_to_string(status), -1,
                          SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

gboolean
gsword_prayer_store_set_status(GswordPrayerStore *self, gint64 id,
                               GswordPrayerStatus status,
                               const gchar *answered_note, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (status == GSWORD_PRAYER_STATUS_ANSWERED) {
        if (sqlite3_prepare_v2(self->db,
                "UPDATE prayers SET status = ?, answered_at ="
                " COALESCE(answered_at, strftime('%s','now')),"
                " answered_note = ?, updated = strftime('%s','now')"
                " WHERE id = ?", -1, &stmt, NULL) != SQLITE_OK ||
            sqlite3_bind_text(stmt, 1, "answered", -1, SQLITE_STATIC) !=
                SQLITE_OK ||
            sqlite3_bind_text(stmt, 2,
                              answered_note != NULL ? answered_note : "",
                              -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_int64(stmt, 3, id) != SQLITE_OK) {
            prayer_store_error(self, error,
                               "Cannot prepare prayer status update");
            sqlite3_finalize(stmt);
            return FALSE;
        }
    } else if (status == GSWORD_PRAYER_STATUS_ACTIVE) {
        if (sqlite3_prepare_v2(self->db,
                "UPDATE prayers SET status = 'active', answered_at = NULL,"
                " answered_note = NULL, updated = strftime('%s','now')"
                " WHERE id = ?", -1, &stmt, NULL) != SQLITE_OK ||
            sqlite3_bind_int64(stmt, 1, id) != SQLITE_OK) {
            prayer_store_error(self, error,
                               "Cannot prepare prayer status update");
            sqlite3_finalize(stmt);
            return FALSE;
        }
    } else {
        if (sqlite3_prepare_v2(self->db,
                "UPDATE prayers SET status = 'archived',"
                " updated = strftime('%s','now') WHERE id = ?",
                -1, &stmt, NULL) != SQLITE_OK ||
            sqlite3_bind_int64(stmt, 1, id) != SQLITE_OK) {
            prayer_store_error(self, error,
                               "Cannot prepare prayer status update");
            sqlite3_finalize(stmt);
            return FALSE;
        }
    }
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok)
        prayer_store_error(self, error, "Cannot update prayer status");
    sqlite3_finalize(stmt);
    return ok;
}

gboolean
gsword_prayer_store_set_verses(GswordPrayerStore *self, gint64 id,
                               GPtrArray *verses, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    guint i;
    gboolean ok = TRUE;

    if (!prayer_store_exec(self, "BEGIN IMMEDIATE", error))
        return FALSE;
    if (sqlite3_prepare_v2(self->db,
            "DELETE FROM prayer_verses WHERE prayer_id = ?",
            -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, id) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        prayer_store_error(self, error, "Cannot clear prayer verses");
        ok = FALSE;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    for (i = 0; ok && verses != NULL && i < verses->len; i++) {
        GswordPrayerVerse *verse = g_ptr_array_index(verses, i);

        if (verse == NULL || verse->module == NULL || verse->key == NULL)
            continue;
        if (sqlite3_prepare_v2(self->db,
                "INSERT OR IGNORE INTO prayer_verses"
                " (prayer_id, module, key, display_key) VALUES(?, ?, ?, ?)",
                -1, &stmt, NULL) != SQLITE_OK ||
            sqlite3_bind_int64(stmt, 1, id) != SQLITE_OK ||
            sqlite3_bind_text(stmt, 2, verse->module, -1,
                              SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt, 3, verse->key, -1,
                              SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt, 4,
                              verse->display_key != NULL
                                  ? verse->display_key : verse->key,
                              -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_step(stmt) != SQLITE_DONE) {
            prayer_store_error(self, error, "Cannot save prayer verse");
            ok = FALSE;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    if (ok) {
        if (sqlite3_prepare_v2(self->db,
                "UPDATE prayers SET updated = strftime('%s','now')"
                " WHERE id = ?", -1, &stmt, NULL) != SQLITE_OK ||
            sqlite3_bind_int64(stmt, 1, id) != SQLITE_OK ||
            sqlite3_step(stmt) != SQLITE_DONE) {
            prayer_store_error(self, error, "Cannot update prayer timestamp");
            ok = FALSE;
        }
        sqlite3_finalize(stmt);
    }

    if (ok)
        prayer_store_exec(self, "COMMIT", error);
    else
        prayer_store_exec(self, "ROLLBACK", NULL);
    return ok;
}

gint64
gsword_prayer_store_add_entry(GswordPrayerStore *self, gint64 prayer_id,
                              const gchar *body, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gint64 id = 0;

    if (prayer_id <= 0 || body == NULL || *body == '\0') {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Prayer entry text must not be empty");
        return 0;
    }
    if (sqlite3_prepare_v2(self->db,
            "INSERT INTO prayer_entries(prayer_id, body, created)"
            " VALUES(?, ?, strftime('%s','now'))", -1, &stmt, NULL) !=
            SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, prayer_id) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, body, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        prayer_store_error(self, error, "Cannot add prayer entry");
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;
    id = sqlite3_last_insert_rowid(self->db);
    if (sqlite3_prepare_v2(self->db,
            "UPDATE prayers SET updated = strftime('%s','now')"
            " WHERE id = ?", -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, prayer_id) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        prayer_store_error(self, error, "Cannot update prayer timestamp");
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    return id;
}

GPtrArray *
gsword_prayer_store_list_entries(GswordPrayerStore *self, gint64 prayer_id)
{
    GPtrArray *entries = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_prayer_entry_free);
    sqlite3_stmt *stmt = NULL;
    const gchar *sql =
        "SELECT id, prayer_id, body, created FROM prayer_entries"
        " WHERE prayer_id = ? ORDER BY created DESC, id DESC";

    if (sqlite3_prepare_v2(self->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, prayer_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            GswordPrayerEntry *entry = g_new0(GswordPrayerEntry, 1);

            entry->id = sqlite3_column_int64(stmt, 0);
            entry->prayer_id = sqlite3_column_int64(stmt, 1);
            entry->body = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 2));
            entry->created = sqlite3_column_int64(stmt, 3);
            g_ptr_array_add(entries, entry);
        }
    }
    sqlite3_finalize(stmt);
    return entries;
}

gboolean
gsword_prayer_store_delete_entry(GswordPrayerStore *self, gint64 entry_id,
                                 GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (sqlite3_prepare_v2(self->db,
            "DELETE FROM prayer_entries WHERE id = ?", -1, &stmt, NULL) !=
            SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, entry_id) != SQLITE_OK) {
        prayer_store_error(self, error, "Cannot prepare prayer entry deletion");
        sqlite3_finalize(stmt);
        return FALSE;
    }
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok)
        prayer_store_error(self, error, "Cannot delete prayer entry");
    sqlite3_finalize(stmt);
    return ok;
}
