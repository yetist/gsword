/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-status-reporter.h: This file is part of gsword.
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

#ifndef __GSW_STATUS_REPORTER_H__ 
#define __GSW_STATUS_REPORTER_H__  1

#include <glib-object.h>

G_BEGIN_DECLS

#define GSW_TYPE_STATUS_REPORTER              (gsw_status_reporter_get_type ())

G_DECLARE_DERIVABLE_TYPE (GswStatusReporter, gsw_status_reporter, GSW, STATUS_REPORTER, GObject)

typedef struct _GswStatusReporterClass        GswStatusReporterClass;

struct _GswStatusReporterClass
{
	GObjectClass       parent_class;
	void (*updating)   (GswStatusReporter *reporter, gulong total, gulong completed);
	void (*pre_update) (GswStatusReporter *reporter, gulong total, gulong completed, const gchar* message);
};

GswStatusReporter*  gsw_status_reporter_new          (void);

G_END_DECLS

#endif /* __GSW_STATUS_REPORTER_H__ */
