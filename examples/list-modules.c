#include <gsword/gsword.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswManager *manager;
    GPtrArray *infos;
    guint i;

    if (argc > 1)
        manager = gsw_manager_new_for_path(argv[1], &error);
    else
        manager = gsw_manager_new(&error);

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
