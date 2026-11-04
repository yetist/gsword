/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "gsw-cipher.h"
#include <gsword/gsw-error.h>
#include <string.h>

static void
hex_to_bytes(const gchar *hex, gchar **out, gsize *len)
{
    gsize i;
    gsize n = strlen(hex) / 2;

    *out = g_malloc(n + 1);
    for (i = 0; i < n; i++) {
        gchar byte[3] = { hex[i * 2], hex[i * 2 + 1], 0 };
        (*out)[i] = (gchar)g_ascii_strtoull(byte, NULL, 16);
    }
    *len = n;
}

static void
test_roundtrip(void)
{
    const gchar *key = "aaaa-bbbb-cccc-dddd-eeee";
    const gchar *plain = "hello world";
    const gchar *expected_hex = "2c315a7899884700317c6b";
    gchar *cipher = NULL;
    gsize cipher_len = 0;
    GBytes *encrypted = NULL;
    GBytes *decrypted = NULL;
    GError *error = NULL;
    gsize len = 0;
    const gchar *data;

    hex_to_bytes(expected_hex, &cipher, &cipher_len);

    g_assert_true(gsw_cipher_decrypt(key, cipher, cipher_len, &decrypted, &error));
    g_assert_no_error(error);
    data = g_bytes_get_data(decrypted, &len);
    g_assert_cmpuint(len, ==, strlen(plain));
    g_assert_cmpmem(data, len, plain, strlen(plain));
    g_bytes_unref(decrypted);

    g_assert_true(gsw_cipher_encrypt(key, plain, strlen(plain), &encrypted, &error));
    g_assert_no_error(error);
    data = g_bytes_get_data(encrypted, &len);
    g_assert_cmpuint(len, ==, cipher_len);
    g_assert_cmpmem(data, len, cipher, cipher_len);
    g_bytes_unref(encrypted);

    g_free(cipher);
}

static void
test_missing_key(void)
{
    GError *error = NULL;
    GBytes *out = NULL;

    g_assert_false(gsw_cipher_decrypt("", "abc", 3, &out, &error));
    g_assert_error(error, GSW_ERROR, GSW_ERROR_CIPHERED);
    g_clear_error(&error);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/cipher/sapphire-vector", test_roundtrip);
    g_test_add_func("/cipher/missing-key", test_missing_key);
    return g_test_run();
}
