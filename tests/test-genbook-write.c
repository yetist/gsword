#include <gsword/gsword.h>
#include "gsw-utils.h"
#include <glib/gstdio.h>

static void
set_module_tree_key(GswModule *module, GswTreeKey *tree)
{
    GError *error = NULL;

    g_assert_true(gsw_module_set_key(module, (GswKey *)tree, &error));
    g_assert_no_error(error);
}

static void
test_genbook_write(void)
{
    GError *error = NULL;
    gchar *prefix = g_dir_make_tmp("gsword-genbook-XXXXXX", &error);
    GswManager *manager;
    GswManager *reopened;
    GswModule *module;
    GswTreeKey *tree;
    gchar *text;

    g_assert_no_error(error);
    manager = gsw_manager_new_for_path(prefix, &error);
    g_assert_no_error(error);
    g_assert_nonnull(manager);

    g_assert_true(gsw_manager_create_genbook(manager, "Journal",
                                             "Journal test", &error));
    g_assert_no_error(error);
    g_assert_true(gsw_manager_has_module(manager, "Journal"));

    module = gsw_manager_get_module(manager, "Journal");
    g_assert_nonnull(module);
    g_assert_true(gsw_module_is_writable(module));

    tree = (GswTreeKey *)gsw_module_create_key(module);
    g_assert_nonnull(tree);
    g_assert_cmpstr(gsw_tree_key_get_local_name(tree), ==, "");

    g_assert_true(gsw_tree_key_append_child(tree, "Chapter 1", &error));
    g_assert_no_error(error);
    g_assert_cmpstr(gsw_tree_key_get_text(tree), ==, "Chapter 1");
    set_module_tree_key(module, tree);
    g_assert_true(gsw_module_set_entry(module, "Entry 1", &error));
    g_assert_no_error(error);

    g_assert_true(gsw_tree_key_append_sibling(tree, "Chapter 2", &error));
    g_assert_no_error(error);
    g_assert_cmpstr(gsw_tree_key_get_text(tree), ==, "Chapter 2");
    set_module_tree_key(module, tree);
    g_assert_true(gsw_module_set_entry(module, "Entry 2", &error));
    g_assert_no_error(error);

    g_assert_true(gsw_tree_key_append_child(tree, "Section 2.1", &error));
    g_assert_no_error(error);
    g_assert_cmpstr(gsw_tree_key_get_text(tree),
                    ==, "Chapter 2/Section 2.1");
    set_module_tree_key(module, tree);
    g_assert_true(gsw_module_set_entry(module, "Nested entry", &error));
    g_assert_no_error(error);

    g_assert_true(gsw_tree_key_set_local_name(tree, "Section Two", &error));
    g_assert_no_error(error);
    g_assert_cmpstr(gsw_tree_key_get_text(tree),
                    ==, "Chapter 2/Section Two");
    set_module_tree_key(module, tree);
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "Nested entry");
    g_free(text);

    g_assert_true(gsw_tree_key_remove_node(tree, &error));
    g_assert_no_error(error);
    g_assert_cmpstr(gsw_tree_key_get_text(tree), ==, "Chapter 2");

    g_object_unref(manager);
    gsw_key_unref(tree);

    reopened = gsw_manager_new_for_path(prefix, &error);
    g_assert_no_error(error);
    g_assert_nonnull(reopened);
    module = gsw_manager_get_module(reopened, "Journal");
    g_assert_nonnull(module);
    tree = (GswTreeKey *)gsw_module_create_key(module);
    g_assert_true(gsw_tree_key_set_text(tree, "Chapter 2"));
    set_module_tree_key(module, tree);
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "Entry 2");
    g_free(text);

    gsw_key_unref(tree);
    g_object_unref(reopened);
    gsw_dir_remove_recursive(prefix, NULL);
    g_free(prefix);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/genbook/write", test_genbook_write);
    return g_test_run();
}
