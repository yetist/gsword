/*
 * gsw-codec.c: SWORD compression codecs
 */

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

#define GSW_LZSS_N 4096
#define GSW_LZSS_F 18
#define GSW_LZSS_THRESHOLD 3

GswCompression
gsw_compression_from_string(const gchar *name)
{
    if (name == NULL || *name == '\0')
        return GSW_COMPRESSION_LZSS;
    if (g_ascii_strcasecmp(name, "LZSS") == 0)
        return GSW_COMPRESSION_LZSS;
    if (g_ascii_strcasecmp(name, "ZIP") == 0)
        return GSW_COMPRESSION_ZIP;
    if (g_ascii_strcasecmp(name, "BZIP2") == 0)
        return GSW_COMPRESSION_BZIP2;
    if (g_ascii_strcasecmp(name, "XZ") == 0)
        return GSW_COMPRESSION_XZ;
    return GSW_COMPRESSION_NONE;
}

static gboolean
gsw_lzss_decode(const gchar *in, gsize in_len, GBytes **out, GError **error)
{
    guchar ring[GSW_LZSS_N + GSW_LZSS_F - 1];
    GByteArray *result;
    gsize pos = 0;
    gint r = GSW_LZSS_N - GSW_LZSS_F;
    guint8 flags = 0;
    gint flag_count = 0;

    memset(ring, ' ', GSW_LZSS_N - GSW_LZSS_F);
    result = g_byte_array_new();
    g_byte_array_set_size(result, 0);

    while (pos < in_len) {
        if (flag_count == 0) {
            flags = (guint8)in[pos++];
            flag_count = 8;
        }

        if (flags & 1) {
            guint8 c;

            if (pos >= in_len) {
                g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                            "Truncated LZSS literal");
                g_byte_array_unref(result);
                return FALSE;
            }
            c = (guint8)in[pos++];
            ring[r] = c;
            r = (r + 1) & (GSW_LZSS_N - 1);
            g_byte_array_append(result, &c, 1);
        } else {
            guint8 b0, b1;
            gint match_pos;
            gint match_len;
            gint k;

            if (pos + 1 >= in_len) {
                g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                            "Truncated LZSS match");
                g_byte_array_unref(result);
                return FALSE;
            }

            b0 = (guint8)in[pos++];
            b1 = (guint8)in[pos++];
            match_pos = b0 | ((b1 & 0xf0) << 4);
            match_len = (b1 & 0x0f) + GSW_LZSS_THRESHOLD;

            for (k = 0; k < match_len; k++) {
                guint8 c = ring[(match_pos + k) & (GSW_LZSS_N - 1)];
                ring[r] = c;
                r = (r + 1) & (GSW_LZSS_N - 1);
                g_byte_array_append(result, &c, 1);
            }
        }

        flags >>= 1;
        flag_count--;
    }

    *out = g_byte_array_free_to_bytes(result);
    return TRUE;
}

static gboolean
gsw_zip_decode(const gchar *in, gsize in_len, gsize expected_size,
               GBytes **out, GError **error)
{
    z_stream zs;
    GByteArray *result;
    guchar buffer[64 * 1024];
    int ret;

    memset(&zs, 0, sizeof(zs));
    if (inflateInit(&zs) != Z_OK) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Cannot initialize zlib inflater");
        return FALSE;
    }

    result = g_byte_array_new();
    zs.next_in = (Bytef *)in;
    zs.avail_in = (uInt)in_len;

    do {
        zs.next_out = buffer;
        zs.avail_out = sizeof(buffer);
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Invalid ZIP data: %s",
                        zs.msg != NULL ? zs.msg : "inflate failed");
            inflateEnd(&zs);
            g_byte_array_unref(result);
            return FALSE;
        }
        g_byte_array_append(result, buffer, (guint)(sizeof(buffer) - zs.avail_out));
        if (ret == Z_OK && zs.avail_in == 0 && zs.avail_out != 0)
            break;
    } while (ret != Z_STREAM_END);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Truncated ZIP data");
        g_byte_array_unref(result);
        return FALSE;
    }
    if (expected_size > 0 && result->len < expected_size) {
        /* Some writers overstate the size; accept shorter data. */
    }

    *out = g_byte_array_free_to_bytes(result);
    return TRUE;
}

static gboolean
gsw_bzip2_decode(const gchar *in, gsize in_len, GBytes **out, GError **error)
{
#ifdef HAVE_BZIP2
    bz_stream bs;
    GByteArray *result;
    gchar buffer[64 * 1024];
    int ret;

    memset(&bs, 0, sizeof(bs));
    if (BZ2_bzDecompressInit(&bs, 0, 0) != BZ_OK) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Cannot initialize bzip2 decompressor");
        return FALSE;
    }

    result = g_byte_array_new();
    bs.next_in = (char *)in;
    bs.avail_in = (unsigned int)in_len;

    do {
        bs.next_out = buffer;
        bs.avail_out = sizeof(buffer);
        ret = BZ2_bzDecompress(&bs);
        if (ret != BZ_OK && ret != BZ_STREAM_END) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Invalid BZIP2 data");
            BZ2_bzDecompressEnd(&bs);
            g_byte_array_unref(result);
            return FALSE;
        }
        g_byte_array_append(result, (guint8 *)buffer,
                            (guint)(sizeof(buffer) - bs.avail_out));
        if (ret == BZ_OK && bs.avail_in == 0 && bs.avail_out != 0)
            break;
    } while (ret != BZ_STREAM_END);

    BZ2_bzDecompressEnd(&bs);

    if (ret != BZ_STREAM_END) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Truncated BZIP2 data");
        g_byte_array_unref(result);
        return FALSE;
    }

    *out = g_byte_array_free_to_bytes(result);
    return TRUE;
#else
    (void)in;
    (void)in_len;
    (void)out;
    g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                "BZIP2 support was not compiled in");
    return FALSE;
#endif
}

static gboolean
gsw_xz_decode(const gchar *in, gsize in_len, gsize expected_size,
              GBytes **out, GError **error)
{
#ifdef HAVE_LZMA
    lzma_stream strm = LZMA_STREAM_INIT;
    GByteArray *result;
    guint8 buffer[64 * 1024];
    lzma_ret ret;
    gsize limit = expected_size > 0 ? expected_size + 1024 : 64 * 1024 * 1024;

    if (lzma_stream_decoder(&strm, UINT64_MAX, 0) != LZMA_OK) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Cannot initialize xz decoder");
        return FALSE;
    }

    result = g_byte_array_new();
    strm.next_in = (const uint8_t *)in;
    strm.avail_in = in_len;

    do {
        strm.next_out = buffer;
        strm.avail_out = sizeof(buffer);
        ret = lzma_code(&strm, LZMA_RUN);
        if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Invalid XZ data: %d", (int)ret);
            lzma_end(&strm);
            g_byte_array_unref(result);
            return FALSE;
        }
        g_byte_array_append(result, buffer,
                            (guint)(sizeof(buffer) - strm.avail_out));
        if (result->len > limit) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "XZ data expands beyond the expected size");
            lzma_end(&strm);
            g_byte_array_unref(result);
            return FALSE;
        }
        if (ret == LZMA_OK && strm.avail_in == 0 && strm.avail_out != 0)
            break;
    } while (ret != LZMA_STREAM_END);

    lzma_end(&strm);

    if (ret != LZMA_STREAM_END) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Truncated XZ data");
        g_byte_array_unref(result);
        return FALSE;
    }

    *out = g_byte_array_free_to_bytes(result);
    return TRUE;
#else
    (void)in;
    (void)in_len;
    (void)expected_size;
    (void)out;
    g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                "XZ support was not compiled in");
    return FALSE;
#endif
}

gboolean
gsw_decompress(GswCompression type, const gchar *in, gsize in_len,
               gsize expected_size, GBytes **out, GError **error)
{
    g_return_val_if_fail(in != NULL || in_len == 0, FALSE);
    g_return_val_if_fail(out != NULL, FALSE);

    switch (type) {
    case GSW_COMPRESSION_NONE:
        *out = g_bytes_new(in, in_len);
        return TRUE;
    case GSW_COMPRESSION_LZSS:
        return gsw_lzss_decode(in, in_len, out, error);
    case GSW_COMPRESSION_ZIP:
        return gsw_zip_decode(in, in_len, expected_size, out, error);
    case GSW_COMPRESSION_BZIP2:
        return gsw_bzip2_decode(in, in_len, out, error);
    case GSW_COMPRESSION_XZ:
        return gsw_xz_decode(in, in_len, expected_size, out, error);
    default:
        g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                    "Unsupported compression type");
        return FALSE;
    }
}
