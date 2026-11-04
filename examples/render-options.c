/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <gsword/gsword.h>
#include <stdio.h>
#include <string.h>

static void
print_options(GswModule *module)
{
    GPtrArray *options = gsw_module_get_options(module);
    guint i;

    printf("Options:\n");
    for (i = 0; i < options->len; i++) {
        const gchar *name = g_ptr_array_index(options, i);
        printf("  %-22s %s\n", name, gsw_module_get_option(module, name));
    }
    g_ptr_array_unref(options);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswManager *manager;
    GswModule *module;
    gchar *text;
    gint i;

    if (argc < 4) {
        fprintf(stderr,
                "usage: %s <library-path> <module> <key> [Option=Value ...]\n",
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

    for (i = 4; i < argc; i++) {
        gchar *eq = strchr(argv[i], '=');
        if (eq == NULL)
            continue;
        *eq = '\0';
        printf("Set %s = %s\n", argv[i], eq + 1);
        if (!gsw_module_set_option(module, argv[i], eq + 1))
            fprintf(stderr, "unknown option: %s\n", argv[i]);
    }

    gsw_module_set_key_text(module, argv[3], &error);
    g_clear_error(&error);

    text = gsw_module_get_render_text(module, &error);
    if (text == NULL) {
        fprintf(stderr, "cannot render: %s\n", error->message);
        g_error_free(error);
        g_object_unref(manager);
        return 1;
    }

    printf("%s\n%s\n", gsw_module_get_key_text(module), text);
    g_free(text);
    print_options(module);
    g_object_unref(manager);
    return 0;
}
