/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#ifndef GSWORD_NOTES_STORE_H
#define GSWORD_NOTES_STORE_H 1

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GswordNotesStore GswordNotesStore;

typedef struct {
    gint64 id;
    gchar *module;
    gchar *key;
    gchar *display_key;
    gchar *text;
    gchar *tags;
    gint64 updated;
} GswordNote;

typedef struct {
    gint64 id;
    gchar *name;
} GswordVerseList;

typedef struct {
    gchar *module;
    gchar *key;
    gchar *display_key;
} GswordVerseListItem;

GswordNotesStore *gsword_notes_store_new(GError **error);
void gsword_notes_store_free(GswordNotesStore *self);

gboolean gsword_notes_store_save_note(GswordNotesStore *self,
                                      const gchar *module,
                                      const gchar *key,
                                      const gchar *display_key,
                                      const gchar *text,
                                      const gchar *tags,
                                      GError **error);
GswordNote *gsword_notes_store_get_note(GswordNotesStore *self,
                                        const gchar *module,
                                        const gchar *key);
GPtrArray *gsword_notes_store_list_notes(GswordNotesStore *self,
                                         const gchar *query,
                                         const gchar *tag);
gboolean gsword_notes_store_delete_note(GswordNotesStore *self,
                                        gint64 id, GError **error);
void gsword_note_free(GswordNote *note);

GPtrArray *gsword_notes_store_list_tags(GswordNotesStore *self);
gboolean gsword_notes_store_delete_tag(GswordNotesStore *self,
                                       const gchar *name, GError **error);
gboolean gsword_notes_store_rename_tag(GswordNotesStore *self,
                                       const gchar *old_name,
                                       const gchar *new_name,
                                       GError **error);

gboolean gsword_notes_store_create_list(GswordNotesStore *self,
                                        const gchar *name, GError **error);
GPtrArray *gsword_notes_store_list_lists(GswordNotesStore *self);
gboolean gsword_notes_store_delete_list(GswordNotesStore *self,
                                        gint64 list_id, GError **error);
gboolean gsword_notes_store_rename_list(GswordNotesStore *self,
                                        gint64 list_id,
                                        const gchar *new_name,
                                        GError **error);
gboolean gsword_notes_store_add_to_list(GswordNotesStore *self, gint64 list_id,
                                        const gchar *module, const gchar *key,
                                        const gchar *display_key,
                                        GError **error);
gboolean gsword_notes_store_remove_from_list(GswordNotesStore *self,
                                             gint64 list_id,
                                             const gchar *module,
                                             const gchar *key,
                                             GError **error);
gboolean gsword_notes_store_is_in_list(GswordNotesStore *self, gint64 list_id,
                                       const gchar *module,
                                       const gchar *key);
GPtrArray *gsword_notes_store_list_items(GswordNotesStore *self, gint64 list_id);
void gsword_verse_list_free(GswordVerseList *list);
void gsword_verse_list_item_free(GswordVerseListItem *item);

gboolean gsword_notes_store_set_highlight(GswordNotesStore *self,
                                          const gchar *module,
                                          const gchar *key,
                                          const gchar *color,
                                          GError **error);
gboolean gsword_notes_store_clear_highlight(GswordNotesStore *self,
                                            const gchar *module,
                                            const gchar *key,
                                            GError **error);
gboolean gsword_notes_store_has_highlight(GswordNotesStore *self,
                                          const gchar *module,
                                          const gchar *key);

G_END_DECLS

#endif /* GSWORD_NOTES_STORE_H */
