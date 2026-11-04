/*
 * gsw-key-private.h: internal key layout
 */

#ifndef __GSW_KEY_PRIVATE_H__
#define __GSW_KEY_PRIVATE_H__ 1

#include <gsword/gsw-key.h>
#include <gsword/gsw-enums.h>
#include <gsword/gsw-versification.h>

struct _GswKey {
    gatomicrefcount ref_count;
    GswKeyKind kind;
    gchar *text;
    gchar *locale;
    gboolean valid;

    /* verse fields */
    GswVersification *v11n;
    gint testament;
    gint book;
    gint chapter;
    gint verse;
    gchar suffix;
    gboolean intros;
    gboolean autonorm;
    gboolean has_lower;
    gboolean has_upper;
    gint lower_testament, lower_book, lower_chapter, lower_verse;
    gint upper_testament, upper_book, upper_chapter, upper_verse;

    /* tree fields */
    gchar *tree_path;       /* module base path, without extension */
    glong tree_offset;      /* current node offset in .idx */
    gint tree_parent;
    gint tree_next;
    gint tree_first_child;
    gchar *tree_name;
    gchar *tree_user_data;
    gint tree_user_size;
};

GswKey *gsw_key_alloc_internal(GswKeyKind kind);
void    gsw_key_set_text_cache(GswKey *self, const gchar *text);
void    gsw_key_set_locale(GswKey *self, const gchar *locale);
const gchar *gsw_key_get_locale(GswKey *self);

/* Implemented in gsw-tree-key.c. */
gboolean gsw_tree_key_set_text_internal(GswKey *self, const gchar *path);
gboolean gsw_tree_key_increment(GswKey *self, gint steps);
gboolean gsw_tree_key_first_child(GswKey *self);
gboolean gsw_tree_key_next_sibling(GswKey *self);
gboolean gsw_tree_key_parent(GswKey *self);
gboolean gsw_tree_key_has_children(GswKey *self);
gint     gsw_tree_key_get_level(GswKey *self);
gboolean gsw_tree_key_set_position(GswKey *self, GswPosition pos);
const gchar *gsw_tree_key_get_user_data(GswKey *self, gint *size);
gboolean gsw_tree_key_set_user_data(GswKey *self, const guint8 *data,
                                    gint size);
gboolean gsw_tree_key_recover_locked(GswKey *self, GError **error);

#endif /* __GSW_KEY_PRIVATE_H__ */
