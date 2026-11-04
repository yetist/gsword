/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include <gsword/gsword.h>
#include <stdio.h>

static void
on_progress(const gchar *module_name, guint processed, guint total,
            gpointer user_data)
{
    (void)user_data;
    if (total > 0)
        printf("\r%s: %u/%u", module_name, processed, total);
    else
        printf("\r%s: %u", module_name, processed);
    fflush(stdout);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswManager *manager;
    GswModule *module;

    if (argc != 4 ||
        (g_strcmp0(argv[3], "build") != 0 &&
         g_strcmp0(argv[3], "delete") != 0 &&
         g_strcmp0(argv[3], "status") != 0)) {
        fprintf(stderr, "usage: %s <library-path> <module> <build|delete|status>\n",
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

    if (g_strcmp0(argv[3], "build") == 0) {
        if (!gsw_module_build_search_index(module, NULL, on_progress, NULL,
                                           &error)) {
            fprintf(stderr, "\nindex build failed: %s\n", error->message);
            g_error_free(error);
            g_object_unref(manager);
            return 1;
        }
        printf("\nindex built\n");
    } else if (g_strcmp0(argv[3], "delete") == 0) {
        if (!gsw_module_delete_search_index(module, &error)) {
            fprintf(stderr, "index delete failed: %s\n", error->message);
            g_error_free(error);
            g_object_unref(manager);
            return 1;
        }
        printf("index deleted\n");
    } else {
        printf("index: %s\n",
               gsw_module_has_search_index(module) ? "available" : "missing");
    }

    g_object_unref(manager);
    return 0;
}
