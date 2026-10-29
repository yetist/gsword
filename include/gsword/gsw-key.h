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

/**
 * GswKey:
 *
 * Opaque base type for module positions. Concrete keys are obtained from
 * gsw_module_create_key() or by creating a #GswVerseKey, #GswStrKey, or
 * #GswTreeKey directly. Reference-counted with gsw_key_ref() and
 * gsw_key_unref().
 */
typedef struct _GswKey GswKey;

#define GSW_TYPE_KEY (gsw_key_get_type())

/**
 * gsw_key_get_type:
 *
 * Gets the #GType for #GswKey.
 *
 * Returns: the #GType for #GswKey
 */
GType        gsw_key_get_type(void);
/**
 * gsw_key_ref:
 * @self: a #GswKey
 *
 * Increases the reference count of @self.
 *
 * Returns: (transfer full): @self with an increased reference count
 */
GswKey      *gsw_key_ref(GswKey *self);
/**
 * gsw_key_unref:
 * @self: a #GswKey
 *
 * Decreases the reference count of @self.
 */
void         gsw_key_unref(GswKey *self);
/**
 * gsw_key_clone:
 * @self: a #GswKey
 *
 * Creates an independent copy of @self.
 *
 * Returns: (transfer full): a new independent #GswKey
 */
GswKey      *gsw_key_clone(GswKey *self);

/**
 * gsw_key_get_text:
 * @self: a #GswKey
 *
 * Gets the text representation of @self.
 *
 * Returns: (transfer none): the key text
 */
const gchar *gsw_key_get_text(GswKey *self);
/**
 * gsw_key_set_text:
 * @self: a #GswKey
 * @text: key text
 *
 * Sets the key from text and reports whether the result is valid.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_key_set_text(GswKey *self, const gchar *text);
/**
 * gsw_key_next:
 * @self: a #GswKey
 * @steps: number of entries to move; may be negative
 *
 * Moves the key forward.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_key_next(GswKey *self, gint steps);
/**
 * gsw_key_prev:
 * @self: a #GswKey
 * @steps: number of entries to move; may be negative
 *
 * Moves the key backward.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_key_prev(GswKey *self, gint steps);
/**
 * gsw_key_compare:
 * @self: a #GswKey
 * @other: the #GswKey to compare against
 *
 * Compares two keys.
 *
 * Returns: a negative value if the first argument sorts before the second, zero if they are equal, or a positive value otherwise
 */
gint         gsw_key_compare(GswKey *self, GswKey *other);
/**
 * gsw_key_equals:
 * @self: a #GswKey
 * @other: the #GswKey to compare against
 *
 * Checks whether two keys refer to the same position.
 *
 * Returns: %TRUE if the keys refer to the same position, %FALSE otherwise
 */
gboolean     gsw_key_equals(GswKey *self, GswKey *other);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswKey, gsw_key_unref)

G_END_DECLS

#endif /* __GSW_KEY_H__ */
