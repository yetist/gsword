/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include <gsword/gsword.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswManager *manager;
    GswModule *module;
    GswVerseKey *key;
    gint chapter;
    gint verses = 0;

    if (argc != 4) {
        fprintf(stderr, "usage: %s <library-path> <module> <chapter-key>\n",
                argv[0]);
        return 2;
    }

    manager = gsw_manager_new_for_path(argv[1], &error);
    if (manager == NULL) {
        fprintf(stderr, "cannot open library: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    module = gsw_manager_get_module(manager, argv[2]);
    if (module == NULL) {
        fprintf(stderr, "module not found: %s\n", argv[2]);
        g_object_unref(manager);
        return 1;
    }

    if (!gsw_module_set_key_text(module, argv[3], &error))
        g_clear_error(&error);

    key = (GswVerseKey *)gsw_module_get_key(module);
    chapter = gsw_verse_key_get_chapter(key);

    while (gsw_verse_key_get_chapter(key) == chapter) {
        gchar *text = gsw_module_get_text(module, &error);

        if (text == NULL) {
            g_clear_error(&error);
            break;
        }
        printf("%d:%d %s\n", gsw_verse_key_get_chapter(key),
               gsw_verse_key_get_verse(key), text);
        g_free(text);
        verses++;

        if (!gsw_module_next(module, 1, NULL))
            break;
    }

    printf("\n%d verse(s)\n", verses);
    g_object_unref(manager);
    return 0;
}
