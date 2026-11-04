/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include <gsword/gsword.h>
#include "gsw-utils.h"
#include <glib/gstdio.h>

static void
write_text(const gchar *path, const gchar *text)
{
    gchar *parent = g_path_get_dirname(path);

    g_mkdir_with_parents(parent, 0755);
    g_assert_true(g_file_set_contents(path, text, -1, NULL));
    g_free(parent);
}

static void
make_source_library(const gchar *root)
{
    gchar *mods = g_build_filename(root, "mods.d", NULL);
    gchar *path;

    g_mkdir_with_parents(mods, 0755);

    path = g_build_filename(mods, "local.conf", NULL);
    g_assert_true(g_file_set_contents(path,
        "[LocalMod]\n"
        "Description=Local module\n"
        "DataPath=./modules/texts/rawtext/local/\n"
        "ModDrv=RawText\n"
        "Encoding=UTF-8\n"
        "File=modules/texts/rawtext/local/ot\n"
        "File=modules/texts/rawtext/local/nt\n"
        "Version=1.0\n"
        "Versification=KJV\n", -1, NULL));
    g_free(path);

    path = g_build_filename(mods, "locked.conf", NULL);
    g_assert_true(g_file_set_contents(path,
        "[LockedMod]\n"
        "Description=Locked local module\n"
        "DataPath=./modules/texts/rawtext/locked/\n"
        "ModDrv=RawText\n"
        "Encoding=UTF-8\n"
        "File=modules/texts/rawtext/locked/ot\n"
        "CipherKey=test-key\n"
        "Version=1.0\n"
        "Versification=KJV\n", -1, NULL));
    g_free(path);

    path = g_build_filename(root, "modules/texts/rawtext/local/ot", NULL);
    write_text(path, "local module");
    g_free(path);
    path = g_build_filename(root, "modules/texts/rawtext/local/nt", NULL);
    write_text(path, "");
    g_free(path);
    path = g_build_filename(root, "modules/texts/rawtext/locked/ot", NULL);
    write_text(path, "locked module");
    g_free(path);

    g_free(mods);
}

static void
test_local_install(void)
{
    GError *error = NULL;
    gchar *source_root = g_dir_make_tmp("gsword-local-src-XXXXXX", &error);
    gchar *dest_root = g_dir_make_tmp("gsword-local-dst-XXXXXX", &error);
    gchar *private_path = g_dir_make_tmp("gsword-local-inst-XXXXXX", &error);
    GswInstallManager *installer;
    GswManager *destination;
    GswInstallSource *source;
    GPtrArray *infos;
    GswModuleInfo *info;
    GswModule *module;
    gchar *config_path;
    gchar *data_path;
    gchar *options_path;
    GswManager *options_manager;

    g_assert_no_error(error);
    make_source_library(source_root);

    installer = gsw_install_manager_new(private_path, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(installer);
    destination = gsw_manager_new_for_path(dest_root, &error);
    g_assert_no_error(error);
    g_assert_nonnull(destination);

    /* Refreshing and listing a local DIR source must not require a disclaimer. */
    source = gsw_install_source_new(GSW_SOURCE_DIR, "Local", source_root,
                                    "", NULL, NULL, "local-dir");
    g_assert_nonnull(source);
    g_assert_cmpstr(gsw_source_type_to_string(GSW_SOURCE_DIR), ==, "DIR");
    g_assert_cmpstr(gsw_source_type_to_scheme(GSW_SOURCE_DIR), ==, "file");
    g_assert_true(gsw_install_manager_add_source(installer, source));
    g_assert_true(gsw_install_manager_refresh_remote_source(installer, source,
                                                            &error));
    g_assert_no_error(error);
    infos = gsw_install_manager_get_remote_module_infos(installer, source,
                                                        &error);
    g_assert_no_error(error);
    g_assert_nonnull(infos);
    g_assert_cmpuint(infos->len, ==, 2);
    g_ptr_array_unref(infos);

    /* Local directory install, including a locked module. */
    g_assert_true(gsw_install_manager_install_module_from_directory(
        installer, destination, source_root, "LocalMod", &error));
    g_assert_no_error(error);
    g_assert_true(gsw_install_manager_install_module_from_directory(
        installer, destination, source_root, "LockedMod", &error));
    g_assert_no_error(error);
    g_assert_true(gsw_manager_has_module(destination, "LocalMod"));
    g_assert_true(gsw_manager_has_module(destination, "LockedMod"));

    data_path = g_build_filename(dest_root, "modules", "texts", "rawtext",
                                 "local", "ot", NULL);
    config_path = g_build_filename(dest_root, "mods.d", "local.conf", NULL);
    g_assert_true(g_file_test(data_path, G_FILE_TEST_IS_REGULAR));
    g_assert_true(g_file_test(config_path, G_FILE_TEST_IS_REGULAR));
    g_free(data_path);
    g_free(config_path);

    info = gsw_manager_lookup_module_info(destination, "LockedMod");
    g_assert_nonnull(info);
    g_assert_true(gsw_module_info_is_ciphered(info));

    /* Runtime CipherKey update and persistence. */
    g_assert_true(gsw_manager_set_module_cipher_key(destination, "LocalMod",
                                                    "new-key", &error));
    g_assert_no_error(error);
    info = gsw_manager_lookup_module_info(destination, "LocalMod");
    g_assert_nonnull(info);
    g_assert_cmpstr(gsw_module_info_get_value(info, "CipherKey"), ==,
                    "new-key");

    /* Global render option persistence. */
    g_assert_true(gsw_manager_set_option(destination, "Footnotes", "On"));
    options_path = g_build_filename(dest_root, "render-options.conf", NULL);
    g_assert_true(gsw_manager_save_render_options(destination, options_path,
                                                  &error));
    g_assert_no_error(error);
    options_manager = gsw_manager_new_for_path(dest_root, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(gsw_manager_get_option(options_manager, "Footnotes"), ==,
                    "Off");
    g_assert_true(gsw_manager_load_render_options(options_manager, options_path,
                                                  &error));
    g_assert_no_error(error);
    g_assert_cmpstr(gsw_manager_get_option(options_manager, "Footnotes"), ==,
                    "On");
    module = gsw_manager_get_module(options_manager, "LocalMod");
    g_assert_nonnull(module);
    g_assert_cmpstr(gsw_module_get_option(module, "Footnotes"), ==, "On");
    g_object_unref(options_manager);
    g_free(options_path);

    g_object_unref(source);
    g_object_unref(destination);
    g_object_unref(installer);

    gsw_dir_remove_recursive(source_root, NULL);
    gsw_dir_remove_recursive(dest_root, NULL);
    gsw_dir_remove_recursive(private_path, NULL);
    g_free(source_root);
    g_free(dest_root);
    g_free(private_path);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/local-install/install", test_local_install);
    return g_test_run();
}
