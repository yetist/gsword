#include <gsword/gsword.h>
#include <glib/gstdio.h>

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswInstallManager *installer;
    GswInstallSource *source;
    GswManager *destination;
    GPtrArray *infos;
    GswModuleInfo *info;
    gchar *data_file;
    gchar *conf_file;

    if (argc != 5) {
        g_printerr("usage: %s <private> <dest> <host:port> <directory>\n", argv[0]);
        return 2;
    }

    installer = gsw_install_manager_new(argv[1], &error);
    if (installer == NULL) {
        g_printerr("install manager: %s\n", error->message);
        return 1;
    }
    gsw_install_manager_set_user_disclaimer_confirmed(installer, TRUE);

    source = gsw_install_source_new(GSW_SOURCE_HTTP, "Local", argv[3],
                                    argv[4], "", "", "local-test");
    if (source == NULL) {
        g_printerr("source creation failed\n");
        return 1;
    }
    gsw_install_manager_add_source(installer, source);

    /* Cancellation must not leave a refreshed source behind. */
    {
        GCancellable *cancellable = g_cancellable_new();
        g_cancellable_cancel(cancellable);
        gsw_install_manager_set_cancellable(installer, cancellable);
        if (gsw_install_manager_refresh_remote_source(installer, source, &error)) {
            g_printerr("cancelled refresh unexpectedly succeeded\n");
            return 1;
        }
        if (!g_error_matches(error, GSW_ERROR, GSW_ERROR_CANCELLED)) {
            g_printerr("expected cancellation, got: %s\n", error->message);
            return 1;
        }
        g_clear_error(&error);
        gsw_install_manager_set_cancellable(installer, NULL);
        g_object_unref(cancellable);
    }

    if (!gsw_install_manager_refresh_remote_source(installer, source, &error)) {
        g_printerr("refresh: %s\n", error->message);
        return 1;
    }

    infos = gsw_install_manager_get_remote_module_infos(installer, source, &error);
    if (infos == NULL) {
        g_printerr("remote infos: %s\n", error->message);
        return 1;
    }
    if (infos->len != 1) {
        g_printerr("expected 1 remote module, got %u\n", infos->len);
        return 1;
    }
    info = g_ptr_array_index(infos, 0);
    if (g_strcmp0(gsw_module_info_get_name(info), "Tiny") != 0) {
        g_printerr("unexpected module: %s\n", gsw_module_info_get_name(info));
        return 1;
    }
    g_ptr_array_unref(infos);

    destination = gsw_manager_new_for_path(argv[2], &error);
    if (destination == NULL) {
        g_printerr("destination: %s\n", error->message);
        return 1;
    }

    if (!gsw_install_manager_install_module(installer, destination, source,
                                            "Tiny", &error)) {
        g_printerr("install: %s\n", error->message);
        return 1;
    }

    if (!gsw_manager_has_module(destination, "Tiny")) {
        g_printerr("module not present after install\n");
        return 1;
    }

    data_file = g_build_filename(argv[2], "modules", "texts", "rawtext",
                                 "tiny", "ot", NULL);
    conf_file = g_build_filename(argv[2], "mods.d", "tiny.conf", NULL);
    if (!g_file_test(data_file, G_FILE_TEST_IS_REGULAR) ||
        !g_file_test(conf_file, G_FILE_TEST_IS_REGULAR)) {
        g_printerr("installed files are missing\n");
        return 1;
    }
    g_free(data_file);
    g_free(conf_file);

    if (!gsw_install_manager_remove_module(installer, destination, "Tiny", &error)) {
        g_printerr("remove: %s\n", error->message);
        return 1;
    }

    if (gsw_manager_has_module(destination, "Tiny")) {
        g_printerr("module still present after remove\n");
        return 1;
    }

    g_object_unref(destination);
    g_object_unref(source);
    g_object_unref(installer);
    return 0;
}
