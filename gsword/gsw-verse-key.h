/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-verse-key.h: This file is part of gsword.
 *
 * Copyright (C) 2017 yetist <yetist@yetibook>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * */

#ifndef __GSW_VERSE_KEY_H__
#define __GSW_VERSE_KEY_H__  1

#include <glib-object.h>
#include <gsw-enum.h>

G_BEGIN_DECLS

#define GSW_TYPE_VERSE_KEY              (gsw_verse_key_get_type ())
G_DECLARE_FINAL_TYPE (GswVerseKey, gsw_verse_key, GSW, VERSE_KEY, GObject);

GswVerseKey*     gsw_verse_key_new_with_boundary  (const gchar* min, const gchar *max, const gchar* v11n);
gpointer         gsw_verse_key_get_data           (GswVerseKey *key);
void             gsw_verse_key_set_verse          (GswVerseKey *key, gint iverse);
gint             gsw_verse_key_get_verse          (GswVerseKey *key);
gint             gsw_verse_key_get_chapter        (GswVerseKey *key);
void             gsw_verse_key_set_chapter        (GswVerseKey *key, gint ichapter);
gint             gsw_verse_key_get_book           (GswVerseKey *key);
gint             gsw_verse_key_get_verse_max      (GswVerseKey *key);
const gchar*     gsw_verse_key_get_text           (GswVerseKey *key);
void             gsw_verse_key_set_text           (GswVerseKey *key, const gchar* ikey);
gboolean         gsw_verse_key_pop_error          (GswVerseKey *key);
void             gsw_verse_key_set_auto_normalize (GswVerseKey *key, gboolean iautonorm);
void             gsw_verse_key_set_intros         (GswVerseKey *key, gboolean val);
void             gsw_verse_key_set_lower_bound    (GswVerseKey *key, GswVerseKey *akey);
void             gsw_verse_key_set_position       (GswVerseKey *key, GswPosition pos);
void             gsw_verse_key_set_upper_bound    (GswVerseKey *key, GswVerseKey *akey);
void             gsw_verse_key_set_testament      (GswVerseKey *key, guint itestament);
void             gsw_verse_key_clear_bounds       (GswVerseKey *key);
const gchar*     gsw_verse_key_get_range_text     (GswVerseKey *key);
void             gsw_verse_key_prev               (GswVerseKey *key);
gint             gsw_verse_key_get_chapter_max    (GswVerseKey *key);
const gchar*     gsw_verse_key_get_versification_system    (GswVerseKey *key);
const gchar*     gsw_verse_key_get_book_name      (GswVerseKey *key);

G_END_DECLS

#endif /* __GSW_VERSE_KEY_H__ */
