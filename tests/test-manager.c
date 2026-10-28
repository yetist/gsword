#include <gsword/gsword.h>
#include "gsw-utils.h"
#include <glib/gstdio.h>

static gchar *
write_conf(const gchar *prefix, const gchar *name, const gchar *contents)
{
    gchar *dir = g_build_filename(prefix, "mods.d", NULL);
    gchar *path;

    g_mkdir_with_parents(dir, 0755);
    path = g_build_filename(dir, name, NULL);
    g_assert_true(g_file_set_contents(path, contents, -1, NULL));
    g_free(dir);
    return path;
}

static void
test_manager(void)
{
    GError *error = NULL;
    gchar *prefix = g_dir_make_tmp("gsword-manager-XXXXXX", &error);
    gchar *empty = g_dir_make_tmp("gsword-empty-XXXXXX", &error);
    gchar *kjv_conf;
    gchar *strongs_conf;
    GswManager *manager;
    GswManager *empty_manager;
    GPtrArray *infos;
    GswModuleInfo *info;
    gchar *expected;
    gchar *mods_d;

    g_assert_no_error(error);
    g_assert_nonnull(prefix);
    g_assert_nonnull(empty);

    kjv_conf = write_conf(prefix, "kjv.conf",
        "[KJV]\n"
        "Description=King James Version\n"
        "DataPath=./modules/texts/ztext/kjv/\n"
        "ModDrv=zText\n"
        "Encoding=UTF-8\n"
        "BlockType=BOOK\n"
        "CompressType=ZIP\n"
        "SourceType=OSIS\n"
        "Lang=en\n"
        "Version=2.9\n"
        "InstallSize=4310668\n"
        "Versification=KJV\n");

    strongs_conf = write_conf(prefix, "strongs.conf",
        "[StrongsGreek]\n"
        "Description=Strong's Greek\n"
        "DataPath=./modules/lexdict/rawld/strongs/strongs\n"
        "ModDrv=RawLD\n"
        "Lang=en\n"
        "Version=1.0\n");

    manager = gsw_manager_new_for_path(prefix, &error);
    g_assert_no_error(error);
    g_assert_nonnull(manager);
    g_assert_true(gsw_manager_has_module(manager, "KJV"));
    g_assert_true(gsw_manager_has_module(manager, "StrongsGreek"));

    infos = gsw_manager_get_module_infos(manager);
    g_assert_cmpuint(infos->len, ==, 2);
    g_ptr_array_unref(infos);

    info = gsw_manager_lookup_module_info(manager, "KJV");
    g_assert_nonnull(info);
    g_assert_cmpstr(gsw_module_info_get_description(info), ==,
                    "King James Version");
    g_assert_cmpint(gsw_module_info_get_driver(info), ==, GSW_DRIVER_Z_TEXT);
    g_assert_cmpint(gsw_module_info_get_module_type(info), ==,
                    GSW_MODULE_TYPE_BIBLES);
    g_assert_cmpstr(gsw_module_info_get_relative_data_dir(info), ==,
                    "modules/texts/ztext/kjv/");
    expected = g_build_filename(prefix, "modules/texts/ztext/kjv", NULL);
    g_assert_cmpstr(gsw_module_info_get_absolute_data_path(info), ==, expected);
    g_free(expected);
    g_assert_cmpuint(gsw_module_info_get_install_size(info), ==, 4310668);
    g_assert_true(gsw_module_info_is_supported(info));
    g_assert_false(gsw_module_info_is_ciphered(info));
    g_assert_cmpstr(gsw_module_info_get_config_file(info), ==, kjv_conf);

    info = gsw_manager_lookup_module_info(manager, "StrongsGreek");
    g_assert_nonnull(info);
    g_assert_cmpint(gsw_module_info_get_driver(info), ==, GSW_DRIVER_RAW_LD);
    g_assert_cmpint(gsw_module_info_get_module_type(info), ==,
                    GSW_MODULE_TYPE_LEXDICTS);
    g_assert_cmpstr(gsw_module_info_get_relative_data_dir(info), ==,
                    "modules/lexdict/rawld/strongs");
    expected = g_build_filename(prefix, "modules/lexdict/rawld/strongs", NULL);
    g_assert_cmpstr(gsw_module_info_get_absolute_data_path(info), ==, expected);
    g_free(expected);

    g_object_unref(manager);

    mods_d = g_build_filename(prefix, "mods.d", NULL);
    manager = gsw_manager_new_for_config(mods_d, &error);
    g_free(mods_d);
    g_assert_no_error(error);
    g_assert_nonnull(manager);
    infos = gsw_manager_get_module_infos(manager);
    g_assert_cmpuint(infos->len, ==, 2);
    g_ptr_array_unref(infos);
    g_object_unref(manager);

    empty_manager = gsw_manager_new_for_path(empty, &error);
    g_assert_no_error(error);
    g_assert_nonnull(empty_manager);
    infos = gsw_manager_get_module_infos(empty_manager);
    g_assert_cmpuint(infos->len, ==, 0);
    g_ptr_array_unref(infos);
    g_object_unref(empty_manager);

    gsw_dir_remove_recursive(prefix, NULL);
    gsw_dir_remove_recursive(empty, NULL);
    g_free(kjv_conf);
    g_free(strongs_conf);
    g_free(prefix);
    g_free(empty);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/manager/load", test_manager);
    return g_test_run();
}
