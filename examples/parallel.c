/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * parallel.c: two independent reading positions in the same module
 */

#include <gsword/gsword.h>

static void
print_usage(const gchar *program)
{
    g_printerr("Usage: %s <prefix> <module> <key1> <key2>\n", program);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswManager *manager;
    GswModule *first;
    GswModule *second;
    gchar *text1;
    gchar *text2;

    if (argc != 5) {
        print_usage(argv[0]);
        return 1;
    }

    manager = gsw_manager_new_for_path(argv[1], &error);
    if (manager == NULL) {
        g_printerr("Cannot open module library: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    first = gsw_manager_create_module(manager, argv[2]);
    second = gsw_manager_create_module(manager, argv[2]);
    if (first == NULL || second == NULL) {
        g_printerr("Module not found: %s\n", argv[2]);
        g_clear_object(&first);
        g_clear_object(&second);
        g_object_unref(manager);
        return 1;
    }

    if (!gsw_module_set_key_text(first, argv[3], &error) ||
        !gsw_module_set_key_text(second, argv[4], &error)) {
        g_printerr("Cannot set key: %s\n", error->message);
        g_clear_error(&error);
        goto out;
    }

    text1 = gsw_module_get_text(first, &error);
    if (text1 == NULL) {
        g_printerr("Cannot read %s: %s\n", argv[3], error->message);
        g_clear_error(&error);
        goto out;
    }

    text2 = gsw_module_get_text(second, &error);
    if (text2 == NULL) {
        g_printerr("Cannot read %s: %s\n", argv[4], error->message);
        g_free(text1);
        g_clear_error(&error);
        goto out;
    }

    g_print("%s: %s\n", argv[3], text1);
    g_print("%s: %s\n", argv[4], text2);

    g_free(text1);
    g_free(text2);

out:
    g_object_unref(first);
    g_object_unref(second);
    g_object_unref(manager);
    return error == NULL ? 0 : 1;
}
