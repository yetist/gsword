/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-status-reporter.c: download and install progress reporting
 */

#include <gsword/gsw-status-reporter.h>

typedef struct {
    GswStatusReporter *self;
    guint64 total_bytes;
    guint64 completed_bytes;
    gchar *message; /* only for pre-status */
} GswStatusEvent;


typedef struct {
    GMainContext *context;
} GswStatusReporterPrivate;

enum {
    SIGNAL_PRE_STATUS,
    SIGNAL_PROGRESS,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE_WITH_PRIVATE(GswStatusReporter, gsw_status_reporter, G_TYPE_OBJECT)

static void
gsw_status_reporter_finalize(GObject *object)
{
    GswStatusReporter *self = GSW_STATUS_REPORTER(object);
    GswStatusReporterPrivate *priv = gsw_status_reporter_get_instance_private(self);

    g_clear_pointer(&priv->context, g_main_context_unref);

    G_OBJECT_CLASS(gsw_status_reporter_parent_class)->finalize(object);
}

static void
gsw_status_reporter_class_init(GswStatusReporterClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = gsw_status_reporter_finalize;

    signals[SIGNAL_PRE_STATUS] =
        g_signal_new("pre-status",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(GswStatusReporterClass, pre_status),
                     NULL, NULL, NULL,
                     G_TYPE_NONE, 3,
                     G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_STRING);

    signals[SIGNAL_PROGRESS] =
        g_signal_new("progress",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(GswStatusReporterClass, progress),
                     NULL, NULL, NULL,
                     G_TYPE_NONE, 2,
                     G_TYPE_UINT64, G_TYPE_UINT64);
}

static void
gsw_status_reporter_init(GswStatusReporter *self)
{
    (void)self;
}

GswStatusReporter *
gsw_status_reporter_new(void)
{
    return g_object_new(GSW_TYPE_STATUS_REPORTER, NULL);
}

void
gsw_status_reporter_set_main_context(GswStatusReporter *self, GMainContext *context)
{
    GswStatusReporterPrivate *priv;

    g_return_if_fail(GSW_IS_STATUS_REPORTER(self));

    priv = gsw_status_reporter_get_instance_private(self);
    if (priv->context == context)
        return;

    g_clear_pointer(&priv->context, g_main_context_unref);
    if (context != NULL)
        priv->context = g_main_context_ref(context);
}

GMainContext *
gsw_status_reporter_get_main_context(GswStatusReporter *self)
{
    GswStatusReporterPrivate *priv;

    g_return_val_if_fail(GSW_IS_STATUS_REPORTER(self), NULL);
    priv = gsw_status_reporter_get_instance_private(self);
    return priv->context;
}

static gboolean
gsw_status_emit_progress_cb(gpointer data)
{
    GswStatusEvent *event = data;

    g_signal_emit(event->self, signals[SIGNAL_PROGRESS], 0,
                  event->total_bytes, event->completed_bytes);
    return G_SOURCE_REMOVE;
}

static gboolean
gsw_status_emit_pre_status_cb(gpointer data)
{
    GswStatusEvent *event = data;

    g_signal_emit(event->self, signals[SIGNAL_PRE_STATUS], 0,
                  event->total_bytes, event->completed_bytes,
                  event->message != NULL ? event->message : "");
    return G_SOURCE_REMOVE;
}

static void
gsw_status_event_free(gpointer data)
{
    GswStatusEvent *event = data;

    if (event == NULL)
        return;
    g_clear_object(&event->self);
    g_free(event->message);
    g_free(event);
}

void
gsw_status_reporter_emit_progress(GswStatusReporter *self,
                                  guint64 total_bytes,
                                  guint64 completed_bytes)
{
    GswStatusReporterPrivate *priv;

    g_return_if_fail(GSW_IS_STATUS_REPORTER(self));

    priv = gsw_status_reporter_get_instance_private(self);
    if (priv->context == NULL) {
        g_signal_emit(self, signals[SIGNAL_PROGRESS], 0,
                      total_bytes, completed_bytes);
        return;
    }

    {
        GswStatusEvent *event = g_new0(GswStatusEvent, 1);
        event->self = g_object_ref(self);
        event->total_bytes = total_bytes;
        event->completed_bytes = completed_bytes;
        g_main_context_invoke_full(priv->context, G_PRIORITY_DEFAULT,
                                   gsw_status_emit_progress_cb, event,
                                   gsw_status_event_free);
    }
}

void
gsw_status_reporter_emit_pre_status(GswStatusReporter *self,
                                    guint64 total_bytes,
                                    guint64 completed_bytes,
                                    const gchar *message)
{
    GswStatusReporterPrivate *priv;

    g_return_if_fail(GSW_IS_STATUS_REPORTER(self));

    priv = gsw_status_reporter_get_instance_private(self);
    if (priv->context == NULL) {
        g_signal_emit(self, signals[SIGNAL_PRE_STATUS], 0,
                      total_bytes, completed_bytes,
                      message != NULL ? message : "");
        return;
    }

    {
        GswStatusEvent *event = g_new0(GswStatusEvent, 1);
        event->self = g_object_ref(self);
        event->total_bytes = total_bytes;
        event->completed_bytes = completed_bytes;
        event->message = g_strdup(message != NULL ? message : "");
        g_main_context_invoke_full(priv->context, G_PRIORITY_DEFAULT,
                                   gsw_status_emit_pre_status_cb, event,
                                   gsw_status_event_free);
    }
}
