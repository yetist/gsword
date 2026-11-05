/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include "insight-store.h"
#include "gsw-utils.h"

#include <glib.h>
#include <glib/gstdio.h>

static GPtrArray *
make_verses(void)
{
    GPtrArray *verses = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_insight_verse_free);

    g_ptr_array_add(verses,
                    gsword_insight_verse_new("KJV", "John.3.16",
                                             "John 3:16"));
    return verses;
}

static GPtrArray *
make_tags(void)
{
    GPtrArray *tags = g_ptr_array_new_with_free_func(g_free);

    g_ptr_array_add(tags, g_strdup("grace"));
    g_ptr_array_add(tags, g_strdup(" salvation "));
    g_ptr_array_add(tags, g_strdup("grace"));
    g_ptr_array_add(tags, g_strdup(""));
    return tags;
}

static void
test_crud(void)
{
    GError *error = NULL;
    GswordInsightStore *store = gsword_insight_store_new(&error);
    GswordInsight *insight;
    GPtrArray *items;
    GPtrArray *verses;
    GPtrArray *tags;
    gint64 id;

    g_assert_no_error(error);
    g_assert_nonnull(store);

    verses = make_verses();
    tags = make_tags();
    id = gsword_insight_store_create(store, "Grace", "Grace upon grace",
                                     verses, tags, &error);
    g_assert_no_error(error);
    g_assert_cmpint(id, >, 0);
    g_ptr_array_unref(verses);
    g_ptr_array_unref(tags);

    insight = gsword_insight_store_get(store, id);
    g_assert_nonnull(insight);
    g_assert_cmpstr(insight->title, ==, "Grace");
    g_assert_cmpstr(insight->body, ==, "Grace upon grace");
    g_assert_cmpuint(insight->verses->len, ==, 1);
    g_assert_cmpuint(insight->tags->len, ==, 2);
    gsword_insight_free(insight);

    verses = make_verses();
    tags = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(tags, g_strdup("grace"));
    g_assert_true(gsword_insight_store_update(
        store, id, "Grace and peace", "Updated body", verses, tags, &error));
    g_assert_no_error(error);
    g_ptr_array_unref(verses);
    g_ptr_array_unref(tags);

    insight = gsword_insight_store_get(store, id);
    g_assert_nonnull(insight);
    g_assert_cmpstr(insight->title, ==, "Grace and peace");
    g_assert_cmpstr(insight->body, ==, "Updated body");
    g_assert_cmpuint(insight->tags->len, ==, 1);
    gsword_insight_free(insight);

    items = gsword_insight_store_list(store, NULL, NULL);
    g_assert_cmpuint(items->len, ==, 1);
    g_ptr_array_unref(items);

    items = gsword_insight_store_list(store, "Updated", NULL);
    g_assert_cmpuint(items->len, ==, 1);
    g_ptr_array_unref(items);

    items = gsword_insight_store_list(store, NULL, "grace");
    g_assert_cmpuint(items->len, ==, 1);
    g_ptr_array_unref(items);

    items = gsword_insight_store_list(store, NULL, "missing");
    g_assert_cmpuint(items->len, ==, 0);
    g_ptr_array_unref(items);

    tags = gsword_insight_store_list_tags(store);
    g_assert_cmpuint(tags->len, ==, 1);
    g_assert_cmpstr(g_ptr_array_index(tags, 0), ==, "grace");
    g_ptr_array_unref(tags);

    g_assert_true(gsword_insight_store_delete(store, id, &error));
    g_assert_no_error(error);
    g_assert_null(gsword_insight_store_get(store, id));
    items = gsword_insight_store_list(store, NULL, NULL);
    g_assert_cmpuint(items->len, ==, 0);
    g_ptr_array_unref(items);
    gsword_insight_store_free(store);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    gchar *root;
    gchar *data_home;

    root = g_dir_make_tmp("gsword-insight-store-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(root);
    data_home = g_build_filename(root, "data", NULL);
    g_mkdir_with_parents(data_home, 0755);
    g_setenv("XDG_DATA_HOME", data_home, TRUE);

    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/insight-store/crud", test_crud);
    {
        int status = g_test_run();

        gsw_dir_remove_recursive(root, NULL);
        g_free(data_home);
        g_free(root);
        return status;
    }
}
