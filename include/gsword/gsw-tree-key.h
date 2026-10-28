/*
 * gsw-tree-key.h: tree key for general books
 *
 * This file is part of gsword.
 */

#ifndef __GSW_TREE_KEY_H__
#define __GSW_TREE_KEY_H__ 1

#include <gsword/gsw-key.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

typedef GswKey GswTreeKey;

#define GSW_TYPE_TREE_KEY (gsw_tree_key_get_type())

GType        gsw_tree_key_get_type(void);
/**
 * gsw_tree_key_new:
 *
 * Returns: (transfer full): result
 */
GswTreeKey  *gsw_tree_key_new(const gchar *base_path);

const gchar *gsw_tree_key_get_text(GswTreeKey *self);
gboolean     gsw_tree_key_set_text(GswTreeKey *self, const gchar *path);
gboolean     gsw_tree_key_first_child(GswTreeKey *self);
gboolean     gsw_tree_key_next_sibling(GswTreeKey *self);
gboolean     gsw_tree_key_parent(GswTreeKey *self);
gboolean     gsw_tree_key_has_children(GswTreeKey *self);
gint         gsw_tree_key_get_level(GswTreeKey *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswTreeKey, gsw_key_unref)

G_END_DECLS

#endif /* __GSW_TREE_KEY_H__ */
