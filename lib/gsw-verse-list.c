/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-verse-list.c: parsed verse lists and ranges
 */

#include <gsword/gsw-verse-list.h>
#include <gsword/gsw-error.h>

#include <string.h>

struct _GswVerseList {
    gatomicrefcount ref_count;
    GPtrArray *ranges;
};

static void
gsw_verse_list_free(GswVerseList *self)
{
    g_ptr_array_unref(self->ranges);
    g_free(self);
}

GType
gsw_verse_list_get_type(void)
{
    static gsize type = 0;

    if (g_once_init_enter(&type)) {
        GType registered = g_boxed_type_register_static(
            "GswVerseList", (GBoxedCopyFunc)gsw_verse_list_ref,
            (GBoxedFreeFunc)gsw_verse_list_unref);
        g_once_init_leave(&type, registered);
    }
    return (GType)type;
}

GswVerseList *
gsw_verse_list_ref(GswVerseList *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_atomic_ref_count_inc(&self->ref_count);
    return self;
}

void
gsw_verse_list_unref(GswVerseList *self)
{
    if (self == NULL)
        return;
    if (g_atomic_ref_count_dec(&self->ref_count))
        gsw_verse_list_free(self);
}

static gboolean
text_is_number(const gchar *text)
{
    const gchar *p = text;

    if (*p == '\0')
        return FALSE;
    while (*p != '\0') {
        if (!g_ascii_isdigit(*p))
            return FALSE;
        p++;
    }
    return TRUE;
}

static gboolean
text_is_chapter_verse(const gchar *text, gint *chapter, gint *verse)
{
    const gchar *colon = strchr(text, ':');
    gchar *end = NULL;
    gint64 c, v;

    if (colon == NULL || colon == text || colon[1] == '\0')
        return FALSE;
    c = g_ascii_strtoll(text, &end, 10);
    if (end != colon)
        return FALSE;
    v = g_ascii_strtoll(colon + 1, &end, 10);
    if (end == colon + 1 || *end != '\0')
        return FALSE;
    *chapter = (gint)c;
    *verse = (gint)v;
    return TRUE;
}

static GswVerseKey *
parse_relative(GswVerseKey *base, const gchar *text, GError **error)
{
    GswVerseKey *key = (GswVerseKey *)gsw_key_clone((GswKey *)base);

    if (key == NULL)
        return NULL;
    if (!gsw_key_set_text((GswKey *)key, text) ||
        !gsw_verse_key_is_valid(key)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Invalid verse reference: %s", text);
        gsw_key_unref(key);
        return NULL;
    }
    return key;
}

static gboolean
parse_upper(GswVerseKey *base, const gchar *text, GswVerseKey **out,
            GError **error)
{
    GswVerseKey *key = (GswVerseKey *)gsw_key_clone((GswKey *)base);
    gint chapter, verse;

    if (key == NULL)
        return FALSE;

    if (text_is_number(text)) {
        gint value = (gint)g_ascii_strtoll(text, NULL, 10);
        gsw_verse_key_set_verse(key, value);
    } else if (text_is_chapter_verse(text, &chapter, &verse)) {
        gsw_verse_key_set_chapter(key, chapter);
        gsw_verse_key_set_verse(key, verse);
    } else if (!gsw_key_set_text((GswKey *)key, text)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Invalid verse range endpoint: %s", text);
        gsw_key_unref(key);
        return FALSE;
    }

    if (!gsw_verse_key_is_valid(key)) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Invalid verse range endpoint: %s", text);
        gsw_key_unref(key);
        return FALSE;
    }
    *out = key;
    return TRUE;
}

GswVerseList *
gsw_verse_list_parse(const gchar *text, GswVerseKey *context, GError **error)
{
    GswVerseList *self;
    GswVerseKey *cursor;
    gchar **segments;
    guint i;

    g_return_val_if_fail(text != NULL, NULL);
    if (context != NULL &&
        gsw_key_get_kind((GswKey *)context) != GSW_KEY_KIND_VERSE) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Verse list context is not a verse key");
        return NULL;
    }

    if (context != NULL) {
        cursor = (GswVerseKey *)gsw_key_clone((GswKey *)context);
    } else {
        cursor = gsw_verse_key_new("Gen 1:1", "KJV");
    }
    if (cursor == NULL)
        return NULL;

    self = g_new0(GswVerseList, 1);
    g_atomic_ref_count_init(&self->ref_count);
    self->ranges = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsw_key_unref);

    segments = g_strsplit_set(text, ";,\n", -1);
    for (i = 0; segments[i] != NULL; i++) {
        gchar *segment = g_strdup(segments[i]);
        gchar *hyphen;
        gchar *lower_text;
        gchar *upper_text = NULL;
        GswVerseKey *lower;
        GswVerseKey *upper;
        GswVerseKey *next_cursor;

        g_strstrip(segment);
        if (*segment == '\0') {
            g_free(segment);
            continue;
        }

        hyphen = strrchr(segment, '-');
        if (hyphen != NULL && hyphen != segment && hyphen[1] != '\0') {
            *hyphen = '\0';
            lower_text = g_strdup(segment);
            upper_text = g_strdup(hyphen + 1);
            g_strstrip(lower_text);
            g_strstrip(upper_text);
        } else {
            lower_text = g_strdup(segment);
        }

        lower = parse_relative(cursor, lower_text, error);
        if (lower == NULL) {
            g_free(lower_text);
            g_free(upper_text);
            g_free(segment);
            goto fail;
        }

        if (upper_text != NULL) {
            if (*upper_text == '\0' ||
                !parse_upper(lower, upper_text, &upper, error)) {
                gsw_key_unref(lower);
                g_free(lower_text);
                g_free(upper_text);
                g_free(segment);
                goto fail;
            }
        } else {
            upper = (GswVerseKey *)gsw_key_clone((GswKey *)lower);
        }

        if (upper == NULL ||
            gsw_key_compare((GswKey *)lower, (GswKey *)upper) > 0) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Verse range upper bound precedes lower bound");
            gsw_key_unref(lower);
            gsw_key_unref(upper);
            g_free(lower_text);
            g_free(upper_text);
            g_free(segment);
            goto fail;
        }

        gsw_verse_key_set_lower_bound(lower, lower);
        gsw_verse_key_set_upper_bound(lower, upper);
        g_ptr_array_add(self->ranges, lower);

        next_cursor = (GswVerseKey *)gsw_key_clone((GswKey *)upper);
        gsw_key_unref(cursor);
        cursor = next_cursor;

        gsw_key_unref(upper);
        g_free(lower_text);
        g_free(upper_text);
        g_free(segment);
    }
    g_strfreev(segments);
    gsw_key_unref(cursor);
    return self;

fail:
    g_strfreev(segments);
    gsw_key_unref(cursor);
    gsw_verse_list_unref(self);
    return NULL;
}

guint
gsw_verse_list_get_length(GswVerseList *self)
{
    g_return_val_if_fail(self != NULL, 0);
    return self->ranges->len;
}

GswVerseKey *
gsw_verse_list_get_range(GswVerseList *self, guint index)
{
    g_return_val_if_fail(self != NULL, NULL);
    if (index >= self->ranges->len)
        return NULL;
    return g_ptr_array_index(self->ranges, index);
}

GPtrArray *
gsw_verse_list_expand(GswVerseList *self, GError **error)
{
    GPtrArray *result;
    guint i;
    guint guard = 0;

    g_return_val_if_fail(self != NULL, NULL);

    result = g_ptr_array_new_with_free_func((GDestroyNotify)gsw_key_unref);
    for (i = 0; i < self->ranges->len; i++) {
        GswVerseKey *range = g_ptr_array_index(self->ranges, i);
        GswVerseKey *lower = gsw_verse_key_get_lower_bound(range);
        GswVerseKey *upper = gsw_verse_key_get_upper_bound(range);
        GswVerseKey *current;

        if (lower == NULL)
            lower = (GswVerseKey *)gsw_key_clone((GswKey *)range);
        if (upper == NULL)
            upper = (GswVerseKey *)gsw_key_clone((GswKey *)range);
        current = (GswVerseKey *)gsw_key_clone((GswKey *)lower);

        while (current != NULL &&
               gsw_key_compare((GswKey *)current, (GswKey *)upper) <= 0) {
            if (guard++ >= 1000000) {
                g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                            "Verse list expansion exceeds one million entries");
                gsw_key_unref(lower);
                gsw_key_unref(upper);
                gsw_key_unref(current);
                g_ptr_array_unref(result);
                return NULL;
            }
            g_ptr_array_add(result, gsw_key_clone((GswKey *)current));
            if (gsw_key_compare((GswKey *)current, (GswKey *)upper) == 0)
                break;
            if (!gsw_key_next((GswKey *)current, 1)) {
                g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                            "Verse range leaves the versification");
                gsw_key_unref(lower);
                gsw_key_unref(upper);
                gsw_key_unref(current);
                g_ptr_array_unref(result);
                return NULL;
            }
        }

        gsw_key_unref(lower);
        gsw_key_unref(upper);
        gsw_key_unref(current);
    }
    return result;
}
