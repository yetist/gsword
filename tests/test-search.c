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
        "God is love",
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

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/search/basic", test_search);
    return g_test_run();
}
