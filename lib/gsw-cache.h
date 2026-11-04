/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-cache.h: LRU cache for decompressed module blocks
 */

#ifndef __GSW_CACHE_H__
#define __GSW_CACHE_H__ 1

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GswBlockCache GswBlockCache;

GswBlockCache *gsw_block_cache_new(guint capacity);
void           gsw_block_cache_free(GswBlockCache *self);
void           gsw_block_cache_clear(GswBlockCache *self);
GBytes        *gsw_block_cache_lookup(GswBlockCache *self, guint64 key);
void           gsw_block_cache_insert(GswBlockCache *self, guint64 key,
                                      GBytes *data);
guint          gsw_block_cache_get_size(GswBlockCache *self);

G_END_DECLS

#endif /* __GSW_CACHE_H__ */
