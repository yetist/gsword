#include <gsword/gsword.h>
#include "gsw-utils.h"
#include <glib/gstdio.h>
#include <string.h>
#include <zlib.h>

static void
append_le32(GByteArray *array, guint32 value)
{
    guint8 bytes[4] = {
        (guint8)(value & 0xff),
        (guint8)((value >> 8) & 0xff),
        (guint8)((value >> 16) & 0xff),
        (guint8)((value >> 24) & 0xff)
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
write_text(const gchar *path, const gchar *text)
{
    write_bytes(path, (const guint8 *)text, strlen(text));
}

static void
write_conf(const gchar *prefix, const gchar *name, const gchar *conf)
{
    gchar *dir = g_build_filename(prefix, "mods.d", NULL);
    gchar *path;

    g_mkdir_with_parents(dir, 0755);
    path = g_build_filename(dir, name, NULL);
    g_assert_true(g_file_set_contents(path, conf, -1, NULL));
    g_free(path);
    g_free(dir);
}

static GByteArray *
make_raw_index(guint32 start, guint32 size, gsize count)
{
    GByteArray *array = g_byte_array_new();
    guint32 i;

    for (i = 0; i < count; i++) {
        if (i == 4) {
            append_le32(array, start);
            append_le16(array, (guint16)size);
        } else {
            append_le32(array, 0);
            append_le16(array, 0);
        }
    }
    return array;
}

static void
make_raw_module(const gchar *prefix)
{
    gchar *dir = g_build_filename(prefix, "modules", "texts", "rawtext",
                                  "tiny", NULL);
    gchar *path;
    GByteArray *idx;
    const gchar *ot = "Raw OT Gen 1:1";
    const gchar *nt = "Raw NT Matt 1:1";

    g_mkdir_with_parents(dir, 0755);

    path = g_build_filename(dir, "ot", NULL);
    write_text(path, ot);
    g_free(path);
    path = g_build_filename(dir, "nt", NULL);
    write_text(path, nt);
    g_free(path);

    idx = make_raw_index(0, (guint32)strlen(ot), 5);
    path = g_build_filename(dir, "ot.vss", NULL);
    write_bytes(path, idx->data, idx->len);
    g_free(path);
    g_byte_array_unref(idx);

    idx = make_raw_index(0, (guint32)strlen(nt), 5);
    path = g_build_filename(dir, "nt.vss", NULL);
    write_bytes(path, idx->data, idx->len);
    g_free(path);
    g_byte_array_unref(idx);

    g_free(dir);

    write_conf(prefix, "tiny-raw.conf",
        "[TinyRaw]\n"
        "Description=Tiny RawText\n"
        "DataPath=./modules/texts/rawtext/tiny/\n"
        "ModDrv=RawText\n"
        "Encoding=UTF-8\n"
        "Version=1.0\n"
        "Versification=KJV\n");
}

static void
make_ztext_module(const gchar *prefix)
{
    gchar *dir = g_build_filename(prefix, "modules", "texts", "ztext",
                                  "tinyz", NULL);
    gchar *path;
    const gchar *entry1 = "ZIP OT Gen 1:1";
    const gchar *entry2 = "ZIP OT Gen 1:2";
    gchar *uncompressed = g_strconcat(entry1, entry2, NULL);
    gsize uncompressed_len = strlen(uncompressed);
    uLongf comp_len = compressBound((uLong)uncompressed_len);
    gchar *compressed = g_malloc(comp_len);
    GByteArray *verse_idx = g_byte_array_new();
    GByteArray *block_idx = g_byte_array_new();
    guint32 i;

    g_assert_cmpint(compress2((Bytef *)compressed, &comp_len,
                              (const Bytef *)uncompressed,
                              (uLong)uncompressed_len, 9), ==, Z_OK);
    g_mkdir_with_parents(dir, 0755);

    for (i = 0; i < 6; i++) {
        if (i == 4) {
            append_le32(verse_idx, 0);
            append_le32(verse_idx, 0);
            append_le16(verse_idx, (guint16)strlen(entry1));
        } else if (i == 5) {
            append_le32(verse_idx, 0);
            append_le32(verse_idx, (guint32)strlen(entry1));
            append_le16(verse_idx, (guint16)strlen(entry2));
        } else {
            append_le32(verse_idx, 0);
            append_le32(verse_idx, 0);
            append_le16(verse_idx, 0);
        }
    }

    append_le32(block_idx, 0);
    append_le32(block_idx, (guint32)comp_len);
    append_le32(block_idx, (guint32)uncompressed_len);

    path = g_build_filename(dir, "ot.bzv", NULL);
    write_bytes(path, verse_idx->data, verse_idx->len);
    g_free(path);
    path = g_build_filename(dir, "ot.bzs", NULL);
    write_bytes(path, block_idx->data, block_idx->len);
    g_free(path);
    path = g_build_filename(dir, "ot.bzz", NULL);
    write_bytes(path, (const guint8 *)compressed, comp_len);
    g_free(path);

    /* Empty NT files are enough for this fixture. */
    path = g_build_filename(dir, "nt.bzv", NULL);
    write_bytes(path, verse_idx->data, verse_idx->len);
    g_free(path);
    path = g_build_filename(dir, "nt.bzs", NULL);
    write_bytes(path, block_idx->data, block_idx->len);
    g_free(path);
    path = g_build_filename(dir, "nt.bzz", NULL);
    write_bytes(path, (const guint8 *)compressed, comp_len);
    g_free(path);

    g_byte_array_unref(verse_idx);
    g_byte_array_unref(block_idx);
    g_free(compressed);
    g_free(uncompressed);
    g_free(dir);

    write_conf(prefix, "tiny-z.conf",
        "[TinyZ]\n"
        "Description=Tiny zText\n"
        "DataPath=./modules/texts/ztext/tinyz/\n"
        "ModDrv=zText\n"
        "Encoding=UTF-8\n"
        "BlockType=BOOK\n"
        "CompressType=ZIP\n"
        "Version=1.0\n"
        "Versification=KJV\n");
}

static void
make_rawld_module(const gchar *prefix)
{
    gchar *dir = g_build_filename(prefix, "modules", "lexdict", "rawld",
                                  "tinyd", NULL);
    gchar *path;
    GByteArray *dat = g_byte_array_new();
    GByteArray *idx = g_byte_array_new();
    const gchar *keys[] = { "alpha", "beta" };
    const gchar *values[] = { "first value", "second value" };
    guint i;

    g_mkdir_with_parents(dir, 0755);
    for (i = 0; i < 2; i++) {
        guint32 offset = dat->len;
        gchar *record = g_strdup_printf("%s\r\n%s", keys[i], values[i]);
        gsize len = strlen(record);

        g_byte_array_append(dat, (const guint8 *)record, (guint)len);
        append_le32(idx, offset);
        append_le16(idx, (guint16)len);
        g_free(record);
    }

    path = g_build_filename(dir, "tinyd.dat", NULL);
    write_bytes(path, dat->data, dat->len);
    g_free(path);
    path = g_build_filename(dir, "tinyd.idx", NULL);
    write_bytes(path, idx->data, idx->len);
    g_free(path);
    g_byte_array_unref(dat);
    g_byte_array_unref(idx);
    g_free(dir);

    write_conf(prefix, "tiny-dict.conf",
        "[TinyDict]\n"
        "Description=Tiny dictionary\n"
        "DataPath=./modules/lexdict/rawld/tinyd/tinyd\n"
        "ModDrv=RawLD\n"
        "Encoding=UTF-8\n"
        "StrongsPadding=false\n"
        "Version=1.0\n");
}

static void
make_zld_module(const gchar *prefix)
{
    gchar *dir = g_build_filename(prefix, "modules", "lexdict", "zld",
                                  "tinyzd", NULL);
    gchar *path;
    const gchar *k1 = "alpha";
    const gchar *k2 = "beta";
    const gchar *v1 = "first value";
    const gchar *v2 = "second value";
    GByteArray *block = g_byte_array_new();
    GByteArray *dat = g_byte_array_new();
    GByteArray *idx = g_byte_array_new();
    GByteArray *zdx = g_byte_array_new();
    guint32 data_start;
    uLongf comp_len;

    /* EntriesBlock: count, two metas, then NUL-terminated values. */
    append_le32(block, 2);
    append_le32(block, 20);
    append_le32(block, (guint32)strlen(v1) + 1);
    append_le32(block, 20 + (guint32)strlen(v1) + 1);
    append_le32(block, (guint32)strlen(v2) + 1);
    g_byte_array_append(block, (const guint8 *)v1, (guint)strlen(v1) + 1);
    g_byte_array_append(block, (const guint8 *)v2, (guint)strlen(v2) + 1);
    data_start = block->len;

    comp_len = compressBound((uLong)data_start);
    {
        gchar *compressed = g_malloc(comp_len);
        g_assert_cmpint(compress2((Bytef *)compressed, &comp_len,
                                  (const Bytef *)block->data,
                                  (uLong)data_start, 9), ==, Z_OK);
        append_le32(zdx, 0);
        append_le32(zdx, (guint32)comp_len);
        g_mkdir_with_parents(dir, 0755);
        path = g_build_filename(dir, "tinyzd.zdt", NULL);
        write_bytes(path, (const guint8 *)compressed, comp_len);
        g_free(path);
        g_free(compressed);
    }

    /* .dat: key\r\n + block/entry. */
    {
        const gchar *keys[] = { k1, k2 };
        guint32 entries[] = { 0, 1 };
        guint i;

        for (i = 0; i < 2; i++) {
            guint32 offset = dat->len;
            gchar *head = g_strdup_printf("%s\r\n", keys[i]);
            gsize head_len = strlen(head);

            g_byte_array_append(dat, (const guint8 *)head, (guint)head_len);
            append_le32(dat, 0);
            append_le32(dat, entries[i]);
            append_le32(idx, offset);
            append_le32(idx, (guint32)(head_len + 8));
            g_free(head);
        }
    }

    path = g_build_filename(dir, "tinyzd.dat", NULL);
    write_bytes(path, dat->data, dat->len);
    g_free(path);
    path = g_build_filename(dir, "tinyzd.idx", NULL);
    write_bytes(path, idx->data, idx->len);
    g_free(path);
    path = g_build_filename(dir, "tinyzd.zdx", NULL);
    write_bytes(path, zdx->data, zdx->len);
    g_free(path);

    g_byte_array_unref(block);
    g_byte_array_unref(dat);
    g_byte_array_unref(idx);
    g_byte_array_unref(zdx);
    g_free(dir);

    write_conf(prefix, "tiny-zdict.conf",
        "[TinyZDict]\n"
        "Description=Tiny zLD dictionary\n"
        "DataPath=./modules/lexdict/zld/tinyzd/tinyzd\n"
        "ModDrv=zLD\n"
        "Encoding=UTF-8\n"
        "CompressType=ZIP\n"
        "StrongsPadding=false\n"
        "Version=1.0\n");
}

static guint32
append_tree_node(GByteArray *dat, gint parent, gint next, gint first,
                 const gchar *name, const gchar *user_data, guint16 user_size)
{
    guint32 offset = dat->len;

    append_le32(dat, (guint32)parent);
    append_le32(dat, (guint32)next);
    append_le32(dat, (guint32)first);
    g_byte_array_append(dat, (const guint8 *)name, (guint)strlen(name) + 1);
    append_le16(dat, user_size);
    if (user_size > 0)
        g_byte_array_append(dat, (const guint8 *)user_data, user_size);
    return offset;
}

static void
make_genbook_module(const gchar *prefix)
{
    gchar *dir = g_build_filename(prefix, "modules", "genbook",
                                  "rawgenbook", "tinygb", NULL);
    gchar *path;
    const gchar *content1 = "Content 1";
    const gchar *content2 = "Content 2";
    GByteArray *dat = g_byte_array_new();
    GByteArray *idx = g_byte_array_new();
    gchar user1[8], user2[8];
    guint32 off_root, off_child1, off_child2;

    g_mkdir_with_parents(dir, 0755);

    {
        guint32 o1 = 0;
        guint32 s1 = (guint32)strlen(content1);
        guint32 o2 = s1;
        guint32 s2 = (guint32)strlen(content2);
        memcpy(user1, &o1, 4);
        memcpy(user1 + 4, &s1, 4);
        memcpy(user2, &o2, 4);
        memcpy(user2 + 4, &s2, 4);
    }

    /* Reserve idx entries by writing dat in node order. */
    off_root = append_tree_node(dat, -1, -1, 4, "", NULL, 0);
    off_child1 = append_tree_node(dat, 0, 8, -1, "Chapter 1", user1, 8);
    off_child2 = append_tree_node(dat, 0, -1, -1, "Chapter 2", user2, 8);

    append_le32(idx, off_root);
    append_le32(idx, off_child1);
    append_le32(idx, off_child2);

    path = g_build_filename(dir, "tinygb.dat", NULL);
    write_bytes(path, dat->data, dat->len);
    g_free(path);
    path = g_build_filename(dir, "tinygb.idx", NULL);
    write_bytes(path, idx->data, idx->len);
    g_free(path);
    path = g_build_filename(dir, "tinygb.bdt", NULL);
    {
        gchar *content = g_strconcat(content1, content2, NULL);
        write_text(path, content);
        g_free(content);
    }
    g_free(path);

    g_byte_array_unref(dat);
    g_byte_array_unref(idx);
    g_free(dir);

    write_conf(prefix, "tiny-genbook.conf",
        "[TinyGenBook]\n"
        "Description=Tiny general book\n"
        "DataPath=./modules/genbook/rawgenbook/tinygb/tinygb\n"
        "ModDrv=RawGenBook\n"
        "Encoding=UTF-8\n"
        "Version=1.0\n");
}

static void
run_reader_test(void)
{
    GError *error = NULL;
    gchar *prefix = g_dir_make_tmp("gsword-reader-XXXXXX", &error);
    GswManager *manager;
    GswModule *module;
    gchar *text;

    g_assert_no_error(error);
    g_assert_nonnull(prefix);

    make_raw_module(prefix);
    make_ztext_module(prefix);
    make_rawld_module(prefix);
    make_zld_module(prefix);
    make_genbook_module(prefix);

    manager = gsw_manager_new_for_path(prefix, &error);
    g_assert_no_error(error);
    g_assert_nonnull(manager);
    g_assert_true(gsw_manager_has_module(manager, "TinyRaw"));
    g_assert_true(gsw_manager_has_module(manager, "TinyZ"));
    g_assert_true(gsw_manager_has_module(manager, "TinyDict"));
    g_assert_true(gsw_manager_has_module(manager, "TinyZDict"));
    g_assert_true(gsw_manager_has_module(manager, "TinyGenBook"));

    module = gsw_manager_get_module(manager, "TinyRaw");
    g_assert_nonnull(module);
    g_assert_true(gsw_module_is_supported(module));
    g_assert_true(gsw_module_set_key_text(module, "Gen 1:1", &error));
    g_assert_no_error(error);
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "Raw OT Gen 1:1");
    g_free(text);
    g_assert_true(gsw_module_has_entry(module));
    g_assert_cmpuint(gsw_module_get_entry_size(module), ==,
                     strlen("Raw OT Gen 1:1"));

    g_assert_true(gsw_module_set_key_text(module, "Matt 1:1", &error));
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "Raw NT Matt 1:1");
    g_free(text);

    module = gsw_manager_get_module(manager, "TinyZ");
    g_assert_nonnull(module);
    g_assert_true(gsw_module_is_supported(module));
    g_assert_true(gsw_module_set_key_text(module, "Gen 1:1", &error));
    g_assert_no_error(error);
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "ZIP OT Gen 1:1");
    g_free(text);

    g_assert_true(gsw_module_next(module, 1, &error));
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "ZIP OT Gen 1:2");
    g_free(text);

    module = gsw_manager_get_module(manager, "TinyDict");
    g_assert_nonnull(module);
    g_assert_true(gsw_module_is_supported(module));
    g_assert_true(gsw_module_set_key_text(module, "alpha", &error));
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "first value");
    g_free(text);
    g_assert_true(gsw_module_has_entry(module));
    g_assert_true(gsw_module_next(module, 1, &error));
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "second value");
    g_free(text);

    module = gsw_manager_get_module(manager, "TinyZDict");
    g_assert_nonnull(module);
    g_assert_true(gsw_module_is_supported(module));
    g_assert_true(gsw_module_set_key_text(module, "alpha", &error));
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "first value");
    g_free(text);
    g_assert_true(gsw_module_next(module, 1, &error));
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "second value");
    g_free(text);

    module = gsw_manager_get_module(manager, "TinyGenBook");
    g_assert_nonnull(module);
    g_assert_true(gsw_module_is_supported(module));
    g_assert_true(gsw_module_set_key_text(module, "Chapter 1", &error));
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "Content 1");
    g_free(text);
    g_assert_true(gsw_module_next(module, 1, &error));
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "Content 2");
    g_free(text);

    g_object_unref(manager);
    gsw_dir_remove_recursive(prefix, NULL);
    g_free(prefix);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/reader/raw-ztext-dicts-genbook", run_reader_test);
    return g_test_run();
}
