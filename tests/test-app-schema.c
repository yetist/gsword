/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include "app-db.h"
#include "app-schema.h"
#include "gsw-utils.h"

#include <glib.h>
#include <glib/gstdio.h>

static gint
count_rows(sqlite3 *db, const gchar *sql)
{
    sqlite3_stmt *stmt = NULL;
    gint count = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

static void
test_migrate_v0(void)
{
    GError *error = NULL;
    gchar *directory = g_build_filename(g_get_user_data_dir(), "gsword",
                                        NULL);
    gchar *path = g_build_filename(directory, "app.db", NULL);
    sqlite3 *raw = NULL;
    GswordAppDb *db;
    sqlite3 *sqlite;

    g_mkdir_with_parents(directory, 0755);
    g_assert_cmpint(sqlite3_open(path, &raw), ==, SQLITE_OK);
    g_assert_cmpint(sqlite3_exec(
        raw,
        "CREATE TABLE verse_lists ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " name TEXT NOT NULL UNIQUE);"
        "INSERT INTO verse_lists(name) VALUES('Old list');"
        "PRAGMA user_version = 0;", NULL, NULL, NULL), ==, SQLITE_OK);
    sqlite3_close(raw);

    db = gsword_app_db_open(&error);
    g_assert_no_error(error);
    g_assert_nonnull(db);
    sqlite = gsword_app_db_get_sqlite(db);
    g_assert_cmpint(count_rows(sqlite, "PRAGMA user_version"), ==,
                    (gint)gsword_app_schema_version());
    g_assert_cmpint(count_rows(
        sqlite,
        "SELECT count(*) FROM verse_lists WHERE kind = 'favorites'"), ==, 1);
    g_assert_cmpint(count_rows(
        sqlite,
        "SELECT count(*) FROM verse_lists"
        " WHERE name = 'Old list' AND kind = 'normal'"), ==, 1);
    g_assert_cmpint(count_rows(
        sqlite,
        "SELECT count(*) FROM sqlite_master"
        " WHERE type = 'table' AND name IN"
        " ('notes','plans','prayers','insights','bookmarks')"), ==, 5);
    gsword_app_db_close(db);
    g_free(path);
    g_free(directory);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    gchar *root;
    gchar *data_home;

    root = g_dir_make_tmp("gsword-app-schema-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(root);
    data_home = g_build_filename(root, "data", NULL);
    g_mkdir_with_parents(data_home, 0755);
    g_setenv("XDG_DATA_HOME", data_home, TRUE);

    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/app-schema/migrate-v0", test_migrate_v0);
    {
        int status = g_test_run();

        gsw_dir_remove_recursive(root, NULL);
        g_free(data_home);
        g_free(root);
        return status;
    }
}
