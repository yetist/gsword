/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GSWORD_PLAN_STORE_H
#define GSWORD_PLAN_STORE_H 1

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GswordPlanStore GswordPlanStore;

typedef struct {
    gint64 id;
    gchar *name;
    gchar *start_ref;
    gint total_days;
    gint chapters_per_day;
    gint completed_days;
    gint streak;
    gint next_day;
    gchar *next_reference;
} GswordPlan;

typedef struct {
    gint day;
    gchar *reference;
    gchar *completed_on;
} GswordPlanDay;

typedef struct {
    gint64 id;
    gchar *module;
    gchar *key;
    gchar *display_key;
    gchar *text;
    gint box;
    gchar *due;
} GswordMemoryVerse;

GswordPlanStore *gsword_plan_store_new(GError **error);
void gsword_plan_store_free(GswordPlanStore *self);

gint64 gsword_plan_store_create_plan(GswordPlanStore *self,
                                     const gchar *name,
                                     const gchar *start_ref,
                                     gint chapters_per_day,
                                     GPtrArray *references,
                                     GError **error);
GPtrArray *gsword_plan_store_list_plans(GswordPlanStore *self);
GswordPlan *gsword_plan_store_get_plan(GswordPlanStore *self, gint64 id);
GPtrArray *gsword_plan_store_get_plan_days(GswordPlanStore *self, gint64 id);
gboolean gsword_plan_store_complete_day(GswordPlanStore *self, gint64 plan_id,
                                        gint day, gboolean completed,
                                        GError **error);
gboolean gsword_plan_store_rename_plan(GswordPlanStore *self, gint64 id,
                                       const gchar *new_name,
                                       GError **error);
gint64 gsword_plan_store_duplicate_plan(GswordPlanStore *self, gint64 id,
                                        GError **error);
gboolean gsword_plan_store_delete_plan(GswordPlanStore *self, gint64 id,
                                       GError **error);
void gsword_plan_free(GswordPlan *plan);
void gsword_plan_day_free(GswordPlanDay *day);

gboolean gsword_plan_store_add_memory_verse(GswordPlanStore *self,
                                            const gchar *module,
                                            const gchar *key,
                                            const gchar *display_key,
                                            const gchar *text,
                                            GError **error);
GPtrArray *gsword_plan_store_list_memory_verses(GswordPlanStore *self);
gboolean gsword_plan_store_review_memory_verse(GswordPlanStore *self,
                                               gint64 id,
                                               gboolean remembered,
                                               GError **error);
gboolean gsword_plan_store_delete_memory_verse(GswordPlanStore *self,
                                               gint64 id,
                                               GError **error);
void gsword_memory_verse_free(GswordMemoryVerse *verse);

gchar *gsword_plan_store_get_teaching_note(GswordPlanStore *self,
                                           const gchar *module,
                                           const gchar *reference);
gboolean gsword_plan_store_save_teaching_note(GswordPlanStore *self,
                                              const gchar *module,
                                              const gchar *reference,
                                              const gchar *notes,
                                              GError **error);

G_END_DECLS

#endif /* GSWORD_PLAN_STORE_H */
