/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <gsword/gsword.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswManager *manager;
    GswModule *module;
    GswLocaleMgr *locales = gsw_locale_mgr_get_default();
    gchar *text;

    if (argc != 5) {
        fprintf(stderr,
                "usage: %s <library-path> <locale> <module> <key>\n", argv[0]);
        return 2;
    }

    gsw_locale_mgr_set_default_locale(locales, argv[2]);
    g_print("Locale: %s\n", gsw_locale_mgr_get_default_locale(locales));

    manager = gsw_manager_new_for_path(argv[1], &error);
    if (manager == NULL) {
        fprintf(stderr, "cannot open library: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    module = gsw_manager_get_module(manager, argv[3]);
    if (module == NULL) {
        fprintf(stderr, "module not found: %s\n", argv[3]);
        g_object_unref(manager);
        return 1;
    }

    if (!gsw_module_set_key_text(module, argv[4], &error)) {
        fprintf(stderr, "cannot set key: %s\n",
                error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
        g_object_unref(manager);
        return 1;
    }

    printf("Key: %s\n", gsw_module_get_key_text(module));
    {
        GswVerseKey *key = (GswVerseKey *)gsw_module_get_key(module);
        gchar *short_text = gsw_verse_key_get_short_text(key);

        printf("Book: %s\n", gsw_verse_key_get_book_name(key));
        printf("Short: %s\n", short_text != NULL ? short_text : "");
        g_free(short_text);
    }

    text = gsw_module_get_text(module, &error);
    if (text == NULL) {
        fprintf(stderr, "cannot read entry: %s\n", error->message);
        g_error_free(error);
        g_object_unref(manager);
        return 1;
    }
    printf("Text: %s\n", text);
    g_free(text);

    g_object_unref(manager);
    return 0;
}
