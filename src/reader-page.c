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
    GswordReferenceChooser *reference_chooser;
    GtkTextTag *highlight_tag;
    GtkTextTag *active_verse_tag;
    GtkTextTag *range_tag;
    gint active_verse;
    gint range_start;
    gint range_end;
    gboolean range_active;
};

static void apply_active_verse(GswordReaderPage *page);
static void clear_range(GswordReaderPage *page);
static void apply_range(GswordReaderPage *page);
static void apply_highlights(GswordReaderPage *page);
static void select_verse_range(GswordReaderPage *page, gint anchor,
                               gint verse);
static void select_verse(GswordReaderPage *page, gint verse);
static void update_reference_status(GswordReaderPage *page);

static void
set_text(GswordReaderPage *page, const gchar *text)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(page->view);

    gtk_text_buffer_set_text(buffer, text != NULL ? text : "", -1);
}

static void
save_last_position(GswordReaderPage *page)
{
    GSettings *settings = gsword_window_get_settings(page->window);
    GswModule *module = gsword_window_get_module(page->window);
    const gchar *name;
    const gchar *key;

    if (settings == NULL || module == NULL)
        return;
    name = gsw_module_get_name(module);
    key = gsw_module_get_key_text(module);
    if (name != NULL)
        g_settings_set_string(settings, "last-module", name);
    if (key != NULL)
        g_settings_set_string(settings, "last-key", key);
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
    locale = gsword_window_locale_name();
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
    gchar *display_key = NULL;
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
        sync_reference_chooser(page);
        gsword_window_refresh_search_index(self);
        return;
    }

    if (gsw_key_get_kind(gsw_module_get_key(module)) == GSW_KEY_KIND_VERSE) {
        GswVerseKey *key = (GswVerseKey *)gsw_module_get_key(module);

        if (gsw_verse_key_get_book(key) >= 1 &&
            gsw_verse_key_get_chapter(key) >= 1) {
            text = gsword_window_chapter_text_for_module(module);
            display_key = g_strdup_printf(
                "%s %d:%d", gsw_verse_key_get_book_name(key),
                gsw_verse_key_get_chapter(key),
                MAX(gsw_verse_key_get_verse(key), 1));
        } else {
            text = gsw_module_get_text(module, &error);
        }
    } else {
        text = gsw_module_get_text(module, &error);
    }

    if (text == NULL) {
        gsword_window_show_error(self, _("Cannot read entry"), error);
        g_free(display_key);
        return;
    }
    set_text(page, text);
    g_free(text);

    if (display_key == NULL)
        display_key = g_strdup(gsw_module_get_key_text(module));

    status = g_strdup_printf("%s  |  %s",
                             gsw_module_get_name(module),
                             display_key != NULL ? display_key : "");
    gsword_window_set_status(self, status);
    g_free(status);
    g_free(display_key);

    apply_highlights(page);
    apply_active_verse(page);
    gsword_study_panel_refresh(page->study_panel);
    gsword_window_refresh_search_index(self);
    sync_reference_chooser(page);
    gsword_window_sync_workspace_from_reader(self);
    save_last_position(page);
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
    gchar *reference;
    gchar *status;

    if (module == NULL)
        return;
    reference = gsword_reader_page_current_reference(page);
    if (reference == NULL)
        return;
    sync_reference_chooser(page);
    status = g_strdup_printf("%s  |  %s",
                             gsw_module_get_name(module), reference);
    gsword_window_set_status(page->window, status);
    g_free(status);
    g_free(reference);
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
        gsword_window_set_status(self, _("Highlight removed"));
    } else {
        gsword_notes_store_set_highlight(
            notes, gsw_module_get_name(module), storage_key,
            "#fff0a8", &error);
        gsword_window_set_status(self, _("Verse highlighted"));
    }
    if (error != NULL)
        gsword_window_show_error(self, _("Cannot update highlight"), error);
    g_free(storage_key);
    apply_highlights(page);
}

void
gsword_reader_page_copy(GswordReaderPage *page)
{
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;
    gchar *text;
    GdkClipboard *clipboard;

    if (page == NULL)
        return;
    buffer = gtk_text_view_get_buffer(page->view);
    if (page->range_active && page->range_start > 0 &&
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
    } else {
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    }
    clipboard = gtk_widget_get_clipboard(GTK_WIDGET(page->window));
    gdk_clipboard_set_text(clipboard, text != NULL ? text : "");
    g_free(text);
    gsword_window_set_status(page->window, _("Copied to clipboard"));
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

static void
reader_page_free(gpointer data)
{
    g_free(data);
}

GswordReaderPage *
gsword_reader_page_new(GswordWindow *window)
{
    GswordReaderPage *page = g_new0(GswordReaderPage, 1);
    GtkWidget *reader_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *study_widget;
    GtkGesture *reader_click;

    page->window = window;
    page->root = reader_box;
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
    gtk_text_tag_set_priority(page->active_verse_tag, 2);
    reader_click = gtk_gesture_click_new();
    g_signal_connect(reader_click, "pressed",
                     G_CALLBACK(on_reader_clicked), page);
    gtk_widget_add_controller(GTK_WIDGET(page->view),
                              GTK_EVENT_CONTROLLER(reader_click));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(page->view));
    study_widget = gsword_study_panel_widget(page->study_panel);
    gtk_widget_set_vexpand(content_box, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(study_widget, FALSE);
    gtk_widget_set_size_request(study_widget, 320, -1);
    gtk_box_append(GTK_BOX(content_box), scrolled);
    gtk_box_append(GTK_BOX(content_box), study_widget);

    gtk_box_append(GTK_BOX(reader_box),
                   gsword_reader_toolbar_widget(page->toolbar));
    gtk_box_append(GTK_BOX(reader_box), content_box);

    gsword_study_panel_bind_toggle(
        page->study_panel,
        gsword_reader_toolbar_get_study_toggle(page->toolbar));

    g_object_set_data_full(G_OBJECT(reader_box), "gsword-reader-page", page,
                           reader_page_free);
    return page;
}
