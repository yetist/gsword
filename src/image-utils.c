/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * image-utils.c: map image HTML and path helpers
 */

#include "image-utils.h"

#include <glib/gstdio.h>

#include <stdlib.h>
#include <string.h>

static gboolean
append_html_entity(GString *out, const gchar *entity)
{
    if (g_strcmp0(entity, "amp") == 0) {
        g_string_append_c(out, '&');
        return TRUE;
    }
    if (g_strcmp0(entity, "quot") == 0) {
        g_string_append_c(out, '"');
        return TRUE;
    }
    if (g_strcmp0(entity, "apos") == 0) {
        g_string_append_c(out, '\'');
        return TRUE;
    }
    if (g_strcmp0(entity, "lt") == 0) {
        g_string_append_c(out, '<');
        return TRUE;
    }
    if (g_strcmp0(entity, "gt") == 0) {
        g_string_append_c(out, '>');
        return TRUE;
    }
    if (g_strcmp0(entity, "nbsp") == 0) {
        g_string_append_c(out, ' ');
        return TRUE;
    }
    if (g_str_has_prefix(entity, "#")) {
        gchar *end = NULL;
        guint64 value;

        if (entity[1] == 'x' || entity[1] == 'X')
            value = g_ascii_strtoull(entity + 2, &end, 16);
        else
            value = g_ascii_strtoull(entity + 1, &end, 10);
        if (end != NULL && *end == '\0' && value <= G_MAXUINT32) {
            gchar utf8[7];
            gint length = g_unichar_to_utf8((gunichar)value, utf8);

            if (length > 0) {
                g_string_append_len(out, utf8, length);
                return TRUE;
            }
        }
    }
    return FALSE;
}

static gchar *
decode_html_entities(const gchar *text)
{
    GString *out = g_string_new(NULL);
    const gchar *cursor = text;

    while (*cursor != '\0') {
        if (*cursor == '&') {
            const gchar *semicolon = strchr(cursor, ';');

            if (semicolon != NULL && semicolon - cursor <= 12) {
                gchar *entity = g_strndup(cursor + 1,
                                          (gsize)(semicolon - cursor - 1));

                if (append_html_entity(out, entity)) {
                    g_free(entity);
                    cursor = semicolon + 1;
                    continue;
                }
                g_free(entity);
            }
        }
        g_string_append_c(out, *cursor);
        cursor++;
    }
    return g_string_free(out, FALSE);
}

static gchar *
unescape_image_source(const gchar *source)
{
    gchar *decoded = decode_html_entities(source);
    gchar *unescaped = g_uri_unescape_string(decoded, NULL);

    if (unescaped != NULL) {
        g_free(decoded);
        return unescaped;
    }
    return decoded;
}

static const gchar *
find_image_tag(const gchar *text)
{
    const gchar *cursor;

    for (cursor = text; *cursor != '\0'; cursor++) {
        if (g_ascii_strncasecmp(cursor, "<img", 4) == 0)
            return cursor;
    }
    return NULL;
}

gchar *
gsword_image_extract_src(const gchar *html)
{
    const gchar *cursor;

    g_return_val_if_fail(html != NULL, NULL);
    cursor = html;
    while ((cursor = find_image_tag(cursor)) != NULL) {
        const gchar *tag_end = strchr(cursor, '>');
        const gchar *attr = cursor + 4;

        if (tag_end == NULL)
            return NULL;
        while (attr < tag_end) {
            const gchar *name;
            const gchar *value;
            const gchar *value_end;
            gsize name_len;

            while (attr < tag_end &&
                   (g_ascii_isspace(*attr) || *attr == '/')) {
                attr++;
            }
            name = attr;
            while (attr < tag_end && !g_ascii_isspace(*attr) &&
                   *attr != '=' && *attr != '>' && *attr != '/') {
                attr++;
            }
            name_len = (gsize)(attr - name);
            if (name_len == 3 &&
                g_ascii_strncasecmp(name, "src", 3) == 0) {
                while (attr < tag_end && g_ascii_isspace(*attr))
                    attr++;
                if (attr < tag_end && *attr == '=') {
                    attr++;
                    while (attr < tag_end && g_ascii_isspace(*attr))
                        attr++;
                    if (attr < tag_end && (*attr == '"' || *attr == '\'')) {
                        gchar quote = *attr++;

                        value = attr;
                        value_end = strchr(attr, quote);
                        if (value_end == NULL || value_end > tag_end)
                            value_end = tag_end;
                    } else {
                        value = attr;
                        while (attr < tag_end && !g_ascii_isspace(*attr) &&
                               *attr != '>') {
                            attr++;
                        }
                        value_end = attr;
                    }
                    if (value_end > value) {
                        gchar *raw = g_strndup(value,
                                               (gsize)(value_end - value));
                        gchar *source = unescape_image_source(raw);

                        g_free(raw);
                        if (source != NULL && *source != '\0')
                            return source;
                        g_free(source);
                    }
                }
            }
            while (attr < tag_end && !g_ascii_isspace(*attr) &&
                   *attr != '>') {
                attr++;
            }
        }
        cursor = tag_end + 1;
    }
    return NULL;
}

gchar *
gsword_image_resolve_path(const gchar *data_dir, const gchar *source,
                          GError **error)
{
    gchar *relative;
    gchar *base;
    gchar *candidate;
    gchar *real_base = NULL;
    gchar *real_candidate = NULL;
    gsize base_len;
    gboolean inside;

    g_return_val_if_fail(data_dir != NULL, NULL);
    g_return_val_if_fail(source != NULL, NULL);

    relative = g_strdup(source);
    for (gchar *p = relative; *p != '\0'; p++) {
        if (*p == '\\')
            *p = '/';
    }
    while (g_str_has_prefix(relative, "/"))
        memmove(relative, relative + 1, strlen(relative));
    if (*relative == '\0') {
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                            "Empty image path");
        g_free(relative);
        return NULL;
    }

    base = g_canonicalize_filename(data_dir, NULL);
    candidate = g_canonicalize_filename(relative, base);
    base_len = strlen(base);
    inside = g_str_has_prefix(candidate, base) &&
             (candidate[base_len] == '\0' ||
              candidate[base_len] == G_DIR_SEPARATOR);
    if (!inside) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                    "Image path escapes module data directory: %s", source);
        g_free(relative);
        g_free(base);
        g_free(candidate);
        return NULL;
    }

    real_base = realpath(base, NULL);
    real_candidate = realpath(candidate, NULL);
    if (real_base != NULL && real_candidate != NULL) {
        base_len = strlen(real_base);
        inside = g_str_has_prefix(real_candidate, real_base) &&
                 (real_candidate[base_len] == '\0' ||
                  real_candidate[base_len] == G_DIR_SEPARATOR);
        if (!inside) {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                        "Image path resolves outside module data directory: %s",
                        source);
            g_free(relative);
            g_free(base);
            g_free(candidate);
            free(real_base);
            free(real_candidate);
            return NULL;
        }
    }
    free(real_base);
    free(real_candidate);
    g_free(relative);
    g_free(base);

    if (!g_file_test(candidate, G_FILE_TEST_IS_REGULAR)) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                    "Image file not found: %s", source);
        g_free(candidate);
        return NULL;
    }
    return candidate;
}

gdouble
gsword_image_fit_width(gint image_width, gint view_width,
                       gint scrollbar_width)
{
    gint available;

    if (image_width <= 0 || view_width <= 0)
        return 1.0;
    available = MAX(1, view_width - MAX(0, scrollbar_width));
    return (gdouble)available / (gdouble)image_width;
}

gdouble
gsword_image_fit_window(gint image_width, gint image_height,
                        gint view_width, gint view_height)
{
    gdouble horizontal;
    gdouble vertical;

    if (image_width <= 0 || image_height <= 0 ||
        view_width <= 0 || view_height <= 0) {
        return 1.0;
    }
    horizontal = (gdouble)view_width / (gdouble)image_width;
    vertical = (gdouble)view_height / (gdouble)image_height;
    return MIN(horizontal, vertical);
}
