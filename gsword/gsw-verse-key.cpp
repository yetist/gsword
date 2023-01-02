/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-verse-key.c: This file is part of ____
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

#include <versekey.h>
#include "gsw-verse-key.h"

typedef struct _GswVerseKey        GswVerseKey;

struct _GswVerseKey
{
	GObject object;
	sword::VerseKey* vkey;
};

G_DEFINE_TYPE (GswVerseKey, gsw_verse_key, G_TYPE_OBJECT);

void gsw_verse_key_dispose (GObject *object)
{
	GswVerseKey *key = GSW_VERSE_KEY(object);
	if (key->vkey != NULL)
	{
		delete key->vkey;
		key->vkey = NULL;
	}

	G_OBJECT_CLASS (gsw_verse_key_parent_class)->dispose (object);
}
	
static void gsw_verse_key_class_init (GswVerseKeyClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  	gobject_class->dispose = gsw_verse_key_dispose;
}

static void gsw_verse_key_init (GswVerseKey *verse_key)
{
		verse_key->vkey = NULL;
}

GswVerseKey* gsw_verse_key_new (gpointer data)
{
	GswVerseKey *key;

	key= (GswVerseKey*) g_object_new (GSW_TYPE_VERSE_KEY, NULL);
	key->vkey = new sword::VerseKey();
	return key;
}

void gsw_verse_key_set_verse (GswVerseKey *key, gint iverse)
{
	g_return_if_fail(key != NULL);

	return key->vkey->setVerse(iverse);
}

gint gsw_verse_key_get_book (GswVerseKey *key)
{
	g_return_val_if_fail(key != NULL, 0);

	return key->vkey->getBook();
}

gint gsw_verse_key_get_chapter (GswVerseKey *key)
{
	g_return_val_if_fail(key != NULL, 0);

	return key->vkey->getChapter();
}

gint gsw_verse_key_get_verse (GswVerseKey *key)
{
	g_return_val_if_fail(key != NULL, 0);

	return key->vkey->getVerse();
}
