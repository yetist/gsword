#include "notes-store.h"
#include "app-paths.h"

#include <glib/gstdio.h>
#include <sqlite3.h>
#include <string.h>

struct _GswordNotesStore {
    sqlite3 *db;
};

static void
notes_store_error(GswordNotesStore *self, GError **error, const gchar *context)
{
    g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                "%s: %s", context, sqlite3_errmsg(self->db));
}

static gboolean
notes_store_exec(GswordNotesStore *self, const gchar *sql, GError **error)
{
    gchar *message = NULL;

    if (sqlite3_exec(self->db, sql, NULL, NULL, &message) != SQLITE_OK) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                    "SQLite error: %s", message != NULL ? message : "unknown");
        sqlite3_free(message);
        return FALSE;
    }
    return TRUE;
}

static void
gsword_verse_list_free_internal(GswordVerseList *list)
{
    if (list == NULL)
        return;
    g_free(list->name);
    g_free(list);
}

void
gsword_verse_list_free(GswordVerseList *list)
{
    gsword_verse_list_free_internal(list);
}

void
gsword_verse_list_item_free(GswordVerseListItem *item)
{
    if (item == NULL)
        return;
    g_free(item->module);
    g_free(item->key);
    g_free(item->display_key);
    g_free(item);
}

void
gsword_note_free(GswordNote *note)
{
    if (note == NULL)
        return;
    g_free(note->module);
    g_free(note->key);
    g_free(note->display_key);
    g_free(note->text);
    g_free(note->tags);
    g_free(note);
}

GswordNotesStore *
gsword_notes_store_new(GError **error)
{
    GswordNotesStore *self = g_new0(GswordNotesStore, 1);
    gchar *directory = gsword_app_data_dir();
    gchar *path = g_build_filename(directory, "app.db", NULL);
    const gchar *schema =
        "PRAGMA foreign_keys = ON;"
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
        " name TEXT NOT NULL UNIQUE);"
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
        " PRIMARY KEY(module, key));";

    g_mkdir_with_parents(directory, 0755);
    if (sqlite3_open_v2(path, &self->db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        NULL) != SQLITE_OK) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                    "Cannot open notes database %s: %s", path,
                    self->db != NULL ? sqlite3_errmsg(self->db) : "unknown");
        sqlite3_close(self->db);
        g_free(path);
        g_free(directory);
        g_free(self);
        return NULL;
    }
    if (!notes_store_exec(self, schema, error)) {
        sqlite3_close(self->db);
        g_free(path);
        g_free(directory);
        g_free(self);
        return NULL;
    }
    g_free(path);
    g_free(directory);
    return self;
}

void
gsword_notes_store_free(GswordNotesStore *self)
{
    if (self == NULL)
        return;
    sqlite3_close(self->db);
    g_free(self);
}

static GswordNote *
note_from_statement(sqlite3_stmt *stmt)
{
    GswordNote *note = g_new0(GswordNote, 1);

    note->id = sqlite3_column_int64(stmt, 0);
    note->module = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
    note->key = g_strdup((const gchar *)sqlite3_column_text(stmt, 2));
    note->display_key = g_strdup((const gchar *)sqlite3_column_text(stmt, 3));
    note->text = g_strdup((const gchar *)sqlite3_column_text(stmt, 4));
    note->tags = g_strdup((const gchar *)sqlite3_column_text(stmt, 5));
    if (note->tags == NULL)
        note->tags = g_strdup("");
    return note;
}

static const gchar *note_select_sql =
    "SELECT n.id, n.module, n.key, n.display_key, n.text,"
    " COALESCE((SELECT group_concat(t.name, ', ') FROM note_tags nt"
    "  JOIN tags t ON t.id = nt.tag_id WHERE nt.note_id = n.id), '')"
    " FROM notes n";

GswordNote *
gsword_notes_store_get_note(GswordNotesStore *self, const gchar *module,
                            const gchar *key)
{
    sqlite3_stmt *stmt = NULL;
    GswordNote *note = NULL;
    gchar *sql = g_strconcat(note_select_sql,
                             " WHERE n.module = ? AND n.key = ?", NULL);

    if (sqlite3_prepare_v2(self->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, module, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, key, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            note = note_from_statement(stmt);
    }
    sqlite3_finalize(stmt);
    g_free(sql);
    return note;
}

GPtrArray *
gsword_notes_store_list_notes(GswordNotesStore *self, const gchar *query)
{
    GPtrArray *result = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_note_free);
    sqlite3_stmt *stmt = NULL;
    gchar *pattern = g_strdup_printf("%%%s%%",
                                     query != NULL ? query : "");
    gchar *sql = g_strconcat(note_select_sql,
        " WHERE ? = '' OR n.text LIKE ? OR n.display_key LIKE ?"
        " OR EXISTS (SELECT 1 FROM note_tags nt JOIN tags t"
        "  ON t.id = nt.tag_id WHERE nt.note_id = n.id AND t.name LIKE ?)"
        " ORDER BY n.updated DESC LIMIT 200", NULL);

    if (sqlite3_prepare_v2(self->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, query != NULL ? query : "", -1,
                          SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, pattern, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, pattern, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW)
            g_ptr_array_add(result, note_from_statement(stmt));
    }
    sqlite3_finalize(stmt);
    g_free(sql);
    g_free(pattern);
    return result;
}

gboolean
gsword_notes_store_save_note(GswordNotesStore *self, const gchar *module,
                             const gchar *key, const gchar *display_key,
                             const gchar *text, const gchar *tags,
                             GError **error)
{
    sqlite3_stmt *stmt = NULL;
    sqlite3_stmt *tag_stmt = NULL;
    sqlite3_stmt *link_stmt = NULL;
    gint64 note_id = 0;
    gchar **items = NULL;
    gboolean ok = FALSE;
    guint i;
    const gchar *upsert =
        "INSERT INTO notes(module, key, display_key, text, updated)"
        " VALUES(?, ?, ?, ?, strftime('%s','now'))"
        " ON CONFLICT(module, key) DO UPDATE SET"
        " display_key = excluded.display_key, text = excluded.text,"
        " updated = strftime('%s','now')";

    if (sqlite3_prepare_v2(self->db, upsert, -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, module, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, key, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 3, display_key, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 4, text, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        notes_store_error(self, error, "Cannot save note");
        goto out;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (sqlite3_prepare_v2(self->db,
            "SELECT id FROM notes WHERE module = ? AND key = ?", -1, &stmt,
            NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, module, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, key, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_ROW) {
        notes_store_error(self, error, "Cannot locate saved note");
        goto out;
    }
    note_id = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (sqlite3_prepare_v2(self->db,
            "DELETE FROM note_tags WHERE note_id = ?", -1, &link_stmt,
            NULL) != SQLITE_OK ||
        sqlite3_bind_int64(link_stmt, 1, note_id) != SQLITE_OK ||
        sqlite3_step(link_stmt) != SQLITE_DONE) {
        notes_store_error(self, error, "Cannot update note tags");
        goto out;
    }
    sqlite3_finalize(link_stmt);
    link_stmt = NULL;

    if (sqlite3_prepare_v2(self->db,
            "INSERT OR IGNORE INTO tags(name) VALUES(?)", -1, &tag_stmt,
            NULL) != SQLITE_OK ||
        sqlite3_prepare_v2(self->db,
            "INSERT OR IGNORE INTO note_tags(note_id, tag_id)"
            " SELECT ?, id FROM tags WHERE name = ?", -1, &link_stmt,
            NULL) != SQLITE_OK) {
        notes_store_error(self, error, "Cannot prepare note tags");
        goto out;
    }

    items = g_strsplit(tags != NULL ? tags : "", ",", -1);
    for (i = 0; items[i] != NULL; i++) {
        gchar *tag = g_strstrip(items[i]);

        if (*tag == '\0')
            continue;
        sqlite3_reset(tag_stmt);
        sqlite3_bind_text(tag_stmt, 1, tag, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(tag_stmt) != SQLITE_DONE) {
            notes_store_error(self, error, "Cannot save note tag");
            goto out;
        }
        sqlite3_reset(link_stmt);
        sqlite3_bind_int64(link_stmt, 1, note_id);
        sqlite3_bind_text(link_stmt, 2, tag, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(link_stmt) != SQLITE_DONE) {
            notes_store_error(self, error, "Cannot link note tag");
            goto out;
        }
    }
    ok = TRUE;

out:
    g_strfreev(items);
    sqlite3_finalize(stmt);
    sqlite3_finalize(tag_stmt);
    sqlite3_finalize(link_stmt);
    return ok;
}

gboolean
gsword_notes_store_delete_note(GswordNotesStore *self, gint64 id,
                               GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok = FALSE;

    if (sqlite3_prepare_v2(self->db, "DELETE FROM notes WHERE id = ?", -1,
                           &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, id) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        notes_store_error(self, error, "Cannot delete note");
        goto out;
    }
    ok = TRUE;

out:
    sqlite3_finalize(stmt);
    return ok;
}

gboolean
gsword_notes_store_delete_tag(GswordNotesStore *self, const gchar *name,
                              GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok = FALSE;

    if (sqlite3_prepare_v2(self->db,
            "DELETE FROM tags WHERE name = ?", -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        notes_store_error(self, error, "Cannot delete tag");
        goto out;
    }
    ok = TRUE;

out:
    sqlite3_finalize(stmt);
    return ok;
}

GPtrArray *
gsword_notes_store_list_tags(GswordNotesStore *self)
{
    GPtrArray *result = g_ptr_array_new_with_free_func(g_free);
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(self->db, "SELECT name FROM tags ORDER BY name",
                           -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW)
            g_ptr_array_add(result,
                            g_strdup((const gchar *)sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    return result;
}

gboolean
gsword_notes_store_create_list(GswordNotesStore *self, const gchar *name,
                               GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok = FALSE;

    if (sqlite3_prepare_v2(self->db,
            "INSERT OR IGNORE INTO verse_lists(name) VALUES(?)", -1, &stmt,
            NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        notes_store_error(self, error, "Cannot create verse list");
        goto out;
    }
    ok = TRUE;

out:
    sqlite3_finalize(stmt);
    return ok;
}

GPtrArray *
gsword_notes_store_list_lists(GswordNotesStore *self)
{
    GPtrArray *result = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_verse_list_free);
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(self->db,
            "SELECT id, name FROM verse_lists ORDER BY name", -1, &stmt,
            NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            GswordVerseList *list = g_new0(GswordVerseList, 1);

            list->id = sqlite3_column_int64(stmt, 0);
            list->name = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
            g_ptr_array_add(result, list);
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

gboolean
gsword_notes_store_add_to_list(GswordNotesStore *self, gint64 list_id,
                               const gchar *module, const gchar *key,
                               const gchar *display_key, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok = FALSE;

    if (sqlite3_prepare_v2(self->db,
            "INSERT OR IGNORE INTO verse_list_items"
            "(list_id, module, key, display_key) VALUES(?, ?, ?, ?)", -1,
            &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, list_id) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, module, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 3, key, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 4, display_key, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        notes_store_error(self, error, "Cannot add verse to list");
        goto out;
    }
    ok = TRUE;

out:
    sqlite3_finalize(stmt);
    return ok;
}

gboolean
gsword_notes_store_delete_list(GswordNotesStore *self, gint64 list_id,
                               GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok = FALSE;

    if (sqlite3_prepare_v2(self->db,
            "DELETE FROM verse_lists WHERE id = ?", -1, &stmt, NULL) !=
            SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, list_id) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        notes_store_error(self, error, "Cannot delete verse list");
        goto out;
    }
    ok = TRUE;

out:
    sqlite3_finalize(stmt);
    return ok;
}

GPtrArray *
gsword_notes_store_list_items(GswordNotesStore *self, gint64 list_id)
{
    GPtrArray *result = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_verse_list_item_free);
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(self->db,
            "SELECT module, key, display_key FROM verse_list_items"
            " WHERE list_id = ? ORDER BY id", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, list_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            GswordVerseListItem *item = g_new0(GswordVerseListItem, 1);

            item->module = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 0));
            item->key = g_strdup((const gchar *)sqlite3_column_text(stmt, 1));
            item->display_key = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 2));
            g_ptr_array_add(result, item);
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

gboolean
gsword_notes_store_set_highlight(GswordNotesStore *self, const gchar *module,
                                 const gchar *key, const gchar *color,
                                 GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok = FALSE;

    if (sqlite3_prepare_v2(self->db,
            "INSERT OR REPLACE INTO highlights(module, key, color)"
            " VALUES(?, ?, ?)", -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, module, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, key, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 3, color, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        notes_store_error(self, error, "Cannot save highlight");
        goto out;
    }
    ok = TRUE;

out:
    sqlite3_finalize(stmt);
    return ok;
}

gboolean
gsword_notes_store_clear_highlight(GswordNotesStore *self,
                                    const gchar *module, const gchar *key,
                                    GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok = FALSE;

    if (sqlite3_prepare_v2(self->db,
            "DELETE FROM highlights WHERE module = ? AND key = ?", -1, &stmt,
            NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, module, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, key, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        notes_store_error(self, error, "Cannot remove highlight");
        goto out;
    }
    ok = TRUE;

out:
    sqlite3_finalize(stmt);
    return ok;
}

gboolean
gsword_notes_store_has_highlight(GswordNotesStore *self, const gchar *module,
                                 const gchar *key)
{
    sqlite3_stmt *stmt = NULL;
    gboolean result = FALSE;

    if (sqlite3_prepare_v2(self->db,
            "SELECT 1 FROM highlights WHERE module = ? AND key = ? LIMIT 1",
            -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, module, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, key, -1, SQLITE_TRANSIENT);
        result = sqlite3_step(stmt) == SQLITE_ROW;
    }
    sqlite3_finalize(stmt);
    return result;
}
