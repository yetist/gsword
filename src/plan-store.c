/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include "plan-store.h"
#include "app-db.h"

#include <glib/gi18n.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

struct _GswordPlanStore {
    GswordAppDb *app_db;
    sqlite3 *db;
};

static void
plan_store_error(GswordPlanStore *self, GError **error, const gchar *context)
{
    gsword_app_db_set_error(self->app_db, error, context);
}

static gchar *
date_today(void)
{
    GDateTime *now = g_date_time_new_now_local();
    gchar *result = g_date_time_format(now, "%Y-%m-%d");

    g_date_time_unref(now);
    return result;
}

static gchar *
date_add_days(gint days)
{
    GDateTime *now = g_date_time_new_now_local();
    GDateTime *later = g_date_time_add_days(now, days);
    gchar *result = g_date_time_format(later, "%Y-%m-%d");

    g_date_time_unref(later);
    g_date_time_unref(now);
    return result;
}

static GDateTime *
date_parse(const gchar *text)
{
    gint year;
    gint month;
    gint day;

    if (text == NULL ||
        sscanf(text, "%d-%d-%d", &year, &month, &day) != 3)
        return NULL;
    return g_date_time_new_local(year, month, day, 0, 0, 0);
}

static gint
plan_streak(GswordPlanStore *self, gint64 plan_id)
{
    sqlite3_stmt *stmt = NULL;
    GArray *dates = g_array_new(FALSE, FALSE, sizeof(GDateTime *));
    gint streak = 0;
    const gchar *sql =
        "SELECT DISTINCT completed_on FROM plan_days"
        " WHERE plan_id = ? AND completed_on IS NOT NULL"
        " ORDER BY completed_on DESC";
    guint i;

    if (sqlite3_prepare_v2(self->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, plan_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            GDateTime *date = date_parse(
                (const gchar *)sqlite3_column_text(stmt, 0));

            if (date != NULL)
                g_array_append_val(dates, date);
        }
    }
    sqlite3_finalize(stmt);

    if (dates->len > 0) {
        GDateTime *today = g_date_time_new_now_local();
        GDateTime *today_date = g_date_time_new_local(
            g_date_time_get_year(today), g_date_time_get_month(today),
            g_date_time_get_day_of_month(today), 0, 0, 0);
        GDateTime *yesterday = g_date_time_add_days(today_date, -1);
        GDateTime *expected = g_date_time_ref(
            g_array_index(dates, GDateTime *, 0));

        if (g_date_time_compare(expected, today_date) != 0 &&
            g_date_time_compare(expected, yesterday) != 0) {
            g_date_time_unref(expected);
            g_date_time_unref(yesterday);
            g_date_time_unref(today_date);
            g_date_time_unref(today);
            for (i = 0; i < dates->len; i++)
                g_date_time_unref(g_array_index(dates, GDateTime *, i));
            g_array_unref(dates);
            return 0;
        }

        for (i = 0; i < dates->len; i++) {
            GDateTime *date = g_array_index(dates, GDateTime *, i);

            if (g_date_time_compare(date, expected) == 0) {
                GDateTime *previous;
                streak++;
                previous = g_date_time_add_days(expected, -1);
                g_date_time_unref(expected);
                expected = previous;
            } else {
                break;
            }
        }
        g_date_time_unref(expected);
        g_date_time_unref(yesterday);
        g_date_time_unref(today_date);
        g_date_time_unref(today);
    }

    for (i = 0; i < dates->len; i++)
        g_date_time_unref(g_array_index(dates, GDateTime *, i));
    g_array_unref(dates);
    return streak;
}

void
gsword_plan_free(GswordPlan *plan)
{
    if (plan == NULL)
        return;
    g_free(plan->name);
    g_free(plan->start_ref);
    g_free(plan->next_reference);
    g_free(plan);
}

void
gsword_plan_day_free(GswordPlanDay *day)
{
    if (day == NULL)
        return;
    g_free(day->reference);
    g_free(day->completed_on);
    g_free(day);
}

void
gsword_memory_verse_free(GswordMemoryVerse *verse)
{
    if (verse == NULL)
        return;
    g_free(verse->module);
    g_free(verse->key);
    g_free(verse->display_key);
    g_free(verse->text);
    g_free(verse->due);
    g_free(verse);
}

GswordPlanStore *
gsword_plan_store_new(GError **error)
{
    GswordPlanStore *self = g_new0(GswordPlanStore, 1);

    self->app_db = gsword_app_db_open(error);
    if (self->app_db == NULL) {
        g_free(self);
        return NULL;
    }
    self->db = gsword_app_db_get_sqlite(self->app_db);
    return self;
}

void
gsword_plan_store_free(GswordPlanStore *self)
{
    if (self == NULL)
        return;
    gsword_app_db_close(self->app_db);
    g_free(self);
}

GswordPlan *
gsword_plan_store_get_plan(GswordPlanStore *self, gint64 id)
{
    sqlite3_stmt *stmt = NULL;
    GswordPlan *plan = NULL;
    const gchar *sql =
        "SELECT id, name, start_ref, total_days, chapters_per_day,"
        " (SELECT count(*) FROM plan_days"
        "  WHERE plan_id = plans.id AND completed_on IS NOT NULL)"
        " FROM plans WHERE id = ?";

    if (sqlite3_prepare_v2(self->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            plan = g_new0(GswordPlan, 1);
            plan->id = sqlite3_column_int64(stmt, 0);
            plan->name = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 1));
            plan->start_ref = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 2));
            plan->total_days = sqlite3_column_int(stmt, 3);
            plan->chapters_per_day = sqlite3_column_int(stmt, 4);
            plan->completed_days = sqlite3_column_int(stmt, 5);
        }
    }
    sqlite3_finalize(stmt);
    if (plan == NULL)
        return NULL;

    if (sqlite3_prepare_v2(self->db,
            "SELECT day, reference FROM plan_days"
            " WHERE plan_id = ? AND completed_on IS NULL"
            " ORDER BY day LIMIT 1", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            plan->next_day = sqlite3_column_int(stmt, 0);
            plan->next_reference = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 1));
        }
    }
    sqlite3_finalize(stmt);
    plan->streak = plan_streak(self, id);
    return plan;
}

GPtrArray *
gsword_plan_store_list_plans(GswordPlanStore *self)
{
    GPtrArray *result = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_plan_free);
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(self->db,
            "SELECT id FROM plans ORDER BY created DESC", -1, &stmt,
            NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            GswordPlan *plan = gsword_plan_store_get_plan(
                self, sqlite3_column_int64(stmt, 0));

            if (plan != NULL)
                g_ptr_array_add(result, plan);
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

gint64
gsword_plan_store_create_plan(GswordPlanStore *self, const gchar *name,
                              const gchar *start_ref, gint chapters_per_day,
                              GPtrArray *references, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gint64 plan_id = 0;
    guint i;

    if (name == NULL || *name == '\0' || start_ref == NULL ||
        *start_ref == '\0' || references == NULL || references->len == 0) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Plan name, start reference, and at least one day are required");
        return 0;
    }

    if (sqlite3_prepare_v2(self->db,
            "INSERT INTO plans(name, start_ref, total_days, chapters_per_day)"
            " VALUES(?, ?, ?, ?)", -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, start_ref, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 3, (gint)references->len) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 4, chapters_per_day) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        plan_store_error(self, error, "Cannot create reading plan");
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;
    plan_id = sqlite3_last_insert_rowid(self->db);

    if (sqlite3_prepare_v2(self->db,
            "INSERT INTO plan_days(plan_id, day, reference) VALUES(?, ?, ?)",
            -1, &stmt, NULL) != SQLITE_OK) {
        plan_store_error(self, error, "Cannot prepare plan days");
        gsword_plan_store_delete_plan(self, plan_id, NULL);
        return 0;
    }
    for (i = 0; i < references->len; i++) {
        const gchar *reference = g_ptr_array_index(references, i);

        sqlite3_reset(stmt);
        sqlite3_bind_int64(stmt, 1, plan_id);
        sqlite3_bind_int(stmt, 2, (gint)i + 1);
        sqlite3_bind_text(stmt, 3, reference != NULL ? reference : "",
                          -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            plan_store_error(self, error, "Cannot add plan day");
            sqlite3_finalize(stmt);
            gsword_plan_store_delete_plan(self, plan_id, NULL);
            return 0;
        }
    }
    sqlite3_finalize(stmt);
    return plan_id;
}

GPtrArray *
gsword_plan_store_get_plan_days(GswordPlanStore *self, gint64 id)
{
    GPtrArray *result = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_plan_day_free);
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(self->db,
            "SELECT day, reference, completed_on FROM plan_days"
            " WHERE plan_id = ? ORDER BY day", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            GswordPlanDay *day = g_new0(GswordPlanDay, 1);

            day->day = sqlite3_column_int(stmt, 0);
            day->reference = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 1));
            day->completed_on = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 2));
            g_ptr_array_add(result, day);
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

gboolean
gsword_plan_store_complete_day(GswordPlanStore *self, gint64 plan_id,
                               gint day, gboolean completed, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (sqlite3_prepare_v2(self->db,
            "UPDATE plan_days SET completed_on = ?"
            " WHERE plan_id = ? AND day = ?", -1, &stmt, NULL) != SQLITE_OK) {
        plan_store_error(self, error, "Cannot prepare plan progress");
        return FALSE;
    }
    if (completed) {
        gchar *today = date_today();
        sqlite3_bind_text(stmt, 1, today, -1, SQLITE_TRANSIENT);
        g_free(today);
    } else {
        sqlite3_bind_null(stmt, 1);
    }
    sqlite3_bind_int64(stmt, 2, plan_id);
    sqlite3_bind_int(stmt, 3, day);
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok)
        plan_store_error(self, error, "Cannot update plan progress");
    sqlite3_finalize(stmt);
    return ok;
}

gboolean
gsword_plan_store_rename_plan(GswordPlanStore *self, gint64 id,
                              const gchar *new_name, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (new_name == NULL || *new_name == '\0') {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Plan name must not be empty");
        return FALSE;
    }
    if (sqlite3_prepare_v2(self->db,
            "UPDATE plans SET name = ? WHERE id = ?", -1, &stmt, NULL) !=
            SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 2, id) != SQLITE_OK) {
        plan_store_error(self, error, "Cannot prepare plan rename");
        sqlite3_finalize(stmt);
        return FALSE;
    }
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok)
        plan_store_error(self, error, "Cannot rename plan");
    sqlite3_finalize(stmt);
    return ok;
}

gint64
gsword_plan_store_duplicate_plan(GswordPlanStore *self, gint64 id,
                                 GError **error)
{
    GswordPlan *plan = gsword_plan_store_get_plan(self, id);
    sqlite3_stmt *stmt = NULL;
    gint64 new_id = 0;
    gchar *new_name;

    if (plan == NULL) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Plan not found");
        return 0;
    }
    new_name = g_strdup_printf(_("%s (copy)"), plan->name);
    if (sqlite3_prepare_v2(self->db,
            "INSERT INTO plans(name, start_ref, total_days, chapters_per_day)"
            " VALUES(?, ?, ?, ?)", -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, plan->start_ref, -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 3, plan->total_days) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 4, plan->chapters_per_day) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        plan_store_error(self, error, "Cannot duplicate plan");
        sqlite3_finalize(stmt);
        g_free(new_name);
        gsword_plan_free(plan);
        return 0;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;
    new_id = sqlite3_last_insert_rowid(self->db);

    if (sqlite3_prepare_v2(self->db,
            "INSERT INTO plan_days(plan_id, day, reference)"
            " SELECT ?, day, reference FROM plan_days WHERE plan_id = ?"
            " ORDER BY day", -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, new_id) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 2, id) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        plan_store_error(self, error, "Cannot duplicate plan schedule");
        sqlite3_finalize(stmt);
        gsword_plan_store_delete_plan(self, new_id, NULL);
        g_free(new_name);
        gsword_plan_free(plan);
        return 0;
    }
    sqlite3_finalize(stmt);
    g_free(new_name);
    gsword_plan_free(plan);
    return new_id;
}

gboolean
gsword_plan_store_delete_plan(GswordPlanStore *self, gint64 id, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (sqlite3_prepare_v2(self->db, "DELETE FROM plans WHERE id = ?", -1,
                           &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, id) != SQLITE_OK) {
        plan_store_error(self, error, "Cannot prepare plan deletion");
        sqlite3_finalize(stmt);
        return FALSE;
    }
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok)
        plan_store_error(self, error, "Cannot delete plan");
    sqlite3_finalize(stmt);
    return ok;
}

gboolean
gsword_plan_store_add_memory_verse(GswordPlanStore *self,
                                   const gchar *module, const gchar *key,
                                   const gchar *display_key,
                                   const gchar *text, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gchar *today = date_today();
    gboolean ok;

    if (sqlite3_prepare_v2(self->db,
            "INSERT INTO memory_verses(module, key, display_key, text, due)"
            " VALUES(?, ?, ?, ?, ?)"
            " ON CONFLICT(module, key) DO UPDATE SET"
            " display_key = excluded.display_key, text = excluded.text",
            -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, module, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, key, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 3, display_key != NULL ? display_key : "", -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 4, text != NULL ? text : "", -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 5, today, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        plan_store_error(self, error, "Cannot save memory verse");
        ok = FALSE;
    } else {
        ok = TRUE;
    }
    sqlite3_finalize(stmt);
    g_free(today);
    return ok;
}

GPtrArray *
gsword_plan_store_list_memory_verses(GswordPlanStore *self)
{
    GPtrArray *result = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_memory_verse_free);
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(self->db,
            "SELECT id, module, key, display_key, text, box, due"
            " FROM memory_verses ORDER BY due, id", -1, &stmt, NULL) ==
        SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            GswordMemoryVerse *verse = g_new0(GswordMemoryVerse, 1);

            verse->id = sqlite3_column_int64(stmt, 0);
            verse->module = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 1));
            verse->key = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 2));
            verse->display_key = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 3));
            verse->text = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 4));
            verse->box = sqlite3_column_int(stmt, 5);
            verse->due = g_strdup(
                (const gchar *)sqlite3_column_text(stmt, 6));
            g_ptr_array_add(result, verse);
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

gboolean
gsword_plan_store_review_memory_verse(GswordPlanStore *self, gint64 id,
                                      gboolean remembered, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gint box = 0;
    const gint intervals[] = { 0, 1, 3, 7, 14, 30 };
    gchar *due = NULL;
    gboolean ok;

    if (sqlite3_prepare_v2(self->db,
            "SELECT box FROM memory_verses WHERE id = ?", -1, &stmt,
            NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            box = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (remembered) {
        box = MIN(box + 1, 5);
        due = date_add_days(intervals[box]);
    } else {
        box = 0;
        due = date_today();
    }

    if (sqlite3_prepare_v2(self->db,
            "UPDATE memory_verses SET box = ?, due = ? WHERE id = ?",
            -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 1, box) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, due, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 3, id) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        plan_store_error(self, error, "Cannot update memory review");
        ok = FALSE;
    } else {
        ok = TRUE;
    }
    sqlite3_finalize(stmt);
    g_free(due);
    return ok;
}

gboolean
gsword_plan_store_delete_memory_verse(GswordPlanStore *self, gint64 id,
                                      GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (sqlite3_prepare_v2(self->db,
            "DELETE FROM memory_verses WHERE id = ?", -1, &stmt,
            NULL) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 1, id) != SQLITE_OK) {
        plan_store_error(self, error, "Cannot prepare memory verse deletion");
        sqlite3_finalize(stmt);
        return FALSE;
    }
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok)
        plan_store_error(self, error, "Cannot delete memory verse");
    sqlite3_finalize(stmt);
    return ok;
}

gchar *
gsword_plan_store_get_teaching_note(GswordPlanStore *self,
                                    const gchar *module,
                                    const gchar *reference)
{
    sqlite3_stmt *stmt = NULL;
    gchar *notes = NULL;

    if (sqlite3_prepare_v2(self->db,
            "SELECT notes FROM teaching_notes"
            " WHERE module = ? AND reference = ?", -1, &stmt, NULL) ==
        SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, module, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, reference, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            notes = g_strdup((const gchar *)sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return notes;
}

gboolean
gsword_plan_store_save_teaching_note(GswordPlanStore *self,
                                     const gchar *module,
                                     const gchar *reference,
                                     const gchar *notes, GError **error)
{
    sqlite3_stmt *stmt = NULL;
    gboolean ok;

    if (sqlite3_prepare_v2(self->db,
            "INSERT INTO teaching_notes(module, reference, notes, updated)"
            " VALUES(?, ?, ?, strftime('%s','now'))"
            " ON CONFLICT(module, reference) DO UPDATE SET"
            " notes = excluded.notes, updated = strftime('%s','now')",
            -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 1, module, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, reference, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 3, notes != NULL ? notes : "", -1,
                          SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE) {
        plan_store_error(self, error, "Cannot save teaching notes");
        ok = FALSE;
    } else {
        ok = TRUE;
    }
    sqlite3_finalize(stmt);
    return ok;
}
