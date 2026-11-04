/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include <gsword/gsword.h>
#include <stdio.h>

typedef struct {
    GMainLoop *loop;
    GswInstallManager *installer;
    GswManager *manager;
    GswInstallSource *source;
    const gchar *module;
    int exit_code;
} AsyncState;

static void
on_pre_status(GswStatusReporter *reporter, guint64 total, guint64 done,
              const gchar *message, gpointer user_data)
{
    (void)reporter;
    (void)user_data;
    printf("[%" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT "] %s\n",
           done, total, message);
}

static void
on_progress(GswStatusReporter *reporter, guint64 total, guint64 done,
            gpointer user_data)
{
    (void)reporter;
    (void)user_data;
    if (total > 0)
        printf("\r  %3" G_GUINT64_FORMAT "%% (%" G_GUINT64_FORMAT
               "/%" G_GUINT64_FORMAT ")", done * 100 / total, done, total);
    else
        printf("\r  %" G_GUINT64_FORMAT " bytes", done);
    fflush(stdout);
}

static gboolean
on_timeout(gpointer data)
{
    AsyncState *state = data;
    fprintf(stderr, "\noperation timed out\n");
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
        fprintf(stderr, "\ninstall failed: %s\n", error->message);
        g_error_free(error);
        state->exit_code = 1;
    } else {
        printf("\nInstalled %s\n", state->module);
        state->exit_code = 0;
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
        fprintf(stderr, "\nrefresh failed: %s\n", error->message);
        g_error_free(error);
        state->exit_code = 1;
        g_main_loop_quit(state->loop);
        return;
    }

    printf("Refreshed; installing %s...\n", state->module);
    gsw_install_manager_install_module_async(state->installer, state->manager,
                                             state->source, state->module,
                                             NULL, install_cb, state);
}

int
main(int argc, char **argv)
{
    GError *error = NULL;
    AsyncState state = { 0 };
    GswStatusReporter *reporter;
    guint timeout_id;

    if (argc != 5) {
        fprintf(stderr,
                "usage: %s <library-path> <installmgr-path> <source-caption> <module>\n",
                argv[0]);
        return 2;
    }

    state.manager = gsw_manager_new_for_path(argv[1], &error);
    if (state.manager == NULL) {
        fprintf(stderr, "cannot open library: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    state.installer = gsw_install_manager_new(argv[2], NULL, &error);
    if (state.installer == NULL) {
        fprintf(stderr, "cannot open installer: %s\n", error->message);
        g_error_free(error);
        g_object_unref(state.manager);
        return 1;
    }

    state.source = gsw_install_manager_lookup_source(state.installer, argv[3]);
    if (state.source == NULL) {
        fprintf(stderr, "source not found: %s\n", argv[3]);
        g_object_unref(state.installer);
        g_object_unref(state.manager);
        return 1;
    }
    state.module = argv[4];

    gsw_install_manager_set_user_disclaimer_confirmed(state.installer, TRUE);
    reporter = gsw_status_reporter_new();
    g_signal_connect(reporter, "pre-status", G_CALLBACK(on_pre_status), NULL);
    g_signal_connect(reporter, "progress", G_CALLBACK(on_progress), NULL);
    gsw_install_manager_set_status_reporter(state.installer, reporter);

    state.loop = g_main_loop_new(NULL, FALSE);
    gsw_install_manager_refresh_remote_source_async(
        state.installer, state.source, NULL, refresh_cb, &state);
    timeout_id = g_timeout_add_seconds(120, on_timeout, &state);
    g_main_loop_run(state.loop);
    g_source_remove(timeout_id);

    g_main_loop_unref(state.loop);
    g_object_unref(reporter);
    g_object_unref(state.installer);
    g_object_unref(state.manager);
    return state.exit_code;
}
