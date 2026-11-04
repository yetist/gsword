/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-versification-data.h: generated KJV tables
 */

#ifndef __GSW_VERSIFICATION_DATA_H__
#define __GSW_VERSIFICATION_DATA_H__ 1

#include <glib.h>

typedef struct {
    const gchar *name;
    const gchar *osis;
    const gchar *abbrev;
    gint chapmax;
} GswV11nBook;

typedef struct {
    const gchar *abbrev;
    const gchar *osis;
} GswV11nAbbrev;

typedef struct {
    const gchar *name;
    const GswV11nBook *ot;
    const GswV11nBook *nt;
    const gint *verse_max;
} GswV11nSystemData;

extern const GswV11nBook gsw_kjv_ot[];
extern const GswV11nBook gsw_kjv_nt[];
extern const gint gsw_kjv_verse_max[];
extern const GswV11nSystemData gsw_v11n_systems[];
extern const GswV11nAbbrev gsw_builtin_abbrevs[];
extern const guint gsw_builtin_abbrevs_count;

#endif /* __GSW_VERSIFICATION_DATA_H__ */
