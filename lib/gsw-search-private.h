/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-search-private.h: internal search index helpers
 */

#ifndef __GSW_SEARCH_PRIVATE_H__
#define __GSW_SEARCH_PRIVATE_H__ 1

#include <glib.h>

G_BEGIN_DECLS

void gsw_search_index_remove_for_tree_path(const gchar *tree_path);

G_END_DECLS

#endif /* __GSW_SEARCH_PRIVATE_H__ */
