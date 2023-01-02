/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-versi-mgr-system.c: This file is part of ____
 *
 * Copyright (C) 2017 yetist <yetist@yetipc>
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
#include <versificationmgr.h>
#include "gsw-versi-mgr.h"
#include "gsw-private.h"

typedef struct _GswVersiMgr GswVersiMgr;
typedef struct _GswVersiMgrSystem GswVersiMgrSystem;
typedef struct _GswVersiMgrBook GswVersiMgrBook;

struct _GswVersiMgr
{
  GObject      object;
  sword::VersificationMgr *mgr;
};

struct _GswVersiMgrSystem
{
  GObject      object;
  sword::VersificationMgr::System *system;
};

struct _GswVersiMgrBook
{
  GObject      object;
  sword::VersificationMgr::Book *book;
};

G_DEFINE_TYPE (GswVersiMgr, gsw_versi_mgr, G_TYPE_OBJECT);
G_DEFINE_TYPE (GswVersiMgrSystem, gsw_versi_mgr_system, G_TYPE_OBJECT);
G_DEFINE_TYPE (GswVersiMgrBook, gsw_versi_mgr_book, G_TYPE_OBJECT);

static void gsw_versi_mgr_class_init (GswVersiMgrClass *klass)
{
}

static void gsw_versi_mgr_init (GswVersiMgr *versi_mgr)
{
	versi_mgr->mgr = sword::VersificationMgr::getSystemVersificationMgr();
}

GswVersiMgr* gsw_versi_mgr_new (void)
{
    return (GswVersiMgr*)g_object_new (GSW_TYPE_VERSI_MGR, NULL);
}

GswVersiMgrSystem* gsw_versi_mgr_get_system (GswVersiMgr *gvmgr, const gchar* name)
{
	GswVersiMgrSystem *system;
	const sword::VersificationMgr::System *refSys;

	refSys = gvmgr->mgr->getVersificationSystem(name);
	system = (GswVersiMgrSystem*) gsw_versi_mgr_system_new ((gpointer)refSys);
	return system;
}

// Book
static void gsw_versi_mgr_book_class_init (GswVersiMgrBookClass *klass)
{
}

static void gsw_versi_mgr_book_init (GswVersiMgrBook *versi_mgr_book)
{
	//versi_mgr_book->mgr = sword::VersificationMgr::getBookVersificationMgr();
}

GswVersiMgrBook* gsw_versi_mgr_book_new (gpointer book)
{
	GswVersiMgrBook *gvmbook;
	gvmbook = (GswVersiMgrBook *) g_object_new (GSW_TYPE_VERSI_MGR_SYSTEM, NULL);
	gvmbook->book = (sword::VersificationMgr::Book*) book;
	return gvmbook;
}

const gchar* gsw_versi_mgr_book_get_osis_name (GswVersiMgrBook *book)
{
	return book->book->getOSISName();
}

// System
static void gsw_versi_mgr_system_class_init (GswVersiMgrSystemClass *klass)
{
}

static void gsw_versi_mgr_system_init (GswVersiMgrSystem *versi_mgr_system)
{
}

GswVersiMgrSystem* gsw_versi_mgr_system_new (gpointer system)
{
	GswVersiMgrSystem *gvmsystem;
	gvmsystem = (GswVersiMgrSystem *) g_object_new (GSW_TYPE_VERSI_MGR_SYSTEM, NULL);
	gvmsystem->system = (sword::VersificationMgr::System*) system;
	return gvmsystem;
}

const gchar* gsw_versi_mgr_system_get_name (GswVersiMgrSystem* gvmsystem)
{
	return gvmsystem->system->getName();
}

GswVersiMgrBook* gsw_versi_mgr_system_get_book (GswVersiMgrSystem* gvmsystem, gint number)
{
	sword::VersificationMgr::Book *book;
	book = (sword::VersificationMgr::Book *)gvmsystem->system->getBook(number);
	GswVersiMgrBook *gswbook;
	gswbook = gsw_versi_mgr_book_new(book);
	return gswbook;
}

gint gsw_versi_mgr_system_get_book_count  (GswVersiMgrSystem* gvmsystem)
{
	return gvmsystem->system->getBookCount();
}

GArray* gsw_versi_mgr_system_get_bmax (GswVersiMgrSystem* gvmsystem)
{
	GArray *array;
	const int *n;

	array = g_array_new (FALSE, FALSE, sizeof (gint));
	n = gvmsystem->system->getBMAX();
	g_array_append_val (array, n[0]);
	g_array_append_val (array, n[1]);
	return array;
}
