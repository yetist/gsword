/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include <gsword/gsword.h>
#include "gsw-utils.h"
#include <glib/gstdio.h>
#include <string.h>

#define WORKER_COUNT 4
#define WORKER_LOOPS 15

typedef struct {
    GswManager *manager;
    guint id;
    GError *error;
} WorkerData;

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
    g_assert_true(g_file_set_contents(path, (const gchar *)data,
                                      (gssize)len, NULL));
    g_free(parent);
}

static void
make_module(const gchar *prefix)
{
    const gchar *entries[] = {
        "God is love",
        "God created the world",
        "love your neighbour"
    };
    gchar *dir = g_build_filename(prefix, "modules", "texts", "rawtext",
                                  "concurrent", NULL);
    gchar *path;
    GByteArray *text = g_byte_array_new();
    GByteArray *idx = g_byte_array_new();
    guint i;

    g_mkdir_with_parents(dir, 0755);
    for (i = 0; i < 4; i++) {
        append_le32(idx, 0);
        append_le16(idx, 0);
    }
    for (i = 0; i < G_N_ELEMENTS(entries); i++) {
        guint32 offset = text->len;

        g_byte_array_append(text, (const guint8 *)entries[i],
                            (guint)strlen(entries[i]));
        g_byte_array_append(text, (const guint8 *)"\n", 1);
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
        gchar *conf;

        g_mkdir_with_parents(mods, 0755);
        conf = g_build_filename(mods, "concurrent.conf", NULL);
        g_assert_true(g_file_set_contents(conf,
            "[Concurrent]\n"
            "Description=Concurrent read test\n"
            "DataPath=./modules/texts/rawtext/concurrent/\n"
            "ModDrv=RawText\n"
            "Encoding=UTF-8\n"
            "Version=1.0\n"
            "Versification=KJV\n", -1, NULL));
        g_free(conf);
        g_free(mods);
    }
}

static gpointer
worker_thread(gpointer user_data)
{
    WorkerData *worker = user_data;
    guint i;

    for (i = 0; i < WORKER_LOOPS; i++) {
        GswModule *module = gsw_manager_create_module(worker->manager,
                                                       "Concurrent");
        GswSearchOptions *options;
        GPtrArray *results;
        gchar *text;

        if (module == NULL) {
            worker->error = g_error_new(GSW_ERROR, GSW_ERROR_NOT_FOUND,
                                        "Cannot create module");
            break;
        }
        if (!gsw_module_set_key_text(module, "Gen 1:1", &worker->error)) {
            g_object_unref(module);
            break;
        }
        text = gsw_module_get_text(module, &worker->error);
        if (text == NULL) {
            g_object_unref(module);
            break;
        }
        if (strstr(text, "God") == NULL) {
            worker->error = g_error_new(GSW_ERROR, GSW_ERROR_INVALID_DATA,
                                        "Unexpected entry text");
            g_free(text);
            g_object_unref(module);
            break;
        }
        g_free(text);

        options = gsw_search_options_new();
        gsw_search_options_set_text(options, "love");
        results = gsw_module_search_full(module, options, NULL, NULL, NULL,
                                         &worker->error);
        gsw_search_options_unref(options);
        if (results == NULL) {
            g_object_unref(module);
            break;
        }
        if (results->len != 2) {
            worker->error = g_error_new(GSW_ERROR, GSW_ERROR_INVALID_DATA,
                                        "Unexpected search result count");
            g_ptr_array_unref(results);
            g_object_unref(module);
            break;
        }
        g_ptr_array_unref(results);

        g_object_unref(module);
    }
    return NULL;
}

static void
test_concurrent_reads(void)
{
    GError *error = NULL;
    gchar *prefix = g_dir_make_tmp("gsword-concurrent-XXXXXX", &error);
    GswManager *manager;
    GThread *threads[WORKER_COUNT];
    WorkerData workers[WORKER_COUNT];
    guint i;

    g_assert_no_error(error);
    make_module(prefix);
    manager = gsw_manager_new_for_path(prefix, &error);
    g_assert_no_error(error);
    g_assert_nonnull(manager);

    for (i = 0; i < WORKER_COUNT; i++) {
        workers[i].manager = manager;
        workers[i].id = i;
        workers[i].error = NULL;
        threads[i] = g_thread_new("gsword-reader", worker_thread,
                                  &workers[i]);
    }
    for (i = 0; i < WORKER_COUNT; i++) {
        g_thread_join(threads[i]);
        g_assert_no_error(workers[i].error);
    }

    g_object_unref(manager);
    gsw_dir_remove_recursive(prefix, NULL);
    g_free(prefix);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/concurrent/independent-modules", test_concurrent_reads);
    return g_test_run();
}
