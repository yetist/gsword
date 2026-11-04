/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-codec.h: internal decompression helpers
 */

#ifndef __GSW_CODEC_H__
#define __GSW_CODEC_H__ 1

#include <glib.h>

typedef enum {
    GSW_COMPRESSION_NONE,
    GSW_COMPRESSION_LZSS,
    GSW_COMPRESSION_ZIP,
    GSW_COMPRESSION_BZIP2,
    GSW_COMPRESSION_XZ
} GswCompression;

GswCompression gsw_compression_from_string(const gchar *name);
gboolean gsw_decompress(GswCompression type, const gchar *in, gsize in_len,
                        gsize expected_size, GBytes **out, GError **error);

#endif /* __GSW_CODEC_H__ */
