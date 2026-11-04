/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include <gsword/gsword.h>
#include <stdio.h>
#include <string.h>

static gchar *
escape_text(const gchar *text)
{
    return g_markup_escape_text(text != NULL ? text : "", -1);
}

static void
append_verse(GString *body, GswModule *module)
{
    gchar *html;
    gchar *reference_html;
    gchar *entry_html;
    GError *error = NULL;

    html = gsw_module_get_render_html(module, &error);
    if (html == NULL) {
        fprintf(stderr, "cannot render entry: %s\n",
                error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
        return;
    }
    if (*html == '\0') {
        g_free(html);
        return;
    }

    reference_html = escape_text(gsw_module_get_key_text(module));
    entry_html = g_strdup(html);
    g_string_append_printf(body,
        "<article class=\"verse\">\n"
        "  <div class=\"reference\">%s</div>\n"
        "  <div class=\"verse-text\">%s</div>\n"
        "</article>\n",
        reference_html, entry_html);
    g_free(reference_html);
    g_free(entry_html);
    g_free(html);
}

int
main(int argc, char **argv)
{
    const gchar *mode;
    const gchar *reference;
    GError *error = NULL;
    GswManager *manager;
    GswModule *module;
    GswVerseKey *key;
    GString *body;
    gchar *start_key;
    gchar *title;
    gchar *title_html;
    gint book;
    gint chapter;
    guint count = 0;
    guint guard = 0;

    if (argc < 4 || argc > 5) {
        fprintf(stderr,
                "usage: %s <library-path> <module> <reference> "
                "[verse|chapter|book]\n",
                argv[0]);
        return 2;
    }
    mode = argc == 5 ? argv[4] : "verse";
    if (g_strcmp0(mode, "verse") != 0 && g_strcmp0(mode, "chapter") != 0 &&
        g_strcmp0(mode, "book") != 0) {
        fprintf(stderr, "invalid mode: %s\n", mode);
        return 2;
    }
    reference = argv[3];

    manager = gsw_manager_new_for_path(argv[1], &error);
    if (manager == NULL) {
        fprintf(stderr, "cannot open library: %s\n",
                error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
        return 1;
    }
    module = gsw_manager_get_module(manager, argv[2]);
    if (module == NULL) {
        fprintf(stderr, "module not found: %s\n", argv[2]);
        g_object_unref(manager);
        return 1;
    }
    if (!gsw_module_set_key_text(module, reference, &error)) {
        fprintf(stderr, "cannot set key: %s\n",
                error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
        g_object_unref(manager);
        return 1;
    }
    key = (GswVerseKey *)gsw_module_get_key(module);
    if (gsw_key_get_kind((GswKey *)key) != GSW_KEY_KIND_VERSE) {
        fprintf(stderr, "module does not use verse keys\n");
        g_object_unref(manager);
        return 1;
    }

    if (g_strcmp0(mode, "book") == 0) {
        gsw_verse_key_set_chapter(key, 1);
        gsw_verse_key_set_verse(key, 1);
    } else if (g_strcmp0(mode, "chapter") == 0) {
        gsw_verse_key_set_verse(key, 1);
    }
    book = gsw_verse_key_get_book(key);
    chapter = gsw_verse_key_get_chapter(key);
    start_key = g_strdup(gsw_module_get_key_text(module));

    body = g_string_new(NULL);
    for (;;) {
        GswVerseKey *current = (GswVerseKey *)gsw_module_get_key(module);

        if (g_strcmp0(mode, "chapter") == 0 &&
            gsw_verse_key_get_chapter(current) != chapter)
            break;
        if (g_strcmp0(mode, "book") == 0 &&
            gsw_verse_key_get_book(current) != book)
            break;
        if (gsw_module_has_entry(module)) {
            append_verse(body, module);
            count++;
        }
        if (g_strcmp0(mode, "verse") == 0)
            break;
        if (++guard >= 100000)
            break;
        if (!gsw_module_next(module, 1, &error)) {
            g_clear_error(&error);
            break;
        }
    }

    title = g_strdup_printf("%s — %s", gsw_module_get_name(module),
                            start_key);
    title_html = escape_text(title);
    printf("<!DOCTYPE html>\n"
           "<html lang=\"en\">\n"
           "<head>\n"
           "  <meta charset=\"utf-8\">\n"
           "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
           "  <title>%s</title>\n"
           "  <style>\n"
           "    :root { color-scheme: light dark; }\n"
           "    body { max-width: 52rem; margin: 0 auto; padding: 2rem; "
           "font-family: system-ui, sans-serif; line-height: 1.65; }\n"
           "    header { border-bottom: 2px solid currentColor; "
           "margin-bottom: 1.5rem; padding-bottom: .75rem; }\n"
           "    h1 { font-size: 1.35rem; margin: 0; }\n"
           "    .verse { border-bottom: 1px solid #8884; padding: .9rem 0; }\n"
           "    .reference { font-size: .85rem; opacity: .65; "
           "margin-bottom: .35rem; }\n"
           "    .verse-text { font-size: 1.08rem; }\n"
           "    .verse-text .strongs, .verse-text .footnote { opacity: .7; }\n"
           "  </style>\n"
           "</head>\n"
           "<body>\n"
           "  <header><h1>%s</h1></header>\n"
           "  <main>\n%s  </main>\n"
           "</body>\n"
           "</html>\n",
           title_html, title_html, body->str);

    fprintf(stderr, "rendered %u verse(s)\n", count);
    g_free(title_html);
    g_free(title);
    g_free(start_key);
    g_string_free(body, TRUE);
    g_object_unref(manager);
    return 0;
}
