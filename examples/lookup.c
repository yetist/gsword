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
    gchar *text;

    if (argc < 4 || argc > 5) {
        fprintf(stderr,
                "usage: %s <library-path> <module> <key> [locale]\n", argv[0]);
        return 2;
    }

    if (argc == 5)
        gsw_locale_mgr_set_default_locale(gsw_locale_mgr_get_default(),
                                          argv[4]);

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
        fprintf(stderr, "cannot set key: %s\n",
                error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
        g_object_unref(manager);
        return 1;
    }

    printf("%s\n", gsw_module_get_key_text(module));
    text = gsw_module_get_text(module, &error);
    if (text == NULL) {
        fprintf(stderr, "cannot read entry: %s\n", error->message);
        g_error_free(error);
        g_object_unref(manager);
        return 1;
    }

    printf("%s\n", text);
    g_free(text);
    g_object_unref(manager);
    return 0;
}
