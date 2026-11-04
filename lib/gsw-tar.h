/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-tar.h: safe tar.gz extraction
 */

#ifndef __GSW_TAR_H__
#define __GSW_TAR_H__ 1

#include <glib.h>

G_BEGIN_DECLS

gboolean gsw_tar_extract_gz(const gchar *archive_path, const gchar *dest_dir,
                            GError **error);

G_END_DECLS

#endif /* __GSW_TAR_H__ */
