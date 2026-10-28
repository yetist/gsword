/*
 * gsw-str-key.h: string key for dictionaries
 *
 * This file is part of gsword.
 */

#ifndef __GSW_STR_KEY_H__
#define __GSW_STR_KEY_H__ 1

#include <gsword/gsw-key.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

typedef GswKey GswStrKey;

#define GSW_TYPE_STR_KEY (gsw_str_key_get_type())

GType       gsw_str_key_get_type(void);
GswStrKey  *gsw_str_key_new(const gchar *text);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswStrKey, gsw_key_unref)

G_END_DECLS

#endif /* __GSW_STR_KEY_H__ */
