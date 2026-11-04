/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <gsword/gsword.h>

typedef struct {
    GMainLoop *loop;
    GswInstallManager *installer;
    GswManager *destination;
    GswInstallSource *source;
    int exit_code;
} AsyncState;

static gboolean
on_timeout(gpointer data)
{
    AsyncState *state = data;
    state->exit_code = 1;
    g_main_loop_quit(state->loop);
    return G_SOURCE_REMOVE;
}

static void
install_cb(GObject *source, GAsyncResult *result, gpointer user_data)
{
    AsyncState *state = user_data;
    GError *error = NULL;

    if (!gsw_install_manager_install_module_finish(
            GSW_INSTALL_MANAGER(source), result, &error)) {
        g_printerr("async install: %s\n", error->message);
        g_error_free(error);
        state->exit_code = 1;
    } else if (!gsw_manager_has_module(state->destination, "Tiny")) {
        g_printerr("module missing after async install\n");
        state->exit_code = 1;
    }
    g_main_loop_quit(state->loop);
}

static void
refresh_cb(GObject *source, GAsyncResult *result, gpointer user_data)
{
    AsyncState *state = user_data;
    GError *error = NULL;

    if (!gsw_install_manager_refresh_remote_source_finish(
            GSW_INSTALL_MANAGER(source), result, &error)) {
        g_printerr("async refresh: %s\n", error->message);
        g_error_free(error);
        state->exit_code = 1;
        g_main_loop_quit(state->loop);
        return;
    }

    gsw_install_manager_install_module_async(state->installer, state->destination,
                                             state->source, "Tiny", NULL,
                                             install_cb, state);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    AsyncState state = { 0 };
    guint timeout_id;

    if (argc != 5) {
        g_printerr("usage: %s <private> <dest> <host:port> <directory>\n", argv[0]);
        return 2;
    }

    state.installer = gsw_install_manager_new(argv[1], NULL, &error);
    if (state.installer == NULL) {
        g_printerr("install manager: %s\n", error->message);
        return 1;
    }
    gsw_install_manager_set_user_disclaimer_confirmed(state.installer, TRUE);

    state.source = gsw_install_source_new(GSW_SOURCE_HTTP, "LocalAsync", argv[3],
                                          argv[4], "", "", "local-async");
    if (state.source == NULL) {
        g_printerr("source creation failed\n");
        return 1;
    }
    gsw_install_manager_add_source(state.installer, state.source);

    state.destination = gsw_manager_new_for_path(argv[2], &error);
    if (state.destination == NULL) {
        g_printerr("destination: %s\n", error->message);
        return 1;
    }

    state.loop = g_main_loop_new(NULL, FALSE);
    gsw_install_manager_refresh_remote_source_async(
        state.installer, state.source, NULL, refresh_cb, &state);
    timeout_id = g_timeout_add_seconds(30, on_timeout, &state);
    g_main_loop_run(state.loop);
    g_source_remove(timeout_id);

    g_main_loop_unref(state.loop);
    g_object_unref(state.destination);
    g_object_unref(state.source);
    g_object_unref(state.installer);
    return state.exit_code;
}
