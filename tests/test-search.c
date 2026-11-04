/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include <gsword/gsword.h>
#include "gsw-utils.h"
#include <glib/gstdio.h>
#include <string.h>

static void
append_le32(GByteArray *array, guint32 value)
{
    guint8 bytes[4] = {
        (guint8)(value & 0xff), (guint8)((value >> 8) & 0xff),
        (guint8)((value >> 16) & 0xff), (guint8)((value >> 24) & 0xff)
    };
    g_byte_array_append(array, bytes, 4);
}

static void
append_le16(GByteArray *array, guint16 value)
{
    guint8 bytes[2] = { (guint8)(value & 0xff), (guint8)(value >> 8) };
    g_byte_array_append(array, bytes, 2);
}

static void
write_bytes(const gchar *path, const guint8 *data, gsize len)
{
    gchar *parent = g_path_get_dirname(path);
    g_mkdir_with_parents(parent, 0755);
    g_assert_true(g_file_set_contents(path, (const gchar *)data, (gssize)len, NULL));
    g_free(parent);
}

static void
make_module(const gchar *prefix)
{
    const gchar *entries[] = {
        "<w lemma=\"strong:G26\">God</w> is love",
        "God created the world",
        "love your neighbour"
    };
    gchar *dir = g_build_filename(prefix, "modules", "texts", "rawtext",
                                  "tinysearch", NULL);
    gchar *path;
    GByteArray *text = g_byte_array_new();
    GByteArray *idx = g_byte_array_new();
    guint i;

    g_mkdir_with_parents(dir, 0755);
    for (i = 0; i < 3; i++) {
        guint32 offset = text->len;
        g_byte_array_append(text, (const guint8 *)entries[i],
                            (guint)strlen(entries[i]));
        g_byte_array_append(text, (const guint8 *)"\n", 1);
        if (i == 0) {
            guint j;
            for (j = 0; j < 4; j++) {
                append_le32(idx, 0);
                append_le16(idx, 0);
            }
        }
        append_le32(idx, offset);
        append_le16(idx, (guint16)strlen(entries[i]));
    }

    path = g_build_filename(dir, "ot", NULL);
    write_bytes(path, text->data, text->len);
    g_free(path);
    path = g_build_filename(dir, "ot.vss", NULL);
    write_bytes(path, idx->data, idx->len);
    g_free(path);

    g_byte_array_set_size(idx, 0);
    for (i = 0; i < 5; i++) {
        append_le32(idx, 0);
        append_le16(idx, 0);
    }
    path = g_build_filename(dir, "nt", NULL);
    write_bytes(path, (const guint8 *)"", 0);
    g_free(path);
    path = g_build_filename(dir, "nt.vss", NULL);
    write_bytes(path, idx->data, idx->len);
    g_free(path);

    g_byte_array_unref(text);
    g_byte_array_unref(idx);
    g_free(dir);

    {
        gchar *mods = g_build_filename(prefix, "mods.d", NULL);
        g_mkdir_with_parents(mods, 0755);
        path = g_build_filename(mods, "tinysearch.conf", NULL);
        g_assert_true(g_file_set_contents(path,
            "[TinySearch]\n"
            "Description=Search test\n"
            "DataPath=./modules/texts/rawtext/tinysearch/\n"
            "ModDrv=RawText\n"
            "Encoding=UTF-8\n"
            "Version=1.0\n"
            "Versification=KJV\n", -1, NULL));
        g_free(path);
        g_free(mods);
    }
}

static void
make_large_module(const gchar *prefix, guint count)
{
    gchar *dir = g_build_filename(prefix, "modules", "texts", "rawtext",
                                  "largesearch", NULL);
    gchar *path;
    GByteArray *text = g_byte_array_new();
    GByteArray *idx = g_byte_array_new();
    guint i;

    g_mkdir_with_parents(dir, 0755);
    for (i = 0; i < 4; i++) {
        append_le32(idx, 0);
        append_le16(idx, 0);
    }
    for (i = 0; i < count; i++) {
        gchar *entry;

        if (i == count / 2)
            entry = g_strdup_printf("entry %u common filler needle", i);
        else
            entry = g_strdup_printf("entry %u common filler word", i);
        {
            guint32 offset = text->len;

            g_byte_array_append(text, (const guint8 *)entry,
                                (guint)strlen(entry));
            g_byte_array_append(text, (const guint8 *)"\n", 1);
            append_le32(idx, offset);
            append_le16(idx, (guint16)strlen(entry));
        }
        g_free(entry);
    }

    path = g_build_filename(dir, "ot", NULL);
    write_bytes(path, text->data, text->len);
    g_free(path);
    path = g_build_filename(dir, "ot.vss", NULL);
    write_bytes(path, idx->data, idx->len);
    g_free(path);

    g_byte_array_set_size(idx, 0);
    for (i = 0; i < 5; i++) {
        append_le32(idx, 0);
        append_le16(idx, 0);
    }
    path = g_build_filename(dir, "nt", NULL);
    write_bytes(path, (const guint8 *)"", 0);
    g_free(path);
    path = g_build_filename(dir, "nt.vss", NULL);
    write_bytes(path, idx->data, idx->len);
    g_free(path);

    g_byte_array_unref(text);
    g_byte_array_unref(idx);
    g_free(dir);

    {
        gchar *mods = g_build_filename(prefix, "mods.d", NULL);
        g_mkdir_with_parents(mods, 0755);
        path = g_build_filename(mods, "largesearch.conf", NULL);
        g_assert_true(g_file_set_contents(path,
            "[LargeSearch]\n"
            "Description=Large search test\n"
            "DataPath=./modules/texts/rawtext/largesearch/\n"
            "ModDrv=RawText\n"
            "Encoding=UTF-8\n"
            "Version=1.0\n"
            "Versification=KJV\n", -1, NULL));
        g_free(path);
        g_free(mods);
    }
}

static void
test_search(void)
{
    GError *error = NULL;
    gchar *prefix = g_dir_make_tmp("gsword-search-XXXXXX", &error);
    GswManager *manager;
    GswModule *module;
    gchar **results;

    g_assert_no_error(error);
    make_module(prefix);
    manager = gsw_manager_new_for_path(prefix, &error);
    g_assert_no_error(error);
    module = gsw_manager_get_module(manager, "TinySearch");
    g_assert_nonnull(module);

    g_assert_true(gsw_module_set_key_text(module, "Gen 1:1", &error));
    results = gsw_module_search(module, "love", GSW_SEARCH_PHRASE, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(results[0], ==, "Genesis 1:1");
    g_assert_cmpstr(results[1], ==, "Genesis 1:3");
    g_assert_null(results[2]);
    g_strfreev(results);
    g_assert_cmpstr(gsw_module_get_key_text(module), ==, "Genesis 1:1");

    results = gsw_module_search(module, "God world", GSW_SEARCH_MULTIWORD, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(results[0], ==, "Genesis 1:2");
    g_assert_null(results[1]);
    g_strfreev(results);

    results = gsw_module_search(module, "l.ve", GSW_SEARCH_REGEX, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(results[0], ==, "Genesis 1:1");
    g_assert_cmpstr(results[1], ==, "Genesis 1:3");
    g_assert_null(results[2]);
    g_strfreev(results);

    g_object_unref(manager);
    gsw_dir_remove_recursive(prefix, NULL);
    g_free(prefix);
}

static void
search_progress(const gchar *module_name, guint processed, guint total,
                gpointer user_data)
{
    guint *calls = user_data;

    g_assert_cmpstr(module_name, ==, "TinySearch");
    g_assert_cmpuint(processed, >, 0);
    (void)total;
    (*calls)++;
}

static void
test_search_options(void)
{
    GError *error = NULL;
    gchar *prefix = g_dir_make_tmp("gsword-search-XXXXXX", &error);
    GswManager *manager;
    GswModule *module;
    GswSearchOptions *options;
    GswVerseList *scope;
    GPtrArray *results;
    GswSearchResult *result;
    GCancellable *cancellable;
    guint progress_calls = 0;

    g_assert_no_error(error);
    make_module(prefix);
    manager = gsw_manager_new_for_path(prefix, &error);
    g_assert_no_error(error);
    module = gsw_manager_get_module(manager, "TinySearch");
    g_assert_nonnull(module);

    options = gsw_search_options_new();
    gsw_search_options_set_text(options, "love");
    gsw_search_options_set_max_results(options, 1);
    results = gsw_module_search_full(module, options, NULL, search_progress,
                                     &progress_calls, &error);
    g_assert_no_error(error);
    g_assert_nonnull(results);
    g_assert_cmpuint(results->len, ==, 1);
    g_assert_cmpuint(progress_calls, >, 0);
    result = g_ptr_array_index(results, 0);
    g_assert_cmpstr(gsw_search_result_get_module_name(result), ==,
                    "TinySearch");
    g_assert_cmpstr(gsw_search_result_get_key(result), ==, "Genesis 1:1");
    g_assert_nonnull(gsw_search_result_get_preview(result));
    g_assert_nonnull(gsw_search_result_get_snippet(result));
    g_ptr_array_unref(results);

    gsw_search_options_set_text(options, "god");
    gsw_search_options_set_max_results(options, 0);
    gsw_search_options_set_case_sensitive(options, TRUE);
    results = gsw_module_search_full(module, options, NULL, NULL, NULL, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(results->len, ==, 0);
    g_ptr_array_unref(results);

    gsw_search_options_set_case_sensitive(options, FALSE);
    results = gsw_module_search_full(module, options, NULL, NULL, NULL, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(results->len, ==, 2);
    g_ptr_array_unref(results);

    scope = gsw_verse_list_parse("Gen 1:1-2",
                                 (GswVerseKey *)gsw_module_get_key(module),
                                 &error);
    g_assert_no_error(error);
    g_assert_nonnull(scope);
    gsw_search_options_set_text(options, "God");
    gsw_search_options_set_max_results(options, 1);
    gsw_search_options_set_scope(options, scope);
    results = gsw_module_search_full(module, options, NULL, NULL, NULL, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(results->len, ==, 1);
    g_ptr_array_unref(results);
    gsw_search_options_set_scope(options, NULL);
    gsw_search_options_set_max_results(options, 0);
    gsw_verse_list_unref(scope);

    gsw_search_options_set_text(options, "G26");
    gsw_search_options_set_attribute(options, "Word", "Lemma");
    results = gsw_module_search_full(module, options, NULL, NULL, NULL, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(results->len, ==, 1);
    result = g_ptr_array_index(results, 0);
    g_assert_cmpstr(gsw_search_result_get_key(result), ==, "Genesis 1:1");
    g_ptr_array_unref(results);
    gsw_search_options_set_attribute(options, NULL, NULL);

    cancellable = g_cancellable_new();
    g_cancellable_cancel(cancellable);
    gsw_search_options_set_text(options, "God");
    results = gsw_module_search_full(module, options, cancellable, NULL, NULL,
                                     &error);
    g_assert_null(results);
    g_assert_error(error, GSW_ERROR, GSW_ERROR_CANCELLED);
    g_clear_error(&error);
    g_object_unref(cancellable);

    {
        const gchar *modules[] = { "TinySearch", NULL };

        gsw_search_options_set_attribute(options, NULL, NULL);
        gsw_search_options_set_text(options, "love");
        results = gsw_manager_search(manager, modules, options, NULL, NULL,
                                     NULL, &error);
        g_assert_no_error(error);
        g_assert_nonnull(results);
        g_assert_cmpuint(results->len, ==, 2);
        result = g_ptr_array_index(results, 0);
        g_assert_cmpstr(gsw_search_result_get_module_name(result), ==,
                        "TinySearch");
        g_ptr_array_unref(results);
    }

    gsw_search_options_unref(options);
    g_object_unref(manager);
    gsw_dir_remove_recursive(prefix, NULL);
    g_free(prefix);
}

static void
index_progress(const gchar *module_name, guint processed, guint total,
               gpointer user_data)
{
    guint *calls = user_data;

    g_assert_cmpstr(module_name, ==, "TinySearch");
    g_assert_cmpuint(processed, >, 0);
    g_assert_cmpuint(total, >, 0);
    (*calls)++;
}

static void
test_search_index(void)
{
    GError *error = NULL;
    gchar *prefix = g_dir_make_tmp("gsword-search-index-XXXXXX", &error);
    gchar *cache_dir = g_dir_make_tmp("gsword-search-cache-XXXXXX", &error);
    gchar *cache_index;
    GswManager *manager;
    GswModule *module;
    GswSearchOptions *options;
    GPtrArray *results;
    guint progress_calls = 0;

    g_assert_no_error(error);
    make_module(prefix);
    manager = gsw_manager_new_for_path(prefix, &error);
    g_assert_no_error(error);
    module = gsw_manager_get_module(manager, "TinySearch");
    g_assert_nonnull(module);
    g_assert_false(gsw_module_has_search_index(module));

    g_assert_true(gsw_module_build_search_index(module, NULL, index_progress,
                                                &progress_calls, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(progress_calls, >, 0);
    g_assert_true(gsw_module_has_search_index(module));

    options = gsw_search_options_new();
    gsw_search_options_set_text(options, "love");
    results = gsw_module_search_full(module, options, NULL, NULL, NULL,
                                     &error);
    g_assert_no_error(error);
    g_assert_cmpuint(results->len, ==, 2);
    g_assert_cmpstr(gsw_search_result_get_key(g_ptr_array_index(results, 0)),
                    ==, "Genesis 1:1");
    g_assert_cmpstr(gsw_search_result_get_key(g_ptr_array_index(results, 1)),
                    ==, "Genesis 1:3");
    g_ptr_array_unref(results);

    gsw_search_options_set_text(options, "God world");
    gsw_search_options_set_search_type(options, GSW_SEARCH_MULTIWORD);
    results = gsw_module_search_full(module, options, NULL, NULL, NULL,
                                     &error);
    g_assert_no_error(error);
    g_assert_cmpuint(results->len, ==, 1);
    g_assert_cmpstr(gsw_search_result_get_key(g_ptr_array_index(results, 0)),
                    ==, "Genesis 1:2");
    g_ptr_array_unref(results);

    /* Index cannot answer case-sensitive or regex queries; the normal scan
     * must still return correct results. */
    gsw_search_options_set_text(options, "god");
    gsw_search_options_set_case_sensitive(options, TRUE);
    results = gsw_module_search_full(module, options, NULL, NULL, NULL,
                                     &error);
    g_assert_no_error(error);
    g_assert_cmpuint(results->len, ==, 0);
    g_ptr_array_unref(results);

    g_assert_true(gsw_module_delete_search_index(module, &error));
    g_assert_no_error(error);
    g_assert_false(gsw_module_has_search_index(module));

    cache_index = g_build_filename(cache_dir, "tiny-search.idx", NULL);
    gsw_module_set_search_index_path(module, cache_index);
    g_assert_cmpstr(gsw_module_get_search_index_path(module), ==,
                    cache_index);
    g_assert_true(gsw_module_build_search_index(module, NULL, NULL, NULL,
                                                &error));
    g_assert_no_error(error);
    g_assert_true(g_file_test(cache_index, G_FILE_TEST_IS_REGULAR));
    g_assert_true(gsw_module_has_search_index(module));
    g_assert_true(gsw_module_delete_search_index(module, &error));
    g_assert_no_error(error);
    g_assert_false(g_file_test(cache_index, G_FILE_TEST_IS_REGULAR));
    gsw_module_set_search_index_path(module, NULL);
    g_assert_null(gsw_module_get_search_index_path(module));

    {
        GCancellable *cancellable = g_cancellable_new();

        g_cancellable_cancel(cancellable);
        g_assert_false(gsw_module_build_search_index(module, cancellable,
                                                     NULL, NULL, &error));
        g_assert_error(error, GSW_ERROR, GSW_ERROR_CANCELLED);
        g_clear_error(&error);
        g_object_unref(cancellable);
        g_assert_false(gsw_module_has_search_index(module));
    }

    gsw_search_options_unref(options);
    g_object_unref(manager);
    gsw_dir_remove_recursive(cache_dir, NULL);
    gsw_dir_remove_recursive(prefix, NULL);
    g_free(cache_index);
    g_free(cache_dir);
    g_free(prefix);
}

static void
test_search_index_large(void)
{
    GError *error = NULL;
    gchar *prefix = g_dir_make_tmp("gsword-search-large-XXXXXX", &error);
    GswManager *manager;
    GswModule *module;
    GswSearchOptions *options;
    GPtrArray *results;
    GswSearchResult *result;
    gint64 scan_start;
    gint64 scan_elapsed;
    gint64 index_start;
    gint64 index_elapsed;
    gchar *scan_key;

    g_assert_no_error(error);
    make_large_module(prefix, 3000);
    manager = gsw_manager_new_for_path(prefix, &error);
    g_assert_no_error(error);
    module = gsw_manager_get_module(manager, "LargeSearch");
    g_assert_nonnull(module);

    options = gsw_search_options_new();
    gsw_search_options_set_text(options, "needle");

    scan_start = g_get_monotonic_time();
    results = gsw_module_search_full(module, options, NULL, NULL, NULL,
                                     &error);
    scan_elapsed = g_get_monotonic_time() - scan_start;
    g_assert_no_error(error);
    g_assert_cmpuint(results->len, ==, 1);
    scan_key = g_strdup(gsw_search_result_get_key(
        g_ptr_array_index(results, 0)));
    g_ptr_array_unref(results);

    g_assert_true(gsw_module_build_search_index(module, NULL, NULL, NULL,
                                                &error));
    g_assert_no_error(error);
    index_start = g_get_monotonic_time();
    results = gsw_module_search_full(module, options, NULL, NULL, NULL,
                                     &error);
    index_elapsed = g_get_monotonic_time() - index_start;
    g_assert_no_error(error);
    g_assert_cmpuint(results->len, ==, 1);
    result = g_ptr_array_index(results, 0);
    g_assert_cmpstr(gsw_search_result_get_key(result), ==, scan_key);
    g_ptr_array_unref(results);
    g_free(scan_key);

    g_test_message("large search: scan=%" G_GINT64_FORMAT
                   "us index=%" G_GINT64_FORMAT "us",
                   scan_elapsed, index_elapsed);
    g_assert_cmpint(index_elapsed, <, scan_elapsed);

    gsw_search_options_unref(options);
    g_object_unref(manager);
    gsw_dir_remove_recursive(prefix, NULL);
    g_free(prefix);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/search/basic", test_search);
    g_test_add_func("/search/options", test_search_options);
    g_test_add_func("/search/index", test_search_index);
    g_test_add_func("/search/index-large", test_search_index_large);
    return g_test_run();
}
