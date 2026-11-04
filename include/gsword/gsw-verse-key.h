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

/**
 * GswVerseKey:
 *
 * A Bible verse key referencing a testament, book, chapter, and verse. It can
 * be used standalone with a versification system or as the key of a Bible
 * module.
 */
typedef GswKey GswVerseKey;

#define GSW_TYPE_VERSE_KEY (gsw_verse_key_get_type())

/**
 * gsw_verse_key_get_type:
 *
 * Gets the #GType for #GswVerseKey.
 *
 * Returns: the #GType for #GswVerseKey
 */
GType        gsw_verse_key_get_type(void);

/**
 * gsw_verse_key_get_global_book_number:
 * @self: a #GswVerseKey
 *
 * Gets the zero-based global book number across both testaments.
 *
 * Returns: the global book number, or -1 for headings or invalid keys
 */
gint         gsw_verse_key_get_global_book_number(GswVerseKey *self);
/**
 * gsw_verse_key_get_osis_ref:
 * @self: a #GswVerseKey
 *
 * Gets the OSIS reference for @self, for example `John.3.16`.
 *
 * Returns: (transfer full): the OSIS reference, or %NULL for headings or
 *   invalid keys
 */
gchar       *gsw_verse_key_get_osis_ref(GswVerseKey *self);
/**
 * gsw_verse_key_get_short_text:
 * @self: a #GswVerseKey
 *
 * Gets a short display form using the preferred book abbreviation.
 *
 * Returns: (transfer full): the short reference, or %NULL for headings or
 *   invalid keys
 */
gchar       *gsw_verse_key_get_short_text(GswVerseKey *self);

/**
 * gsw_verse_key_new:
 * @text: key text
 * @versification: versification system name, or %NULL for the default
 *
 * Creates a verse key from text.
 *
 * Returns: (transfer full): a new #GswVerseKey
 */
GswVerseKey *gsw_verse_key_new(const gchar *text, const gchar *versification);
/**
 * gsw_verse_key_new_with_bounds:
 * @lower: lower boundary key
 * @upper: upper boundary key
 * @versification: versification system name, or %NULL for the default
 *
 * Creates a verse key with lower and upper bounds.
 *
 * Returns: (transfer full): a new #GswVerseKey
 */
GswVerseKey *gsw_verse_key_new_with_bounds(const gchar *lower,
                                           const gchar *upper,
                                           const gchar *versification);

/**
 * gsw_verse_key_get_testament:
 * @self: a #GswVerseKey
 *
 * Gets the testament.
 *
 * Returns: the testament number
 */
gint         gsw_verse_key_get_testament(GswVerseKey *self);
/**
 * gsw_verse_key_get_book:
 * @self: a #GswVerseKey
 *
 * Gets the book.
 *
 * Returns: the book number
 */
gint         gsw_verse_key_get_book(GswVerseKey *self);
/**
 * gsw_verse_key_get_chapter:
 * @self: a #GswVerseKey
 *
 * Gets the chapter.
 *
 * Returns: the chapter number
 */
gint         gsw_verse_key_get_chapter(GswVerseKey *self);
/**
 * gsw_verse_key_get_verse:
 * @self: a #GswVerseKey
 *
 * Gets the verse.
 *
 * Returns: the verse number
 */
gint         gsw_verse_key_get_verse(GswVerseKey *self);

/**
 * gsw_verse_key_set_testament:
 * @self: a #GswVerseKey
 * @testament: testament number
 *
 * Sets the testament.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_verse_key_set_testament(GswVerseKey *self, gint testament);
/**
 * gsw_verse_key_set_book:
 * @self: a #GswVerseKey
 * @book: book number
 *
 * Sets the book.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_verse_key_set_book(GswVerseKey *self, gint book);
/**
 * gsw_verse_key_set_chapter:
 * @self: a #GswVerseKey
 * @chapter: chapter number
 *
 * Sets the chapter.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_verse_key_set_chapter(GswVerseKey *self, gint chapter);
/**
 * gsw_verse_key_set_verse:
 * @self: a #GswVerseKey
 * @verse: verse number
 *
 * Sets the verse.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_verse_key_set_verse(GswVerseKey *self, gint verse);

/**
 * gsw_verse_key_get_book_name:
 * @self: a #GswVerseKey
 *
 * Gets the localized book name using the key's display locale. The locale is
 * taken from gsw_key_set_locale(), then the owning module's language, then the
 * locale manager's default locale. It is not derived from process environment
 * variables.
 *
 * Returns: (transfer none): the localized book name
 */
const gchar *gsw_verse_key_get_book_name(GswVerseKey *self);
/**
 * gsw_verse_key_set_book_name:
 * @self: a #GswVerseKey
 * @name: book name, OSIS name, or abbreviation
 *
 * Sets the book by name, OSIS name, or abbreviation.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_verse_key_set_book_name(GswVerseKey *self, const gchar *name);
/**
 * gsw_verse_key_get_versification:
 * @self: a #GswVerseKey
 *
 * Gets the versification system name.
 *
 * Returns: (transfer none): the versification system name
 */
const gchar *gsw_verse_key_get_versification(GswVerseKey *self);
/**
 * gsw_verse_key_get_chapter_max:
 * @self: a #GswVerseKey
 *
 * Gets the maximum chapter number for the current book.
 *
 * Returns: the maximum chapter number for the current book
 */
gint         gsw_verse_key_get_chapter_max(GswVerseKey *self);
/**
 * gsw_verse_key_get_verse_max:
 * @self: a #GswVerseKey
 *
 * Gets the maximum verse number for the current chapter.
 *
 * Returns: the maximum verse number for the current chapter
 */
gint         gsw_verse_key_get_verse_max(GswVerseKey *self);

/**
 * gsw_verse_key_set_auto_normalize:
 * @self: a #GswVerseKey
 * @enabled: whether to enable automatic clamping
 *
 * Enables or disables automatic clamping of out-of-range references.
 */
void         gsw_verse_key_set_auto_normalize(GswVerseKey *self, gboolean enabled);
/**
 * gsw_verse_key_get_auto_normalize:
 * @self: a #GswVerseKey
 *
 * Checks whether automatic clamping is enabled.
 *
 * Returns: %TRUE if automatic clamping is enabled, %FALSE otherwise
 */
gboolean     gsw_verse_key_get_auto_normalize(GswVerseKey *self);
/**
 * gsw_verse_key_set_intros:
 * @self: a #GswVerseKey
 * @enabled: whether introduction entries are allowed
 *
 * Sets whether introduction entries (chapter or verse 0) are valid.
 */
void         gsw_verse_key_set_intros(GswVerseKey *self, gboolean enabled);
/**
 * gsw_verse_key_get_intros:
 * @self: a #GswVerseKey
 *
 * Checks whether introduction entries are allowed.
 *
 * Returns: %TRUE if introduction entries are allowed, %FALSE otherwise
 */
gboolean     gsw_verse_key_get_intros(GswVerseKey *self);
/**
 * gsw_verse_key_is_valid:
 * @self: a #GswVerseKey
 *
 * Checks whether the key is a valid verse reference.
 *
 * Returns: %TRUE if the condition holds, %FALSE otherwise
 */
gboolean     gsw_verse_key_is_valid(GswVerseKey *self);

/**
 * gsw_verse_key_set_lower_bound:
 * @self: a #GswVerseKey
 * @bound: the boundary key, or %NULL to clear it
 *
 * Sets the lower bound of the key.
 */
void         gsw_verse_key_set_lower_bound(GswVerseKey *self, GswVerseKey *bound);
/**
 * gsw_verse_key_set_upper_bound:
 * @self: a #GswVerseKey
 * @bound: the boundary key, or %NULL to clear it
 *
 * Sets the upper bound of the key.
 */
void         gsw_verse_key_set_upper_bound(GswVerseKey *self, GswVerseKey *bound);
/**
 * gsw_verse_key_get_lower_bound:
 * @self: a #GswVerseKey
 *
 * Gets the lower bound as a new key.
 *
 * Returns: (transfer full): the lower bound, or %NULL if no lower bound is set
 */
GswVerseKey *gsw_verse_key_get_lower_bound(GswVerseKey *self);
/**
 * gsw_verse_key_get_upper_bound:
 * @self: a #GswVerseKey
 *
 * Gets the upper bound as a new key.
 *
 * Returns: (transfer full): the upper bound, or %NULL if no upper bound is set
 */
GswVerseKey *gsw_verse_key_get_upper_bound(GswVerseKey *self);
/**
 * gsw_verse_key_clear_bounds:
 * @self: a #GswVerseKey
 *
 * Removes the current bounds.
 */
void         gsw_verse_key_clear_bounds(GswVerseKey *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswVerseKey, gsw_key_unref)

G_END_DECLS

#endif /* __GSW_VERSE_KEY_H__ */
