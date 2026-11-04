/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include "gsw-codec.h"
#include <gsword/gsw-error.h>
#include <string.h>
#include <zlib.h>
#ifdef HAVE_BZIP2
#include <bzlib.h>
#endif
#ifdef HAVE_LZMA
#include <lzma.h>
#endif

static void
check_lzss(const gchar *hex, const gchar *expected)
{
    GError *error = NULL;
    GBytes *out = NULL;
    gsize len = 0;
    const gchar *data;
    gsize i;
    gsize hex_len = strlen(hex);
    gchar *input = g_malloc(hex_len / 2 + 1);

    for (i = 0; i < hex_len; i += 2) {
        gchar byte[3] = { hex[i], hex[i + 1], 0 };
        input[i / 2] = (gchar)g_ascii_strtoull(byte, NULL, 16);
    }

    g_assert_true(gsw_decompress(GSW_COMPRESSION_LZSS, input, hex_len / 2,
                                 0, &out, &error));
    g_assert_no_error(error);
    data = g_bytes_get_data(out, &len);
    g_assert_cmpuint(len, ==, strlen(expected));
    g_assert_cmpmem(data, len, expected, strlen(expected));
    g_bytes_unref(out);
    g_free(input);
}

static void
test_lzss(void)
{
    check_lzss("", "");
    check_lzss("0161", "a");
    check_lzss("07616263", "abc");
    check_lzss("1f68656c6c6fedf9", "hello hello hello");
    check_lzss("00edf6", "         ");
    check_lzss("07616263eefffaf6", "abcabcabcabcabcabcabcabcabcabc");
}

static void
test_zip(void)
{
    const gchar *input = "zip round trip: hello hello hello";
    uLongf comp_len = compressBound((uLong)strlen(input));
    gchar *compressed = g_malloc(comp_len);
    GError *error = NULL;
    GBytes *out = NULL;
    gsize len = 0;
    const gchar *data;

    g_assert_cmpint(compress2((Bytef *)compressed, &comp_len,
                              (const Bytef *)input, (uLong)strlen(input), 9),
                    ==, Z_OK);
    g_assert_true(gsw_decompress(GSW_COMPRESSION_ZIP, compressed,
                                 comp_len, strlen(input), &out, &error));
    g_assert_no_error(error);
    data = g_bytes_get_data(out, &len);
    g_assert_cmpuint(len, ==, strlen(input));
    g_assert_cmpmem(data, len, input, strlen(input));
    g_bytes_unref(out);
    g_free(compressed);
}


static void
test_bzip2(void)
{
#ifdef HAVE_BZIP2
    const gchar *input = "bzip2 round trip: hello hello hello";
    unsigned int comp_len = (unsigned int)(strlen(input) + 600);
    gchar *compressed = g_malloc(comp_len);
    GError *error = NULL;
    GBytes *out = NULL;
    gsize len = 0;
    const gchar *data;

    g_assert_cmpint(BZ2_bzBuffToBuffCompress(compressed, &comp_len,
                                             (char *)input,
                                             (unsigned int)strlen(input),
                                             9, 0, 30), ==, BZ_OK);
    g_assert_true(gsw_decompress(GSW_COMPRESSION_BZIP2, compressed, comp_len,
                                 0, &out, &error));
    g_assert_no_error(error);
    data = g_bytes_get_data(out, &len);
    g_assert_cmpuint(len, ==, strlen(input));
    g_assert_cmpmem(data, len, input, strlen(input));
    g_bytes_unref(out);
    g_free(compressed);
#else
    g_test_skip("BZIP2 support not compiled in");
#endif
}

static void
test_xz(void)
{
#ifdef HAVE_LZMA
    const gchar *input = "xz round trip: hello hello hello";
    gsize bound = lzma_stream_buffer_bound(strlen(input));
    guint8 *compressed = g_malloc(bound);
    gsize comp_len = 0;
    GError *error = NULL;
    GBytes *out = NULL;
    gsize len = 0;
    const gchar *data;

    g_assert_cmpint(lzma_easy_buffer_encode(6, LZMA_CHECK_CRC64, NULL,
                                            (const uint8_t *)input,
                                            strlen(input), compressed,
                                            &comp_len, bound), ==, LZMA_OK);
    g_assert_true(gsw_decompress(GSW_COMPRESSION_XZ, (const gchar *)compressed,
                                 comp_len, strlen(input), &out, &error));
    g_assert_no_error(error);
    data = g_bytes_get_data(out, &len);
    g_assert_cmpuint(len, ==, strlen(input));
    g_assert_cmpmem(data, len, input, strlen(input));
    g_bytes_unref(out);
    g_free(compressed);
#else
    g_test_skip("XZ support not compiled in");
#endif
}

static void
test_invalid(void)
{
    GError *error = NULL;
    GBytes *out = NULL;
    const gchar bad[] = "\x01";

    g_assert_false(gsw_decompress(GSW_COMPRESSION_LZSS, bad, sizeof(bad) - 1,
                                  0, &out, &error));
    g_assert_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA);
    g_clear_error(&error);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/codec/lzss", test_lzss);
    g_test_add_func("/codec/zip", test_zip);
    g_test_add_func("/codec/bzip2", test_bzip2);
    g_test_add_func("/codec/xz", test_xz);
    g_test_add_func("/codec/invalid", test_invalid);
    return g_test_run();
}
