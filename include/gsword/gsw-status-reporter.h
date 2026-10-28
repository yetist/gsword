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

#define GSW_TYPE_STATUS_REPORTER (gsw_status_reporter_get_type())
G_DECLARE_DERIVABLE_TYPE(GswStatusReporter, gsw_status_reporter, GSW,
                         STATUS_REPORTER, GObject)

struct _GswStatusReporterClass {
    GObjectClass parent_class;

    void (*pre_status)(GswStatusReporter *self, guint64 total_bytes,
                       guint64 completed_bytes, const gchar *message);
    void (*progress)(GswStatusReporter *self, guint64 total_bytes,
                     guint64 completed_bytes);
};

GswStatusReporter *gsw_status_reporter_new(void);

void         gsw_status_reporter_set_main_context(GswStatusReporter *self,
                                                  GMainContext *context);
GMainContext *gsw_status_reporter_get_main_context(GswStatusReporter *self);

void gsw_status_reporter_emit_pre_status(GswStatusReporter *self,
                                         guint64 total_bytes,
                                         guint64 completed_bytes,
                                         const gchar *message);
void gsw_status_reporter_emit_progress(GswStatusReporter *self,
                                       guint64 total_bytes,
                                       guint64 completed_bytes);

G_END_DECLS

#endif /* __GSW_STATUS_REPORTER_H__ */
