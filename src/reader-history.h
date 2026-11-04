/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * reader-history.h: recent Bible reading history
 */

#ifndef GSWORD_READER_HISTORY_H
#define GSWORD_READER_HISTORY_H 1

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct {
    gchar *module;
    gchar *reference;
    gchar *display;
    gint64 visited_at;
} GswordReaderHistoryEntry;

typedef struct _GswordReaderHistory GswordReaderHistory;

GswordReaderHistory *gsword_reader_history_new(GSettings *settings,
                                               guint max_entries);
void gsword_reader_history_free(GswordReaderHistory *history);

void gsword_reader_history_push(GswordReaderHistory *history,
                                const gchar *module,
                                const gchar *reference,
                                const gchar *display);
gboolean gsword_reader_history_can_back(GswordReaderHistory *history);
gboolean gsword_reader_history_can_forward(GswordReaderHistory *history);
const GswordReaderHistoryEntry *gsword_reader_history_back(
    GswordReaderHistory *history);
const GswordReaderHistoryEntry *gsword_reader_history_forward(
    GswordReaderHistory *history);
const GswordReaderHistoryEntry *gsword_reader_history_get_entry(
    GswordReaderHistory *history, guint index);
const GswordReaderHistoryEntry *gsword_reader_history_get_current(
    GswordReaderHistory *history);
guint gsword_reader_history_get_length(GswordReaderHistory *history);
gint gsword_reader_history_get_current_index(GswordReaderHistory *history);
void gsword_reader_history_set_current(GswordReaderHistory *history,
                                       guint index);
void gsword_reader_history_clear(GswordReaderHistory *history);

G_END_DECLS

#endif /* GSWORD_READER_HISTORY_H */
