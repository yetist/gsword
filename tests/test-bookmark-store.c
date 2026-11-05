/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include "bookmark-store.h"
#include "gsw-utils.h"

#include <glib.h>
#include <glib/gstdio.h>

static void
test_crud(void)
{
    GError *error = NULL;
    GswordBookmarkStore *store = gsword_bookmark_store_new(&error);
    GswordBookmark *bookmark;
    GPtrArray *items;
    gint64 id;

    g_assert_no_error(error);
    g_assert_nonnull(store);

    id = gsword_bookmark_store_create(
        store, "Morning reading", "KJV", "John.3.16", "John 3:16",
        "Return here tomorrow", &error);
    g_assert_no_error(error);
    g_assert_cmpint(id, >, 0);

    bookmark = gsword_bookmark_store_get(store, id);
    g_assert_nonnull(bookmark);
    g_assert_cmpstr(bookmark->title, ==, "Morning reading");
    g_assert_cmpstr(bookmark->module, ==, "KJV");
    g_assert_cmpstr(bookmark->key, ==, "John.3.16");
    g_assert_cmpstr(bookmark->note, ==, "Return here tomorrow");
    g_assert_cmpint(bookmark->created, >, 0);
    gsword_bookmark_free(bookmark);

    g_assert_true(gsword_bookmark_store_update(
        store, id, "Evening reading", "Updated note", &error));
    g_assert_no_error(error);
    bookmark = gsword_bookmark_store_get(store, id);
    g_assert_nonnull(bookmark);
    g_assert_cmpstr(bookmark->title, ==, "Evening reading");
    g_assert_cmpstr(bookmark->note, ==, "Updated note");
    gsword_bookmark_free(bookmark);

    items = gsword_bookmark_store_list(store);
    g_assert_cmpuint(items->len, ==, 1);
    g_ptr_array_unref(items);

    g_assert_true(gsword_bookmark_store_delete(store, id, &error));
    g_assert_no_error(error);
    g_assert_null(gsword_bookmark_store_get(store, id));
    items = gsword_bookmark_store_list(store);
    g_assert_cmpuint(items->len, ==, 0);
    g_ptr_array_unref(items);
    gsword_bookmark_store_free(store);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    gchar *root;
    gchar *data_home;

    root = g_dir_make_tmp("gsword-bookmark-store-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(root);
    data_home = g_build_filename(root, "data", NULL);
    g_mkdir_with_parents(data_home, 0755);
    g_setenv("XDG_DATA_HOME", data_home, TRUE);

    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/bookmark-store/crud", test_crud);
    {
        int status = g_test_run();

        gsw_dir_remove_recursive(root, NULL);
        g_free(data_home);
        g_free(root);
        return status;
    }
}
