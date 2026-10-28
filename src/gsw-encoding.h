/*
 * gsw-encoding.h: internal encoding helpers
 */

#ifndef __GSW_ENCODING_H__
#define __GSW_ENCODING_H__ 1

#include <glib.h>

gboolean gsw_scsu_to_utf8(const gchar *input, gsize len, gchar **out,
                          gsize *out_len, GError **error);

#endif /* __GSW_ENCODING_H__ */
