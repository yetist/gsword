/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-versification.c: SWORD versification systems
 */

#include <gsword/gsw-versification.h>
#include <gsword/gsw-locale.h>
#include "gsw-versification-data.h"

#include <string.h>

struct _GswBook {
    gatomicrefcount ref_count;
    gchar *name;
    gchar *osis;
    gchar *abbrev;
    gint chapmax;
    gint *verse_max;   /* chapmax entries */
    glong *offsets;    /* chapmax entries: absolute offset of chapter heading */
};

struct _GswVersification {
    gatomicrefcount ref_count;
    gchar *name;
    GPtrArray *books;  /* GswBook* */
    GHashTable *osis_lookup; /* osis -> 0-based book index */
    gint bmax[2];
    glong nt_start_offset;
};

struct _GswVersificationMgr {
    GObject parent_instance;
};

typedef struct {
    GHashTable *systems; /* name -> GswVersification* */
    GPtrArray *order;    /* GswVersification* pointers */
} GswVersificationMgrPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GswVersificationMgr, gsw_versification_mgr,
                           G_TYPE_OBJECT)

static GswVersificationMgr *default_manager = NULL;

static void
gsw_book_free(GswBook *book)
{
    if (book == NULL)
        return;

    g_free(book->name);
    g_free(book->osis);
    g_free(book->abbrev);
    g_free(book->verse_max);
    g_free(book->offsets);
    g_free(book);
}

static GswBook *
gsw_book_copy(GswBook *book)
{
    GswBook *copy;

    if (book == NULL)
        return NULL;

    copy = g_new0(GswBook, 1);
    g_atomic_ref_count_init(&copy->ref_count);
    copy->name = g_strdup(book->name);
    copy->osis = g_strdup(book->osis);
    copy->abbrev = g_strdup(book->abbrev);
    copy->chapmax = book->chapmax;
    copy->verse_max = g_new(gint, (gsize)book->chapmax);
    memcpy(copy->verse_max, book->verse_max,
           sizeof(gint) * (gsize)book->chapmax);
    copy->offsets = g_new(glong, (gsize)book->chapmax);
    memcpy(copy->offsets, book->offsets,
           sizeof(glong) * (gsize)book->chapmax);
    return copy;
}

GType
gsw_book_get_type(void)
{
    static gsize type = 0;

    if (g_once_init_enter(&type)) {
        GType registered = g_boxed_type_register_static(
            "GswBook", (GBoxedCopyFunc)gsw_book_copy,
            (GBoxedFreeFunc)gsw_book_unref);
        g_once_init_leave(&type, registered);
    }
    return (GType)type;
}

GswBook *
gsw_book_ref(GswBook *self)
{
    g_atomic_ref_count_inc(&self->ref_count);
    return self;
}

void
gsw_book_unref(GswBook *self)
{
    if (self == NULL)
        return;
    if (g_atomic_ref_count_dec(&self->ref_count))
        gsw_book_free(self);
}

static GswVersification *
gsw_versification_copy(GswVersification *self)
{
    return gsw_versification_ref(self);
}

GType
gsw_versification_get_type(void)
{
    static gsize type = 0;

    if (g_once_init_enter(&type)) {
        GType registered = g_boxed_type_register_static(
            "GswVersification", (GBoxedCopyFunc)gsw_versification_copy,
            (GBoxedFreeFunc)gsw_versification_unref);
        g_once_init_leave(&type, registered);
    }
    return (GType)type;
}

GswVersification *
gsw_versification_ref(GswVersification *self)
{
    g_atomic_ref_count_inc(&self->ref_count);
    return self;
}

void
gsw_versification_unref(GswVersification *self)
{
    if (self == NULL)
        return;
    if (g_atomic_ref_count_dec(&self->ref_count)) {
        g_free(self->name);
        g_ptr_array_unref(self->books);
        g_hash_table_unref(self->osis_lookup);
        g_free(self);
    }
}

static GswVersification *
gsw_versification_build(const gchar *name,
                        const GswV11nBook *ot, const GswV11nBook *nt,
                        const gint *verse_max)
{
    GswVersification *self = g_new0(GswVersification, 1);
    glong offset = 0;
    gint chapter = 0;
    gint book_index = 0;

    g_atomic_ref_count_init(&self->ref_count);
    self->name = g_strdup(name);
    self->books = g_ptr_array_new_with_free_func((GDestroyNotify)gsw_book_unref);
    self->osis_lookup = g_hash_table_new_full(g_str_hash, g_str_equal,
                                              g_free, NULL);

    offset = 0;
    offset++; /* OT testament heading */

    for (; ot->chapmax > 0; ot++, book_index++) {
        GswBook *book = g_new0(GswBook, 1);
        gint c;

        g_atomic_ref_count_init(&book->ref_count);
        book->name = g_strdup(ot->name);
        book->osis = g_strdup(ot->osis);
        book->abbrev = g_strdup(ot->abbrev);
        book->chapmax = ot->chapmax;
        book->verse_max = g_new0(gint, (gsize)book->chapmax);
        book->offsets = g_new0(glong, (gsize)book->chapmax);

        offset++; /* book heading */
        for (c = 0; c < book->chapmax; c++) {
            book->verse_max[c] = verse_max[chapter++];
            offset++; /* chapter heading */
            book->offsets[c] = offset;
            offset += book->verse_max[c];
        }

        g_ptr_array_add(self->books, book);
        g_hash_table_insert(self->osis_lookup, g_strdup(book->osis),
                            GINT_TO_POINTER(book_index));
    }
    self->bmax[0] = book_index;

    self->nt_start_offset = offset;
    offset++; /* NT testament heading */

    for (; nt->chapmax > 0; nt++, book_index++) {
        GswBook *book = g_new0(GswBook, 1);
        gint c;

        g_atomic_ref_count_init(&book->ref_count);
        book->name = g_strdup(nt->name);
        book->osis = g_strdup(nt->osis);
        book->abbrev = g_strdup(nt->abbrev);
        book->chapmax = nt->chapmax;
        book->verse_max = g_new0(gint, (gsize)book->chapmax);
        book->offsets = g_new0(glong, (gsize)book->chapmax);

        offset++; /* book heading */
        for (c = 0; c < book->chapmax; c++) {
            book->verse_max[c] = verse_max[chapter++];
            offset++; /* chapter heading */
            book->offsets[c] = offset;
            offset += book->verse_max[c];
        }

        g_ptr_array_add(self->books, book);
        g_hash_table_insert(self->osis_lookup, g_strdup(book->osis),
                            GINT_TO_POINTER(book_index));
    }
    self->bmax[1] = book_index - self->bmax[0];

    return self;
}

static void
gsw_versification_mgr_finalize(GObject *object)
{
    GswVersificationMgr *self = GSW_VERSIFICATION_MGR(object);
    GswVersificationMgrPrivate *priv = gsw_versification_mgr_get_instance_private(self);

    g_clear_pointer(&priv->systems, g_hash_table_unref);
    g_clear_pointer(&priv->order, g_ptr_array_unref);

    G_OBJECT_CLASS(gsw_versification_mgr_parent_class)->finalize(object);
}

static void
gsw_versification_mgr_class_init(GswVersificationMgrClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = gsw_versification_mgr_finalize;
}

static void
gsw_versification_mgr_init(GswVersificationMgr *self)
{
    GswVersificationMgrPrivate *priv = gsw_versification_mgr_get_instance_private(self);
    const GswV11nSystemData *system;

    priv->systems = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                          (GDestroyNotify)gsw_versification_unref);
    priv->order = g_ptr_array_new();

    for (system = gsw_v11n_systems; system->name != NULL; system++) {
        GswVersification *v11n;

        if (g_hash_table_contains(priv->systems, system->name))
            continue;
        v11n = gsw_versification_build(system->name, system->ot, system->nt,
                                       system->verse_max);
        g_hash_table_insert(priv->systems, g_strdup(system->name), v11n);
        g_ptr_array_add(priv->order, v11n);
    }
}

GswVersificationMgr *
gsw_versification_mgr_get_default(void)
{
    static gsize once = 0;

    if (g_once_init_enter(&once)) {
        default_manager = g_object_new(GSW_TYPE_VERSIFICATION_MGR, NULL);
        g_once_init_leave(&once, 1);
    }
    return default_manager;
}

GPtrArray *
gsw_versification_mgr_get_systems(GswVersificationMgr *self)
{
    GswVersificationMgrPrivate *priv;
    GPtrArray *result;
    guint i;

    g_return_val_if_fail(GSW_IS_VERSIFICATION_MGR(self), NULL);
    priv = gsw_versification_mgr_get_instance_private(self);
    result = g_ptr_array_new_with_free_func((GDestroyNotify)gsw_versification_unref);
    for (i = 0; i < priv->order->len; i++)
        g_ptr_array_add(result, gsw_versification_ref(g_ptr_array_index(priv->order, i)));
    return result;
}

GswVersification *
gsw_versification_mgr_get_system(GswVersificationMgr *self, const gchar *name)
{
    GswVersificationMgrPrivate *priv;

    g_return_val_if_fail(GSW_IS_VERSIFICATION_MGR(self), NULL);
    g_return_val_if_fail(name != NULL, NULL);

    priv = gsw_versification_mgr_get_instance_private(self);
    return g_hash_table_lookup(priv->systems, name);
}

const gchar *
gsw_versification_get_name(GswVersification *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->name;
}

gint
gsw_versification_get_book_count(GswVersification *self)
{
    g_return_val_if_fail(self != NULL, 0);
    return (gint)self->books->len;
}

GswBook *
gsw_versification_get_book(GswVersification *self, gint number)
{
    g_return_val_if_fail(self != NULL, NULL);
    if (number < 0 || number >= (gint)self->books->len)
        return NULL;
    return g_ptr_array_index(self->books, number);
}

gint
gsw_versification_get_book_number_by_osis(GswVersification *self, const gchar *osis)
{
    gpointer value;

    g_return_val_if_fail(self != NULL, -1);
    g_return_val_if_fail(osis != NULL, -1);

    if (!g_hash_table_lookup_extended(self->osis_lookup, osis, NULL, &value))
        return -1;
    return GPOINTER_TO_INT(value);
}

gint
gsw_versification_get_nt_start_offset(GswVersification *self)
{
    g_return_val_if_fail(self != NULL, 0);
    return (gint)self->nt_start_offset;
}

void
gsw_versification_get_bmax(GswVersification *self, gint *ot_books, gint *nt_books)
{
    g_return_if_fail(self != NULL);
    if (ot_books != NULL)
        *ot_books = self->bmax[0];
    if (nt_books != NULL)
        *nt_books = self->bmax[1];
}

glong
gsw_versification_get_offset(GswVersification *self, gint book,
                             gint chapter, gint verse)
{
    GswBook *b;
    glong offset;

    g_return_val_if_fail(self != NULL, -1);

    b = gsw_versification_get_book(self, book);
    if (b == NULL)
        return -1;

    if (chapter > 0) {
        if (chapter > b->chapmax)
            return -1;
        if (verse > b->verse_max[chapter - 1])
            return -1;
        offset = b->offsets[chapter - 1];
    } else if (chapter == 0) {
        offset = b->offsets[0] - 1;
    } else {
        return -1;
    }

    return offset + verse;
}

gboolean
gsw_versification_get_verse(GswVersification *self, glong offset,
                            gint *book, gint *chapter, gint *verse)
{
    guint i;

    g_return_val_if_fail(self != NULL, FALSE);

    *book = -1;
    *chapter = 0;
    *verse = 0;

    if (offset < 1)
        return FALSE;

    for (i = 0; i < self->books->len; i++) {
        GswBook *b = g_ptr_array_index(self->books, i);
        glong book_heading = b->offsets[0] - 1;
        glong book_end = b->offsets[b->chapmax - 1] + b->verse_max[b->chapmax - 1];
        gint c;

        if (offset < book_heading)
            break;
        if (offset > book_end)
            continue;

        *book = (gint)i;

        if (offset == book_heading)
            return TRUE;

        for (c = 0; c < b->chapmax; c++) {
            if (offset < b->offsets[c])
                break;
            if (offset <= b->offsets[c] + b->verse_max[c]) {
                *chapter = c + 1;
                *verse = (gint)(offset - b->offsets[c]);
                return TRUE;
            }
        }
        return TRUE;
    }

    return FALSE;
}

const gchar *
gsw_book_get_name(GswBook *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->name;
}

const gchar *
gsw_book_get_name_for_locale(GswBook *self, const gchar *locale)
{
    g_return_val_if_fail(self != NULL, NULL);
    return gsw_locale_mgr_translate(gsw_locale_mgr_get_default(), self->name,
                                    locale);
}

const gchar *
gsw_book_get_osis_name(GswBook *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->osis;
}

const gchar *
gsw_book_get_preferred_abbreviation(GswBook *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->abbrev;
}

const gchar *
gsw_book_get_preferred_abbreviation_for_locale(GswBook *self,
                                               const gchar *locale)
{
    const gchar *translated;

    g_return_val_if_fail(self != NULL, NULL);
    if (self->abbrev == NULL || *self->abbrev == '\0')
        return self->abbrev;
    translated = gsw_locale_mgr_translate_preferred_abbreviation(
        gsw_locale_mgr_get_default(), self->abbrev, locale);
    return translated != NULL ? translated : self->abbrev;
}

gint
gsw_book_get_chapter_max(GswBook *self)
{
    g_return_val_if_fail(self != NULL, 0);
    return self->chapmax;
}

gint
gsw_book_get_verse_max(GswBook *self, gint chapter)
{
    g_return_val_if_fail(self != NULL, 0);
    if (chapter < 1 || chapter > self->chapmax)
        return 0;
    return self->verse_max[chapter - 1];
}
