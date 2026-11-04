/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include <gsword/gsword.h>
#include <glib/gstdio.h>

static void
test_parse_and_save(void)
{
    const gchar *data =
        "\xef\xbb\xbf# comment\n"
        "[KJV]\n"
        "Description=King James Version\n"
        "  # indented comment\n"
        "GlobalOptionFilter=OSISStrongs\n"
        "GlobalOptionFilter=OSISFootnotes\n"
        "Empty=\n"
        "ValueWithEquals=a=b=c\n";
    GError *error = NULL;
    GswConfig *config = gsw_config_new_from_data(data, -1, &error);
    gchar **values;
    gchar *dir;
    gchar *path;
    gchar *mods_dir;
    GswConfig *reloaded;
    GswConfig *copy;

    g_assert_no_error(error);
    g_assert_nonnull(config);

    g_assert_true(gsw_config_has_section(config, "KJV"));
    g_assert_cmpstr(gsw_config_get(config, "KJV", "Description"), ==,
                    "King James Version");
    g_assert_cmpstr(gsw_config_get(config, "KJV", "Empty"), ==, "");
    g_assert_cmpstr(gsw_config_get(config, "KJV", "ValueWithEquals"), ==,
                    "a=b=c");

    values = gsw_config_get_all(config, "KJV", "GlobalOptionFilter");
    g_assert_nonnull(values);
    g_assert_cmpstr(values[0], ==, "OSISStrongs");
    g_assert_cmpstr(values[1], ==, "OSISFootnotes");
    g_assert_null(values[2]);
    g_strfreev(values);

    copy = gsw_config_copy(config);
    g_assert_cmpstr(gsw_config_get(copy, "KJV", "Description"), ==,
                    "King James Version");

    dir = g_dir_make_tmp("gsword-config-XXXXXX", &error);
    g_assert_no_error(error);
    path = g_build_filename(dir, "mods.d", "test.conf", NULL);
    g_assert_true(gsw_config_save(copy, path, &error));
    g_assert_no_error(error);

    reloaded = gsw_config_new_from_file(path, &error);
    g_assert_no_error(error);
    g_assert_nonnull(reloaded);
    g_assert_cmpstr(gsw_config_get(reloaded, "KJV", "Description"), ==,
                    "King James Version");
    values = gsw_config_get_all(reloaded, "KJV", "GlobalOptionFilter");
    g_assert_nonnull(values);
    g_assert_cmpstr(values[0], ==, "OSISStrongs");
    g_assert_cmpstr(values[1], ==, "OSISFootnotes");
    g_strfreev(values);

    g_object_unref(reloaded);
    g_object_unref(copy);
    g_object_unref(config);
    mods_dir = g_path_get_dirname(path);
    g_remove(path);
    g_rmdir(mods_dir);
    g_rmdir(dir);
    g_free(mods_dir);
    g_free(path);
    g_free(dir);
}

static void
test_augment(void)
{
    GswConfig *a = gsw_config_new();
    GswConfig *b = gsw_config_new();

    gsw_config_set(a, "S", "One", "1");
    gsw_config_add(a, "S", "Multi", "a1");
    gsw_config_add(a, "S", "Multi", "a2");
    gsw_config_set(b, "S", "One", "2");
    gsw_config_add(b, "S", "Multi", "b1");
    gsw_config_set(b, "T", "New", "x");

    gsw_config_augment(a, b);

    g_assert_cmpstr(gsw_config_get(a, "S", "One"), ==, "2");
    g_assert_cmpstr(gsw_config_get(a, "T", "New"), ==, "x");
    {
        gchar **values = gsw_config_get_all(a, "S", "Multi");
        g_assert_nonnull(values);
        g_assert_cmpstr(values[0], ==, "a1");
        g_assert_cmpstr(values[1], ==, "a2");
        g_assert_cmpstr(values[2], ==, "b1");
        g_assert_null(values[3]);
        g_strfreev(values);
    }

    g_object_unref(a);
    g_object_unref(b);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/config/parse-save", test_parse_and_save);
    g_test_add_func("/config/augment", test_augment);
    return g_test_run();
}
