/*
 * gsw-version.h: version macros for gsword
 *
 * This file is part of gsword.
 *
 * gsword is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __GSW_VERSION_H__
#define __GSW_VERSION_H__ 1

#include <glib.h>

G_BEGIN_DECLS

#define GSW_MAJOR_VERSION 0
#define GSW_MINOR_VERSION 1
#define GSW_MICRO_VERSION 0

#define GSW_VERSION_S "0.1.0"

#define GSW_CHECK_VERSION(major, minor, micro) \
    (GSW_MAJOR_VERSION > (major) || \
     (GSW_MAJOR_VERSION == (major) && GSW_MINOR_VERSION > (minor)) || \
     (GSW_MAJOR_VERSION == (major) && GSW_MINOR_VERSION == (minor) && \
      GSW_MICRO_VERSION >= (micro)))

#define GSW_AVAILABLE_IN_0_1

gint gsw_version_compare(const gchar *a, const gchar *b);

G_END_DECLS

#endif /* __GSW_VERSION_H__ */
