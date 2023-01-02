/* vi: set sw=4 ts=4 wrap ai: */
/*
 * test.c: This file is part of ____
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
#include <stdio.h>
#include <locale.h>
#include "gsw-manager.h"
#include "gsw-modinfo.h"

int main(int argc, char** argv)
{
	g_setenv("LC_ALL", "zh_CN.utf8", TRUE);
	setlocale (LC_ALL, "");
	GswManager* mag;
	GList *list, *l;
	mag = gsw_manager_new ();
	g_print("mag version=%s\n", gsw_manager_get_version(mag));
	list = gsw_manager_get_modinfo_list(mag);
	for (l = list; l != NULL; l = l->next)
	{
		GswModinfo *info = (GswModinfo*)l->data;
		g_print("modinfo:name=%s\n", gsw_modinfo_get_name(info));
		g_print("       :desc=%s\n", gsw_modinfo_get_description(info));
		g_print("       :cate=%s\n", gsw_modinfo_get_category(info));
		g_print("       :lang=%s\n", gsw_modinfo_get_language(info));
		g_print("       :vern=%s\n", gsw_modinfo_get_version(info));
		g_print("       :delt=%s\n", gsw_modinfo_get_delta(info));
		// do something with l->data
	}
	gsw_manager_delete(mag);

}
