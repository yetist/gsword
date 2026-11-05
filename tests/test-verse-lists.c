/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include "notes-store.h"
#include "gsw-utils.h"

#include <glib.h>
#include <glib/gstdio.h>

static GswordVerseList *
find_list(GPtrArray *lists, const gchar *name)
{
    guint i;

    for (i = 0; i < lists->len; i++) {
        GswordVerseList *list = g_ptr_array_index(lists, i);

        if (g_strcmp0(list->name, name) == 0)
            return list;
    }
    return NULL;
}

static void
test_lists_and_favorites(void)
{
    GError *error = NULL;
    GswordNotesStore *store = gsword_notes_store_new(&error);
    GPtrArray *lists;
    GPtrArray *items;
    GswordVerseList *normal;
    GswordVerseList *favorites;
    gint64 favorites_id;
    gint64 normal_id;

    g_assert_no_error(error);
    g_assert_nonnull(store);

    favorites_id = gsword_notes_store_get_favorites_list_id(store);
    g_assert_cmpint(favorites_id, >, 0);
    g_assert_true(gsword_notes_store_list_is_favorites(store, favorites_id));

    g_assert_true(gsword_notes_store_create_list(store, "Study", &error));
    g_assert_no_error(error);
    lists = gsword_notes_store_list_lists(store);
    normal = find_list(lists, "Study");
    favorites = find_list(lists, "Favorites");
    g_assert_nonnull(normal);
    g_assert_nonnull(favorites);
    g_assert_cmpstr(normal->kind, ==, "normal");
    g_assert_cmpstr(favorites->kind, ==, "favorites");
    normal_id = normal->id;
    g_ptr_array_unref(lists);

    g_assert_true(gsword_notes_store_add_to_list(
        store, normal_id, "KJV", "John.3.16", "John 3:16", &error));
    g_assert_no_error(error);
    items = gsword_notes_store_list_items(store, normal_id);
    g_assert_cmpuint(items->len, ==, 1);
    g_ptr_array_unref(items);

    g_assert_false(gsword_notes_store_is_favorite(store, "KJV", "John.3.16"));
    g_assert_true(gsword_notes_store_add_favorite(
        store, "KJV", "John.3.16", "John 3:16", &error));
    g_assert_no_error(error);
    g_assert_true(gsword_notes_store_is_favorite(store, "KJV", "John.3.16"));
    items = gsword_notes_store_list_items(store, favorites_id);
    g_assert_cmpuint(items->len, ==, 1);
    g_ptr_array_unref(items);
    g_assert_true(gsword_notes_store_remove_favorite(
        store, "KJV", "John.3.16", &error));
    g_assert_no_error(error);
    g_assert_false(gsword_notes_store_is_favorite(store, "KJV", "John.3.16"));

    g_assert_true(gsword_notes_store_rename_list(store, normal_id,
                                                 "Study notes", &error));
    g_assert_no_error(error);
    g_assert_false(gsword_notes_store_rename_list(store, favorites_id,
                                                  "Nope", &error));
    g_assert_nonnull(error);
    g_clear_error(&error);
    g_assert_false(gsword_notes_store_delete_list(store, favorites_id,
                                                  &error));
    g_assert_nonnull(error);
    g_clear_error(&error);
    g_assert_true(gsword_notes_store_delete_list(store, normal_id, &error));
    g_assert_no_error(error);

    gsword_notes_store_free(store);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    gchar *root;
    gchar *data_home;

    root = g_dir_make_tmp("gsword-verse-lists-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(root);
    data_home = g_build_filename(root, "data", NULL);
    g_mkdir_with_parents(data_home, 0755);
    g_setenv("XDG_DATA_HOME", data_home, TRUE);

    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/verse-lists/favorites", test_lists_and_favorites);
    {
        int status = g_test_run();

        gsw_dir_remove_recursive(root, NULL);
        g_free(data_home);
        g_free(root);
        return status;
    }
}
