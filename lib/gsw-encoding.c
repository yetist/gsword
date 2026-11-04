/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-encoding.c: SCSU -> UTF-8 decoder
 *
 * Based on the public domain SCSU decoder by Roman Czyborra, as used
 * by SWORD's SCSUUTF8 filter.
 */

#include "gsw-encoding.h"
#include <gsword/gsw-error.h>

#include <string.h>

static const guint16 gsw_scsu_start[8] = {
    0x0000, 0x0080, 0x0100, 0x0300, 0x2000, 0x2080, 0x2100, 0x3000
};

static const guint16 gsw_scsu_slide_init[8] = {
    0x0080, 0x00C0, 0x0400, 0x0600, 0x0900, 0x3040, 0x30A0, 0xFF00
};

static const guint16 gsw_scsu_win[256] = {
    0x0000, 0x0080, 0x0100, 0x0180, 0x0200, 0x0280, 0x0300, 0x0380,
    0x0400, 0x0480, 0x0500, 0x0580, 0x0600, 0x0680, 0x0700, 0x0780,
    0x0800, 0x0880, 0x0900, 0x0980, 0x0A00, 0x0A80, 0x0B00, 0x0B80,
    0x0C00, 0x0C80, 0x0D00, 0x0D80, 0x0E00, 0x0E80, 0x0F00, 0x0F80,
    0x1000, 0x1080, 0x1100, 0x1180, 0x1200, 0x1280, 0x1300, 0x1380,
    0x1400, 0x1480, 0x1500, 0x1580, 0x1600, 0x1680, 0x1700, 0x1780,
    0x1800, 0x1880, 0x1900, 0x1980, 0x1A00, 0x1A80, 0x1B00, 0x1B80,
    0x1C00, 0x1C80, 0x1D00, 0x1D80, 0x1E00, 0x1E80, 0x1F00, 0x1F80,
    0x2000, 0x2080, 0x2100, 0x2180, 0x2200, 0x2280, 0x2300, 0x2380,
    0x2400, 0x2480, 0x2500, 0x2580, 0x2600, 0x2680, 0x2700, 0x2780,
    0x2800, 0x2880, 0x2900, 0x2980, 0x2A00, 0x2A80, 0x2B00, 0x2B80,
    0x2C00, 0x2C80, 0x2D00, 0x2D80, 0x2E00, 0x2E80, 0x2F00, 0x2F80,
    0x3000, 0x3080, 0x3100, 0x3180, 0x3200, 0x3280, 0x3300, 0x3800,
    0xE000, 0xE080, 0xE100, 0xE180, 0xE200, 0xE280, 0xE300, 0xE380,
    0xE400, 0xE480, 0xE500, 0xE580, 0xE600, 0xE680, 0xE700, 0xE780,
    0xE800, 0xE880, 0xE900, 0xE980, 0xEA00, 0xEA80, 0xEB00, 0xEB80,
    0xEC00, 0xEC80, 0xED00, 0xED80, 0xEE00, 0xEE80, 0xEF00, 0xEF80,
    0xF000, 0xF080, 0xF100, 0xF180, 0xF200, 0xF280, 0xF300, 0xF380,
    0xF400, 0xF480, 0xF500, 0xF580, 0xF600, 0xF680, 0xF700, 0xF780,
    0xF800, 0xF880, 0xF900, 0xF980, 0xFA00, 0xFA80, 0xFB00, 0xFB80,
    0xFC00, 0xFC80, 0xFD00, 0xFD80, 0xFE00, 0xFE80, 0xFF00, 0xFF80,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x00C0, 0x0250, 0x0370, 0x0530, 0x3040, 0x30A0, 0xFF60
};

static void
gsw_scsu_emit(GString *out, gulong uchar, gulong *pending_high)
{
    if (uchar >= 0xd800 && uchar <= 0xdbff) {
        *pending_high = uchar & 0x3ff;
        return;
    }
    if (uchar >= 0xdc00 && uchar <= 0xdfff)
        uchar = uchar + 0x2400 + *pending_high * 0x400;

    if (uchar < 0x80) {
        g_string_append_c(out, (gchar)uchar);
    } else if (uchar < 0x800) {
        g_string_append_c(out, (gchar)(0xc0 | (uchar >> 6)));
        g_string_append_c(out, (gchar)(0x80 | (uchar & 0x3f)));
    } else if (uchar < 0x10000) {
        g_string_append_c(out, (gchar)(0xe0 | (uchar >> 12)));
        g_string_append_c(out, (gchar)(0x80 | ((uchar >> 6) & 0x3f)));
        g_string_append_c(out, (gchar)(0x80 | (uchar & 0x3f)));
    } else if (uchar < 0x200000) {
        g_string_append_c(out, (gchar)(0xf0 | (uchar >> 18)));
        g_string_append_c(out, (gchar)(0x80 | ((uchar >> 12) & 0x3f)));
        g_string_append_c(out, (gchar)(0x80 | ((uchar >> 6) & 0x3f)));
        g_string_append_c(out, (gchar)(0x80 | (uchar & 0x3f)));
    }
}

gboolean
gsw_scsu_to_utf8(const gchar *input, gsize len, gchar **out, gsize *out_len,
                 GError **error)
{
    const guint8 *s = (const guint8 *)input;
    GString *result;
    guint16 slide[8];
    guint active = 0;
    gboolean unicode_mode = FALSE;
    gulong pending_high = 0;
    gsize i = 0;

    (void)error;

    memcpy(slide, gsw_scsu_slide_init, sizeof(slide));
    result = g_string_new(NULL);

    while (i < len) {
        guint c = s[i++];

        if (c >= 0x80) {
            gsw_scsu_emit(result, c - 0x80 + slide[active], &pending_high);
        } else if ((c >= 0x20 && c <= 0x7F) || c == 0 || c == 0x9 ||
                   c == 0xA || c == 0xC || c == 0xD) {
            gsw_scsu_emit(result, c, &pending_high);
        } else if (c >= 0x1 && c <= 0x8) {
            if (i >= len)
                break;
            {
                guint d = s[i++];
                gsw_scsu_emit(result, d < 0x80 ? d + gsw_scsu_start[c - 1]
                                               : d - 0x80 + slide[c - 1],
                              &pending_high);
            }
        } else if (c >= 0x10 && c <= 0x17) {
            active = c - 0x10;
        } else if (c >= 0x18 && c <= 0x1F) {
            active = c - 0x18;
            if (i >= len)
                break;
            slide[active] = gsw_scsu_win[s[i++]];
        } else if (c == 0xB) {
            guint c2, d;
            if (i >= len)
                break;
            c2 = s[i++];
            if (i >= len)
                break;
            d = s[i++];
            slide[active = c2 >> 5] = (guint16)(0x10000 +
                ((((c2 & 0x1F) << 8) | d) << 7));
        } else if (c == 0xE) {
            guint c2;
            if (i >= len)
                break;
            c2 = s[i++];
            if (i >= len)
                break;
            gsw_scsu_emit(result, (c2 << 8) | s[i++], &pending_high);
        } else if (c == 0xF) {
            unicode_mode = TRUE;
            while (unicode_mode) {
                if (i >= len)
                    break;
                c = s[i++];
                if (c <= 0xDF || c >= 0xF3) {
                    if (i >= len)
                        break;
                    gsw_scsu_emit(result, (c << 8) | s[i++], &pending_high);
                } else if (c == 0xF0) {
                    guint c2;
                    if (i >= len)
                        break;
                    c2 = s[i++];
                    if (i >= len)
                        break;
                    gsw_scsu_emit(result, (c2 << 8) | s[i++], &pending_high);
                } else if (c >= 0xE0 && c <= 0xE7) {
                    active = c - 0xE0;
                    unicode_mode = FALSE;
                } else if (c >= 0xE8 && c <= 0xEF) {
                    if (i >= len)
                        break;
                    slide[active = c - 0xE8] = gsw_scsu_win[s[i++]];
                    unicode_mode = FALSE;
                } else if (c == 0xF1) {
                    guint c2, d;
                    if (i >= len)
                        break;
                    c2 = s[i++];
                    if (i >= len)
                        break;
                    d = s[i++];
                    slide[active = c2 >> 5] = (guint16)(0x10000 +
                        ((((c2 & 0x1F) << 8) | d) << 7));
                    unicode_mode = FALSE;
                }
            }
        }
    }

    if (out_len != NULL)
        *out_len = result->len;
    *out = g_string_free(result, FALSE);
    return TRUE;
}
