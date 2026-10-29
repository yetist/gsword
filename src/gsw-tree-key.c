/*
 * gsw-tree-key.c: TreeKeyIdx key for RawGenBook modules
 */

#include <gsword/gsw-tree-key.h>
#include <gsword/gsw-str-key.h>
#include <gsword/gsw-error.h>
#include "gsw-key-private.h"
#include "gsw-data.h"
#include "gsw-lock.h"

#include <errno.h>
#include <glib/gstdio.h>
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

static gint tree_lock(GswKey *self, GError **error);
gboolean gsw_tree_key_recover_locked(GswKey *self, GError **error);

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

static void
tree_append_u32(GByteArray *array, guint32 value)
{
    guint8 bytes[4] = {
        (guint8)(value & 0xff), (guint8)((value >> 8) & 0xff),
        (guint8)((value >> 16) & 0xff), (guint8)((value >> 24) & 0xff)
    };

    g_byte_array_append(array, bytes, 4);
}

static void
tree_append_u16(GByteArray *array, guint16 value)
{
    guint8 bytes[2] = { (guint8)(value & 0xff), (guint8)(value >> 8) };

    g_byte_array_append(array, bytes, 2);
}

static void
tree_nodes_free(GPtrArray *nodes)
{
    guint i;

    if (nodes == NULL)
        return;
    for (i = 0; i < nodes->len; i++) {
        GswTreeNode *node = g_ptr_array_index(nodes, i);

        gsw_tree_node_clear(node);
        g_free(node);
    }
    g_ptr_array_unref(nodes);
}

static gboolean
tree_load_nodes(GswKey *self, GPtrArray **out, GError **error)
{
    gchar *idx_path;
    GStatBuf st;
    GPtrArray *nodes;
    glong count;
    glong i;

    *out = NULL;
    idx_path = g_strconcat(self->tree_path, ".idx", NULL);
    if (g_stat(idx_path, &st) != 0 || st.st_size <= 0 ||
        st.st_size % 4 != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Invalid general book index %s", idx_path);
        g_free(idx_path);
        return FALSE;
    }
    g_free(idx_path);
    count = (glong)(st.st_size / 4);
    nodes = g_ptr_array_new();

    for (i = 0; i < count; i++) {
        GswTreeNode *node = g_new0(GswTreeNode, 1);

        if (!gsw_tree_read_node(self->tree_path, i * 4, node, error)) {
            gsw_tree_node_clear(node);
            g_free(node);
            tree_nodes_free(nodes);
            return FALSE;
        }
        g_ptr_array_add(nodes, node);
    }

    *out = nodes;
    return TRUE;
}

static gint
tree_map_offset(GArray *remap, gint offset)
{
    if (offset < 0)
        return -1;
    if (remap == NULL)
        return offset;
    if ((guint)(offset / 4) >= remap->len)
        return -1;
    return g_array_index(remap, gint, offset / 4);
}

static gboolean
tree_restore_backups(const gchar *dat_path, const gchar *idx_path,
                     const gchar *dat_bak, const gchar *idx_bak)
{
    gboolean dat_ok = TRUE;
    gboolean idx_ok = TRUE;

    if (g_file_test(dat_bak, G_FILE_TEST_EXISTS))
        dat_ok = g_rename(dat_bak, dat_path) == 0;
    if (g_file_test(idx_bak, G_FILE_TEST_EXISTS))
        idx_ok = g_rename(idx_bak, idx_path) == 0;
    return dat_ok && idx_ok &&
           g_file_test(dat_path, G_FILE_TEST_EXISTS) &&
           g_file_test(idx_path, G_FILE_TEST_EXISTS);
}

static gboolean
tree_write_nodes(GswKey *self, GPtrArray *nodes, GArray *remap,
                 GError **error)
{
    GByteArray *dat = g_byte_array_new();
    GByteArray *idx = g_byte_array_new();
    GArray *dat_offsets = g_array_new(FALSE, FALSE, sizeof(guint32));
    gchar *dat_path = g_strconcat(self->tree_path, ".dat", NULL);
    gchar *idx_path = g_strconcat(self->tree_path, ".idx", NULL);
    gchar *dat_new = g_strconcat(self->tree_path, ".dat.new", NULL);
    gchar *idx_new = g_strconcat(self->tree_path, ".idx.new", NULL);
    gchar *dat_bak = g_strconcat(self->tree_path, ".dat.bak", NULL);
    gchar *idx_bak = g_strconcat(self->tree_path, ".idx.bak", NULL);
    gchar *txn_path = g_strconcat(self->tree_path, ".txn", NULL);
    guint i;
    gboolean ok = FALSE;

    for (i = 0; i < nodes->len; i++) {
        GswTreeNode *node = g_ptr_array_index(nodes, i);
        guint32 dat_offset = (guint32)dat->len;

        tree_append_u32(dat, (guint32)tree_map_offset(remap, node->parent));
        tree_append_u32(dat, (guint32)tree_map_offset(remap, node->next));
        tree_append_u32(dat, (guint32)tree_map_offset(remap, node->first_child));
        g_byte_array_append(dat, (const guint8 *)node->name,
                            (guint)strlen(node->name) + 1);
        tree_append_u16(dat, (guint16)node->user_size);
        if (node->user_size > 0)
            g_byte_array_append(dat, (const guint8 *)node->user_data,
                                (guint)node->user_size);
        g_array_append_val(dat_offsets, dat_offset);
    }
    for (i = 0; i < dat_offsets->len; i++) {
        guint32 offset = g_array_index(dat_offsets, guint32, i);

        tree_append_u32(idx, offset);
    }

    if (!g_file_set_contents(dat_new, (const gchar *)dat->data,
                             (gssize)dat->len, error) ||
        !g_file_set_contents(idx_new, (const gchar *)idx->data,
                             (gssize)idx->len, error))
        goto out;

    if (!g_file_set_contents(txn_path, "gsword-tree-txn-1\n", -1, error)) {
        g_unlink(txn_path);
        goto out;
    }

    if (g_rename(dat_path, dat_bak) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot back up %s: %s", dat_path, g_strerror(errno));
        if (tree_restore_backups(dat_path, idx_path, dat_bak, idx_bak))
            g_unlink(txn_path);
        goto out;
    }
    if (g_rename(idx_path, idx_bak) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot back up %s: %s", idx_path, g_strerror(errno));
        if (tree_restore_backups(dat_path, idx_path, dat_bak, idx_bak))
            g_unlink(txn_path);
        goto out;
    }
    if (g_rename(dat_new, dat_path) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot replace %s: %s", dat_path, g_strerror(errno));
        if (tree_restore_backups(dat_path, idx_path, dat_bak, idx_bak))
            g_unlink(txn_path);
        goto out;
    }
    if (g_rename(idx_new, idx_path) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot replace %s: %s", idx_path, g_strerror(errno));
        if (tree_restore_backups(dat_path, idx_path, dat_bak, idx_bak))
            g_unlink(txn_path);
        goto out;
    }

    g_unlink(txn_path);
    g_unlink(dat_bak);
    g_unlink(idx_bak);
    ok = TRUE;

out:
    if (!g_file_test(txn_path, G_FILE_TEST_EXISTS)) {
        g_unlink(dat_new);
        g_unlink(idx_new);
    }
    g_free(dat_path);
    g_free(idx_path);
    g_free(dat_new);
    g_free(idx_new);
    g_free(dat_bak);
    g_free(idx_bak);
    g_free(txn_path);
    g_byte_array_unref(dat);
    g_byte_array_unref(idx);
    g_array_unref(dat_offsets);
    return ok;
}

gboolean
gsw_tree_key_recover_locked(GswKey *self, GError **error)
{
    gchar *dat_path = g_strconcat(self->tree_path, ".dat", NULL);
    gchar *idx_path = g_strconcat(self->tree_path, ".idx", NULL);
    gchar *dat_new = g_strconcat(self->tree_path, ".dat.new", NULL);
    gchar *idx_new = g_strconcat(self->tree_path, ".idx.new", NULL);
    gchar *dat_bak = g_strconcat(self->tree_path, ".dat.bak", NULL);
    gchar *idx_bak = g_strconcat(self->tree_path, ".idx.bak", NULL);
    gchar *txn_path = g_strconcat(self->tree_path, ".txn", NULL);
    gboolean ok = TRUE;

    if (g_file_test(txn_path, G_FILE_TEST_EXISTS)) {
        gboolean dat_new_exists = g_file_test(dat_new, G_FILE_TEST_EXISTS);
        gboolean idx_new_exists = g_file_test(idx_new, G_FILE_TEST_EXISTS);

        if (dat_new_exists && idx_new_exists) {
            if (g_rename(dat_new, dat_path) != 0 ||
                g_rename(idx_new, idx_path) != 0) {
                g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                            "Cannot recover general book transaction: %s",
                            g_strerror(errno));
                ok = FALSE;
                goto out;
            }
        } else if (dat_new_exists || idx_new_exists) {
            if (g_file_test(dat_bak, G_FILE_TEST_EXISTS) ||
                g_file_test(idx_bak, G_FILE_TEST_EXISTS)) {
                if (!tree_restore_backups(dat_path, idx_path,
                                          dat_bak, idx_bak)) {
                    g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                                "Cannot recover general book transaction");
                    ok = FALSE;
                    goto out;
                }
            } else {
                if (dat_new_exists &&
                    g_rename(dat_new, dat_path) != 0) {
                    g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                                "Cannot recover %s: %s", dat_path,
                                g_strerror(errno));
                    ok = FALSE;
                    goto out;
                }
                if (idx_new_exists &&
                    g_rename(idx_new, idx_path) != 0) {
                    g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                                "Cannot recover %s: %s", idx_path,
                                g_strerror(errno));
                    ok = FALSE;
                    goto out;
                }
            }
        }

        if (!g_file_test(dat_path, G_FILE_TEST_EXISTS) ||
            !g_file_test(idx_path, G_FILE_TEST_EXISTS)) {
            if (!tree_restore_backups(dat_path, idx_path, dat_bak, idx_bak)) {
                g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                            "Cannot recover general book transaction");
                ok = FALSE;
                goto out;
            }
        }
        g_unlink(txn_path);
        g_unlink(dat_bak);
        g_unlink(idx_bak);
        g_unlink(dat_new);
        g_unlink(idx_new);
    } else {
        g_unlink(dat_new);
        g_unlink(idx_new);
        if (!g_file_test(dat_path, G_FILE_TEST_EXISTS)) {
            if (g_file_test(dat_bak, G_FILE_TEST_EXISTS))
                g_rename(dat_bak, dat_path);
        } else {
            g_unlink(dat_bak);
        }
        if (!g_file_test(idx_path, G_FILE_TEST_EXISTS)) {
            if (g_file_test(idx_bak, G_FILE_TEST_EXISTS))
                g_rename(idx_bak, idx_path);
        } else {
            g_unlink(idx_bak);
        }
    }

out:
    g_free(dat_path);
    g_free(idx_path);
    g_free(dat_new);
    g_free(idx_new);
    g_free(dat_bak);
    g_free(idx_bak);
    g_free(txn_path);
    return ok;
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
    gint lock_fd;

    g_return_val_if_fail(base_path != NULL, NULL);

    self = gsw_key_alloc_internal(GSW_KEY_KIND_TREE);
    self->tree_path = g_strdup(base_path);
    self->tree_offset = 0;
    lock_fd = tree_lock(self, &error);
    if (lock_fd >= 0) {
        if (!gsw_tree_key_recover_locked(self, &error))
            g_clear_error(&error);
        if (!gsw_tree_key_load(self, 0, &error)) {
            g_clear_error(&error);
            self->valid = FALSE;
        } else {
            self->valid = TRUE;
        }
        gsw_file_unlock(lock_fd);
    } else {
        g_clear_error(&error);
        if (!gsw_tree_key_load(self, 0, &error)) {
            g_clear_error(&error);
            self->valid = FALSE;
        } else {
            self->valid = TRUE;
        }
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

glong
gsw_tree_key_get_offset(GswTreeKey *self)
{
    return self != NULL ? self->tree_offset : 0;
}

gboolean
gsw_tree_key_set_offset(GswTreeKey *self, glong offset)
{
    GError *error = NULL;

    g_return_val_if_fail(self != NULL, FALSE);
    if (offset < 0 || !gsw_tree_key_load((GswKey *)self, offset, &error)) {
        g_clear_error(&error);
        return FALSE;
    }
    self->valid = TRUE;
    gsw_tree_key_update_text((GswKey *)self);
    return TRUE;
}

const gchar *
gsw_tree_key_get_local_name(GswTreeKey *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->tree_name != NULL ? self->tree_name : "";
}

gboolean
gsw_tree_key_previous_sibling(GswTreeKey *self)
{
    GswTreeNode parent;
    gint child;
    gint previous = -1;

    g_return_val_if_fail(self != NULL, FALSE);
    if (self->tree_parent < 0 || self->tree_offset <= 0)
        return FALSE;

    if (!gsw_tree_read_node(self->tree_path, self->tree_parent, &parent, NULL))
        return FALSE;

    child = parent.first_child;
    while (child >= 0) {
        GswTreeNode node;

        if (child == self->tree_offset) {
            gsw_tree_node_clear(&parent);
            if (previous < 0)
                return FALSE;
            if (!gsw_tree_key_load((GswKey *)self, previous, NULL))
                return FALSE;
            gsw_tree_key_update_text((GswKey *)self);
            return TRUE;
        }

        if (!gsw_tree_read_node(self->tree_path, child, &node, NULL))
            break;
        previous = child;
        child = node.next;
        gsw_tree_node_clear(&node);
    }

    gsw_tree_node_clear(&parent);
    return FALSE;
}

static gint
tree_lock(GswKey *self, GError **error)
{
    gchar *path = g_strconcat(self->tree_path, ".lock", NULL);
    gint fd = gsw_file_lock(path, error);

    g_free(path);
    return fd;
}

static gboolean
tree_append_node(GswKey *self, const gchar *name, gboolean sibling,
                 GError **error)
{
    GPtrArray *nodes = NULL;
    GswTreeNode *current;
    GswTreeNode *node;
    glong current_index;
    glong new_offset;
    gint lock_fd;
    gboolean ok = FALSE;

    if (*name == '\0') {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Tree node name must not be empty");
        return FALSE;
    }
    lock_fd = tree_lock(self, error);
    if (lock_fd < 0)
        return FALSE;
    if (!gsw_tree_key_recover_locked(self, error))
        goto out;
    if (!tree_load_nodes(self, &nodes, error))
        goto out;

    current_index = self->tree_offset / 4;
    if (self->tree_offset < 0 || self->tree_offset % 4 != 0 ||
        (guint)current_index >= nodes->len) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Current tree node is invalid");
        goto out;
    }
    current = g_ptr_array_index(nodes, current_index);
    if (sibling && current->parent < 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "The root node cannot have a sibling");
        goto out;
    }

    new_offset = (glong)nodes->len * 4;
    node = g_new0(GswTreeNode, 1);
    node->parent = sibling ? current->parent : self->tree_offset;
    node->next = sibling ? current->next : -1;
    node->first_child = -1;
    node->name = g_strdup(name);
    node->user_size = 8;
    node->user_data = g_malloc0(8);

    if (sibling) {
        current->next = (gint)new_offset;
    } else if (current->first_child < 0) {
        current->first_child = (gint)new_offset;
    } else {
        GswTreeNode *last;
        gint child = current->first_child;

        while (child >= 0) {
            last = g_ptr_array_index(nodes, child / 4);
            if (last->next < 0)
                break;
            child = last->next;
        }
        last->next = (gint)new_offset;
    }

    g_ptr_array_add(nodes, node);
    if (!tree_write_nodes(self, nodes, NULL, error))
        goto out;
    if (!gsw_tree_key_load(self, new_offset, error))
        goto out;
    self->valid = TRUE;
    gsw_tree_key_update_text(self);
    ok = TRUE;

out:
    tree_nodes_free(nodes);
    gsw_file_unlock(lock_fd);
    return ok;
}

gboolean
gsw_tree_key_append_child(GswTreeKey *self, const gchar *name, GError **error)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(name != NULL, FALSE);
    return tree_append_node((GswKey *)self, name, FALSE, error);
}

gboolean
gsw_tree_key_append_sibling(GswTreeKey *self, const gchar *name, GError **error)
{
    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(name != NULL, FALSE);
    return tree_append_node((GswKey *)self, name, TRUE, error);
}

gboolean
gsw_tree_key_set_local_name(GswTreeKey *self, const gchar *name, GError **error)
{
    GPtrArray *nodes = NULL;
    GswTreeNode *node;
    glong index;
    gint lock_fd;
    gboolean ok = FALSE;

    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(name != NULL, FALSE);
    if (*name == '\0') {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Tree node name must not be empty");
        return FALSE;
    }
    lock_fd = tree_lock((GswKey *)self, error);
    if (lock_fd < 0)
        return FALSE;
    if (!gsw_tree_key_recover_locked((GswKey *)self, error))
        goto out;
    if (!tree_load_nodes((GswKey *)self, &nodes, error))
        goto out;

    index = self->tree_offset / 4;
    if (self->tree_offset < 0 || self->tree_offset % 4 != 0 ||
        (guint)index >= nodes->len) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Current tree node is invalid");
        goto out;
    }
    node = g_ptr_array_index(nodes, index);
    g_free(node->name);
    node->name = g_strdup(name);
    if (!tree_write_nodes((GswKey *)self, nodes, NULL, error))
        goto out;
    if (!gsw_tree_key_load((GswKey *)self, self->tree_offset, error))
        goto out;
    self->valid = TRUE;
    gsw_tree_key_update_text((GswKey *)self);
    ok = TRUE;

out:
    tree_nodes_free(nodes);
    gsw_file_unlock(lock_fd);
    return ok;
}

static void
tree_mark_subtree(GPtrArray *nodes, gboolean *mark, gint offset)
{
    GswTreeNode *node;
    gint child;

    if (offset < 0 || offset % 4 != 0 || (guint)(offset / 4) >= nodes->len ||
        mark[offset / 4])
        return;
    mark[offset / 4] = TRUE;
    node = g_ptr_array_index(nodes, offset / 4);
    child = node->first_child;
    while (child >= 0) {
        GswTreeNode *child_node = g_ptr_array_index(nodes, child / 4);

        tree_mark_subtree(nodes, mark, child);
        child = child_node->next;
    }
}

gboolean
gsw_tree_key_remove_node(GswTreeKey *self, GError **error)
{
    GPtrArray *nodes = NULL;
    GPtrArray *kept = NULL;
    GArray *remap = NULL;
    GswTreeNode *current;
    GswTreeNode *parent;
    gboolean *marked = NULL;
    glong current_index;
    glong new_parent = 0;
    gint lock_fd;
    guint i;
    gboolean ok = FALSE;

    g_return_val_if_fail(self != NULL, FALSE);
    if (self->tree_offset <= 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "The root node cannot be removed");
        return FALSE;
    }
    lock_fd = tree_lock((GswKey *)self, error);
    if (lock_fd < 0)
        return FALSE;
    if (!gsw_tree_key_recover_locked((GswKey *)self, error))
        goto out;
    if (!tree_load_nodes((GswKey *)self, &nodes, error))
        goto out;

    current_index = self->tree_offset / 4;
    if ((guint)current_index >= nodes->len) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_OUT_OF_BOUNDS,
                    "Current tree node is invalid");
        goto out;
    }
    current = g_ptr_array_index(nodes, current_index);
    marked = g_new0(gboolean, nodes->len);
    tree_mark_subtree(nodes, marked, self->tree_offset);

    if (current->parent >= 0) {
        parent = g_ptr_array_index(nodes, current->parent / 4);
        if (parent->first_child == self->tree_offset) {
            parent->first_child = current->next;
        } else {
            gint child = parent->first_child;

            while (child >= 0) {
                GswTreeNode *node = g_ptr_array_index(nodes, child / 4);

                if (node->next == self->tree_offset) {
                    node->next = current->next;
                    break;
                }
                child = node->next;
            }
        }
    }

    kept = g_ptr_array_new();
    remap = g_array_new(FALSE, FALSE, sizeof(gint));
    {
        gint missing = -1;

        for (i = 0; i < nodes->len; i++)
            g_array_append_val(remap, missing);
    }
    for (i = 0; i < nodes->len; i++) {
        if (marked[i])
            continue;
        g_array_index(remap, gint, i) = (gint)(kept->len * 4);
        g_ptr_array_add(kept, g_ptr_array_index(nodes, i));
    }

    if (!tree_write_nodes((GswKey *)self, kept, remap, error))
        goto out;
    if (current->parent >= 0)
        new_parent = g_array_index(remap, gint, current->parent / 4);
    if (!gsw_tree_key_load((GswKey *)self, new_parent, error))
        goto out;
    self->valid = TRUE;
    gsw_tree_key_update_text((GswKey *)self);
    ok = TRUE;

out:
    if (kept != NULL)
        g_ptr_array_free(kept, TRUE);
    if (remap != NULL)
        g_array_unref(remap);
    g_free(marked);
    tree_nodes_free(nodes);
    gsw_file_unlock(lock_fd);
    return ok;
}

gboolean
gsw_tree_key_set_position(GswKey *self, GswPosition pos)
{
    GError *error = NULL;
    gint lock_fd = tree_lock(self, &error);

    if (lock_fd >= 0) {
        if (!gsw_tree_key_recover_locked(self, &error))
            g_clear_error(&error);
        gsw_file_unlock(lock_fd);
    } else {
        g_clear_error(&error);
    }

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

gboolean
gsw_tree_key_set_user_data(GswKey *self, const guint8 *data, gint size)
{
    gchar *idx_path;
    gchar *dat_path;
    FILE *idx = NULL;
    FILE *dat = NULL;
    guint8 raw_offset[4];
    guint8 size_bytes[2];
    guint32 dat_offset;
    gint old_size;
    gint name_len = 0;
    gboolean ok = FALSE;

    if (self == NULL || self->kind != GSW_KEY_KIND_TREE ||
        data == NULL || size < 0 || size > 65535)
        return FALSE;

    idx_path = g_strconcat(self->tree_path, ".idx", NULL);
    dat_path = g_strconcat(self->tree_path, ".dat", NULL);
    idx = fopen(idx_path, "rb");
    if (idx == NULL)
        goto out;
    if (fseeko(idx, (off_t)self->tree_offset, SEEK_SET) != 0 ||
        fread(raw_offset, 1, 4, idx) != 4)
        goto out;
    dat_offset = gsw_le32(raw_offset);

    dat = fopen(dat_path, "r+b");
    if (dat == NULL)
        goto out;
    if (fseeko(dat, (off_t)dat_offset + 12, SEEK_SET) != 0)
        goto out;
    while (name_len <= 4096) {
        gint ch = fgetc(dat);

        if (ch == EOF)
            goto out;
        if (ch == 0)
            break;
        name_len++;
    }
    if (name_len > 4096 || fread(size_bytes, 1, 2, dat) != 2)
        goto out;
    old_size = (gint)gsw_le16(size_bytes);
    if (old_size != size)
        goto out;
    if (fwrite(data, 1, (gsize)size, dat) != (gsize)size ||
        fflush(dat) != 0)
        goto out;

    g_free(self->tree_user_data);
    self->tree_user_data = g_malloc((gsize)size);
    memcpy(self->tree_user_data, data, (gsize)size);
    self->tree_user_size = size;
    ok = TRUE;

out:
    if (idx != NULL)
        fclose(idx);
    if (dat != NULL)
        fclose(dat);
    g_free(idx_path);
    g_free(dat_path);
    return ok;
}
