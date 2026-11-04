/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
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

/**
 * GswTreeKey:
 *
 * A slash-separated path key for general book modules. It supports moving
 * between a node, its parent, its first child, and its next sibling.
 */
typedef GswKey GswTreeKey;

#define GSW_TYPE_TREE_KEY (gsw_tree_key_get_type())

/**
 * gsw_tree_key_get_type:
 *
 * Gets the #GType for #GswTreeKey.
 *
 * Returns: the #GType for #GswTreeKey
 */
GType        gsw_tree_key_get_type(void);
/**
 * gsw_tree_key_new:
 * @base_path: base directory of the general book
 *
 * Creates a tree key rooted at @base_path.
 *
 * Returns: (transfer full): a new #GswTreeKey
 */
GswTreeKey  *gsw_tree_key_new(const gchar *base_path);

/**
 * gsw_tree_key_get_text:
 * @self: a #GswTreeKey
 *
 * Gets the current slash-separated tree path.
 *
 * Returns: (transfer none): the current tree path
 */
const gchar *gsw_tree_key_get_text(GswTreeKey *self);
/**
 * gsw_tree_key_set_text:
 * @self: a #GswTreeKey
 * @path: a filesystem path
 *
 * Positions the tree key at a slash-separated path.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_tree_key_set_text(GswTreeKey *self, const gchar *path);
/**
 * gsw_tree_key_first_child:
 * @self: a #GswTreeKey
 *
 * Moves to the first child node.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_tree_key_first_child(GswTreeKey *self);
/**
 * gsw_tree_key_next_sibling:
 * @self: a #GswTreeKey
 *
 * Moves to the next sibling node.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_tree_key_next_sibling(GswTreeKey *self);
/**
 * gsw_tree_key_parent:
 * @self: a #GswTreeKey
 *
 * Moves to the parent node.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_tree_key_parent(GswTreeKey *self);
/**
 * gsw_tree_key_has_children:
 * @self: a #GswTreeKey
 *
 * Checks whether the current node has children.
 *
 * Returns: %TRUE if the condition holds, %FALSE otherwise
 */
gboolean     gsw_tree_key_has_children(GswTreeKey *self);
/**
 * gsw_tree_key_get_level:
 * @self: a #GswTreeKey
 *
 * Gets the depth of the current node.
 *
 * Returns: the depth of the current node
 */
gint         gsw_tree_key_get_level(GswTreeKey *self);
/**
 * gsw_tree_key_get_offset:
 * @self: a #GswTreeKey
 *
 * Gets the current node offset from the book's index file.
 *
 * Returns: the node offset, or 0 for the root or an invalid key
 */
glong        gsw_tree_key_get_offset(GswTreeKey *self);
/**
 * gsw_tree_key_set_offset:
 * @self: a #GswTreeKey
 * @offset: node offset from the book's index file
 *
 * Positions the key at a node offset.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_tree_key_set_offset(GswTreeKey *self, glong offset);
/**
 * gsw_tree_key_get_local_name:
 * @self: a #GswTreeKey
 *
 * Gets the local name of the current node, without its parent path.
 *
 * Returns: (transfer none): the local node name
 */
const gchar *gsw_tree_key_get_local_name(GswTreeKey *self);
/**
 * gsw_tree_key_previous_sibling:
 * @self: a #GswTreeKey
 *
 * Moves to the previous sibling node.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_tree_key_previous_sibling(GswTreeKey *self);
/**
 * gsw_tree_key_append_child:
 * @self: a #GswTreeKey
 * @name: local name for the new node
 * @error: return location for a #GError, or %NULL
 *
 * Appends a child node to the current node and positions the key on it.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_tree_key_append_child(GswTreeKey *self, const gchar *name,
                                       GError **error);
/**
 * gsw_tree_key_append_sibling:
 * @self: a #GswTreeKey
 * @name: local name for the new node
 * @error: return location for a #GError, or %NULL
 *
 * Inserts a sibling after the current node and positions the key on it.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_tree_key_append_sibling(GswTreeKey *self, const gchar *name,
                                         GError **error);
/**
 * gsw_tree_key_set_local_name:
 * @self: a #GswTreeKey
 * @name: new local name
 * @error: return location for a #GError, or %NULL
 *
 * Renames the current node.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_tree_key_set_local_name(GswTreeKey *self, const gchar *name,
                                         GError **error);
/**
 * gsw_tree_key_remove_node:
 * @self: a #GswTreeKey
 * @error: return location for a #GError, or %NULL
 *
 * Removes the current node and its descendants, then positions the key on the
 * parent node.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean     gsw_tree_key_remove_node(GswTreeKey *self, GError **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswTreeKey, gsw_key_unref)

G_END_DECLS

#endif /* __GSW_TREE_KEY_H__ */
