/*
 * gsw-cache.c: LRU cache for decompressed module blocks
 */

#include "gsw-cache.h"

typedef struct {
    guint64 key;
    GBytes *data;
} GswBlockCacheEntry;

struct _GswBlockCache {
    guint capacity;
    GHashTable *map;
    GQueue queue;
};

static guint64 *
block_key_new(guint64 key)
{
    guint64 *copy = g_new(guint64, 1);

    *copy = key;
    return copy;
}

static void
block_entry_free(GswBlockCacheEntry *entry)
{
    if (entry == NULL)
        return;
    g_clear_pointer(&entry->data, g_bytes_unref);
    g_free(entry);
}

GswBlockCache *
gsw_block_cache_new(guint capacity)
{
    GswBlockCache *self;

    g_return_val_if_fail(capacity > 0, NULL);

    self = g_new0(GswBlockCache, 1);
    self->capacity = capacity;
    self->map = g_hash_table_new_full(g_int64_hash, g_int64_equal, g_free,
                                      NULL);
    g_queue_init(&self->queue);
    return self;
}

void
gsw_block_cache_clear(GswBlockCache *self)
{
    g_return_if_fail(self != NULL);

    g_hash_table_remove_all(self->map);
    while (!g_queue_is_empty(&self->queue))
        block_entry_free(g_queue_pop_head(&self->queue));
}

void
gsw_block_cache_free(GswBlockCache *self)
{
    if (self == NULL)
        return;
    gsw_block_cache_clear(self);
    g_hash_table_unref(self->map);
    g_free(self);
}

GBytes *
gsw_block_cache_lookup(GswBlockCache *self, guint64 key)
{
    GList *link;
    GswBlockCacheEntry *entry;

    g_return_val_if_fail(self != NULL, NULL);

    link = g_hash_table_lookup(self->map, &key);
    if (link == NULL)
        return NULL;

    entry = link->data;
    g_queue_unlink(&self->queue, link);
    g_queue_push_head_link(&self->queue, link);
    return entry->data;
}

void
gsw_block_cache_insert(GswBlockCache *self, guint64 key, GBytes *data)
{
    GList *link;
    GswBlockCacheEntry *entry;

    g_return_if_fail(self != NULL);
    g_return_if_fail(data != NULL);

    link = g_hash_table_lookup(self->map, &key);
    if (link != NULL) {
        entry = link->data;
        g_bytes_unref(entry->data);
        entry->data = data;
        g_queue_unlink(&self->queue, link);
        g_queue_push_head_link(&self->queue, link);
        return;
    }

    entry = g_new0(GswBlockCacheEntry, 1);
    entry->key = key;
    entry->data = data;
    g_queue_push_head(&self->queue, entry);
    link = g_queue_peek_head_link(&self->queue);
    g_hash_table_insert(self->map, block_key_new(key), link);

    while (g_queue_get_length(&self->queue) > self->capacity) {
        GList *tail = g_queue_peek_tail_link(&self->queue);

        entry = tail->data;
        g_hash_table_remove(self->map, &entry->key);
        g_queue_delete_link(&self->queue, tail);
        block_entry_free(entry);
    }
}

guint
gsw_block_cache_get_size(GswBlockCache *self)
{
    g_return_val_if_fail(self != NULL, 0);

    return g_queue_get_length(&self->queue);
}
