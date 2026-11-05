/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * insight-store.c: SQLite-backed free-form insights for the gsword application
 */

#include "insight-store.h"
#include "app-db.h"

#include <sqlite3.h>

struct _GswordInsightStore {
    GswordAppDb *app_db;
    sqlite3 *db;
};

static void
insight_store_error(GswordInsightStore *self, GError **error,
                    const gchar *context)
{
    gsword_app_db_set_error(self->app_db, error, context);
}

GswordInsightVerse *
gsword_insight_verse_new(const gchar *module, const gchar *key,
                         const gchar *display_key)
{
    GswordInsightVerse *verse = g_new0(GswordInsightVerse, 1);

    verse->module = g_strdup(module);
    verse->key = g_strdup(key);
    verse->display_key = g_strdup(display_key != NULL && *display_key != '\0'
                                  ? display_key : key);
    return verse;
}

void
gsword_insight_verse_free(GswordInsightVerse *verse)
{
    if (verse == NULL)
        return;
    g_free(verse->module);
    g_free(verse->key);
    g_free(verse->display_key);
    g_free(verse);
}

void
gsword_insight_free(GswordInsight *insight)
{
    if (insight == NULL)
        return;
    g_free(insight->title);
    g_free(insight->body);
    g_clear_pointer(&insight->verses, g_ptr_array_unref);
    g_clear_pointer(&insight->tags, g_ptr_array_unref);
    g_free(insight);
}

GswordInsightStore *
gsword_insight_store_new(GError **error)
{
    GswordInsightStore *self = g_new0(GswordInsightStore, 1);

    self->app_db = gsword_app_db_open(error);
    if (self->app_db == NULL) {
        g_free(self);
        return NULL;
    }
    self->db = gsword_app_db_get_sqlite(self->app_db);
    return self;
}

void
gsword_insight_store_free(GswordInsightStore *self)
{
    if (self == NULL)
        return;
    gsword_app_db_close(self->app_db);
    g_free(self);
}

static GPtrArray *
load_verses(GswordInsightStore *self, gint64 insight_id)
{
    GPtrArray *verses = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_insight_verse_free);
    sqlite3_stmt *stmt = NULL;
    const gchar *sql =
        "SELECT module, key, display_key FROM insight_verses"
        " WHERE insight_id = ? ORDER BY module, key";

    if (sqlite3_prepare_v2(self->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, insight_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            GswordInsightVerse *verse = g_new0(GswordInsightVerse, 1);

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

static GPtrArray *
load_tags(GswordInsightStore *self, gint64 insight_id)
{
    GPtrArray *tags = g_ptr_array_new_with_free_func(g_free);
    sqlite3_stmt *stmt = NULL;
    const gchar *sql =
        "SELECT tag FROM insight_tags WHERE insight_id = ? ORDER BY tag";

    if (sqlite3_prepare_v2(self->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, insight_id);
        while (sqlite3_step(stmt) == SQLITE_ROW)
            g_ptr_array_add(tags, g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    return tags;
}

GswordInsight *
gsword_insight_store_get(GswordInsightStore *self, gint64 id)
{
    sqlite3_stmt *stmt = NULL;
    GswordInsight *insight = NULL;
    const gchar *sql =
        "SELECT id, title, body, created, updated FROM insights WHERE id = ?";

    if (sqlite3_prepare_v2(self->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            insight = g_new0(GswordInsight, 1);
            insight->id = sqlite3_column_int64(stmt, 0);
            insight->title = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 1));
            insight->body = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 2));
            insight->created = sqlite3_column_int64(stmt, 3);
            insight->updated = sqlite3_column_int64(stmt, 4);
        }
    }
    sqlite3_finalize(stmt);
    if (insight != NULL) {
        insight->verses = load_verses(self, id);
        insight->tags = load_tags(self, id);
    }
    return insight;
}

GPtrArray *
gsword_insight_store_list(GswordInsightStore *self, const gchar *query,
                          const gchar *tag)
{
    GPtrArray *result = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_insight_free);
    GArray *ids = g_array_new(FALSE, FALSE, sizeof(gint64));
    sqlite3_stmt *stmt = NULL;
    GString *sql = g_string_new("SELECT id FROM insights WHERE 1 = 1");
    gboolean has_query = query != NULL && *query != '\0';
    gboolean has_tag = tag != NULL && *tag != '\0';
    guint i;

    if (has_query)
        g_string_append(sql, " AND (title LIKE ? OR body LIKE ?)");
    if (has_tag)
        g_string_append(sql,
                        " AND EXISTS (SELECT 1 FROM insight_tags t"
                        " WHERE t.insight_id = insights.id AND t.tag = ?)");
    g_string_append(sql, " ORDER BY updated DESC, id DESC");

    if (sqlite3_prepare_v2(self->db, sql->str, -1, &stmt, NULL) ==
        SQLITE_OK) {
        gint index = 1;

        if (has_query) {
            gchar *pattern = g_strdup_printf("%%%s%%", query);

            sqlite3_bind_text(stmt, index++, pattern, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, index++, pattern, -1, SQLITE_TRANSIENT);
            g_free(pattern);
        }
        if (has_tag)
            sqlite3_bind_text(stmt, index, tag, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            gint64 id = sqlite3_column_int64(stmt, 0);

            g_array_append_val(ids, id);
        }
    }
    sqlite3_finalize(stmt);
    g_string_free(sql, TRUE);

    for (i = 0; i < ids->len; i++) {
        GswordInsight *insight = gsword_insight_store_get(
            self, g_array_index(ids, gint64, i));

        if (insight != NULL)
            g_ptr_array_add(result, insight);
    }
    g_array_unref(ids);
    return result;
}

GPtrArray *
gsword_insight_store_list_tags(GswordInsightStore *self)
{
    GPtrArray *tags = g_ptr_array_new_with_free_func(g_free);
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(self->db,
            "SELECT DISTINCT tag FROM insight_tags ORDER BY tag",
            -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW)
            g_ptr_array_add(tags, g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    return tags;
}

static gboolean
save_relations(GswordInsightStore *self, gint64 insight_id,
               GPtrArray *verses, GPtrArray *tags, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    GHashTable *seen_tags = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  g_free, NULL);
    gboolean ok = TRUE;
    guint i;

    if (sqlite3_prepare_v2(self->db,
            "DELETE FROM insight_verses WHERE insight_id = ?",
            -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, insight_id) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        insight_store_error(self, error, "Cannot clear insight verses");
        ok = FALSE;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (ok &&
        (sqlite3_prepare_v2(self->db,
             "DELETE FROM insight_tags WHERE insight_id = ?",
             -1, &stmt, NULL) != SQLITE_OK ||
         sqlite3_bind_int64(stmt, 1, insight_id) != SQLITE_OK ||
         sqlite3_step(stmt) != SQLITE_DONE)) {
        insight_store_error(self, error, "Cannot clear insight tags");
        ok = FALSE;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    for (i = 0; ok && verses != NULL && i < verses->len; i++) {
        GswordInsightVerse *verse = g_ptr_array_index(verses, i);

        if (verse == NULL || verse->module == NULL || verse->key == NULL)
            continue;
        if (sqlite3_prepare_v2(self->db,
                "INSERT OR IGNORE INTO insight_verses"
                " (insight_id, module, key, display_key) VALUES(?, ?, ?, ?)",
                -1, &stmt, NULL) != SQLITE_OK ||
            sqlite3_bind_int64(stmt, 1, insight_id) != SQLITE_OK ||
            sqlite3_bind_text(stmt, 2, verse->module, -1,
                              SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt, 3, verse->key, -1,
                              SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(stmt, 4,
                              verse->display_key != NULL
                                  ? verse->display_key : verse->key,
                              -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_step(stmt) != SQLITE_DONE) {
            insight_store_error(self, error, "Cannot save insight verse");
            ok = FALSE;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    for (i = 0; ok && tags != NULL && i < tags->len; i++) {
        const gchar *raw = g_ptr_array_index(tags, i);
        gchar *tag = g_strdup(raw);
        gchar *stripped;

        if (tag == NULL)
            continue;
        stripped = g_strstrip(tag);
        if (*stripped == '\0' || g_hash_table_contains(seen_tags, stripped)) {
            g_free(tag);
            continue;
        }
        g_hash_table_add(seen_tags, g_strdup(stripped));
        if (sqlite3_prepare_v2(self->db,
                "INSERT OR IGNORE INTO insight_tags (insight_id, tag)"
                " VALUES(?, ?)", -1, &stmt, NULL) != SQLITE_OK ||
            sqlite3_bind_int64(stmt, 1, insight_id) != SQLITE_OK ||
            sqlite3_bind_text(stmt, 2, stripped, -1, SQLITE_TRANSIENT) !=
                SQLITE_OK ||
            sqlite3_step(stmt) != SQLITE_DONE) {
            insight_store_error(self, error, "Cannot save insight tag");
            ok = FALSE;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
        g_free(tag);
    }

    g_hash_table_unref(seen_tags);
    return ok;
}

gint64
gsword_insight_store_create(GswordInsightStore *self, const gchar *title,
                            const gchar *body, GPtrArray *verses,
                            GPtrArray *tags, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gint64 id = 0;

    if (!gsword_app_db_begin(self->app_db, error))
        return 0;
    if (sqlite3_prepare_v2(self->db,
            "INSERT INTO insights(title, body, created, updated)"
            " VALUES(?, ?, strftime('%s','now'), strftime('%s','now'))",
            -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, title != NULL ? title : "", -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, body != NULL ? body : "", -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        insight_store_error(self, error, "Cannot create insight");
        sqlite3_finalize(stmt);
        gsword_app_db_rollback(self->app_db);
        return 0;
    }
    sqlite3_finalize(stmt);
    id = sqlite3_last_insert_rowid(self->db);
    if (!save_relations(self, id, verses, tags, error)) {
        gsword_app_db_rollback(self->app_db);
        return 0;
    }
    if (!gsword_app_db_commit(self->app_db, error)) {
        gsword_app_db_rollback(self->app_db);
        return 0;
    }
    return id;
}

gboolean
gsword_insight_store_update(GswordInsightStore *self, gint64 id,
                            const gchar *title, const gchar *body,
                            GPtrArray *verses, GPtrArray *tags, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (!gsword_app_db_begin(self->app_db, error))
        return FALSE;
    if (sqlite3_prepare_v2(self->db,
            "UPDATE insights SET title = ?, body = ?,"
            " updated = strftime('%s','now') WHERE id = ?",
            -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, title != NULL ? title : "", -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, body != NULL ? body : "", -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 3, id) != SQLITE_OK) {
        insight_store_error(self, error, "Cannot prepare insight update");
        sqlite3_finalize(stmt);
        gsword_app_db_rollback(self->app_db);
        return FALSE;
    }
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    if (!ok) {
        insight_store_error(self, error, "Cannot update insight");
        gsword_app_db_rollback(self->app_db);
        return FALSE;
    }
    if (!save_relations(self, id, verses, tags, error)) {
        gsword_app_db_rollback(self->app_db);
        return FALSE;
    }
    if (!gsword_app_db_commit(self->app_db, error)) {
        gsword_app_db_rollback(self->app_db);
        return FALSE;
    }
    return TRUE;
}

gboolean
gsword_insight_store_delete(GswordInsightStore *self, gint64 id,
                            GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (sqlite3_prepare_v2(self->db, "DELETE FROM insights WHERE id = ?",
                           -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, id) != SQLITE_OK) {
        insight_store_error(self, error, "Cannot prepare insight deletion");
        sqlite3_finalize(stmt);
        return FALSE;
    }
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok)
        insight_store_error(self, error, "Cannot delete insight");
    sqlite3_finalize(stmt);
    return ok;
}
