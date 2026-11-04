/*
 * workspace-pane.c: a single comparison pane in the gsword comparison workspace
 */

#include "app-locale.h"
#include "workspace-pane.h"
#include "workspace-page.h"
#include "window-private.h"
#include "gsword-reference-chooser.h"

#include <glib/gi18n.h>

typedef enum {
    PENDING_SCROLL_NONE,
    PENDING_SCROLL_RESTORE,
    PENDING_SCROLL_VERSE,
    PENDING_SCROLL_ACTIVE
} PendingScroll;

struct _GswordWorkspacePane {
    GswordWindow *window;
    GswordWorkspacePage *page;
    GtkWidget *root;
    GtkWidget *header;
    GtkDropDown *module_selector;
    GtkStringList *module_names;
    GswordReferenceChooser *reference_chooser;
    gchar *versification;
    gchar *locale;
    GtkDropDown *follow_mode;
    GtkEventController *focus;
    gint mode;
    GtkScrolledWindow *scroll;
    GtkTextView *view;
    GtkTextTag *active_tag;
    GtkTextTag *diff_tag;
    GswModule *module;
    gchar *module_name;
    guint id;
    guint updating;
    guint scroll_idle;
    guint scrolling_idle;
    gboolean scrolling;
    gboolean press_pending;
    gdouble press_x;
    gdouble press_y;
    PendingScroll pending_scroll;
    gint pending_verse;
    gdouble restore_scroll_value;
};

static guint workspace_pane_next_id = 1;

static void scroll_to_active_now(GswordWorkspacePane *pane);
static void scroll_to_verse_now(GswordWorkspacePane *pane, gint verse);
static void schedule_scroll_restore(GswordWorkspacePane *pane,
                                    gdouble value);
static void schedule_scroll_to_active(GswordWorkspacePane *pane);
static void schedule_scroll_to_verse(GswordWorkspacePane *pane, gint verse);

gboolean
gsword_workspace_pane_supports_module(GswModuleInfo *info)
{
    return info != NULL &&
           gsw_module_info_is_supported(info) &&
           gsword_window_module_effective_type(info) ==
               GSW_MODULE_TYPE_BIBLES;
}

static void
set_initial_key(GswModule *module)
{
    if (module == NULL)
        return;
    if (gsw_key_get_kind(gsw_module_get_key(module)) == GSW_KEY_KIND_VERSE) {
        if (gsw_module_set_key_text(module, "Gen 1:1", NULL))
            return;
        if (gsw_module_set_key_text(module, "Matt 1:1", NULL))
            return;
    }
    gsw_module_set_position(module, GSW_POSITION_TOP, NULL);
}

static void
fill_modules(GswordWorkspacePane *pane)
{
    GswManager *manager = gsword_window_get_manager(pane->window);
    GPtrArray *infos;
    GtkStringList *names;
    guint i;

    if (manager == NULL)
        return;
    infos = gsw_manager_get_module_infos(manager);
    names = gtk_string_list_new(NULL);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (gsword_workspace_pane_supports_module(info))
            gtk_string_list_append(names,
                                   gsw_module_info_get_name(info));
    }
    g_ptr_array_unref(infos);
    pane->updating++;
    if (pane->module_selector != NULL)
        gtk_drop_down_set_model(pane->module_selector, NULL);
    g_clear_object(&pane->module_names);
    pane->module_names = names;
    if (pane->module_selector != NULL)
        gtk_drop_down_set_model(pane->module_selector, G_LIST_MODEL(names));
    pane->updating--;
}

static gboolean
clear_programmatic_scroll(gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;

    pane->scrolling_idle = 0;
    pane->scrolling = FALSE;
    return G_SOURCE_REMOVE;
}

static void
begin_programmatic_scroll(GswordWorkspacePane *pane)
{
    pane->scrolling = TRUE;
    if (pane->scrolling_idle != 0)
        g_source_remove(pane->scrolling_idle);
    pane->scrolling_idle = g_timeout_add(
        100, clear_programmatic_scroll, pane);
}

static gboolean
find_verse_line(GswordWorkspacePane *pane, gint verse,
                GtkTextIter *line_start, GtkTextIter *line_end)
{
    GtkTextBuffer *buffer;
    gint line_count;
    gint line;

    if (pane == NULL || pane->view == NULL || verse < 1)
        return FALSE;
    buffer = gtk_text_view_get_buffer(pane->view);
    line_count = gtk_text_buffer_get_line_count(buffer);
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
        g_free(text);
        if (found == verse) {
            if (line_start != NULL)
                *line_start = start;
            if (line_end != NULL)
                *line_end = end;
            return TRUE;
        }
    }
    return FALSE;
}

static void
apply_active(GswordWorkspacePane *pane)
{
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;
    GswKey *key;
    gint active;

    if (pane->view == NULL || pane->active_tag == NULL || pane->module == NULL)
        return;
    buffer = gtk_text_view_get_buffer(pane->view);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gtk_text_buffer_remove_tag(buffer, pane->active_tag, &start, &end);
    key = gsw_module_get_key(pane->module);
    if (gsw_key_get_kind(key) != GSW_KEY_KIND_VERSE)
        return;
    active = gsw_verse_key_get_verse((GswVerseKey *)key);
    if (active < 1)
        return;
    if (find_verse_line(pane, active, &start, &end))
        gtk_text_buffer_apply_tag(buffer, pane->active_tag, &start, &end);
}

static gchar *
normalize_verse_text(const gchar *line)
{
    const gchar *p = line;
    GString *result;

    if (p == NULL)
        return g_strdup("");
    while (g_ascii_isspace(*p))
        p++;
    while (g_ascii_isdigit(*p))
        p++;
    while (g_ascii_isspace(*p))
        p++;
    result = g_string_new(NULL);
    while (*p != '\0') {
        if (g_ascii_isspace(*p)) {
            if (result->len > 0 &&
                result->str[result->len - 1] != ' ')
                g_string_append_c(result, ' ');
        } else {
            g_string_append_c(result, *p);
        }
        p++;
    }
    while (result->len > 0 &&
           result->str[result->len - 1] == ' ')
        g_string_truncate(result, result->len - 1);
    return g_string_free(result, FALSE);
}

GHashTable *
gsword_workspace_pane_dup_verse_texts(GswordWorkspacePane *pane)
{
    GHashTable *verses = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                               NULL, g_free);
    GtkTextBuffer *buffer;
    gint line_count;
    gint line;

    if (pane == NULL || pane->view == NULL)
        return verses;
    buffer = gtk_text_view_get_buffer(pane->view);
    line_count = gtk_text_buffer_get_line_count(buffer);
    for (line = 1; line < line_count; line++) {
        GtkTextIter start;
        GtkTextIter end;
        gchar *text;
        gchar *normalized;
        gint verse;

        gtk_text_buffer_get_iter_at_line(buffer, &start, line);
        end = start;
        if (!gtk_text_iter_forward_line(&end))
            gtk_text_buffer_get_end_iter(buffer, &end);
        text = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
        verse = gsword_window_verse_number_from_line(text);
        if (verse > 0) {
            normalized = normalize_verse_text(text);
            g_hash_table_replace(verses, GINT_TO_POINTER(verse), normalized);
        }
        g_free(text);
    }
    return verses;
}

void
gsword_workspace_pane_set_diff_verses(GswordWorkspacePane *pane,
                                      const GHashTable *verses)
{
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;
    gint line_count;
    gint line;

    if (pane == NULL || pane->view == NULL || pane->diff_tag == NULL)
        return;
    buffer = gtk_text_view_get_buffer(pane->view);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gtk_text_buffer_remove_tag(buffer, pane->diff_tag, &start, &end);
    if (verses == NULL || g_hash_table_size((GHashTable *)verses) == 0)
        return;
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
        if (verse > 0 &&
            g_hash_table_contains((GHashTable *)verses,
                                  GINT_TO_POINTER(verse)))
            gtk_text_buffer_apply_tag(buffer, pane->diff_tag,
                                      &line_start, &line_end);
    }
}

static void
scroll_to_active_now(GswordWorkspacePane *pane)
{
    GtkTextIter start;
    GswKey *key;
    gint active;

    if (pane == NULL || pane->view == NULL || pane->module == NULL)
        return;
    key = gsw_module_get_key(pane->module);
    if (gsw_key_get_kind(key) != GSW_KEY_KIND_VERSE)
        return;
    active = gsw_verse_key_get_verse((GswVerseKey *)key);
    if (active < 1 || !find_verse_line(pane, active, &start, NULL))
        return;
    begin_programmatic_scroll(pane);
    gtk_text_view_scroll_to_iter(pane->view, &start, 0.1, FALSE, 0.0, 0.0);
}

void
gsword_workspace_pane_scroll_to_active(GswordWorkspacePane *pane)
{
    if (pane == NULL || pane->view == NULL || pane->module == NULL)
        return;
    if (pane->scroll_idle != 0) {
        g_source_remove(pane->scroll_idle);
        pane->scroll_idle = 0;
    }
    scroll_to_active_now(pane);
    schedule_scroll_to_active(pane);
}

gint
gsword_workspace_pane_get_top_verse(GswordWorkspacePane *pane)
{
    GtkTextView *view;
    GtkTextBuffer *buffer;
    GtkTextIter iter;
    gint bx;
    gint by;
    gint line;
    gint line_count;

    if (pane == NULL || pane->view == NULL)
        return 0;
    view = pane->view;
    gtk_text_view_window_to_buffer_coords(view, GTK_TEXT_WINDOW_WIDGET,
                                          0, 0, &bx, &by);
    gtk_text_view_get_iter_at_location(view, &iter, bx, by);
    buffer = gtk_text_view_get_buffer(view);
    line_count = gtk_text_buffer_get_line_count(buffer);
    for (line = gtk_text_iter_get_line(&iter); line < line_count; line++) {
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
        if (verse > 0)
            return verse;
    }
    return 0;
}

static void
scroll_to_verse_now(GswordWorkspacePane *pane, gint verse)
{
    GtkTextIter start;
    GtkAdjustment *adjustment;

    if (pane == NULL || pane->view == NULL || pane->scroll == NULL)
        return;
    adjustment = gtk_scrolled_window_get_vadjustment(pane->scroll);
    if (adjustment == NULL)
        return;
    if (verse > 0 && find_verse_line(pane, verse, &start, NULL)) {
        begin_programmatic_scroll(pane);
        gtk_text_view_scroll_to_iter(pane->view, &start, 0.0, TRUE,
                                     0.0, 0.0);
    } else {
        begin_programmatic_scroll(pane);
        gtk_adjustment_set_value(adjustment, 0.0);
    }
}

void
gsword_workspace_pane_scroll_to_verse(GswordWorkspacePane *pane, gint verse)
{
    if (pane == NULL || pane->view == NULL)
        return;
    if (pane->scroll_idle != 0) {
        g_source_remove(pane->scroll_idle);
        pane->scroll_idle = 0;
    }
    scroll_to_verse_now(pane, verse);
    schedule_scroll_to_verse(pane, verse);
}

static gboolean
pending_scroll_idle(gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;
    PendingScroll pending = pane->pending_scroll;
    gint verse = pane->pending_verse;

    pane->scroll_idle = 0;
    pane->pending_scroll = PENDING_SCROLL_NONE;
    if (pending == PENDING_SCROLL_RESTORE) {
        GtkAdjustment *adjustment;

        if (pane->scroll == NULL)
            return G_SOURCE_REMOVE;
        adjustment = gtk_scrolled_window_get_vadjustment(pane->scroll);
        if (adjustment == NULL)
            return G_SOURCE_REMOVE;
        begin_programmatic_scroll(pane);
        gtk_adjustment_set_value(adjustment, pane->restore_scroll_value);
    } else if (pending == PENDING_SCROLL_VERSE) {
        scroll_to_verse_now(pane, verse);
    } else if (pending == PENDING_SCROLL_ACTIVE) {
        scroll_to_active_now(pane);
    }
    return G_SOURCE_REMOVE;
}

static void
schedule_pending_scroll(GswordWorkspacePane *pane, PendingScroll pending,
                        gint verse, gdouble value)
{
    if (pane == NULL)
        return;
    pane->pending_scroll = pending;
    pane->pending_verse = verse;
    pane->restore_scroll_value = value;
    if (pane->scroll_idle != 0)
        g_source_remove(pane->scroll_idle);
    pane->scroll_idle = g_timeout_add(25, pending_scroll_idle, pane);
}

static void
schedule_scroll_restore(GswordWorkspacePane *pane, gdouble value)
{
    schedule_pending_scroll(pane, PENDING_SCROLL_RESTORE, 0, value);
}

static void
schedule_scroll_to_active(GswordWorkspacePane *pane)
{
    schedule_pending_scroll(pane, PENDING_SCROLL_ACTIVE, 0, 0.0);
}

static void
schedule_scroll_to_verse(GswordWorkspacePane *pane, gint verse)
{
    schedule_pending_scroll(pane, PENDING_SCROLL_VERSE, verse, 0.0);
}

static void
set_context(GswordWorkspacePane *pane)
{
    GswModuleInfo *info;
    const gchar *versification = NULL;
    gchar *locale;

    if (pane->reference_chooser == NULL || pane->module == NULL)
        return;
    info = gsw_module_get_info(pane->module);
    if (info != NULL)
        versification = gsw_module_info_get_versification(info);
    if (versification == NULL || *versification == '\0')
        versification = "KJV";
    if (g_strcmp0(pane->versification, versification) != 0) {
        g_free(pane->versification);
        pane->versification = g_strdup(versification);
        gsword_reference_chooser_set_versification(pane->reference_chooser,
                                                   versification);
    }
    locale = gsword_window_book_name_locale(pane->window, info);
    if (g_strcmp0(pane->locale, locale) != 0) {
        g_free(pane->locale);
        pane->locale = g_strdup(locale);
        gsword_reference_chooser_set_locale(pane->reference_chooser, locale);
    }
    g_free(locale);
    gsword_window_apply_book_name_locale(pane->window, pane->module);
}

void
gsword_workspace_pane_apply_book_name_policy(GswordWorkspacePane *pane)
{
    if (pane == NULL || pane->module == NULL)
        return;
    gsword_window_apply_book_name_locale(pane->window, pane->module);
    set_context(pane);
}

static void
sync_chooser(GswordWorkspacePane *pane)
{
    GswKey *key;
    GswVerseKey *verse_key;
    gint book;
    gint chapter;
    gint verse;

    if (pane->reference_chooser == NULL)
        return;
    set_context(pane);
    if (pane->module == NULL ||
        gsw_key_get_kind(gsw_module_get_key(pane->module)) !=
            GSW_KEY_KIND_VERSE) {
        gtk_widget_set_sensitive(GTK_WIDGET(pane->reference_chooser), FALSE);
        return;
    }
    gtk_widget_set_sensitive(GTK_WIDGET(pane->reference_chooser), TRUE);
    key = gsw_module_get_key(pane->module);
    verse_key = (GswVerseKey *)key;
    book = gsw_verse_key_get_global_book_number(verse_key);
    chapter = gsw_verse_key_get_chapter(verse_key);
    verse = MAX(gsw_verse_key_get_verse(verse_key), 1);
    if (book < 0 || chapter < 1)
        return;
    gsword_reference_chooser_set_reference(pane->reference_chooser,
                                           book, chapter, verse);
}

static void
render(GswordWorkspacePane *pane)
{
    GError *error = NULL;
    GswModuleInfo *info;
    GswModuleType type;
    GtkAdjustment *adjustment;
    gdouble old_scroll = 0.0;
    gchar *text = NULL;

    adjustment = pane->scroll != NULL
               ? gtk_scrolled_window_get_vadjustment(pane->scroll) : NULL;
    if (adjustment != NULL)
        old_scroll = gtk_adjustment_get_value(adjustment);
    if (pane->module == NULL) {
        gsword_window_set_view_text(pane->view,
                                    _("No module selected."));
        return;
    }
    info = gsw_module_get_info(pane->module);
    type = gsword_window_module_effective_type(info);
    if (type == GSW_MODULE_TYPE_BIBLES) {
        text = gsword_window_chapter_text_for_module_with_missing(
            pane->module, TRUE);
    }
    if (text == NULL)
        text = gsw_module_get_text(pane->module, &error);
    if (text == NULL || *text == '\0') {
        g_free(text);
        text = g_strdup(type == GSW_MODULE_TYPE_COMMENTARIES
                      ? _("No commentary for this verse.")
                      : _("No content for this reference."));
    }
    gsword_window_set_view_text(pane->view, text);
    g_free(text);
    g_clear_error(&error);
    {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(pane->view);
        GtkTextIter start;

        gtk_text_buffer_get_start_iter(buffer, &start);
        gtk_text_buffer_place_cursor(buffer, &start);
    }
    sync_chooser(pane);
    apply_active(pane);
    gsword_workspace_page_refresh_diff(pane->page);
    if (adjustment != NULL) {
        begin_programmatic_scroll(pane);
        gtk_adjustment_set_value(adjustment, old_scroll);
    }
    schedule_scroll_restore(pane, old_scroll);
}

static void
set_module(GswordWorkspacePane *pane, const gchar *name, const gchar *key)
{
    GswManager *manager;
    GError *error = NULL;
    guint i;
    guint selected = GTK_INVALID_LIST_POSITION;

    if (pane->module_selector == NULL)
        return;
    if (name == NULL && pane->module_names != NULL &&
        gtk_string_list_get_string(pane->module_names, 0) != NULL)
        name = gtk_string_list_get_string(pane->module_names, 0);
    if (name == NULL)
        return;

    if (pane->module_names != NULL) {
        for (i = 0; i < g_list_model_get_n_items(
                            G_LIST_MODEL(pane->module_names)); i++) {
            const gchar *candidate = gtk_string_list_get_string(
                pane->module_names, i);

            if (g_strcmp0(candidate, name) == 0) {
                selected = i;
                break;
            }
        }
    }
    if (selected == GTK_INVALID_LIST_POSITION) {
        if (pane->module_names == NULL ||
            g_list_model_get_n_items(G_LIST_MODEL(pane->module_names)) == 0) {
            gsword_window_set_view_text(pane->view,
                                        _("No Bible text module is installed"));
            return;
        }
        name = gtk_string_list_get_string(pane->module_names, 0);
        selected = 0;
    }
    pane->updating++;
    if (selected != GTK_INVALID_LIST_POSITION)
        gtk_drop_down_set_selected(pane->module_selector, selected);
    pane->updating--;

    g_free(pane->module_name);
    pane->module_name = g_strdup(name);
    manager = gsword_window_get_manager(pane->window);
    g_clear_object(&pane->module);
    if (manager != NULL)
        pane->module = gsw_manager_create_module(manager, name);
    if (pane->module == NULL) {
        gsword_window_set_view_text(pane->view, _("Cannot open module."));
        return;
    }

    if (key != NULL && *key != '\0' &&
        !gsw_module_set_key_text(pane->module, key, &error)) {
        g_clear_error(&error);
        set_initial_key(pane->module);
    } else if (key == NULL || *key == '\0') {
        set_initial_key(pane->module);
    }
    pane->updating++;
    {
        gboolean can_follow =
            gsw_key_get_kind(gsw_module_get_key(pane->module)) ==
            GSW_KEY_KIND_VERSE;

        gtk_widget_set_sensitive(GTK_WIDGET(pane->follow_mode), can_follow);
        if (!can_follow)
            gsword_workspace_pane_set_follow_mode(pane, 1);
    }
    pane->updating--;
    set_context(pane);
    render(pane);
}

void
gsword_workspace_pane_set_reference(GswordWorkspacePane *pane,
                                    const gchar *reference)
{
    GError *error = NULL;

    if (pane->module == NULL || reference == NULL || *reference == '\0')
        return;
    if (!gsw_module_set_key_text(pane->module, reference, &error)) {
        g_clear_error(&error);
        return;
    }
    render(pane);
}

static void
note_reference(GswordWorkspacePane *pane)
{
    gchar *reference = NULL;

    if (pane->module != NULL &&
        gsw_key_get_kind(gsw_module_get_key(pane->module)) ==
            GSW_KEY_KIND_VERSE) {
        GswVerseKey *key = (GswVerseKey *)gsw_module_get_key(pane->module);

        reference = g_strdup_printf(
            "%s %d:%d", gsw_verse_key_get_book_name(key),
            gsw_verse_key_get_chapter(key), gsw_verse_key_get_verse(key));
    }
    gsword_workspace_pane_scroll_to_active(pane);
    if (reference != NULL)
        gsword_workspace_page_sync_reference(pane->page, reference, pane);
    g_free(reference);
    gsword_workspace_page_save(pane->page);
}

static void
on_reference_activate(GswordReferenceChooser *chooser, gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;
    gint book;
    gint chapter;
    gint verse;
    gchar *osis;
    gchar *text;

    if (pane->module == NULL ||
        !gsword_reference_chooser_get_reference(chooser, &book, &chapter,
                                                &verse))
        return;
    gsword_workspace_page_set_active_pane(pane->page, pane);
    osis = gsword_reference_chooser_dup_osis(chooser);
    if (osis == NULL || *osis == '\0') {
        g_free(osis);
        return;
    }
    text = g_strdup_printf("%s %d:%d", osis, chapter, verse);
    gsword_workspace_pane_set_reference(pane, text);
    g_free(text);
    g_free(osis);
    note_reference(pane);
}

static void
on_module_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;
    const gchar *name;
    guint selected;

    (void)pspec;
    if (pane->updating || pane->module_names == NULL)
        return;
    selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    if (selected == GTK_INVALID_LIST_POSITION)
        return;
    name = gtk_string_list_get_string(pane->module_names, selected);
    if (name == NULL || g_strcmp0(name, pane->module_name) == 0)
        return;
    {
        gchar *key = gsword_workspace_pane_dup_storage_key(pane);

        set_module(pane, name, key);
        g_free(key);
    }
    gsword_workspace_page_save(pane->page);
}

static void
on_follow_mode_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));

    (void)pspec;
    if (pane->updating || selected == GTK_INVALID_LIST_POSITION)
        return;
    pane->mode = (gint)selected;
    if (pane->mode == 0) {
        const gchar *reference =
            gsword_workspace_page_get_reference(pane->page);

        if (reference != NULL) {
            gsword_workspace_pane_set_reference(pane, reference);
            gsword_workspace_pane_scroll_to_active(pane);
            gsword_workspace_page_scroll_to_reference(pane->page, pane);
        }
    }
    gsword_workspace_page_save(pane->page);
}

static void
on_focus_enter(GtkEventControllerFocus *controller, gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;

    (void)controller;
    gsword_workspace_page_set_active_pane(pane->page, pane);
}

static void
on_pressed(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y,
           gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;

    (void)n_press;
    gsword_workspace_page_set_active_pane(pane->page, pane);
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture)) !=
        GDK_BUTTON_PRIMARY)
        return;
    pane->press_pending = TRUE;
    pane->press_x = x;
    pane->press_y = y;
}

static void
on_released(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y,
            gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;
    GtkTextView *view;
    GtkTextBuffer *buffer;
    GtkTextIter iter;
    GtkTextIter line_start;
    GtkTextIter line_end;
    gchar *line;
    GError *error = NULL;
    gdouble dx;
    gdouble dy;
    gint verse;
    gint bx;
    gint by;
    gint trailing;
    gint line_y;
    gint line_height;
    GtkAdjustment *adjustment;
    gdouble old_scroll = 0.0;
    GswModuleInfo *info;

    (void)n_press;
    if (!pane->press_pending)
        return;
    pane->press_pending = FALSE;
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture)) !=
        GDK_BUTTON_PRIMARY)
        return;
    dx = x - pane->press_x;
    dy = y - pane->press_y;
    if (dx * dx + dy * dy > 16.0)
        return;
    if (pane->module == NULL)
        return;
    info = gsw_module_get_info(pane->module);
    if (gsword_window_module_effective_type(info) != GSW_MODULE_TYPE_BIBLES)
        return;
    view = GTK_TEXT_VIEW(gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(gesture)));
    adjustment = pane->scroll != NULL
               ? gtk_scrolled_window_get_vadjustment(pane->scroll) : NULL;
    if (adjustment != NULL)
        old_scroll = gtk_adjustment_get_value(adjustment);
    gtk_text_view_window_to_buffer_coords(view, GTK_TEXT_WINDOW_WIDGET,
                                          (gint)x, (gint)y, &bx, &by);
    if (!gtk_text_view_get_iter_at_position(view, &iter, &trailing, bx, by))
        return;
    if (gtk_text_iter_get_line(&iter) == 0)
        return;
    gtk_text_view_get_line_yrange(view, &iter, &line_y, &line_height);
    if (by < line_y || by >= line_y + line_height)
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
    if (verse < 1 ||
        verse > gsw_verse_key_get_verse_max(
            (GswVerseKey *)gsw_module_get_key(pane->module)))
        return;
    gsw_verse_key_set_verse((GswVerseKey *)gsw_module_get_key(pane->module),
                            verse);
    if (!gsw_module_set_key_text(pane->module,
                                 gsw_module_get_key_text(pane->module),
                                 &error)) {
        g_clear_error(&error);
        return;
    }
    apply_active(pane);
    sync_chooser(pane);
    if (adjustment != NULL) {
        begin_programmatic_scroll(pane);
        gtk_adjustment_set_value(adjustment, old_scroll);
    }
    {
        GswVerseKey *key = (GswVerseKey *)gsw_module_get_key(pane->module);
        gchar *reference = g_strdup_printf(
            "%s %d:%d", gsw_verse_key_get_book_name(key),
            gsw_verse_key_get_chapter(key), gsw_verse_key_get_verse(key));

        gsword_workspace_page_sync_reference(pane->page, reference, pane);
        g_free(reference);
    }
    if (gsword_window_is_page_visible(pane->window, "panes"))
        gsword_window_set_status(pane->window, _("Comparison updated"));
    gsword_workspace_page_save(pane->page);
    schedule_scroll_restore(pane, old_scroll);
}

static void
on_close_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_workspace_page_remove_pane(
        ((GswordWorkspacePane *)user_data)->page, user_data);
}

static void
on_drag_update(GtkGestureDrag *gesture, gdouble x, gdouble y,
               gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;
    gdouble dx = 0.0;
    gdouble dy = 0.0;

    (void)x;
    (void)y;
    gtk_gesture_drag_get_offset(gesture, &dx, &dy);
    if ((dx * dx + dy * dy) < 16.0)
        return;
    gtk_widget_add_css_class(pane->root, "dragging");
    gsword_window_set_status(pane->window,
                             _("Drag to another comparison to rearrange"));
}

static void
on_drag_end(GtkGestureDrag *gesture, gdouble x, gdouble y,
            gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;
    gdouble dx = 0.0;
    gdouble dy = 0.0;

    (void)x;
    (void)y;
    gtk_widget_remove_css_class(pane->root, "dragging");
    gtk_gesture_drag_get_offset(gesture, &dx, &dy);
    gsword_workspace_page_reorder_from_drag(pane->page, pane, dx, dy);
}

static void
setup_drag(GswordWorkspacePane *pane, GtkWidget *handle)
{
    GtkGesture *gesture = gtk_gesture_drag_new();

    gtk_widget_set_can_target(handle, TRUE);
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture),
                                  GDK_BUTTON_PRIMARY);
    g_signal_connect(gesture, "drag-update", G_CALLBACK(on_drag_update), pane);
    g_signal_connect(gesture, "drag-end", G_CALLBACK(on_drag_end), pane);
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(gesture), GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(pane->header, GTK_EVENT_CONTROLLER(gesture));
}

static void
on_scrolled(GtkAdjustment *adjustment, gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;

    if (pane->scrolling)
        return;
    gsword_workspace_page_sync_scroll(pane->page, pane, adjustment);
}

GswordWorkspacePane *
gsword_workspace_pane_new(GswordWindow *window, GswordWorkspacePage *page,
                          const gchar *module_name, const gchar *key,
                          gboolean follow, gboolean save)
{
    GswordWorkspacePane *pane = g_new0(GswordWorkspacePane, 1);
    GtkWidget *header;
    GtkWidget *handle;
    GtkWidget *close;
    GtkWidget *key_box;
    GtkWidget *scrolled;
    GtkGesture *click;
    gchar *initial_key = key != NULL
                       ? g_strdup(key)
                       : gsword_window_current_start_reference(window);

    pane->window = window;
    pane->page = page;
    pane->id = workspace_pane_next_id++;
    pane->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(pane->root, "workspace-pane");
    gtk_widget_set_hexpand(pane->root, TRUE);
    gtk_widget_set_vexpand(pane->root, TRUE);
    gtk_widget_set_size_request(pane->root, 260, 200);

    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(header, "workspace-pane-header");
    gtk_widget_set_margin_start(header, 6);
    gtk_widget_set_margin_end(header, 6);
    gtk_widget_set_margin_top(header, 4);
    gtk_widget_set_margin_bottom(header, 4);
    pane->header = header;

    handle = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_tooltip_text(handle, _("Drag to rearrange comparison"));
    gtk_widget_set_cursor_from_name(handle, "move");
    gtk_widget_set_size_request(handle, 20, -1);
    gtk_box_append(GTK_BOX(handle),
                   gtk_image_new_from_icon_name("list-drag-handle-symbolic"));
    gtk_box_append(GTK_BOX(header), handle);

    pane->module_selector = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    gtk_widget_set_hexpand(GTK_WIDGET(pane->module_selector), TRUE);
    g_signal_connect(pane->module_selector, "notify::selected",
                     G_CALLBACK(on_module_changed), pane);
    gtk_box_append(GTK_BOX(header), GTK_WIDGET(pane->module_selector));

    pane->follow_mode = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(
        (const gchar *[]) { _("Follow"), _("Pinned"), _("Free"), NULL }));
    pane->mode = follow ? 0 : 1;
    gtk_drop_down_set_selected(pane->follow_mode, (guint)pane->mode);
    gtk_widget_set_tooltip_text(GTK_WIDGET(pane->follow_mode),
                                _("Follow, pin, or free this comparison"));
    g_signal_connect(pane->follow_mode, "notify::selected",
                     G_CALLBACK(on_follow_mode_changed), pane);
    gtk_box_append(GTK_BOX(header), GTK_WIDGET(pane->follow_mode));

    close = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(close, "flat");
    gtk_widget_set_tooltip_text(close, _("Close comparison"));
    g_signal_connect(close, "clicked", G_CALLBACK(on_close_clicked), pane);
    gtk_box_append(GTK_BOX(header), close);

    key_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_start(key_box, 6);
    gtk_widget_set_margin_end(key_box, 6);
    gtk_widget_set_margin_bottom(key_box, 4);
    {
        gchar *locale = gsword_app_locale_name();

        pane->reference_chooser = GSWORD_REFERENCE_CHOOSER(
            gsword_reference_chooser_new_compact("KJV", locale));
        g_free(locale);
    }
    gtk_widget_set_hexpand(GTK_WIDGET(pane->reference_chooser), FALSE);
    gtk_widget_set_halign(GTK_WIDGET(pane->reference_chooser),
                          GTK_ALIGN_START);
    g_signal_connect(pane->reference_chooser, "activate",
                     G_CALLBACK(on_reference_activate), pane);
    gtk_box_append(GTK_BOX(key_box),
                   GTK_WIDGET(pane->reference_chooser));

    pane->view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(pane->view, FALSE);
    gtk_text_view_set_cursor_visible(pane->view, FALSE);
    gtk_text_view_set_wrap_mode(pane->view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(pane->view, 10);
    gtk_text_view_set_right_margin(pane->view, 10);
    gtk_text_view_set_top_margin(pane->view, 10);
    gtk_text_view_set_bottom_margin(pane->view, 10);
    pane->diff_tag = gtk_text_buffer_create_tag(
        gtk_text_view_get_buffer(pane->view), "workspace-diff",
        "background", "#fef3c7", NULL);
    pane->active_tag = gtk_text_buffer_create_tag(
        gtk_text_view_get_buffer(pane->view), "workspace-active",
        "background", "#dbeafe", "weight", PANGO_WEIGHT_SEMIBOLD, NULL);
    scrolled = gtk_scrolled_window_new();
    pane->scroll = GTK_SCROLLED_WINDOW(scrolled);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(pane->view));
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    g_signal_connect(gtk_scrolled_window_get_vadjustment(pane->scroll),
                     "value-changed", G_CALLBACK(on_scrolled), pane);

    click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click),
                                  GDK_BUTTON_PRIMARY);
    g_signal_connect(click, "pressed", G_CALLBACK(on_pressed), pane);
    g_signal_connect(click, "released", G_CALLBACK(on_released), pane);
    gtk_widget_add_controller(GTK_WIDGET(pane->view),
                              GTK_EVENT_CONTROLLER(click));
    {
        GtkEventController *focus = gtk_event_controller_focus_new();

        g_signal_connect(focus, "enter", G_CALLBACK(on_focus_enter), pane);
        pane->focus = focus;
        gtk_widget_add_controller(pane->root, focus);
    }

    gtk_box_append(GTK_BOX(pane->root), header);
    gtk_box_append(GTK_BOX(pane->root), key_box);
    gtk_box_append(GTK_BOX(pane->root), scrolled);

    fill_modules(pane);
    set_module(pane, module_name, initial_key != NULL ? initial_key : "");
    setup_drag(pane, handle);
    g_free(initial_key);

    if (save)
        gsword_workspace_page_save(page);
    return pane;
}

GtkWidget *
gsword_workspace_pane_widget(GswordWorkspacePane *pane)
{
    g_return_val_if_fail(pane != NULL, NULL);
    return pane->root;
}

const gchar *
gsword_workspace_pane_get_module_name(GswordWorkspacePane *pane)
{
    return pane != NULL ? pane->module_name : NULL;
}

GswModule *
gsword_workspace_pane_get_module(GswordWorkspacePane *pane)
{
    return pane != NULL ? pane->module : NULL;
}

gchar *
gsword_workspace_pane_dup_storage_key(GswordWorkspacePane *pane)
{
    GswKey *key;

    if (pane == NULL || pane->module == NULL)
        return NULL;
    key = gsw_module_get_key(pane->module);
    if (gsw_key_get_kind(key) == GSW_KEY_KIND_VERSE)
        return gsw_verse_key_get_osis_ref((GswVerseKey *)key);
    return g_strdup(gsw_module_get_key_text(pane->module));
}

gboolean
gsword_workspace_pane_get_follow(GswordWorkspacePane *pane)
{
    return pane != NULL && pane->mode == 0;
}

gint
gsword_workspace_pane_get_follow_mode(GswordWorkspacePane *pane)
{
    return pane != NULL ? pane->mode : 1;
}

void
gsword_workspace_pane_set_follow_mode(GswordWorkspacePane *pane, gint mode)
{
    if (pane == NULL)
        return;
    pane->mode = CLAMP(mode, 0, 2);
    if (pane->follow_mode != NULL) {
        pane->updating++;
        gtk_drop_down_set_selected(pane->follow_mode, (guint)pane->mode);
        pane->updating--;
    }
}

GtkScrolledWindow *
gsword_workspace_pane_get_scroll(GswordWorkspacePane *pane)
{
    return pane != NULL ? pane->scroll : NULL;
}

void
gsword_workspace_pane_free(GswordWorkspacePane *pane)
{
    if (pane == NULL)
        return;
    /*
     * Do not touch child widgets here.  During window teardown GTK may have
     * disposed them already, and during normal pane removal the root widget is
     * destroyed before this function runs.  Timers are ours to clean up; widget
     * signal handlers are removed by GTK when the widgets are finalized.
     */
    if (pane->scroll_idle != 0) {
        g_source_remove(pane->scroll_idle);
        pane->scroll_idle = 0;
    }
    if (pane->scrolling_idle != 0) {
        g_source_remove(pane->scrolling_idle);
        pane->scrolling_idle = 0;
    }
    g_clear_object(&pane->module);
    g_clear_object(&pane->module_names);
    g_free(pane->module_name);
    g_free(pane->versification);
    g_free(pane->locale);
    g_free(pane);
}
