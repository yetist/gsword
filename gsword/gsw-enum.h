/* vi: set sw=4 ts=4 wrap ai: */
/*
 * gsw-enum.h: This file is part of gsword.
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

#ifndef __GSW_ENUM_H__ 
#define __GSW_ENUM_H__  1

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
    GSW_POSITION_UNKNOWN,
    GSW_POSITION_TOP,
    GSW_POSITION_BOTTOM,
} GswPosition;

typedef enum {
    GSW_MODTYPE_UNKNOWN,
	GSW_MODTYPE_BIBLES,
	GSW_MODTYPE_COMMENTARIES,
	GSW_MODTYPE_LEXDICTS,
	GSW_MODTYPE_GENBOOKS,
	GSW_MODTYPE_DAILYDEVOS
} GSWModType;

G_END_DECLS

#endif /* __GSW_ENUM_H__ */
