/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * reader-copy.c: reader verse and chapter copying
 */

#include "reader-page-private.h"
#include "window-private.h"

#include <glib/gi18n.h>

static gchar *
copy_verse_text(GswordReaderPage *page, gint verse)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(page->view);
    gint line_count = gtk_text_buffer_get_line_count(buffer);
    gint line;

    for (line = 1; line < line_count; line++) {
        GtkTextIter start;
        GtkTextIter end;
        gchar *text;
        gint found;

        gtk_text_buffer_get_iter_at_line(buffer, &start, line);
        end = start;
        if (!gtk_text_iter_forward_line(&end))
            gtk_text_buffer_get_end_iter(buffer, &end);
        text = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
        found = gsword_window_verse_number_from_line(text);
        if (found == verse)
            return text;
        g_free(text);
    }
    return NULL;
}

static void
copy_text_to_clipboard(GswordReaderPage *page, const gchar *text)
{
    GdkClipboard *clipboard =
        gtk_widget_get_clipboard(GTK_WIDGET(READER_WINDOW(page)));

    gdk_clipboard_set_text(clipboard, text != NULL ? text : "");
    gsword_window_toast(READER_WINDOW(page), _("Copied to clipboard"));
}

void
gsword_reader_page_copy(GswordReaderPage *page)
{
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;
    gchar *text = NULL;

    if (page == NULL)
        return;
    buffer = gtk_text_view_get_buffer(page->view);
    if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
        text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    } else if (page->range_active && page->range_start > 0 &&
               page->range_end > 0) {
        gint first = MIN(page->range_start, page->range_end);
        gint last = MAX(page->range_start, page->range_end);
        gint line_count = gtk_text_buffer_get_line_count(buffer);
        GString *selected = g_string_new(NULL);
        gint line;

        for (line = 1; line < line_count; line++) {
            GtkTextIter line_start;
            GtkTextIter line_end;
            gchar *line_text;
            gint verse;

            gtk_text_buffer_get_iter_at_line(buffer, &line_start, line);
            line_end = line_start;
            if (!gtk_text_iter_forward_line(&line_end))
                gtk_text_buffer_get_end_iter(buffer, &line_end);
            line_text = gtk_text_buffer_get_text(buffer, &line_start,
                                                 &line_end, TRUE);
            verse = gsword_window_verse_number_from_line(line_text);
            if (verse >= first && verse <= last)
                g_string_append(selected, line_text);
            g_free(line_text);
        }
        text = g_string_free(selected, FALSE);
    } else if (page->active_verse > 0) {
        text = copy_verse_text(page, page->active_verse);
    }
    if (text == NULL) {
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    }
    copy_text_to_clipboard(page, text);
    g_free(text);
}

void
gsword_reader_page_copy_chapter(GswordReaderPage *page)
{
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;
    gchar *text;

    if (page == NULL)
        return;
    buffer = gtk_text_view_get_buffer(page->view);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    copy_text_to_clipboard(page, text);
    g_free(text);
}
