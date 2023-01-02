/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-status-reporter.c: This file is part of ____
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

#include "gsw-status-reporter.h"

namespace {
	class MyStatusReporter : public StatusReporter {
		public:
			int last;
			GswStatusReporter *statusReporter;
			UpdateCallback updateFunc;
			PreStatusCallback prestatusFunc;
			MyStatusReporter() :
				last(0), statusReporter(0), updateFunc(0), prestatusFunc(0) {
			}
			void init(GswStatusReporter* sr) {
				statusReporter = sr;
				last = 0;
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
					SWBuf output;
					output.setFormatted("[ Total Bytes: %ld; Completed Bytes: %ld", totalBytes, completedBytes);
					while (output.size() < 75) output += " ";
					output += "]";
					last = 0;
				}
			}
	};
}

GswStatusReporter* gsw_status_reporter_new      (void)
{
	return (GswStatusReporter) new MyStatusReporter();
}

void gsw_status_reporter_set_update_callback (GswStatusReporter *report, UpdateCallback func);
{
#define GETSWMGR(handle, failReturn)
	HandleSWMgr *hmgr = (HandleSWMgr *)handle;
	if (!hmgr) return;
	WebMgr *mgr = hmgr->mgr;
	if (!mgr) return failReturn;
}

void gsw_status_reporter_set_prestatus_callback (GswStatusReporter *report, PreStatusCallback func);
