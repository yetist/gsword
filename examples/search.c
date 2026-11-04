/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <gsword/gsword.h>
#include <stdio.h>

static GswSearchType
parse_type(const gchar *name)
{
    if (g_ascii_strcasecmp(name, "multiword") == 0)
        return GSW_SEARCH_MULTIWORD;
    if (g_ascii_strcasecmp(name, "regex") == 0)
        return GSW_SEARCH_REGEX;
    return GSW_SEARCH_PHRASE;
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswManager *manager;
    GswModule *module;
    gchar **results;
    guint i;

    if (argc < 4 || argc > 5) {
        fprintf(stderr,
                "usage: %s <library-path> <module> <query> [phrase|multiword|regex]\n",
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

    results = gsw_module_search(module, argv[3],
                                parse_type(argc == 5 ? argv[4] : "phrase"),
                                &error);
    if (results == NULL) {
        fprintf(stderr, "search failed: %s\n", error->message);
        g_error_free(error);
        g_object_unref(manager);
        return 1;
    }

    for (i = 0; results[i] != NULL; i++)
        printf("%s\n", results[i]);
    printf("\n%u result(s)\n", i);

    g_strfreev(results);
    g_object_unref(manager);
    return 0;
}
