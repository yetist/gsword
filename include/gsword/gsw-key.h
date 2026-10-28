/*
 * gsw-key.h: base type for module keys
 *
 * This file is part of gsword.
 */

#ifndef __GSW_KEY_H__
#define __GSW_KEY_H__ 1

#include <glib-object.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

typedef struct _GswKey GswKey;

#define GSW_TYPE_KEY (gsw_key_get_type())

GType        gsw_key_get_type(void);
GswKey      *gsw_key_ref(GswKey *self);
void         gsw_key_unref(GswKey *self);
GswKey      *gsw_key_clone(GswKey *self);

const gchar *gsw_key_get_text(GswKey *self);
gboolean     gsw_key_set_text(GswKey *self, const gchar *text);
gboolean     gsw_key_next(GswKey *self, gint steps);
gboolean     gsw_key_prev(GswKey *self, gint steps);
gint         gsw_key_compare(GswKey *self, GswKey *other);
gboolean     gsw_key_equals(GswKey *self, GswKey *other);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswKey, gsw_key_unref)

G_END_DECLS

#endif /* __GSW_KEY_H__ */
