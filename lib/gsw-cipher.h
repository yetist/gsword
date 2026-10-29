/*
 * gsw-cipher.h: SWORD CipherKey / Sapphire helpers
 */

#ifndef __GSW_CIPHER_H__
#define __GSW_CIPHER_H__ 1

#include <glib.h>

G_BEGIN_DECLS

gboolean gsw_cipher_decrypt(const gchar *key, const gchar *data, gsize len,
                            GBytes **out, GError **error);
gboolean gsw_cipher_encrypt(const gchar *key, const gchar *data, gsize len,
                            GBytes **out, GError **error);

G_END_DECLS

#endif /* __GSW_CIPHER_H__ */
