/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-render.c: option-aware HTML rendering and entry attributes
 */

#include "gsw-render.h"

#include <string.h>

gboolean
gsw_render_option_on(GHashTable *options, const gchar *name)
{
    const gchar *value = g_hash_table_lookup(options, name);

    return value != NULL &&
           (g_ascii_strcasecmp(value, "On") == 0 ||
            g_ascii_strcasecmp(value, "true") == 0 ||
            g_ascii_strcasecmp(value, "1") == 0);
}

gboolean
gsw_render_option_any(GHashTable *options, const gchar *const *names)
{
    guint i;

    for (i = 0; names[i] != NULL; i++) {
        if (gsw_render_option_on(options, names[i]))
            return TRUE;
    }
    return FALSE;
}

static gchar *
render_tag_attr(const gchar *attrs, const gchar *name)
{
    const gchar *p;
    gsize name_len = strlen(name);

    if (attrs == NULL)
        return NULL;

    p = attrs;
    while ((p = strstr(p, name)) != NULL) {
        const gchar *q = p + name_len;

        if (p > attrs && (g_ascii_isalnum(p[-1]) || p[-1] == '_')) {
            p = q;
            continue;
        }
        while (*q == ' ' || *q == '\t')
            q++;
        if (*q != '=') {
            p = q;
            continue;
        }
        q++;
        while (*q == ' ' || *q == '\t')
            q++;
        if (*q == '"' || *q == '\'') {
            gchar quote = *q++;
            const gchar *end = strchr(q, quote);

            if (end != NULL)
                return g_strndup(q, (gsize)(end - q));
        } else {
            const gchar *end = q;

            while (*end != '\0' && *end != ' ' && *end != '\t')
                end++;
            return g_strndup(q, (gsize)(end - q));
        }
        p = q;
    }
    return NULL;
}

static void
strip_append_entity(GString *out, const gchar *text, gsize len, gsize *i)
{
    const gchar *semi;

    if (g_str_has_prefix(text + *i, "&amp;")) {
        g_string_append_c(out, '&');
        *i += 5;
        return;
    }
    if (g_str_has_prefix(text + *i, "&lt;")) {
        g_string_append_c(out, '<');
        *i += 3;
        return;
    }
    if (g_str_has_prefix(text + *i, "&gt;")) {
        g_string_append_c(out, '>');
        *i += 3;
        return;
    }
    if (g_str_has_prefix(text + *i, "&quot;")) {
        g_string_append_c(out, '"');
        *i += 5;
        return;
    }
    if (g_str_has_prefix(text + *i, "&apos;")) {
        g_string_append_c(out, '\'');
        *i += 5;
        return;
    }
    semi = memchr(text + *i, ';', len - *i);
    if (semi != NULL && text[*i + 1] == '#') {
        gchar *number = g_strndup(text + *i + 2,
                                  (gsize)(semi - (text + *i + 2)));
        guint32 code = (guint32)g_ascii_strtoull(number, NULL, 10);
        g_free(number);
        if (code > 0) {
            gchar utf8[8] = { 0 };
            gint n = g_unichar_to_utf8((gunichar)code, utf8);
            g_string_append_len(out, utf8, n);
            *i = (gsize)(semi - text);
            return;
        }
    }

    g_string_append_c(out, '&');
    (*i)++;
}

gchar *
gsw_render_strip_markup(const gchar *markup)
{
    GString *out = g_string_new(NULL);
    gboolean last_space = TRUE;
    const gchar *p = markup != NULL ? markup : "";

    while (*p != '\0') {
        if (*p == '<') {
            const gchar *end = strchr(p + 1, '>');

            if (end == NULL)
                break;
            p = end + 1;
            continue;
        }

        if (g_str_has_prefix(p, "\\par")) {
            if (!last_space)
                g_string_append_c(out, ' ');
            last_space = TRUE;
            p += 4;
            continue;
        }

        if (*p == '&') {
            gsize i = 0;

            strip_append_entity(out, p, strlen(p), &i);
            p += i;
            last_space = FALSE;
            continue;
        }

        if (g_ascii_isspace(*p)) {
            if (!last_space)
                g_string_append_c(out, ' ');
            last_space = TRUE;
        } else {
            g_string_append_c(out, *p);
            last_space = FALSE;
        }
        p++;
    }

    g_strstrip(out->str);
    out->len = strlen(out->str);
    return g_string_free(out, FALSE);
}

static void
html_append_text(GString *out, const gchar *text, gsize len)
{
    gsize i;

    for (i = 0; i < len; i++) {
        if (text[i] == '\\' && i + 3 < len && !strncmp(text + i, "\\par", 4)) {
            g_string_append(out, "<br/>");
            i += 3;
        } else {
            g_string_append_c(out, text[i]);
        }
    }
}

static void
html_append_strongs(GString *out, const gchar *value)
{
    gchar **tokens;
    guint i;

    if (value == NULL)
        return;
    tokens = g_strsplit(value, " ", -1);
    for (i = 0; tokens[i] != NULL; i++) {
        if (g_str_has_prefix(tokens[i], "strong:"))
            g_string_append_printf(out, "<span class=\"gsw-strongs\">%s</span>",
                                   tokens[i] + 7);
    }
    g_strfreev(tokens);
}

static gboolean
tag_is_heading(const gchar *name)
{
    return g_strcmp0(name, "title") == 0 ||
           g_strcmp0(name, "H") == 0 ||
           g_strcmp0(name, "h1") == 0 ||
           g_strcmp0(name, "h2") == 0 ||
           g_strcmp0(name, "h3") == 0 ||
           g_strcmp0(name, "h4") == 0 ||
           g_strcmp0(name, "h5") == 0 ||
           g_strcmp0(name, "h6") == 0;
}

static gboolean
tag_is_note(const gchar *name)
{
    return g_strcmp0(name, "note") == 0 || g_strcmp0(name, "F") == 0;
}

static gboolean
tag_is_reference(const gchar *name)
{
    return g_strcmp0(name, "reference") == 0 || g_strcmp0(name, "xref") == 0;
}

static gboolean
tag_is_block(const gchar *name)
{
    return g_strcmp0(name, "p") == 0 || g_strcmp0(name, "div") == 0;
}

static gboolean
tag_is_inline(const gchar *name)
{
    return g_strcmp0(name, "span") == 0 ||
           g_strcmp0(name, "a") == 0 ||
           g_strcmp0(name, "i") == 0 ||
           g_strcmp0(name, "b") == 0 ||
           g_strcmp0(name, "em") == 0 ||
           g_strcmp0(name, "strong") == 0 ||
           g_strcmp0(name, "sup") == 0 ||
           g_strcmp0(name, "sub") == 0;
}

gchar *
gsw_render_markup_html(GHashTable *options, const gchar *markup)
{
    static const gchar *const strongs_names[] = {
        "Strong's Numbers", "Lemmas", NULL
    };
    static const gchar *const morph_names[] = {
        "Morphology", "Morphological Tags", NULL
    };
    static const gchar *const red_names[] = {
        "Red Letter Words", "Words of Christ in Red", NULL
    };
    GString *out = g_string_new(NULL);
    gboolean strongs = gsw_render_option_any(options, strongs_names);
    gboolean morph = gsw_render_option_any(options, morph_names);
    gboolean footnotes = gsw_render_option_on(options, "Footnotes");
    gboolean headings = gsw_render_option_on(options, "Headings");
    gboolean redletter = gsw_render_option_any(options, red_names);
    gboolean xrefs = gsw_render_option_on(options, "Cross-references");
    gboolean images = gsw_render_option_on(options, "Images");
    gchar *skip_tag = NULL;
    gint skip_depth = 0;
    gchar *word_lemma = NULL;
    gchar *word_morph = NULL;
    const gchar *p = markup != NULL ? markup : "";

    while (*p != '\0') {
        if (*p == '<') {
            const gchar *end = strchr(p + 1, '>');
            gchar *tag;
            gchar *name;
            gchar *attrs;
            gboolean closing = FALSE;
            gboolean self_closing = FALSE;

            if (end == NULL)
                break;
            tag = g_strndup(p + 1, (gsize)(end - p - 1));
            p = end + 1;

            {
                gchar *q = tag;

                while (*q == ' ' || *q == '\t')
                    q++;
                if (*q == '/') {
                    closing = TRUE;
                    q++;
                }
                {
                    gchar *name_start = q;

                    while (*q != '\0' && *q != ' ' && *q != '\t' && *q != '/')
                        q++;
                    name = g_strndup(name_start, (gsize)(q - name_start));
                    attrs = g_strdup(q);
                }
                {
                    gchar *trim = g_strdup(attrs);

                    g_strstrip(trim);
                    if (trim[0] != '\0' && trim[strlen(trim) - 1] == '/')
                        self_closing = TRUE;
                    g_free(trim);
                }
            }

            if (skip_depth > 0) {
                if (g_strcmp0(name, skip_tag) == 0) {
                    if (closing) {
                        skip_depth--;
                        if (skip_depth == 0) {
                            g_free(skip_tag);
                            skip_tag = NULL;
                        }
                    } else if (!self_closing) {
                        skip_depth++;
                    }
                }
                g_free(name);
                g_free(attrs);
                g_free(tag);
                continue;
            }

            if (!closing &&
                ((!footnotes && tag_is_note(name)) ||
                 (!headings && tag_is_heading(name)) ||
                 (!xrefs && tag_is_reference(name)))) {
                g_free(skip_tag);
                skip_tag = g_strdup(name);
                skip_depth = 1;
                g_free(name);
                g_free(attrs);
                g_free(tag);
                continue;
            }

            if (tag_is_note(name)) {
                if (footnotes)
                    g_string_append(out, closing
                                    ? "</span>"
                                    : "<span class=\"gsw-footnote\">");
            } else if (tag_is_heading(name)) {
                if (headings)
                    g_string_append(out, closing
                                    ? "</div>"
                                    : "<div class=\"gsw-heading\">");
            } else if (tag_is_reference(name)) {
                if (xrefs)
                    g_string_append(out, closing
                                    ? "</span>"
                                    : "<span class=\"gsw-crossref\">");
            } else if (g_strcmp0(name, "w") == 0 ||
                       g_strcmp0(name, "W") == 0) {
                if (closing) {
                    if (word_lemma != NULL && strongs)
                        html_append_strongs(out, word_lemma);
                    if (word_morph != NULL && morph)
                        g_string_append_printf(out,
                                               "<span class=\"gsw-morph\">%s</span>",
                                               word_morph);
                    g_clear_pointer(&word_lemma, g_free);
                    g_clear_pointer(&word_morph, g_free);
                    g_string_append(out, "</span>");
                } else if (!self_closing) {
                    word_lemma = render_tag_attr(attrs, "lemma");
                    word_morph = render_tag_attr(attrs, "morph");
                    g_string_append(out, "<span class=\"gsw-word\">");
                }
            } else if (g_strcmp0(name, "q") == 0 ||
                       g_strcmp0(name, "R") == 0) {
                if (redletter)
                    g_string_append(out, closing
                                    ? "</span>"
                                    : "<span class=\"gsw-words-of-christ\">");
            } else if (g_strcmp0(name, "sync") == 0 && !closing) {
                gchar *type = render_tag_attr(attrs, "type");
                gchar *value = render_tag_attr(attrs, "value");

                if (type != NULL && value != NULL) {
                    if (strongs && g_ascii_strcasecmp(type, "Strongs") == 0)
                        g_string_append_printf(out,
                                               "<span class=\"gsw-strongs\">%s</span>",
                                               value);
                    else if (morph && g_ascii_strcasecmp(type, "morph") == 0)
                        g_string_append_printf(out,
                                               "<span class=\"gsw-morph\">%s</span>",
                                               value);
                }
                g_free(type);
                g_free(value);
            } else if (g_strcmp0(name, "img") == 0 ||
                       g_strcmp0(name, "image") == 0) {
                if (images && !closing) {
                    gchar *src = render_tag_attr(attrs, "src");

                    if (src == NULL)
                        src = render_tag_attr(attrs, "href");
                    if (src != NULL) {
                        g_string_append_printf(out,
                                               "<img src=\"%s\" alt=\"\"/>",
                                               src);
                        g_free(src);
                    }
                }
            } else if (g_strcmp0(name, "br") == 0 ||
                       g_strcmp0(name, "lb") == 0) {
                g_string_append(out, "<br/>");
            } else if (tag_is_block(name) || tag_is_inline(name)) {
                g_string_append_c(out, '<');
                g_string_append(out, tag);
                g_string_append_c(out, '>');
            }

            g_free(name);
            g_free(attrs);
            g_free(tag);
        } else {
            const gchar *start = p;

            while (*p != '\0' && *p != '<')
                p++;
            html_append_text(out, start, (gsize)(p - start));
        }
    }

    g_free(skip_tag);
    g_free(word_lemma);
    g_free(word_morph);
    return g_string_free(out, FALSE);
}

static void
collect_attributes(GHashTable *attributes, const gchar *markup,
                   const gchar *pattern, const gchar *level1,
                   const gchar *level2)
{
    GRegex *regex;
    GMatchInfo *match = NULL;
    guint index = 0;

    regex = g_regex_new(pattern, G_REGEX_CASELESS | G_REGEX_DOTALL, 0, NULL);
    if (regex == NULL)
        return;

    if (g_regex_match(regex, markup, 0, &match)) {
        do {
            gchar *inner = g_match_info_fetch(match, 1);
            gchar *value = gsw_render_strip_markup(inner);
            gchar *key = g_strdup_printf("%s/%s/%u", level1, level2, index);

            g_hash_table_replace(attributes, key, value);
            g_free(inner);
            index++;
        } while (g_match_info_next(match, NULL));
    }
    g_match_info_free(match);
    g_regex_unref(regex);
}

static void
collect_tag_attributes(GHashTable *attributes, const gchar *markup,
                       const gchar *pattern, const gchar *level1,
                       const gchar *level2)
{
    GRegex *regex;
    GMatchInfo *match = NULL;
    guint index = 0;

    regex = g_regex_new(pattern, G_REGEX_CASELESS | G_REGEX_DOTALL, 0, NULL);
    if (regex == NULL)
        return;

    if (g_regex_match(regex, markup, 0, &match)) {
        do {
            gchar *value = g_match_info_fetch(match, 1);
            gchar *key;

            if (value == NULL)
                value = g_match_info_fetch(match, 2);
            if (value == NULL)
                value = g_match_info_fetch(match, 3);
            if (value == NULL || *value == '\0') {
                g_free(value);
                continue;
            }

            key = g_strdup_printf("%s/%s/%u", level1, level2, index++);
            g_hash_table_replace(attributes, key, value);
        } while (g_match_info_next(match, NULL));
    }
    g_match_info_free(match);
    g_regex_unref(regex);
}

static void
collect_images(GHashTable *attributes, const gchar *markup)
{
    GRegex *regex;
    GMatchInfo *match = NULL;
    guint index = 0;
    const gchar *pattern =
        "<(?:img|image)\\b[^>]*(?:src|href)\\s*=\\s*"
        "(?:\"([^\"]+)\"|'([^']+)'|([^\\s>]+))[^>]*>";

    regex = g_regex_new(pattern, G_REGEX_CASELESS | G_REGEX_DOTALL, 0, NULL);
    if (regex == NULL)
        return;

    if (g_regex_match(regex, markup, 0, &match)) {
        do {
            gchar *value = g_match_info_fetch(match, 1);
            gchar *key;

            if (value == NULL)
                value = g_match_info_fetch(match, 2);
            if (value == NULL)
                value = g_match_info_fetch(match, 3);
            if (value == NULL)
                continue;

            key = g_strdup_printf("Image/src/%u", index++);
            g_hash_table_replace(attributes, key, value);
        } while (g_match_info_next(match, NULL));
    }
    g_match_info_free(match);
    g_regex_unref(regex);
}

GHashTable *
gsw_render_collect_attributes(const gchar *markup)
{
    GHashTable *attributes;
    const gchar *text = markup != NULL ? markup : "";

    attributes = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    collect_attributes(attributes, text,
                       "<(?:title|h[1-6]|H)\\b[^>]*>(.*?)"
                       "</(?:title|h[1-6]|H)\\s*>",
                       "Heading", "Preverse");
    collect_attributes(attributes, text,
                       "<(?:note|F)\\b[^>]*>(.*?)</(?:note|F)\\s*>",
                       "Footnote", "body");
    collect_attributes(attributes, text,
                       "<(?:reference|xref)\\b[^>]*>(.*?)"
                       "</(?:reference|xref)\\s*>",
                       "Crossref", "body");
    collect_tag_attributes(
        attributes, text,
        "<w\\b[^>]*\\blemma\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s>]+))",
        "Word", "Lemma");
    collect_tag_attributes(
        attributes, text,
        "<w\\b[^>]*\\bmorph\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s>]+))",
        "Word", "Morph");
    collect_images(attributes, text);
    return attributes;
}
