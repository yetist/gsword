/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "gsw-cache.h"

static void
test_lru_eviction(void)
{
    GswBlockCache *cache = gsw_block_cache_new(2);
    GBytes *a = g_bytes_new_static("a", 1);
    GBytes *b = g_bytes_new_static("b", 1);
    GBytes *c = g_bytes_new_static("c", 1);
    GBytes *d = g_bytes_new_static("d", 1);

    gsw_block_cache_insert(cache, 1, a);
    gsw_block_cache_insert(cache, 2, b);
    g_assert_cmpuint(gsw_block_cache_get_size(cache), ==, 2);
    g_assert_true(gsw_block_cache_lookup(cache, 1) == a);

    /* 2 is now the least recently used entry. */
    gsw_block_cache_insert(cache, 3, c);
    g_assert_cmpuint(gsw_block_cache_get_size(cache), ==, 2);
    g_assert_null(gsw_block_cache_lookup(cache, 2));
    g_assert_true(gsw_block_cache_lookup(cache, 1) == a);
    g_assert_true(gsw_block_cache_lookup(cache, 3) == c);

    /* Replacing an existing key keeps the cache size stable. */
    gsw_block_cache_insert(cache, 1, d);
    g_assert_cmpuint(gsw_block_cache_get_size(cache), ==, 2);
    g_assert_true(gsw_block_cache_lookup(cache, 1) == d);
    g_assert_true(gsw_block_cache_lookup(cache, 3) == c);

    gsw_block_cache_free(cache);
}

static void
test_clear(void)
{
    GswBlockCache *cache = gsw_block_cache_new(4);

    gsw_block_cache_insert(cache, 10, g_bytes_new_static("x", 1));
    gsw_block_cache_insert(cache, 11, g_bytes_new_static("y", 1));
    g_assert_cmpuint(gsw_block_cache_get_size(cache), ==, 2);

    gsw_block_cache_clear(cache);
    g_assert_cmpuint(gsw_block_cache_get_size(cache), ==, 0);
    g_assert_null(gsw_block_cache_lookup(cache, 10));

    gsw_block_cache_free(cache);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/cache/lru-eviction", test_lru_eviction);
    g_test_add_func("/cache/clear", test_clear);
    return g_test_run();
}
