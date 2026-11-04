/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * reader-history.c: recent Bible reading history
 */

#include "reader-history.h"

#include <string.h>

#define HISTORY_SETTINGS_KEY "reader-history"
#define HISTORY_INDEX_KEY "reader-history-index"

struct _GswordReaderHistory {
    GPtrArray *entries;
    GSettings *settings;
    gint current;
    guint max_entries;
};

static void
entry_free(gpointer data)
{
    GswordReaderHistoryEntry *entry = data;

    if (entry == NULL)
        return;
    g_free(entry->module);
    g_free(entry->reference);
    g_free(entry->display);
    g_free(entry);
}

static void
save_history(GswordReaderHistory *history)
{
    gchar **values;
    guint i;

    if (history == NULL || history->settings == NULL)
        return;
    values = g_new0(gchar *, history->entries->len + 1);
    for (i = 0; i < history->entries->len; i++) {
        GswordReaderHistoryEntry *entry = g_ptr_array_index(history->entries,
                                                            i);

        values[i] = g_strdup_printf("%s\t%s\t%s\t%" G_GINT64_FORMAT,
                                    entry->module, entry->reference,
                                    entry->display, entry->visited_at);
    }
    g_settings_set_strv(history->settings, HISTORY_SETTINGS_KEY,
                        (const gchar * const *)values);
    g_settings_set_int(history->settings, HISTORY_INDEX_KEY,
                       MAX(history->current, 0));
    g_strfreev(values);
}

static void
load_history(GswordReaderHistory *history)
{
    gchar **values;
    guint i;

    if (history == NULL || history->settings == NULL)
        return;
    values = g_settings_get_strv(history->settings, HISTORY_SETTINGS_KEY);
    for (i = 0; values != NULL && values[i] != NULL; i++) {
        gchar **parts = g_strsplit(values[i], "\t", 4);

        if (parts[0] != NULL && parts[1] != NULL && parts[2] != NULL) {
            GswordReaderHistoryEntry *entry = g_new0(
                GswordReaderHistoryEntry, 1);

            entry->module = g_strdup(parts[0]);
            entry->reference = g_strdup(parts[1]);
            entry->display = g_strdup(parts[2]);
            entry->visited_at = parts[3] != NULL
                              ? g_ascii_strtoll(parts[3], NULL, 10) : 0;
            g_ptr_array_add(history->entries, entry);
        }
        g_strfreev(parts);
    }
    g_strfreev(values);
    while (history->entries->len > history->max_entries)
        g_ptr_array_remove_index(history->entries, 0);
    history->current = g_settings_get_int(history->settings,
                                          HISTORY_INDEX_KEY);
    if (history->entries->len == 0)
        history->current = -1;
    else
        history->current = CLAMP(history->current, 0,
                                 (gint)history->entries->len - 1);
}

GswordReaderHistory *
gsword_reader_history_new(GSettings *settings, guint max_entries)
{
    GswordReaderHistory *history = g_new0(GswordReaderHistory, 1);

    history->settings = settings;
    history->max_entries = max_entries > 0 ? max_entries : 50;
    history->current = -1;
    history->entries = g_ptr_array_new_with_free_func(entry_free);
    load_history(history);
    return history;
}

void
gsword_reader_history_free(GswordReaderHistory *history)
{
    if (history == NULL)
        return;
    g_ptr_array_unref(history->entries);
    g_free(history);
}

void
gsword_reader_history_push(GswordReaderHistory *history, const gchar *module,
                           const gchar *reference, const gchar *display)
{
    GswordReaderHistoryEntry *entry;
    guint i;

    if (history == NULL || module == NULL || *module == '\0' ||
        reference == NULL || *reference == '\0')
        return;

    entry = history->current >= 0
          ? g_ptr_array_index(history->entries, history->current) : NULL;
    if (entry != NULL && g_strcmp0(entry->module, module) == 0 &&
        g_strcmp0(entry->reference, reference) == 0)
        return;

    for (i = history->current + 1; i < history->entries->len; i++)
        g_ptr_array_remove_index(history->entries, history->entries->len - 1);

    if (history->entries->len >= history->max_entries) {
        g_ptr_array_remove_index(history->entries, 0);
        history->current--;
    }

    entry = g_new0(GswordReaderHistoryEntry, 1);
    entry->module = g_strdup(module);
    entry->reference = g_strdup(reference);
    entry->display = g_strdup(display != NULL ? display : reference);
    entry->visited_at = g_get_real_time() / G_USEC_PER_SEC;
    g_ptr_array_add(history->entries, entry);
    history->current = (gint)history->entries->len - 1;
    save_history(history);
}

gboolean
gsword_reader_history_can_back(GswordReaderHistory *history)
{
    return history != NULL && history->current > 0;
}

gboolean
gsword_reader_history_can_forward(GswordReaderHistory *history)
{
    return history != NULL && history->current >= 0 &&
           history->current + 1 < (gint)history->entries->len;
}

const GswordReaderHistoryEntry *
gsword_reader_history_back(GswordReaderHistory *history)
{
    if (!gsword_reader_history_can_back(history))
        return NULL;
    history->current--;
    save_history(history);
    return g_ptr_array_index(history->entries, history->current);
}

const GswordReaderHistoryEntry *
gsword_reader_history_forward(GswordReaderHistory *history)
{
    if (!gsword_reader_history_can_forward(history))
        return NULL;
    history->current++;
    save_history(history);
    return g_ptr_array_index(history->entries, history->current);
}

const GswordReaderHistoryEntry *
gsword_reader_history_get_entry(GswordReaderHistory *history, guint index)
{
    if (history == NULL || index >= history->entries->len)
        return NULL;
    return g_ptr_array_index(history->entries, index);
}

const GswordReaderHistoryEntry *
gsword_reader_history_get_current(GswordReaderHistory *history)
{
    if (history == NULL || history->current < 0 ||
        history->current >= (gint)history->entries->len)
        return NULL;
    return g_ptr_array_index(history->entries, history->current);
}

guint
gsword_reader_history_get_length(GswordReaderHistory *history)
{
    return history != NULL ? history->entries->len : 0;
}

gint
gsword_reader_history_get_current_index(GswordReaderHistory *history)
{
    return history != NULL ? history->current : -1;
}

void
gsword_reader_history_set_current(GswordReaderHistory *history, guint index)
{
    if (history == NULL || index >= history->entries->len)
        return;
    history->current = (gint)index;
    save_history(history);
}

void
gsword_reader_history_clear(GswordReaderHistory *history)
{
    if (history == NULL)
        return;
    g_ptr_array_set_size(history->entries, 0);
    history->current = -1;
    save_history(history);
}
