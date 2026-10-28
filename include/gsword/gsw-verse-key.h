/*
 * gsw-verse-key.h: Bible verse key
 *
 * This file is part of gsword.
 */

#ifndef __GSW_VERSE_KEY_H__
#define __GSW_VERSE_KEY_H__ 1

#include <gsword/gsw-key.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

typedef GswKey GswVerseKey;

#define GSW_TYPE_VERSE_KEY (gsw_verse_key_get_type())

GType        gsw_verse_key_get_type(void);

/**
 * gsw_verse_key_new:
 *
 * Returns: (transfer full): result
 */
GswVerseKey *gsw_verse_key_new(const gchar *text, const gchar *versification);
/**
 * gsw_verse_key_new_with_bounds:
 *
 * Returns: (transfer full): result
 */
GswVerseKey *gsw_verse_key_new_with_bounds(const gchar *lower,
                                           const gchar *upper,
                                           const gchar *versification);

gint         gsw_verse_key_get_testament(GswVerseKey *self);
gint         gsw_verse_key_get_book(GswVerseKey *self);
gint         gsw_verse_key_get_chapter(GswVerseKey *self);
gint         gsw_verse_key_get_verse(GswVerseKey *self);

gboolean     gsw_verse_key_set_testament(GswVerseKey *self, gint testament);
gboolean     gsw_verse_key_set_book(GswVerseKey *self, gint book);
gboolean     gsw_verse_key_set_chapter(GswVerseKey *self, gint chapter);
gboolean     gsw_verse_key_set_verse(GswVerseKey *self, gint verse);

const gchar *gsw_verse_key_get_book_name(GswVerseKey *self);
gboolean     gsw_verse_key_set_book_name(GswVerseKey *self, const gchar *name);
const gchar *gsw_verse_key_get_versification(GswVerseKey *self);
gint         gsw_verse_key_get_chapter_max(GswVerseKey *self);
gint         gsw_verse_key_get_verse_max(GswVerseKey *self);

void         gsw_verse_key_set_auto_normalize(GswVerseKey *self, gboolean enabled);
gboolean     gsw_verse_key_get_auto_normalize(GswVerseKey *self);
void         gsw_verse_key_set_intros(GswVerseKey *self, gboolean enabled);
gboolean     gsw_verse_key_get_intros(GswVerseKey *self);
gboolean     gsw_verse_key_is_valid(GswVerseKey *self);

void         gsw_verse_key_set_lower_bound(GswVerseKey *self, GswVerseKey *bound);
void         gsw_verse_key_set_upper_bound(GswVerseKey *self, GswVerseKey *bound);
void         gsw_verse_key_clear_bounds(GswVerseKey *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswVerseKey, gsw_key_unref)

G_END_DECLS

#endif /* __GSW_VERSE_KEY_H__ */
