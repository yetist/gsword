/*
 * gsw-key-internal.h: internal key helpers
 */

#ifndef __GSW_KEY_INTERNAL_H__
#define __GSW_KEY_INTERNAL_H__ 1

#include <gsword/gsw-key.h>
#include <gsword/gsw-verse-key.h>

G_BEGIN_DECLS

GswKey *gsw_key_new_verse(const gchar *text, const gchar *versification);
GswKey *gsw_key_new_string(const gchar *text);

glong gsw_verse_key_get_index(GswVerseKey *self);
glong gsw_verse_key_get_testament_index(GswVerseKey *self);
void  gsw_verse_key_set_index(GswVerseKey *self, glong index);
gboolean gsw_verse_key_set_last(GswKey *key, glong ot_index, glong nt_index,
                                GError **error);
gint  gsw_verse_key_get_global_book(GswVerseKey *self);
gint  gsw_verse_key_get_global_book_from_name(GswVerseKey *self,
                                              const gchar *name);

G_END_DECLS

#endif /* __GSW_KEY_INTERNAL_H__ */
