/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-verse-key.h: This file is part of ____
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

G_BEGIN_DECLS

#define GSW_TYPE_VERSE_KEY              (gsw_verse_key_get_type ())
G_DECLARE_FINAL_TYPE (GswVerseKey, gsw_verse_key, GSW, VERSE_KEY, GObject);

GType            gsw_verse_key_get_type           (void) G_GNUC_CONST;
GswVerseKey*     gsw_verse_key_new                (gpointer data);
void             gsw_verse_key_set_verse          (GswVerseKey *key, gint iverse);
gint             gsw_verse_key_get_verse          (GswVerseKey *key);
gint             gsw_verse_key_get_chapter        (GswVerseKey *key);
gint             gsw_verse_key_get_book           (GswVerseKey *key);

G_END_DECLS

#endif /* __GSW_VERSE_KEY_H__ */