/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * notes-editor.c: markdown editing helpers for the notes page
 */

#include "notes-page-private.h"

#include <glib/gi18n.h>

static void
focus_note_editor(GswordNotesPage *page)
{
    if (page->text != NULL)
        gtk_widget_grab_focus(GTK_WIDGET(page->text));
}

static void
wrap_selection(GswordNotesPage *page, const gchar *prefix,
               const gchar *suffix)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(page->text);
    GtkTextIter start;
    GtkTextIter end;
    gboolean has_selection = gtk_text_buffer_get_selection_bounds(
        buffer, &start, &end);

    gtk_text_buffer_begin_user_action(buffer);
    if (has_selection) {
        gchar *selected = gtk_text_buffer_get_text(buffer, &start, &end,
                                                   FALSE);
        gchar *replacement = g_strdup_printf("%s%s%s", prefix, selected,
                                             suffix);

        gtk_text_buffer_delete(buffer, &start, &end);
        gtk_text_buffer_insert(buffer, &start, replacement, -1);
        g_free(replacement);
        g_free(selected);
    } else {
        GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
        GtkTextIter iter;
        gchar *replacement = g_strdup_printf("%s%s", prefix, suffix);

        gtk_text_buffer_insert_at_cursor(buffer, replacement, -1);
        gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
        gtk_text_iter_backward_chars(
            &iter, (gint)g_utf8_strlen(suffix, -1));
        gtk_text_buffer_place_cursor(buffer, &iter);
        g_free(replacement);
    }
    gtk_text_buffer_end_user_action(buffer);
    focus_note_editor(page);
}

static void
prefix_selected_lines(GswordNotesPage *page, const gchar *prefix)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(page->text);
    GtkTextIter start;
    GtkTextIter end;
    gboolean has_selection = gtk_text_buffer_get_selection_bounds(
        buffer, &start, &end);

    gtk_text_buffer_begin_user_action(buffer);
    if (has_selection) {
        gint start_line = gtk_text_iter_get_line(&start);
        gint end_line = gtk_text_iter_get_line(&end);
        gint line;

        for (line = end_line; line >= start_line; line--) {
            GtkTextIter iter;
            gtk_text_buffer_get_iter_at_line(buffer, &iter, line);
            gtk_text_buffer_insert(buffer, &iter, prefix, -1);
        }
    } else {
        GtkTextIter iter;
        gtk_text_buffer_get_iter_at_mark(
            buffer, &iter, gtk_text_buffer_get_insert(buffer));
        gtk_text_iter_set_line_offset(&iter, 0);
        gtk_text_buffer_insert(buffer, &iter, prefix, -1);
    }
    gtk_text_buffer_end_user_action(buffer);
    focus_note_editor(page);
}

static void
on_markdown_bold(GtkButton *button, gpointer user_data)
{
    (void)button;
    wrap_selection(user_data, "**", "**");
}

static void
on_markdown_italic(GtkButton *button, gpointer user_data)
{
    (void)button;
    wrap_selection(user_data, "*", "*");
}

static void
on_markdown_heading(GtkButton *button, gpointer user_data)
{
    (void)button;
    prefix_selected_lines(user_data, "# ");
}

static void
on_markdown_bullet(GtkButton *button, gpointer user_data)
{
    (void)button;
    prefix_selected_lines(user_data, "- ");
}

static void
on_markdown_numbered(GtkButton *button, gpointer user_data)
{
    (void)button;
    prefix_selected_lines(user_data, "1. ");
}

static void
on_markdown_quote(GtkButton *button, gpointer user_data)
{
    (void)button;
    prefix_selected_lines(user_data, "> ");
}

static void
on_markdown_link(GtkButton *button, gpointer user_data)
{
    (void)button;
    wrap_selection(user_data, "[", "](url)");
}

static void
on_markdown_code(GtkButton *button, gpointer user_data)
{
    (void)button;
    wrap_selection(user_data, "`", "`");
}

static GtkWidget *
make_markdown_button(const gchar *label, const gchar *tooltip,
                     GCallback callback, GswordNotesPage *page)
{
    GtkWidget *button = gtk_button_new_with_label(label);

    gtk_widget_add_css_class(button, "flat");
    gtk_widget_set_tooltip_text(button, tooltip);
    g_signal_connect(button, "clicked", callback, page);
    return button;
}

void
gsword_notes_editor_add_format_buttons(GswordNotesPage *page, GtkWidget *bar)
{
    gtk_box_append(GTK_BOX(bar),
                   make_markdown_button("B", _("Bold"),
                                        G_CALLBACK(on_markdown_bold), page));
    gtk_box_append(GTK_BOX(bar),
                   make_markdown_button("I", _("Italic"),
                                        G_CALLBACK(on_markdown_italic), page));
    gtk_box_append(GTK_BOX(bar),
                   make_markdown_button("H", _("Heading"),
                                        G_CALLBACK(on_markdown_heading), page));
    gtk_box_append(GTK_BOX(bar),
                   make_markdown_button("•", _("Bullet list"),
                                        G_CALLBACK(on_markdown_bullet), page));
    gtk_box_append(GTK_BOX(bar),
                   make_markdown_button("1.", _("Numbered list"),
                                        G_CALLBACK(on_markdown_numbered), page));
    gtk_box_append(GTK_BOX(bar),
                   make_markdown_button(">", _("Quote"),
                                        G_CALLBACK(on_markdown_quote), page));
    gtk_box_append(GTK_BOX(bar),
                   make_markdown_button("[]", _("Link"),
                                        G_CALLBACK(on_markdown_link), page));
    gtk_box_append(GTK_BOX(bar),
                   make_markdown_button("`", _("Inline code"),
                                        G_CALLBACK(on_markdown_code), page));
}
