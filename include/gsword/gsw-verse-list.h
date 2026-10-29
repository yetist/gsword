/*
 * gsw-verse-list.h: parsed verse lists and ranges
 *
 * This file is part of gsword.
 */

#ifndef __GSW_VERSE_LIST_H__
#define __GSW_VERSE_LIST_H__ 1

#include <gsword/gsw-verse-key.h>

G_BEGIN_DECLS

/**
 * GswVerseList:
 *
 * An ordered list of verse ranges parsed from text such as
 * <literal>John 3:16-18; Rom 8:1</literal>. Each entry is a #GswVerseKey with
 * lower and upper bounds. The list is reference-counted.
 */
typedef struct _GswVerseList GswVerseList;

#define GSW_TYPE_VERSE_LIST (gsw_verse_list_get_type())

/**
 * gsw_verse_list_get_type:
 *
 * Gets the #GType for #GswVerseList.
 *
 * Returns: the #GType for #GswVerseList
 */
GType        gsw_verse_list_get_type(void);
/**
 * gsw_verse_list_parse:
 * @text: verse list text
 * @context: (nullable): key used to resolve omitted book or chapter names
 * @error: return location for a #GError, or %NULL
 *
 * Parses a list of single verses and ranges. Segments may be separated by
 * semicolons, commas, or newlines. An upper endpoint may omit the book or
 * chapter, for example `John 3:16-18` or `Gen 1:1-2:3`.
 *
 * Returns: (transfer full): a new #GswVerseList, or %NULL on error
 */
GswVerseList *gsw_verse_list_parse(const gchar *text, GswVerseKey *context,
                                   GError **error);
/**
 * gsw_verse_list_ref:
 * @self: a #GswVerseList
 *
 * Increases the reference count of @self.
 *
 * Returns: (transfer full): @self with an increased reference count
 */
GswVerseList *gsw_verse_list_ref(GswVerseList *self);
/**
 * gsw_verse_list_unref:
 * @self: a #GswVerseList
 *
 * Decreases the reference count of @self.
 */
void          gsw_verse_list_unref(GswVerseList *self);
/**
 * gsw_verse_list_get_length:
 * @self: a #GswVerseList
 *
 * Gets the number of parsed ranges.
 *
 * Returns: the number of ranges
 */
guint         gsw_verse_list_get_length(GswVerseList *self);
/**
 * gsw_verse_list_get_range:
 * @self: a #GswVerseList
 * @index: zero-based range index
 *
 * Gets a parsed range. The returned key is owned by @self.
 *
 * Returns: (transfer none): the range key, or %NULL if @index is out of range
 */
GswVerseKey  *gsw_verse_list_get_range(GswVerseList *self, guint index);
/**
 * gsw_verse_list_expand:
 * @self: a #GswVerseList
 * @error: return location for a #GError, or %NULL
 *
 * Expands all ranges into individual verse keys.
 *
 * Returns: (transfer container) (element-type GswVerseKey): an array of
 *   #GswVerseKey, or %NULL on error
 */
GPtrArray    *gsw_verse_list_expand(GswVerseList *self, GError **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswVerseList, gsw_verse_list_unref)

G_END_DECLS

#endif /* __GSW_VERSE_LIST_H__ */
