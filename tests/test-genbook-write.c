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

static void
test_genbook_recovery(void)
{
    GError *error = NULL;
    gchar *prefix = g_dir_make_tmp("gsword-genbook-recovery-XXXXXX", &error);
    GswManager *manager;
    GswModule *module;
    GswTreeKey *tree;
    gchar *base;
    gchar *dat_path;
    gchar *idx_path;
    gchar *dat_new;
    gchar *idx_new;
    gchar *dat_bak;
    gchar *idx_bak;
    gchar *txn_path;
    gchar *dat = NULL;
    gchar *idx = NULL;
    gsize dat_len = 0;
    gsize idx_len = 0;
    gchar *text;

    g_assert_no_error(error);
    manager = gsw_manager_new_for_path(prefix, &error);
    g_assert_no_error(error);
    g_assert_true(gsw_manager_create_genbook(manager, "Journal",
                                             "Journal recovery", &error));
    g_assert_no_error(error);

    module = gsw_manager_get_module(manager, "Journal");
    tree = (GswTreeKey *)gsw_module_create_key(module);
    g_assert_true(gsw_tree_key_append_child(tree, "Recovered", &error));
    g_assert_no_error(error);
    set_module_tree_key(module, tree);
    g_assert_true(gsw_module_set_entry(module, "Recovered entry", &error));
    g_assert_no_error(error);
    gsw_key_unref(tree);
    g_object_unref(manager);

    base = g_build_filename(prefix, "modules", "genbook", "rawgenbook",
                            "Journal", "Journal", NULL);
    dat_path = g_strconcat(base, ".dat", NULL);
    idx_path = g_strconcat(base, ".idx", NULL);
    dat_new = g_strconcat(base, ".dat.new", NULL);
    idx_new = g_strconcat(base, ".idx.new", NULL);
    dat_bak = g_strconcat(base, ".dat.bak", NULL);
    idx_bak = g_strconcat(base, ".idx.bak", NULL);
    txn_path = g_strconcat(base, ".txn", NULL);

    g_assert_true(g_file_get_contents(dat_path, &dat, &dat_len, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_get_contents(idx_path, &idx, &idx_len, &error));
    g_assert_no_error(error);

    /* Simulate a crash after both new files were written and the old files
     * were moved aside, but before either new file was installed. */
    g_assert_true(g_file_set_contents(dat_new, dat, (gssize)dat_len, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_set_contents(idx_new, idx, (gssize)idx_len, &error));
    g_assert_no_error(error);
    g_assert_cmpint(g_rename(dat_path, dat_bak), ==, 0);
    g_assert_cmpint(g_rename(idx_path, idx_bak), ==, 0);
    g_assert_true(g_file_set_contents(txn_path, "gsword-tree-txn-1\n", -1,
                                      &error));
    g_assert_no_error(error);

    manager = gsw_manager_new_for_path(prefix, &error);
    g_assert_no_error(error);
    module = gsw_manager_get_module(manager, "Journal");
    g_assert_nonnull(module);
    tree = (GswTreeKey *)gsw_module_create_key(module);
    g_assert_true(gsw_tree_key_set_text(tree, "Recovered"));
    set_module_tree_key(module, tree);
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "Recovered entry");
    g_free(text);
    gsw_key_unref(tree);
    g_object_unref(manager);

    g_assert_false(g_file_test(txn_path, G_FILE_TEST_EXISTS));
    g_assert_false(g_file_test(dat_new, G_FILE_TEST_EXISTS));
    g_assert_false(g_file_test(idx_new, G_FILE_TEST_EXISTS));
    g_assert_false(g_file_test(dat_bak, G_FILE_TEST_EXISTS));
    g_assert_false(g_file_test(idx_bak, G_FILE_TEST_EXISTS));

    /* Simulate a crash after one side of the pair was moved/replaced.
     * Recovery must restore a consistent .dat/.idx pair. */
    g_free(dat);
    dat = NULL;
    g_free(idx);
    idx = NULL;
    g_assert_true(g_file_get_contents(dat_path, &dat, &dat_len, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_get_contents(idx_path, &idx, &idx_len, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_set_contents(idx_new, idx, (gssize)idx_len, &error));
    g_assert_no_error(error);
    g_assert_cmpint(g_rename(idx_path, idx_bak), ==, 0);
    g_assert_true(g_file_set_contents(txn_path, "gsword-tree-txn-1\n", -1,
                                      &error));
    g_assert_no_error(error);

    manager = gsw_manager_new_for_path(prefix, &error);
    g_assert_no_error(error);
    module = gsw_manager_get_module(manager, "Journal");
    g_assert_nonnull(module);
    tree = (GswTreeKey *)gsw_module_create_key(module);
    g_assert_true(gsw_tree_key_set_text(tree, "Recovered"));
    set_module_tree_key(module, tree);
    text = gsw_module_get_text(module, &error);
    g_assert_no_error(error);
    g_assert_cmpstr(text, ==, "Recovered entry");
    g_free(text);

    g_assert_false(g_file_test(txn_path, G_FILE_TEST_EXISTS));
    g_assert_false(g_file_test(idx_new, G_FILE_TEST_EXISTS));
    g_assert_false(g_file_test(idx_bak, G_FILE_TEST_EXISTS));

    gsw_key_unref(tree);
    g_object_unref(manager);
    g_free(dat);
    g_free(idx);
    g_free(base);
    g_free(dat_path);
    g_free(idx_path);
    g_free(dat_new);
    g_free(idx_new);
    g_free(dat_bak);
    g_free(idx_bak);
    g_free(txn_path);
    gsw_dir_remove_recursive(prefix, NULL);
    g_free(prefix);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/genbook/write", test_genbook_write);
    g_test_add_func("/genbook/recovery", test_genbook_recovery);
    return g_test_run();
}
