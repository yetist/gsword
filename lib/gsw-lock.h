/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-lock.h: small cross-process file lock helper
 */

#ifndef __GSW_LOCK_H__
#define __GSW_LOCK_H__ 1

#include <glib.h>

G_BEGIN_DECLS

gint gsw_file_lock(const gchar *path, GError **error);
void gsw_file_unlock(gint fd);

G_END_DECLS

#endif /* __GSW_LOCK_H__ */
