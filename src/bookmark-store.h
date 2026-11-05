/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * bookmark-store.h: SQLite-backed reading-position bookmarks
 */

#ifndef GSWORD_BOOKMARK_STORE_H
#define GSWORD_BOOKMARK_STORE_H 1

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GswordBookmarkStore GswordBookmarkStore;

typedef struct {
    gint64 id;
    gchar *title;
    gchar *module;
    gchar *key;
    gchar *display_key;
    gchar *note;
    gint64 created;
} GswordBookmark;

GswordBookmarkStore *gsword_bookmark_store_new(GError **error);
void gsword_bookmark_store_free(GswordBookmarkStore *self);

gint64 gsword_bookmark_store_create(GswordBookmarkStore *self,
                                    const gchar *title,
                                    const gchar *module,
                                    const gchar *key,
                                    const gchar *display_key,
                                    const gchar *note,
                                    GError **error);
gboolean gsword_bookmark_store_update(GswordBookmarkStore *self, gint64 id,
                                      const gchar *title,
                                      const gchar *note, GError **error);
gboolean gsword_bookmark_store_delete(GswordBookmarkStore *self, gint64 id,
                                      GError **error);
GswordBookmark *gsword_bookmark_store_get(GswordBookmarkStore *self,
                                          gint64 id);
GPtrArray *gsword_bookmark_store_list(GswordBookmarkStore *self);
void gsword_bookmark_free(GswordBookmark *bookmark);

G_END_DECLS

#endif /* GSWORD_BOOKMARK_STORE_H */
