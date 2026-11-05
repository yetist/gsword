/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * reader-page.c: scripture reader page for the gsword window
 */

#include "reader-page.h"
#include "reader-toolbar.h"
#include "study-panel.h"
#include "window-private.h"

#include <glib/gi18n.h>

#include <string.h>

struct _GswordReaderPage {
    GswordWindow *window;
    GtkWidget *root;
    GswordReaderToolbar *toolbar;
    GswordStudyPanel *study_panel;
    GtkTextView *view;
    GtkWidget *scrolled;
    GtkWidget *empty_bar;
    GtkPaned *paned;
    GtkWidget *study_widget;
    GswordReferenceChooser *reference_chooser;
    GtkTextTag *highlight_tag;
    GtkTextTag *active_verse_tag;
    GtkTextTag *range_tag;
    GtkTextTag *spacing_tag;
    GtkPopover *context_menu;
    gint active_verse;
    gint range_start;
    gint range_end;
    gboolean range_active;
    GswordReaderHistory *history;
    gboolean history_navigating;
    gboolean focus_mode;
    gboolean focus_sidebar_visible;
    gboolean focus_study_visible;
    gboolean updating_study_width;
    gboolean study_width_ready;
    gint study_width;
    gint paragraph_spacing;
    gint reading_width;
    guint margin_idle;
    guint scroll_idle;
};

static void apply_active_verse(GswordReaderPage *page);
static void clear_range(GswordReaderPage *page);
static void apply_range(GswordReaderPage *page);
static void apply_highlights(GswordReaderPage *page);
static void select_verse_range(GswordReaderPage *page, gint anchor,
                               gint verse);
static void select_verse(GswordReaderPage *page, gint verse);
static void update_reference_status(GswordReaderPage *page);
static void record_current_history(GswordReaderPage *page);
static void schedule_scroll_to_active(GswordReaderPage *page);

static void
set_text(GswordReaderPage *page, const gchar *text)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(page->view);

    gtk_text_buffer_set_text(buffer, text != NULL ? text : "", -1);
}

static void
update_reading_margins(GswordReaderPage *page)
{
    gint width;
    gint margin;

    if (page == NULL || page->scrolled == NULL || page->view == NULL)
        return;
    width = gtk_widget_get_width(page->scrolled);
    if (width <= 0)
        return;
    margin = MAX(28, (width - page->reading_width) / 2);
    gtk_text_view_set_left_margin(page->view, margin);
    gtk_text_view_set_right_margin(page->view, margin);
}

static gboolean
update_reading_margins_idle(gpointer user_data)
{
    GswordReaderPage *page = user_data;

    page->margin_idle = 0;
    update_reading_margins(page);
    return G_SOURCE_REMOVE;
}

static void
schedule_reading_margins(GswordReaderPage *page)
{
    if (page->margin_idle != 0)
        g_source_remove(page->margin_idle);
    page->margin_idle = g_timeout_add(50, update_reading_margins_idle, page);
}

static void
on_reader_size_changed(GObject *object, GParamSpec *pspec,
                       gpointer user_data)
{
    (void)object;
    (void)pspec;
    update_reading_margins(user_data);
    schedule_reading_margins(user_data);
}

static void
on_reader_mapped(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    update_reading_margins(user_data);
    schedule_reading_margins(user_data);
    schedule_scroll_to_active(user_data);
}

static void
apply_paragraph_spacing(GswordReaderPage *page)
{
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;

    if (page == NULL || page->spacing_tag == NULL || page->view == NULL)
        return;
    buffer = gtk_text_view_get_buffer(page->view);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gtk_text_buffer_remove_tag(buffer, page->spacing_tag, &start, &end);
    if (page->paragraph_spacing > 0)
        gtk_text_buffer_apply_tag(buffer, page->spacing_tag, &start, &end);
}

gint
gsword_reader_page_get_paragraph_spacing(GswordReaderPage *page)
{
    return page != NULL ? page->paragraph_spacing : 0;
}

void
gsword_reader_page_set_paragraph_spacing(GswordReaderPage *page, gint spacing)
{
    GSettings *settings;

    if (page == NULL)
        return;
    page->paragraph_spacing = CLAMP(spacing, 0, 24);
    settings = gsword_window_get_settings(page->window);
    if (settings != NULL)
        g_settings_set_int(settings, "reader-paragraph-spacing",
                           page->paragraph_spacing);
    if (page->spacing_tag != NULL)
        g_object_set(page->spacing_tag,
                     "pixels-above-lines", page->paragraph_spacing / 2,
                     "pixels-below-lines", page->paragraph_spacing / 2,
                     NULL);
    apply_paragraph_spacing(page);
}

gint
gsword_reader_page_get_reading_width(GswordReaderPage *page)
{
    return page != NULL ? page->reading_width : 760;
}

void
gsword_reader_page_set_reading_width(GswordReaderPage *page, gint width)
{
    GSettings *settings;

    if (page == NULL)
        return;
    page->reading_width = CLAMP(width, 480, 1200);
    settings = gsword_window_get_settings(page->window);
    if (settings != NULL)
        g_settings_set_int(settings, "reader-width", page->reading_width);
    update_reading_margins(page);
}

static void
on_empty_manage_modules(GtkButton *button, gpointer user_data)
{
    GswordReaderPage *page = user_data;

    (void)button;
    gsword_window_open_library(page->window);
}

static void
save_last_position(GswordReaderPage *page)
{
    GSettings *settings = gsword_window_get_settings(page->window);
    GswModule *module = gsword_window_get_module(page->window);
    const gchar *name;
    gchar *key;

    if (settings == NULL || module == NULL)
        return;
    name = gsw_module_get_name(module);
    key = gsword_reader_page_current_storage_key(page);
    if (name != NULL)
        g_settings_set_string(settings, "last-module", name);
    if (key != NULL)
        g_settings_set_string(settings, "last-key", key);
    g_free(key);
}

static void
record_current_history(GswordReaderPage *page)
{
    GswordWindow *self;
    GswModule *module;
    GswKey *key;
    gchar *reference;
    gchar *display;
    GswVerseKey *verse_key;

    if (page == NULL || page->history == NULL || page->history_navigating)
        return;
    self = page->window;
    module = gsword_window_get_module(self);
    if (module == NULL)
        return;
    key = gsw_module_get_key(module);
    if (gsw_key_get_kind(key) != GSW_KEY_KIND_VERSE)
        return;
    verse_key = (GswVerseKey *)key;
    reference = gsw_verse_key_get_osis_ref(verse_key);
    if (reference == NULL)
        return;
    display = g_strdup_printf("%s %d:%d",
                              gsw_verse_key_get_book_name(verse_key),
                              gsw_verse_key_get_chapter(verse_key),
                              MAX(gsw_verse_key_get_verse(verse_key), 1));
    gsword_reader_history_push(page->history,
                               gsw_module_get_name(module),
                               reference, display);
    gsword_reader_toolbar_update_history(page->toolbar);
    g_free(display);
    g_free(reference);
}

void
gsword_reader_page_sync_context(GswordReaderPage *page)
{
    GswModule *module = gsword_window_get_module(page->window);
    GswModuleInfo *info;
    const gchar *versification = NULL;
    gchar *locale;

    if (page == NULL || page->reference_chooser == NULL || module == NULL)
        return;
    info = gsw_module_get_info(module);
    if (info != NULL)
        versification = gsw_module_info_get_versification(info);
    locale = gsword_window_book_name_locale(page->window, info);
    gsword_reference_chooser_set_versification(
        page->reference_chooser,
        versification != NULL && *versification != '\0'
        ? versification : "KJV");
    gsword_reference_chooser_set_locale(page->reference_chooser, locale);
    g_free(locale);
}

static void
sync_reference_chooser(GswordReaderPage *page)
{
    GswModule *module = gsword_window_get_module(page->window);
    GswKey *key;
    GswVerseKey *verse_key;
    gint book;
    gint chapter;
    gint verse;

    if (page->reference_chooser == NULL)
        return;
    if (module == NULL ||
        gsw_key_get_kind(gsw_module_get_key(module)) != GSW_KEY_KIND_VERSE) {
        gtk_widget_set_sensitive(GTK_WIDGET(page->reference_chooser), FALSE);
        return;
    }
    gtk_widget_set_sensitive(GTK_WIDGET(page->reference_chooser), TRUE);
    key = gsw_module_get_key(module);
    verse_key = (GswVerseKey *)key;
    book = gsw_verse_key_get_global_book_number(verse_key);
    chapter = gsw_verse_key_get_chapter(verse_key);
    verse = MAX(gsw_verse_key_get_verse(verse_key), 1);
    if (book < 0 || chapter < 1)
        return;
    gsword_reference_chooser_set_reference(page->reference_chooser,
                                           book, chapter, verse);
}

void
gsword_reader_page_refresh(GswordReaderPage *page)
{
    GswordWindow *self;
    GswModule *module;
    GError *error = NULL;
    gchar *text;
    gchar *status;

    if (page == NULL)
        return;
    self = page->window;
    module = gsword_window_get_module(self);
    clear_range(page);
    if (module == NULL) {
        set_text(page,
            _("No modules found.\n\nInstall SWORD modules or start gsword with "
              "--library PATH."));
        gsword_window_set_status(self, _("No modules found"));
        gtk_widget_set_visible(page->empty_bar, TRUE);
        gsword_reader_toolbar_set_available(page->toolbar, FALSE);
        sync_reference_chooser(page);
        gsword_window_refresh_search_index(self);
        return;
    }
    gtk_widget_set_visible(page->empty_bar, FALSE);
    gsword_reader_toolbar_set_available(page->toolbar, TRUE);

    if (gsw_key_get_kind(gsw_module_get_key(module)) == GSW_KEY_KIND_VERSE) {
        GswVerseKey *key = (GswVerseKey *)gsw_module_get_key(module);

        if (gsw_verse_key_get_book(key) >= 1 &&
            gsw_verse_key_get_chapter(key) >= 1) {
            text = gsword_window_chapter_text_for_module(module);
        } else {
            text = gsw_module_get_text(module, &error);
        }
    } else {
        text = gsw_module_get_text(module, &error);
    }

    if (text == NULL) {
        gsword_window_show_error(self, _("Cannot read entry"), error);
        return;
    }
    set_text(page, text);
    g_free(text);
    apply_paragraph_spacing(page);

    status = g_strdup(gsw_module_get_name(module));
    gsword_window_set_status(self, status);
    g_free(status);

    apply_highlights(page);
    apply_active_verse(page);
    schedule_scroll_to_active(page);
    gsword_study_panel_refresh(page->study_panel);
    gsword_window_refresh_search_index(self);
    sync_reference_chooser(page);
    gsword_window_sync_workspace_from_reader(self);
    save_last_position(page);
    record_current_history(page);
}

gboolean
gsword_reader_page_set_key_text(GswordReaderPage *page, const gchar *text)
{
    GswordWindow *self;
    GswModule *module;
    GError *error = NULL;
    gchar *normalized;

    if (page == NULL)
        return FALSE;
    self = page->window;
    module = gsword_window_get_module(self);
    if (module == NULL)
        return FALSE;
    normalized = gsword_window_normalize_reference(text);
    if (!gsw_module_set_key_text(module, normalized, &error)) {
        gsword_window_show_error(self, _("Cannot set key"), error);
        g_free(normalized);
        return FALSE;
    }
    g_free(normalized);
    gsword_reader_page_refresh(page);
    return TRUE;
}

void
gsword_reader_page_set_initial_key(GswordReaderPage *page)
{
    GswModule *module;

    if (page == NULL)
        return;
    module = gsword_window_get_module(page->window);
    if (module == NULL)
        return;

    if (gsw_key_get_kind(gsw_module_get_key(module)) ==
        GSW_KEY_KIND_VERSE) {
        if (gsw_module_set_key_text(module, "Gen 1:1", NULL))
            return;
        if (gsw_module_set_key_text(module, "Matt 1:1", NULL))
            return;
    }
    gsw_module_set_position(module, GSW_POSITION_TOP, NULL);
}

void
gsword_reader_page_begin_reference_edit(GswordReaderPage *page)
{
    if (page != NULL && page->reference_chooser != NULL)
        gsword_reference_chooser_begin_edit(page->reference_chooser);
}

void
gsword_reader_page_activate_reference(GswordReaderPage *page)
{
    gint book;
    gint chapter;
    gint verse;
    gchar *osis;
    gchar *text;

    if (page == NULL || page->reference_chooser == NULL)
        return;
    if (!gsword_reference_chooser_get_reference(page->reference_chooser,
                                                &book, &chapter, &verse))
        return;
    osis = gsword_reference_chooser_dup_osis(page->reference_chooser);
    if (osis == NULL || *osis == '\0') {
        g_free(osis);
        return;
    }
    text = g_strdup_printf("%s %d:%d", osis, chapter, verse);
    if (!gsword_reader_page_set_key_text(page, text))
        sync_reference_chooser(page);
    g_free(text);
    g_free(osis);
}

gchar *
gsword_reader_page_current_storage_key(GswordReaderPage *page)
{
    GswModule *module;
    GswKey *key;

    if (page == NULL)
        return NULL;
    module = gsword_window_get_module(page->window);
    if (module == NULL)
        return NULL;
    key = gsw_module_get_key(module);
    if (gsw_key_get_kind(key) == GSW_KEY_KIND_VERSE)
        return gsw_verse_key_get_osis_ref((GswVerseKey *)key);
    return g_strdup(gsw_module_get_key_text(module));
}

gchar *
gsword_reader_page_current_display_key(GswordReaderPage *page)
{
    GswModule *module;

    if (page == NULL)
        return NULL;
    module = gsword_window_get_module(page->window);
    if (module == NULL)
        return NULL;
    return g_strdup(gsw_module_get_key_text(module));
}

static void
apply_highlights(GswordReaderPage *page)
{
    GswordNotesStore *notes = gsword_window_get_notes_store(page->window);
    GswModule *module = gsword_window_get_module(page->window);
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

gchar *
gsword_reader_page_current_reference(GswordReaderPage *page)
{
    GswModule *module;
    GswKey *key;

    if (page == NULL)
        return NULL;
    module = gsword_window_get_module(page->window);
    if (module == NULL)
        return NULL;
    key = gsw_module_get_key(module);
    if (gsw_key_get_kind(key) == GSW_KEY_KIND_VERSE) {
        GswVerseKey *verse_key = (GswVerseKey *)key;
        gint book = gsw_verse_key_get_book(verse_key);
        gint chapter = gsw_verse_key_get_chapter(verse_key);
        gint verse = gsw_verse_key_get_verse(verse_key);

        if (book >= 1 && chapter >= 1) {
            if (page->range_active && page->range_start > 0 &&
                page->range_end > 0) {
                gint first = MIN(page->range_start, page->range_end);
                gint last = MAX(page->range_start, page->range_end);

                return g_strdup_printf("%s %d:%d-%d",
                                       gsw_verse_key_get_book_name(verse_key),
                                       chapter, first, last);
            }
            return g_strdup_printf("%s %d:%d",
                                   gsw_verse_key_get_book_name(verse_key),
                                   chapter, verse > 0 ? verse : 1);
        }
    }
    return g_strdup(gsw_module_get_key_text(module));
}

gchar *
gsword_reader_page_current_start_reference(GswordReaderPage *page)
{
    GswModule *module;
    GswKey *key;

    if (page == NULL)
        return NULL;
    module = gsword_window_get_module(page->window);
    if (module == NULL)
        return NULL;
    key = gsw_module_get_key(module);
    if (gsw_key_get_kind(key) == GSW_KEY_KIND_VERSE && page->range_active &&
        page->range_start > 0 && page->range_end > 0) {
        GswVerseKey *verse_key = (GswVerseKey *)key;
        gint book = gsw_verse_key_get_book(verse_key);
        gint chapter = gsw_verse_key_get_chapter(verse_key);

        if (book >= 1 && chapter >= 1) {
            return g_strdup_printf("%s %d:%d",
                                   gsw_verse_key_get_book_name(verse_key),
                                   chapter,
                                   MIN(page->range_start, page->range_end));
        }
    }
    return gsword_reader_page_current_reference(page);
}

static void
update_reference_status(GswordReaderPage *page)
{
    GswModule *module = gsword_window_get_module(page->window);

    if (module == NULL)
        return;
    sync_reference_chooser(page);
    gsword_window_set_status(page->window, gsw_module_get_name(module));
}

static void
apply_active_verse(GswordReaderPage *page)
{
    GswModule *module = gsword_window_get_module(page->window);
    GtkTextBuffer *buffer;
    GtkTextIter iter;
    GtkTextIter finish;
    gint active;
    gint line_count;
    gint line;

    if (page->view == NULL || page->active_verse_tag == NULL)
        return;
    buffer = gtk_text_view_get_buffer(page->view);
    gtk_text_buffer_get_bounds(buffer, &iter, &finish);
    gtk_text_buffer_remove_tag(buffer, page->active_verse_tag, &iter, &finish);

    if (module == NULL ||
        gsw_key_get_kind(gsw_module_get_key(module)) != GSW_KEY_KIND_VERSE) {
        apply_range(page);
        return;
    }

    active = gsw_verse_key_get_verse((GswVerseKey *)gsw_module_get_key(module));
    if (active < 1)
        active = 1;
    page->active_verse = active;

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
        if (verse == active) {
            gtk_text_buffer_apply_tag(buffer, page->active_verse_tag,
                                      &line_start, &line_end);
            break;
        }
    }
    apply_range(page);
}

static void
scroll_to_active_verse(GswordReaderPage *page)
{
    GtkTextBuffer *buffer;
    gint line_count;
    gint line;
    gint active;

    if (page == NULL || page->view == NULL)
        return;
    active = page->active_verse;
    if (active < 1)
        return;
    buffer = gtk_text_view_get_buffer(page->view);
    line_count = gtk_text_buffer_get_line_count(buffer);
    for (line = 1; line < line_count; line++) {
        GtkTextIter start;
        GtkTextIter end;
        gchar *text;
        gint verse;

        gtk_text_buffer_get_iter_at_line(buffer, &start, line);
        end = start;
        if (!gtk_text_iter_forward_line(&end))
            gtk_text_buffer_get_end_iter(buffer, &end);
        text = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
        verse = gsword_window_verse_number_from_line(text);
        g_free(text);
        if (verse == active) {
            GtkWidget *scrolled = gtk_widget_get_ancestor(
                GTK_WIDGET(page->view), GTK_TYPE_SCROLLED_WINDOW);
            GtkAdjustment *adjustment = scrolled != NULL
                ? gtk_scrolled_window_get_vadjustment(
                      GTK_SCROLLED_WINDOW(scrolled)) : NULL;
            gint y = 0;
            gint height = 0;

            gtk_text_view_get_line_yrange(page->view, &start, &y, &height);
            if (adjustment != NULL) {
                gdouble page_size = gtk_adjustment_get_page_size(adjustment);
                gdouble target = y - page_size * 0.15;

                gtk_adjustment_set_value(adjustment, MAX(target, 0.0));
            } else {
                gtk_text_view_scroll_to_iter(page->view, &start, 0.1, TRUE,
                                             0.0, 0.15);
            }
            return;
        }
    }
}

static gboolean
scroll_to_active_idle(gpointer user_data)
{
    GswordReaderPage *page = user_data;

    page->scroll_idle = 0;
    scroll_to_active_verse(page);
    return G_SOURCE_REMOVE;
}

static void
schedule_scroll_to_active(GswordReaderPage *page)
{
    if (page->scroll_idle != 0)
        g_source_remove(page->scroll_idle);
    page->scroll_idle = g_timeout_add(200, scroll_to_active_idle, page);
}

void
gsword_reader_page_scroll_to_active(GswordReaderPage *page)
{
    schedule_scroll_to_active(page);
}

static void
clear_range(GswordReaderPage *page)
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

static void
apply_range(GswordReaderPage *page)
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

static void
select_verse_range(GswordReaderPage *page, gint anchor, gint verse)
{
    GswModule *module = gsword_window_get_module(page->window);
    GswVerseKey *key;
    GError *error = NULL;
    gint maximum;
    gchar *text;

    if (module == NULL ||
        gsw_key_get_kind(gsw_module_get_key(module)) != GSW_KEY_KIND_VERSE)
        return;
    key = (GswVerseKey *)gsw_module_get_key(module);
    maximum = gsw_verse_key_get_verse_max(key);
    if (verse < 1 || verse > maximum)
        return;
    if (anchor < 1 || anchor > maximum)
        anchor = page->active_verse;
    if (anchor < 1 || anchor > maximum)
        anchor = verse;

    gsw_verse_key_set_verse(key, anchor);
    text = g_strdup(gsw_module_get_key_text(module));
    if (!gsw_module_set_key_text(module, text, &error)) {
        gsword_window_show_error(page->window, _("Cannot set key"), error);
        g_free(text);
        return;
    }
    g_free(text);

    page->range_active = TRUE;
    page->range_start = anchor;
    page->range_end = verse;
    page->active_verse = anchor;
    update_reference_status(page);
    gsword_study_panel_refresh(page->study_panel);
    apply_active_verse(page);
    gsword_window_sync_workspace_from_reader(page->window);
    save_last_position(page);
    record_current_history(page);
}

static void
select_verse(GswordReaderPage *page, gint verse)
{
    GswModule *module = gsword_window_get_module(page->window);
    GswKey *key;
    GError *error = NULL;
    gchar *text;

    if (module == NULL)
        return;
    key = gsw_module_get_key(module);
    if (gsw_key_get_kind(key) != GSW_KEY_KIND_VERSE)
        return;
    if (verse < 1 || verse > gsw_verse_key_get_verse_max((GswVerseKey *)key))
        return;

    clear_range(page);
    gsw_verse_key_set_verse((GswVerseKey *)key, verse);
    text = g_strdup(gsw_module_get_key_text(module));
    if (!gsw_module_set_key_text(module, text, &error)) {
        gsword_window_show_error(page->window, _("Cannot set key"), error);
        g_free(text);
        return;
    }
    g_free(text);
    page->active_verse = verse;
    update_reference_status(page);
    gsword_study_panel_refresh(page->study_panel);
    apply_active_verse(page);
    gsword_window_sync_workspace_from_reader(page->window);
    save_last_position(page);
    record_current_history(page);
}

static gboolean
verse_at_position(GtkTextView *view, gdouble x, gdouble y, gint *verse)
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

static void
on_context_copy(GtkButton *button, gpointer user_data)
{
    GswordReaderPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    gsword_reader_page_copy(page);
}

static void
on_context_note(GtkButton *button, gpointer user_data)
{
    GswordReaderPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    gsword_window_open_current_note(page->window);
}

static void
on_context_highlight(GtkButton *button, gpointer user_data)
{
    GswordReaderPage *page = user_data;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    gsword_reader_page_toggle_highlight(page);
}

static void
on_context_prayer(GtkButton *button, gpointer user_data)
{
    GswordReaderPage *page = user_data;
    GswModule *module = gsword_window_get_module(page->window);
    gchar *storage_key;
    gchar *display_key;

    (void)button;
    gtk_popover_popdown(page->context_menu);
    if (module == NULL)
        return;
    storage_key = gsword_window_current_storage_key(page->window);
    display_key = gsword_window_current_display_key(page->window);
    gsword_window_open_prayer_for(page->window, gsw_module_get_name(module),
                                  storage_key, display_key);
    g_free(storage_key);
    g_free(display_key);
}

static void
on_reader_context_clicked(GtkGestureClick *gesture, gint n_press,
                          gdouble x, gdouble y, gpointer user_data)
{
    GswordReaderPage *page = user_data;
    GtkTextView *view;
    GdkRectangle rect = { (gint)x, (gint)y, 1, 1 };
    gint verse;

    (void)n_press;
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture)) !=
        GDK_BUTTON_SECONDARY)
        return;
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    view = GTK_TEXT_VIEW(gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(gesture)));
    if (!verse_at_position(view, x, y, &verse))
        return;
    select_verse(page, verse);
    gtk_popover_set_pointing_to(page->context_menu, &rect);
    gtk_popover_popup(page->context_menu);
}

static void
on_reader_clicked(GtkGestureClick *gesture, gint n_press, gdouble x,
                  gdouble y, gpointer user_data)
{
    GswordReaderPage *page = user_data;
    GtkTextView *view;
    GtkTextBuffer *buffer;
    GtkTextIter iter;
    GtkTextIter line_start;
    GtkTextIter line_end;
    gchar *line;
    gint verse;
    gint bx;
    gint by;
    GdkModifierType state;

    (void)n_press;
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture)) !=
        GDK_BUTTON_PRIMARY)
        return;
    state = gtk_event_controller_get_current_event_state(
        GTK_EVENT_CONTROLLER(gesture));
    view = GTK_TEXT_VIEW(gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(gesture)));
    gtk_text_view_window_to_buffer_coords(view, GTK_TEXT_WINDOW_WIDGET,
                                          (gint)x, (gint)y, &bx, &by);
    gtk_text_view_get_iter_at_location(view, &iter, bx, by);
    if (gtk_text_iter_get_line(&iter) == 0)
        return;
    buffer = gtk_text_view_get_buffer(view);
    line_start = iter;
    gtk_text_iter_set_line_offset(&line_start, 0);
    line_end = line_start;
    if (!gtk_text_iter_forward_line(&line_end))
        gtk_text_buffer_get_end_iter(buffer, &line_end);
    line = gtk_text_buffer_get_text(buffer, &line_start, &line_end, TRUE);
    verse = gsword_window_verse_number_from_line(line);
    g_free(line);
    if (verse <= 0)
        return;
    if ((state & GDK_SHIFT_MASK) != 0) {
        gint anchor = page->range_active && page->range_start > 0
                    ? page->range_start : page->active_verse;

        select_verse_range(page, anchor, verse);
    } else {
        select_verse(page, verse);
    }
}

void
gsword_reader_page_toggle_highlight(GswordReaderPage *page)
{
    GswordWindow *self;
    GswordNotesStore *notes;
    GswModule *module;
    gchar *storage_key;
    GError *error = NULL;

    if (page == NULL)
        return;
    self = page->window;
    notes = gsword_window_get_notes_store(self);
    module = gsword_window_get_module(self);
    if (notes == NULL || module == NULL)
        return;
    storage_key = gsword_reader_page_current_storage_key(page);
    if (storage_key == NULL)
        return;
    if (gsword_notes_store_has_highlight(
            notes, gsw_module_get_name(module), storage_key)) {
        gsword_notes_store_clear_highlight(
            notes, gsw_module_get_name(module), storage_key, &error);
        gsword_window_toast(self, _("Highlight removed"));
    } else {
        gsword_notes_store_set_highlight(
            notes, gsw_module_get_name(module), storage_key,
            "#fff0a8", &error);
        gsword_window_toast(self, _("Verse highlighted"));
    }
    if (error != NULL)
        gsword_window_show_error(self, _("Cannot update highlight"), error);
    g_free(storage_key);
    apply_highlights(page);
}

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
        gtk_widget_get_clipboard(GTK_WIDGET(page->window));

    gdk_clipboard_set_text(clipboard, text != NULL ? text : "");
    gsword_window_toast(page->window, _("Copied to clipboard"));
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

void
gsword_reader_page_set_focus_mode(GswordReaderPage *page, gboolean focus)
{
    if (page == NULL || page->focus_mode == focus)
        return;
    page->focus_mode = focus;
    if (focus) {
        page->focus_sidebar_visible =
            gsword_window_get_sidebar_visible(page->window);
        page->focus_study_visible =
            gtk_widget_get_visible(page->study_widget);
        gsword_window_set_sidebar_visible(page->window, FALSE);
        gtk_widget_set_visible(page->study_widget, FALSE);
        update_reading_margins(page);
        schedule_reading_margins(page);
    } else {
        gsword_window_set_sidebar_visible(
            page->window, page->focus_sidebar_visible);
        gtk_widget_set_visible(page->study_widget, page->focus_study_visible);
        update_reading_margins(page);
        schedule_reading_margins(page);
    }
    gsword_reader_toolbar_set_focus_mode(page->toolbar, focus);
}

GswordStudyPanel *
gsword_reader_page_get_study_panel(GswordReaderPage *page)
{
    return page != NULL ? page->study_panel : NULL;
}

GtkWidget *
gsword_reader_page_widget(GswordReaderPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}

static gboolean
navigate_history_entry(GswordReaderPage *page,
                       const GswordReaderHistoryEntry *entry)
{
    GswordWindow *self;
    GswModule *module;

    if (page == NULL || entry == NULL)
        return FALSE;
    self = page->window;
    page->history_navigating = TRUE;
    module = gsword_window_get_module(self);
    if (module == NULL ||
        g_strcmp0(gsw_module_get_name(module), entry->module) != 0)
        gsword_window_load_module(self, entry->module);
    module = gsword_window_get_module(self);
    if (module == NULL ||
        g_strcmp0(gsw_module_get_name(module), entry->module) != 0) {
        page->history_navigating = FALSE;
        return FALSE;
    }
    if (!gsword_window_set_key_text(self, entry->reference)) {
        page->history_navigating = FALSE;
        return FALSE;
    }
    page->history_navigating = FALSE;
    return TRUE;
}

gboolean
gsword_reader_page_history_can_back(GswordReaderPage *page)
{
    return page != NULL &&
           gsword_reader_history_can_back(page->history);
}

gboolean
gsword_reader_page_history_can_forward(GswordReaderPage *page)
{
    return page != NULL &&
           gsword_reader_history_can_forward(page->history);
}

gboolean
gsword_reader_page_history_back(GswordReaderPage *page)
{
    const GswordReaderHistoryEntry *entry;
    gint previous;
    gboolean result;

    if (!gsword_reader_page_history_can_back(page))
        return FALSE;
    previous = gsword_reader_page_history_current(page);
    entry = gsword_reader_history_back(page->history);
    result = navigate_history_entry(page, entry);
    if (!result)
        gsword_reader_history_set_current(page->history, (guint)previous);
    gsword_reader_toolbar_update_history(page->toolbar);
    return result;
}

gboolean
gsword_reader_page_history_forward(GswordReaderPage *page)
{
    const GswordReaderHistoryEntry *entry;
    gint previous;
    gboolean result;

    if (!gsword_reader_page_history_can_forward(page))
        return FALSE;
    previous = gsword_reader_page_history_current(page);
    entry = gsword_reader_history_forward(page->history);
    result = navigate_history_entry(page, entry);
    if (!result)
        gsword_reader_history_set_current(page->history, (guint)previous);
    gsword_reader_toolbar_update_history(page->toolbar);
    return result;
}

gboolean
gsword_reader_page_history_jump(GswordReaderPage *page, guint index)
{
    const GswordReaderHistoryEntry *entry;

    if (page == NULL || page->history == NULL)
        return FALSE;
    entry = gsword_reader_history_get_entry(page->history, index);
    if (entry == NULL)
        return FALSE;
    if (!navigate_history_entry(page, entry))
        return FALSE;
    gsword_reader_history_set_current(page->history, index);
    gsword_reader_toolbar_update_history(page->toolbar);
    return TRUE;
}

void
gsword_reader_page_history_clear(GswordReaderPage *page)
{
    if (page == NULL || page->history == NULL)
        return;
    gsword_reader_history_clear(page->history);
    gsword_reader_toolbar_update_history(page->toolbar);
}

guint
gsword_reader_page_history_count(GswordReaderPage *page)
{
    return page != NULL ? gsword_reader_history_get_length(page->history) : 0;
}

gint
gsword_reader_page_history_current(GswordReaderPage *page)
{
    return page != NULL
         ? gsword_reader_history_get_current_index(page->history) : -1;
}

const GswordReaderHistoryEntry *
gsword_reader_page_history_entry(GswordReaderPage *page, guint index)
{
    return page != NULL
         ? gsword_reader_history_get_entry(page->history, index) : NULL;
}

static void
on_study_position_changed(GObject *object, GParamSpec *pspec,
                          gpointer user_data)
{
    GswordReaderPage *page = user_data;
    GSettings *settings = gsword_window_get_settings(page->window);
    gint width = gtk_widget_get_width(GTK_WIDGET(object));
    gint position = gtk_paned_get_position(GTK_PANED(object));
    gint study_width = width - position;

    (void)pspec;
    schedule_reading_margins(page);
    if (page->updating_study_width || settings == NULL ||
        !gtk_widget_get_visible(page->study_widget) ||
        study_width < 200 || study_width > width - 160)
        return;
    page->study_width = study_width;
    g_settings_set_int(settings, "study-width", study_width);
}

static void
on_paned_width_changed(GObject *object, GParamSpec *pspec,
                       gpointer user_data)
{
    GswordReaderPage *page = user_data;
    gint width = gtk_widget_get_width(GTK_WIDGET(object));

    (void)pspec;
    schedule_reading_margins(page);
    if (page->study_width_ready || page->updating_study_width ||
        !gtk_widget_get_visible(page->study_widget) ||
        width <= page->study_width + 160)
        return;
    page->updating_study_width = TRUE;
    gtk_paned_set_position(GTK_PANED(object), width - page->study_width);
    page->updating_study_width = FALSE;
    page->study_width_ready = TRUE;
}

static void
on_study_visible_changed(GObject *object, GParamSpec *pspec,
                         gpointer user_data)
{
    GswordReaderPage *page = user_data;

    (void)object;
    (void)pspec;
    if (gtk_widget_get_visible(page->study_widget)) {
        gint width = gtk_widget_get_width(GTK_WIDGET(page->paned));

        page->study_width_ready = FALSE;
        if (width > page->study_width + 160) {
            page->updating_study_width = TRUE;
            gtk_paned_set_position(page->paned,
                                   width - page->study_width);
            page->updating_study_width = FALSE;
            page->study_width_ready = TRUE;
        }
    }
    schedule_reading_margins(page);
}

static gboolean
on_reader_key_pressed(GtkEventControllerKey *controller, guint keyval,
                      guint keycode, GdkModifierType state, gpointer user_data)
{
    GswordReaderPage *page = user_data;

    (void)controller;
    (void)keycode;
    if (keyval == GDK_KEY_F11 ||
        (keyval == GDK_KEY_f &&
         (state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) ==
         (GDK_CONTROL_MASK | GDK_SHIFT_MASK))) {
        gsword_reader_page_set_focus_mode(page, !page->focus_mode);
        return GDK_EVENT_STOP;
    }
    return GDK_EVENT_PROPAGATE;
}

static void
reader_page_free(gpointer data)
{
    GswordReaderPage *page = data;

    if (page == NULL)
        return;
    if (page->margin_idle != 0)
        g_source_remove(page->margin_idle);
    if (page->scroll_idle != 0)
        g_source_remove(page->scroll_idle);
    gsword_reader_history_free(page->history);
    g_free(page);
}

GswordReaderPage *
gsword_reader_page_new(GswordWindow *window)
{
    GswordReaderPage *page = g_new0(GswordReaderPage, 1);
    GtkWidget *reader_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *empty_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *empty_label = gtk_label_new(
        _("No Bible text module is installed."));
    GtkWidget *empty_manage = gtk_button_new_with_label(_("Manage modules"));
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *study_widget;
    GSettings *settings;
    gint study_width = 320;
    GtkGesture *reader_click;

    page->window = window;
    page->root = reader_box;
    page->scrolled = scrolled;
    page->reading_width = 760;
    settings = gsword_window_get_settings(window);
    if (settings != NULL) {
        page->paragraph_spacing = CLAMP(
            g_settings_get_int(settings, "reader-paragraph-spacing"), 0, 24);
        page->reading_width = CLAMP(
            g_settings_get_int(settings, "reader-width"), 480, 1200);
    }
    page->history = gsword_reader_history_new(
        gsword_window_get_settings(window), 50);
    page->toolbar = gsword_reader_toolbar_new(window, page);
    page->reference_chooser = gsword_reader_toolbar_get_reference_chooser(
        page->toolbar);
    page->study_panel = gsword_study_panel_new(window);

    page->view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(page->view, FALSE);
    gtk_text_view_set_cursor_visible(page->view, FALSE);
    gtk_text_view_set_wrap_mode(page->view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(page->view, 28);
    gtk_text_view_set_right_margin(page->view, 28);
    gtk_text_view_set_top_margin(page->view, 22);
    gtk_text_view_set_bottom_margin(page->view, 22);
    gtk_widget_add_css_class(GTK_WIDGET(page->view), "gsw-reader");
    page->spacing_tag = gtk_text_buffer_create_tag(
        gtk_text_view_get_buffer(page->view), "gsword-spacing", NULL);
    page->highlight_tag = gtk_text_buffer_create_tag(
        gtk_text_view_get_buffer(page->view), "gsword-highlight",
        "background", "#fff0a8", NULL);
    page->range_tag = gtk_text_buffer_create_tag(
        gtk_text_view_get_buffer(page->view), "gsword-range",
        "background", "#ede9fe", NULL);
    gtk_text_tag_set_priority(page->range_tag, 1);
    page->active_verse_tag = gtk_text_buffer_create_tag(
        gtk_text_view_get_buffer(page->view), "gsword-active-verse",
        "background", "#dbeafe", "weight", PANGO_WEIGHT_SEMIBOLD, NULL);
    g_object_set(page->spacing_tag,
                 "pixels-above-lines", page->paragraph_spacing / 2,
                 "pixels-below-lines", page->paragraph_spacing / 2,
                 NULL);
    gtk_text_tag_set_priority(page->active_verse_tag, 2);
    reader_click = gtk_gesture_click_new();
    g_signal_connect(reader_click, "pressed",
                     G_CALLBACK(on_reader_clicked), page);
    gtk_widget_add_controller(GTK_WIDGET(page->view),
                              GTK_EVENT_CONTROLLER(reader_click));
    {
        GtkGesture *context = gtk_gesture_click_new();
        GtkWidget *popover = gtk_popover_new();
        GtkWidget *menu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *copy_item = gtk_button_new_with_label(_("Copy verse"));
        GtkWidget *note_item = gtk_button_new_with_label(_("Open note"));
        GtkWidget *highlight_item = gtk_button_new_with_label(
            _("Toggle highlight"));
        GtkWidget *prayer_item = gtk_button_new_with_label(
            _("Add to prayer"));

        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(context),
                                      GDK_BUTTON_SECONDARY);
        gtk_event_controller_set_propagation_phase(
            GTK_EVENT_CONTROLLER(context), GTK_PHASE_CAPTURE);
        g_signal_connect(context, "pressed",
                         G_CALLBACK(on_reader_context_clicked), page);
        gtk_widget_add_controller(GTK_WIDGET(page->view),
                                  GTK_EVENT_CONTROLLER(context));

        page->context_menu = GTK_POPOVER(popover);
        gtk_popover_set_autohide(page->context_menu, TRUE);
        gtk_popover_set_has_arrow(page->context_menu, TRUE);
        gtk_widget_set_focusable(GTK_WIDGET(popover), FALSE);
        gtk_widget_set_margin_top(menu, 6);
        gtk_widget_set_margin_bottom(menu, 6);
        gtk_widget_set_margin_start(menu, 6);
        gtk_widget_set_margin_end(menu, 6);
        gtk_widget_add_css_class(copy_item, "flat");
        gtk_widget_add_css_class(note_item, "flat");
        gtk_widget_add_css_class(highlight_item, "flat");
        gtk_widget_add_css_class(prayer_item, "flat");
        g_signal_connect(copy_item, "clicked",
                         G_CALLBACK(on_context_copy), page);
        g_signal_connect(note_item, "clicked",
                         G_CALLBACK(on_context_note), page);
        g_signal_connect(highlight_item, "clicked",
                         G_CALLBACK(on_context_highlight), page);
        g_signal_connect(prayer_item, "clicked",
                         G_CALLBACK(on_context_prayer), page);
        gtk_box_append(GTK_BOX(menu), copy_item);
        gtk_box_append(GTK_BOX(menu), note_item);
        gtk_box_append(GTK_BOX(menu), highlight_item);
        gtk_box_append(GTK_BOX(menu), prayer_item);
        gtk_popover_set_child(page->context_menu, menu);
        gtk_widget_set_parent(popover, GTK_WIDGET(page->view));
    }
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(page->view));
    g_signal_connect(scrolled, "notify::width",
                     G_CALLBACK(on_reader_size_changed), page);
    g_signal_connect(scrolled, "map", G_CALLBACK(on_reader_mapped), page);
    study_widget = gsword_study_panel_widget(page->study_panel);
    page->study_widget = study_widget;
    page->paned = GTK_PANED(paned);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_widget_set_hexpand(paned, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(study_widget, FALSE);
    gtk_widget_set_size_request(study_widget, 240, -1);
    gtk_paned_set_start_child(GTK_PANED(paned), scrolled);
    gtk_paned_set_end_child(GTK_PANED(paned), study_widget);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(paned), FALSE);
    settings = gsword_window_get_settings(window);
    if (settings != NULL) {
        study_width = g_settings_get_int(settings, "study-width");
        if (study_width < 240 || study_width > 800)
            study_width = 320;
    }
    page->study_width = study_width;
    page->study_width_ready = FALSE;
    g_signal_connect(paned, "notify::position",
                     G_CALLBACK(on_study_position_changed), page);
    g_signal_connect(paned, "notify::width",
                     G_CALLBACK(on_paned_width_changed), page);
    g_signal_connect(study_widget, "notify::visible",
                     G_CALLBACK(on_study_visible_changed), page);

    page->empty_bar = empty_bar;
    gtk_widget_set_margin_top(empty_bar, 8);
    gtk_widget_set_margin_bottom(empty_bar, 8);
    gtk_widget_set_margin_start(empty_bar, 12);
    gtk_widget_set_margin_end(empty_bar, 12);
    gtk_widget_add_css_class(empty_label, "dim-label");
    gtk_widget_set_hexpand(empty_label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(empty_label), 0.0f);
    gtk_widget_add_css_class(empty_manage, "suggested-action");
    g_signal_connect(empty_manage, "clicked",
                     G_CALLBACK(on_empty_manage_modules), page);
    gtk_box_append(GTK_BOX(empty_bar), empty_label);
    gtk_box_append(GTK_BOX(empty_bar), empty_manage);
    gtk_widget_set_visible(empty_bar, FALSE);

    gtk_box_append(GTK_BOX(reader_box),
                   gsword_reader_toolbar_widget(page->toolbar));
    gtk_box_append(GTK_BOX(reader_box), empty_bar);
    gtk_box_append(GTK_BOX(reader_box), paned);

    gsword_study_panel_bind_toggle(
        page->study_panel,
        gsword_reader_toolbar_get_study_toggle(page->toolbar));

    {
        GtkEventController *keys = gtk_event_controller_key_new();

        g_signal_connect(keys, "key-pressed",
                         G_CALLBACK(on_reader_key_pressed), page);
        gtk_widget_add_controller(reader_box, keys);
    }
    update_reading_margins(page);
    g_object_set_data_full(G_OBJECT(reader_box), "gsword-reader-page", page,
                           reader_page_free);
    return page;
}
