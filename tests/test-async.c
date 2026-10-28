#include <gsword/gsword.h>
#include "gsw-utils.h"
#include <glib/gstdio.h>

typedef struct {
    GMainLoop *loop;
    gboolean called;
    gboolean result;
    GError *error;
} AsyncState;

static gboolean
on_timeout(gpointer data)
{
    AsyncState *state = data;
    g_main_loop_quit(state->loop);
    return G_SOURCE_REMOVE;
}

static void
refresh_cb(GObject *source, GAsyncResult *result, gpointer user_data)
{
    AsyncState *state = user_data;
    GswInstallManager *manager = GSW_INSTALL_MANAGER(source);

    state->called = TRUE;
    state->result = gsw_install_manager_refresh_remote_source_finish(
        manager, result, &state->error);
    g_main_loop_quit(state->loop);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    gchar *private_path = g_dir_make_tmp("gsword-async-XXXXXX", &error);
    GswInstallManager *manager;
    GswInstallSource *source;
    AsyncState state = { 0 };
    guint timeout_id;

    g_test_init(&argc, &argv, NULL);
    g_assert_no_error(error);
    g_assert_nonnull(private_path);

    manager = gsw_install_manager_new(private_path, &error);
    g_assert_no_error(error);
    gsw_install_manager_set_timeout(manager, 200);
    gsw_install_manager_set_user_disclaimer_confirmed(manager, TRUE);

    source = gsw_install_source_new(GSW_SOURCE_HTTP, "Unreachable",
                                    "127.0.0.1:1", "/", "", "", "unreachable");
    g_assert_nonnull(source);
    gsw_install_manager_add_source(manager, source);

    state.loop = g_main_loop_new(NULL, FALSE);
    gsw_install_manager_refresh_remote_source_async(manager, source, NULL,
                                                    refresh_cb, &state);
    timeout_id = g_timeout_add_seconds(10, on_timeout, &state);
    g_main_loop_run(state.loop);
    g_source_remove(timeout_id);

    g_assert_true(state.called);
    g_assert_false(state.result);
    g_assert_nonnull(state.error);
    g_clear_error(&state.error);

    g_main_loop_unref(state.loop);
    g_object_unref(source);
    g_object_unref(manager);
    gsw_dir_remove_recursive(private_path, NULL);
    g_free(private_path);
    return 0;
}
