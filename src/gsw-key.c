/*
 * gsw-key.c: verse/string keys
 */

#include <gsword/gsw-key.h>
#include <gsword/gsw-verse-key.h>
#include <gsword/gsw-str-key.h>
#include <gsword/gsw-locale.h>
#include <gsword/gsw-versification.h>
#include <gsword/gsw-error.h>
#include "gsw-key-internal.h"
#include "gsw-key-private.h"
#include "gsw-versification-data.h"

#include <string.h>

GswKey *
gsw_key_alloc_internal(GswKeyKind kind)
{
    GswKey *self = g_new0(GswKey, 1);

    g_atomic_ref_count_init(&self->ref_count);
    self->kind = kind;
    self->valid = TRUE;
    self->testament = 1;
    self->book = 1;
    self->chapter = 1;
    self->verse = 1;
    self->intros = FALSE;
    self->autonorm = TRUE;
    return self;
}

static GswKey *
gsw_key_copy_func(GswKey *self)
{
    return gsw_key_clone(self);
}

GType
gsw_key_get_type(void)
{
    static gsize type = 0;

    if (g_once_init_enter(&type)) {
        GType registered = g_boxed_type_register_static(
            "GswKey", (GBoxedCopyFunc)gsw_key_copy_func,
            (GBoxedFreeFunc)gsw_key_unref);
        g_once_init_leave(&type, registered);
    }
    return (GType)type;
}

GType
gsw_str_key_get_type(void)
{
    static gsize type = 0;

    if (g_once_init_enter(&type)) {
        GType registered = g_boxed_type_register_static(
            "GswStrKey", (GBoxedCopyFunc)gsw_key_copy_func,
            (GBoxedFreeFunc)gsw_key_unref);
        g_once_init_leave(&type, registered);
    }
    return (GType)type;
}

GswStrKey *
gsw_str_key_new(const gchar *text)
{
    return (GswStrKey *)gsw_key_new_string(text);
}

GType
gsw_verse_key_get_type(void)
{
    static gsize type = 0;

    if (g_once_init_enter(&type)) {
        GType registered = g_boxed_type_register_static(
            "GswVerseKey", (GBoxedCopyFunc)gsw_key_copy_func,
            (GBoxedFreeFunc)gsw_key_unref);
        g_once_init_leave(&type, registered);
    }
    return (GType)type;
}

GswKeyKind
gsw_key_get_kind(GswKey *self)
{
    g_return_val_if_fail(self != NULL, GSW_KEY_KIND_STRING);
    return self->kind;
}

GswKey *
gsw_key_ref(GswKey *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_atomic_ref_count_inc(&self->ref_count);
    return self;
}

void
gsw_key_unref(GswKey *self)
{
    if (self == NULL)
        return;
    if (g_atomic_ref_count_dec(&self->ref_count)) {
        g_free(self->text);
        g_clear_pointer(&self->v11n, gsw_versification_unref);
        g_free(self->tree_path);
        g_free(self->tree_name);
        g_free(self->tree_user_data);
        g_free(self);
    }
}

GswKey *
gsw_key_clone(GswKey *self)
{
    GswKey *copy;

    g_return_val_if_fail(self != NULL, NULL);

    copy = gsw_key_alloc_internal(self->kind);
    copy->text = g_strdup(self->text);
    copy->valid = self->valid;
    copy->v11n = self->v11n != NULL ? gsw_versification_ref(self->v11n) : NULL;
    copy->testament = self->testament;
    copy->book = self->book;
    copy->chapter = self->chapter;
    copy->verse = self->verse;
    copy->suffix = self->suffix;
    copy->intros = self->intros;
    copy->autonorm = self->autonorm;
    copy->has_lower = self->has_lower;
    copy->has_upper = self->has_upper;
    copy->lower_testament = self->lower_testament;
    copy->lower_book = self->lower_book;
    copy->lower_chapter = self->lower_chapter;
    copy->lower_verse = self->lower_verse;
    copy->upper_testament = self->upper_testament;
    copy->upper_book = self->upper_book;
    copy->upper_chapter = self->upper_chapter;
    copy->upper_verse = self->upper_verse;
    copy->tree_path = g_strdup(self->tree_path);
    copy->tree_offset = self->tree_offset;
    copy->tree_parent = self->tree_parent;
    copy->tree_next = self->tree_next;
    copy->tree_first_child = self->tree_first_child;
    copy->tree_name = g_strdup(self->tree_name);
    copy->tree_user_data = self->tree_user_data != NULL
                         ? g_memdup2(self->tree_user_data,
                                     (gsize)self->tree_user_size) : NULL;
    copy->tree_user_size = self->tree_user_size;
    return copy;
}

static const gchar *
gsw_key_book_name(GswBook *book)
{
    const gchar *name = gsw_book_get_name(book);
    return gsw_locale_mgr_translate(gsw_locale_mgr_get_default(), name, NULL);
}

static gint
gsw_key_lookup_book(GswVersification *v11n, const gchar *token)
{
    gchar *upper;
    gchar *normalized;
    gint best = -1;
    gint best_len = 0;
    gint count;
    gint i;

    if (token == NULL || *token == '\0')
        return -1;

    {
        const gchar *reverse = gsw_locale_mgr_reverse(
            gsw_locale_mgr_get_default(), token, NULL);
        if (g_strcmp0(reverse, token) != 0)
            return gsw_key_lookup_book(v11n, reverse);
    }

    upper = g_ascii_strup(token, -1);
    normalized = g_strdup(upper);
    g_strstrip(normalized);

    /* Normalize common Roman prefixes to arabic. */
    if (g_str_has_prefix(normalized, "III ")) {
        gchar *tmp = g_strconcat("3 ", normalized + 4, NULL);
        g_free(normalized);
        normalized = tmp;
    } else if (g_str_has_prefix(normalized, "II ")) {
        gchar *tmp = g_strconcat("2 ", normalized + 3, NULL);
        g_free(normalized);
        normalized = tmp;
    } else if (g_str_has_prefix(normalized, "I ")) {
        gchar *tmp = g_strconcat("1 ", normalized + 2, NULL);
        g_free(normalized);
        normalized = tmp;
    }

    count = gsw_versification_get_book_count(v11n);
    for (i = 0; i < count; i++) {
        GswBook *book = gsw_versification_get_book(v11n, i);
        const gchar *osis = gsw_book_get_osis_name(book);
        const gchar *abbrev = gsw_book_get_preferred_abbreviation(book);
        const gchar *name = gsw_book_get_name(book);
        gchar *osis_upper = g_ascii_strup(osis, -1);
        gchar *abbrev_upper = g_ascii_strup(abbrev, -1);
        gchar *name_upper = g_ascii_strup(name, -1);
        gboolean match = FALSE;
        gint match_len = 0;

        if (g_strcmp0(normalized, osis_upper) == 0 ||
            g_strcmp0(normalized, abbrev_upper) == 0 ||
            g_strcmp0(normalized, name_upper) == 0) {
            match = TRUE;
            match_len = (gint)strlen(normalized);
        } else if (g_strcmp0(normalized, gsw_key_book_name(book)) == 0) {
            match = TRUE;
            match_len = (gint)strlen(normalized);
        } else if (g_str_has_prefix(abbrev_upper, normalized) ||
                   g_str_has_prefix(normalized, abbrev_upper)) {
            match = TRUE;
            match_len = (gint)strlen(abbrev_upper);
        }

        if (match && match_len > best_len) {
            best = i;
            best_len = match_len;
        }

        g_free(osis_upper);
        g_free(abbrev_upper);
        g_free(name_upper);
    }

    /* Built-in abbreviation table covers alternate spellings. */
    if (best < 0) {
        guint j;
        for (j = 0; j < gsw_builtin_abbrevs_count; j++) {
            if (g_ascii_strcasecmp(gsw_builtin_abbrevs[j].abbrev, normalized) == 0 ||
                g_str_has_prefix(gsw_builtin_abbrevs[j].abbrev, normalized)) {
                gint index = gsw_versification_get_book_number_by_osis(
                    v11n, gsw_builtin_abbrevs[j].osis);
                if (index >= 0) {
                    best = index;
                    break;
                }
            }
        }
    }

    g_free(upper);
    g_free(normalized);
    return best;
}

static void
gsw_verse_key_freshtext(GswKey *self)
{
    GString *text;

    if (self->testament == 0) {
        g_free(self->text);
        self->text = g_strdup("[ Module Heading ]");
        return;
    }
    if (self->testament == 2 && self->book == 0) {
        g_free(self->text);
        self->text = g_strdup("[ Testament 2 Heading ]");
        return;
    }
    if (self->book == 0) {
        g_free(self->text);
        self->text = g_strdup("[ Testament 1 Heading ]");
        return;
    }

    text = g_string_new(NULL);
    {
        gint bmax0 = 0;
        gint global;
        GswBook *book;

        gsw_versification_get_bmax(self->v11n, &bmax0, NULL);
        global = (self->testament == 2 ? bmax0 : 0) + self->book - 1;
        book = gsw_versification_get_book(self->v11n, global);
        if (book != NULL)
            g_string_append(text, gsw_key_book_name(book));
        else
            g_string_append_printf(text, "Book %d", self->book);
    }
    g_string_append_printf(text, " %d:%d", self->chapter, self->verse);
    if (self->suffix != 0)
        g_string_append_c(text, self->suffix);

    g_free(self->text);
    self->text = g_string_free(text, FALSE);
}

static gint
gsw_key_global_book(GswKey *self)
{
    gint bmax0 = 0;

    if (self->v11n == NULL || self->book < 1)
        return -1;
    gsw_versification_get_bmax(self->v11n, &bmax0, NULL);
    return (self->testament == 2 ? bmax0 : 0) + self->book - 1;
}

static gboolean
gsw_key_verse_validity(GswKey *self)
{
    GswBook *book;
    gint global;

    if (self->v11n == NULL)
        return FALSE;

    if (self->testament == 0)
        return TRUE;
    if (self->book == 0)
        return TRUE;

    global = gsw_key_global_book(self);
    book = gsw_versification_get_book(self->v11n, global);
    if (book == NULL)
        return FALSE;

    if (self->chapter < (self->intros ? 0 : 1) ||
        self->chapter > gsw_book_get_chapter_max(book))
        return FALSE;
    if (self->chapter == 0)
        return self->verse == 0;
    if (self->verse < (self->intros ? 0 : 1) ||
        self->verse > gsw_book_get_verse_max(book, self->chapter))
        return FALSE;

    return TRUE;
}

static void
gsw_key_normalize(GswKey *self)
{
    GswBook *book;
    gint global;
    gint bmax0 = 0;

    if (self->v11n == NULL)
        return;

    if (self->testament < 1) {
        self->testament = 1;
        self->book = 1;
        self->chapter = 1;
        self->verse = 1;
        self->valid = FALSE;
        return;
    }

    gsw_versification_get_bmax(self->v11n, &bmax0, NULL);
    if (self->testament > 2) {
        self->testament = 2;
        self->book = bmax0 > 0 ? bmax0 : 1;
        self->chapter = 1;
        self->verse = 1;
        self->valid = FALSE;
        return;
    }

    if (self->book == 0) {
        self->chapter = 0;
        self->verse = 0;
        return;
    }

    if (self->book < 1)
        self->book = 1;

    global = gsw_key_global_book(self);
    book = gsw_versification_get_book(self->v11n, global);
    if (book == NULL) {
        self->book = 1;
        global = gsw_key_global_book(self);
        book = gsw_versification_get_book(self->v11n, global);
    }
    if (book == NULL)
        return;

    {
        gint bmax0_local = 0, bmax1_local = 0;
        gint max;
        gsw_versification_get_bmax(self->v11n, &bmax0_local, &bmax1_local);
        max = self->testament == 2 ? bmax1_local : bmax0_local;
        if (self->book > max) {
            /* Clamp to the end of the testament. */
            self->book = max;
            global = gsw_key_global_book(self);
            book = gsw_versification_get_book(self->v11n, global);
            self->valid = FALSE;
        }
    }

    if (book == NULL)
        return;

    if (self->chapter < (self->intros ? 0 : 1)) {
        self->chapter = self->intros ? 0 : 1;
        self->valid = FALSE;
    }
    if (self->chapter > gsw_book_get_chapter_max(book)) {
        self->chapter = gsw_book_get_chapter_max(book);
        self->valid = FALSE;
    }

    if (self->chapter == 0) {
        self->verse = 0;
        return;
    }

    if (self->verse < (self->intros ? 0 : 1)) {
        self->verse = self->intros ? 0 : 1;
        self->valid = FALSE;
    }
    if (self->verse > gsw_book_get_verse_max(book, self->chapter)) {
        self->verse = gsw_book_get_verse_max(book, self->chapter);
        self->valid = FALSE;
    }
}

static gboolean
gsw_key_parse_verse(GswKey *self, const gchar *text)
{
    gchar *work;
    gint split = -1;
    gint book_index = -1;
    gint chapter = 1;
    gint verse = 1;
    gchar suffix = 0;
    gint bmax0 = 0;
    gint i;

    if (text == NULL)
        text = "";

    work = g_strdup(text);
    g_strstrip(work);

    if (g_strcmp0(work, "[ Module Heading ]") == 0) {
        self->testament = 0;
        self->book = 0;
        self->chapter = 0;
        self->verse = 0;
        self->valid = TRUE;
        g_free(work);
        return TRUE;
    }
    if (g_strcmp0(work, "[ Testament 1 Heading ]") == 0) {
        self->testament = 1;
        self->book = 0;
        self->chapter = 0;
        self->verse = 0;
        self->valid = TRUE;
        g_free(work);
        return TRUE;
    }
    if (g_strcmp0(work, "[ Testament 2 Heading ]") == 0) {
        self->testament = 2;
        self->book = 0;
        self->chapter = 0;
        self->verse = 0;
        self->valid = TRUE;
        g_free(work);
        return TRUE;
    }

    self->valid = TRUE;

    /* Find the longest leading token that names a book. */
    for (i = 0; work[i] != '\0'; i++) {
        if (g_ascii_isdigit(work[i])) {
            gchar *candidate = g_strndup(work, (gsize)i);
            gchar *trimmed = g_strdup(candidate);
            gsize len;

            g_strstrip(trimmed);
            len = strlen(trimmed);
            while (len > 0 && (trimmed[len - 1] == '.' || trimmed[len - 1] == ','))
                trimmed[--len] = '\0';

            book_index = gsw_key_lookup_book(self->v11n, trimmed);
            g_free(trimmed);
            g_free(candidate);

            if (book_index >= 0) {
                split = i;
                break;
            }
        }
    }

    if (split < 0) {
        book_index = gsw_key_lookup_book(self->v11n, work);
        if (book_index >= 0) {
            split = (gint)strlen(work);
        } else {
            split = 0;
        }
    }

    if (split >= 0) {
        const gchar *num = work + split;

        while (*num != '\0' && !g_ascii_isdigit(*num))
            num++;
        if (g_ascii_isdigit(*num)) {
            gchar *end = NULL;
            chapter = (gint)g_ascii_strtoll(num, &end, 10);
            if (end != NULL && (*end == ':' || *end == '.')) {
                num = end + 1;
                if (g_ascii_isdigit(*num)) {
                    verse = (gint)g_ascii_strtoll(num, &end, 10);
                    if (end != NULL && g_ascii_isalpha(*end))
                        suffix = *end;
                }
            }
        }
    }

    if (book_index < 0) {
        /* No book name: retain the current book. */
        book_index = gsw_key_global_book(self);
        if (book_index < 0)
            book_index = 0;
    }

    gsw_versification_get_bmax(self->v11n, &bmax0, NULL);
    if (book_index < bmax0) {
        self->testament = 1;
        self->book = book_index + 1;
    } else {
        self->testament = 2;
        self->book = book_index - bmax0 + 1;
    }
    self->chapter = chapter;
    self->verse = verse;
    self->suffix = suffix;

    gsw_key_normalize(self);
    if (self->autonorm)
        self->valid = gsw_key_verse_validity(self) && self->valid;

    gsw_verse_key_freshtext(self);
    g_free(work);
    return self->valid;
}

GswKey *
gsw_key_new_verse(const gchar *text, const gchar *versification)
{
    GswKey *self = gsw_key_alloc_internal(GSW_KEY_KIND_VERSE);
    GswVersification *v11n = gsw_versification_mgr_get_system(
        gsw_versification_mgr_get_default(),
        versification != NULL && *versification != '\0' ? versification : "KJV");

    if (v11n == NULL)
        v11n = gsw_versification_mgr_get_system(
            gsw_versification_mgr_get_default(), "KJV");

    self->v11n = gsw_versification_ref(v11n);
    gsw_key_parse_verse(self, text);
    return self;
}

void
gsw_key_set_text_cache(GswKey *self, const gchar *text)
{
    g_return_if_fail(self != NULL);
    g_free(self->text);
    self->text = g_strdup(text != NULL ? text : "");
}

GswKey *
gsw_key_new_string(const gchar *text)
{
    GswKey *self = gsw_key_alloc_internal(GSW_KEY_KIND_STRING);

    self->text = g_strdup(text != NULL ? text : "");
    self->valid = TRUE;
    return self;
}

const gchar *
gsw_key_get_text(GswKey *self)
{
    g_return_val_if_fail(self != NULL, NULL);

    if (self->kind == GSW_KEY_KIND_VERSE && self->text == NULL)
        gsw_verse_key_freshtext(self);
    return self->text != NULL ? self->text : "";
}

gboolean
gsw_key_set_text(GswKey *self, const gchar *text)
{
    g_return_val_if_fail(self != NULL, FALSE);

    if (self->kind == GSW_KEY_KIND_STRING) {
        g_free(self->text);
        self->text = g_strdup(text != NULL ? text : "");
        self->valid = TRUE;
        return TRUE;
    }

    if (self->kind == GSW_KEY_KIND_TREE)
        return gsw_tree_key_set_text_internal(self, text);

    return gsw_key_parse_verse(self, text);
}

glong
gsw_verse_key_get_index(GswVerseKey *self)
{
    gint global;

    g_return_val_if_fail(self != NULL, -1);

    if (self->testament == 0)
        return 0;
    if (self->book == 0)
        return self->testament == 2
             ? gsw_versification_get_nt_start_offset(self->v11n) + 1
             : 1;

    global = gsw_key_global_book(self);
    return gsw_versification_get_offset(self->v11n, global,
                                        self->chapter, self->verse);
}

glong
gsw_verse_key_get_testament_index(GswVerseKey *self)
{
    glong index = gsw_verse_key_get_index(self);

    if (index < 0)
        return index;
    if (self->testament == 2)
        index -= gsw_versification_get_nt_start_offset(self->v11n);
    return index;
}

gint
gsw_verse_key_get_global_book(GswVerseKey *self)
{
    return gsw_key_global_book(self);
}

gint
gsw_verse_key_get_global_book_from_name(GswVerseKey *self, const gchar *name)
{
    return gsw_key_lookup_book(self->v11n, name);
}

void
gsw_verse_key_set_index(GswVerseKey *self, glong index)
{
    gint book_global = -1;
    gint chapter = 0;
    gint verse = 0;
    gint bmax0 = 0;

    g_return_if_fail(self != NULL);

    if (index == 0) {
        self->testament = 0;
        self->book = 0;
        self->chapter = 0;
        self->verse = 0;
        self->valid = TRUE;
        gsw_verse_key_freshtext(self);
        return;
    }

    gsw_versification_get_bmax(self->v11n, &bmax0, NULL);
    if (index == 1) {
        self->testament = 1;
        self->book = 0;
        self->chapter = 0;
        self->verse = 0;
        self->valid = TRUE;
        gsw_verse_key_freshtext(self);
        return;
    }
    if (index == gsw_versification_get_nt_start_offset(self->v11n) + 1) {
        self->testament = 2;
        self->book = 0;
        self->chapter = 0;
        self->verse = 0;
        self->valid = TRUE;
        gsw_verse_key_freshtext(self);
        return;
    }

    if (!gsw_versification_get_verse(self->v11n, index, &book_global,
                                     &chapter, &verse)) {
        self->valid = FALSE;
        return;
    }

    self->testament = book_global < bmax0 ? 1 : 2;
    self->book = book_global - (self->testament == 2 ? bmax0 : 0) + 1;
    self->chapter = chapter;
    self->verse = verse;
    self->valid = TRUE;
    gsw_verse_key_freshtext(self);
}

gboolean
gsw_verse_key_set_last(GswKey *key, glong ot_index, glong nt_index,
                       GError **error)
{
    glong index;

    g_return_val_if_fail(key != NULL, FALSE);
    if (key->kind != GSW_KEY_KIND_VERSE) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                    "Bottom position requires a verse key");
        return FALSE;
    }

    if (nt_index >= 0) {
        index = gsw_versification_get_nt_start_offset(key->v11n) + nt_index;
    } else if (ot_index >= 0) {
        index = ot_index;
    } else {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Module contains no verse entries");
        return FALSE;
    }

    gsw_verse_key_set_index((GswVerseKey *)key, index);
    if (!key->valid) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Last module entry is outside the versification");
        return FALSE;
    }
    return TRUE;
}

gboolean
gsw_key_next(GswKey *self, gint steps)
{
    glong index;

    g_return_val_if_fail(self != NULL, FALSE);
    if (self->kind == GSW_KEY_KIND_TREE)
        return gsw_tree_key_increment(self, steps);
    if (self->kind != GSW_KEY_KIND_VERSE)
        return FALSE;

    index = gsw_verse_key_get_index(self);
    if (index < 0)
        return FALSE;

    index += steps;
    if (index < 0) {
        self->valid = FALSE;
        return FALSE;
    }

    if (!self->intros) {
        /* Skip module/testament/book/chapter heading entries. */
        while (index >= 0) {
            gsw_verse_key_set_index(self, index);
            if (!self->valid)
                return FALSE;
            if (self->testament == 0 || self->book == 0 ||
                self->chapter == 0 || self->verse == 0) {
                index += steps > 0 ? 1 : -1;
                continue;
            }
            break;
        }
        if (index < 0) {
            self->valid = FALSE;
            return FALSE;
        }
        return TRUE;
    }

    gsw_verse_key_set_index(self, index);
    return self->valid;
}

gboolean
gsw_key_prev(GswKey *self, gint steps)
{
    return gsw_key_next(self, -steps);
}

gint
gsw_key_compare(GswKey *self, GswKey *other)
{
    g_return_val_if_fail(self != NULL, 0);
    g_return_val_if_fail(other != NULL, 0);

    if (self->kind == GSW_KEY_KIND_STRING ||
        other->kind == GSW_KEY_KIND_STRING)
        return g_strcmp0(self->text, other->text);

    if (self->kind == GSW_KEY_KIND_TREE || other->kind == GSW_KEY_KIND_TREE)
        return (self->tree_offset > other->tree_offset) -
               (self->tree_offset < other->tree_offset);

    {
        glong a = gsw_verse_key_get_index(self);
        glong b = gsw_verse_key_get_index(other);
        return (a > b) - (a < b);
    }
}

gboolean
gsw_key_equals(GswKey *self, GswKey *other)
{
    return gsw_key_compare(self, other) == 0;
}

GswVerseKey *
gsw_verse_key_new(const gchar *text, const gchar *versification)
{
    return (GswVerseKey *)gsw_key_new_verse(text, versification);
}

GswVerseKey *
gsw_verse_key_new_with_bounds(const gchar *lower, const gchar *upper,
                              const gchar *versification)
{
    GswVerseKey *self = gsw_verse_key_new(lower, versification);
    GswVerseKey *upper_key = gsw_verse_key_new(upper, versification);

    gsw_verse_key_set_lower_bound(self, self);
    gsw_verse_key_set_upper_bound(self, upper_key);
    gsw_key_unref(upper_key);
    gsw_key_set_text(self, lower);
    return self;
}

gint
gsw_verse_key_get_testament(GswVerseKey *self)
{
    return self->testament;
}

gint
gsw_verse_key_get_book(GswVerseKey *self)
{
    return self->book;
}

gint
gsw_verse_key_get_chapter(GswVerseKey *self)
{
    return self->chapter;
}

gint
gsw_verse_key_get_verse(GswVerseKey *self)
{
    return self->verse;
}

gboolean
gsw_verse_key_set_testament(GswVerseKey *self, gint testament)
{
    g_return_val_if_fail(self != NULL, FALSE);
    self->testament = testament;
    self->valid = TRUE;
    gsw_key_normalize(self);
    gsw_verse_key_freshtext(self);
    return self->valid;
}

gboolean
gsw_verse_key_set_book(GswVerseKey *self, gint book)
{
    g_return_val_if_fail(self != NULL, FALSE);
    self->book = book;
    self->valid = TRUE;
    gsw_key_normalize(self);
    gsw_verse_key_freshtext(self);
    return self->valid;
}

gboolean
gsw_verse_key_set_chapter(GswVerseKey *self, gint chapter)
{
    g_return_val_if_fail(self != NULL, FALSE);
    self->chapter = chapter;
    self->valid = TRUE;
    gsw_key_normalize(self);
    gsw_verse_key_freshtext(self);
    return self->valid;
}

gboolean
gsw_verse_key_set_verse(GswVerseKey *self, gint verse)
{
    g_return_val_if_fail(self != NULL, FALSE);
    self->verse = verse;
    self->valid = TRUE;
    gsw_key_normalize(self);
    gsw_verse_key_freshtext(self);
    return self->valid;
}

const gchar *
gsw_verse_key_get_book_name(GswVerseKey *self)
{
    GswBook *book;
    gint global;

    g_return_val_if_fail(self != NULL, NULL);
    if (self->testament == 0)
        return "[ Module Heading ]";
    if (self->book == 0)
        return self->testament == 2 ? "[ Testament 2 Heading ]"
                                    : "[ Testament 1 Heading ]";

    global = gsw_key_global_book(self);
    book = gsw_versification_get_book(self->v11n, global);
    return book != NULL ? gsw_key_book_name(book) : "";
}

gboolean
gsw_verse_key_set_book_name(GswVerseKey *self, const gchar *name)
{
    gint index;

    g_return_val_if_fail(self != NULL, FALSE);
    index = gsw_key_lookup_book(self->v11n, name);
    if (index < 0)
        return FALSE;

    {
        gint bmax0 = 0;
        gsw_versification_get_bmax(self->v11n, &bmax0, NULL);
        if (index < bmax0) {
            self->testament = 1;
            self->book = index + 1;
        } else {
            self->testament = 2;
            self->book = index - bmax0 + 1;
        }
    }
    self->chapter = 1;
    self->verse = 1;
    self->valid = TRUE;
    gsw_key_normalize(self);
    gsw_verse_key_freshtext(self);
    return self->valid;
}

const gchar *
gsw_verse_key_get_versification(GswVerseKey *self)
{
    return self->v11n != NULL ? gsw_versification_get_name(self->v11n) : NULL;
}

gint
gsw_verse_key_get_chapter_max(GswVerseKey *self)
{
    GswBook *book;
    gint global;

    g_return_val_if_fail(self != NULL, 0);
    global = gsw_key_global_book(self);
    book = gsw_versification_get_book(self->v11n, global);
    return book != NULL ? gsw_book_get_chapter_max(book) : 0;
}

gint
gsw_verse_key_get_verse_max(GswVerseKey *self)
{
    GswBook *book;
    gint global;

    g_return_val_if_fail(self != NULL, 0);
    global = gsw_key_global_book(self);
    book = gsw_versification_get_book(self->v11n, global);
    return book != NULL ? gsw_book_get_verse_max(book, self->chapter) : 0;
}

void
gsw_verse_key_set_auto_normalize(GswVerseKey *self, gboolean enabled)
{
    g_return_if_fail(self != NULL);
    self->autonorm = enabled;
}

gboolean
gsw_verse_key_get_auto_normalize(GswVerseKey *self)
{
    return self->autonorm;
}

void
gsw_verse_key_set_intros(GswVerseKey *self, gboolean enabled)
{
    g_return_if_fail(self != NULL);
    self->intros = enabled;
}

gboolean
gsw_verse_key_get_intros(GswVerseKey *self)
{
    return self->intros;
}

gboolean
gsw_verse_key_is_valid(GswVerseKey *self)
{
    return self->valid;
}

void
gsw_verse_key_set_lower_bound(GswVerseKey *self, GswVerseKey *bound)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(bound != NULL);
    self->has_lower = TRUE;
    self->lower_testament = bound->testament;
    self->lower_book = bound->book;
    self->lower_chapter = bound->chapter;
    self->lower_verse = bound->verse;
}

void
gsw_verse_key_set_upper_bound(GswVerseKey *self, GswVerseKey *bound)
{
    g_return_if_fail(self != NULL);
    g_return_if_fail(bound != NULL);
    self->has_upper = TRUE;
    self->upper_testament = bound->testament;
    self->upper_book = bound->book;
    self->upper_chapter = bound->chapter;
    self->upper_verse = bound->verse;
}

void
gsw_verse_key_clear_bounds(GswVerseKey *self)
{
    g_return_if_fail(self != NULL);
    self->has_lower = FALSE;
    self->has_upper = FALSE;
}
