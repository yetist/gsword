/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include <gsword/gsw-error.h>
#include "gsw-tar.h"
#include <gsword/gsw-error.h>
#include "gsw-utils.h"
#include <glib/gstdio.h>
#include <string.h>
#include <zlib.h>

static void
tar_write_block(gzFile gz, const gchar *block)
{
    g_assert_cmpint(gzwrite(gz, block, 512), ==, 512);
}

static void
tar_add(gzFile gz, const gchar *name, const gchar *data, gsize len,
        gchar type)
{
    gchar block[512];

    memset(block, 0, sizeof(block));
    g_strlcpy(block, name, 100);
    g_snprintf(block + 100, 8, "%07o", 0644);
    g_snprintf(block + 108, 8, "%07o", 0);
    g_snprintf(block + 116, 8, "%07o", 0);
    g_snprintf(block + 124, 12, "%011lo", (unsigned long)len);
    g_snprintf(block + 136, 12, "%011lo", 0UL);
    memset(block + 148, ' ', 8);
    block[156] = type;
    memcpy(block + 257, "ustar", 6);
    memcpy(block + 263, "00", 2);

    tar_write_block(gz, block);

    if (len > 0 && data != NULL) {
        gsize padded = ((len + 511) / 512) * 512;
        gchar *buffer = g_malloc0(padded);
        memcpy(buffer, data, len);
        g_assert_cmpint(gzwrite(gz, buffer, (unsigned int)padded), ==,
                        (int)padded);
        g_free(buffer);
    }
}

static gchar *
make_archive(const gchar *dir, const gchar *name, gboolean malicious)
{
    gchar *path = g_build_filename(dir, name, NULL);
    gzFile gz = gzopen(path, "wb");

    g_assert_nonnull(gz);

    if (malicious) {
        tar_add(gz, "../evil.conf", "evil\n", 5, '0');
    } else {
        tar_add(gz, "mods.d/", NULL, 0, '5');
        tar_add(gz, "mods.d/test.conf",
                "[Test]\nDescription=Test\nModDrv=RawText\n", 39, '0');
    }

    {
        gchar zero[512];
        memset(zero, 0, sizeof(zero));
        tar_write_block(gz, zero);
        tar_write_block(gz, zero);
    }

    gzclose(gz);
    return path;
}

static void
test_extract(void)
{
    GError *error = NULL;
    gchar *dir = g_dir_make_tmp("gsword-tar-XXXXXX", &error);
    gchar *archive;
    gchar *dest;
    gchar *conf;
    gchar *content = NULL;
    gsize len = 0;

    g_assert_no_error(error);
    g_assert_nonnull(dir);

    archive = make_archive(dir, "mods.d.tar.gz", FALSE);
    dest = g_build_filename(dir, "out", NULL);
    g_assert_true(gsw_tar_extract_gz(archive, dest, &error));
    g_assert_no_error(error);

    conf = g_build_filename(dest, "mods.d", "test.conf", NULL);
    g_assert_true(g_file_get_contents(conf, &content, &len, &error));
    g_assert_no_error(error);
    g_assert_nonnull(strstr(content, "[Test]"));

    g_free(content);
    g_free(conf);
    g_free(dest);
    g_free(archive);

    archive = make_archive(dir, "evil.tar.gz", TRUE);
    dest = g_build_filename(dir, "evil-out", NULL);
    g_assert_false(gsw_tar_extract_gz(archive, dest, &error));
    g_assert_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA);
    g_clear_error(&error);
    {
        gchar *evil = g_build_filename(dir, "evil.conf", NULL);
        g_assert_false(g_file_test(evil, G_FILE_TEST_EXISTS));
        g_free(evil);
    }

    g_free(dest);
    g_free(archive);
    gsw_dir_remove_recursive(dir, NULL);
    g_free(dir);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/tar/extract", test_extract);
    return g_test_run();
}
