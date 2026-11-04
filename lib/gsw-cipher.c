/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-cipher.c: SWORD SWCipher (Sapphire II) compatible decryption
 *
 * The Sapphire II algorithm is public domain (Michael Paul Johnson).
 * The personalization/checksum logic follows SWORD's SWCipher.
 */

#include "gsw-cipher.h"
#include <gsword/gsw-error.h>

#include <string.h>

typedef struct {
    guint8 cards[256];
    guint8 rotor;
    guint8 ratchet;
    guint8 avalanche;
    guint8 last_plain;
    guint8 last_cipher;
} GswSapphire;

static const gchar gsw_lats[62] = {
    'b', 'c', 'e', 'a', 'f', 'g', 'i', 'j', 'k', 'l',
    'h', 'm', 'p', 'q', 'B', 'r', 'H', 'o', 's', 't',
    'T', 'u', 'w', 'x', 'y', 'A', 'd', 'C', 'D', 'z',
    'E', 'F', 'I', 'J', 'K', 'G', 'L', 'N', 'O', '7',
    'P', 'Q', 'M', 'R', 'S', 'U', 'V', 'W', 'X', 'Y',
    '9', '0', '1', '2', 'Z', '3', '6', '4', 'n', '8',
    'v', '5'
};

static guint8
gsw_sapphire_keyrand(GswSapphire *s, int limit, const guint8 *key,
                     guint8 keysize, guint8 *rsum, guint *keypos)
{
    guint u;
    guint retry_limiter = 0;
    guint mask = 1;

    if (limit == 0)
        return 0;

    while (mask < (guint)limit)
        mask = (mask << 1) + 1;

    do {
        *rsum = s->cards[*rsum] + key[(*keypos)++];
        if (*keypos >= keysize) {
            *keypos = 0;
            *rsum += keysize;
        }
        u = mask & *rsum;
        if (++retry_limiter > 11)
            u %= (guint)limit;
    } while (u > (guint)limit);

    return (guint8)u;
}

static void
gsw_sapphire_hash_init(GswSapphire *s)
{
    int i, j;

    s->rotor = 1;
    s->ratchet = 3;
    s->avalanche = 5;
    s->last_plain = 7;
    s->last_cipher = 11;

    for (i = 0, j = 255; i < 256; i++, j--)
        s->cards[i] = (guint8)j;
}

static void
gsw_sapphire_initialize(GswSapphire *s, const guint8 *key, guint8 keysize)
{
    int i;
    guint8 toswap, swaptemp, rsum;
    guint keypos;

    if (keysize < 1) {
        gsw_sapphire_hash_init(s);
        return;
    }

    for (i = 0; i < 256; i++)
        s->cards[i] = (guint8)i;

    toswap = 0;
    keypos = 0;
    rsum = 0;
    for (i = 255; i >= 0; i--) {
        toswap = gsw_sapphire_keyrand(s, i, key, keysize, &rsum, &keypos);
        swaptemp = s->cards[i];
        s->cards[i] = s->cards[toswap];
        s->cards[toswap] = swaptemp;
    }

    s->rotor = s->cards[1];
    s->ratchet = s->cards[3];
    s->avalanche = s->cards[5];
    s->last_plain = s->cards[7];
    s->last_cipher = s->cards[rsum];
}

static guint8
gsw_sapphire_decrypt_byte(GswSapphire *s, guint8 b)
{
    guint8 swaptemp;

    s->ratchet += s->cards[s->rotor++];
    swaptemp = s->cards[s->last_cipher];
    s->cards[s->last_cipher] = s->cards[s->ratchet];
    s->cards[s->ratchet] = s->cards[s->last_plain];
    s->cards[s->last_plain] = s->cards[s->rotor];
    s->cards[s->rotor] = swaptemp;
    s->avalanche += s->cards[swaptemp];

    s->last_plain = b ^ s->cards[(s->cards[s->ratchet] +
                                  s->cards[s->rotor]) & 0xFF] ^
                    s->cards[s->cards[(s->cards[s->last_plain] +
                                       s->cards[s->last_cipher] +
                                       s->cards[s->avalanche]) & 0xFF]];
    s->last_cipher = b;
    return s->last_plain;
}

static guint8
gsw_sapphire_encrypt_byte(GswSapphire *s, guint8 b)
{
    guint8 swaptemp;

    s->ratchet += s->cards[s->rotor++];
    swaptemp = s->cards[s->last_cipher];
    s->cards[s->last_cipher] = s->cards[s->ratchet];
    s->cards[s->ratchet] = s->cards[s->last_plain];
    s->cards[s->last_plain] = s->cards[s->rotor];
    s->cards[s->rotor] = swaptemp;
    s->avalanche += s->cards[swaptemp];

    s->last_cipher = b ^ s->cards[(s->cards[s->ratchet] +
                                   s->cards[s->rotor]) & 0xFF] ^
                     s->cards[s->cards[(s->cards[s->last_plain] +
                                        s->cards[s->last_cipher] +
                                        s->cards[s->avalanche]) & 0xFF]];
    s->last_plain = b;
    return s->last_cipher;
}

static gint
gsw_cipher_char_hash(gchar c)
{
    gint i;

    for (i = 0; i < 62; i++) {
        if (gsw_lats[i] == c)
            return i;
    }
    return 0; /* std::map<char,int>::operator[] default */
}

static gchar *
gsw_cipher_personalize(const gchar *key, gboolean encode)
{
    GString *segs[5];
    gsize segn = 0;
    gsize i;
    gchar chk_sum[4] = { 0, 0, 0, 0 };
    GString *result;
    const gchar *p;

    if (key == NULL)
        key = "";

    for (i = 0; i < 5; i++)
        segs[i] = g_string_new(NULL);

    for (p = key; *p != '\0' && segn < 5; p++) {
        if (*p == '-') {
            segn++;
        } else {
            g_string_append_c(segs[segn], *p);
        }
    }

    /* SWORD truncates/zero-pads segment 4 to four bytes for the checksum. */
    if (segs[4]->len > 4)
        g_string_truncate(segs[4], 4);
    while (segs[4]->len < 4)
        g_string_append_c(segs[4], '\0');
    memcpy(chk_sum, segs[4]->str, 4);

    for (i = 0; i < 4; i++) {
        gint csum = 0;
        gsize j;
        gsize seg0_len = segs[0]->len;

        for (j = 0; j < segs[i]->len && j < seg0_len; j++) {
            gint hash = gsw_cipher_char_hash(segs[i]->str[j]);
            gint obfus;
            gchar ch;

            if (seg0_len == 0)
                obfus = 0;
            else
                obfus = gsw_cipher_char_hash(segs[0]->str[j % seg0_len]);

            if (encode) {
                obfus = hash - (i ? obfus : 0);
                if (obfus < 0)
                    obfus = 62 + obfus;
            } else {
                obfus = (hash + (i ? obfus : 0)) % 62;
            }

            if (i) {
                ch = gsw_lats[obfus];
                segs[i]->str[j] = ch;
            }
            csum += encode ? obfus : hash;
        }
        segs[4]->str[i] = gsw_lats[csum % 62];
    }

    result = g_string_new(NULL);
    for (i = 0; i < 4; i++) {
        if (result->len > 0)
            g_string_append_c(result, '-');
        if (!(!encode && i == 0))
            g_string_append_len(result, segs[i]->str, (gssize)segs[i]->len);
    }
    if (encode) {
        g_string_append_c(result, '-');
        g_string_append_len(result, segs[4]->str, 4);
    }

    {
        gboolean checksum_ok = memcmp(chk_sum, segs[4]->str, 4) == 0;
        gchar *ret;

        if (!encode && !checksum_ok) {
            g_string_free(result, TRUE);
            ret = g_strdup(key);
        } else {
            ret = g_string_free(result, FALSE);
        }
        for (i = 0; i < 5; i++)
            g_string_free(segs[i], TRUE);
        return ret;
    }
}

static gboolean
gsw_cipher_run(const gchar *key, const gchar *data, gsize len,
               gboolean decrypt, GBytes **out, GError **error)
{
    GswSapphire master;
    GswSapphire work;
    gchar *personalized;
    guint8 *buffer;
    gsize i;

    if (key == NULL || *key == '\0') {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CIPHERED,
                    "Module is encrypted but no CipherKey is configured");
        return FALSE;
    }

    personalized = gsw_cipher_personalize(key, FALSE);
    gsw_sapphire_initialize(&master, (const guint8 *)personalized,
                            (guint8)strlen(personalized));
    g_free(personalized);

    work = master;
    buffer = g_malloc(len + 1);
    for (i = 0; i < len; i++) {
        buffer[i] = decrypt
                  ? gsw_sapphire_decrypt_byte(&work, (guint8)data[i])
                  : gsw_sapphire_encrypt_byte(&work, (guint8)data[i]);
    }
    buffer[len] = 0;

    *out = g_bytes_new_take(buffer, len);
    return TRUE;
}

gboolean
gsw_cipher_decrypt(const gchar *key, const gchar *data, gsize len,
                   GBytes **out, GError **error)
{
    return gsw_cipher_run(key, data, len, TRUE, out, error);
}

gboolean
gsw_cipher_encrypt(const gchar *key, const gchar *data, gsize len,
                   GBytes **out, GError **error)
{
    return gsw_cipher_run(key, data, len, FALSE, out, error);
}
