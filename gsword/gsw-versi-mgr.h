/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-versi-mgr.h: This file is part of gsword.
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

#ifndef __GSW_VERSI_MGR_H__
#define __GSW_VERSI_MGR_H__  1

#include <glib-object.h>

G_BEGIN_DECLS

#define GSW_TYPE_VERSI_MGR              (gsw_versi_mgr_get_type ())
#define GSW_TYPE_VERSI_MGR_SYSTEM       (gsw_versi_mgr_system_get_type ())
#define GSW_TYPE_VERSI_MGR_BOOK         (gsw_versi_mgr_book_get_type ())

G_DECLARE_FINAL_TYPE (GswVersiMgr, gsw_versi_mgr, GSW, VERSI_MGR, GObject);
G_DECLARE_FINAL_TYPE (GswVersiMgrSystem, gsw_versi_mgr_system, GSW, VERSI_MGR_SYSTEM, GObject);
G_DECLARE_FINAL_TYPE (GswVersiMgrBook, gsw_versi_mgr_book, GSW, VERSI_MGR_BOOK, GObject);

GswVersiMgr*           gsw_versi_mgr_new                    (void);
GswVersiMgrSystem*     gsw_versi_mgr_get_system             (GswVersiMgr *gvmgr, const gchar* name);

const gchar*           gsw_versi_mgr_system_get_name        (GswVersiMgrSystem* gvmsystem);
GswVersiMgrBook*       gsw_versi_mgr_system_get_book        (GswVersiMgrSystem* gvmsystem, gint number);
gint                   gsw_versi_mgr_system_get_book_count  (GswVersiMgrSystem* gvmsystem);
GArray*                gsw_versi_mgr_system_get_bmax        (GswVersiMgrSystem* gvmsystem);

const gchar*           gsw_versi_mgr_book_get_osis_name     (GswVersiMgrBook *book);

G_END_DECLS

#endif /* __GSW_VERSI_MGR_H__ */
