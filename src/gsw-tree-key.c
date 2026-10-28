/*
 * gsw-tree-key.c: TreeKeyIdx key for RawGenBook modules
 */

#include <gsword/gsw-tree-key.h>
#include <gsword/gsw-str-key.h>
#include <gsword/gsw-error.h>
#include "gsw-key-private.h"
#include "gsw-data.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    gint parent;
    gint next;
    gint first_child;
    gchar *name;
    gchar *user_data;
    gint user_size;
} GswTreeNode;

static gboolean
gsw_tree_read_node(const gchar *base, glong offset, GswTreeNode *node,
                   GError **error)
{
    gchar *idx_path = g_strconcat(base, ".idx", NULL);
    gchar *dat_path = g_strconcat(base, ".dat", NULL);
    FILE *idx = fopen(idx_path, "rb");
    FILE *dat = NULL;
    guint8 raw[12];
    guint32 dat_offset;
    GString *name;
    gint ch;
    guint8 size_bytes[2];
    gboolean ok = FALSE;

    g_free(idx_path);
    g_free(dat_path);

    memset(node, 0, sizeof(*node));
    node->parent = node->next = node->first_child = -1;

    if (idx == NULL)
        goto out;
    if (!gsw_file_read_at(idx, offset, raw, 4, error))
        goto out;
    dat_offset = gsw_le32(raw);

    dat_path = g_strconcat(base, ".dat", NULL);
    dat = fopen(dat_path, "rb");
    g_free(dat_path);
    if (dat == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                    "Cannot open tree data file");
        goto out;
    }

    if (!gsw_file_read_at(dat, dat_offset, raw, 12, error))
        goto out;
    node->parent = (gint)gsw_le32(raw);
    node->next = (gint)gsw_le32(raw + 4);
    node->first_child = (gint)gsw_le32(raw + 8);

    name = g_string_new(NULL);
    while ((ch = fgetc(dat)) != EOF && ch != 0) {
        if (name->len > 4096) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Tree node name is too long");
            g_string_free(name, TRUE);
            goto out;
        }
        g_string_append_c(name, (gchar)ch);
    }
    if (ch == EOF) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Truncated tree node name");
        g_string_free(name, TRUE);
        goto out;
    }
    node->name = g_string_free(name, FALSE);

    if (fread(size_bytes, 1, 2, dat) != 2) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Truncated tree node metadata");
        goto out;
    }
    node->user_size = gsw_le16(size_bytes);
    if (node->user_size > 4096) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Tree node user data is too large");
        goto out;
    }
    if (node->user_size > 0) {
        node->user_data = g_malloc((gsize)node->user_size);
        if (fread(node->user_data, 1, (gsize)node->user_size, dat) !=
            (gsize)node->user_size) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                        "Truncated tree node user data");
            goto out;
        }
    }

    ok = TRUE;

out:
    if (idx != NULL)
        fclose(idx);
    if (dat != NULL)
        fclose(dat);
    if (!ok) {
        g_free(node->name);
        g_free(node->user_data);
        node->name = NULL;
        node->user_data = NULL;
    }
    return ok;
}

static void
gsw_tree_node_clear(GswTreeNode *node)
{
    g_free(node->name);
    g_free(node->user_data);
    memset(node, 0, sizeof(*node));
}

static gboolean
gsw_tree_key_load(GswKey *self, glong offset, GError **error)
{
    GswTreeNode node;

    if (!gsw_tree_read_node(self->tree_path, offset, &node, error))
        return FALSE;

    g_free(self->tree_name);
    g_free(self->tree_user_data);
    self->tree_offset = offset;
    self->tree_parent = node.parent;
    self->tree_next = node.next;
    self->tree_first_child = node.first_child;
    self->tree_name = node.name;
    self->tree_user_data = node.user_data;
    self->tree_user_size = node.user_size;
    return TRUE;
}

static void
gsw_tree_key_update_text(GswKey *self)
{
    GString *path = g_string_new(NULL);
    gint parent = self->tree_parent;

    if (self->tree_name != NULL && self->tree_name[0] != '\0')
        g_string_assign(path, self->tree_name);

    while (parent >= 0) {
        GswTreeNode node;

        if (!gsw_tree_read_node(self->tree_path, parent, &node, NULL)) {
            break;
        }
        if (node.name != NULL && node.name[0] != '\0') {
            if (path->len > 0)
                g_string_prepend_c(path, '/');
            g_string_prepend(path, node.name);
        }
        parent = node.parent;
        gsw_tree_node_clear(&node);
    }

    gsw_key_set_text_cache(self, path->str);
    g_string_free(path, TRUE);
}

GType
gsw_tree_key_get_type(void)
{
    static gsize type = 0;

    if (g_once_init_enter(&type)) {
        GType registered = g_boxed_type_register_static(
            "GswTreeKey", (GBoxedCopyFunc)gsw_key_clone,
            (GBoxedFreeFunc)gsw_key_unref);
        g_once_init_leave(&type, registered);
    }
    return (GType)type;
}

GswTreeKey *
gsw_tree_key_new(const gchar *base_path)
{
    GswKey *self;
    GError *error = NULL;

    g_return_val_if_fail(base_path != NULL, NULL);

    self = gsw_key_alloc_internal(GSW_KEY_KIND_TREE);
    self->tree_path = g_strdup(base_path);
    self->tree_offset = 0;
    if (!gsw_tree_key_load(self, 0, &error)) {
        g_clear_error(&error);
        self->valid = FALSE;
    } else {
        self->valid = TRUE;
    }
    gsw_tree_key_update_text(self);
    return (GswTreeKey *)self;
}

const gchar *
gsw_tree_key_get_text(GswTreeKey *self)
{
    return gsw_key_get_text((GswKey *)self);
}

gboolean
gsw_tree_key_set_text(GswTreeKey *self, const gchar *path)
{
    return gsw_key_set_text((GswKey *)self, path);
}

gboolean
gsw_tree_key_set_text_internal(GswKey *self, const gchar *path)
{
    gchar **parts;
    guint i;
    gboolean valid = TRUE;
    GError *error = NULL;

    if (!gsw_tree_key_load(self, 0, &error)) {
        g_clear_error(&error);
        self->valid = FALSE;
        return FALSE;
    }

    parts = g_strsplit(path != NULL ? path : "", "/", -1);
    for (i = 0; parts[i] != NULL; i++) {
        gchar *part = g_strstrip(parts[i]);

        if (*part == '\0')
            continue;

        if (self->tree_first_child < 0) {
            valid = FALSE;
            break;
        }

        if (!gsw_tree_key_load(self, self->tree_first_child, &error)) {
            g_clear_error(&error);
            valid = FALSE;
            break;
        }

        while (g_strcmp0(self->tree_name, part) != 0) {
            if (self->tree_next < 0) {
                valid = FALSE;
                break;
            }
            if (!gsw_tree_key_load(self, self->tree_next, &error)) {
                g_clear_error(&error);
                valid = FALSE;
                break;
            }
        }
        if (!valid)
            break;
    }
    g_strfreev(parts);

    self->valid = valid;
    gsw_tree_key_update_text(self);
    return valid;
}

gboolean
gsw_tree_key_increment(GswKey *self, gint steps)
{
    GError *error = NULL;
    glong offset = self->tree_offset + (glong)steps * 4;

    if (offset < 0 || !gsw_tree_key_load(self, offset, &error)) {
        g_clear_error(&error);
        self->valid = FALSE;
        return FALSE;
    }
    self->valid = TRUE;
    gsw_tree_key_update_text(self);
    return TRUE;
}

gboolean
gsw_tree_key_first_child(GswKey *self)
{
    GError *error = NULL;

    if (self->tree_first_child < 0)
        return FALSE;
    if (!gsw_tree_key_load(self, self->tree_first_child, &error)) {
        g_clear_error(&error);
        return FALSE;
    }
    gsw_tree_key_update_text(self);
    return TRUE;
}

gboolean
gsw_tree_key_next_sibling(GswKey *self)
{
    GError *error = NULL;

    if (self->tree_next < 0)
        return FALSE;
    if (!gsw_tree_key_load(self, self->tree_next, &error)) {
        g_clear_error(&error);
        return FALSE;
    }
    gsw_tree_key_update_text(self);
    return TRUE;
}

gboolean
gsw_tree_key_parent(GswKey *self)
{
    GError *error = NULL;

    if (self->tree_parent < 0)
        return FALSE;
    if (!gsw_tree_key_load(self, self->tree_parent, &error)) {
        g_clear_error(&error);
        return FALSE;
    }
    gsw_tree_key_update_text(self);
    return TRUE;
}

gboolean
gsw_tree_key_has_children(GswKey *self)
{
    return self->tree_first_child >= 0;
}

gint
gsw_tree_key_get_level(GswKey *self)
{
    gint level = 0;
    gint parent = self->tree_parent;

    while (parent >= 0) {
        GswTreeNode node;
        if (!gsw_tree_read_node(self->tree_path, parent, &node, NULL))
            break;
        level++;
        parent = node.parent;
        gsw_tree_node_clear(&node);
    }
    return level;
}

gboolean
gsw_tree_key_set_position(GswKey *self, GswPosition pos)
{
    if (pos == GSW_POSITION_TOP)
        return gsw_tree_key_load(self, 0, NULL);

    {
        gchar *path = g_strconcat(self->tree_path, ".idx", NULL);
        FILE *idx = fopen(path, "rb");
        gint64 size;
        gboolean ok;

        g_free(path);
        if (idx == NULL)
            return FALSE;
        size = gsw_file_size(idx);
        fclose(idx);
        if (size < 4)
            return FALSE;
        ok = gsw_tree_key_load(self, (glong)(size - 4), NULL);
        if (ok)
            gsw_tree_key_update_text(self);
        return ok;
    }
}

const gchar *
gsw_tree_key_get_user_data(GswKey *self, gint *size)
{
    if (size != NULL)
        *size = self->tree_user_size;
    return self->tree_user_data;
}
