#include <gsword/gsword.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswInstallManager *installer;
    GPtrArray *sources;
    gboolean refresh;
    guint i;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s <installmgr-path> [refresh]\n", argv[0]);
        return 2;
    }

    refresh = argc == 3 && g_strcmp0(argv[2], "refresh") == 0;

    installer = gsw_install_manager_new(argv[1], NULL, &error);
    if (installer == NULL) {
        fprintf(stderr, "cannot open installer: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    gsw_install_manager_set_user_disclaimer_confirmed(installer, TRUE);
    sources = gsw_install_manager_get_sources(installer);
    if (sources->len == 0) {
        printf("No sources configured in %s/InstallMgr.conf\n", argv[1]);
        g_ptr_array_unref(sources);
        g_object_unref(installer);
        return 0;
    }

    for (i = 0; i < sources->len; i++) {
        GswInstallSource *source = g_ptr_array_index(sources, i);

        printf("%s: %s://%s%s (uid=%s)\n",
               gsw_install_source_get_caption(source),
               gsw_source_type_to_scheme(
                   gsw_install_source_get_source_type(source)),
               gsw_install_source_get_host(source),
               gsw_install_source_get_directory(source),
               gsw_install_source_get_uid(source));

        if (refresh) {
            GPtrArray *infos;
            guint j;

            printf("  refreshing...\n");
            if (!gsw_install_manager_refresh_remote_source(installer, source,
                                                           &error)) {
                fprintf(stderr, "  refresh failed: %s\n", error->message);
                g_clear_error(&error);
                continue;
            }

            infos = gsw_install_manager_get_remote_module_infos(
                installer, source, &error);
            if (infos == NULL) {
                fprintf(stderr, "  list failed: %s\n", error->message);
                g_clear_error(&error);
                continue;
            }
            printf("  %u module(s):\n", infos->len);
            for (j = 0; j < infos->len; j++) {
                GswModuleInfo *info = g_ptr_array_index(infos, j);
                printf("    %-24s %-10s %s\n",
                       gsw_module_info_get_name(info),
                       gsw_module_info_get_version(info),
                       gsw_module_info_get_description(info));
            }
            g_ptr_array_unref(infos);
        }
    }

    g_ptr_array_unref(sources);
    g_object_unref(installer);
    return 0;
}
