/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * test-image-utils.c: map image path and sizing helpers
 */

#include "image-utils.h"

#include <glib.h>
#include <glib/gstdio.h>

static void
test_extract_source(void)
{
    gchar *source;

    source = gsword_image_extract_src("<img src=\"images/foo.jpg\">");
    g_assert_cmpstr(source, ==, "images/foo.jpg");
    g_free(source);

    source = gsword_image_extract_src("<IMG SRC='images/bar.png'>");
    g_assert_cmpstr(source, ==, "images/bar.png");
    g_free(source);

    source = gsword_image_extract_src(
        "<img data-src=\"bad.png\" src=\"good.png\">");
    g_assert_cmpstr(source, ==, "good.png");
    g_free(source);

    source = gsword_image_extract_src("<img src = \"images/spaced.png\">");
    g_assert_cmpstr(source, ==, "images/spaced.png");
    g_free(source);

    source = gsword_image_extract_src("<img src=\"a&amp;b.jpg\">");
    g_assert_cmpstr(source, ==, "a&b.jpg");
    g_free(source);

    source = gsword_image_extract_src("<p>No image here</p>");
    g_assert_null(source);
}

static void
test_resolve_path(void)
{
    GError *error = NULL;
    gchar *base = g_dir_make_tmp("gsword-image-XXXXXX", &error);
    gchar *images;
    gchar *file;
    gchar *resolved;

    g_assert_no_error(error);
    g_assert_nonnull(base);
    images = g_build_filename(base, "images", NULL);
    g_assert_cmpint(g_mkdir_with_parents(images, 0755), ==, 0);
    file = g_build_filename(images, "foo.png", NULL);
    g_assert_true(g_file_set_contents(file, "x", 1, &error));
    g_assert_no_error(error);

    resolved = gsword_image_resolve_path(base, "images/foo.png", &error);
    g_assert_no_error(error);
    g_assert_cmpstr(resolved, ==, file);
    g_free(resolved);

    resolved = gsword_image_resolve_path(base, "/images/foo.png", &error);
    g_assert_no_error(error);
    g_assert_cmpstr(resolved, ==, file);
    g_free(resolved);

    resolved = gsword_image_resolve_path(base, "../outside.png", &error);
    g_assert_null(resolved);
    g_assert_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL);
    g_clear_error(&error);

    resolved = gsword_image_resolve_path(base, "images/missing.png", &error);
    g_assert_null(resolved);
    g_assert_error(error, G_FILE_ERROR, G_FILE_ERROR_NOENT);
    g_clear_error(&error);

    g_remove(file);
    g_rmdir(images);
    g_rmdir(base);
    g_free(file);
    g_free(images);
    g_free(base);
}

static void
test_fit_sizes(void)
{
    g_assert_cmpfloat_with_epsilon(
        gsword_image_fit_width(1000, 500, 10), 0.49, 0.0001);
    g_assert_cmpfloat_with_epsilon(
        gsword_image_fit_width(1000, 0, 10), 1.0, 0.0001);
    g_assert_cmpfloat_with_epsilon(
        gsword_image_fit_window(1000, 2000, 500, 500), 0.25, 0.0001);
    g_assert_cmpfloat_with_epsilon(
        gsword_image_fit_window(0, 2000, 500, 500), 1.0, 0.0001);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/image-utils/extract-source", test_extract_source);
    g_test_add_func("/image-utils/resolve-path", test_resolve_path);
    g_test_add_func("/image-utils/fit-sizes", test_fit_sizes);
    return g_test_run();
}
