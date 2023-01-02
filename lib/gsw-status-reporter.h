/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-status-reporter.h: This file is part of ____
 *
 * Copyright (C) 2017 yetist <yetist@yetibook>
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

#include <glib.h>

G_BEGIN_DECLS

typedef void GswStatusReporter;

typedef void  (*UpdateCallback) (gulong total, gulong completed);
typedef void  (*PreStatusCallback) (glong total, glong completed, const gchar* message);

GswStatusReporter* gsw_status_reporter_new      (void);
gpointer gsw_status_reporter_get_class  (GswStatusReporter *report);
void gsw_status_reporter_set_update_callback    (GswStatusReporter *report, UpdateCallback func);
void gsw_status_reporter_set_prestatus_callback (GswStatusReporter *report, PreStatusCallback func);

G_END_DECLS

#endif /* __GSW_STATUS_REPORTER_H__ */
