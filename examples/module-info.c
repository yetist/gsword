/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include <gsword/gsword.h>
#include <stdio.h>

static void
print_values(GswModuleInfo *info, const gchar *key)
{
    gchar **values = gsw_module_info_get_values(info, key);
    guint i;

    if (values == NULL)
        return;
    for (i = 0; values[i] != NULL; i++)
        printf("  %s=%s\n", key, values[i]);
    g_strfreev(values);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    GswManager *manager;
    GswModuleInfo *info;
    gchar **keys;
    guint i;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <library-path> <module>\n", argv[0]);
        return 2;
    }

    manager = gsw_manager_new_for_path(argv[1], &error);
    if (manager == NULL) {
        fprintf(stderr, "cannot open library: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    info = gsw_manager_lookup_module_info(manager, argv[2]);
    if (info == NULL) {
        fprintf(stderr, "module not found: %s\n", argv[2]);
        g_object_unref(manager);
        return 1;
    }

    printf("Name:              %s\n", gsw_module_info_get_name(info));
    printf("Description:       %s\n", gsw_module_info_get_description(info));
    printf("Driver:            %s\n", gsw_module_info_get_driver_name(info));
    printf("Type:              %s\n", gsw_module_info_get_type_string(info));
    printf("Language:          %s\n", gsw_module_info_get_language(info));
    printf("Version:           %s\n", gsw_module_info_get_version(info));
    printf("Minimum version:   %s\n", gsw_module_info_get_minimum_version(info));
    printf("Encoding:          %s\n", gsw_module_info_get_encoding(info));
    printf("Source type:       %s\n", gsw_module_info_get_source_type(info));
    printf("Versification:     %s\n", gsw_module_info_get_versification(info));
    printf("Install size:      %" G_GUINT64_FORMAT " bytes\n",
           gsw_module_info_get_install_size(info));
    printf("Data path:         %s\n", gsw_module_info_get_data_path(info));
    printf("Absolute path:     %s\n", gsw_module_info_get_absolute_data_path(info));
    printf("Config file:       %s\n", gsw_module_info_get_config_file(info));
    printf("Ciphered:          %s\n", gsw_module_info_is_ciphered(info) ? "yes" : "no");
    printf("Supported:         %s\n", gsw_module_info_is_supported(info) ? "yes" : "no");
    printf("Copyright:         %s\n", gsw_module_info_get_copyright(info));
    printf("License:           %s\n", gsw_module_info_get_distribution_license(info));

    printf("\nConfiguration:\n");
    keys = gsw_module_info_get_keys(info);
    for (i = 0; keys != NULL && keys[i] != NULL; i++)
        print_values(info, keys[i]);
    g_strfreev(keys);

    g_object_unref(manager);
    return 0;
}
