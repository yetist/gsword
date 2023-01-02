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

#include <remotetrans.h>
#include "gsw-marshal.h"
#include "gsw-status-reporter.h"
#include "gsw-private.h"

enum {
	UPDATING,
	PRE_UPDATE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _GswStatusReporterPrivate        GswStatusReporterPrivate;

static char* str_replace(const gchar* string, const gchar* old, const gchar* newly)
{
	GRegex* regex;
	char* str;

	regex = g_regex_new (old, (GRegexCompileFlags)0, (GRegexMatchFlags)0, NULL);
	str = g_regex_replace(regex, string, -1, 0, newly, (GRegexMatchFlags)0, NULL);
	g_regex_unref (regex);
	return str;
}

class GStatusReporter : public sword::StatusReporter {
	public:
		GswStatusReporter *inst;
		GStatusReporter(GswStatusReporter *instance){
			inst = instance;
		}
		virtual void update(unsigned long totalBytes, unsigned long completedBytes) {
			g_signal_emit (inst, signals[UPDATING], 0, totalBytes, completedBytes);
		}
		virtual void preStatus(long totalBytes, long completedBytes, const char *message) {
			//FIXME: use i18n.
			gchar* msg = str_replace(message, "Downloading", "正在下载");
			g_signal_emit (inst, signals[PRE_UPDATE], 0, totalBytes, completedBytes, msg);
			g_free(msg);
		}
};

struct _GswStatusReporterPrivate
{
	GStatusReporter* reporter;
};

G_DEFINE_TYPE_WITH_PRIVATE (GswStatusReporter, gsw_status_reporter, G_TYPE_OBJECT);

static void gsw_status_reporter_dispose (GObject *object)
{
	GswStatusReporter* reporter = GSW_STATUS_REPORTER(object);
	GswStatusReporterPrivate* priv =(GswStatusReporterPrivate*)  gsw_status_reporter_get_instance_private(reporter);

	if (priv->reporter != NULL)
	{
		delete priv->reporter;
		priv->reporter = NULL;
	}
	G_OBJECT_CLASS (gsw_status_reporter_parent_class)->dispose (object);
}

static void gsw_status_reporter_class_init (GswStatusReporterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->dispose = gsw_status_reporter_dispose;
	//gobject_class->updating = ;
	//gobject_class->pre-update = ;
	//void (*updating)   (GswStatusReporter *reporter, gulong total, gulong completed);
	//void (*pre-update) (GswStatusReporter *reporter, gulong total, gulong completed, const gchar* message);

	signals[UPDATING] =
		g_signal_new ("updating",
				G_TYPE_FROM_CLASS (gobject_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (GswStatusReporterClass, updating),
				NULL, NULL,
				gsw_marshal_VOID__ULONG_ULONG,
				G_TYPE_NONE, 2, G_TYPE_ULONG, G_TYPE_ULONG);

	signals[PRE_UPDATE] =
		g_signal_new ("pre-update",
				G_TYPE_FROM_CLASS (gobject_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (GswStatusReporterClass, pre_update),
				NULL, NULL,
				gsw_marshal_VOID__ULONG_ULONG_STRING,
				G_TYPE_NONE, 3, G_TYPE_ULONG, G_TYPE_ULONG, G_TYPE_STRING);
}

static void gsw_status_reporter_init (GswStatusReporter *reporter)
{
	GswStatusReporterPrivate *priv;

	priv = (GswStatusReporterPrivate*)  gsw_status_reporter_get_instance_private(reporter);
	priv->reporter = NULL;
}

GswStatusReporter* gsw_status_reporter_new (void)
{
	GswStatusReporter* reporter;
	GswStatusReporterPrivate *priv;
	reporter = (GswStatusReporter*) g_object_new (GSW_TYPE_STATUS_REPORTER, NULL);
	priv = (GswStatusReporterPrivate*) gsw_status_reporter_get_instance_private(reporter);
	priv->reporter = new GStatusReporter(reporter);
	return reporter;
}

gpointer gsw_status_reporter_get_internal (GswStatusReporter *reporter)
{
	GswStatusReporterPrivate *priv;
	priv = (GswStatusReporterPrivate*) gsw_status_reporter_get_instance_private(reporter);

	return priv->reporter;
}
