#include <gsword/gsword.h>
#include <stdio.h>

static void
on_pre_status(GswStatusReporter *reporter, guint64 total, guint64 done,
              const gchar *message, gpointer user_data)
{
    (void)reporter;
    (void)user_data;
    printf("[%" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT "] %s\n",
           done, total, message);
}

static void
on_progress(GswStatusReporter *reporter, guint64 total, guint64 done,
            gpointer user_data)
{
    (void)reporter;
    (void)user_data;
    if (total > 0)
        printf("\r  %3" G_GUINT64_FORMAT "%% (%" G_GUINT64_FORMAT
               "/%" G_GUINT64_FORMAT ")",
               done * 100 / total, done, total);
    else
        printf("\r  %" G_GUINT64_FORMAT " bytes", done);
    fflush(stdout);
}

static void
print_sources(GswInstallManager *installer)
{
    GPtrArray *sources = gsw_install_manager_get_sources(installer);
    guint i;

    fprintf(stderr, "Available sources:\n");
    for (i = 0; i < sources->len; i++) {
        GswInstallSource *source = g_ptr_array_index(sources, i);
        fprintf(stderr, "  %-24s %s://%s%s\n",
                gsw_install_source_get_caption(source),
                gsw_source_type_to_scheme(
                    gsw_install_source_get_source_type(source)),
                gsw_install_source_get_host(source),
                gsw_install_source_get_directory(source));
    }
    g_ptr_array_unref(sources);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswManager *manager;
    GswInstallManager *installer;
    GswInstallSource *source;
    GswStatusReporter *reporter;
    GswModuleInfo *remote_info;
    guint status;

    if (argc != 5) {
        fprintf(stderr,
                "usage: %s <library-path> <installmgr-path> <source-caption> <module>\n",
                argv[0]);
        return 2;
    }

    manager = gsw_manager_new_for_path(argv[1], &error);
    if (manager == NULL) {
        fprintf(stderr, "cannot open library: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    installer = gsw_install_manager_new(argv[2], NULL, &error);
    if (installer == NULL) {
        fprintf(stderr, "cannot open installer: %s\n", error->message);
        g_error_free(error);
        g_object_unref(manager);
        return 1;
    }

    source = gsw_install_manager_lookup_source(installer, argv[3]);
    if (source == NULL) {
        fprintf(stderr, "source not found: %s\n", argv[3]);
        print_sources(installer);
        g_object_unref(installer);
        g_object_unref(manager);
        return 1;
    }

    reporter = gsw_status_reporter_new();
    g_signal_connect(reporter, "pre-status", G_CALLBACK(on_pre_status), NULL);
    g_signal_connect(reporter, "progress", G_CALLBACK(on_progress), NULL);
    gsw_install_manager_set_status_reporter(installer, reporter);

    gsw_install_manager_set_user_disclaimer_confirmed(installer, TRUE);

    printf("Refreshing %s...\n", gsw_install_source_get_caption(source));
    if (!gsw_install_manager_refresh_remote_source(installer, source, &error)) {
        fprintf(stderr, "\nrefresh failed: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    remote_info = gsw_install_manager_lookup_remote_module_info(
        installer, source, argv[4], &error);
    if (remote_info == NULL) {
        fprintf(stderr, "remote module not found: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    status = gsw_install_manager_get_module_status(manager, remote_info);
    printf("Status flags: 0x%x%s%s%s%s\n", status,
           (status & GSW_MODULE_STATUS_NEW) ? " NEW" : "",
           (status & GSW_MODULE_STATUS_UPDATED) ? " UPDATED" : "",
           (status & GSW_MODULE_STATUS_OLDER) ? " OLDER" : "",
           (status & GSW_MODULE_STATUS_SAME_VERSION) ? " SAME" : "");

    printf("Installing %s...\n", argv[4]);
    if (!gsw_install_manager_install_module(installer, manager, source,
                                            argv[4], &error)) {
        fprintf(stderr, "\ninstall failed: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    printf("\nInstalled %s\n", argv[4]);
    g_object_unref(reporter);
    g_object_unref(installer);
    g_object_unref(manager);
    return 0;
}
