#include <gsword/gsword.h>
#include <stdio.h>
#include <stdlib.h>

static void
walk(GswModule *module, GswTreeKey *key, gint depth, gint max_depth)
{
    if (depth > max_depth)
        return;

    do {
        GError *error = NULL;
        gchar *text;
        gchar *path = g_strdup(gsw_key_get_text((GswKey *)key));

        gsw_module_set_key_text(module, path, NULL);
        text = gsw_module_get_text(module, &error);
        if (text == NULL) {
            g_clear_error(&error);
            text = g_strdup("(unreadable)");
        }

        {
            gchar *short_text = g_strndup(text, 100);
            printf("%*s%s\n%*s  %s\n", depth * 2, "", path,
                   depth * 2, "", short_text);
            g_free(short_text);
        }
        g_free(text);
        g_free(path);

        if (gsw_tree_key_has_children(key)) {
            GswTreeKey *child = (GswTreeKey *)gsw_key_clone((GswKey *)key);
            if (gsw_tree_key_first_child(child))
                walk(module, child, depth + 1, max_depth);
            gsw_key_unref((GswKey *)child);
        }
    } while (gsw_tree_key_next_sibling(key));
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswManager *manager;
    GswModule *module;
    GswTreeKey *key;
    gint max_depth = 1;

    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: %s <library-path> <module> [depth]\n", argv[0]);
        return 2;
    }
    if (argc == 4)
        max_depth = atoi(argv[3]);

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

    key = (GswTreeKey *)gsw_module_create_key(module);
    if (gsw_tree_key_first_child(key))
        walk(module, key, 0, max_depth);
    else
        fprintf(stderr, "module has no tree entries\n");

    gsw_key_unref((GswKey *)key);
    g_object_unref(manager);
    return 0;
}
