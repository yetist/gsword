#include <gsword/gsword.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswManager *manager;
    GPtrArray *infos;
    gchar *default_path = NULL;
    guint i;

    if (argc > 1) {
        manager = gsw_manager_new_for_path(argv[1], &error);
    } else {
        const gchar *env = g_getenv("GSWORD_LIBRARY");

        if (env != NULL && *env != '\0') {
            default_path = g_strdup(env);
        } else {
            default_path = g_build_filename(g_get_user_data_dir(), "gsword",
                                            "library", NULL);
        }
        manager = gsw_manager_new_for_path(default_path, &error);
        g_free(default_path);
    }

    if (manager == NULL) {
        fprintf(stderr, "Cannot open module library: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    infos = gsw_manager_get_module_infos(manager);
    printf("%-24s %-16s %-10s %s\n", "Name", "Type", "Version", "Description");
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);
        printf("%-24s %-16s %-10s %s\n",
               gsw_module_info_get_name(info),
               gsw_module_info_get_driver_name(info),
               gsw_module_info_get_version(info),
               gsw_module_info_get_description(info));
    }

    printf("\n%u module(s)\n", infos->len);
    g_ptr_array_unref(infos);
    g_object_unref(manager);
    return 0;
}
