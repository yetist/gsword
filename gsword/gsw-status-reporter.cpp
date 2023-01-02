/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-status-reporter.c: This file is part of gsword.
 *
 * Copyright (C) 2017 yetist <yetist@gmail.com>
 *
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * */

#include <swbuf.h>
#include <remotetrans.h>
#include "gsw-status-reporter.h"

enum {
	UPDATING,
	PRE_UPDATE,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _GswStatusReporterPrivate        GswStatusReporterPrivate;

struct _GswStatusReporterPrivate
{
	GStatusReporter* reporter;
};

G_DEFINE_TYPE_WITH_PRIVATE (GswStatusReporter, gsw_status_reporter, G_TYPE_OBJECT);


static void gsw_status_reporter_dispose (GObject *object)
{
	GswStatusReporter* reporter = GSW_STATUS_REPORTER(object);
	GswStatusReporterPrivate* priv =(GswStatusReporter*)  gsw_status_reporter_get_instance_private(reporter);

	if (priv->reporter != NULL)
	{
		delete priv->reporter;
		priv->reporter = NULL;
	}
	G_OBJECT_CLASS (gsw_status_reporter_parent_class)->dispose (object);
}

static void gsw_status_reporter_class_init (GswStatusReporterClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    //gobject_class->updating = ;
    //gobject_class->pre-update = ;
	//void (*updating)   (GswStatusReporter *reporter, gulong total, gulong completed);
	//void (*pre-update) (GswStatusReporter *reporter, gulong total, gulong completed, const gchar* message);

	// TODO:
	signals[UPDATING] =
		g_signal_new ("updating",
				G_TYPE_FROM_CLASS (gobject_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (GswStatusReporterClass, updating),
				NULL, NULL,
				g_cclosure_marshal_VOID__UINT,
				G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void
gsw_status_reporter_init (GswStatusReporter *status_reporter)
{
    GswStatusReporterPrivate *priv;

    priv = GSW_STATUS_REPORTER_GET_PRIVATE (status_reporter);
	priv->reporter = NULL;

}

GswStatusReporter* gsw_status_reporter_new (void)
{
	GswStatusReporter* reporter;
	GswStatusReporterPrivate *priv;
    reporter = g_object_new (GSW_TYPE_STATUS_REPORTER, NULL);
	priv = gsw_status_reporter_get_instance_private(reporter);
	priv->reporter = new GStatusReporter(reporter);
	return reporter;
}

gpointer gsw_status_reporter_get_class  (GswStatusReporter *report)
{
	GswStatusReporterPrivate *priv;
	priv = gsw_status_reporter_get_instance_private(report);

	return priv->reporter;
}

namespace {
	using namespace std;
	using namespace sword;
	class GStatusReporter : public sword::StatusReporter {
		public:
			int last;
			UpdateCallback updateFunc;
			PreStatusCallback prestatusFunc;
			GswStatusReporter *inst;
			GStatusReporter(GswStatusReporter *instance):
				last(0), updateFunc(0), prestatusFunc(0) {
					inst = instance;
				}
			virtual void update(unsigned long totalBytes, unsigned long completedBytes) {
				if (updateFunc != NULL) {
					updateFunc(totalBytes, completedBytes);
				} else {
					int p = (totalBytes > 0) ? (int)(74.0 * ((double)completedBytes / (double)totalBytes)) : 0;
					for (;last < p; ++last) {
						if (!last) {
							SWBuf output;
							output.setFormatted("[ File Bytes: %ld", totalBytes);
							while (output.size() < 75) output += " ";
							output += "]";
						}
					}
				}
			}
			virtual void preStatus(long totalBytes, long completedBytes, const char *message) {
				if (prestatusFunc != NULL) {
					prestatusFunc(totalBytes, completedBytes, message);
				} else {
					sword::SWBuf output;
					output.setFormatted("[ Total Bytes: %ld; Completed Bytes: %ld", totalBytes, completedBytes);
					while (output.size() < 75) output += " ";
					output += "]";
					last = 0;
				}
			}
	};

	class HandleReporter {
		public:
			GStatusReporter *reporter;
			HandleReporter() {
				this->reporter = new GStatusReporter();
			}
			~HandleReporter() {
				delete reporter;
			}
	};
};

GswStatusReporter* gsw_status_reporter_new (void)
{
	return (GswStatusReporter*) new HandleReporter();
}

gpointer gsw_status_reporter_get_class  (GswStatusReporter *report)
{
	HandleReporter *h_reporter;
	GStatusReporter *reporter;

	h_reporter = (HandleReporter*) report;
	if (!h_reporter) {
		return NULL;
	}
	reporter = h_reporter->reporter;
	return reporter;
}

void gsw_status_reporter_set_update_callback (GswStatusReporter *report, UpdateCallback func)
{
	HandleReporter *h_reporter;
	GStatusReporter *reporter;

	h_reporter = (HandleReporter*) report;
	if (!h_reporter) {
		return;
	}
	reporter = h_reporter->reporter;
	if (!reporter) {
		return;
	}
	reporter->updateFunc = func;
}

void gsw_status_reporter_set_prestatus_callback (GswStatusReporter *report, PreStatusCallback func)
{
	HandleReporter *h_reporter;
	GStatusReporter *reporter;

	h_reporter = (HandleReporter*) report;
	if (!h_reporter) {
		return;
	}
	reporter = h_reporter->reporter;
	if (!reporter) {
		return;
	}
	reporter->prestatusFunc = func;
}
