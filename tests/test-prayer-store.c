/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include "prayer-store.h"
#include "gsw-utils.h"

#include <glib.h>
#include <glib/gstdio.h>

static void
test_create_update_filter(void)
{
    GError *error = NULL;
    GswordPrayerStore *store = gsword_prayer_store_new(&error);
    GswordPrayer *prayer;
    GPtrArray *items;
    gint64 id;

    g_assert_no_error(error);
    g_assert_nonnull(store);

    id = gsword_prayer_store_create(store, "Family", "Pray for peace", &error);
    g_assert_no_error(error);
    g_assert_cmpint(id, >, 0);

    prayer = gsword_prayer_store_get(store, id);
    g_assert_nonnull(prayer);
    g_assert_cmpstr(prayer->title, ==, "Family");
    g_assert_cmpstr(prayer->body, ==, "Pray for peace");
    g_assert_cmpint(prayer->status, ==, GSWORD_PRAYER_STATUS_ACTIVE);
    g_assert_cmpuint(prayer->verses->len, ==, 0);
    gsword_prayer_free(prayer);

    g_assert_true(gsword_prayer_store_update(store, id, "Family and friends",
                                             "Pray for peace and health",
                                             &error));
    g_assert_no_error(error);
    prayer = gsword_prayer_store_get(store, id);
    g_assert_nonnull(prayer);
    g_assert_cmpstr(prayer->title, ==, "Family and friends");
    g_assert_cmpstr(prayer->body, ==, "Pray for peace and health");
    gsword_prayer_free(prayer);

    g_assert_cmpint(gsword_prayer_store_count(store,
                                              GSWORD_PRAYER_STATUS_ACTIVE),
                    ==, 1);
    items = gsword_prayer_store_list(store, GSWORD_PRAYER_STATUS_ACTIVE,
                                     NULL);
    g_assert_cmpuint(items->len, ==, 1);
    g_ptr_array_unref(items);

    items = gsword_prayer_store_list(store, GSWORD_PRAYER_STATUS_ANY,
                                     "health");
    g_assert_cmpuint(items->len, ==, 1);
    g_ptr_array_unref(items);

    items = gsword_prayer_store_list(store, GSWORD_PRAYER_STATUS_ANY,
                                     "missing");
    g_assert_cmpuint(items->len, ==, 0);
    g_ptr_array_unref(items);

    g_assert_true(gsword_prayer_store_delete(store, id, &error));
    g_assert_no_error(error);
    g_assert_null(gsword_prayer_store_get(store, id));
    gsword_prayer_store_free(store);
}

static void
test_status(void)
{
    GError *error = NULL;
    GswordPrayerStore *store = gsword_prayer_store_new(&error);
    GswordPrayer *prayer;
    gint64 id;

    g_assert_no_error(error);
    g_assert_nonnull(store);
    id = gsword_prayer_store_create(store, "Work", NULL, &error);
    g_assert_no_error(error);
    g_assert_cmpint(id, >, 0);

    g_assert_true(gsword_prayer_store_set_status(
        store, id, GSWORD_PRAYER_STATUS_ANSWERED, "Got the job", &error));
    g_assert_no_error(error);
    prayer = gsword_prayer_store_get(store, id);
    g_assert_nonnull(prayer);
    g_assert_cmpint(prayer->status, ==, GSWORD_PRAYER_STATUS_ANSWERED);
    g_assert_cmpint(prayer->answered_at, >, 0);
    g_assert_cmpstr(prayer->answered_note, ==, "Got the job");
    gsword_prayer_free(prayer);

    g_assert_true(gsword_prayer_store_set_status(
        store, id, GSWORD_PRAYER_STATUS_ARCHIVED, NULL, &error));
    g_assert_no_error(error);
    prayer = gsword_prayer_store_get(store, id);
    g_assert_nonnull(prayer);
    g_assert_cmpint(prayer->status, ==, GSWORD_PRAYER_STATUS_ARCHIVED);
    gsword_prayer_free(prayer);

    g_assert_true(gsword_prayer_store_set_status(
        store, id, GSWORD_PRAYER_STATUS_ACTIVE, NULL, &error));
    g_assert_no_error(error);
    prayer = gsword_prayer_store_get(store, id);
    g_assert_nonnull(prayer);
    g_assert_cmpint(prayer->status, ==, GSWORD_PRAYER_STATUS_ACTIVE);
    g_assert_cmpint(prayer->answered_at, ==, 0);
    g_assert_null(prayer->answered_note);
    gsword_prayer_free(prayer);

    gsword_prayer_store_free(store);
}

static void
test_entries(void)
{
    GError *error = NULL;
    GswordPrayerStore *store = gsword_prayer_store_new(&error);
    GPtrArray *entries;
    GswordPrayerEntry *entry;
    gint64 id;
    gint64 entry_id;

    g_assert_no_error(error);
    g_assert_nonnull(store);
    id = gsword_prayer_store_create(store, "Entries", "", &error);
    g_assert_no_error(error);
    g_assert_cmpint(id, >, 0);

    g_assert_cmpint(gsword_prayer_store_add_entry(store, id, "", &error), ==,
                    0);
    g_assert_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL);
    g_clear_error(&error);

    entry_id = gsword_prayer_store_add_entry(store, id, "First update",
                                             &error);
    g_assert_no_error(error);
    g_assert_cmpint(entry_id, >, 0);
    g_assert_cmpint(gsword_prayer_store_add_entry(store, id, "Second update",
                                                  &error), >, 0);
    g_assert_no_error(error);

    entries = gsword_prayer_store_list_entries(store, id);
    g_assert_cmpuint(entries->len, ==, 2);
    entry = g_ptr_array_index(entries, 0);
    g_assert_cmpstr(entry->body, ==, "Second update");
    g_ptr_array_unref(entries);

    g_assert_true(gsword_prayer_store_delete_entry(store, entry_id, &error));
    g_assert_no_error(error);
    entries = gsword_prayer_store_list_entries(store, id);
    g_assert_cmpuint(entries->len, ==, 1);
    entry = g_ptr_array_index(entries, 0);
    g_assert_cmpstr(entry->body, ==, "Second update");
    g_ptr_array_unref(entries);

    g_assert_true(gsword_prayer_store_delete(store, id, &error));
    g_assert_no_error(error);
    entries = gsword_prayer_store_list_entries(store, id);
    g_assert_cmpuint(entries->len, ==, 0);
    g_ptr_array_unref(entries);
    gsword_prayer_store_free(store);
}

static void
test_verses(void)
{
    GError *error = NULL;
    GswordPrayerStore *store = gsword_prayer_store_new(&error);
    GPtrArray *verses;
    GswordPrayer *prayer;
    gint64 id;

    g_assert_no_error(error);
    g_assert_nonnull(store);
    id = gsword_prayer_store_create(store, "Verses", "", &error);
    g_assert_no_error(error);
    g_assert_cmpint(id, >, 0);

    verses = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_prayer_verse_free);
    g_ptr_array_add(verses,
                    gsword_prayer_verse_new("KJV", "Phil.4.6", "Philippians 4:6"));
    g_ptr_array_add(verses,
                    gsword_prayer_verse_new("KJV", "Phil.4.7", "Philippians 4:7"));
    g_assert_true(gsword_prayer_store_set_verses(store, id, verses, &error));
    g_assert_no_error(error);
    g_ptr_array_unref(verses);

    prayer = gsword_prayer_store_get(store, id);
    g_assert_nonnull(prayer);
    g_assert_cmpuint(prayer->verses->len, ==, 2);
    gsword_prayer_free(prayer);

    verses = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_prayer_verse_free);
    g_ptr_array_add(verses,
                    gsword_prayer_verse_new("KJV", "Phil.4.6", "Philippians 4:6"));
    g_assert_true(gsword_prayer_store_set_verses(store, id, verses, &error));
    g_assert_no_error(error);
    g_ptr_array_unref(verses);

    prayer = gsword_prayer_store_get(store, id);
    g_assert_nonnull(prayer);
    g_assert_cmpuint(prayer->verses->len, ==, 1);
    g_assert_cmpstr(((GswordPrayerVerse *)g_ptr_array_index(
                        prayer->verses, 0))->key, ==, "Phil.4.6");
    gsword_prayer_free(prayer);

    g_assert_true(gsword_prayer_store_delete(store, id, &error));
    g_assert_no_error(error);
    gsword_prayer_store_free(store);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    gchar *root;
    gchar *data_home;

    root = g_dir_make_tmp("gsword-prayer-store-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(root);
    data_home = g_build_filename(root, "data", NULL);
    g_mkdir_with_parents(data_home, 0755);
    g_setenv("XDG_DATA_HOME", data_home, TRUE);

    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/prayer-store/create-update-filter",
                    test_create_update_filter);
    g_test_add_func("/prayer-store/status", test_status);
    g_test_add_func("/prayer-store/entries", test_entries);
    g_test_add_func("/prayer-store/verses", test_verses);
    {
        int status = g_test_run();

        gsw_dir_remove_recursive(root, NULL);
        g_free(data_home);
        g_free(root);
        return status;
    }
}
