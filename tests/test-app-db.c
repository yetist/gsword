/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include "app-db.h"
#include "app-schema.h"
#include "gsw-utils.h"

#include <glib.h>
#include <glib/gstdio.h>

static gint
count_rows(sqlite3 *db, const gchar *table)
{
    sqlite3_stmt *stmt = NULL;
    gchar *sql = g_strdup_printf("SELECT count(*) FROM %s", table);
    gint count = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    g_free(sql);
    return count;
}

static void
test_open_exec(void)
{
    GError *error = NULL;
    GswordAppDb *db = gsword_app_db_open(&error);
    sqlite3 *sqlite;
    sqlite3_stmt *stmt = NULL;

    g_assert_no_error(error);
    g_assert_nonnull(db);
    sqlite = gsword_app_db_get_sqlite(db);
    g_assert_nonnull(sqlite);

    if (sqlite3_prepare_v2(sqlite, "PRAGMA user_version", -1, &stmt, NULL) ==
            SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW)
        g_assert_cmpint(sqlite3_column_int(stmt, 0), ==,
                        (gint)gsword_app_schema_version());
    sqlite3_finalize(stmt);
    stmt = NULL;
    g_assert_cmpint(count_rows(sqlite, "verse_lists"), >=, 1);

    g_assert_true(gsword_app_db_exec(
        db,
        "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT);"
        "INSERT INTO items(name) VALUES('first');",
        &error));
    g_assert_no_error(error);
    g_assert_cmpint(count_rows(sqlite, "items"), ==, 1);

    g_assert_true(gsword_app_db_begin(db, &error));
    g_assert_no_error(error);
    g_assert_true(gsword_app_db_exec(
        db, "INSERT INTO items(name) VALUES('second');", &error));
    g_assert_no_error(error);
    gsword_app_db_rollback(db);
    g_assert_cmpint(count_rows(sqlite, "items"), ==, 1);

    g_assert_true(gsword_app_db_begin(db, &error));
    g_assert_no_error(error);
    g_assert_true(gsword_app_db_exec(
        db, "INSERT INTO items(name) VALUES('third');", &error));
    g_assert_no_error(error);
    g_assert_true(gsword_app_db_commit(db, &error));
    g_assert_no_error(error);
    g_assert_cmpint(count_rows(sqlite, "items"), ==, 2);

    if (sqlite3_prepare_v2(sqlite, "PRAGMA foreign_keys", -1, &stmt, NULL) ==
        SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW)
        g_assert_cmpint(sqlite3_column_int(stmt, 0), ==, 1);
    sqlite3_finalize(stmt);

    g_assert_false(gsword_app_db_exec(db, "NOT VALID SQL", &error));
    g_assert_nonnull(error);
    g_clear_error(&error);
    gsword_app_db_close(db);
}

static void
test_newer_version_rejected(void)
{
    GError *error = NULL;
    GswordAppDb *db;
    sqlite3 *sqlite;
    gchar *path = g_build_filename(g_get_user_data_dir(), "gsword", "app.db",
                                   NULL);

    g_assert_cmpint(sqlite3_open(path, &sqlite), ==, SQLITE_OK);
    g_assert_cmpint(sqlite3_exec(sqlite, "PRAGMA user_version = 999", NULL,
                                 NULL, NULL), ==, SQLITE_OK);
    sqlite3_close(sqlite);
    g_free(path);

    db = gsword_app_db_open(&error);
    g_assert_null(db);
    g_assert_nonnull(error);
    g_clear_error(&error);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    gchar *root;
    gchar *data_home;

    root = g_dir_make_tmp("gsword-app-db-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(root);
    data_home = g_build_filename(root, "data", NULL);
    g_mkdir_with_parents(data_home, 0755);
    g_setenv("XDG_DATA_HOME", data_home, TRUE);

    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/app-db/open-exec", test_open_exec);
    g_test_add_func("/app-db/newer-version", test_newer_version_rejected);
    {
        int status = g_test_run();

        gsw_dir_remove_recursive(root, NULL);
        g_free(data_home);
        g_free(root);
        return status;
    }
}
