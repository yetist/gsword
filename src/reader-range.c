/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * reader-range.c: reader range, active verse, and highlight rendering
 */

#include "reader-page-private.h"
#include "window-private.h"

#include <glib/gi18n.h>

#include <string.h>

void
gsword_reader_page_apply_highlights(GswordReaderPage *page)
{
    GswordNotesStore *notes = gsword_window_get_notes_store(READER_WINDOW(page));
    GswModule *module = gsword_window_get_module(READER_WINDOW(page));
    GswKey *key;
    gchar *reference;
    gchar *dot;
    GtkTextBuffer *buffer;
    GtkTextIter iter;

    if (notes == NULL || module == NULL || page->highlight_tag == NULL)
        return;
    key = gsw_module_get_key(module);
    if (gsw_key_get_kind(key) != GSW_KEY_KIND_VERSE)
        return;
    reference = gsw_verse_key_get_osis_ref((GswVerseKey *)key);
    if (reference == NULL)
        return;
    dot = strrchr(reference, '.');
    if (dot != NULL)
        *dot = '\0';

    buffer = gtk_text_view_get_buffer(page->view);
    {
        GtkTextIter start;
        GtkTextIter finish;

        gtk_text_buffer_get_bounds(buffer, &start, &finish);
        gtk_text_buffer_remove_tag(buffer, page->highlight_tag,
                                   &start, &finish);
    }
    gtk_text_buffer_get_start_iter(buffer, &iter);
    if (!gtk_text_iter_forward_line(&iter))
        return;
    while (TRUE) {
        GtkTextIter end = iter;
        gchar *line;
        gchar *p;
        gchar *number_end = NULL;
        guint verse;
        gchar *verse_key;

        if (!gtk_text_iter_forward_line(&end))
            break;
        line = gtk_text_buffer_get_text(buffer, &iter, &end, TRUE);
        p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (!g_ascii_isdigit(*p)) {
            g_free(line);
            if (!gtk_text_iter_forward_line(&iter))
                break;
            continue;
        }
        verse = (guint)g_ascii_strtoll(p, &number_end, 10);
        verse_key = g_strdup_printf("%s.%u", reference, verse);
        if (gsword_notes_store_has_highlight(
                notes, gsw_module_get_name(module), verse_key)) {
            GtkTextIter line_start = iter;
            GtkTextIter line_end = end;

            gtk_text_buffer_apply_tag(buffer, page->highlight_tag,
                                      &line_start, &line_end);
        }
        g_free(verse_key);
        g_free(line);
        if (!gtk_text_iter_forward_line(&iter))
            break;
    }
    g_free(reference);
}


void
gsword_reader_page_clear_range(GswordReaderPage *page)
{
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;

    page->range_active = FALSE;
    page->range_start = 0;
    page->range_end = 0;
    if (page->view == NULL || page->range_tag == NULL)
        return;
    buffer = gtk_text_view_get_buffer(page->view);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gtk_text_buffer_remove_tag(buffer, page->range_tag, &start, &end);
}

void
gsword_reader_page_apply_range(GswordReaderPage *page)
{
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;
    gint first;
    gint last;
    gint line_count;
    gint line;

    if (page->view == NULL || page->range_tag == NULL)
        return;
    buffer = gtk_text_view_get_buffer(page->view);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gtk_text_buffer_remove_tag(buffer, page->range_tag, &start, &end);
    if (!page->range_active || page->range_start < 1 || page->range_end < 1)
        return;

    first = MIN(page->range_start, page->range_end);
    last = MAX(page->range_start, page->range_end);
    line_count = gtk_text_buffer_get_line_count(buffer);
    for (line = 1; line < line_count; line++) {
        GtkTextIter line_start;
        GtkTextIter line_end;
        gchar *text;
        gint verse;

        gtk_text_buffer_get_iter_at_line(buffer, &line_start, line);
        line_end = line_start;
        if (!gtk_text_iter_forward_line(&line_end))
            gtk_text_buffer_get_end_iter(buffer, &line_end);
        text = gtk_text_buffer_get_text(buffer, &line_start, &line_end, TRUE);
        verse = gsword_window_verse_number_from_line(text);
        g_free(text);
        if (verse >= first && verse <= last &&
            gtk_text_iter_get_line(&line_start) > 0)
            gtk_text_buffer_apply_tag(buffer, page->range_tag,
                                      &line_start, &line_end);
    }
}


gboolean
gsword_reader_page_verse_at_position(GtkTextView *view, gdouble x, gdouble y, gint *verse)
{
    GtkTextBuffer *buffer;
    GtkTextIter iter;
    GtkTextIter line_start;
    GtkTextIter line_end;
    gchar *line;
    gint bx;
    gint by;
    gint trailing;
    gint line_y;
    gint line_height;
    gint found = 0;

    gtk_text_view_window_to_buffer_coords(view, GTK_TEXT_WINDOW_WIDGET,
                                          (gint)x, (gint)y, &bx, &by);
    if (!gtk_text_view_get_iter_at_position(view, &iter, &trailing, bx, by))
        return FALSE;
    if (gtk_text_iter_get_line(&iter) == 0)
        return FALSE;
    gtk_text_view_get_line_yrange(view, &iter, &line_y, &line_height);
    if (by < line_y || by >= line_y + line_height)
        return FALSE;
    buffer = gtk_text_view_get_buffer(view);
    line_start = iter;
    gtk_text_iter_set_line_offset(&line_start, 0);
    line_end = line_start;
    if (!gtk_text_iter_forward_line(&line_end))
        gtk_text_buffer_get_end_iter(buffer, &line_end);
    line = gtk_text_buffer_get_text(buffer, &line_start, &line_end, TRUE);
    found = gsword_window_verse_number_from_line(line);
    g_free(line);
    if (found <= 0)
        return FALSE;
    if (verse != NULL)
        *verse = found;
    return TRUE;
}

