/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include <gsword/gsword.h>

static guint progress_count = 0;
static guint pre_status_count = 0;
static guint64 last_total = 0;
static guint64 last_done = 0;

static void
on_progress(GswStatusReporter *reporter, guint64 total, guint64 done,
            gpointer user_data)
{
    (void)reporter;
    (void)user_data;
    progress_count++;
    last_total = total;
    last_done = done;
}

static void
on_pre_status(GswStatusReporter *reporter, guint64 total, guint64 done,
              const gchar *message, gpointer user_data)
{
    (void)reporter;
    (void)total;
    (void)done;
    (void)user_data;
    g_assert_cmpstr(message, ==, "Downloading");
    pre_status_count++;
}

static void
test_signals(void)
{
    GswStatusReporter *reporter = gsw_status_reporter_new();

    g_signal_connect(reporter, "progress", G_CALLBACK(on_progress), NULL);
    g_signal_connect(reporter, "pre-status", G_CALLBACK(on_pre_status), NULL);

    gsw_status_reporter_emit_progress(reporter, 100, 42);
    gsw_status_reporter_emit_pre_status(reporter, 100, 42, "Downloading");

    g_assert_cmpuint(progress_count, ==, 1);
    g_assert_cmpuint(pre_status_count, ==, 1);
    g_assert_cmpuint(last_total, ==, 100);
    g_assert_cmpuint(last_done, ==, 42);

    g_object_unref(reporter);
}

static void
test_main_context_dispatch(void)
{
    GMainContext *context = g_main_context_new();
    GswStatusReporter *reporter = gsw_status_reporter_new();

    progress_count = 0;
    pre_status_count = 0;
    g_signal_connect(reporter, "progress", G_CALLBACK(on_progress), NULL);
    g_signal_connect(reporter, "pre-status", G_CALLBACK(on_pre_status), NULL);
    gsw_status_reporter_set_main_context(reporter, context);

    gsw_status_reporter_emit_progress(reporter, 100, 42);
    gsw_status_reporter_emit_pre_status(reporter, 100, 42, "Downloading");

    g_assert_cmpuint(progress_count, ==, 0);
    g_assert_cmpuint(pre_status_count, ==, 0);
    while (g_main_context_iteration(context, FALSE))
        ;
    g_assert_cmpuint(progress_count, ==, 1);
    g_assert_cmpuint(pre_status_count, ==, 1);
    g_assert_cmpuint(last_total, ==, 100);
    g_assert_cmpuint(last_done, ==, 42);

    g_object_unref(reporter);
    g_main_context_unref(context);
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/status/signals", test_signals);
    g_test_add_func("/status/main-context-dispatch", test_main_context_dispatch);
    return g_test_run();
}
