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

typedef struct _GswVersification GswVersification;
typedef struct _GswBook GswBook;

#define GSW_TYPE_VERSIFICATION (gsw_versification_get_type())
#define GSW_TYPE_BOOK (gsw_book_get_type())

GType gsw_versification_get_type(void);
GType gsw_book_get_type(void);

GswVersification *gsw_versification_ref(GswVersification *self);
void              gsw_versification_unref(GswVersification *self);
GswBook          *gsw_book_ref(GswBook *self);
void              gsw_book_unref(GswBook *self);

#define GSW_TYPE_VERSIFICATION_MGR (gsw_versification_mgr_get_type())
G_DECLARE_FINAL_TYPE(GswVersificationMgr, gsw_versification_mgr, GSW,
                     VERSIFICATION_MGR, GObject)

GswVersificationMgr *gsw_versification_mgr_get_default(void);
GPtrArray           *gsw_versification_mgr_get_systems(GswVersificationMgr *self);
GswVersification    *gsw_versification_mgr_get_system(GswVersificationMgr *self,
                                                      const gchar *name);

const gchar *gsw_versification_get_name(GswVersification *self);
gint         gsw_versification_get_book_count(GswVersification *self);
GswBook     *gsw_versification_get_book(GswVersification *self, gint number);
gint         gsw_versification_get_book_number_by_osis(GswVersification *self,
                                                       const gchar *osis);
gint         gsw_versification_get_nt_start_offset(GswVersification *self);
void         gsw_versification_get_bmax(GswVersification *self,
                                        gint *ot_books, gint *nt_books);
glong        gsw_versification_get_offset(GswVersification *self, gint book,
                                          gint chapter, gint verse);
gboolean     gsw_versification_get_verse(GswVersification *self, glong offset,
                                         gint *book, gint *chapter, gint *verse);

const gchar *gsw_book_get_name(GswBook *self);
const gchar *gsw_book_get_osis_name(GswBook *self);
const gchar *gsw_book_get_preferred_abbreviation(GswBook *self);
gint         gsw_book_get_chapter_max(GswBook *self);
gint         gsw_book_get_verse_max(GswBook *self, gint chapter);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswVersification, gsw_versification_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswBook, gsw_book_unref)

G_END_DECLS

#endif /* __GSW_VERSIFICATION_H__ */
