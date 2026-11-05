/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * insight-store.h: SQLite-backed free-form insights for the gsword application
 */

#ifndef GSWORD_INSIGHT_STORE_H
#define GSWORD_INSIGHT_STORE_H 1

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GswordInsightStore GswordInsightStore;

typedef struct {
    gchar *module;
    gchar *key;
    gchar *display_key;
} GswordInsightVerse;

typedef struct {
    gint64 id;
    gchar *title;
    gchar *body;
    gint64 created;
    gint64 updated;
    GPtrArray *verses;
    GPtrArray *tags;
} GswordInsight;

GswordInsightStore *gsword_insight_store_new(GError **error);
void gsword_insight_store_free(GswordInsightStore *self);

gint64 gsword_insight_store_create(GswordInsightStore *self,
                                   const gchar *title, const gchar *body,
                                   GPtrArray *verses, GPtrArray *tags,
                                   GError **error);
gboolean gsword_insight_store_update(GswordInsightStore *self, gint64 id,
                                     const gchar *title, const gchar *body,
                                     GPtrArray *verses, GPtrArray *tags,
                                     GError **error);
gboolean gsword_insight_store_delete(GswordInsightStore *self, gint64 id,
                                     GError **error);
GswordInsight *gsword_insight_store_get(GswordInsightStore *self, gint64 id);
GPtrArray *gsword_insight_store_list(GswordInsightStore *self,
                                     const gchar *query, const gchar *tag);
GPtrArray *gsword_insight_store_list_tags(GswordInsightStore *self);

void gsword_insight_free(GswordInsight *insight);
void gsword_insight_verse_free(GswordInsightVerse *verse);
GswordInsightVerse *gsword_insight_verse_new(const gchar *module,
                                             const gchar *key,
                                             const gchar *display_key);

G_END_DECLS

#endif /* GSWORD_INSIGHT_STORE_H */
