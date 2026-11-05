/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * app-schema.c: versioned application database schema
 */

#include "app-schema.h"

#define GSWORD_APP_SCHEMA_VERSION 1

guint
gsword_app_schema_version(void)
{
    return GSWORD_APP_SCHEMA_VERSION;
}

static gboolean
has_column(GswordAppDb *db, const gchar *table, const gchar *column)
{
    sqlite3 *sqlite = gsword_app_db_get_sqlite(db);
    sqlite3_stmt *stmt = NULL;
    gchar *sql = g_strdup_printf("PRAGMA table_info(%s)", table);
    gboolean found = FALSE;

    if (sqlite3_prepare_v2(sqlite, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const gchar *name = (const gchar *)sqlite3_column_text(stmt, 1);

            if (g_strcmp0(name, column) == 0) {
                found = TRUE;
                break;
            }
        }
    }
    sqlite3_finalize(stmt);
    g_free(sql);
    return found;
}

gboolean
gsword_app_schema_migrate(GswordAppDb *db, GError **error)
{
    gint version = 0;
    const gchar *schema =
        "CREATE TABLE IF NOT EXISTS notes ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " module TEXT NOT NULL,"
        " key TEXT NOT NULL,"
        " display_key TEXT NOT NULL,"
        " text TEXT NOT NULL,"
        " created INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
        " updated INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
        " UNIQUE(module, key));"
        "CREATE TABLE IF NOT EXISTS tags ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " name TEXT NOT NULL UNIQUE);"
        "CREATE TABLE IF NOT EXISTS note_tags ("
        " note_id INTEGER NOT NULL REFERENCES notes(id) ON DELETE CASCADE,"
        " tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,"
        " PRIMARY KEY(note_id, tag_id));"
        "CREATE TABLE IF NOT EXISTS verse_lists ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " name TEXT NOT NULL UNIQUE,"
        " kind TEXT NOT NULL DEFAULT 'normal');"
        "CREATE TABLE IF NOT EXISTS verse_list_items ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " list_id INTEGER NOT NULL REFERENCES verse_lists(id) ON DELETE CASCADE,"
        " module TEXT NOT NULL,"
        " key TEXT NOT NULL,"
        " display_key TEXT NOT NULL,"
        " UNIQUE(list_id, module, key));"
        "CREATE TABLE IF NOT EXISTS highlights ("
        " module TEXT NOT NULL,"
        " key TEXT NOT NULL,"
        " color TEXT NOT NULL DEFAULT '#fff0a8',"
        " PRIMARY KEY(module, key));"
        "CREATE TABLE IF NOT EXISTS plans ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " name TEXT NOT NULL,"
        " start_ref TEXT NOT NULL,"
        " total_days INTEGER NOT NULL,"
        " chapters_per_day INTEGER NOT NULL DEFAULT 1,"
        " created INTEGER NOT NULL DEFAULT (strftime('%s','now')));"
        "CREATE TABLE IF NOT EXISTS plan_days ("
        " plan_id INTEGER NOT NULL REFERENCES plans(id) ON DELETE CASCADE,"
        " day INTEGER NOT NULL,"
        " reference TEXT NOT NULL,"
        " completed_on TEXT,"
        " PRIMARY KEY(plan_id, day));"
        "CREATE TABLE IF NOT EXISTS memory_verses ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " module TEXT NOT NULL,"
        " key TEXT NOT NULL,"
        " display_key TEXT NOT NULL,"
        " text TEXT NOT NULL,"
        " box INTEGER NOT NULL DEFAULT 0,"
        " due TEXT NOT NULL,"
        " created INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
        " UNIQUE(module, key));"
        "CREATE TABLE IF NOT EXISTS teaching_notes ("
        " module TEXT NOT NULL,"
        " reference TEXT NOT NULL,"
        " notes TEXT NOT NULL,"
        " updated INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
        " PRIMARY KEY(module, reference));"
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
        " PRIMARY KEY (prayer_id, module, key));"
        "CREATE TABLE IF NOT EXISTS insights ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " title TEXT NOT NULL DEFAULT '',"
        " body TEXT NOT NULL DEFAULT '',"
        " created INTEGER NOT NULL,"
        " updated INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS insight_verses ("
        " insight_id INTEGER NOT NULL REFERENCES insights(id) ON DELETE CASCADE,"
        " module TEXT NOT NULL,"
        " key TEXT NOT NULL,"
        " display_key TEXT NOT NULL,"
        " PRIMARY KEY (insight_id, module, key));"
        "CREATE TABLE IF NOT EXISTS insight_tags ("
        " insight_id INTEGER NOT NULL REFERENCES insights(id) ON DELETE CASCADE,"
        " tag TEXT NOT NULL,"
        " PRIMARY KEY (insight_id, tag));"
        "CREATE INDEX IF NOT EXISTS insight_tags_tag"
        " ON insight_tags(tag);"
        "CREATE TABLE IF NOT EXISTS bookmarks ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " title TEXT NOT NULL DEFAULT '',"
        " module TEXT NOT NULL,"
        " key TEXT NOT NULL,"
        " display_key TEXT NOT NULL,"
        " note TEXT NOT NULL DEFAULT '',"
        " created INTEGER NOT NULL);"
        "CREATE INDEX IF NOT EXISTS bookmarks_created"
        " ON bookmarks(created DESC);";
    sqlite3 *sqlite = gsword_app_db_get_sqlite(db);
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (sqlite3_prepare_v2(sqlite, "PRAGMA user_version", -1, &stmt, NULL) ==
            SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW)
        version = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (version > GSWORD_APP_SCHEMA_VERSION) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                    "Database schema version %d is newer than supported "
                    "version %d", version, GSWORD_APP_SCHEMA_VERSION);
        return FALSE;
    }
    if (version == GSWORD_APP_SCHEMA_VERSION)
        return TRUE;

    if (!gsword_app_db_begin(db, error))
        return FALSE;
    if (!gsword_app_db_exec(db, schema, error))
        goto fail;
    if (!has_column(db, "verse_lists", "kind")) {
        if (!gsword_app_db_exec(
                db,
                "ALTER TABLE verse_lists ADD COLUMN kind TEXT NOT NULL"
                " DEFAULT 'normal';", error))
            goto fail;
    }
    if (!gsword_app_db_exec(
            db,
            "INSERT OR IGNORE INTO verse_lists(name, kind)"
            " VALUES('Favorites', 'favorites');"
            "UPDATE verse_lists SET kind = 'favorites'"
            " WHERE name = 'Favorites';"
            "CREATE UNIQUE INDEX IF NOT EXISTS verse_lists_favorites"
            " ON verse_lists(kind) WHERE kind = 'favorites';", error))
        goto fail;
    {
        gchar *version_sql = g_strdup_printf(
            "PRAGMA user_version = %d", GSWORD_APP_SCHEMA_VERSION);

        ok = gsword_app_db_exec(db, version_sql, error);
        g_free(version_sql);
    }
    if (!ok)
        goto fail;
    if (!gsword_app_db_commit(db, error))
        goto fail;
    return TRUE;

fail:
    gsword_app_db_rollback(db);
    return FALSE;
}
