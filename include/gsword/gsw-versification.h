/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-versification.h: SWORD versification systems
 *
 * This file is part of gsword.
 */

#ifndef __GSW_VERSIFICATION_H__
#define __GSW_VERSIFICATION_H__ 1

#include <glib-object.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

/**
 * GswVersification:
 *
 * An immutable versification system describing the books, chapters, and
 * verses used by a module. Reference-counted with gsw_versification_ref() and
 * gsw_versification_unref().
 */
typedef struct _GswVersification GswVersification;

/**
 * GswBook:
 *
 * A book in a #GswVersification, including its OSIS name, localized name,
 * preferred abbreviation, and chapter and verse limits. Reference-counted with
 * gsw_book_ref() and gsw_book_unref().
 */
typedef struct _GswBook GswBook;

#define GSW_TYPE_VERSIFICATION (gsw_versification_get_type())
#define GSW_TYPE_BOOK (gsw_book_get_type())

/**
 * gsw_versification_get_type:
 *
 * Gets the #GType for #GswVersification.
 *
 * Returns: the #GType for #GswVersification
 */
GType gsw_versification_get_type(void);
/**
 * gsw_book_get_type:
 *
 * Gets the #GType for #GswBook.
 *
 * Returns: the #GType for #GswBook
 */
GType gsw_book_get_type(void);

/**
 * gsw_versification_ref:
 * @self: a #GswVersification
 *
 * Increases the reference count of the versification system.
 *
 * Returns: (transfer full): @self with an increased reference count
 */
GswVersification *gsw_versification_ref(GswVersification *self);
/**
 * gsw_versification_unref:
 * @self: a #GswVersification
 *
 * Decreases the reference count of the versification system.
 */
void              gsw_versification_unref(GswVersification *self);
/**
 * gsw_book_ref:
 * @self: a #GswBook
 *
 * Increases the reference count of the book.
 *
 * Returns: (transfer full): @self with an increased reference count
 */
GswBook          *gsw_book_ref(GswBook *self);
/**
 * gsw_book_unref:
 * @self: a #GswBook
 *
 * Decreases the reference count of the book.
 */
void              gsw_book_unref(GswBook *self);

/**
 * GswVersificationMgr:
 *
 * Registry of available #GswVersification systems. A process-wide instance is
 * available through gsw_versification_mgr_get_default().
 */
#define GSW_TYPE_VERSIFICATION_MGR (gsw_versification_mgr_get_type())
G_DECLARE_FINAL_TYPE(GswVersificationMgr, gsw_versification_mgr, GSW,
                     VERSIFICATION_MGR, GObject)

/**
 * gsw_versification_mgr_get_default:
 *
 * Gets the process-wide versification manager.
 *
 * Returns: (transfer none): the process-wide versification manager
 */
GswVersificationMgr *gsw_versification_mgr_get_default(void);
/**
 * gsw_versification_mgr_get_systems:
 * @self: a #GswVersificationMgr
 *
 * Gets all registered versification systems.
 *
 * Returns: (transfer container) (element-type GswVersification): a newly allocated array of #GswVersification
 */
GPtrArray           *gsw_versification_mgr_get_systems(GswVersificationMgr *self);
/**
 * gsw_versification_mgr_get_system:
 * @self: a #GswVersificationMgr
 * @name: versification system name
 *
 * Gets a versification system by name.
 *
 * Returns: (transfer none): the versification system, or %NULL if not found
 */
GswVersification    *gsw_versification_mgr_get_system(GswVersificationMgr *self,
                                                      const gchar *name);

/**
 * gsw_versification_get_name:
 * @self: a #GswVersification
 *
 * Gets the versification system name.
 *
 * Returns: (transfer none): the versification system name
 */
const gchar *gsw_versification_get_name(GswVersification *self);
/**
 * gsw_versification_get_book_count:
 * @self: a #GswVersification
 *
 * Gets the number of books.
 *
 * Returns: the number of books
 */
gint         gsw_versification_get_book_count(GswVersification *self);
/**
 * gsw_versification_get_book:
 * @self: a #GswVersification
 * @number: zero-based book number
 *
 * Gets a book by zero-based number.
 *
 * Returns: (transfer none): the book, or %NULL if @number is out of range
 */
GswBook     *gsw_versification_get_book(GswVersification *self, gint number);
/**
 * gsw_versification_get_book_number_by_osis:
 * @self: a #GswVersification
 * @osis: OSIS book name
 *
 * Finds a book number by OSIS name.
 *
 * Returns: the zero-based book number, or -1 if @osis is not found
 */
gint         gsw_versification_get_book_number_by_osis(GswVersification *self,
                                                       const gchar *osis);
/**
 * gsw_versification_get_nt_start_offset:
 * @self: a #GswVersification
 *
 * Gets the global offset at which the New Testament starts.
 *
 * Returns: the global offset at which the New Testament starts
 */
gint         gsw_versification_get_nt_start_offset(GswVersification *self);
/**
 * gsw_versification_get_bmax:
 * @self: a #GswVersification
 * @ot_books: return location for the number of Old Testament books
 * @nt_books: return location for the number of New Testament books
 *
 * Gets the number of books in the Old and New Testaments.
 */
void         gsw_versification_get_bmax(GswVersification *self,
                                        gint *ot_books, gint *nt_books);
/**
 * gsw_versification_get_offset:
 * @self: a #GswVersification
 * @book: book number
 * @chapter: chapter number
 * @verse: verse number
 *
 * Computes a global offset from a book, chapter, and verse.
 *
 * Returns: the global offset, or -1 if the reference is invalid
 */
glong        gsw_versification_get_offset(GswVersification *self, gint book,
                                          gint chapter, gint verse);
/**
 * gsw_versification_get_verse:
 * @self: a #GswVersification
 * @offset: global versification offset
 * @book: return location for the book number
 * @chapter: return location for the chapter number
 * @verse: return location for the verse number
 *
 * Converts a global offset to a book, chapter, and verse.
 *
 * Returns: %TRUE if @offset was valid, %FALSE otherwise
 */
gboolean     gsw_versification_get_verse(GswVersification *self, glong offset,
                                         gint *book, gint *chapter, gint *verse);

/**
 * gsw_book_get_name:
 * @self: a #GswBook
 *
 * Gets the canonical long book name, for example `Genesis`. This is the
 * stable name stored with the versification, normally English, and is not
 * affected by the process locale or the locale manager's default locale. Use
 * gsw_book_get_name_for_locale() for display.
 *
 * Returns: (transfer none): the canonical long book name
 */
const gchar *gsw_book_get_name(GswBook *self);
/**
 * gsw_book_get_name_for_locale:
 * @self: a #GswBook
 * @locale: locale name, or %NULL for the locale manager default
 *
 * Gets the long book name translated for an explicit locale. This function
 * only uses @locale and the locale manager's explicit default; it never reads
 * process environment variables.
 *
 * Returns: (transfer none): the translated long book name, or the canonical
 *   name if no translation exists
 */
const gchar *gsw_book_get_name_for_locale(GswBook *self,
                                          const gchar *locale);
/**
 * gsw_book_get_osis_name:
 * @self: a #GswBook
 *
 * Gets the OSIS book name.
 *
 * Returns: (transfer none): the OSIS book name
 */
const gchar *gsw_book_get_osis_name(GswBook *self);
/**
 * gsw_book_get_preferred_abbreviation:
 * @self: a #GswBook
 *
 * Gets the canonical preferred abbreviation.
 *
 * Returns: (transfer none): the canonical preferred abbreviation
 */
const gchar *gsw_book_get_preferred_abbreviation(GswBook *self);
/**
 * gsw_book_get_preferred_abbreviation_for_locale:
 * @self: a #GswBook
 * @locale: locale name, or %NULL for the locale manager default
 *
 * Gets the preferred abbreviation translated for an explicit locale. This
 * function never reads process environment variables.
 *
 * Returns: (transfer none): the translated abbreviation, or the canonical
 *   abbreviation if no translation exists
 */
const gchar *gsw_book_get_preferred_abbreviation_for_locale(
                                                GswBook *self,
                                                const gchar *locale);
/**
 * gsw_book_get_chapter_max:
 * @self: a #GswBook
 *
 * Gets the maximum chapter number.
 *
 * Returns: the maximum chapter number
 */
gint         gsw_book_get_chapter_max(GswBook *self);
/**
 * gsw_book_get_verse_max:
 * @self: a #GswBook
 * @chapter: chapter number
 *
 * Gets the maximum verse number in a chapter.
 *
 * Returns: the maximum verse number in @chapter
 */
gint         gsw_book_get_verse_max(GswBook *self, gint chapter);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswVersification, gsw_versification_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswBook, gsw_book_unref)

G_END_DECLS

#endif /* __GSW_VERSIFICATION_H__ */
