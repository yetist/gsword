/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * prayer-store.h: SQLite-backed prayer items for the gsword application
 */

#ifndef GSWORD_PRAYER_STORE_H
#define GSWORD_PRAYER_STORE_H 1

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GswordPrayerStore GswordPrayerStore;

typedef enum {
    GSWORD_PRAYER_STATUS_ACTIVE,
    GSWORD_PRAYER_STATUS_ANSWERED,
    GSWORD_PRAYER_STATUS_ARCHIVED,
    GSWORD_PRAYER_STATUS_ANY = -1
} GswordPrayerStatus;

typedef struct {
    gchar *module;
    gchar *key;
    gchar *display_key;
} GswordPrayerVerse;

typedef struct {
    gint64 id;
    gchar *title;
    gchar *body;
    GswordPrayerStatus status;
    gint64 created;
    gint64 updated;
    gint64 answered_at;
    gchar *answered_note;
    GPtrArray *verses;
} GswordPrayer;

GswordPrayerStore *gsword_prayer_store_new(GError **error);
void gsword_prayer_store_free(GswordPrayerStore *self);

gint64 gsword_prayer_store_create(GswordPrayerStore *self,
                                  const gchar *title,
                                  const gchar *body,
                                  GError **error);
gboolean gsword_prayer_store_update(GswordPrayerStore *self, gint64 id,
                                    const gchar *title, const gchar *body,
                                    GError **error);
gboolean gsword_prayer_store_delete(GswordPrayerStore *self, gint64 id,
                                    GError **error);
GswordPrayer *gsword_prayer_store_get(GswordPrayerStore *self, gint64 id);
GPtrArray *gsword_prayer_store_list(GswordPrayerStore *self,
                                    GswordPrayerStatus status,
                                    const gchar *query);
gint gsword_prayer_store_count(GswordPrayerStore *self,
                               GswordPrayerStatus status);

gboolean gsword_prayer_store_set_status(GswordPrayerStore *self, gint64 id,
                                        GswordPrayerStatus status,
                                        const gchar *answered_note,
                                        GError **error);
gboolean gsword_prayer_store_set_verses(GswordPrayerStore *self, gint64 id,
                                        GPtrArray *verses, GError **error);

void gsword_prayer_free(GswordPrayer *prayer);
void gsword_prayer_verse_free(GswordPrayerVerse *verse);
GswordPrayerVerse *gsword_prayer_verse_new(const gchar *module,
                                           const gchar *key,
                                           const gchar *display_key);

G_END_DECLS

#endif /* GSWORD_PRAYER_STORE_H */
