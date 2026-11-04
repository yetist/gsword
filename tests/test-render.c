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

static GByteArray *
empty_index(void)
{
    GByteArray *array = g_byte_array_new();
    guint i;

    for (i = 0; i < 5; i++) {
        append_le32(array, 0);
        append_le16(array, 0);
    }
    return array;
}

static void
make_module(const gchar *prefix)
{
    const gchar *entry =
        "<title>Heading</title> "
        "<h2>Second heading</h2> "
        "<w lemma=\"strong:G26\" morph=\"N-ASF\">love</w> "
        "<note>note text</note> "
        "<q who=\"Jesus\">red words</q> "
        "<reference>John 3:16</reference> "
        "<gloss>gloss text</gloss> "
        "<img src=\"map.png\"/>";
    gchar *dir = g_build_filename(prefix, "modules", "texts", "rawtext",
                                  "tinyrender", NULL);
    gchar *path;
    GByteArray *idx;

    g_mkdir_with_parents(dir, 0755);
    path = g_build_filename(dir, "ot", NULL);
    write_bytes(path, (const guint8 *)entry, strlen(entry));
    g_free(path);
    path = g_build_filename(dir, "nt", NULL);
    write_bytes(path, (const guint8 *)"", 0);
    g_free(path);

    idx = empty_index();
    g_byte_array_set_size(idx, 4 * 6); /* keep four zero records, then Gen 1:1 */
    append_le32(idx, 0);
    append_le16(idx, (guint16)strlen(entry));
    path = g_build_filename(dir, "ot.vss", NULL);
    write_bytes(path, idx->data, idx->len);
    g_free(path);
    g_byte_array_unref(idx);

    idx = empty_index();
    path = g_build_filename(dir, "nt.vss", NULL);
    write_bytes(path, idx->data, idx->len);
    g_free(path);
    g_byte_array_unref(idx);
    g_free(dir);

    {
        gchar *mods = g_build_filename(prefix, "mods.d", NULL);
        g_mkdir_with_parents(mods, 0755);
        path = g_build_filename(mods, "tinyrender.conf", NULL);
        g_assert_true(g_file_set_contents(path,
            "[TinyRender]\n"
            "Description=Render test\n"
            "DataPath=./modules/texts/rawtext/tinyrender/\n"
            "ModDrv=RawText\n"
            "Encoding=UTF-8\n"
            "SourceType=OSIS\n"
            "Version=1.0\n"
            "Versification=KJV\n", -1, NULL));
        g_free(path);
        g_free(mods);
    }
}

static void
test_render_options(void)
{
    GError *error = NULL;
    gchar *prefix = g_dir_make_tmp("gsword-render-XXXXXX", &error);
    GswManager *manager;
    GswModule *module;
    GswModule *fresh;
    gchar *text;
    gchar *html;
    GPtrArray *items;
    GPtrArray *keys;

    g_assert_no_error(error);
    make_module(prefix);

    manager = gsw_manager_new_for_path(prefix, &error);
    g_assert_no_error(error);
    module = gsw_manager_get_module(manager, "TinyRender");
    g_assert_nonnull(module);

    g_assert_true(gsw_module_set_key_text(module, "Gen 1:1", &error));
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "love red words");
    g_free(text);

    text = gsw_module_strip_text(module,
                                 "<b>Hello</b> &amp; <i>world</i>", &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "Hello & world");
    g_free(text);

    text = gsw_module_get_entry_attribute(module, "Heading", "Preverse", "0",
                                          &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "Heading");
    g_free(text);
    text = gsw_module_get_entry_attribute(module, "Heading", "Preverse", "1",
                                          &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "Second heading");
    g_free(text);
    text = gsw_module_get_entry_attribute(module, "Footnote", "body", "0",
                                          &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "note text");
    g_free(text);
    text = gsw_module_get_entry_attribute(module, "Crossref", "body", "0",
                                          &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "John 3:16");
    g_free(text);
    text = gsw_module_get_entry_attribute(module, "Image", "src", "0",
                                          &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "map.png");
    g_free(text);
    keys = gsw_module_get_entry_attribute_keys(module, "Heading", "Preverse",
                                               &error);
    g_assert_no_error(error);
    g_assert_cmpuint(keys->len, ==, 2);
    g_ptr_array_unref(keys);

    g_assert_true(gsw_module_set_option(module, "Footnotes", "On"));
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_nonnull(strstr(text, "note text"));
    g_free(text);

    g_assert_true(gsw_module_set_option(module, "Footnotes", "Off"));
    g_assert_true(gsw_module_set_option(module, "Strong's Numbers", "On"));
    text = gsw_module_get_render_text(module, &error);
    g_assert_no_error(error);
    g_assert_nonnull(strstr(text, "{G26}"));
    g_free(text);

    g_assert_true(gsw_module_set_option(module, "Headings", "On"));
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_nonnull(strstr(text, "Heading"));
    g_free(text);

    g_assert_true(gsw_module_set_option(module, "Red Letter Words", "On"));
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_nonnull(strstr(text, "\xC2\xABred words\xC2\xBB"));
    g_free(text);

    g_assert_true(gsw_module_set_option(module, "Footnotes", "On"));
    g_assert_true(gsw_module_set_option(module, "Cross-references", "On"));
    g_assert_true(gsw_module_set_option(module, "Images", "On"));
    g_assert_true(gsw_module_set_option(module, "Morphological Tags", "On"));
    html = gsw_module_get_render_html(module, &error);
    g_assert_no_error(error);
    g_assert_nonnull(strstr(html, "<div class=\"gsw-heading\">Heading</div>"));
    g_assert_nonnull(strstr(html, "<span class=\"gsw-strongs\">G26</span>"));
    g_assert_nonnull(strstr(html, "<span class=\"gsw-morph\">N-ASF</span>"));
    g_assert_nonnull(strstr(html, "class=\"gsw-footnote\""));
    g_assert_nonnull(strstr(html, "class=\"gsw-words-of-christ\""));
    g_assert_nonnull(strstr(html, "class=\"gsw-crossref\""));
    g_assert_nonnull(strstr(html, "<img src=\"map.png\""));
    g_free(html);
    g_assert_true(gsw_module_set_option(module, "Footnotes", "Off"));

    g_assert_cmpstr(gsw_module_get_option(module, "Headings"), ==, "On");
    items = gsw_module_get_options(module);
    g_assert_cmpuint(items->len, >=, 5);
    g_ptr_array_unref(items);
    items = gsw_module_get_option_values(module, "Footnotes");
    g_assert_cmpuint(items->len, ==, 2);
    g_assert_cmpstr(g_ptr_array_index(items, 0), ==, "On");
    g_ptr_array_unref(items);

    items = gsw_manager_get_option_values(manager, "Textual Variants");
    g_assert_cmpuint(items->len, ==, 4);
    g_ptr_array_unref(items);
    g_assert_true(gsw_manager_set_option(manager, "Footnotes", "On"));
    g_assert_false(gsw_manager_set_option(manager, "No Such Option", "On"));
    g_assert_cmpstr(gsw_manager_get_option(manager, "Footnotes"), ==, "On");
    g_assert_cmpstr(gsw_module_get_option(module, "Footnotes"), ==, "Off");
    fresh = gsw_manager_create_module(manager, "TinyRender");
    g_assert_nonnull(fresh);
    g_assert_cmpstr(gsw_module_get_option(fresh, "Footnotes"), ==, "On");
    g_object_unref(fresh);

    g_object_unref(manager);
    gsw_dir_remove_recursive(prefix, NULL);
    g_free(prefix);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/render/options", test_render_options);
    return g_test_run();
}
