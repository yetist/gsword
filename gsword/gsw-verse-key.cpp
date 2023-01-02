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

static void gsw_verse_key_init (GswVerseKey *key)
{
	key->vkey = NULL;
}

GswVerseKey* gsw_verse_key_new (gpointer data)
{
	GswVerseKey *key;

	key= (GswVerseKey*) g_object_new (GSW_TYPE_VERSE_KEY, NULL);
	key->vkey = (sword::VerseKey*) data;
	return key;
}

gpointer gsw_verse_key_get_data (GswVerseKey *key)
{
	g_return_val_if_fail(key != NULL, NULL);

	return (gpointer) key->vkey;
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

void gsw_verse_key_set_chapter (GswVerseKey *key, gint ichapter)
{
	g_return_if_fail(key != NULL);

	key->vkey->setChapter(ichapter);
}

gint gsw_verse_key_get_verse (GswVerseKey *key)
{
	g_return_val_if_fail(key != NULL, 0);

	return key->vkey->getVerse();
}

gint gsw_verse_key_get_verse_max (GswVerseKey *key)
{
	g_return_val_if_fail(key != NULL, 0);

	return key->vkey->getVerseMax();
}

const gchar* gsw_verse_key_get_text (GswVerseKey *key)
{
	g_return_val_if_fail(key != NULL, 0);

	return key->vkey->getText();
}

void gsw_verse_key_set_text (GswVerseKey *key, const gchar* ikey)
{
	g_return_if_fail(key != NULL);

	return key->vkey->setText(ikey);
}

gboolean gsw_verse_key_pop_error (GswVerseKey *key)
{
	g_return_val_if_fail(key != NULL, FALSE);

	return key->vkey->popError();
}

void gsw_verse_key_set_auto_normalize (GswVerseKey *key, gboolean iautonorm)
{
	g_return_if_fail(key != NULL);

	return key->vkey->setAutoNormalize(iautonorm);
}

void gsw_verse_key_set_intros (GswVerseKey *key, gboolean val)
{
	g_return_if_fail(key != NULL);

	return key->vkey->setIntros(val);
}

void gsw_verse_key_set_lower_bound (GswVerseKey *key, GswVerseKey *akey)
{
	g_return_if_fail(key != NULL);
	sword::VerseKey *vkey;

	vkey = (sword::VerseKey*) gsw_verse_key_get_data(akey);
	key->vkey->setLowerBound(vkey);
}

void gsw_verse_key_set_position (GswVerseKey *key, GswPosition pos)
{
	g_return_if_fail(key != NULL);

	key->vkey->setPosition(sword::SW_POSITION((char) pos));
}

void gsw_verse_key_set_upper_bound(GswVerseKey *key, GswVerseKey *akey)
{
	g_return_if_fail(key != NULL);
	sword::VerseKey *vkey;

	vkey = (sword::VerseKey*) gsw_verse_key_get_data(akey);
	key->vkey->setUpperBound(vkey);
}

void gsw_verse_key_set_testament (GswVerseKey *key, guint itestament)
{
	g_return_if_fail(key != NULL);

	key->vkey->setTestament(itestament);
}

void gsw_verse_key_clear_bounds (GswVerseKey *key)
{
	g_return_if_fail(key != NULL);

	return key->vkey->clearBounds();
}

const gchar* gsw_verse_key_get_range_text (GswVerseKey *key)
{
	g_return_val_if_fail(key != NULL, NULL);

	return key->vkey->getRangeText();
}

void gsw_verse_key_prev (GswVerseKey *key)
{
	g_return_if_fail(key != NULL);
	key->vkey->decrement(1);
}

gint gsw_verse_key_get_chapter_max (GswVerseKey *key)
{
	g_return_val_if_fail(key != NULL, -1);
	return key->vkey->getChapterMax();
}

const gchar* gsw_verse_key_get_versification_system (GswVerseKey *key)
{
	g_return_val_if_fail(key != NULL, NULL);
	return key->vkey->getVersificationSystem();
}
