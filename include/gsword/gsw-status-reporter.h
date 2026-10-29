/*
 * gsw-status-reporter.h: download and install progress reporting
 *
 * This file is part of gsword.
 */

#ifndef __GSW_STATUS_REPORTER_H__
#define __GSW_STATUS_REPORTER_H__ 1

#include <glib-object.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

/**
 * GswStatusReporter:
 *
 * Dispatches download and install progress notifications. The
 * <signal>pre-status</signal> signal describes a new operation, while
 * <signal>progress</signal> reports transferred byte counts. Signals are
 * emitted on a configurable #GMainContext.
 */
#define GSW_TYPE_STATUS_REPORTER (gsw_status_reporter_get_type())
G_DECLARE_DERIVABLE_TYPE(GswStatusReporter, gsw_status_reporter, GSW,
                         STATUS_REPORTER, GObject)

/**
 * GswStatusReporterClass:
 * @parent_class: the parent class
 * @pre_status: class handler for the <signal>pre-status</signal> signal
 * @progress: class handler for the <signal>progress</signal> signal
 *
 * Class structure for #GswStatusReporter. Subclasses can override these
 * handlers to receive progress notifications without connecting to the
 * public signals.
 */
struct _GswStatusReporterClass {
    GObjectClass parent_class;

    void (*pre_status)(GswStatusReporter *self, guint64 total_bytes,
                       guint64 completed_bytes, const gchar *message);
    void (*progress)(GswStatusReporter *self, guint64 total_bytes,
                     guint64 completed_bytes);
};

/**
 * gsw_status_reporter_new:
 *
 * Creates a status reporter.
 *
 * Returns: (transfer full): a new #GswStatusReporter
 */
GswStatusReporter *gsw_status_reporter_new(void);

/**
 * gsw_status_reporter_set_main_context:
 * @self: a #GswStatusReporter
 * @context: a #GMainContext, or %NULL for the default
 *
 * Sets the main context used to dispatch signals.
 */
void         gsw_status_reporter_set_main_context(GswStatusReporter *self,
                                                  GMainContext *context);
/**
 * gsw_status_reporter_get_main_context:
 * @self: a #GswStatusReporter
 *
 * Gets the dispatch main context.
 *
 * Returns: (transfer none): the dispatch main context
 */
GMainContext *gsw_status_reporter_get_main_context(GswStatusReporter *self);

/**
 * gsw_status_reporter_emit_pre_status:
 * @self: a #GswStatusReporter
 * @total_bytes: total number of bytes to transfer
 * @completed_bytes: number of bytes transferred so far
 * @message: human-readable status message
 *
 * Emits the <signal>pre-status</signal> signal.
 */
void gsw_status_reporter_emit_pre_status(GswStatusReporter *self,
                                         guint64 total_bytes,
                                         guint64 completed_bytes,
                                         const gchar *message);
/**
 * gsw_status_reporter_emit_progress:
 * @self: a #GswStatusReporter
 * @total_bytes: total number of bytes to transfer
 * @completed_bytes: number of bytes transferred so far
 *
 * Emits the <signal>progress</signal> signal.
 */
void gsw_status_reporter_emit_progress(GswStatusReporter *self,
                                       guint64 total_bytes,
                                       guint64 completed_bytes);

G_END_DECLS

#endif /* __GSW_STATUS_REPORTER_H__ */
