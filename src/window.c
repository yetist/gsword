#include "window.h"
#include "notes-store.h"
#include "plan-store.h"
#include "app-paths.h"

#include <string.h>

struct _GswordWindow {
    AdwApplicationWindow parent_instance;
};

typedef struct {
    GswManager *manager;
    GSettings *settings;
    GswModule *module;
    GtkListBox *module_list;
    GtkStack *content_stack;
    GtkTextView *reader;
    GtkEntry *key_entry;
    GtkLabel *status_label;
    AdwDialog *preferences_dialog;
    GtkCssProvider *css;
    gchar *theme;
    gdouble font_scale;

    GtkWidget *study_panel;
    GtkToggleButton *study_toggle;
    GtkNotebook *study_notebook;
    GtkTextView *commentary_view;
    GtkTextView *strongs_view;
    GtkTextView *crossref_view;
    GtkTextView *lexicon_view;
    GtkEntry *lexicon_entry;
    gchar *commentary_name;
    gchar *lexicon_name;
    GswModule *commentary_module;
    GswModule *lexicon_module;

    GtkStringList *module_names;
    GtkDropDown *parallel_a;
    GtkDropDown *parallel_b;
    GtkEntry *parallel_key;
    GtkTextView *parallel_view_a;
    GtkTextView *parallel_view_b;
    GtkScrolledWindow *parallel_scroll_a;
    GtkScrolledWindow *parallel_scroll_b;
    GtkCheckButton *parallel_sync;
    GswModule *parallel_module_a;
    GswModule *parallel_module_b;
    gboolean syncing_scroll;

    GtkEntry *search_entry;
    GtkDropDown *search_type;
    GtkCheckButton *search_case;
    GtkListBox *search_results;
    GtkLabel *index_label;

    GswInstallManager *installer;
    GswStatusReporter *install_reporter;
    GPtrArray *install_sources;
    GtkStringList *source_names;
    GtkStringList *installed_names;
    GtkDropDown *source_dropdown;
    GtkDropDown *installed_dropdown;
    GtkEntry *install_search;
    GtkListBox *remote_list;
    GtkLabel *remote_title;
    GtkLabel *remote_description;
    GtkLabel *remote_status;
    GtkProgressBar *install_progress;
    GtkLabel *install_status;
    GtkButton *install_button;
    GtkButton *remove_button;
    GtkButton *cancel_button;
    GtkButton *refresh_source_button;
    GtkButton *refresh_sources_button;
    GCancellable *install_cancellable;
    GtkEntry *local_dir_entry;
    GtkEntry *local_name_entry;
    GtkButton *local_install_button;
    gchar *selected_remote_name;

    gboolean selecting_module;

    GswordPlanStore *plans;
    GtkDropDown *plan_selector;
    GtkStringList *plan_names;
    GPtrArray *plan_items;
    GtkLabel *plan_summary;
    GtkLabel *plan_today;
    GtkButton *plan_open_button;
    GtkButton *plan_complete_button;
    GtkListBox *plan_days;
    GtkEntry *plan_name_entry;
    GtkEntry *plan_start_entry;
    GtkSpinButton *plan_days_spin;
    GtkSpinButton *plan_chapters_spin;
    gint64 selected_plan_id;

    GtkDropDown *devotion_selector;
    GtkStringList *devotion_names;
    GtkTextView *devotion_view;
    GtkLabel *devotion_title;

    GtkListBox *memory_list;
    GPtrArray *memory_items;
    GtkTextView *memory_card;
    GtkLabel *memory_title;
    GtkLabel *memory_due;
    GtkEntry *memory_input;
    GtkLabel *memory_result;
    GtkDropDown *memory_mode;
    gchar *memory_module;
    gchar *memory_key;
    gchar *memory_display;
    gchar *memory_text;
    gint64 memory_id;

    GtkTextView *teaching_view;
    GtkTextView *teaching_notes;
    GtkEntry *teaching_ref_entry;
    GtkButton *teaching_fullscreen_button;
    gchar *teaching_module;
    gchar *teaching_reference;
    gboolean teaching_fullscreen;

    GswordNotesStore *notes;
    GtkEntry *note_search;
    GtkEntry *note_key_entry;
    GtkTextView *note_text;
    GtkEntry *note_tags_entry;
    GtkListBox *notes_list;
    GtkListBox *tags_list;
    GtkListBox *lists_list;
    GtkListBox *list_items;
    GtkEntry *new_list_entry;
    GtkTextTag *highlight_tag;
    gchar *note_module;
    gchar *note_storage_key;
    gchar *note_display_key;
    gint64 note_id;
    gint64 selected_list_id;
} GswordWindowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GswordWindow, gsword_window, ADW_TYPE_APPLICATION_WINDOW)

static void gsword_window_update_index_label(GswordWindow *self);
static void gsword_window_parallel_load(GswordWindow *self);
static void gsword_window_open_note_for_current(GswordWindow *self);
static void gsword_window_refresh_notes(GswordWindow *self);
static gchar *gsword_window_current_storage_key(GswordWindow *self);
static gchar *gsword_window_current_display_key(GswordWindow *self);
static void gsword_window_apply_highlights(GswordWindow *self);
static void gsword_window_load_module(GswordWindow *self, const gchar *name);
static gboolean gsword_window_set_key_text(GswordWindow *self,
                                           const gchar *text);
static void gsword_window_refresh_remote_modules(GswordWindow *self);
static void gsword_window_refresh_install_sources(GswordWindow *self);
static void gsword_window_refresh_installed_dropdown(GswordWindow *self);
static void on_refresh_sources_finished(GObject *source, GAsyncResult *result,
                                        gpointer user_data);
static void on_refresh_remote_finished(GObject *source, GAsyncResult *result,
                                       gpointer user_data);
static void on_install_finished(GObject *source, GAsyncResult *result,
                                gpointer user_data);
static void on_remove_finished(GObject *source, GAsyncResult *result,
                               gpointer user_data);
static void on_plan_day_toggled(GtkCheckButton *button, gpointer user_data);
static void gsword_window_refresh_plans(GswordWindow *self);
static void gsword_window_refresh_memory(GswordWindow *self);
static void gsword_window_refresh_devotion(GswordWindow *self);
static void gsword_window_teaching_load(GswordWindow *self);

static void
gsword_window_set_status(GswordWindow *self, const gchar *message)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    gtk_label_set_text(priv->status_label, message != NULL ? message : "");
}

static void
gsword_window_set_text(GswordWindow *self, const gchar *text)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(priv->reader);

    gtk_text_buffer_set_text(buffer, text != NULL ? text : "", -1);
}

static void
gsword_window_show_error(GswordWindow *self, const gchar *prefix,
                         GError *error)
{
    const gchar *detail = error != NULL ? error->message : "unknown error";
    gchar *message = g_strdup_printf("%s: %s", prefix, detail);

    gsword_window_set_status(self, message);
    g_free(message);

    if (gtk_widget_get_mapped(GTK_WIDGET(self))) {
        AdwDialog *dialog = adw_alert_dialog_new(prefix, NULL);

        adw_alert_dialog_format_body(ADW_ALERT_DIALOG(dialog), "%s", detail);
        adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog),
                                      "close", "Close");
        adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog),
                                              "close");
        adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog),
                                            "close");
        /* AdwDialog is floating; adw_dialog_present() takes ownership. */
        adw_dialog_present(dialog, GTK_WIDGET(self));
    }
    g_clear_error(&error);
}

static void
gsword_window_apply_css(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gchar *css_text;
    gdouble points = 14.0 * priv->font_scale;

    if (priv->css == NULL)
        return;
    css_text = g_strdup_printf(
        ".gsw-reader { font-size: %.1fpt; }\n"
        ".memory-card { font-size: 16pt; }\n"
        ".teaching-reader { font-size: 18pt; }", points);
    gtk_css_provider_load_from_string(priv->css, css_text);
    g_free(css_text);
}

static void
gsword_window_apply_theme(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    AdwStyleManager *manager = adw_style_manager_get_default();
    AdwColorScheme scheme = ADW_COLOR_SCHEME_DEFAULT;

    if (g_strcmp0(priv->theme, "light") == 0)
        scheme = ADW_COLOR_SCHEME_FORCE_LIGHT;
    else if (g_strcmp0(priv->theme, "dark") == 0)
        scheme = ADW_COLOR_SCHEME_FORCE_DARK;
    adw_style_manager_set_color_scheme(manager, scheme);
}

static void
gsword_window_save_last_position(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *name;
    const gchar *key;

    if (priv->settings == NULL || priv->module == NULL)
        return;
    name = gsw_module_get_name(priv->module);
    key = gsw_module_get_key_text(priv->module);
    if (name != NULL)
        g_settings_set_string(priv->settings, "last-module", name);
    if (key != NULL)
        g_settings_set_string(priv->settings, "last-key", key);
}

static gchar *
gsword_window_chapter_text_for_module(GswModule *module)
{
    GswVerseKey *key;
    gchar *original;
    gchar *book_name;
    gint chapter;
    gint maximum;
    gint verse;
    GString *result;

    if (module == NULL ||
        gsw_key_get_kind(gsw_module_get_key(module)) != GSW_KEY_KIND_VERSE)
        return NULL;

    key = (GswVerseKey *)gsw_module_get_key(module);
    if (gsw_verse_key_get_book(key) < 1 ||
        gsw_verse_key_get_chapter(key) < 1)
        return NULL;

    chapter = gsw_verse_key_get_chapter(key);
    maximum = gsw_verse_key_get_verse_max(key);
    if (maximum < 1)
        return NULL;

    original = g_strdup(gsw_module_get_key_text(module));
    book_name = g_strdup(gsw_verse_key_get_book_name(key));
    result = g_string_new(NULL);
    g_string_append_printf(result, "%s %d\n\n", book_name, chapter);

    for (verse = 1; verse <= maximum; verse++) {
        GError *error = NULL;
        gchar *text;

        gsw_verse_key_set_verse(key, verse);
        if (!gsw_module_set_key_text(module,
                                     gsw_module_get_key_text(module),
                                     &error)) {
            g_clear_error(&error);
            break;
        }
        text = gsw_module_get_text(module, &error);
        if (text != NULL && *text != '\0') {
            g_string_append_printf(result, "%d  %s\n\n", verse, text);
        }
        g_free(text);
        g_clear_error(&error);
    }

    if (original != NULL) {
        gsw_module_set_key_text(module, original, NULL);
        g_free(original);
    }
    g_free(book_name);
    return g_string_free(result, FALSE);
}

static void
gsword_window_set_view_text(GtkTextView *view, const gchar *text)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);

    gtk_text_buffer_set_text(buffer, text != NULL ? text : "", -1);
}

static gchar *
gsword_window_find_module_by_type(GswordWindow *self, GswModuleType type)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *infos;
    gchar *result = NULL;
    guint i;

    if (priv->manager == NULL)
        return NULL;
    infos = gsw_manager_get_module_infos(priv->manager);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (gsw_module_info_get_module_type(info) == type) {
            result = g_strdup(gsw_module_info_get_name(info));
            break;
        }
    }
    g_ptr_array_unref(infos);
    return result;
}

static void
gsword_window_ensure_study_modules(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->commentary_name == NULL)
        priv->commentary_name = gsword_window_find_module_by_type(
            self, GSW_MODULE_TYPE_COMMENTARIES);
    if (priv->lexicon_name == NULL)
        priv->lexicon_name = gsword_window_find_module_by_type(
            self, GSW_MODULE_TYPE_LEXDICTS);
    if (priv->commentary_module == NULL && priv->commentary_name != NULL)
        priv->commentary_module = gsw_manager_create_module(
            priv->manager, priv->commentary_name);
    if (priv->lexicon_module == NULL && priv->lexicon_name != NULL)
        priv->lexicon_module = gsw_manager_create_module(
            priv->manager, priv->lexicon_name);
}

static void
gsword_window_update_study(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;
    const gchar *key_text;
    GString *text;

    gsword_window_ensure_study_modules(self);
    if (priv->module == NULL)
        return;
    key_text = gsw_module_get_key_text(priv->module);

    if (priv->commentary_module != NULL) {
        gchar *commentary = NULL;

        if (gsw_module_set_key_text(priv->commentary_module, key_text,
                                    &error)) {
            commentary = gsw_module_get_text(priv->commentary_module, &error);
        }
        gsword_window_set_view_text(priv->commentary_view,
                                    commentary != NULL
                                    ? commentary : "No commentary for this verse.");
        g_free(commentary);
        g_clear_error(&error);
    } else {
        gsword_window_set_view_text(priv->commentary_view,
                                    "No commentary module is installed.");
    }

    text = g_string_new(NULL);
    {
        GPtrArray *keys = gsw_module_get_entry_attribute_keys(
            priv->module, "Word", "Lemma", &error);

        if (keys != NULL) {
            guint i;

            for (i = 0; i < keys->len; i++) {
                const gchar *key = g_ptr_array_index(keys, i);
                gchar *value = gsw_module_get_entry_attribute(
                    priv->module, "Word", "Lemma", key, &error);

                if (value != NULL) {
                    g_string_append_printf(text, "%s: %s\n", key, value);
                    g_free(value);
                }
            }
            g_ptr_array_unref(keys);
        }
        g_clear_error(&error);
    }
    if (text->len == 0)
        g_string_append(text, "No Strong's data for this verse.");
    gsword_window_set_view_text(priv->strongs_view, text->str);
    g_string_free(text, TRUE);

    text = g_string_new(NULL);
    {
        GPtrArray *keys = gsw_module_get_entry_attribute_keys(
            priv->module, "Crossref", "body", &error);

        if (keys != NULL) {
            guint i;

            for (i = 0; i < keys->len; i++) {
                const gchar *key = g_ptr_array_index(keys, i);
                gchar *value = gsw_module_get_entry_attribute(
                    priv->module, "Crossref", "body", key, &error);

                if (value != NULL) {
                    g_string_append_printf(text, "%s\n", value);
                    g_free(value);
                }
            }
            g_ptr_array_unref(keys);
        }
        g_clear_error(&error);
    }
    if (text->len == 0)
        g_string_append(text, "No cross-references for this verse.");
    gsword_window_set_view_text(priv->crossref_view, text->str);
    g_string_free(text, TRUE);
}

static void
gsword_window_lexicon_lookup(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;
    const gchar *key;
    gchar *text;

    gsword_window_ensure_study_modules(self);
    if (priv->lexicon_module == NULL) {
        gsword_window_set_view_text(priv->lexicon_view,
                                    "No lexicon module is installed.");
        return;
    }
    key = gtk_editable_get_text(GTK_EDITABLE(priv->lexicon_entry));
    if (!gsw_module_set_key_text(priv->lexicon_module, key, &error)) {
        gsword_window_show_error(self, "Cannot look up lexicon entry", error);
        return;
    }
    text = gsw_module_get_text(priv->lexicon_module, &error);
    if (text == NULL) {
        gsword_window_show_error(self, "Cannot read lexicon entry", error);
        return;
    }
    gsword_window_set_view_text(priv->lexicon_view, text);
    g_free(text);
}

static void
gsword_window_clear_list(GtkListBox *list)
{
    while (gtk_widget_get_first_child(GTK_WIDGET(list)) != NULL)
        gtk_list_box_remove(list, gtk_widget_get_first_child(GTK_WIDGET(list)));
}

static void
gsword_window_refresh_notes(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *notes;
    GPtrArray *tags;
    GPtrArray *lists;
    guint i;
    const gchar *query = priv->note_search != NULL
                       ? gtk_editable_get_text(GTK_EDITABLE(priv->note_search))
                       : "";

    if (priv->notes == NULL)
        return;
    notes = gsword_notes_store_list_notes(priv->notes, query);
    tags = gsword_notes_store_list_tags(priv->notes);
    lists = gsword_notes_store_list_lists(priv->notes);

    gsword_window_clear_list(priv->notes_list);
    gsword_window_clear_list(priv->tags_list);
    gsword_window_clear_list(priv->lists_list);

    for (i = 0; i < notes->len; i++) {
        GswordNote *note = g_ptr_array_index(notes, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label;
        gchar *text = g_strdup_printf("%s\n%s",
            note->display_key, note->text != NULL ? note->text : "");

        label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 32);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "note-module",
                               g_strdup(note->module), g_free);
        g_object_set_data_full(G_OBJECT(row), "note-key",
                               g_strdup(note->key), g_free);
        g_object_set_data_full(G_OBJECT(row), "note-display",
                               g_strdup(note->display_key), g_free);
        gtk_list_box_append(priv->notes_list, row);
        g_free(text);
    }
    for (i = 0; i < tags->len; i++) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(g_ptr_array_index(tags, i));

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "tag-name",
                               g_strdup(g_ptr_array_index(tags, i)), g_free);
        gtk_list_box_append(priv->tags_list, row);
    }
    for (i = 0; i < lists->len; i++) {
        GswordVerseList *list = g_ptr_array_index(lists, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(list->name);
        gchar *id = g_strdup_printf("%" G_GINT64_FORMAT, list->id);

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "list-id", id, g_free);
        gtk_list_box_append(priv->lists_list, row);
    }

    g_ptr_array_unref(notes);
    g_ptr_array_unref(tags);
    g_ptr_array_unref(lists);
}

static void
gsword_window_load_note(GswordWindow *self, const gchar *module,
                        const gchar *key, const gchar *display_key)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswordNote *note = NULL;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(priv->note_text);

    g_free(priv->note_module);
    g_free(priv->note_storage_key);
    g_free(priv->note_display_key);
    priv->note_module = g_strdup(module);
    priv->note_storage_key = g_strdup(key);
    priv->note_display_key = g_strdup(display_key);
    priv->note_id = 0;

    if (priv->notes != NULL)
        note = gsword_notes_store_get_note(priv->notes, module, key);
    if (note != NULL) {
        priv->note_id = note->id;
        gtk_text_buffer_set_text(buffer, note->text, -1);
        gtk_editable_set_text(GTK_EDITABLE(priv->note_tags_entry), note->tags);
    } else {
        gtk_text_buffer_set_text(buffer, "", -1);
        gtk_editable_set_text(GTK_EDITABLE(priv->note_tags_entry), "");
    }
    gtk_editable_set_text(GTK_EDITABLE(priv->note_key_entry),
                          display_key != NULL ? display_key : "");
    gsword_note_free(note);
}

static void
gsword_window_open_note_for_current(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gchar *storage_key;
    gchar *display_key;

    if (priv->module == NULL)
        return;
    storage_key = gsword_window_current_storage_key(self);
    display_key = gsword_window_current_display_key(self);
    if (storage_key == NULL || display_key == NULL) {
        g_free(storage_key);
        g_free(display_key);
        return;
    }
    gsword_window_load_note(self, gsw_module_get_name(priv->module),
                            storage_key, display_key);
    g_free(storage_key);
    g_free(display_key);
    gtk_stack_set_visible_child_name(priv->content_stack, "notes");
    gtk_widget_grab_focus(GTK_WIDGET(priv->note_text));
}

static void
on_note_search_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    gsword_window_refresh_notes(GSWORD_WINDOW(user_data));
}

static void
on_note_row_selected(GtkListBox *list, GtkListBoxRow *row,
                     gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    const gchar *module;
    const gchar *key;
    const gchar *display;

    (void)list;
    if (row == NULL)
        return;
    module = g_object_get_data(G_OBJECT(row), "note-module");
    key = g_object_get_data(G_OBJECT(row), "note-key");
    display = g_object_get_data(G_OBJECT(row), "note-display");
    if (module != NULL && key != NULL)
        gsword_window_load_note(self, module, key, display);
}

static void
on_note_save(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(priv->note_text);
    GtkTextIter start;
    GtkTextIter end;
    gchar *text;
    const gchar *tags;
    GswordNote *saved;

    (void)button;
    if (priv->notes == NULL || priv->note_storage_key == NULL)
        return;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    tags = gtk_editable_get_text(GTK_EDITABLE(priv->note_tags_entry));
    if (!gsword_notes_store_save_note(
            priv->notes, priv->note_module, priv->note_storage_key,
            priv->note_display_key, text, tags, &error)) {
        gsword_window_show_error(self, "Cannot save note", error);
        g_free(text);
        return;
    }
    g_free(text);
    saved = gsword_notes_store_get_note(
        priv->notes, priv->note_module, priv->note_storage_key);
    if (saved != NULL) {
        priv->note_id = saved->id;
        gsword_note_free(saved);
    }
    gsword_window_refresh_notes(self);
    gsword_window_set_status(self, "Note saved");
}

static void
on_note_delete(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;

    (void)button;
    if (priv->notes == NULL || priv->note_id == 0)
        return;
    if (!gsword_notes_store_delete_note(priv->notes, priv->note_id, &error)) {
        gsword_window_show_error(self, "Cannot delete note", error);
        return;
    }
    priv->note_id = 0;
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(priv->note_text), "", -1);
    gtk_editable_set_text(GTK_EDITABLE(priv->note_tags_entry), "");
    gsword_window_refresh_notes(self);
    gsword_window_set_status(self, "Note deleted");
}

static void
on_export_save(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result,
                                              &error);
    const gchar *content = g_object_get_data(source, "export-content");

    (void)user_data;
    if (file == NULL) {
        g_clear_error(&error);
        return;
    }
    if (content != NULL)
        g_file_replace_contents(file, content, strlen(content), NULL, FALSE,
                                G_FILE_CREATE_REPLACE_DESTINATION, NULL, NULL,
                                &error);
    g_clear_error(&error);
    g_object_unref(file);
}

static void
on_note_export(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(priv->note_text);
    GtkTextIter start;
    GtkTextIter end;
    gchar *text;
    gchar *content;
    GtkFileDialog *dialog;

    (void)button;
    if (priv->note_display_key == NULL || priv->notes == NULL)
        return;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    content = g_strdup_printf("# %s\n\n%s\n\nTags: %s\n",
                              priv->note_display_key, text,
                              gtk_editable_get_text(
                                  GTK_EDITABLE(priv->note_tags_entry)));
    g_free(text);

    dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Export note");
    gtk_file_dialog_set_initial_name(dialog, "gsword-note.md");
    g_object_set_data_full(G_OBJECT(dialog), "export-content", content, g_free);
    gtk_file_dialog_save(dialog, GTK_WINDOW(self), NULL, on_export_save, self);
    g_object_unref(dialog);
}

static void
on_tag_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *tag;
    const gchar *current;

    (void)list;
    if (row == NULL)
        return;
    tag = g_object_get_data(G_OBJECT(row), "tag-name");
    if (tag == NULL)
        return;
    current = gtk_editable_get_text(GTK_EDITABLE(priv->note_tags_entry));
    if (current == NULL || *current == '\0') {
        gtk_editable_set_text(GTK_EDITABLE(priv->note_tags_entry), tag);
    } else if (strstr(current, tag) == NULL) {
        gchar *combined = g_strdup_printf("%s, %s", current, tag);

        gtk_editable_set_text(GTK_EDITABLE(priv->note_tags_entry), combined);
        g_free(combined);
    }
}

static void
on_list_item_selected(GtkListBox *list, GtkListBoxRow *row,
                      gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *module;
    const gchar *key;

    (void)list;
    if (row == NULL)
        return;
    module = g_object_get_data(G_OBJECT(row), "item-module");
    key = g_object_get_data(G_OBJECT(row), "item-display");
    if (module == NULL || key == NULL)
        return;
    if (priv->module == NULL ||
        g_strcmp0(module, gsw_module_get_name(priv->module)) != 0) {
        gsword_window_load_module(self, module);
    }
    if (gsword_window_set_key_text(self, key))
        gtk_stack_set_visible_child_name(priv->content_stack, "reader");
}

static void
on_list_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *id;

    (void)list;
    if (row == NULL)
        return;
    id = g_object_get_data(G_OBJECT(row), "list-id");
    priv->selected_list_id = id != NULL ? g_ascii_strtoll(id, NULL, 10) : 0;
    gsword_window_clear_list(priv->list_items);
    if (priv->notes != NULL && priv->selected_list_id != 0) {
        GPtrArray *items = gsword_notes_store_list_items(
            priv->notes, priv->selected_list_id);
        guint i;

        for (i = 0; i < items->len; i++) {
            GswordVerseListItem *item = g_ptr_array_index(items, i);
            GtkWidget *item_row = gtk_list_box_row_new();
            GtkWidget *label = gtk_label_new(item->display_key);

            gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(item_row), label);
            g_object_set_data_full(G_OBJECT(item_row), "item-module",
                                   g_strdup(item->module), g_free);
            g_object_set_data_full(G_OBJECT(item_row), "item-display",
                                   g_strdup(item->display_key), g_free);
            gtk_list_box_append(priv->list_items, item_row);
        }
        g_ptr_array_unref(items);
    }
}

static void
on_create_list(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;
    const gchar *name = gtk_editable_get_text(
        GTK_EDITABLE(priv->new_list_entry));

    (void)button;
    if (priv->notes == NULL || name == NULL || *name == '\0')
        return;
    if (!gsword_notes_store_create_list(priv->notes, name, &error)) {
        gsword_window_show_error(self, "Cannot create list", error);
        return;
    }
    gtk_editable_set_text(GTK_EDITABLE(priv->new_list_entry), "");
    gsword_window_refresh_notes(self);
    gsword_window_set_status(self, "Verse list created");
}

static void
on_add_to_list(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;

    (void)button;
    if (priv->notes == NULL || priv->selected_list_id == 0 ||
        priv->note_storage_key == NULL) {
        gsword_window_set_status(self, "Select a verse list first");
        return;
    }
    if (!gsword_notes_store_add_to_list(
            priv->notes, priv->selected_list_id, priv->note_module,
            priv->note_storage_key, priv->note_display_key, &error)) {
        gsword_window_show_error(self, "Cannot add verse to list", error);
        return;
    }
    gsword_window_refresh_notes(self);
    gsword_window_set_status(self, "Verse added to list");
}

static GtkWidget *
gsword_window_make_notes_page(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *notes_scroll = gtk_scrolled_window_new();
    GtkWidget *tags_scroll = gtk_scrolled_window_new();
    GtkWidget *lists_scroll = gtk_scrolled_window_new();
    GtkWidget *list_items_scroll = gtk_scrolled_window_new();
    GtkWidget *new_list_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *new_list_button = gtk_button_new_with_label("Add list");
    GtkWidget *notes_label = gtk_label_new("Notes");
    GtkWidget *tags_label = gtk_label_new("Tags");
    GtkWidget *lists_label = gtk_label_new("Verse lists");
    GtkWidget *list_items_label = gtk_label_new("List verses");
    GtkWidget *editor_scroll = gtk_scrolled_window_new();
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *save = gtk_button_new_with_label("Save");
    GtkWidget *delete = gtk_button_new_with_label("Delete");
    GtkWidget *export = gtk_button_new_with_label("Export");
    GtkWidget *add_to_list = gtk_button_new_with_label("Add to list");
    GtkWidget *tags_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    gtk_widget_set_margin_top(left, 8);
    gtk_widget_set_margin_bottom(left, 8);
    gtk_widget_set_margin_start(left, 10);
    gtk_widget_set_margin_end(left, 10);
    gtk_widget_set_margin_top(right, 8);
    gtk_widget_set_margin_bottom(right, 8);
    gtk_widget_set_margin_start(right, 10);
    gtk_widget_set_margin_end(right, 10);
    gtk_widget_set_size_request(left, 260, -1);

    priv->note_search = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->note_search, "Search notes");
    g_signal_connect(priv->note_search, "changed",
                     G_CALLBACK(on_note_search_changed), self);

    priv->notes_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(priv->notes_list, "row-selected",
                     G_CALLBACK(on_note_row_selected), self);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(notes_scroll),
                                  GTK_WIDGET(priv->notes_list));
    gtk_widget_set_vexpand(notes_scroll, TRUE);

    priv->tags_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(priv->tags_list, "row-selected",
                     G_CALLBACK(on_tag_selected), self);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tags_scroll),
                                  GTK_WIDGET(priv->tags_list));
    gtk_widget_set_size_request(tags_scroll, -1, 120);

    priv->lists_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(priv->lists_list, "row-selected",
                     G_CALLBACK(on_list_selected), self);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(lists_scroll),
                                  GTK_WIDGET(priv->lists_list));
    gtk_widget_set_size_request(lists_scroll, -1, 90);

    priv->list_items = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(priv->list_items, "row-selected",
                     G_CALLBACK(on_list_item_selected), self);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(list_items_scroll),
                                  GTK_WIDGET(priv->list_items));
    gtk_widget_set_size_request(list_items_scroll, -1, 100);
    priv->new_list_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->new_list_entry, "New list");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->new_list_entry), TRUE);
    g_signal_connect(new_list_button, "clicked",
                     G_CALLBACK(on_create_list), self);
    gtk_box_append(GTK_BOX(new_list_row), GTK_WIDGET(priv->new_list_entry));
    gtk_box_append(GTK_BOX(new_list_row), new_list_button);

    gtk_label_set_xalign(GTK_LABEL(notes_label), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(tags_label), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(lists_label), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(list_items_label), 0.0f);
    gtk_widget_add_css_class(notes_label, "heading");
    gtk_widget_add_css_class(tags_label, "heading");
    gtk_widget_add_css_class(lists_label, "heading");
    gtk_widget_add_css_class(list_items_label, "heading");
    gtk_box_append(GTK_BOX(left), GTK_WIDGET(priv->note_search));
    gtk_box_append(GTK_BOX(left), notes_label);
    gtk_box_append(GTK_BOX(left), notes_scroll);
    gtk_box_append(GTK_BOX(left), tags_label);
    gtk_box_append(GTK_BOX(left), tags_scroll);
    gtk_box_append(GTK_BOX(left), lists_label);
    gtk_box_append(GTK_BOX(left), lists_scroll);
    gtk_box_append(GTK_BOX(left), list_items_label);
    gtk_box_append(GTK_BOX(left), list_items_scroll);
    gtk_box_append(GTK_BOX(left), new_list_row);

    priv->note_key_entry = GTK_ENTRY(gtk_entry_new());
    gtk_editable_set_editable(GTK_EDITABLE(priv->note_key_entry), FALSE);
    gtk_entry_set_placeholder_text(priv->note_key_entry, "Verse key");
    priv->note_tags_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->note_tags_entry,
                                   "Tags, separated by commas");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->note_tags_entry), TRUE);
    gtk_box_append(GTK_BOX(tags_row), gtk_label_new("Tags:"));
    gtk_box_append(GTK_BOX(tags_row), GTK_WIDGET(priv->note_tags_entry));

    priv->note_text = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_wrap_mode(priv->note_text, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(priv->note_text, 12);
    gtk_text_view_set_right_margin(priv->note_text, 12);
    gtk_text_view_set_top_margin(priv->note_text, 12);
    gtk_text_view_set_bottom_margin(priv->note_text, 12);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(editor_scroll),
                                  GTK_WIDGET(priv->note_text));
    gtk_widget_set_vexpand(editor_scroll, TRUE);

    g_signal_connect(save, "clicked", G_CALLBACK(on_note_save), self);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_note_delete), self);
    g_signal_connect(export, "clicked", G_CALLBACK(on_note_export), self);
    g_signal_connect(add_to_list, "clicked", G_CALLBACK(on_add_to_list),
                     self);
    gtk_box_append(GTK_BOX(buttons), save);
    gtk_box_append(GTK_BOX(buttons), delete);
    gtk_box_append(GTK_BOX(buttons), add_to_list);
    gtk_box_append(GTK_BOX(buttons), export);

    gtk_box_append(GTK_BOX(right), GTK_WIDGET(priv->note_key_entry));
    gtk_box_append(GTK_BOX(right), tags_row);
    gtk_box_append(GTK_BOX(right), editor_scroll);
    gtk_box_append(GTK_BOX(right), buttons);

    gtk_paned_set_start_child(GTK_PANED(paned), left);
    gtk_paned_set_end_child(GTK_PANED(paned), right);
    gtk_paned_set_position(GTK_PANED(paned), 280);
    return paned;
}

static GswModule *
gsword_window_ensure_verse_module(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *infos;
    guint i;

    if (priv->module != NULL &&
        gsw_key_get_kind(gsw_module_get_key(priv->module)) ==
            GSW_KEY_KIND_VERSE)
        return priv->module;

    infos = gsw_manager_get_module_infos(priv->manager);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (gsw_module_info_get_module_type(info) == GSW_MODULE_TYPE_BIBLES) {
            gchar *name = g_strdup(gsw_module_info_get_name(info));

            g_ptr_array_unref(infos);
            gsword_window_load_module(self, name);
            g_free(name);
            if (priv->module != NULL &&
                gsw_key_get_kind(gsw_module_get_key(priv->module)) ==
                    GSW_KEY_KIND_VERSE)
                return priv->module;
            return NULL;
        }
    }
    g_ptr_array_unref(infos);
    gsword_window_set_status(self, "No Bible text module is installed");
    return NULL;
}

static void
gsword_window_open_reference(GswordWindow *self, const gchar *reference)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (reference == NULL || *reference == '\0' ||
        gsword_window_ensure_verse_module(self) == NULL)
        return;
    if (gsword_window_set_key_text(self, reference))
        gtk_stack_set_visible_child_name(priv->content_stack, "reader");
}

static GPtrArray *
gsword_window_generate_plan_references(const gchar *start_ref,
                                       gint days, gint chapters_per_day)
{
    GPtrArray *references = g_ptr_array_new_with_free_func(g_free);
    GswVerseKey *key;
    GswVersificationMgr *mgr;
    GswVersification *versification;
    gint global_book;
    gint chapter;
    gint book_count;
    gint total_chapters = 0;
    gint per_day;
    gint extra;
    gint day;

    key = gsw_verse_key_new(start_ref, NULL);
    if (key == NULL)
        return references;
    global_book = gsw_verse_key_get_global_book_number(key);
    chapter = gsw_verse_key_get_chapter(key);
    if (global_book < 0 || chapter < 1) {
        gsw_key_unref(key);
        return references;
    }

    mgr = gsw_versification_mgr_get_default();
    versification = gsw_versification_mgr_get_system(
        mgr, gsw_verse_key_get_versification(key));
    if (versification == NULL) {
        gsw_key_unref(key);
        return references;
    }
    book_count = gsw_versification_get_book_count(versification);

    {
        gint book_index;
        gint first_chapter = chapter;

        for (book_index = global_book; book_index < book_count; book_index++) {
            GswBook *book = gsw_versification_get_book(versification,
                                                       book_index);
            gint maximum;
            gint first;

            if (book == NULL)
                break;
            maximum = gsw_book_get_chapter_max(book);
            first = book_index == global_book ? first_chapter : 1;
            if (maximum >= first)
                total_chapters += maximum - first + 1;
        }
    }

    if (total_chapters <= 0) {
        gsw_key_unref(key);
        return references;
    }

    /*
     * When the requested days are enough to cover the selected range, spread
     * chapters evenly over the days (e.g. 1189 chapters over 365 days gives a
     * mix of 3- and 4-chapter days). Otherwise use the requested chapters per
     * day and stop when the Bible ends.
     */
    if (total_chapters <= days * chapters_per_day) {
        per_day = total_chapters / days;
        extra = total_chapters % days;
        if (per_day == 0) {
            per_day = 1;
            extra = 0;
            days = MIN(days, total_chapters);
        }
    } else {
        per_day = chapters_per_day;
        extra = 0;
    }

    for (day = 0; day < days && global_book < book_count; day++) {
        GswBook *book = gsw_versification_get_book(versification,
                                                   global_book);
        gint count = per_day + (day < extra ? 1 : 0);
        gint c;

        if (book == NULL)
            break;
        g_ptr_array_add(references,
            g_strdup_printf("%s %d", gsw_book_get_name(book), chapter));

        for (c = 0; c < count; c++) {
            gint maximum = gsw_book_get_chapter_max(book);

            if (chapter < maximum) {
                chapter++;
            } else if (global_book + 1 < book_count) {
                global_book++;
                chapter = 1;
                book = gsw_versification_get_book(versification, global_book);
            } else {
                global_book = book_count;
                break;
            }
        }
    }
    gsw_key_unref(key);
    return references;
}

static GswordPlan *
gsword_window_selected_plan(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint i;

    if (priv->plan_items == NULL)
        return NULL;
    for (i = 0; i < priv->plan_items->len; i++) {
        GswordPlan *plan = g_ptr_array_index(priv->plan_items, i);

        if (plan->id == priv->selected_plan_id)
            return plan;
    }
    return priv->plan_items->len > 0
         ? g_ptr_array_index(priv->plan_items, 0) : NULL;
}

static void
gsword_window_refresh_plan_views(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswordPlan *plan = gsword_window_selected_plan(self);
    GPtrArray *days;

    if (priv->plan_days == NULL)
        return;
    gsword_window_clear_list(priv->plan_days);

    if (plan == NULL) {
        gtk_label_set_text(priv->plan_summary,
                           "Create a reading plan to get started.");
        gtk_label_set_text(priv->plan_today, "");
        gtk_widget_set_sensitive(GTK_WIDGET(priv->plan_open_button), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(priv->plan_complete_button),
                                 FALSE);
        return;
    }

    priv->selected_plan_id = plan->id;
    {
        gdouble percent = plan->total_days > 0
                        ? (gdouble)plan->completed_days * 100.0 /
                          plan->total_days : 0.0;
        gchar *summary = g_strdup_printf(
            "%s  ·  %d/%d days (%.1f%%)  ·  streak %d",
            plan->name, plan->completed_days, plan->total_days, percent,
            plan->streak);

        gtk_label_set_text(priv->plan_summary, summary);
        g_free(summary);
    }
    if (plan->next_day > 0) {
        gchar *today = g_strdup_printf("Day %d: %s", plan->next_day,
                                       plan->next_reference != NULL
                                       ? plan->next_reference : "");
        gtk_label_set_text(priv->plan_today, today);
        g_free(today);
        gtk_widget_set_sensitive(GTK_WIDGET(priv->plan_open_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(priv->plan_complete_button),
                                 TRUE);
    } else {
        gtk_label_set_text(priv->plan_today, "Plan complete");
        gtk_widget_set_sensitive(GTK_WIDGET(priv->plan_open_button), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(priv->plan_complete_button),
                                 FALSE);
    }

    days = gsword_plan_store_get_plan_days(priv->plans, plan->id);
    {
        guint i;

        for (i = 0; i < days->len; i++) {
            GswordPlanDay *day = g_ptr_array_index(days, i);
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            GtkWidget *label;
            GtkWidget *check;
            gchar *text;
            gchar *day_number = g_strdup_printf("%d", day->day);
            gchar *plan_number = g_strdup_printf("%" G_GINT64_FORMAT,
                                                 plan->id);

            text = g_strdup_printf("Day %d   %s", day->day,
                                   day->reference != NULL ? day->reference : "");

            label = gtk_label_new(text);
            gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
            gtk_widget_set_hexpand(label, TRUE);
            check = gtk_check_button_new_with_label("Done");
            gtk_check_button_set_active(GTK_CHECK_BUTTON(check),
                                        day->completed_on != NULL);
            g_object_set_data_full(G_OBJECT(check), "plan-id", plan_number,
                                   g_free);
            g_object_set_data_full(G_OBJECT(check), "plan-day", day_number,
                                   g_free);
            g_signal_connect(check, "toggled",
                             G_CALLBACK(on_plan_day_toggled), self);
            gtk_box_append(GTK_BOX(box), label);
            gtk_box_append(GTK_BOX(box), check);
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
            g_object_set_data_full(G_OBJECT(row), "plan-reference",
                                   g_strdup(day->reference), g_free);
            gtk_list_box_append(priv->plan_days, row);
            g_free(text);
        }
    }
    g_ptr_array_unref(days);
}

static void
gsword_window_refresh_plans(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *plans;
    GtkStringList *names;
    gchar *active = NULL;
    guint selected = 0;
    guint i;

    if (priv->plans == NULL || priv->plan_selector == NULL)
        return;

    plans = gsword_plan_store_list_plans(priv->plans);
    names = gtk_string_list_new(NULL);
    g_clear_pointer(&priv->plan_items, g_ptr_array_unref);
    priv->plan_items = plans;
    if (priv->settings != NULL)
        active = g_settings_get_string(priv->settings, "active-plan-id");

    for (i = 0; i < plans->len; i++) {
        GswordPlan *plan = g_ptr_array_index(plans, i);

        gtk_string_list_append(names, plan->name);
        if (active != NULL && *active != '\0' &&
            plan->id == g_ascii_strtoll(active, NULL, 10))
            selected = i;
    }
    g_clear_object(&priv->plan_names);
    priv->plan_names = names;
    gtk_drop_down_set_model(priv->plan_selector, G_LIST_MODEL(names));
    if (plans->len > 0) {
        if (selected >= plans->len)
            selected = 0;
        priv->selected_plan_id =
            ((GswordPlan *)g_ptr_array_index(plans, selected))->id;
        gtk_drop_down_set_selected(priv->plan_selector, selected);
    } else {
        priv->selected_plan_id = 0;
    }
    g_free(active);
    gsword_window_refresh_plan_views(self);
}

static gboolean
refresh_plan_idle(gpointer data)
{
    gsword_window_refresh_plans(GSWORD_WINDOW(data));
    return G_SOURCE_REMOVE;
}

static void
gsword_window_schedule_plan_refresh(GswordWindow *self)
{
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, refresh_plan_idle,
                    g_object_ref(self), g_object_unref);
}

static void
on_plan_selected(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint selected = gtk_drop_down_get_selected(priv->plan_selector);
    GswordPlan *plan;

    (void)object;
    (void)pspec;
    if (priv->plan_items == NULL || selected == GTK_INVALID_LIST_POSITION ||
        selected >= priv->plan_items->len)
        return;
    plan = g_ptr_array_index(priv->plan_items, selected);
    priv->selected_plan_id = plan->id;
    if (priv->settings != NULL) {
        gchar *id = g_strdup_printf("%" G_GINT64_FORMAT, plan->id);

        g_settings_set_string(priv->settings, "active-plan-id", id);
        g_free(id);
    }
    gsword_window_refresh_plan_views(self);
}

static void
on_plan_day_toggled(GtkCheckButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *plan_id = g_object_get_data(G_OBJECT(button), "plan-id");
    const gchar *day = g_object_get_data(G_OBJECT(button), "plan-day");
    GError *error = NULL;

    if (priv->plans == NULL || plan_id == NULL || day == NULL)
        return;
    if (!gsword_plan_store_complete_day(
            priv->plans, g_ascii_strtoll(plan_id, NULL, 10),
            (gint)g_ascii_strtoll(day, NULL, 10),
            gtk_check_button_get_active(button), &error)) {
        gsword_window_show_error(self, "Cannot update plan", error);
        return;
    }
    gsword_window_set_status(self, "Reading plan updated");
    gsword_window_schedule_plan_refresh(self);
}

static void
on_plan_day_activated(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    const gchar *reference;

    (void)list;
    if (row == NULL)
        return;
    reference = g_object_get_data(G_OBJECT(row), "plan-reference");
    if (reference != NULL)
        gsword_window_open_reference(GSWORD_WINDOW(user_data), reference);
}

static void
on_plan_open_today(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordPlan *plan = gsword_window_selected_plan(self);

    (void)button;
    if (plan != NULL && plan->next_reference != NULL)
        gsword_window_open_reference(self, plan->next_reference);
}

static void
on_plan_complete_today(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswordPlan *plan = gsword_window_selected_plan(self);
    GError *error = NULL;

    (void)button;
    if (priv->plans == NULL || plan == NULL || plan->next_day <= 0)
        return;
    if (!gsword_plan_store_complete_day(priv->plans, plan->id, plan->next_day,
                                        TRUE, &error)) {
        gsword_window_show_error(self, "Cannot update plan", error);
        return;
    }
    gsword_window_set_status(self, "Day marked complete");
    gsword_window_refresh_plans(self);
}

static void
on_plan_create(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *name = gtk_editable_get_text(GTK_EDITABLE(priv->plan_name_entry));
    const gchar *start = gtk_editable_get_text(GTK_EDITABLE(priv->plan_start_entry));
    gint days = gtk_spin_button_get_value_as_int(priv->plan_days_spin);
    gint chapters = gtk_spin_button_get_value_as_int(priv->plan_chapters_spin);
    GPtrArray *references;
    GError *error = NULL;
    gint64 plan_id;
    gchar *id;

    (void)button;
    if (priv->plans == NULL)
        return;
    if (name == NULL || *name == '\0' ||
        start == NULL || *start == '\0') {
        gsword_window_set_status(self, "Enter a plan name and start reference");
        return;
    }
    references = gsword_window_generate_plan_references(start, days, chapters);
    if (references->len == 0) {
        gsword_window_show_error(self, "Cannot create plan",
            g_error_new_literal(G_FILE_ERROR, G_FILE_ERROR_INVAL,
                                "Invalid start reference or empty plan"));
        g_ptr_array_unref(references);
        return;
    }
    plan_id = gsword_plan_store_create_plan(priv->plans, name, start, chapters,
                                            references, &error);
    g_ptr_array_unref(references);
    if (plan_id == 0) {
        gsword_window_show_error(self, "Cannot create plan", error);
        return;
    }
    id = g_strdup_printf("%" G_GINT64_FORMAT, plan_id);
    if (priv->settings != NULL)
        g_settings_set_string(priv->settings, "active-plan-id", id);
    g_free(id);
    gsword_window_refresh_plans(self);
    gsword_window_set_status(self, "Reading plan created");
}

static void
on_plan_delete_response(AdwAlertDialog *dialog, const gchar *response,
                        gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswordPlan *plan = gsword_window_selected_plan(self);
    GError *error = NULL;

    (void)dialog;
    if (g_strcmp0(response, "delete") != 0 || plan == NULL ||
        priv->plans == NULL)
        return;
    if (!gsword_plan_store_delete_plan(priv->plans, plan->id, &error)) {
        gsword_window_show_error(self, "Cannot delete plan", error);
        return;
    }
    if (priv->settings != NULL)
        g_settings_set_string(priv->settings, "active-plan-id", "");
    priv->selected_plan_id = 0;
    gsword_window_refresh_plans(self);
    gsword_window_set_status(self, "Reading plan deleted");
}

static void
on_plan_delete(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordPlan *plan = gsword_window_selected_plan(self);
    AdwDialog *dialog;

    (void)button;
    if (plan == NULL)
        return;
    dialog = adw_alert_dialog_new("Delete reading plan?", NULL);
    adw_alert_dialog_format_body(ADW_ALERT_DIALOG(dialog),
        "Delete “%s” and its reading progress? This cannot be undone.",
        plan->name);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "cancel", "Cancel",
                                   "delete", "Delete",
                                   NULL);
    adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog),
                                             "delete",
                                             ADW_RESPONSE_DESTRUCTIVE);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    g_signal_connect(dialog, "response",
                     G_CALLBACK(on_plan_delete_response), self);
    adw_dialog_present(dialog, GTK_WIDGET(self));
}

static gboolean
module_info_is_devotional(GswModuleInfo *info)
{
    gchar **features;
    const gchar *category;
    gboolean result = FALSE;
    guint i;

    if (gsw_module_info_get_module_type(info) == GSW_MODULE_TYPE_DAILY_DEVOS)
        return TRUE;
    features = gsw_module_info_get_values(info, "Feature");
    for (i = 0; features != NULL && features[i] != NULL; i++) {
        if (g_ascii_strcasecmp(features[i], "DailyDevotion") == 0) {
            result = TRUE;
            break;
        }
    }
    g_strfreev(features);
    if (result)
        return TRUE;
    category = gsw_module_info_get_value(info, "Category");
    return category != NULL &&
           g_ascii_strncasecmp(category, "Daily Devotional", 16) == 0;
}

static void
gsword_window_load_devotion(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint selected = priv->devotion_selector != NULL
                   ? gtk_drop_down_get_selected(priv->devotion_selector)
                   : GTK_INVALID_LIST_POSITION;
    const gchar *name;
    GError *error = NULL;
    GDateTime *now;
    gchar *date_text;
    gchar *text = NULL;
    gchar *title;
    const gchar *formats[] = {
        "%m.%d", "%m%d", "%m-%d", "%m/%d", "%B %-d", "%b %-d", "%B %d",
        "%b %d", NULL
    };
    guint i;

    if (priv->devotion_view == NULL || priv->devotion_names == NULL ||
        selected == GTK_INVALID_LIST_POSITION)
        return;
    name = gtk_string_list_get_string(priv->devotion_names, selected);
    if (name == NULL || *name == '\0')
        return;

    now = g_date_time_new_now_local();
    date_text = g_date_time_format(now, "%B %-d, %Y");
    for (i = 0; formats[i] != NULL; i++) {
        gchar *key = g_date_time_format(now, formats[i]);
        GswModule *module = gsw_manager_create_module(priv->manager, name);

        if (module != NULL &&
            gsw_module_set_key_text(module, key, NULL) &&
            gsw_module_has_entry(module)) {
            text = gsw_module_get_text(module, &error);
            if (text != NULL && *text == '\0') {
                g_free(text);
                text = NULL;
            }
        }
        g_clear_object(&module);
        g_free(key);
        if (text != NULL)
            break;
    }
    g_date_time_unref(now);

    if (text == NULL) {
        gsword_window_set_view_text(
            priv->devotion_view,
            "No devotional entry is available for today in this module.");
    } else {
        gsword_window_set_view_text(priv->devotion_view, text);
        g_free(text);
    }
    title = g_strdup_printf("Daily Devotion · %s · %s", name,
                            date_text != NULL ? date_text : "");
    gtk_label_set_text(priv->devotion_title, title);
    g_free(title);
    g_free(date_text);
    g_clear_error(&error);
    if (priv->settings != NULL)
        g_settings_set_string(priv->settings, "devotion-module", name);
}

static void
gsword_window_refresh_devotion(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *infos;
    GtkStringList *names;
    gchar *wanted = NULL;
    guint selected = 0;
    gboolean found = FALSE;
    guint i;
    guint count = 0;

    if (priv->devotion_selector == NULL)
        return;
    infos = gsw_manager_get_module_infos(priv->manager);
    names = gtk_string_list_new(NULL);
    if (priv->settings != NULL)
        wanted = g_settings_get_string(priv->settings, "devotion-module");
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (!module_info_is_devotional(info))
            continue;
        gtk_string_list_append(names, gsw_module_info_get_name(info));
        count++;
        if (wanted != NULL &&
            g_strcmp0(wanted, gsw_module_info_get_name(info)) == 0 &&
            !found) {
            selected = count - 1;
            found = TRUE;
        }
    }
    g_ptr_array_unref(infos);
    g_clear_object(&priv->devotion_names);
    priv->devotion_names = names;
    gtk_drop_down_set_model(priv->devotion_selector, G_LIST_MODEL(names));
    if (count > 0) {
        gtk_drop_down_set_selected(priv->devotion_selector, selected);
        gsword_window_load_devotion(self);
    } else {
        gtk_label_set_text(priv->devotion_title, "Daily Devotion");
        gsword_window_set_view_text(
            priv->devotion_view,
            "No daily devotional module is installed.\n\n"
            "Install a module such as Daily Light or Morning and Evening "
            "from the Library page.");
    }
    g_free(wanted);
}

static void
on_devotion_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    gsword_window_load_devotion(GSWORD_WINDOW(user_data));
}

static void
on_devotion_refresh(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_refresh_devotion(GSWORD_WINDOW(user_data));
}

static gchar *
memory_masked_text(const gchar *text, gboolean first_letters)
{
    GString *result = g_string_new(NULL);
    const gchar *p = text != NULL ? text : "";
    gboolean in_word = FALSE;

    while (*p != '\0') {
        gunichar c = g_utf8_get_char(p);

        p = g_utf8_next_char(p);
        if (g_unichar_isspace(c)) {
            g_string_append_unichar(result, c);
            in_word = FALSE;
        } else if (g_unichar_isalnum(c)) {
            if (first_letters && !in_word)
                g_string_append_unichar(result, c);
            else
                g_string_append_c(result, '_');
            in_word = TRUE;
        } else {
            g_string_append_unichar(result, c);
            in_word = FALSE;
        }
    }
    return g_string_free(result, FALSE);
}

static gchar *
memory_normalized_text(const gchar *text)
{
    GString *result = g_string_new(NULL);
    const gchar *p = text != NULL ? text : "";

    while (*p != '\0') {
        gunichar c = g_utf8_get_char(p);

        p = g_utf8_next_char(p);
        if (g_unichar_isalnum(c))
            g_string_append_unichar(result, g_unichar_tolower(c));
    }
    return g_string_free(result, FALSE);
}

static void
gsword_window_update_memory_card(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint mode;
    gchar *masked;

    if (priv->memory_card == NULL)
        return;
    if (priv->memory_text == NULL || *priv->memory_text == '\0') {
        gsword_window_set_view_text(priv->memory_card, "");
        return;
    }
    mode = priv->memory_mode != NULL
         ? gtk_drop_down_get_selected(priv->memory_mode) : 0;
    if (mode == 1)
        masked = memory_masked_text(priv->memory_text, TRUE);
    else if (mode == 2)
        masked = memory_masked_text(priv->memory_text, FALSE);
    else
        masked = g_strdup(priv->memory_text);
    gsword_window_set_view_text(priv->memory_card, masked);
    g_free(masked);
}

static void
gsword_window_clear_memory_selection(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    priv->memory_id = 0;
    g_clear_pointer(&priv->memory_module, g_free);
    g_clear_pointer(&priv->memory_key, g_free);
    g_clear_pointer(&priv->memory_display, g_free);
    g_clear_pointer(&priv->memory_text, g_free);
    gtk_label_set_text(priv->memory_title, "Select a verse card");
    gtk_label_set_text(priv->memory_due, "");
    gtk_editable_set_text(GTK_EDITABLE(priv->memory_input), "");
    gtk_label_set_text(priv->memory_result, "");
    gsword_window_set_view_text(priv->memory_card, "");
}

static void
on_memory_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *id_text;
    gint64 id;
    guint i;

    (void)list;
    if (row == NULL || priv->memory_items == NULL) {
        gsword_window_clear_memory_selection(self);
        return;
    }
    id_text = g_object_get_data(G_OBJECT(row), "memory-id");
    if (id_text == NULL)
        return;
    id = g_ascii_strtoll(id_text, NULL, 10);
    for (i = 0; i < priv->memory_items->len; i++) {
        GswordMemoryVerse *verse = g_ptr_array_index(priv->memory_items, i);
        gchar *due;

        if (verse->id != id)
            continue;
        priv->memory_id = verse->id;
        g_free(priv->memory_module);
        g_free(priv->memory_key);
        g_free(priv->memory_display);
        g_free(priv->memory_text);
        priv->memory_module = g_strdup(verse->module);
        priv->memory_key = g_strdup(verse->key);
        priv->memory_display = g_strdup(verse->display_key);
        priv->memory_text = g_strdup(verse->text);
        due = g_strdup_printf("Due %s  ·  box %d", verse->due, verse->box);
        gtk_label_set_text(priv->memory_title, verse->display_key);
        gtk_label_set_text(priv->memory_due, due);
        g_free(due);
        gtk_editable_set_text(GTK_EDITABLE(priv->memory_input), "");
        gtk_label_set_text(priv->memory_result, "");
        gsword_window_update_memory_card(self);
        return;
    }
}

static void
gsword_window_refresh_memory(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint i;
    GtkListBoxRow *wanted = NULL;
    GtkListBoxRow *first = NULL;

    if (priv->memory_list == NULL || priv->plans == NULL)
        return;
    gsword_window_clear_list(priv->memory_list);
    g_clear_pointer(&priv->memory_items, g_ptr_array_unref);
    priv->memory_items = gsword_plan_store_list_memory_verses(priv->plans);

    for (i = 0; i < priv->memory_items->len; i++) {
        GswordMemoryVerse *verse = g_ptr_array_index(priv->memory_items, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label;
        gchar *text = g_strdup_printf("%s\nDue %s  ·  box %d",
                                      verse->display_key, verse->due,
                                      verse->box);
        gchar *id = g_strdup_printf("%" G_GINT64_FORMAT, verse->id);

        label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "memory-id", id, g_free);
        gtk_list_box_append(priv->memory_list, row);
        if (first == NULL)
            first = GTK_LIST_BOX_ROW(row);
        if (verse->id == priv->memory_id)
            wanted = GTK_LIST_BOX_ROW(row);
        g_free(text);
    }

    if (wanted == NULL)
        wanted = first;
    if (wanted != NULL)
        gtk_list_box_select_row(priv->memory_list, wanted);
    else
        gsword_window_clear_memory_selection(self);
}

static void
on_memory_mode_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    gsword_window_update_memory_card(GSWORD_WINDOW(user_data));
}

static void
on_memory_add_current(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswModule *module;
    gchar *storage_key;
    gchar *display_key;
    gchar *text;
    GError *error = NULL;
    GPtrArray *items;
    guint i;

    (void)button;
    if (priv->plans == NULL)
        return;
    module = gsword_window_ensure_verse_module(self);
    if (module == NULL)
        return;
    storage_key = gsword_window_current_storage_key(self);
    display_key = gsword_window_current_display_key(self);
    text = gsw_module_get_text(module, &error);
    if (storage_key == NULL || display_key == NULL || text == NULL) {
        gsword_window_show_error(self, "Cannot add memory verse", error);
        g_free(storage_key);
        g_free(display_key);
        g_free(text);
        return;
    }
    if (!gsword_plan_store_add_memory_verse(
            priv->plans, gsw_module_get_name(module), storage_key,
            display_key, text, &error)) {
        gsword_window_show_error(self, "Cannot save memory verse", error);
        g_free(storage_key);
        g_free(display_key);
        g_free(text);
        return;
    }
    items = gsword_plan_store_list_memory_verses(priv->plans);
    for (i = 0; i < items->len; i++) {
        GswordMemoryVerse *verse = g_ptr_array_index(items, i);

        if (g_strcmp0(verse->module, gsw_module_get_name(module)) == 0 &&
            g_strcmp0(verse->key, storage_key) == 0) {
            priv->memory_id = verse->id;
            break;
        }
    }
    g_ptr_array_unref(items);
    g_free(storage_key);
    g_free(display_key);
    g_free(text);
    gsword_window_refresh_memory(self);
    gsword_window_set_status(self, "Verse added to memory review");
}

static void
on_memory_review(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gboolean remembered = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(button), "remembered"));
    GError *error = NULL;

    if (priv->plans == NULL || priv->memory_id == 0)
        return;
    if (!gsword_plan_store_review_memory_verse(
            priv->plans, priv->memory_id, remembered, &error)) {
        gsword_window_show_error(self, "Cannot schedule review", error);
        return;
    }
    gsword_window_refresh_memory(self);
    gsword_window_set_status(self, remembered
                             ? "Review scheduled: nice work"
                             : "Card will be shown again today");
}

static void
on_memory_check(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *answer;
    gchar *normalized_answer;
    gchar *normalized_text;

    (void)button;
    if (priv->memory_text == NULL)
        return;
    answer = gtk_editable_get_text(GTK_EDITABLE(priv->memory_input));
    normalized_answer = memory_normalized_text(answer);
    normalized_text = memory_normalized_text(priv->memory_text);
    if (*normalized_answer != '\0' &&
        g_strcmp0(normalized_answer, normalized_text) == 0) {
        gtk_label_set_text(priv->memory_result, "Correct");
        gsword_window_set_status(self, "Memory verse recited correctly");
    } else {
        gtk_label_set_text(priv->memory_result, "Keep practicing");
    }
    g_free(normalized_answer);
    g_free(normalized_text);
}

static void
on_memory_delete(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;

    (void)button;
    if (priv->plans == NULL || priv->memory_id == 0)
        return;
    if (!gsword_plan_store_delete_memory_verse(priv->plans, priv->memory_id,
                                               &error)) {
        gsword_window_show_error(self, "Cannot delete memory verse", error);
        return;
    }
    priv->memory_id = 0;
    gsword_window_refresh_memory(self);
    gsword_window_set_status(self, "Memory verse removed");
}

static gchar *
gsword_window_current_chapter_reference(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswKey *key;
    gchar *reference;
    gchar *dot;

    if (priv->module == NULL)
        return NULL;
    key = gsw_module_get_key(priv->module);
    if (gsw_key_get_kind(key) != GSW_KEY_KIND_VERSE)
        return g_strdup(gsw_module_get_key_text(priv->module));
    reference = gsw_verse_key_get_osis_ref((GswVerseKey *)key);
    if (reference == NULL)
        return NULL;
    dot = strrchr(reference, '.');
    if (dot != NULL)
        *dot = '\0';
    return reference;
}

static gchar *
gsword_window_current_chapter_display(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->module == NULL)
        return g_strdup("");
    if (gsw_key_get_kind(gsw_module_get_key(priv->module)) ==
        GSW_KEY_KIND_VERSE) {
        GswVerseKey *key = (GswVerseKey *)gsw_module_get_key(priv->module);

        return g_strdup_printf("%s %d", gsw_verse_key_get_book_name(key),
                               gsw_verse_key_get_chapter(key));
    }
    return g_strdup(gsw_module_get_key_text(priv->module));
}

static void
gsword_window_teaching_load(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswModule *module;
    gchar *reference;
    gchar *display;
    gchar *text;
    gchar *notes;
    GError *error = NULL;

    if (priv->teaching_view == NULL)
        return;
    module = gsword_window_ensure_verse_module(self);
    if (module == NULL)
        return;
    reference = gsword_window_current_chapter_reference(self);
    display = gsword_window_current_chapter_display(self);
    if (reference == NULL) {
        g_free(display);
        return;
    }

    text = gsword_window_chapter_text_for_module(module);
    if (text == NULL)
        text = gsw_module_get_text(module, &error);
    if (text == NULL) {
        gsword_window_show_error(self, "Cannot read teaching passage", error);
        g_free(reference);
        g_free(display);
        return;
    }
    gsword_window_set_view_text(priv->teaching_view, text);
    g_free(text);
    gtk_editable_set_text(GTK_EDITABLE(priv->teaching_ref_entry),
                          display != NULL ? display : reference);
    g_clear_pointer(&priv->teaching_module, g_free);
    g_clear_pointer(&priv->teaching_reference, g_free);
    priv->teaching_module = g_strdup(gsw_module_get_name(module));
    priv->teaching_reference = g_strdup(reference);
    notes = priv->plans != NULL
          ? gsword_plan_store_get_teaching_note(
                priv->plans, priv->teaching_module, reference)
          : NULL;
    gtk_text_buffer_set_text(
        gtk_text_view_get_buffer(priv->teaching_notes),
        notes != NULL ? notes : "", -1);
    g_free(notes);
    g_free(reference);
    g_free(display);
    gtk_stack_set_visible_child_name(priv->content_stack, "teaching");
}

static void
on_teaching_load(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_teaching_load(GSWORD_WINDOW(user_data));
}

static void
on_teaching_save(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;
    gchar *notes;
    GError *error = NULL;

    (void)button;
    if (priv->plans == NULL || priv->teaching_module == NULL ||
        priv->teaching_reference == NULL)
        return;
    buffer = gtk_text_view_get_buffer(priv->teaching_notes);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    notes = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (!gsword_plan_store_save_teaching_note(
            priv->plans, priv->teaching_module, priv->teaching_reference,
            notes, &error)) {
        gsword_window_show_error(self, "Cannot save teaching notes", error);
        g_free(notes);
        return;
    }
    g_free(notes);
    gsword_window_set_status(self, "Teaching notes saved");
}

static void
on_teaching_fullscreen(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    (void)button;
    priv->teaching_fullscreen = !priv->teaching_fullscreen;
    if (priv->teaching_fullscreen)
        gtk_window_fullscreen(GTK_WINDOW(self));
    else
        gtk_window_unfullscreen(GTK_WINDOW(self));
    gtk_button_set_label(priv->teaching_fullscreen_button,
                         priv->teaching_fullscreen
                         ? "Exit fullscreen" : "Fullscreen");
}

static void
on_teaching_close(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    (void)button;
    if (priv->teaching_fullscreen) {
        priv->teaching_fullscreen = FALSE;
        gtk_window_unfullscreen(GTK_WINDOW(self));
        gtk_button_set_label(priv->teaching_fullscreen_button, "Fullscreen");
    }
    gtk_stack_set_visible_child_name(priv->content_stack, "reader");
}

static void
on_teaching_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    (void)button;
    gsword_window_teaching_load(self);
    gtk_stack_set_visible_child_name(priv->content_stack, "teaching");
}

static GtkWidget *
gsword_window_make_plan_page(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *notebook = gtk_notebook_new();
    GtkWidget *today = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *today_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *plans = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *plan_form = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *plan_form_row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *plan_form_row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *days_scroll = gtk_scrolled_window_new();
    GtkWidget *devotion = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *devotion_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *memory = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *memory_left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *memory_right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *memory_scroll = gtk_scrolled_window_new();
    GtkWidget *memory_left_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *memory_card_scroll = gtk_scrolled_window_new();
    GtkWidget *memory_mode_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *memory_input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *memory_review_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *open = gtk_button_new_with_label("Open today's reading");
    GtkWidget *complete = gtk_button_new_with_label("Mark today complete");
    GtkWidget *create = gtk_button_new_with_label("Create plan");
    GtkWidget *delete_plan = gtk_button_new_with_label("Delete plan");
    GtkWidget *add_current = gtk_button_new_with_label("Add current verse");
    GtkWidget *delete_verse = gtk_button_new_with_label("Delete");
    GtkWidget *check = gtk_button_new_with_label("Check answer");
    GtkWidget *again = gtk_button_new_with_label("Again");
    GtkWidget *good = gtk_button_new_with_label("Good");
    GtkWidget *refresh_devotion = gtk_button_new_with_label("Refresh");
    GtkStringList *modes = gtk_string_list_new(
        (const gchar * const[]) { "Full text", "First letters", "Hide words",
                                  NULL });

    gtk_widget_set_margin_top(today, 10);
    gtk_widget_set_margin_bottom(today, 10);
    gtk_widget_set_margin_start(today, 12);
    gtk_widget_set_margin_end(today, 12);
    gtk_widget_set_margin_top(plans, 10);
    gtk_widget_set_margin_bottom(plans, 10);
    gtk_widget_set_margin_start(plans, 12);
    gtk_widget_set_margin_end(plans, 12);
    gtk_widget_set_margin_top(devotion, 10);
    gtk_widget_set_margin_bottom(devotion, 10);
    gtk_widget_set_margin_start(devotion, 12);
    gtk_widget_set_margin_end(devotion, 12);
    gtk_widget_set_margin_top(memory_left, 10);
    gtk_widget_set_margin_bottom(memory_left, 10);
    gtk_widget_set_margin_start(memory_left, 12);
    gtk_widget_set_margin_end(memory_left, 12);
    gtk_widget_set_margin_top(memory_right, 10);
    gtk_widget_set_margin_bottom(memory_right, 10);
    gtk_widget_set_margin_start(memory_right, 12);
    gtk_widget_set_margin_end(memory_right, 12);

    priv->plan_selector = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    gtk_widget_set_hexpand(GTK_WIDGET(priv->plan_selector), TRUE);
    g_signal_connect(priv->plan_selector, "notify::selected",
                     G_CALLBACK(on_plan_selected), self);
    priv->plan_summary = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->plan_summary, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->plan_summary), "heading");
    priv->plan_today = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->plan_today, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->plan_today), "title-2");
    priv->plan_open_button = GTK_BUTTON(open);
    priv->plan_complete_button = GTK_BUTTON(complete);
    g_signal_connect(open, "clicked", G_CALLBACK(on_plan_open_today), self);
    g_signal_connect(complete, "clicked", G_CALLBACK(on_plan_complete_today),
                     self);
    gtk_box_append(GTK_BOX(today_controls), GTK_WIDGET(priv->plan_selector));
    gtk_box_append(GTK_BOX(today_controls), open);
    gtk_box_append(GTK_BOX(today_controls), complete);
    gtk_box_append(GTK_BOX(today), today_controls);
    gtk_box_append(GTK_BOX(today), GTK_WIDGET(priv->plan_summary));
    gtk_box_append(GTK_BOX(today), GTK_WIDGET(priv->plan_today));
    {
        GtkWidget *label = gtk_label_new("Schedule");
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_add_css_class(label, "heading");
        gtk_box_append(GTK_BOX(today), label);
    }
    priv->plan_days = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_activate_on_single_click(priv->plan_days, FALSE);
    g_signal_connect(priv->plan_days, "row-activated",
                     G_CALLBACK(on_plan_day_activated), self);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(days_scroll),
                                  GTK_WIDGET(priv->plan_days));
    gtk_widget_set_vexpand(days_scroll, TRUE);
    gtk_box_append(GTK_BOX(today), days_scroll);

    priv->plan_name_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->plan_name_entry, "Plan name");
    priv->plan_start_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->plan_start_entry, "Start, e.g. Genesis 1");
    gtk_editable_set_width_chars(GTK_EDITABLE(priv->plan_start_entry), 16);
    priv->plan_days_spin = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(1, 3650, 1));
    gtk_spin_button_set_value(priv->plan_days_spin, 365);
    priv->plan_chapters_spin = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(1, 20, 1));
    gtk_spin_button_set_value(priv->plan_chapters_spin, 4);
    g_signal_connect(create, "clicked", G_CALLBACK(on_plan_create), self);
    g_signal_connect(delete_plan, "clicked", G_CALLBACK(on_plan_delete), self);
    gtk_widget_add_css_class(delete_plan, "destructive-action");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->plan_name_entry), TRUE);
    gtk_box_append(GTK_BOX(plan_form_row1), GTK_WIDGET(priv->plan_name_entry));
    gtk_box_append(GTK_BOX(plan_form_row1), GTK_WIDGET(priv->plan_start_entry));
    gtk_box_append(GTK_BOX(plan_form_row1), gtk_label_new("Days"));
    gtk_box_append(GTK_BOX(plan_form_row1), GTK_WIDGET(priv->plan_days_spin));
    gtk_box_append(GTK_BOX(plan_form_row1), gtk_label_new("Chapters/day"));
    gtk_box_append(GTK_BOX(plan_form_row1), GTK_WIDGET(priv->plan_chapters_spin));
    gtk_box_append(GTK_BOX(plan_form_row2), create);
    gtk_box_append(GTK_BOX(plan_form_row2), delete_plan);
    gtk_box_append(GTK_BOX(plan_form), plan_form_row1);
    gtk_box_append(GTK_BOX(plan_form), plan_form_row2);
    {
        GtkWidget *hint = gtk_label_new(
            "The plan advances through chapters from the start reference. "
            "Reading days can be opened by double-clicking a schedule row.");
        gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
        gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
        gtk_widget_add_css_class(hint, "dim-label");
        gtk_box_append(GTK_BOX(plans), plan_form);
        gtk_box_append(GTK_BOX(plans), hint);
    }

    priv->devotion_selector = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    gtk_widget_set_hexpand(GTK_WIDGET(priv->devotion_selector), TRUE);
    g_signal_connect(priv->devotion_selector, "notify::selected",
                     G_CALLBACK(on_devotion_changed), self);
    g_signal_connect(refresh_devotion, "clicked",
                     G_CALLBACK(on_devotion_refresh), self);
    priv->devotion_title = GTK_LABEL(gtk_label_new("Daily Devotion"));
    gtk_label_set_xalign(priv->devotion_title, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->devotion_title), "heading");
    priv->devotion_view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(priv->devotion_view, FALSE);
    gtk_text_view_set_cursor_visible(priv->devotion_view, FALSE);
    gtk_text_view_set_wrap_mode(priv->devotion_view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(priv->devotion_view, 12);
    gtk_text_view_set_right_margin(priv->devotion_view, 12);
    gtk_text_view_set_top_margin(priv->devotion_view, 12);
    gtk_text_view_set_bottom_margin(priv->devotion_view, 12);
    gtk_box_append(GTK_BOX(devotion_controls),
                   GTK_WIDGET(priv->devotion_selector));
    gtk_box_append(GTK_BOX(devotion_controls), refresh_devotion);
    {
        GtkWidget *scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
                                      GTK_WIDGET(priv->devotion_view));
        gtk_widget_set_vexpand(scroll, TRUE);
        gtk_box_append(GTK_BOX(devotion), devotion_controls);
        gtk_box_append(GTK_BOX(devotion), GTK_WIDGET(priv->devotion_title));
        gtk_box_append(GTK_BOX(devotion), scroll);
    }

    priv->memory_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(priv->memory_list, "row-selected",
                     G_CALLBACK(on_memory_selected), self);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(memory_scroll),
                                  GTK_WIDGET(priv->memory_list));
    gtk_widget_set_vexpand(memory_scroll, TRUE);
    gtk_widget_set_size_request(memory_scroll, 260, -1);
    g_signal_connect(add_current, "clicked",
                     G_CALLBACK(on_memory_add_current), self);
    g_signal_connect(delete_verse, "clicked",
                     G_CALLBACK(on_memory_delete), self);
    gtk_box_append(GTK_BOX(memory_left_buttons), add_current);
    gtk_box_append(GTK_BOX(memory_left_buttons), delete_verse);
    gtk_box_append(GTK_BOX(memory_left), memory_scroll);
    gtk_box_append(GTK_BOX(memory_left), memory_left_buttons);

    priv->memory_title = GTK_LABEL(gtk_label_new("Select a verse card"));
    gtk_label_set_xalign(priv->memory_title, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->memory_title), "title-2");
    priv->memory_due = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->memory_due, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->memory_due), "dim-label");
    priv->memory_card = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(priv->memory_card, FALSE);
    gtk_text_view_set_cursor_visible(priv->memory_card, FALSE);
    gtk_text_view_set_wrap_mode(priv->memory_card, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(priv->memory_card, 12);
    gtk_text_view_set_right_margin(priv->memory_card, 12);
    gtk_text_view_set_top_margin(priv->memory_card, 12);
    gtk_text_view_set_bottom_margin(priv->memory_card, 12);
    gtk_widget_add_css_class(GTK_WIDGET(priv->memory_card), "memory-card");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(memory_card_scroll),
                                  GTK_WIDGET(priv->memory_card));
    gtk_widget_set_vexpand(memory_card_scroll, TRUE);
    priv->memory_mode = GTK_DROP_DOWN(gtk_drop_down_new(
        G_LIST_MODEL(modes), NULL));
    g_signal_connect(priv->memory_mode, "notify::selected",
                     G_CALLBACK(on_memory_mode_changed), self);
    gtk_box_append(GTK_BOX(memory_mode_row), gtk_label_new("Card mode"));
    gtk_box_append(GTK_BOX(memory_mode_row), GTK_WIDGET(priv->memory_mode));
    priv->memory_input = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->memory_input, "Type the verse");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->memory_input), TRUE);
    g_signal_connect(check, "clicked", G_CALLBACK(on_memory_check), self);
    gtk_box_append(GTK_BOX(memory_input_row), GTK_WIDGET(priv->memory_input));
    gtk_box_append(GTK_BOX(memory_input_row), check);
    priv->memory_result = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->memory_result, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->memory_result), "dim-label");
    g_object_set_data(G_OBJECT(again), "remembered", GINT_TO_POINTER(0));
    g_object_set_data(G_OBJECT(good), "remembered", GINT_TO_POINTER(1));
    g_signal_connect(again, "clicked", G_CALLBACK(on_memory_review), self);
    g_signal_connect(good, "clicked", G_CALLBACK(on_memory_review), self);
    gtk_box_append(GTK_BOX(memory_review_row), again);
    gtk_box_append(GTK_BOX(memory_review_row), good);
    gtk_box_append(GTK_BOX(memory_right), GTK_WIDGET(priv->memory_title));
    gtk_box_append(GTK_BOX(memory_right), GTK_WIDGET(priv->memory_due));
    gtk_box_append(GTK_BOX(memory_right), memory_card_scroll);
    gtk_box_append(GTK_BOX(memory_right), memory_mode_row);
    gtk_box_append(GTK_BOX(memory_right), memory_input_row);
    gtk_box_append(GTK_BOX(memory_right), GTK_WIDGET(priv->memory_result));
    gtk_box_append(GTK_BOX(memory_right), memory_review_row);
    gtk_paned_set_start_child(GTK_PANED(memory), memory_left);
    gtk_paned_set_end_child(GTK_PANED(memory), memory_right);
    gtk_paned_set_position(GTK_PANED(memory), 280);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), today,
                             gtk_label_new("Today"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), plans,
                             gtk_label_new("Plans"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), devotion,
                             gtk_label_new("Devotion"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), memory,
                             gtk_label_new("Memorize"));
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    g_object_unref(modes);
    return notebook;
}

static GtkWidget *
gsword_window_make_teaching_page(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *scripture_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *notes_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *scripture_scroll = gtk_scrolled_window_new();
    GtkWidget *notes_scroll = gtk_scrolled_window_new();
    GtkWidget *scripture_label = gtk_label_new("Scripture");
    GtkWidget *notes_label = gtk_label_new("Teaching notes");
    GtkWidget *load = gtk_button_new_with_label("Load current passage");
    GtkWidget *save = gtk_button_new_with_label("Save notes");
    GtkWidget *back = gtk_button_new_with_label("Back to reading");
    GtkWidget *fullscreen = gtk_button_new_with_label("Fullscreen");

    gtk_widget_set_margin_top(toolbar, 8);
    gtk_widget_set_margin_bottom(toolbar, 8);
    gtk_widget_set_margin_start(toolbar, 12);
    gtk_widget_set_margin_end(toolbar, 12);
    priv->teaching_ref_entry = GTK_ENTRY(gtk_entry_new());
    gtk_editable_set_editable(GTK_EDITABLE(priv->teaching_ref_entry), FALSE);
    gtk_widget_set_hexpand(GTK_WIDGET(priv->teaching_ref_entry), TRUE);
    gtk_entry_set_placeholder_text(priv->teaching_ref_entry, "Current passage");
    priv->teaching_view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(priv->teaching_view, FALSE);
    gtk_text_view_set_cursor_visible(priv->teaching_view, FALSE);
    gtk_text_view_set_wrap_mode(priv->teaching_view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(priv->teaching_view, 24);
    gtk_text_view_set_right_margin(priv->teaching_view, 24);
    gtk_text_view_set_top_margin(priv->teaching_view, 18);
    gtk_text_view_set_bottom_margin(priv->teaching_view, 18);
    gtk_widget_add_css_class(GTK_WIDGET(priv->teaching_view),
                             "teaching-reader");
    priv->teaching_notes = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_wrap_mode(priv->teaching_notes, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(priv->teaching_notes, 12);
    gtk_text_view_set_right_margin(priv->teaching_notes, 12);
    gtk_text_view_set_top_margin(priv->teaching_notes, 12);
    gtk_text_view_set_bottom_margin(priv->teaching_notes, 12);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scripture_scroll),
                                  GTK_WIDGET(priv->teaching_view));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(notes_scroll),
                                  GTK_WIDGET(priv->teaching_notes));
    gtk_widget_set_vexpand(scripture_scroll, TRUE);
    gtk_widget_set_vexpand(notes_scroll, TRUE);
    gtk_widget_set_size_request(notes_scroll, 320, -1);
    gtk_label_set_xalign(GTK_LABEL(scripture_label), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(notes_label), 0.0f);
    gtk_widget_add_css_class(scripture_label, "heading");
    gtk_widget_add_css_class(notes_label, "heading");
    gtk_widget_set_margin_start(scripture_label, 12);
    gtk_widget_set_margin_start(notes_label, 12);
    gtk_box_append(GTK_BOX(scripture_box), scripture_label);
    gtk_box_append(GTK_BOX(scripture_box), scripture_scroll);
    gtk_box_append(GTK_BOX(notes_box), notes_label);
    gtk_box_append(GTK_BOX(notes_box), notes_scroll);
    g_signal_connect(load, "clicked", G_CALLBACK(on_teaching_load), self);
    g_signal_connect(save, "clicked", G_CALLBACK(on_teaching_save), self);
    g_signal_connect(back, "clicked", G_CALLBACK(on_teaching_close), self);
    g_signal_connect(fullscreen, "clicked",
                     G_CALLBACK(on_teaching_fullscreen), self);
    priv->teaching_fullscreen_button = GTK_BUTTON(fullscreen);
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(priv->teaching_ref_entry));
    gtk_box_append(GTK_BOX(toolbar), load);
    gtk_box_append(GTK_BOX(toolbar), save);
    gtk_box_append(GTK_BOX(toolbar), fullscreen);
    gtk_box_append(GTK_BOX(toolbar), back);
    gtk_paned_set_start_child(GTK_PANED(paned), scripture_box);
    gtk_paned_set_end_child(GTK_PANED(paned), notes_box);
    gtk_paned_set_position(GTK_PANED(paned), 660);
    gtk_box_append(GTK_BOX(box), toolbar);
    gtk_box_append(GTK_BOX(box), paned);
    return box;
}

static void
gsword_window_refresh_reader(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;
    gchar *text;
    gchar *display_key = NULL;
    gchar *status;

    if (priv->module == NULL) {
        gsword_window_set_text(self,
            "No modules found.\n\nInstall SWORD modules or start gsword with "
            "--library PATH.");
        gtk_editable_set_text(GTK_EDITABLE(priv->key_entry), "");
        gsword_window_set_status(self, "No modules found");
        gtk_stack_set_visible_child_name(priv->content_stack, "reader");
        gsword_window_update_index_label(self);
        return;
    }

    if (gsw_key_get_kind(gsw_module_get_key(priv->module)) ==
        GSW_KEY_KIND_VERSE) {
        GswVerseKey *key = (GswVerseKey *)gsw_module_get_key(priv->module);

        if (gsw_verse_key_get_book(key) >= 1 &&
            gsw_verse_key_get_chapter(key) >= 1) {
            text = gsword_window_chapter_text_for_module(priv->module);
            display_key = g_strdup_printf("%s %d",
                                          gsw_verse_key_get_book_name(key),
                                          gsw_verse_key_get_chapter(key));
        } else {
            text = gsw_module_get_text(priv->module, &error);
        }
    } else {
        text = gsw_module_get_text(priv->module, &error);
    }

    if (text == NULL) {
        gsword_window_show_error(self, "Cannot read entry", error);
        g_free(display_key);
        return;
    }
    gsword_window_set_text(self, text);
    g_free(text);

    if (display_key == NULL)
        display_key = g_strdup(gsw_module_get_key_text(priv->module));
    gtk_editable_set_text(GTK_EDITABLE(priv->key_entry),
                          display_key != NULL ? display_key : "");

    status = g_strdup_printf("%s  |  %s",
                             gsw_module_get_name(priv->module),
                             display_key != NULL ? display_key : "");
    gsword_window_set_status(self, status);
    g_free(status);
    g_free(display_key);

    gtk_stack_set_visible_child_name(priv->content_stack, "reader");
    gsword_window_apply_highlights(self);
    gsword_window_update_study(self);
    gsword_window_update_index_label(self);
    gsword_window_save_last_position(self);
}

static gboolean
gsword_window_set_key_text(GswordWindow *self, const gchar *text)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;

    if (priv->module == NULL)
        return FALSE;
    if (!gsw_module_set_key_text(priv->module, text, &error)) {
        gsword_window_show_error(self, "Cannot set key", error);
        return FALSE;
    }
    gsword_window_refresh_reader(self);
    return TRUE;
}

static gchar *
gsword_window_current_storage_key(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswKey *key;

    if (priv->module == NULL)
        return NULL;
    key = gsw_module_get_key(priv->module);
    if (gsw_key_get_kind(key) == GSW_KEY_KIND_VERSE)
        return gsw_verse_key_get_osis_ref((GswVerseKey *)key);
    return g_strdup(gsw_module_get_key_text(priv->module));
}

static gchar *
gsword_window_current_display_key(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->module == NULL)
        return NULL;
    return g_strdup(gsw_module_get_key_text(priv->module));
}

static void
gsword_window_apply_highlights(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswKey *key;
    gchar *reference;
    gchar *dot;
    GtkTextBuffer *buffer;
    GtkTextIter iter;

    if (priv->notes == NULL || priv->module == NULL || priv->highlight_tag == NULL)
        return;
    key = gsw_module_get_key(priv->module);
    if (gsw_key_get_kind(key) != GSW_KEY_KIND_VERSE)
        return;
    reference = gsw_verse_key_get_osis_ref((GswVerseKey *)key);
    if (reference == NULL)
        return;
    dot = strrchr(reference, '.');
    if (dot != NULL)
        *dot = '\0';

    buffer = gtk_text_view_get_buffer(priv->reader);
    {
        GtkTextIter start;
        GtkTextIter finish;

        gtk_text_buffer_get_bounds(buffer, &start, &finish);
        gtk_text_buffer_remove_tag(buffer, priv->highlight_tag,
                                   &start, &finish);
    }
    gtk_text_buffer_get_start_iter(buffer, &iter);
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
                priv->notes, gsw_module_get_name(priv->module), verse_key)) {
            GtkTextIter line_start = iter;
            GtkTextIter line_end = end;

            gtk_text_buffer_apply_tag(buffer, priv->highlight_tag,
                                      &line_start, &line_end);
        }
        g_free(verse_key);
        g_free(line);
        if (!gtk_text_iter_forward_line(&iter))
            break;
    }
    g_free(reference);
}

static void
gsword_window_toggle_highlight(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gchar *storage_key;
    GError *error = NULL;

    if (priv->notes == NULL || priv->module == NULL)
        return;
    storage_key = gsword_window_current_storage_key(self);
    if (storage_key == NULL)
        return;
    if (gsword_notes_store_has_highlight(
            priv->notes, gsw_module_get_name(priv->module), storage_key)) {
        gsword_notes_store_clear_highlight(
            priv->notes, gsw_module_get_name(priv->module), storage_key,
            &error);
        gsword_window_set_status(self, "Highlight removed");
    } else {
        gsword_notes_store_set_highlight(
            priv->notes, gsw_module_get_name(priv->module), storage_key,
            "#fff0a8", &error);
        gsword_window_set_status(self, "Verse highlighted");
    }
    if (error != NULL)
        gsword_window_show_error(self, "Cannot update highlight", error);
    g_free(storage_key);
    gsword_window_apply_highlights(self);
}

static void
gsword_window_set_initial_key(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->module == NULL)
        return;

    if (gsw_key_get_kind(gsw_module_get_key(priv->module)) ==
        GSW_KEY_KIND_VERSE) {
        if (gsw_module_set_key_text(priv->module, "Gen 1:1", NULL))
            return;
        if (gsw_module_set_key_text(priv->module, "Matt 1:1", NULL))
            return;
    }
    gsw_module_set_position(priv->module, GSW_POSITION_TOP, NULL);
}

static void
gsword_window_load_module(GswordWindow *self, const gchar *name)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswModule *module;
    gchar *saved_module = NULL;
    gchar *saved_key = NULL;
    gboolean restored = FALSE;

    module = gsw_manager_create_module(priv->manager, name);
    if (module == NULL) {
        gsword_window_set_status(self, "Cannot open module");
        return;
    }

    g_clear_object(&priv->module);
    priv->module = module;

    if (priv->settings != NULL) {
        saved_module = g_settings_get_string(priv->settings, "last-module");
        saved_key = g_settings_get_string(priv->settings, "last-key");
    }
    if (saved_module != NULL && *saved_module != '\0' &&
        g_strcmp0(saved_module, name) == 0 &&
        saved_key != NULL && *saved_key != '\0')
        restored = gsw_module_set_key_text(priv->module, saved_key, NULL);
    if (!restored)
        gsword_window_set_initial_key(self);

    g_free(saved_module);
    g_free(saved_key);
    if (priv->module_list != NULL) {
        GtkWidget *child = gtk_widget_get_first_child(
            GTK_WIDGET(priv->module_list));

        priv->selecting_module = TRUE;
        while (child != NULL) {
            const gchar *row_name = g_object_get_data(G_OBJECT(child),
                                                      "module-name");

            if (row_name != NULL && g_strcmp0(row_name, name) == 0) {
                gtk_list_box_select_row(priv->module_list,
                                        GTK_LIST_BOX_ROW(child));
                break;
            }
            child = gtk_widget_get_next_sibling(child);
        }
        priv->selecting_module = FALSE;
    }
    gsword_window_refresh_reader(self);
}

static void
on_module_row_selected(GtkListBox *list, GtkListBoxRow *row,
                       gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *name;

    (void)list;
    if (row == NULL || priv->selecting_module)
        return;
    name = g_object_get_data(G_OBJECT(row), "module-name");
    if (name != NULL) {
        gsword_window_load_module(self, name);
        gtk_stack_set_visible_child_name(priv->content_stack, "reader");
    }
}

static GtkWidget *
gsword_window_make_module_row(GswModuleInfo *info)
{
    const gchar *name = gsw_module_info_get_name(info);
    const gchar *description = gsw_module_info_get_description(info);
    const gchar *type = gsw_module_info_get_type_string(info);
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *name_label = gtk_label_new(name);
    GtkWidget *description_label = gtk_label_new(
        description != NULL ? description : "");
    GtkWidget *type_label = gtk_label_new(type != NULL ? type : "");

    gtk_widget_add_css_class(box, "module-row");
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);

    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0f);
    gtk_widget_add_css_class(name_label, "heading");
    gtk_label_set_xalign(GTK_LABEL(description_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(description_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(description_label), 28);
    gtk_widget_add_css_class(description_label, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(type_label), 0.0f);
    gtk_widget_add_css_class(type_label, "caption");
    gtk_widget_add_css_class(type_label, "dim-label");

    gtk_box_append(GTK_BOX(box), name_label);
    if (description != NULL && *description != '\0')
        gtk_box_append(GTK_BOX(box), description_label);
    if (type != NULL && *type != '\0')
        gtk_box_append(GTK_BOX(box), type_label);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    g_object_set_data_full(G_OBJECT(row), "module-name", g_strdup(name),
                           g_free);
    return row;
}

static void
gsword_window_populate_modules(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *infos;
    gchar *wanted = NULL;
    GtkListBoxRow *selected = NULL;
    GtkListBoxRow *first = NULL;
    guint i;

    if (priv->manager == NULL) {
        gtk_stack_set_visible_child_name(priv->content_stack, "reader");
        gsword_window_refresh_reader(self);
        return;
    }

    infos = gsw_manager_get_module_infos(priv->manager);
    if (priv->settings != NULL)
        wanted = g_settings_get_string(priv->settings, "last-module");

    while (gtk_widget_get_first_child(GTK_WIDGET(priv->module_list)) != NULL) {
        GtkWidget *child = gtk_widget_get_first_child(
            GTK_WIDGET(priv->module_list));

        gtk_list_box_remove(priv->module_list, child);
    }

    {
        GtkStringList *names = gtk_string_list_new(NULL);
        gchar *parallel_a = priv->settings != NULL
            ? g_settings_get_string(priv->settings, "parallel-a") : NULL;
        gchar *parallel_b = priv->settings != NULL
            ? g_settings_get_string(priv->settings, "parallel-b") : NULL;
        guint selected_a = 0;
        guint selected_b = infos->len > 1 ? 1 : 0;

        for (i = 0; i < infos->len; i++) {
            GswModuleInfo *info = g_ptr_array_index(infos, i);
            GtkWidget *row = gsword_window_make_module_row(info);
            const gchar *name = gsw_module_info_get_name(info);

            gtk_string_list_append(names, name);
            gtk_list_box_append(priv->module_list, row);
            if (first == NULL)
                first = GTK_LIST_BOX_ROW(row);
            if (wanted != NULL && g_strcmp0(wanted, name) == 0)
                selected = GTK_LIST_BOX_ROW(row);
            if (parallel_a != NULL && g_strcmp0(parallel_a, name) == 0)
                selected_a = i;
            if (parallel_b != NULL && g_strcmp0(parallel_b, name) == 0)
                selected_b = i;
        }

        g_clear_object(&priv->module_names);
        priv->module_names = names;
        gtk_drop_down_set_model(priv->parallel_a, G_LIST_MODEL(names));
        gtk_drop_down_set_model(priv->parallel_b, G_LIST_MODEL(names));
        gtk_drop_down_set_selected(priv->parallel_a, selected_a);
        gtk_drop_down_set_selected(priv->parallel_b, selected_b);
        g_free(parallel_a);
        g_free(parallel_b);
    }
    g_ptr_array_unref(infos);
    g_free(wanted);

    if (selected == NULL)
        selected = first;
    if (selected != NULL) {
        gtk_list_box_select_row(priv->module_list, selected);
    } else {
        gtk_stack_set_visible_child_name(priv->content_stack, "reader");
        gsword_window_refresh_reader(self);
    }
}

static void
on_key_activate(GtkEntry *entry, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);

    gsword_window_set_key_text(self, gtk_editable_get_text(GTK_EDITABLE(entry)));
}

static void
on_go_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    (void)button;
    gsword_window_set_key_text(
        self, gtk_editable_get_text(GTK_EDITABLE(priv->key_entry)));
}

static void
on_chapter_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gint direction = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button),
                                                       "direction"));
    GswVerseKey *key;
    gint chapter;
    gint maximum;
    GError *error = NULL;

    if (priv->module == NULL ||
        gsw_key_get_kind(gsw_module_get_key(priv->module)) !=
            GSW_KEY_KIND_VERSE)
        return;
    key = (GswVerseKey *)gsw_module_get_key(priv->module);
    chapter = gsw_verse_key_get_chapter(key);
    maximum = gsw_verse_key_get_chapter_max(key);
    if (chapter < 1 || maximum < 1)
        return;
    if ((direction < 0 && chapter <= 1) ||
        (direction > 0 && chapter >= maximum))
        return;

    gsw_verse_key_set_chapter(key, chapter + direction);
    if (!gsw_module_set_key_text(priv->module,
                                 gsw_module_get_key_text(priv->module),
                                 &error)) {
        gsword_window_show_error(self, "Cannot change chapter", error);
        return;
    }
    gsword_window_refresh_reader(self);
}

static void
on_copy_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(priv->reader);
    GtkTextIter start;
    GtkTextIter end;
    gchar *text;
    GdkClipboard *clipboard;

    (void)button;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    clipboard = gtk_widget_get_clipboard(GTK_WIDGET(self));
    gdk_clipboard_set_text(clipboard, text != NULL ? text : "");
    g_free(text);
    gsword_window_set_status(self, "Copied to clipboard");
}

static void
on_note_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_open_note_for_current(GSWORD_WINDOW(user_data));
}

static void
on_highlight_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_toggle_highlight(GSWORD_WINDOW(user_data));
}

static void
on_font_scale_changed(GtkSpinButton *spin, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    priv->font_scale = gtk_spin_button_get_value(spin);
    if (priv->settings != NULL)
        g_settings_set_double(priv->settings, "font-scale", priv->font_scale);
    gsword_window_apply_css(self);
}

static void
on_theme_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint selected = adw_combo_row_get_selected(ADW_COMBO_ROW(object));

    (void)pspec;
    g_free(priv->theme);
    priv->theme = g_strdup(selected == 1 ? "light"
                          : selected == 2 ? "dark" : "system");
    if (priv->settings != NULL)
        g_settings_set_string(priv->settings, "theme", priv->theme);
    gsword_window_apply_theme(self);
}

static void
gsword_window_show_preferences(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    AdwPreferencesDialog *dialog;
    AdwPreferencesPage *page;
    AdwPreferencesGroup *group;
    AdwActionRow *font_row;
    AdwComboRow *theme_row;
    GtkAdjustment *adjustment;
    GtkWidget *spin;
    GtkStringList *themes;
    guint selected = 0;

    if (priv->preferences_dialog != NULL) {
        adw_dialog_present(priv->preferences_dialog, GTK_WIDGET(self));
        return;
    }

    dialog = ADW_PREFERENCES_DIALOG(adw_preferences_dialog_new());
    adw_dialog_set_title(ADW_DIALOG(dialog), "Preferences");
    adw_dialog_set_content_width(ADW_DIALOG(dialog), 620);

    page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
    group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(group, "Reading");
    adw_preferences_group_set_description(
        group, "Reading appearance is applied immediately.");

    font_row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(font_row),
                                  "Font size");
    adw_action_row_set_subtitle(font_row, "Scale the scripture text size");
    adjustment = gtk_adjustment_new(priv->font_scale, 0.75, 2.5, 0.1, 0.25, 0);
    spin = gtk_spin_button_new(adjustment, 0.1, 1);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    g_signal_connect(spin, "value-changed",
                     G_CALLBACK(on_font_scale_changed), self);
    adw_action_row_add_suffix(font_row, spin);
    adw_action_row_set_activatable_widget(font_row, spin);

    theme_row = ADW_COMBO_ROW(adw_combo_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(theme_row),
                                  "Color scheme");
    adw_action_row_set_subtitle(ADW_ACTION_ROW(theme_row),
                                "Follow the desktop or force light/dark mode");
    themes = gtk_string_list_new(
        (const gchar *[]) { "Follow system", "Light", "Dark", NULL });
    adw_combo_row_set_model(theme_row, G_LIST_MODEL(themes));
    g_object_unref(themes);
    if (g_strcmp0(priv->theme, "light") == 0)
        selected = 1;
    else if (g_strcmp0(priv->theme, "dark") == 0)
        selected = 2;
    adw_combo_row_set_selected(theme_row, selected);
    g_signal_connect(theme_row, "notify::selected",
                     G_CALLBACK(on_theme_changed), self);

    adw_preferences_group_add(group, GTK_WIDGET(font_row));
    adw_preferences_group_add(group, GTK_WIDGET(theme_row));
    adw_preferences_page_add(page, group);
    adw_preferences_dialog_add(dialog, page);

    priv->preferences_dialog = ADW_DIALOG(g_object_ref_sink(dialog));
    adw_dialog_present(priv->preferences_dialog, GTK_WIDGET(self));
}

static void
on_preferences_action(GSimpleAction *action, GVariant *parameter,
                      gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_show_preferences(GSWORD_WINDOW(user_data));
}

static void
on_focus_key_action(GSimpleAction *action, GVariant *parameter,
                    gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    (void)action;
    (void)parameter;
    gtk_widget_grab_focus(GTK_WIDGET(priv->key_entry));
}

static GtkWidget *
gsword_window_make_sidebar(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *label = gtk_label_new("Modules");
    GtkWidget *scrolled = gtk_scrolled_window_new();

    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "heading");
    gtk_widget_set_margin_top(label, 10);
    gtk_widget_set_margin_start(label, 12);
    gtk_widget_set_margin_end(label, 12);

    priv->module_list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_widget_add_css_class(GTK_WIDGET(priv->module_list),
                             "navigation-sidebar");
    gtk_list_box_set_selection_mode(priv->module_list, GTK_SELECTION_SINGLE);
    g_signal_connect(priv->module_list, "row-selected",
                     G_CALLBACK(on_module_row_selected), self);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(priv->module_list));
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), scrolled);
    return box;
}

static GtkWidget *
gsword_window_make_scrolled_text(GtkTextView **view)
{
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkWidget *text = gtk_text_view_new();

    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 10);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(text), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(text), 10);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text);
    *view = GTK_TEXT_VIEW(text);
    return scrolled;
}

static void
on_study_toggled(GtkToggleButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    gtk_widget_set_visible(priv->study_panel,
                           gtk_toggle_button_get_active(button));
}

static void
on_workspace_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *name = gtk_stack_get_visible_child_name(
        GTK_STACK(object));

    (void)pspec;
    if (priv->settings != NULL && name != NULL)
        g_settings_set_string(priv->settings, "active-page", name);
    if (g_strcmp0(name, "parallel") == 0 && priv->parallel_key != NULL) {
        if (priv->module != NULL)
            gtk_editable_set_text(
                GTK_EDITABLE(priv->parallel_key),
                gsw_module_get_key_text(priv->module));
        gsword_window_parallel_load(self);
    } else if (g_strcmp0(name, "search") == 0) {
        gsword_window_update_index_label(self);
    } else if (g_strcmp0(name, "notes") == 0) {
        gsword_window_refresh_notes(self);
        if (priv->note_storage_key == NULL && priv->module != NULL)
            gsword_window_open_note_for_current(self);
    } else if (g_strcmp0(name, "plan") == 0) {
        gsword_window_refresh_plans(self);
        gsword_window_refresh_devotion(self);
        gsword_window_refresh_memory(self);
    } else if (g_strcmp0(name, "teaching") == 0) {
        gsword_window_teaching_load(self);
    } else if (g_strcmp0(name, "library") == 0) {
        if (priv->install_sources == NULL ||
            priv->install_sources->len == 0)
            gsword_window_refresh_install_sources(self);
        gsword_window_refresh_installed_dropdown(self);
        gsword_window_refresh_remote_modules(self);
    }
}

static void
gsword_window_restore_page(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gchar *page = NULL;

    if (priv->settings != NULL)
        page = g_settings_get_string(priv->settings, "active-page");
    if (page != NULL &&
        (g_strcmp0(page, "reader") == 0 ||
         g_strcmp0(page, "parallel") == 0 ||
         g_strcmp0(page, "search") == 0 ||
         g_strcmp0(page, "plan") == 0 ||
         g_strcmp0(page, "teaching") == 0))
        gtk_stack_set_visible_child_name(priv->content_stack, page);
    g_free(page);
}

static void
on_lexicon_lookup(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_lexicon_lookup(GSWORD_WINDOW(user_data));
}

static GtkWidget *
gsword_window_make_study_panel(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *notebook = gtk_notebook_new();
    GtkWidget *commentary;
    GtkWidget *strongs;
    GtkWidget *crossrefs;
    GtkWidget *lexicon_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *lexicon_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *lexicon_button = gtk_button_new_with_label("Lookup");
    GtkWidget *lexicon;

    commentary = gsword_window_make_scrolled_text(&priv->commentary_view);
    strongs = gsword_window_make_scrolled_text(&priv->strongs_view);
    crossrefs = gsword_window_make_scrolled_text(&priv->crossref_view);
    lexicon = gsword_window_make_scrolled_text(&priv->lexicon_view);
    priv->lexicon_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->lexicon_entry, "Strong's / dictionary key");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->lexicon_entry), TRUE);
    g_signal_connect(priv->lexicon_entry, "activate",
                     G_CALLBACK(on_lexicon_lookup), self);
    g_signal_connect(lexicon_button, "clicked",
                     G_CALLBACK(on_lexicon_lookup), self);
    gtk_box_append(GTK_BOX(lexicon_controls), GTK_WIDGET(priv->lexicon_entry));
    gtk_box_append(GTK_BOX(lexicon_controls), lexicon_button);
    gtk_box_append(GTK_BOX(lexicon_box), lexicon_controls);
    gtk_widget_set_vexpand(lexicon, TRUE);
    gtk_box_append(GTK_BOX(lexicon_box), lexicon);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), commentary,
                             gtk_label_new("Commentary"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), strongs,
                             gtk_label_new("Strong's"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), crossrefs,
                             gtk_label_new("Cross-refs"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), lexicon_box,
                             gtk_label_new("Lexicon"));
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    gtk_widget_set_size_request(notebook, 300, -1);
    priv->study_notebook = GTK_NOTEBOOK(notebook);
    priv->study_panel = notebook;
    return notebook;
}

static GtkWidget *
gsword_window_make_reader(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *reader_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *previous = gtk_button_new_from_icon_name(
        "go-previous-symbolic");
    GtkWidget *next = gtk_button_new_from_icon_name("go-next-symbolic");
    GtkWidget *go = gtk_button_new_with_label("Go");
    GtkWidget *copy = gtk_button_new_from_icon_name("edit-copy-symbolic");
    GtkWidget *note = gtk_button_new_from_icon_name("document-edit-symbolic");
    GtkWidget *highlight = gtk_button_new_from_icon_name(
        "emblem-favorite-symbolic");
    GtkWidget *teaching = gtk_button_new_with_label("Teach");
    GtkWidget *study_button = gtk_toggle_button_new();
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *study_panel = gsword_window_make_study_panel(self);

    gtk_widget_set_margin_top(toolbar, 8);
    gtk_widget_set_margin_bottom(toolbar, 8);
    gtk_widget_set_margin_start(toolbar, 12);
    gtk_widget_set_margin_end(toolbar, 12);

    priv->key_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->key_entry,
                                   "Genesis 1:1 / 启示录 1:1");
    gtk_editable_set_width_chars(GTK_EDITABLE(priv->key_entry), 24);
    gtk_widget_set_hexpand(GTK_WIDGET(priv->key_entry), TRUE);
    g_signal_connect(priv->key_entry, "activate",
                     G_CALLBACK(on_key_activate), self);
    g_signal_connect(go, "clicked", G_CALLBACK(on_go_clicked), self);

    g_object_set_data(G_OBJECT(previous), "direction", GINT_TO_POINTER(-1));
    g_object_set_data(G_OBJECT(next), "direction", GINT_TO_POINTER(1));
    g_signal_connect(previous, "clicked", G_CALLBACK(on_chapter_clicked),
                     self);
    g_signal_connect(next, "clicked", G_CALLBACK(on_chapter_clicked), self);
    g_signal_connect(copy, "clicked", G_CALLBACK(on_copy_clicked), self);
    g_signal_connect(note, "clicked", G_CALLBACK(on_note_clicked), self);
    g_signal_connect(highlight, "clicked",
                     G_CALLBACK(on_highlight_clicked), self);
    g_signal_connect(teaching, "clicked",
                     G_CALLBACK(on_teaching_clicked), self);
    gtk_widget_set_tooltip_text(teaching,
                                "Open teaching layout for this passage");
    gtk_button_set_icon_name(GTK_BUTTON(study_button),
                             "dialog-information-symbolic");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(study_button), TRUE);
    g_signal_connect(study_button, "toggled",
                     G_CALLBACK(on_study_toggled), self);
    gtk_widget_set_tooltip_text(previous, "Previous chapter");
    gtk_widget_set_tooltip_text(next, "Next chapter");
    gtk_widget_set_tooltip_text(copy, "Copy entry");
    gtk_widget_set_tooltip_text(note, "Open note for this verse");
    gtk_widget_set_tooltip_text(highlight, "Toggle verse highlight");
    gtk_widget_set_tooltip_text(study_button, "Toggle study panel");

    gtk_box_append(GTK_BOX(toolbar), previous);
    gtk_box_append(GTK_BOX(toolbar), next);
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(priv->key_entry));
    gtk_box_append(GTK_BOX(toolbar), go);
    gtk_box_append(GTK_BOX(toolbar), note);
    gtk_box_append(GTK_BOX(toolbar), highlight);
    gtk_box_append(GTK_BOX(toolbar), teaching);
    gtk_box_append(GTK_BOX(toolbar), study_button);
    gtk_box_append(GTK_BOX(toolbar), copy);

    priv->reader = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(priv->reader, FALSE);
    gtk_text_view_set_cursor_visible(priv->reader, FALSE);
    gtk_text_view_set_wrap_mode(priv->reader, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(priv->reader, 28);
    gtk_text_view_set_right_margin(priv->reader, 28);
    gtk_text_view_set_top_margin(priv->reader, 22);
    gtk_text_view_set_bottom_margin(priv->reader, 22);
    gtk_widget_add_css_class(GTK_WIDGET(priv->reader), "gsw-reader");
    priv->highlight_tag = gtk_text_buffer_create_tag(
        gtk_text_view_get_buffer(priv->reader), "gsword-highlight",
        "background", "#fff0a8", NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(priv->reader));
    gtk_widget_set_vexpand(content_box, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(study_panel, FALSE);
    gtk_widget_set_size_request(study_panel, 320, -1);
    gtk_box_append(GTK_BOX(content_box), scrolled);
    gtk_box_append(GTK_BOX(content_box), study_panel);

    gtk_box_append(GTK_BOX(reader_box), toolbar);
    gtk_box_append(GTK_BOX(reader_box), content_box);
    return reader_box;
}

static const gchar *
gsword_window_dropdown_name(GswordWindow *self, GtkDropDown *dropdown)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint selected = gtk_drop_down_get_selected(dropdown);

    if (priv->module_names == NULL || selected == GTK_INVALID_LIST_POSITION)
        return NULL;
    return gtk_string_list_get_string(priv->module_names, selected);
}

static void
gsword_window_set_initial_key_for_module(GswModule *module)
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
gsword_window_parallel_set_text(GtkTextView *view, GswModule *module,
                                const gchar *key_text)
{
    GError *error = NULL;
    gchar *text;

    if (module == NULL) {
        gsword_window_set_view_text(view, "No module selected.");
        return;
    }
    if (key_text != NULL && *key_text != '\0' &&
        !gsw_module_set_key_text(module, key_text, &error)) {
        g_clear_error(&error);
        gsword_window_set_initial_key_for_module(module);
    } else if (key_text == NULL || *key_text == '\0') {
        gsword_window_set_initial_key_for_module(module);
    }

    text = gsword_window_chapter_text_for_module(module);
    if (text == NULL)
        text = gsw_module_get_text(module, &error);
    if (text == NULL) {
        gsword_window_set_view_text(view, "Cannot read this entry.");
        g_clear_error(&error);
        return;
    }
    gsword_window_set_view_text(view, text);
    g_free(text);
}

static void
gsword_window_parallel_load(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *name_a = gsword_window_dropdown_name(self, priv->parallel_a);
    const gchar *name_b = gsword_window_dropdown_name(self, priv->parallel_b);
    const gchar *entry_text = gtk_editable_get_text(
        GTK_EDITABLE(priv->parallel_key));
    gchar *key_text = entry_text != NULL && *entry_text != '\0'
                    ? g_strdup(entry_text)
                    : g_strdup(priv->module != NULL
                               ? gsw_module_get_key_text(priv->module)
                               : "Gen 1:1");

    if (key_text != NULL)
        gtk_editable_set_text(GTK_EDITABLE(priv->parallel_key), key_text);

    g_clear_object(&priv->parallel_module_a);
    g_clear_object(&priv->parallel_module_b);
    if (name_a != NULL)
        priv->parallel_module_a = gsw_manager_create_module(priv->manager,
                                                           name_a);
    if (name_b != NULL)
        priv->parallel_module_b = gsw_manager_create_module(priv->manager,
                                                           name_b);
    gsword_window_parallel_set_text(priv->parallel_view_a,
                                   priv->parallel_module_a, key_text);
    gsword_window_parallel_set_text(priv->parallel_view_b,
                                   priv->parallel_module_b, key_text);

    if (priv->settings != NULL) {
        if (name_a != NULL)
            g_settings_set_string(priv->settings, "parallel-a", name_a);
        if (name_b != NULL)
            g_settings_set_string(priv->settings, "parallel-b", name_b);
    }
    g_free(key_text);
}

static void
on_parallel_load(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_parallel_load(GSWORD_WINDOW(user_data));
}

static void
gsword_window_sync_adjustment(GtkAdjustment *from, GtkAdjustment *to)
{
    gdouble from_span = gtk_adjustment_get_upper(from) -
                        gtk_adjustment_get_page_size(from);
    gdouble to_span = gtk_adjustment_get_upper(to) -
                      gtk_adjustment_get_page_size(to);
    gdouble fraction = from_span > 0
                     ? gtk_adjustment_get_value(from) / from_span : 0.0;

    gtk_adjustment_set_value(to, fraction * MAX(to_span, 0.0));
}

static void
on_parallel_scroll_a(GtkAdjustment *adjustment, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->syncing_scroll ||
        !gtk_check_button_get_active(priv->parallel_sync))
        return;
    priv->syncing_scroll = TRUE;
    gsword_window_sync_adjustment(
        adjustment,
        gtk_scrolled_window_get_vadjustment(priv->parallel_scroll_b));
    priv->syncing_scroll = FALSE;
}

static void
on_parallel_scroll_b(GtkAdjustment *adjustment, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->syncing_scroll ||
        !gtk_check_button_get_active(priv->parallel_sync))
        return;
    priv->syncing_scroll = TRUE;
    gsword_window_sync_adjustment(
        adjustment,
        gtk_scrolled_window_get_vadjustment(priv->parallel_scroll_a));
    priv->syncing_scroll = FALSE;
}

static GtkWidget *
gsword_window_make_parallel_page(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *module_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *key_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *load = gtk_button_new_with_label("Load both");
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *scrolled_a;
    GtkWidget *scrolled_b;

    gtk_widget_set_margin_top(controls, 8);
    gtk_widget_set_margin_bottom(controls, 8);
    gtk_widget_set_margin_start(controls, 12);
    gtk_widget_set_margin_end(controls, 12);

    priv->parallel_a = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    priv->parallel_b = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    priv->parallel_key = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->parallel_key,
                                   "Shared chapter/key");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->parallel_key), TRUE);
    priv->parallel_sync = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label("Sync scroll"));
    gtk_check_button_set_active(priv->parallel_sync, TRUE);
    g_signal_connect(load, "clicked", G_CALLBACK(on_parallel_load), self);
    g_signal_connect(priv->parallel_key, "activate",
                     G_CALLBACK(on_parallel_load), self);

    gtk_box_append(GTK_BOX(module_row), GTK_WIDGET(priv->parallel_a));
    gtk_box_append(GTK_BOX(module_row), GTK_WIDGET(priv->parallel_b));
    gtk_box_append(GTK_BOX(module_row), load);
    gtk_widget_set_hexpand(GTK_WIDGET(priv->parallel_key), TRUE);
    gtk_box_append(GTK_BOX(key_row), GTK_WIDGET(priv->parallel_key));
    gtk_box_append(GTK_BOX(key_row), GTK_WIDGET(priv->parallel_sync));
    gtk_box_append(GTK_BOX(controls), module_row);
    gtk_box_append(GTK_BOX(controls), key_row);

    scrolled_a = gsword_window_make_scrolled_text(&priv->parallel_view_a);
    scrolled_b = gsword_window_make_scrolled_text(&priv->parallel_view_b);
    priv->parallel_scroll_a = GTK_SCROLLED_WINDOW(scrolled_a);
    priv->parallel_scroll_b = GTK_SCROLLED_WINDOW(scrolled_b);
    g_signal_connect(gtk_scrolled_window_get_vadjustment(
                         priv->parallel_scroll_a),
                     "value-changed", G_CALLBACK(on_parallel_scroll_a), self);
    g_signal_connect(gtk_scrolled_window_get_vadjustment(
                         priv->parallel_scroll_b),
                     "value-changed", G_CALLBACK(on_parallel_scroll_b), self);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_paned_set_start_child(GTK_PANED(paned), scrolled_a);
    gtk_paned_set_end_child(GTK_PANED(paned), scrolled_b);

    gtk_box_append(GTK_BOX(box), controls);
    gtk_box_append(GTK_BOX(box), paned);
    return box;
}

static void
gsword_window_run_search(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswSearchOptions *options;
    GPtrArray *results;
    GError *error = NULL;
    guint i;
    guint selected = gtk_drop_down_get_selected(priv->search_type);

    if (priv->module == NULL) {
        gsword_window_set_status(self, "Open a module before searching");
        return;
    }
    while (gtk_widget_get_first_child(GTK_WIDGET(priv->search_results)) != NULL)
        gtk_list_box_remove(priv->search_results,
                            gtk_widget_get_first_child(
                                GTK_WIDGET(priv->search_results)));

    options = gsw_search_options_new();
    gsw_search_options_set_text(
        options, gtk_editable_get_text(GTK_EDITABLE(priv->search_entry)));
    gsw_search_options_set_search_type(
        options, selected == 1 ? GSW_SEARCH_MULTIWORD
                             : selected == 2 ? GSW_SEARCH_REGEX
                                             : GSW_SEARCH_PHRASE);
    gsw_search_options_set_case_sensitive(
        options, gtk_check_button_get_active(priv->search_case));
    gsw_search_options_set_max_results(options, 200);
    results = gsw_module_search_full(priv->module, options, NULL, NULL, NULL,
                                     &error);
    gsw_search_options_unref(options);
    if (results == NULL) {
        gsword_window_show_error(self, "Search failed", error);
        return;
    }

    for (i = 0; i < results->len; i++) {
        GswSearchResult *result = g_ptr_array_index(results, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label;
        gchar *text = g_strdup_printf("%s\n%s",
            gsw_search_result_get_key(result),
            gsw_search_result_get_snippet(result));

        label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "result-module",
                               g_strdup(gsw_search_result_get_module_name(result)),
                               g_free);
        g_object_set_data_full(G_OBJECT(row), "result-key",
                               g_strdup(gsw_search_result_get_key(result)),
                               g_free);
        gtk_list_box_append(priv->search_results, row);
        g_free(text);
    }
    {
        gchar *message = g_strdup_printf("Search: %u result(s)", results->len);

        gsword_window_set_status(self, message);
        g_free(message);
    }
    g_ptr_array_unref(results);
}

static void
on_search_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_run_search(GSWORD_WINDOW(user_data));
}

static void
on_search_activate(GtkEntry *entry, gpointer user_data)
{
    (void)entry;
    gsword_window_run_search(GSWORD_WINDOW(user_data));
}

static void
on_search_result_selected(GtkListBox *list, GtkListBoxRow *row,
                          gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *module_name;
    const gchar *key;

    (void)list;
    if (row == NULL)
        return;
    module_name = g_object_get_data(G_OBJECT(row), "result-module");
    key = g_object_get_data(G_OBJECT(row), "result-key");
    if (module_name != NULL && g_strcmp0(module_name,
                                         gsw_module_get_name(priv->module)) != 0)
        gsword_window_load_module(self, module_name);
    if (key != NULL)
        gsword_window_set_key_text(self, key);
    gtk_stack_set_visible_child_name(priv->content_stack, "reader");
}

static void
gsword_window_index_progress(const gchar *module_name, guint processed,
                             guint total, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    gchar *message = g_strdup_printf("Indexing %s: %u/%u",
                                     module_name, processed, total);

    gsword_window_set_status(self, message);
    g_free(message);
    while (g_main_context_iteration(NULL, FALSE))
        ;
}

static void
gsword_window_update_index_label(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->module == NULL)
        gtk_label_set_text(priv->index_label, "Index: no module");
    else
        gtk_label_set_text(priv->index_label,
            gsw_module_has_search_index(priv->module)
            ? "Index: available" : "Index: not built");
}

static void
on_build_index(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;

    (void)button;
    if (priv->module == NULL)
        return;
    if (!gsw_module_build_search_index(priv->module, NULL,
                                       gsword_window_index_progress, self,
                                       &error)) {
        gsword_window_show_error(self, "Cannot build index", error);
        return;
    }
    gsword_window_set_status(self, "Search index built");
    gsword_window_update_index_label(self);
}

static void
on_delete_index(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;

    (void)button;
    if (priv->module == NULL)
        return;
    if (!gsw_module_delete_search_index(priv->module, &error)) {
        gsword_window_show_error(self, "Cannot delete index", error);
        return;
    }
    gsword_window_set_status(self, "Search index deleted");
    gsword_window_update_index_label(self);
}

static GtkWidget *
gsword_window_make_search_page(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *search_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *options_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *search = gtk_button_new_with_label("Search");
    GtkWidget *build = gtk_button_new_from_icon_name(
        "document-save-symbolic");
    GtkWidget *delete = gtk_button_new_from_icon_name("edit-delete-symbolic");
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkStringList *types;

    gtk_widget_set_margin_top(controls, 8);
    gtk_widget_set_margin_bottom(controls, 8);
    gtk_widget_set_margin_start(controls, 12);
    gtk_widget_set_margin_end(controls, 12);
    gtk_widget_set_tooltip_text(build, "Build search index");
    gtk_widget_set_tooltip_text(delete, "Delete search index");

    priv->search_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->search_entry,
                                   "Search text");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->search_entry), TRUE);
    priv->search_type = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    types = gtk_string_list_new(
        (const gchar *[]) { "Phrase", "Multiword", "Regex", NULL });
    gtk_drop_down_set_model(priv->search_type, G_LIST_MODEL(types));
    g_object_unref(types);
    priv->search_case = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label("Case sensitive"));
    priv->index_label = GTK_LABEL(gtk_label_new("Index: no module"));
    gtk_label_set_xalign(priv->index_label, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->index_label), "dim-label");
    gtk_widget_add_css_class(GTK_WIDGET(priv->index_label), "caption");

    g_signal_connect(search, "clicked", G_CALLBACK(on_search_clicked), self);
    g_signal_connect(priv->search_entry, "activate",
                     G_CALLBACK(on_search_activate), self);
    g_signal_connect(build, "clicked", G_CALLBACK(on_build_index), self);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_delete_index), self);

    gtk_box_append(GTK_BOX(search_row), GTK_WIDGET(priv->search_entry));
    gtk_box_append(GTK_BOX(search_row), search);
    gtk_box_append(GTK_BOX(options_row), GTK_WIDGET(priv->search_type));
    gtk_box_append(GTK_BOX(options_row), GTK_WIDGET(priv->search_case));
    gtk_box_append(GTK_BOX(options_row), build);
    gtk_box_append(GTK_BOX(options_row), delete);
    gtk_box_append(GTK_BOX(controls), search_row);
    gtk_box_append(GTK_BOX(controls), options_row);
    gtk_box_append(GTK_BOX(controls), GTK_WIDGET(priv->index_label));

    priv->search_results = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(priv->search_results, "row-selected",
                     G_CALLBACK(on_search_result_selected), self);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(priv->search_results));
    gtk_widget_set_vexpand(scrolled, TRUE);

    gtk_box_append(GTK_BOX(box), controls);
    gtk_box_append(GTK_BOX(box), scrolled);
    return box;
}

static GswInstallSource *
gsword_window_current_install_source(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint selected = priv->source_dropdown != NULL
                   ? gtk_drop_down_get_selected(priv->source_dropdown)
                   : GTK_INVALID_LIST_POSITION;

    if (priv->install_sources == NULL ||
        selected == GTK_INVALID_LIST_POSITION ||
        selected >= priv->install_sources->len)
        return NULL;
    return g_ptr_array_index(priv->install_sources, selected);
}

static const gchar *
module_status_text(guint status)
{
    if (status & GSW_MODULE_STATUS_NEW)
        return "Not installed";
    if (status & GSW_MODULE_STATUS_UPDATED)
        return "Update available";
    if (status & GSW_MODULE_STATUS_OLDER)
        return "Installed - local version is newer";
    if (status & GSW_MODULE_STATUS_SAME_VERSION)
        return "Installed - up to date";
    return "Installed";
}

static void
gsword_window_refresh_installed_dropdown(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *infos = gsw_manager_get_module_infos(priv->manager);
    GtkStringList *names = gtk_string_list_new(NULL);
    guint i;

    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        gtk_string_list_append(names, gsw_module_info_get_name(info));
    }
    g_clear_object(&priv->installed_names);
    priv->installed_names = names;
    gtk_drop_down_set_model(priv->installed_dropdown, G_LIST_MODEL(names));
    if (infos->len > 0)
        gtk_drop_down_set_selected(priv->installed_dropdown, 0);
    g_ptr_array_unref(infos);
}

static void
gsword_window_refresh_install_sources(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *sources;
    GtkStringList *names;
    guint i;

    if (priv->installer == NULL) {
        gsword_window_set_status(self, "Install manager is not available");
        return;
    }
    sources = gsw_install_manager_get_sources(priv->installer);
    if (priv->install_sources != NULL)
        g_ptr_array_unref(priv->install_sources);
    priv->install_sources = g_ptr_array_new_with_free_func(g_object_unref);
    names = gtk_string_list_new(NULL);
    for (i = 0; i < sources->len; i++) {
        GswInstallSource *source = g_ptr_array_index(sources, i);

        g_ptr_array_add(priv->install_sources, g_object_ref(source));
        gtk_string_list_append(names,
            gsw_install_source_get_caption(source) != NULL
            ? gsw_install_source_get_caption(source) : "Source");
    }
    g_ptr_array_unref(sources);
    g_clear_object(&priv->source_names);
    priv->source_names = names;
    gtk_drop_down_set_model(priv->source_dropdown, G_LIST_MODEL(names));
    if (priv->install_sources->len > 0)
        gtk_drop_down_set_selected(priv->source_dropdown, 0);
    gsword_window_refresh_remote_modules(self);
}

static void
gsword_window_update_remote_details(GswordWindow *self, const gchar *name)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswInstallSource *source = gsword_window_current_install_source(self);
    GswModuleInfo *info = NULL;
    guint status = 0;
    gchar *details;

    if (source == NULL || name == NULL)
        return;
    info = gsw_install_manager_lookup_remote_module_info(
        priv->installer, source, name, NULL);
    if (info == NULL)
        return;
    status = gsw_install_manager_get_module_status(priv->manager, info);
    gtk_label_set_text(priv->remote_title, name);
    gtk_label_set_text(priv->remote_status, module_status_text(status));
    details = g_strdup_printf("%s\n\n%s  |  %s  |  %" G_GUINT64_FORMAT " bytes",
        gsw_module_info_get_description(info) != NULL
        ? gsw_module_info_get_description(info) : "",
        gsw_module_info_get_type_string(info) != NULL
        ? gsw_module_info_get_type_string(info) : "Unknown",
        gsw_module_info_get_version(info) != NULL
        ? gsw_module_info_get_version(info) : "?",
        gsw_module_info_get_install_size(info));
    gtk_label_set_text(priv->remote_description, details);
    g_free(details);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->install_button),
                             (status & GSW_MODULE_STATUS_NEW) ||
                             (status & GSW_MODULE_STATUS_UPDATED));
}

static void
gsword_window_refresh_remote_modules(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswInstallSource *source = gsword_window_current_install_source(self);
    GPtrArray *infos;
    GError *error = NULL;
    const gchar *query;
    gchar *folded_query = NULL;
    guint i;

    gsword_window_clear_list(priv->remote_list);
    gtk_label_set_text(priv->remote_title, "Select a module");
    gtk_label_set_text(priv->remote_description, "");
    gtk_label_set_text(priv->remote_status, "");
    gtk_widget_set_sensitive(GTK_WIDGET(priv->install_button), FALSE);
    if (source == NULL) {
        gtk_label_set_text(priv->remote_status,
                           "No source selected. Refresh the source list first.");
        return;
    }
    infos = gsw_install_manager_get_remote_module_infos(
        priv->installer, source, &error);
    if (infos == NULL) {
        gsword_window_show_error(self, "Cannot read remote modules", error);
        return;
    }
    query = gtk_editable_get_text(GTK_EDITABLE(priv->install_search));
    if (query != NULL && *query != '\0')
        folded_query = g_utf8_casefold(query, -1);

    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);
        const gchar *name = gsw_module_info_get_name(info);
        const gchar *description = gsw_module_info_get_description(info);
        guint status = gsw_install_manager_get_module_status(priv->manager,
                                                             info);
        GtkWidget *row;
        GtkWidget *label;
        gchar *text;
        gchar *folded_name = NULL;
        gchar *folded_description = NULL;

        if (folded_query != NULL) {
            folded_name = g_utf8_casefold(name, -1);
            folded_description = g_utf8_casefold(
                description != NULL ? description : "", -1);
            if (strstr(folded_name, folded_query) == NULL &&
                strstr(folded_description, folded_query) == NULL) {
                g_free(folded_name);
                g_free(folded_description);
                continue;
            }
            g_free(folded_name);
            g_free(folded_description);
        }
        text = g_strdup_printf("%s\n%s", name, module_status_text(status));
        row = gtk_list_box_row_new();
        label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "remote-name", g_strdup(name),
                               g_free);
        gtk_list_box_append(priv->remote_list, row);
        g_free(text);
    }
    g_free(folded_query);
    g_ptr_array_unref(infos);
}

static void
on_remote_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *name;

    (void)list;
    if (row == NULL)
        return;
    name = g_object_get_data(G_OBJECT(row), "remote-name");
    if (name == NULL)
        return;
    g_free(priv->selected_remote_name);
    priv->selected_remote_name = g_strdup(name);
    gsword_window_update_remote_details(self, name);
}

static void
on_refresh_sources_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    (void)button;

    if (priv->installer == NULL)
        return;
    gtk_widget_set_sensitive(GTK_WIDGET(priv->refresh_sources_button), FALSE);
    gsword_window_set_status(self, "Refreshing install sources...");
    gsw_install_manager_refresh_remote_source_configuration_async(
        priv->installer, NULL, on_refresh_sources_finished, self);
}

static void
on_refresh_sources_finished(GObject *source, GAsyncResult *result,
                            gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;

    if (!gsw_install_manager_refresh_remote_source_configuration_finish(
            GSW_INSTALL_MANAGER(source), result, &error)) {
        gsword_window_show_error(self, "Cannot refresh install sources", error);
    } else {
        gsword_window_set_status(self, "Install sources refreshed");
    }
    gtk_widget_set_sensitive(GTK_WIDGET(priv->refresh_sources_button), TRUE);
    gsword_window_refresh_install_sources(self);
}

static void
on_refresh_remote_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswInstallSource *install_source = gsword_window_current_install_source(self);

    (void)button;
    if (install_source == NULL)
        return;
    g_clear_object(&priv->install_cancellable);
    priv->install_cancellable = g_cancellable_new();
    gsw_install_manager_set_cancellable(priv->installer,
                                        priv->install_cancellable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->refresh_source_button), FALSE);
    gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), TRUE);
    gtk_progress_bar_pulse(priv->install_progress);
    gsword_window_set_status(self, "Refreshing remote module list...");
    gsw_install_manager_refresh_remote_source_async(
        priv->installer, install_source, priv->install_cancellable,
        on_refresh_remote_finished, self);
}

static gboolean
clear_install_progress_cb(gpointer data)
{
    GswordWindow *self = GSWORD_WINDOW(data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->install_cancellable == NULL) {
        gtk_progress_bar_set_fraction(priv->install_progress, 0.0);
        gtk_label_set_text(priv->install_status, "");
    }
    return G_SOURCE_REMOVE;
}

static void
gsword_window_clear_install_progress_later(GswordWindow *self)
{
    /* Progress events are queued on the main context from the worker thread.
     * Clear after those events have been processed so a final progress update
     * cannot leave a stale "Downloading..." label after the operation ends. */
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, clear_install_progress_cb,
                    g_object_ref(self), g_object_unref);
}

static void
on_refresh_remote_finished(GObject *source, GAsyncResult *result,
                           gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;

    if (!gsw_install_manager_refresh_remote_source_finish(
            GSW_INSTALL_MANAGER(source), result, &error)) {
        gsword_window_show_error(self, "Cannot refresh remote modules", error);
    } else {
        gsword_window_set_status(self, "Remote module list refreshed");
    }
    gtk_widget_set_sensitive(GTK_WIDGET(priv->refresh_source_button), TRUE);
    gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), FALSE);
    gtk_progress_bar_set_fraction(priv->install_progress, 0.0);
    g_clear_object(&priv->install_cancellable);
    gsword_window_refresh_remote_modules(self);
    gsword_window_clear_install_progress_later(self);
}

static void
on_install_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswInstallSource *source = gsword_window_current_install_source(self);

    (void)button;
    if (source == NULL || priv->selected_remote_name == NULL)
        return;
    g_clear_object(&priv->install_cancellable);
    priv->install_cancellable = g_cancellable_new();
    gsw_install_manager_set_cancellable(priv->installer,
                                        priv->install_cancellable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->install_button), FALSE);
    gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), TRUE);
    gsword_window_set_status(self, "Installing module...");
    gsw_install_manager_install_module_async(
        priv->installer, priv->manager, source, priv->selected_remote_name,
        priv->install_cancellable, on_install_finished, self);
}

static void
on_install_finished(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;
    gboolean installed;
    gchar *installed_name = g_strdup(priv->selected_remote_name);

    installed = gsw_install_manager_install_module_finish(
        GSW_INSTALL_MANAGER(source), result, &error);
    if (!installed) {
        gsword_window_show_error(self, "Module install failed", error);
    }
    gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), FALSE);
    gtk_progress_bar_set_fraction(priv->install_progress, 0.0);
    g_clear_object(&priv->install_cancellable);
    if (priv->manager != NULL)
        gsw_manager_reload(priv->manager, NULL);
    if (installed && installed_name != NULL && priv->settings != NULL) {
        /* Make the newly installed module visible and selected instead of
         * silently keeping the previously selected module. */
        g_settings_set_string(priv->settings, "last-module", installed_name);
        g_settings_set_string(priv->settings, "last-key", "");
    }
    gsword_window_populate_modules(self);
    gsword_window_refresh_installed_dropdown(self);
    gsword_window_refresh_remote_modules(self);
    gtk_stack_set_visible_child_name(priv->content_stack, "library");
    if (installed && installed_name != NULL) {
        gchar *message = g_strdup_printf("Installed module %s",
                                         installed_name);
        gsword_window_set_status(self, message);
        g_free(message);
    }
    g_free(installed_name);
    gsword_window_clear_install_progress_later(self);
}

static void
on_remove_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint selected = gtk_drop_down_get_selected(priv->installed_dropdown);
    const gchar *name;

    (void)button;
    if (priv->installer == NULL || priv->installed_names == NULL ||
        selected == GTK_INVALID_LIST_POSITION)
        return;
    name = gtk_string_list_get_string(priv->installed_names, selected);
    if (name == NULL)
        return;
    g_clear_object(&priv->install_cancellable);
    priv->install_cancellable = g_cancellable_new();
    gsw_install_manager_set_cancellable(priv->installer,
                                        priv->install_cancellable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->remove_button), FALSE);
    gsword_window_set_status(self, "Removing module...");
    gsw_install_manager_remove_module_async(
        priv->installer, priv->manager, name, priv->install_cancellable,
        on_remove_finished, self);
}

static void
on_remove_finished(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;

    if (!gsw_install_manager_remove_module_finish(
            GSW_INSTALL_MANAGER(source), result, &error)) {
        gsword_window_show_error(self, "Module removal failed", error);
    } else {
        gsword_window_set_status(self, "Module removed");
    }
    g_clear_object(&priv->install_cancellable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->remove_button), TRUE);
    if (priv->manager != NULL)
        gsw_manager_reload(priv->manager, NULL);
    gsword_window_populate_modules(self);
    gsword_window_refresh_installed_dropdown(self);
    gsword_window_refresh_remote_modules(self);
    gtk_stack_set_visible_child_name(priv->content_stack, "library");
    gsword_window_clear_install_progress_later(self);
}

static void
on_cancel_install_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    (void)button;

    if (priv->install_cancellable != NULL)
        g_cancellable_cancel(priv->install_cancellable);
    gsword_window_set_status(self, "Cancelling...");
}

static void
on_install_pre_status(GswStatusReporter *reporter, guint64 total, guint64 done,
                      const gchar *message, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    (void)reporter;

    gtk_progress_bar_set_fraction(priv->install_progress,
                                  total > 0 ? (gdouble)done / total : 0.0);
    gtk_label_set_text(priv->install_status,
                       message != NULL ? message : "Installing...");
    (void)self;
}

static void
on_install_progress(GswStatusReporter *reporter, guint64 total, guint64 done,
                    gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gchar *status;
    (void)reporter;

    if (total > 0) {
        gtk_progress_bar_set_fraction(priv->install_progress,
                                      (gdouble)done / total);
        status = g_strdup_printf("Downloading: %" G_GUINT64_FORMAT
                                 " / %" G_GUINT64_FORMAT " bytes",
                                 done, total);
    } else {
        gtk_progress_bar_pulse(priv->install_progress);
        status = g_strdup_printf("Downloading: %" G_GUINT64_FORMAT " bytes",
                                 done);
    }
    gtk_label_set_text(priv->install_status, status);
    g_free(status);
    (void)self;
}

static void
on_local_install_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *directory;
    const gchar *name;
    GError *error = NULL;

    (void)button;
    if (priv->installer == NULL || priv->manager == NULL)
        return;
    directory = gtk_editable_get_text(GTK_EDITABLE(priv->local_dir_entry));
    name = gtk_editable_get_text(GTK_EDITABLE(priv->local_name_entry));
    if (directory == NULL || *directory == '\0' ||
        name == NULL || *name == '\0') {
        gsword_window_set_status(self,
            "Enter a local SWORD directory and module name");
        return;
    }
    if (!gsw_install_manager_install_module_from_directory(
            priv->installer, priv->manager, directory, name, &error)) {
        gsword_window_show_error(self, "Local install failed", error);
        return;
    }
    gsw_manager_reload(priv->manager, NULL);
    if (priv->settings != NULL) {
        g_settings_set_string(priv->settings, "last-module", name);
        g_settings_set_string(priv->settings, "last-key", "");
    }
    gsword_window_populate_modules(self);
    gsword_window_refresh_installed_dropdown(self);
    gsword_window_refresh_remote_modules(self);
    gtk_stack_set_visible_child_name(priv->content_stack, "library");
    gsword_window_set_status(self, "Module installed from local directory");
}

static void
on_remote_search_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    gsword_window_refresh_remote_modules(GSWORD_WINDOW(user_data));
}

static void
on_source_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    gsword_window_refresh_remote_modules(GSWORD_WINDOW(user_data));
}

static GtkWidget *
gsword_window_make_library_page(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *source_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *progress_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *left_scroll = gtk_scrolled_window_new();
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *details_scroll = gtk_scrolled_window_new();
    GtkWidget *refresh_sources = gtk_button_new_from_icon_name(
        "view-refresh-symbolic");
    GtkWidget *refresh_remote = gtk_button_new_from_icon_name(
        "network-transmit-receive-symbolic");
    GtkWidget *install = gtk_button_new_with_label("Install / Update");
    GtkWidget *remove = gtk_button_new_with_label("Remove");
    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    GtkWidget *installed_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *local_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *local_label = gtk_label_new("Install from local directory");

    gtk_widget_set_margin_top(controls, 8);
    gtk_widget_set_margin_bottom(controls, 8);
    gtk_widget_set_margin_start(controls, 12);
    gtk_widget_set_margin_end(controls, 12);

    priv->source_dropdown = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    priv->install_search = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->install_search, "Filter modules");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->install_search), TRUE);
    gtk_widget_set_tooltip_text(refresh_sources,
                                "Refresh the install source list");
    gtk_widget_set_tooltip_text(refresh_remote,
                                "Refresh modules from the selected source");
    g_signal_connect(refresh_sources, "clicked",
                     G_CALLBACK(on_refresh_sources_clicked), self);
    g_signal_connect(refresh_remote, "clicked",
                     G_CALLBACK(on_refresh_remote_clicked), self);
    g_signal_connect(priv->source_dropdown, "notify::selected",
                     G_CALLBACK(on_source_changed), self);
    g_signal_connect(priv->install_search, "changed",
                     G_CALLBACK(on_remote_search_changed), self);

    gtk_box_append(GTK_BOX(source_row), GTK_WIDGET(priv->source_dropdown));
    gtk_box_append(GTK_BOX(source_row), refresh_sources);
    gtk_box_append(GTK_BOX(source_row), refresh_remote);
    gtk_box_append(GTK_BOX(source_row), GTK_WIDGET(priv->install_search));

    priv->install_progress = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_progress_bar_set_show_text(priv->install_progress, FALSE);
    gtk_widget_set_hexpand(GTK_WIDGET(priv->install_progress), TRUE);
    priv->install_status = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->install_status, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->install_status), "dim-label");
    gtk_widget_set_visible(cancel, FALSE);
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel_install_clicked),
                     self);
    gtk_box_append(GTK_BOX(progress_row), GTK_WIDGET(priv->install_progress));
    gtk_box_append(GTK_BOX(progress_row), GTK_WIDGET(priv->install_status));
    gtk_box_append(GTK_BOX(progress_row), cancel);
    priv->cancel_button = GTK_BUTTON(cancel);
    priv->refresh_sources_button = GTK_BUTTON(refresh_sources);
    priv->refresh_source_button = GTK_BUTTON(refresh_remote);
    gtk_box_append(GTK_BOX(controls), source_row);
    gtk_box_append(GTK_BOX(controls), progress_row);

    priv->remote_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(priv->remote_list, "row-selected",
                     G_CALLBACK(on_remote_selected), self);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(left_scroll),
                                  GTK_WIDGET(priv->remote_list));
    gtk_widget_set_size_request(left_scroll, 300, -1);

    priv->remote_title = GTK_LABEL(gtk_label_new("Select a module"));
    gtk_label_set_xalign(priv->remote_title, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->remote_title), "title-2");
    priv->remote_description = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->remote_description, 0.0f);
    gtk_label_set_wrap(priv->remote_description, TRUE);
    priv->remote_status = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->remote_status, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->remote_status), "dim-label");
    gtk_box_append(GTK_BOX(right), GTK_WIDGET(priv->remote_title));
    gtk_box_append(GTK_BOX(right), GTK_WIDGET(priv->remote_status));
    gtk_box_append(GTK_BOX(right), GTK_WIDGET(priv->remote_description));
    gtk_widget_set_vexpand(details_scroll, TRUE);
    gtk_box_append(GTK_BOX(right), details_scroll);
    gtk_box_append(GTK_BOX(right), install);
    priv->install_button = GTK_BUTTON(install);
    gtk_widget_set_sensitive(install, FALSE);
    g_signal_connect(install, "clicked", G_CALLBACK(on_install_clicked), self);

    priv->installed_dropdown = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    gtk_widget_set_hexpand(GTK_WIDGET(priv->installed_dropdown), TRUE);
    g_signal_connect(remove, "clicked", G_CALLBACK(on_remove_clicked), self);
    priv->remove_button = GTK_BUTTON(remove);
    gtk_box_append(GTK_BOX(installed_row), gtk_label_new("Installed:"));
    gtk_box_append(GTK_BOX(installed_row), GTK_WIDGET(priv->installed_dropdown));
    gtk_box_append(GTK_BOX(installed_row), remove);
    gtk_box_append(GTK_BOX(right), installed_row);

    gtk_label_set_xalign(GTK_LABEL(local_label), 0.0f);
    gtk_widget_add_css_class(local_label, "heading");
    priv->local_dir_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->local_dir_entry,
                                   "Local SWORD library directory");
    priv->local_name_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->local_name_entry, "Module name");
    priv->local_install_button = GTK_BUTTON(
        gtk_button_new_with_label("Install local module"));
    g_signal_connect(priv->local_install_button, "clicked",
                     G_CALLBACK(on_local_install_clicked), self);
    gtk_box_append(GTK_BOX(local_box), local_label);
    gtk_box_append(GTK_BOX(local_box), GTK_WIDGET(priv->local_dir_entry));
    gtk_box_append(GTK_BOX(local_box), GTK_WIDGET(priv->local_name_entry));
    gtk_box_append(GTK_BOX(local_box),
                   GTK_WIDGET(priv->local_install_button));
    gtk_box_append(GTK_BOX(right), local_box);

    gtk_widget_set_vexpand(paned, TRUE);
    gtk_paned_set_start_child(GTK_PANED(paned), left_scroll);
    gtk_paned_set_end_child(GTK_PANED(paned), right);
    gtk_paned_set_position(GTK_PANED(paned), 360);
    gtk_box_append(GTK_BOX(box), controls);
    gtk_box_append(GTK_BOX(box), paned);

    return box;
}

static void
gsword_window_build(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    AdwToolbarView *toolbar_view;
    AdwHeaderBar *header;
    AdwNavigationSplitView *split;
    AdwNavigationPage *sidebar_page;
    AdwNavigationPage *content_page;
    GtkWidget *workspace_box;
    GtkWidget *switcher_bar;
    GtkWidget *switcher;
    GtkWidget *menu_button;
    GtkWidget *header_title;
    GMenu *menu;
    GSimpleAction *preferences;
    GSimpleAction *focus_key;
    GtkWidget *status;

    priv->css = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(priv->css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_window_set_title(GTK_WINDOW(self), "gsword");
    gtk_window_set_default_size(GTK_WINDOW(self), 1100, 760);

    toolbar_view = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
    header = ADW_HEADER_BAR(adw_header_bar_new());
    header_title = adw_window_title_new("gsword", "SWORD study");
    adw_header_bar_set_title_widget(header, header_title);

    menu = g_menu_new();
    g_menu_append(menu, "Preferences", "win.preferences");
    g_menu_append(menu, "Quit", "app.quit");
    menu_button = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button),
                                  "open-menu-symbolic");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button),
                                   G_MENU_MODEL(menu));
    gtk_widget_set_tooltip_text(menu_button, "Main menu");
    adw_header_bar_pack_end(header, menu_button);
    g_object_unref(menu);

    split = ADW_NAVIGATION_SPLIT_VIEW(adw_navigation_split_view_new());
    adw_navigation_split_view_set_sidebar_width_fraction(split, 0.24);
    sidebar_page = adw_navigation_page_new(gsword_window_make_sidebar(self),
                                           "Library");

    priv->content_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_reader(self),
                         "reader", "Reading");
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_parallel_page(self),
                         "parallel", "Parallel");
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_search_page(self),
                         "search", "Search");
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_notes_page(self),
                         "notes", "Notes");
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_plan_page(self),
                         "plan", "Plan");
    gtk_stack_add_named(priv->content_stack,
                        gsword_window_make_teaching_page(self),
                        "teaching");
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_library_page(self),
                         "library", "Library");
    gtk_stack_set_visible_child_name(priv->content_stack, "reader");
    g_signal_connect(priv->content_stack, "notify::visible-child",
                     G_CALLBACK(on_workspace_changed), self);

    workspace_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    switcher_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    switcher = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher),
                                  priv->content_stack);
    gtk_widget_set_margin_top(switcher, 4);
    gtk_widget_set_margin_bottom(switcher, 4);
    gtk_widget_set_halign(switcher, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(switcher_bar), switcher);
    gtk_widget_set_vexpand(GTK_WIDGET(priv->content_stack), TRUE);
    gtk_box_append(GTK_BOX(workspace_box), switcher_bar);
    gtk_box_append(GTK_BOX(workspace_box),
                   GTK_WIDGET(priv->content_stack));

    content_page = adw_navigation_page_new(workspace_box, "Workspace");
    adw_navigation_split_view_set_sidebar(split, sidebar_page);
    adw_navigation_split_view_set_content(split, content_page);

    adw_toolbar_view_add_top_bar(toolbar_view, GTK_WIDGET(header));
    adw_toolbar_view_set_content(toolbar_view, GTK_WIDGET(split));

    status = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(status), 0.0f);
    gtk_widget_add_css_class(status, "caption");
    gtk_widget_add_css_class(status, "dim-label");
    gtk_widget_set_margin_top(status, 4);
    gtk_widget_set_margin_bottom(status, 4);
    gtk_widget_set_margin_start(status, 10);
    gtk_widget_set_margin_end(status, 10);
    adw_toolbar_view_add_bottom_bar(toolbar_view, status);
    priv->status_label = GTK_LABEL(status);

    adw_application_window_set_content(ADW_APPLICATION_WINDOW(self),
                                       GTK_WIDGET(toolbar_view));

    preferences = g_simple_action_new("preferences", NULL);
    g_signal_connect(preferences, "activate",
                     G_CALLBACK(on_preferences_action), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(preferences));
    g_object_unref(preferences);

    focus_key = g_simple_action_new("focus-key", NULL);
    g_signal_connect(focus_key, "activate",
                     G_CALLBACK(on_focus_key_action), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(focus_key));
    g_object_unref(focus_key);

    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.preferences",
        (const gchar * const[]) { "<Control>comma", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.focus-key",
        (const gchar * const[]) { "<Control>l", NULL });
}

static void
gsword_window_load_settings(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    priv->font_scale = 1.0;
    priv->theme = g_strdup("system");
    if (priv->settings != NULL) {
        priv->font_scale = g_settings_get_double(priv->settings, "font-scale");
        if (priv->font_scale <= 0.0)
            priv->font_scale = 1.0;
        g_free(priv->theme);
        priv->theme = g_settings_get_string(priv->settings, "theme");
        if (priv->theme == NULL || *priv->theme == '\0') {
            g_free(priv->theme);
            priv->theme = g_strdup("system");
        }
    }
    gsword_window_apply_css(self);
    gsword_window_apply_theme(self);
}

static void
gsword_window_finalize(GObject *object)
{
    GswordWindow *self = GSWORD_WINDOW(object);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    g_clear_object(&priv->module);
    g_clear_object(&priv->settings);
    g_clear_object(&priv->preferences_dialog);
    g_clear_pointer(&priv->css, g_object_unref);
    g_clear_pointer(&priv->notes, gsword_notes_store_free);
    g_clear_pointer(&priv->plans, gsword_plan_store_free);
    g_clear_pointer(&priv->plan_items, g_ptr_array_unref);
    g_clear_object(&priv->plan_names);
    g_clear_object(&priv->devotion_names);
    g_clear_pointer(&priv->memory_items, g_ptr_array_unref);
    g_clear_pointer(&priv->memory_module, g_free);
    g_clear_pointer(&priv->memory_key, g_free);
    g_clear_pointer(&priv->memory_display, g_free);
    g_clear_pointer(&priv->memory_text, g_free);
    g_clear_pointer(&priv->teaching_module, g_free);
    g_clear_pointer(&priv->teaching_reference, g_free);
    g_clear_object(&priv->installer);
    g_clear_object(&priv->install_reporter);
    g_clear_pointer(&priv->install_sources, g_ptr_array_unref);
    g_clear_object(&priv->source_names);
    g_clear_object(&priv->installed_names);
    g_clear_object(&priv->install_cancellable);
    g_free(priv->selected_remote_name);
    g_free(priv->theme);
    g_free(priv->note_module);
    g_free(priv->note_storage_key);
    g_free(priv->note_display_key);

    G_OBJECT_CLASS(gsword_window_parent_class)->finalize(object);
}

static void
gsword_window_class_init(GswordWindowClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = gsword_window_finalize;
}

static void
gsword_window_init(GswordWindow *self)
{
    gsword_window_get_instance_private(self);
}

GtkWidget *
gsword_window_new(GtkApplication *application, GswManager *manager,
                  GSettings *settings)
{
    GswordWindow *self;
    GswordWindowPrivate *priv;
    GError *error = NULL;

    g_return_val_if_fail(GTK_IS_APPLICATION(application), NULL);
    self = g_object_new(GSWORD_TYPE_WINDOW, "application", application, NULL);
    priv = gsword_window_get_instance_private(self);
    priv->manager = manager;
    priv->settings = settings != NULL ? g_object_ref(settings) : NULL;
    priv->notes = gsword_notes_store_new(&error);
    if (priv->notes == NULL) {
        g_printerr("gsword: cannot open notes database: %s\n",
                   error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
    }
    priv->plans = gsword_plan_store_new(&error);
    if (priv->plans == NULL) {
        g_printerr("gsword: cannot open planner database: %s\n",
                   error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
    }
    {
        gchar *data_dir = gsword_app_data_dir();
        gchar *installmgr_dir = g_build_filename(data_dir, "installmgr", NULL);

        priv->installer = gsw_install_manager_new(installmgr_dir, &error);
        if (priv->installer == NULL) {
            g_printerr("gsword: cannot open install manager: %s\n",
                       error != NULL ? error->message : "unknown error");
            g_clear_error(&error);
        } else {
            priv->install_reporter = gsw_status_reporter_new();
            /* Install tasks run in a GTask worker thread. Dispatch progress
             * signals on GTK's main context so callbacks only touch widgets
             * from the UI thread. */
            gsw_status_reporter_set_main_context(priv->install_reporter,
                                                 g_main_context_default());
            g_signal_connect(priv->install_reporter, "pre-status",
                             G_CALLBACK(on_install_pre_status), self);
            g_signal_connect(priv->install_reporter, "progress",
                             G_CALLBACK(on_install_progress), self);
            gsw_install_manager_set_status_reporter(priv->installer,
                                                    priv->install_reporter);
            gsw_install_manager_set_user_disclaimer_confirmed(
                priv->installer, TRUE);
        }
        g_free(installmgr_dir);
        g_free(data_dir);
    }

    gsword_window_build(self);
    gsword_window_load_settings(self);
    gsword_window_populate_modules(self);
    gsword_window_refresh_notes(self);
    gsword_window_refresh_install_sources(self);
    gsword_window_refresh_installed_dropdown(self);
    gsword_window_restore_page(self);
    return GTK_WIDGET(self);
}
