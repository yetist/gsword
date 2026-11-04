/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <gsword/gsword.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswManager *manager;
    GswModule *module;
    const gchar *raw;
    gchar *text;
    gint i;

    if (argc != 4) {
        fprintf(stderr, "usage: %s <library-path> <module> <key>\n", argv[0]);
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

    if (!gsw_module_set_key_text(module, argv[3], &error)) {
        fprintf(stderr, "key warning: %s\n", error->message);
        g_clear_error(&error);
    }

    raw = gsw_module_peek_raw(module, &error);
    if (raw == NULL) {
        fprintf(stderr, "cannot read entry: %s\n", error->message);
        g_error_free(error);
        g_object_unref(manager);
        return 1;
    }

    printf("Key: %s\n", gsw_module_get_key_text(module));
    printf("Raw: %s\n", raw);

    text = gsw_module_get_text(module, &error);
    if (text != NULL) {
        printf("Text: %s\n", text);
        g_free(text);
    } else {
        fprintf(stderr, "text error: %s\n", error->message);
        g_clear_error(&error);
    }

    printf("\nNext entries:\n");
    for (i = 0; i < 5; i++) {
        if (!gsw_module_next(module, 1, NULL))
            break;
        text = gsw_module_get_text(module, &error);
        if (text == NULL) {
            g_clear_error(&error);
            break;
        }
        printf("  %s = %s\n", gsw_module_get_key_text(module), text);
        g_free(text);
    }

    g_object_unref(manager);
    return 0;
}
