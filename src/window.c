#include "window.h"
#include "notes-store.h"
#include "plan-store.h"
#include "app-paths.h"
#include "gsword-reference-chooser.h"
#include "window-private.h"

#include <glib/gi18n.h>

#include <string.h>

struct _GswordWindow {
    AdwApplicationWindow parent_instance;
};

typedef struct {
    gchar *path;
    gchar *name;
    gint depth;
} GswordResourceEntry;

typedef struct {
    GtkWidget *root;
    GtkWidget *header;
    GtkDropDown *module_selector;
    GtkStringList *module_names;
    GswordReferenceChooser *reference_chooser;
    gchar *versification;
    GtkToggleButton *follow;
    GtkScrolledWindow *scroll;
    GtkTextView *view;
    GtkTextTag *active_tag;
    GswModule *module;
    gchar *module_name;
    guint id;
    guint updating;
} GswordWorkspacePane;

typedef struct {
    GswManager *manager;
    GSettings *settings;
    GswModule *module;
    GtkListBox *module_list;
    GtkStack *content_stack;
    GtkStack *workspace_stack;
    GtkGrid *workspace_grid;
    GtkSpinButton *workspace_columns;
    GPtrArray *workspace_panes;
    gchar *workspace_reference;
    gboolean workspace_restoring;
    gboolean workspace_syncing;
    gboolean page_restored;
    gboolean startup_selecting;
    GtkTextView *reader;
    GswordReferenceChooser *reference_chooser;
    GtkLabel *status_label;
    AdwDialog *preferences_dialog;
    GtkCssProvider *css;
    gchar *theme;
    gdouble font_scale;

    GtkWidget *study_panel;
    GtkToggleButton *study_toggle;
    GtkCheckButton *study_follow_toggle;
    GtkNotebook *study_notebook;
    GtkLabel *commentary_context;
    gboolean commentary_follow;
    gchar *pinned_commentary_key;
    GtkTextView *commentary_view;
    GtkListBox *strongs_list;
    GtkTextView *strongs_view;
    GtkListBox *crossrefs_list;
    GtkTextView *crossref_view;
    GtkTextView *lexicon_view;
    GtkDropDown *lexicon_selector;
    GtkStringList *lexicon_names;
    GtkEntry *lexicon_entry;
    gchar *commentary_name;
    gchar *lexicon_name;
    GswModule *commentary_module;
    GswModule *lexicon_module;

    GtkCheckButton *workspace_sync_scroll_toggle;
    gboolean workspace_sync_scroll_enabled;
    gboolean workspace_syncing_scroll;

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
    GtkNotebook *plan_notebook;
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
    GDateTime *devotion_date;
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

    GswModule *resource_module;
    gchar *resource_data_dir;
    gboolean resource_map_mode;
    GtkLabel *resource_title;
    GtkLabel *resource_path;
    GtkListBox *resource_tree;
    GtkStack *resource_content;
    GtkTextView *resource_text;
    GtkPicture *resource_picture;

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
    GtkTextTag *active_verse_tag;
    GtkTextTag *range_tag;
    gint active_verse;
    gint range_start;
    gint range_end;
    gboolean range_active;
    gchar *note_module;
    gchar *note_storage_key;
    gchar *note_display_key;
    gint64 note_id;
    gint64 selected_list_id;
} GswordWindowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GswordWindow, gsword_window, ADW_TYPE_APPLICATION_WINDOW)

static void gsword_window_clear_list(GtkListBox *list);
static void gsword_window_update_index_label(GswordWindow *self);
static void gsword_window_open_note_for_current(GswordWindow *self);
static void gsword_window_refresh_notes(GswordWindow *self);
static void gsword_window_apply_active_verse(GswordWindow *self);
static void gsword_window_clear_range(GswordWindow *self);
static void gsword_window_apply_range(GswordWindow *self);
static void gsword_window_select_verse_range(GswordWindow *self, gint anchor,
                                             gint verse);
static void gsword_window_select_verse(GswordWindow *self, gint verse);
static void gsword_window_update_reference_status(GswordWindow *self);
static void gsword_window_apply_highlights(GswordWindow *self);
static void gsword_window_load_module(GswordWindow *self, const gchar *name);
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
static void gsword_window_refresh_lexicon_modules(GswordWindow *self);
static void gsword_window_open_resource(GswordWindow *self,
                                        const gchar *name, gboolean map);
static void gsword_window_workspace_save(GswordWindow *self);
static void gsword_window_workspace_restore(GswordWindow *self);
static void gsword_window_workspace_reflow(GswordWindow *self);
static void gsword_window_workspace_sync_from_reader(GswordWindow *self);
static void gsword_window_workspace_sync_reference(
    GswordWindow *self, const gchar *reference, GswordWorkspacePane *source);
static void gsword_window_apply_parallel_template(GswordWindow *self);

static void
gsword_window_set_text(GswordWindow *self, const gchar *text)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(priv->reader);

    gtk_text_buffer_set_text(buffer, text != NULL ? text : "", -1);
}

GswManager *
gsword_window_get_manager(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    return priv->manager;
}

GSettings *
gsword_window_get_settings(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    return priv->settings;
}

GswModule *
gsword_window_get_module(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    return priv->module;
}

GswordNotesStore *
gsword_window_get_notes_store(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    return priv->notes;
}

GswordPlanStore *
gsword_window_get_plan_store(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    return priv->plans;
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
        ".teaching-reader { font-size: 18pt; }\n"
        ".workspace-pane { border: 1px solid alpha(currentColor, 0.16);"
        " border-radius: 8px; }\n"
        ".workspace-pane.dragging { opacity: 0.55; }\n"
        ".workspace-pane-header { min-height: 34px; }", points);
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
    const gchar *commentary_key;
    GString *text;

    gsword_window_ensure_study_modules(self);
    if (priv->module == NULL)
        return;
    key_text = gsw_module_get_key_text(priv->module);
    commentary_key = key_text;
    if (!priv->commentary_follow && priv->pinned_commentary_key != NULL &&
        *priv->pinned_commentary_key != '\0')
        commentary_key = priv->pinned_commentary_key;

    if (priv->commentary_context != NULL) {
        gchar *reference = priv->commentary_follow
                         ? gsword_window_current_reference(self)
                         : g_strdup(commentary_key);
        gchar *title = priv->commentary_name != NULL
                     ? g_strdup_printf("%s — %s", priv->commentary_name,
                                       reference != NULL ? reference : "")
                     : g_strdup(_("Commentary"));

        gtk_label_set_text(priv->commentary_context, title);
        g_free(title);
        g_free(reference);
    }

    if (priv->commentary_module != NULL) {
        gchar *commentary = NULL;

        if (gsw_module_set_key_text(priv->commentary_module, commentary_key,
                                    &error)) {
            commentary = gsw_module_get_text(priv->commentary_module, &error);
        }
        gsword_window_set_view_text(priv->commentary_view,
                                    commentary != NULL
                                    ? commentary : _("No commentary for this verse."));
        g_free(commentary);
        g_clear_error(&error);
    } else {
        gsword_window_set_view_text(priv->commentary_view,
                                    _("No commentary module is installed."));
    }

    gsword_window_clear_list(priv->strongs_list);
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
                    GtkWidget *row = gtk_list_box_row_new();
                    GtkWidget *label = gtk_label_new(key);

                    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
                    gtk_widget_set_margin_top(label, 2);
                    gtk_widget_set_margin_bottom(label, 2);
                    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
                    g_object_set_data_full(G_OBJECT(row), "strongs-key",
                                           g_strdup(key), g_free);
                    gtk_list_box_append(priv->strongs_list, row);
                    g_string_append_printf(text, "%s: %s\n", key, value);
                    g_free(value);
                }
            }
            g_ptr_array_unref(keys);
        }
        g_clear_error(&error);
    }
    if (text->len == 0)
        g_string_append(text, _("No Strong's data for this verse."));
    gsword_window_set_view_text(priv->strongs_view, text->str);
    g_string_free(text, TRUE);

    gsword_window_clear_list(priv->crossrefs_list);
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
                    gchar **parts = g_strsplit(value, ";", -1);
                    guint j;

                    for (j = 0; parts[j] != NULL; j++) {
                        gchar *reference = g_strstrip(parts[j]);

                        if (*reference != '\0') {
                            GtkWidget *row = gtk_list_box_row_new();
                            GtkWidget *label = gtk_label_new(reference);

                            gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
                            gtk_label_set_ellipsize(GTK_LABEL(label),
                                                     PANGO_ELLIPSIZE_END);
                            gtk_widget_set_margin_top(label, 2);
                            gtk_widget_set_margin_bottom(label, 2);
                            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row),
                                                       label);
                            g_object_set_data_full(G_OBJECT(row),
                                                   "crossref-key",
                                                   g_strdup(reference), g_free);
                            gtk_list_box_append(priv->crossrefs_list, row);
                        }
                    }
                    g_strfreev(parts);
                    g_string_append_printf(text, "%s\n", value);
                    g_free(value);
                }
            }
            g_ptr_array_unref(keys);
        }
        g_clear_error(&error);
    }
    if (text->len == 0)
        g_string_append(text, _("No cross-references for this verse."));
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
                                    _("No lexicon module is installed."));
        return;
    }
    key = gtk_editable_get_text(GTK_EDITABLE(priv->lexicon_entry));
    if (!gsw_module_set_key_text(priv->lexicon_module, key, &error)) {
        gsword_window_show_error(self, _("Cannot look up lexicon entry"), error);
        return;
    }
    text = gsw_module_get_text(priv->lexicon_module, &error);
    if (text == NULL) {
        gsword_window_show_error(self, _("Cannot read lexicon entry"), error);
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
    gsword_window_show_page(self, "notes");
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
        gsword_window_show_error(self, _("Cannot save note"), error);
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
    gsword_window_set_status(self, _("Note saved"));
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
        gsword_window_show_error(self, _("Cannot delete note"), error);
        return;
    }
    priv->note_id = 0;
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(priv->note_text), "", -1);
    gtk_editable_set_text(GTK_EDITABLE(priv->note_tags_entry), "");
    gsword_window_refresh_notes(self);
    gsword_window_set_status(self, _("Note deleted"));
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
    gtk_file_dialog_set_title(dialog, _("Export note"));
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
        gsword_window_show_page(self, "reader");
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
        gsword_window_show_error(self, _("Cannot create list"), error);
        return;
    }
    gtk_editable_set_text(GTK_EDITABLE(priv->new_list_entry), "");
    gsword_window_refresh_notes(self);
    gsword_window_set_status(self, _("Verse list created"));
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
        gsword_window_set_status(self, _("Select a verse list first"));
        return;
    }
    if (!gsword_notes_store_add_to_list(
            priv->notes, priv->selected_list_id, priv->note_module,
            priv->note_storage_key, priv->note_display_key, &error)) {
        gsword_window_show_error(self, _("Cannot add verse to list"), error);
        return;
    }
    gsword_window_refresh_notes(self);
    gsword_window_set_status(self, _("Verse added to list"));
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
    GtkWidget *new_list_button = gtk_button_new_with_label(_("Add list"));
    GtkWidget *notes_label = gtk_label_new(_("Notes"));
    GtkWidget *tags_label = gtk_label_new(_("Tags"));
    GtkWidget *lists_label = gtk_label_new(_("Verse lists"));
    GtkWidget *list_items_label = gtk_label_new(_("List verses"));
    GtkWidget *editor_scroll = gtk_scrolled_window_new();
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *save = gtk_button_new_with_label(_("Save"));
    GtkWidget *delete = gtk_button_new_with_label(_("Delete"));
    GtkWidget *export = gtk_button_new_with_label(_("Export"));
    GtkWidget *add_to_list = gtk_button_new_with_label(_("Add to list"));
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
    gtk_entry_set_placeholder_text(priv->note_search, _("Search notes"));
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
    gtk_entry_set_placeholder_text(priv->new_list_entry, _("New list"));
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
    gtk_entry_set_placeholder_text(priv->note_key_entry, _("Verse key"));
    priv->note_tags_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->note_tags_entry,
                                   _("Tags, separated by commas"));
    gtk_widget_set_hexpand(GTK_WIDGET(priv->note_tags_entry), TRUE);
    gtk_box_append(GTK_BOX(tags_row), gtk_label_new(_("Tags:")));
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
    gsword_window_set_status(self, _("No Bible text module is installed"));
    return NULL;
}

gboolean
gsword_window_open_reference(GswordWindow *self, const gchar *reference)
{
    if (reference == NULL || *reference == '\0' ||
        gsword_window_ensure_verse_module(self) == NULL)
        return FALSE;
    if (!gsword_window_set_key_text(self, reference))
        return FALSE;
    gsword_window_show_page(self, "reader");
    return TRUE;
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
                           _("Create a reading plan to get started."));
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
            _("%s  ·  %d/%d days (%.1f%%)  ·  streak %d"),
            plan->name, plan->completed_days, plan->total_days, percent,
            plan->streak);

        gtk_label_set_text(priv->plan_summary, summary);
        g_free(summary);
    }
    if (plan->next_day > 0) {
        gchar *today = g_strdup_printf(_("Day %d: %s"), plan->next_day,
                                       plan->next_reference != NULL
                                       ? plan->next_reference : "");
        gtk_label_set_text(priv->plan_today, today);
        g_free(today);
        gtk_widget_set_sensitive(GTK_WIDGET(priv->plan_open_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(priv->plan_complete_button),
                                 TRUE);
    } else {
        gtk_label_set_text(priv->plan_today, _("Plan complete"));
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

            text = g_strdup_printf(_("Day %d   %s"), day->day,
                                   day->reference != NULL ? day->reference : "");

            label = gtk_label_new(text);
            gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
            gtk_widget_set_hexpand(label, TRUE);
            check = gtk_check_button_new_with_label(_("Done"));
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
        gsword_window_show_error(self, _("Cannot update plan"), error);
        return;
    }
    gsword_window_set_status(self, _("Reading plan updated"));
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
        gsword_window_show_error(self, _("Cannot update plan"), error);
        return;
    }
    gsword_window_set_status(self, _("Day marked complete"));
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
        gsword_window_set_status(self, _("Enter a plan name and start reference"));
        return;
    }
    references = gsword_window_generate_plan_references(start, days, chapters);
    if (references->len == 0) {
        gsword_window_show_error(self, _("Cannot create plan"),
            g_error_new_literal(G_FILE_ERROR, G_FILE_ERROR_INVAL,
                                _("Invalid start reference or empty plan")));
        g_ptr_array_unref(references);
        return;
    }
    plan_id = gsword_plan_store_create_plan(priv->plans, name, start, chapters,
                                            references, &error);
    g_ptr_array_unref(references);
    if (plan_id == 0) {
        gsword_window_show_error(self, _("Cannot create plan"), error);
        return;
    }
    id = g_strdup_printf("%" G_GINT64_FORMAT, plan_id);
    if (priv->settings != NULL)
        g_settings_set_string(priv->settings, "active-plan-id", id);
    g_free(id);
    gsword_window_refresh_plans(self);
    gsword_window_set_status(self, _("Reading plan created"));
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
        gsword_window_show_error(self, _("Cannot delete plan"), error);
        return;
    }
    if (priv->settings != NULL)
        g_settings_set_string(priv->settings, "active-plan-id", "");
    priv->selected_plan_id = 0;
    gsword_window_refresh_plans(self);
    gsword_window_set_status(self, _("Reading plan deleted"));
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
    dialog = adw_alert_dialog_new(_("Delete reading plan?"), NULL);
    adw_alert_dialog_format_body(ADW_ALERT_DIALOG(dialog),
        _("Delete “%s” and its reading progress? This cannot be undone."),
        plan->name);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "cancel", _("Cancel"),
                                   "delete", _("Delete"),
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

static gboolean
module_info_is_map(GswModuleInfo *info)
{
    const gchar *category;

    if (gsw_module_info_get_module_type(info) == GSW_MODULE_TYPE_MAPS)
        return TRUE;
    category = gsw_module_info_get_value(info, "Category");
    return category != NULL &&
           g_ascii_strncasecmp(category, "Maps", 4) == 0;
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

    if (priv->devotion_date == NULL)
        priv->devotion_date = g_date_time_new_now_local();
    now = g_date_time_ref(priv->devotion_date);
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
            _("No devotional entry is available for today in this module."));
    } else {
        gsword_window_set_view_text(priv->devotion_view, text);
        g_free(text);
    }
    title = g_strdup_printf(_("Daily Devotion · %s · %s"), name,
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
        gtk_label_set_text(priv->devotion_title, _("Daily Devotion"));
        gsword_window_set_view_text(
            priv->devotion_view,
            _("No daily devotional module is installed.\n\n"
              "Install a module such as Daily Light or Morning and Evening "
              "from the Library page."));
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

static void
on_devotion_shift(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gint direction = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button),
                                                       "direction"));
    GDateTime *date;

    if (priv->devotion_date == NULL)
        priv->devotion_date = g_date_time_new_now_local();
    if (direction == 0) {
        g_date_time_unref(priv->devotion_date);
        priv->devotion_date = g_date_time_new_now_local();
    } else {
        date = g_date_time_add_days(priv->devotion_date, direction);
        g_date_time_unref(priv->devotion_date);
        priv->devotion_date = date;
    }
    gsword_window_load_devotion(self);
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
    gtk_label_set_text(priv->memory_title, _("Select a verse card"));
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
        due = g_strdup_printf(_("Due %s  ·  box %d"), verse->due, verse->box);
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
        gchar *text = g_strdup_printf(_("%s\nDue %s  ·  box %d"),
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
        gsword_window_show_error(self, _("Cannot add memory verse"), error);
        g_free(storage_key);
        g_free(display_key);
        g_free(text);
        return;
    }
    if (!gsword_plan_store_add_memory_verse(
            priv->plans, gsw_module_get_name(module), storage_key,
            display_key, text, &error)) {
        gsword_window_show_error(self, _("Cannot save memory verse"), error);
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
    gsword_window_set_status(self, _("Verse added to memory review"));
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
        gsword_window_show_error(self, _("Cannot schedule review"), error);
        return;
    }
    gsword_window_refresh_memory(self);
    gsword_window_set_status(self, remembered
                             ? _("Review scheduled: nice work")
                             : _("Card will be shown again today"));
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
        gtk_label_set_text(priv->memory_result, _("Correct"));
        gsword_window_set_status(self, _("Memory verse recited correctly"));
    } else {
        gtk_label_set_text(priv->memory_result, _("Keep practicing"));
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
        gsword_window_show_error(self, _("Cannot delete memory verse"), error);
        return;
    }
    priv->memory_id = 0;
    gsword_window_refresh_memory(self);
    gsword_window_set_status(self, _("Memory verse removed"));
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
        gsword_window_show_error(self, _("Cannot read teaching passage"), error);
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
    gsword_window_show_page(self, "teaching");
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
        gsword_window_show_error(self, _("Cannot save teaching notes"), error);
        g_free(notes);
        return;
    }
    g_free(notes);
    gsword_window_set_status(self, _("Teaching notes saved"));
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
                         ? _("Exit fullscreen") : _("Fullscreen"));
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
        gtk_button_set_label(priv->teaching_fullscreen_button, _("Fullscreen"));
    }
    gsword_window_show_page(self, "reader");
}

static void
on_teaching_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);

    (void)button;
    gsword_window_teaching_load(self);
    gsword_window_show_page(self, "teaching");
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
    GtkWidget *open = gtk_button_new_with_label(_("Open today's reading"));
    GtkWidget *complete = gtk_button_new_with_label(_("Mark today complete"));
    GtkWidget *create = gtk_button_new_with_label(_("Create plan"));
    GtkWidget *delete_plan = gtk_button_new_with_label(_("Delete plan"));
    GtkWidget *add_current = gtk_button_new_with_label(_("Add current verse"));
    GtkWidget *delete_verse = gtk_button_new_with_label(_("Delete"));
    GtkWidget *check = gtk_button_new_with_label(_("Check answer"));
    GtkWidget *again = gtk_button_new_with_label(_("Again"));
    GtkWidget *good = gtk_button_new_with_label(_("Good"));
    GtkWidget *refresh_devotion = gtk_button_new_with_label(_("Refresh"));
    GtkWidget *previous_day = gtk_button_new_from_icon_name(
        "go-previous-symbolic");
    GtkWidget *next_day = gtk_button_new_from_icon_name("go-next-symbolic");
    GtkWidget *today_button = gtk_button_new_with_label(_("Today"));
    GtkStringList *modes = gtk_string_list_new(
        (const gchar * const[]) { _("Full text"), _("First letters"), _("Hide words"),
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
        GtkWidget *label = gtk_label_new(_("Schedule"));
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
    gtk_entry_set_placeholder_text(priv->plan_name_entry, _("Plan name"));
    priv->plan_start_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->plan_start_entry, _("Start, e.g. Genesis 1"));
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
    gtk_box_append(GTK_BOX(plan_form_row1), gtk_label_new(_("Days")));
    gtk_box_append(GTK_BOX(plan_form_row1), GTK_WIDGET(priv->plan_days_spin));
    gtk_box_append(GTK_BOX(plan_form_row1), gtk_label_new(_("Chapters/day")));
    gtk_box_append(GTK_BOX(plan_form_row1), GTK_WIDGET(priv->plan_chapters_spin));
    gtk_box_append(GTK_BOX(plan_form_row2), create);
    gtk_box_append(GTK_BOX(plan_form_row2), delete_plan);
    gtk_box_append(GTK_BOX(plan_form), plan_form_row1);
    gtk_box_append(GTK_BOX(plan_form), plan_form_row2);
    {
        GtkWidget *hint = gtk_label_new(
            _("The plan advances through chapters from the start reference. "
              "Reading days can be opened by double-clicking a schedule row."));
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
    g_object_set_data(G_OBJECT(previous_day), "direction", GINT_TO_POINTER(-1));
    g_object_set_data(G_OBJECT(next_day), "direction", GINT_TO_POINTER(1));
    g_object_set_data(G_OBJECT(today_button), "direction", GINT_TO_POINTER(0));
    g_signal_connect(previous_day, "clicked",
                     G_CALLBACK(on_devotion_shift), self);
    g_signal_connect(next_day, "clicked",
                     G_CALLBACK(on_devotion_shift), self);
    g_signal_connect(today_button, "clicked", G_CALLBACK(on_devotion_shift), self);
    gtk_widget_set_tooltip_text(previous_day, _("Previous day"));
    gtk_widget_set_tooltip_text(next_day, _("Next day"));
    priv->devotion_title = GTK_LABEL(gtk_label_new(_("Daily Devotion")));
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
    gtk_box_append(GTK_BOX(devotion_controls), previous_day);
    gtk_box_append(GTK_BOX(devotion_controls), today_button);
    gtk_box_append(GTK_BOX(devotion_controls), next_day);
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

    priv->memory_title = GTK_LABEL(gtk_label_new(_("Select a verse card")));
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
    gtk_box_append(GTK_BOX(memory_mode_row), gtk_label_new(_("Card mode")));
    gtk_box_append(GTK_BOX(memory_mode_row), GTK_WIDGET(priv->memory_mode));
    priv->memory_input = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->memory_input, _("Type the verse"));
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
                             gtk_label_new(_("Today")));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), plans,
                             gtk_label_new(_("Plans")));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), devotion,
                             gtk_label_new(_("Devotion")));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), memory,
                             gtk_label_new(_("Memorize")));
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    priv->plan_notebook = GTK_NOTEBOOK(notebook);
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
    GtkWidget *scripture_label = gtk_label_new(_("Scripture"));
    GtkWidget *notes_label = gtk_label_new(_("Teaching notes"));
    GtkWidget *load = gtk_button_new_with_label(_("Load current passage"));
    GtkWidget *save = gtk_button_new_with_label(_("Save notes"));
    GtkWidget *back = gtk_button_new_with_label(_("Back to reading"));
    GtkWidget *fullscreen = gtk_button_new_with_label(_("Fullscreen"));

    gtk_widget_set_margin_top(toolbar, 8);
    gtk_widget_set_margin_bottom(toolbar, 8);
    gtk_widget_set_margin_start(toolbar, 12);
    gtk_widget_set_margin_end(toolbar, 12);
    priv->teaching_ref_entry = GTK_ENTRY(gtk_entry_new());
    gtk_editable_set_editable(GTK_EDITABLE(priv->teaching_ref_entry), FALSE);
    gtk_widget_set_hexpand(GTK_WIDGET(priv->teaching_ref_entry), TRUE);
    gtk_entry_set_placeholder_text(priv->teaching_ref_entry, _("Current passage"));
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
gsword_resource_entry_free(gpointer data)
{
    GswordResourceEntry *entry = data;

    if (entry == NULL)
        return;
    g_free(entry->path);
    g_free(entry->name);
    g_free(entry);
}

static void
gsword_window_resource_collect(GswTreeKey *node, GPtrArray *entries,
                               gint depth)
{
    GswKey *cursor;

    if (!gsw_tree_key_has_children(node))
        return;
    cursor = gsw_key_clone((GswKey *)node);
    if (cursor == NULL)
        return;
    if (!gsw_tree_key_first_child((GswTreeKey *)cursor)) {
        gsw_key_unref(cursor);
        return;
    }
    do {
        GswordResourceEntry *entry = g_new0(GswordResourceEntry, 1);

        entry->path = g_strdup(gsw_tree_key_get_text((GswTreeKey *)cursor));
        entry->name = g_strdup(
            gsw_tree_key_get_local_name((GswTreeKey *)cursor));
        entry->depth = depth;
        g_ptr_array_add(entries, entry);
        if (gsw_tree_key_has_children((GswTreeKey *)cursor)) {
            GswKey *descendant = gsw_key_clone(cursor);

            if (descendant != NULL) {
                gsword_window_resource_collect((GswTreeKey *)descendant,
                                               entries, depth + 1);
                gsw_key_unref(descendant);
            }
        }
    } while (gsw_tree_key_next_sibling((GswTreeKey *)cursor));
    gsw_key_unref(cursor);
}

static void
gsword_window_resource_show_path(GswordWindow *self, const gchar *path)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;
    gchar *text;
    const gchar *raw;

    if (priv->resource_module == NULL || path == NULL)
        return;
    if (!gsw_module_set_key_text(priv->resource_module, path, &error)) {
        gsword_window_show_error(self, _("Cannot open resource entry"), error);
        return;
    }
    gtk_label_set_text(priv->resource_path, path);
    raw = gsw_module_peek_raw(priv->resource_module, NULL);
    if (priv->resource_map_mode && raw != NULL) {
        const gchar *image = strstr(raw, "<img");
        const gchar *src = image != NULL ? strstr(image, "src=\"") : NULL;

        if (src != NULL) {
            const gchar *end = strchr(src + 5, '"');

            if (end != NULL) {
                gchar *relative = g_strndup(src + 5, (gsize)(end - (src + 5)));
                gchar *file_path;
                GFile *file;

                while (g_str_has_prefix(relative, "/"))
                    memmove(relative, relative + 1, strlen(relative));
                file_path = g_build_filename(priv->resource_data_dir,
                                             relative, NULL);
                g_free(relative);
                if (g_file_test(file_path, G_FILE_TEST_IS_REGULAR)) {
                    file = g_file_new_for_path(file_path);
                    gtk_picture_set_file(priv->resource_picture, file);
                    g_object_unref(file);
                    gtk_stack_set_visible_child_name(priv->resource_content,
                                                     "picture");
                    g_free(file_path);
                    return;
                }
                g_free(file_path);
            }
        }
    }

    text = gsw_module_get_text(priv->resource_module, &error);
    if (text == NULL) {
        gsword_window_show_error(self, _("Cannot read resource entry"), error);
        return;
    }
    gsword_window_set_view_text(priv->resource_text, text);
    g_free(text);
    gtk_stack_set_visible_child_name(priv->resource_content, "text");
}

static void
on_resource_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    const gchar *path;

    (void)list;
    if (row == NULL)
        return;
    path = g_object_get_data(G_OBJECT(row), "resource-path");
    if (path != NULL)
        gsword_window_resource_show_path(GSWORD_WINDOW(user_data), path);
}

static void
gsword_window_resource_fill(GswordWindow *self, GPtrArray *entries)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkListBoxRow *first = NULL;
    guint i;

    gsword_window_clear_list(priv->resource_tree);
    for (i = 0; i < entries->len; i++) {
        GswordResourceEntry *entry = g_ptr_array_index(entries, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label;
        gchar *indent = g_strnfill((gsize)(entry->depth * 2), ' ');
        gchar *text = g_strconcat(indent, entry->name, NULL);

        label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "resource-path",
                               g_strdup(entry->path), g_free);
        gtk_list_box_append(priv->resource_tree, row);
        if (first == NULL)
            first = GTK_LIST_BOX_ROW(row);
        g_free(text);
        g_free(indent);
    }
    if (first != NULL)
        gtk_list_box_select_row(priv->resource_tree, first);
}

static void
gsword_window_open_resource(GswordWindow *self, const gchar *name,
                            gboolean map)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswModuleInfo *info;
    GPtrArray *entries;
    gchar *root_text;
    GError *error = NULL;

    if (priv->manager == NULL || name == NULL)
        return;
    info = gsw_manager_lookup_module_info(priv->manager, name);
    if (info == NULL) {
        gsword_window_set_status(self, _("Cannot open module"));
        return;
    }
    g_clear_object(&priv->resource_module);
    priv->resource_module = gsw_manager_create_module(priv->manager, name);
    if (priv->resource_module == NULL) {
        gsword_window_set_status(self, _("Cannot open module"));
        return;
    }
    g_free(priv->resource_data_dir);
    priv->resource_data_dir = g_strdup(
        gsw_module_info_get_absolute_data_path(info));
    priv->resource_map_mode = map;
    gtk_label_set_text(priv->resource_title, name);
    gtk_label_set_text(priv->resource_path, "");

    gsw_module_set_position(priv->resource_module, GSW_POSITION_TOP, NULL);
    entries = g_ptr_array_new_with_free_func(gsword_resource_entry_free);
    gsword_window_resource_collect(
        (GswTreeKey *)gsw_module_get_key(priv->resource_module), entries, 0);

    root_text = gsw_module_get_text(priv->resource_module, &error);
    if (root_text != NULL) {
        gsword_window_set_view_text(priv->resource_text, root_text);
        gtk_stack_set_visible_child_name(priv->resource_content, "text");
        g_free(root_text);
    }
    g_clear_error(&error);
    gsword_window_resource_fill(self, entries);
    g_ptr_array_unref(entries);
    gsword_window_show_page(self, "resource");
}

static void
on_resource_back(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);

    (void)button;
    gsword_window_show_page(self, "reader");
}

static GtkWidget *
gsword_window_make_resource_page(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *tree_scroll = gtk_scrolled_window_new();
    GtkWidget *content_scroll = gtk_scrolled_window_new();
    GtkWidget *picture_scroll = gtk_scrolled_window_new();
    GtkWidget *back = gtk_button_new_with_label(_("Back to reading"));
    GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_widget_set_margin_top(header, 8);
    gtk_widget_set_margin_bottom(header, 8);
    gtk_widget_set_margin_start(header, 12);
    gtk_widget_set_margin_end(header, 12);
    priv->resource_title = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->resource_title, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->resource_title), "heading");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->resource_title), TRUE);
    priv->resource_path = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->resource_path, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->resource_path), "dim-label");
    gtk_box_append(GTK_BOX(header), GTK_WIDGET(priv->resource_title));
    gtk_box_append(GTK_BOX(header), GTK_WIDGET(priv->resource_path));
    gtk_box_append(GTK_BOX(header), back);
    g_signal_connect(back, "clicked", G_CALLBACK(on_resource_back), self);

    priv->resource_tree = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(priv->resource_tree, "row-selected",
                     G_CALLBACK(on_resource_selected), self);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tree_scroll),
                                  GTK_WIDGET(priv->resource_tree));
    gtk_widget_set_size_request(tree_scroll, 280, -1);

    priv->resource_text = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(priv->resource_text, FALSE);
    gtk_text_view_set_cursor_visible(priv->resource_text, FALSE);
    gtk_text_view_set_wrap_mode(priv->resource_text, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(priv->resource_text, 16);
    gtk_text_view_set_right_margin(priv->resource_text, 16);
    gtk_text_view_set_top_margin(priv->resource_text, 16);
    gtk_text_view_set_bottom_margin(priv->resource_text, 16);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(content_scroll),
                                  GTK_WIDGET(priv->resource_text));
    gtk_widget_set_vexpand(content_scroll, TRUE);
    gtk_box_append(GTK_BOX(text_box), content_scroll);

    priv->resource_picture = GTK_PICTURE(gtk_picture_new());
    gtk_picture_set_can_shrink(priv->resource_picture, TRUE);
    gtk_picture_set_content_fit(priv->resource_picture,
                                GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_vexpand(GTK_WIDGET(priv->resource_picture), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(priv->resource_picture), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(picture_scroll),
                                  GTK_WIDGET(priv->resource_picture));

    priv->resource_content = GTK_STACK(gtk_stack_new());
    gtk_stack_add_named(priv->resource_content, text_box, "text");
    gtk_stack_add_named(priv->resource_content, picture_scroll, "picture");
    gtk_widget_set_vexpand(GTK_WIDGET(priv->resource_content), TRUE);

    gtk_paned_set_start_child(GTK_PANED(paned), tree_scroll);
    gtk_paned_set_end_child(GTK_PANED(paned), GTK_WIDGET(priv->resource_content));
    gtk_paned_set_position(GTK_PANED(paned), 300);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_box_append(GTK_BOX(box), header);
    gtk_box_append(GTK_BOX(box), paned);
    return box;
}

static void
gsword_window_sync_reference_context(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswModuleInfo *info;
    const gchar *versification = NULL;
    gchar *locale;

    if (priv->reference_chooser == NULL || priv->module == NULL)
        return;
    info = gsw_module_get_info(priv->module);
    if (info != NULL)
        versification = gsw_module_info_get_versification(info);
    locale = gsword_window_locale_name();
    gsword_reference_chooser_set_versification(
        priv->reference_chooser,
        versification != NULL && *versification != '\0'
        ? versification : "KJV");
    gsword_reference_chooser_set_locale(priv->reference_chooser, locale);
    g_free(locale);
}

static void
gsword_window_sync_reference_chooser(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswKey *key;
    GswVerseKey *verse_key;
    gint book;
    gint chapter;
    gint verse;

    if (priv->reference_chooser == NULL)
        return;
    if (priv->module == NULL ||
        gsw_key_get_kind(gsw_module_get_key(priv->module)) !=
            GSW_KEY_KIND_VERSE) {
        gtk_widget_set_sensitive(GTK_WIDGET(priv->reference_chooser), FALSE);
        return;
    }
    gtk_widget_set_sensitive(GTK_WIDGET(priv->reference_chooser), TRUE);
    key = gsw_module_get_key(priv->module);
    verse_key = (GswVerseKey *)key;
    book = gsw_verse_key_get_global_book_number(verse_key);
    chapter = gsw_verse_key_get_chapter(verse_key);
    verse = MAX(gsw_verse_key_get_verse(verse_key), 1);
    if (book < 0 || chapter < 1)
        return;
    gsword_reference_chooser_set_reference(priv->reference_chooser,
                                           book, chapter, verse);
}

static void
gsword_window_refresh_reader(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;
    gchar *text;
    gchar *display_key = NULL;
    gchar *status;

    gsword_window_clear_range(self);
    if (priv->module == NULL) {
        gsword_window_set_text(self,
            _("No modules found.\n\nInstall SWORD modules or start gsword with "
              "--library PATH."));
        gsword_window_set_status(self, _("No modules found"));
        gsword_window_sync_reference_chooser(self);
        gsword_window_update_index_label(self);
        return;
    }

    if (gsw_key_get_kind(gsw_module_get_key(priv->module)) ==
        GSW_KEY_KIND_VERSE) {
        GswVerseKey *key = (GswVerseKey *)gsw_module_get_key(priv->module);

        if (gsw_verse_key_get_book(key) >= 1 &&
            gsw_verse_key_get_chapter(key) >= 1) {
            text = gsword_window_chapter_text_for_module(priv->module);
            display_key = g_strdup_printf(
                "%s %d:%d", gsw_verse_key_get_book_name(key),
                gsw_verse_key_get_chapter(key),
                MAX(gsw_verse_key_get_verse(key), 1));
        } else {
            text = gsw_module_get_text(priv->module, &error);
        }
    } else {
        text = gsw_module_get_text(priv->module, &error);
    }

    if (text == NULL) {
        gsword_window_show_error(self, _("Cannot read entry"), error);
        g_free(display_key);
        return;
    }
    gsword_window_set_text(self, text);
    g_free(text);

    if (display_key == NULL)
        display_key = g_strdup(gsw_module_get_key_text(priv->module));

    status = g_strdup_printf("%s  |  %s",
                             gsw_module_get_name(priv->module),
                             display_key != NULL ? display_key : "");
    gsword_window_set_status(self, status);
    g_free(status);
    g_free(display_key);

    gsword_window_apply_highlights(self);
    gsword_window_apply_active_verse(self);
    gsword_window_update_study(self);
    gsword_window_update_index_label(self);
    gsword_window_sync_reference_chooser(self);
    gsword_window_workspace_sync_from_reader(self);
    gsword_window_save_last_position(self);
}

gboolean
gsword_window_set_key_text(GswordWindow *self, const gchar *text)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;
    gchar *normalized;

    if (priv->module == NULL)
        return FALSE;
    normalized = gsword_window_normalize_reference(text);
    if (!gsw_module_set_key_text(priv->module, normalized, &error)) {
        gsword_window_show_error(self, _("Cannot set key"), error);
        g_free(normalized);
        return FALSE;
    }
    g_free(normalized);
    gsword_window_refresh_reader(self);
    return TRUE;
}

gchar *
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

gchar *
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

gchar *
gsword_window_current_reference(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswKey *key;

    if (priv->module == NULL)
        return NULL;
    key = gsw_module_get_key(priv->module);
    if (gsw_key_get_kind(key) == GSW_KEY_KIND_VERSE) {
        GswVerseKey *verse_key = (GswVerseKey *)key;
        gint book = gsw_verse_key_get_book(verse_key);
        gint chapter = gsw_verse_key_get_chapter(verse_key);
        gint verse = gsw_verse_key_get_verse(verse_key);

        if (book >= 1 && chapter >= 1) {
            if (priv->range_active && priv->range_start > 0 &&
                priv->range_end > 0) {
                gint first = MIN(priv->range_start, priv->range_end);
                gint last = MAX(priv->range_start, priv->range_end);

                return g_strdup_printf("%s %d:%d-%d",
                                       gsw_verse_key_get_book_name(verse_key),
                                       chapter, first, last);
            }
            return g_strdup_printf("%s %d:%d",
                                   gsw_verse_key_get_book_name(verse_key),
                                   chapter, verse > 0 ? verse : 1);
        }
    }
    return g_strdup(gsw_module_get_key_text(priv->module));
}

gchar *
gsword_window_current_start_reference(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswKey *key;

    if (priv->module == NULL)
        return NULL;
    key = gsw_module_get_key(priv->module);
    if (gsw_key_get_kind(key) == GSW_KEY_KIND_VERSE && priv->range_active &&
        priv->range_start > 0 && priv->range_end > 0) {
        GswVerseKey *verse_key = (GswVerseKey *)key;
        gint book = gsw_verse_key_get_book(verse_key);
        gint chapter = gsw_verse_key_get_chapter(verse_key);

        if (book >= 1 && chapter >= 1) {
            return g_strdup_printf("%s %d:%d",
                                   gsw_verse_key_get_book_name(verse_key),
                                   chapter,
                                   MIN(priv->range_start, priv->range_end));
        }
    }
    return gsword_window_current_reference(self);
}

static void
gsword_window_update_reference_status(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gchar *reference;
    gchar *status;

    if (priv->module == NULL)
        return;
    reference = gsword_window_current_reference(self);
    if (reference == NULL)
        return;
    gsword_window_sync_reference_chooser(self);
    status = g_strdup_printf("%s  |  %s",
                             gsw_module_get_name(priv->module), reference);
    gsword_window_set_status(self, status);
    g_free(status);
    g_free(reference);
}

static void
gsword_window_apply_active_verse(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkTextBuffer *buffer;
    GtkTextIter iter;
    GtkTextIter finish;
    gint active;
    gint line_count;
    gint line;

    if (priv->reader == NULL || priv->active_verse_tag == NULL)
        return;
    buffer = gtk_text_view_get_buffer(priv->reader);
    gtk_text_buffer_get_bounds(buffer, &iter, &finish);
    gtk_text_buffer_remove_tag(buffer, priv->active_verse_tag, &iter, &finish);

    if (priv->module == NULL ||
        gsw_key_get_kind(gsw_module_get_key(priv->module)) !=
            GSW_KEY_KIND_VERSE) {
        gsword_window_apply_range(self);
        return;
    }

    active = gsw_verse_key_get_verse(
        (GswVerseKey *)gsw_module_get_key(priv->module));
    if (active < 1)
        active = 1;
    priv->active_verse = active;

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
            gtk_text_buffer_apply_tag(buffer, priv->active_verse_tag,
                                      &line_start, &line_end);
            break;
        }
    }
    gsword_window_apply_range(self);
}

static void
gsword_window_clear_range(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;

    priv->range_active = FALSE;
    priv->range_start = 0;
    priv->range_end = 0;
    if (priv->reader == NULL || priv->range_tag == NULL)
        return;
    buffer = gtk_text_view_get_buffer(priv->reader);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gtk_text_buffer_remove_tag(buffer, priv->range_tag, &start, &end);
}

static void
gsword_window_apply_range(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;
    gint first;
    gint last;
    gint line_count;
    gint line;

    if (priv->reader == NULL || priv->range_tag == NULL)
        return;
    buffer = gtk_text_view_get_buffer(priv->reader);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gtk_text_buffer_remove_tag(buffer, priv->range_tag, &start, &end);
    if (!priv->range_active || priv->range_start < 1 || priv->range_end < 1)
        return;

    first = MIN(priv->range_start, priv->range_end);
    last = MAX(priv->range_start, priv->range_end);
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
            gtk_text_buffer_apply_tag(buffer, priv->range_tag,
                                      &line_start, &line_end);
    }
}

static void
gsword_window_select_verse_range(GswordWindow *self, gint anchor, gint verse)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswVerseKey *key;
    GError *error = NULL;
    gint maximum;
    gchar *text;

    if (priv->module == NULL ||
        gsw_key_get_kind(gsw_module_get_key(priv->module)) !=
            GSW_KEY_KIND_VERSE)
        return;
    key = (GswVerseKey *)gsw_module_get_key(priv->module);
    maximum = gsw_verse_key_get_verse_max(key);
    if (verse < 1 || verse > maximum)
        return;
    if (anchor < 1 || anchor > maximum)
        anchor = priv->active_verse;
    if (anchor < 1 || anchor > maximum)
        anchor = verse;

    gsw_verse_key_set_verse(key, anchor);
    text = g_strdup(gsw_module_get_key_text(priv->module));
    if (!gsw_module_set_key_text(priv->module, text, &error)) {
        gsword_window_show_error(self, _("Cannot set key"), error);
        g_free(text);
        return;
    }
    g_free(text);

    priv->range_active = TRUE;
    priv->range_start = anchor;
    priv->range_end = verse;
    priv->active_verse = anchor;
    gsword_window_update_reference_status(self);
    gsword_window_update_study(self);
    gsword_window_apply_active_verse(self);
    gsword_window_workspace_sync_from_reader(self);
    gsword_window_save_last_position(self);
}

static void
gsword_window_select_verse(GswordWindow *self, gint verse)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswKey *key;
    GError *error = NULL;
    gchar *text;

    if (priv->module == NULL)
        return;
    key = gsw_module_get_key(priv->module);
    if (gsw_key_get_kind(key) != GSW_KEY_KIND_VERSE)
        return;
    if (verse < 1 || verse > gsw_verse_key_get_verse_max((GswVerseKey *)key))
        return;

    gsword_window_clear_range(self);
    gsw_verse_key_set_verse((GswVerseKey *)key, verse);
    text = g_strdup(gsw_module_get_key_text(priv->module));
    if (!gsw_module_set_key_text(priv->module, text, &error)) {
        gsword_window_show_error(self, _("Cannot set key"), error);
        g_free(text);
        return;
    }
    g_free(text);
    priv->active_verse = verse;
    gsword_window_update_reference_status(self);
    gsword_window_update_study(self);
    gsword_window_apply_active_verse(self);
    gsword_window_workspace_sync_from_reader(self);
    gsword_window_save_last_position(self);
}

static void
on_reader_clicked(GtkGestureClick *gesture, gint n_press, gdouble x,
                  gdouble y, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
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
        GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
        gint anchor = priv->range_active && priv->range_start > 0
                    ? priv->range_start : priv->active_verse;

        gsword_window_select_verse_range(self, anchor, verse);
    } else {
        gsword_window_select_verse(self, verse);
    }
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
        gsword_window_set_status(self, _("Highlight removed"));
    } else {
        gsword_notes_store_set_highlight(
            priv->notes, gsw_module_get_name(priv->module), storage_key,
            "#fff0a8", &error);
        gsword_window_set_status(self, _("Verse highlighted"));
    }
    if (error != NULL)
        gsword_window_show_error(self, _("Cannot update highlight"), error);
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
        gsword_window_set_status(self, _("Cannot open module"));
        return;
    }

    g_clear_object(&priv->module);
    priv->module = module;
    gsword_window_sync_reference_context(self);

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
    if (name == NULL || priv->startup_selecting)
        return;

    {
        gint type = GPOINTER_TO_INT(
            g_object_get_data(G_OBJECT(row), "module-type"));

        if (type == GSW_MODULE_TYPE_COMMENTARIES) {
            g_free(priv->commentary_name);
            priv->commentary_name = g_strdup(name);
            g_clear_object(&priv->commentary_module);
            priv->commentary_module = gsw_manager_create_module(
                priv->manager, name);
            if (priv->settings != NULL)
                g_settings_set_string(priv->settings, "study-module", name);
            gtk_widget_set_visible(priv->study_panel, TRUE);
            if (priv->study_toggle != NULL)
                gtk_toggle_button_set_active(priv->study_toggle, TRUE);
            if (priv->study_notebook != NULL)
                gtk_notebook_set_current_page(priv->study_notebook, 0);
            gsword_window_update_study(self);
            gsword_window_show_page(self, "reader");
            gsword_window_set_status(self, _("Commentary opened in study panel"));
            return;
        }
        if (type == GSW_MODULE_TYPE_LEXDICTS) {
            g_free(priv->lexicon_name);
            priv->lexicon_name = g_strdup(name);
            g_clear_object(&priv->lexicon_module);
            priv->lexicon_module = gsw_manager_create_module(priv->manager,
                                                             name);
            if (priv->settings != NULL)
                g_settings_set_string(priv->settings, "study-lexicon", name);
            gsword_window_refresh_lexicon_modules(self);
            gtk_widget_set_visible(priv->study_panel, TRUE);
            if (priv->study_toggle != NULL)
                gtk_toggle_button_set_active(priv->study_toggle, TRUE);
            if (priv->study_notebook != NULL)
                gtk_notebook_set_current_page(priv->study_notebook, 3);
            gsword_window_show_page(self, "reader");
            gsword_window_set_status(self, _("Lexicon opened in study panel"));
            return;
        }
        if (type == GSW_MODULE_TYPE_DAILY_DEVOS) {
            if (priv->settings != NULL)
                g_settings_set_string(priv->settings, "devotion-module", name);
            gsword_window_show_page(self, "plan");
            if (priv->plan_notebook != NULL)
                gtk_notebook_set_current_page(priv->plan_notebook, 2);
            gsword_window_refresh_devotion(self);
            return;
        }
        if (type == GSW_MODULE_TYPE_GENBOOKS ||
            type == GSW_MODULE_TYPE_MAPS) {
            gsword_window_open_resource(self, name,
                                        type == GSW_MODULE_TYPE_MAPS);
            return;
        }
    }

    gsword_window_load_module(self, name);
    gsword_window_show_page(self, "reader");
}

static const gchar *
gsword_window_module_type_display(GswModuleInfo *info)
{
    if (module_info_is_devotional(info))
        return _("Daily Devotional");
    if (module_info_is_map(info))
        return _("Maps");
    switch (gsw_module_info_get_module_type(info)) {
    case GSW_MODULE_TYPE_BIBLES:       return _("Biblical Texts");
    case GSW_MODULE_TYPE_COMMENTARIES: return _("Commentaries");
    case GSW_MODULE_TYPE_LEXDICTS:     return _("Lexicons / Dictionaries");
    case GSW_MODULE_TYPE_GENBOOKS:     return _("Generic Books");
    case GSW_MODULE_TYPE_DAILY_DEVOS:  return _("Daily Devotional");
    case GSW_MODULE_TYPE_MAPS:         return _("Maps");
    case GSW_MODULE_TYPE_OTHER:        return _("Other");
    case GSW_MODULE_TYPE_UNKNOWN:
    default:
        return gsw_module_info_get_type_string(info) != NULL
             ? gsw_module_info_get_type_string(info) : _("Unknown");
    }
}

static GswModuleType
gsword_window_resource_type(GswModuleInfo *info)
{
    if (module_info_is_devotional(info))
        return GSW_MODULE_TYPE_DAILY_DEVOS;
    if (module_info_is_map(info))
        return GSW_MODULE_TYPE_MAPS;
    return gsw_module_info_get_module_type(info);
}

static const gchar *
gsword_window_module_group_title(GswModuleType type)
{
    switch (type) {
    case GSW_MODULE_TYPE_BIBLES:       return _("Bibles");
    case GSW_MODULE_TYPE_COMMENTARIES: return _("Commentaries");
    case GSW_MODULE_TYPE_LEXDICTS:     return _("Lexicons / Dictionaries");
    case GSW_MODULE_TYPE_DAILY_DEVOS:  return _("Daily Devotions");
    case GSW_MODULE_TYPE_GENBOOKS:     return _("General Books");
    case GSW_MODULE_TYPE_MAPS:         return _("Maps");
    case GSW_MODULE_TYPE_OTHER:
    case GSW_MODULE_TYPE_UNKNOWN:
    default:                           return _("Other Modules");
    }
}

static GtkWidget *
gsword_window_make_module_group_row(const gchar *title)
{
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *label = gtk_label_new(title);

    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "heading");
    gtk_widget_set_margin_top(label, 10);
    gtk_widget_set_margin_bottom(label, 2);
    gtk_widget_set_margin_start(label, 10);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
    return row;
}

static GtkWidget *
gsword_window_make_module_row(GswModuleInfo *info)
{
    const gchar *name = gsw_module_info_get_name(info);
    const gchar *description = gsw_module_info_get_description(info);
    const gchar *type = gsword_window_module_type_display(info);
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
    g_object_set_data(G_OBJECT(row), "module-type",
                      GINT_TO_POINTER((gint)
                          gsword_window_resource_type(info)));
    return row;
}

static gboolean
gsword_window_reset_startup_selecting(gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    priv->startup_selecting = FALSE;
    return G_SOURCE_REMOVE;
}

static void
gsword_window_populate_modules(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    static const GswModuleType group_types[] = {
        GSW_MODULE_TYPE_BIBLES,
        GSW_MODULE_TYPE_COMMENTARIES,
        GSW_MODULE_TYPE_LEXDICTS,
        GSW_MODULE_TYPE_DAILY_DEVOS,
        GSW_MODULE_TYPE_GENBOOKS,
        GSW_MODULE_TYPE_MAPS,
        GSW_MODULE_TYPE_OTHER,
    };
    GPtrArray *infos;
    gchar *wanted = NULL;
    GtkListBoxRow *selected = NULL;
    GtkListBoxRow *first = NULL;
    guint group;

    if (priv->manager == NULL) {
        gsword_window_show_page(self, "reader");
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

    for (group = 0; group < G_N_ELEMENTS(group_types); group++) {
        gboolean header_added = FALSE;
        guint i;

        for (i = 0; i < infos->len; i++) {
            GswModuleInfo *info = g_ptr_array_index(infos, i);
            GswModuleType type = gsword_window_resource_type(info);
            GswModuleType effective_type = type == GSW_MODULE_TYPE_UNKNOWN
                                         ? GSW_MODULE_TYPE_OTHER : type;
            const gchar *name = gsw_module_info_get_name(info);
            GtkWidget *row;

            if (effective_type != group_types[group])
                continue;
            if (!header_added) {
                gtk_list_box_append(
                    priv->module_list,
                    gsword_window_make_module_group_row(
                        gsword_window_module_group_title(group_types[group])));
                header_added = TRUE;
            }
            row = gsword_window_make_module_row(info);
            gtk_list_box_append(priv->module_list, row);
            if (first == NULL)
                first = GTK_LIST_BOX_ROW(row);

            if (type == GSW_MODULE_TYPE_BIBLES && wanted != NULL &&
                g_strcmp0(wanted, name) == 0)
                selected = GTK_LIST_BOX_ROW(row);
        }
    }

    g_ptr_array_unref(infos);
    g_free(wanted);

    if (selected == NULL)
        selected = first;
    if (selected != NULL) {
        const gchar *selected_name = g_object_get_data(G_OBJECT(selected),
                                                       "module-name");

        priv->startup_selecting = TRUE;
        gtk_list_box_select_row(priv->module_list, selected);
        if (selected_name != NULL)
            gsword_window_load_module(self, selected_name);
        gsword_window_show_page(self, "reader");
        g_idle_add_full(G_PRIORITY_DEFAULT,
                        gsword_window_reset_startup_selecting,
                        g_object_ref(self), g_object_unref);
    } else {
        gsword_window_show_page(self, "reader");
        gsword_window_refresh_reader(self);
    }
}

static void
on_reference_chooser_activate(GswordReferenceChooser *chooser,
                              gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    gint book;
    gint chapter;
    gint verse;
    gchar *osis;
    gchar *text;

    if (!gsword_reference_chooser_get_reference(chooser, &book, &chapter,
                                                &verse))
        return;
    osis = gsword_reference_chooser_dup_osis(chooser);
    if (osis == NULL || *osis == '\0') {
        g_free(osis);
        return;
    }
    text = g_strdup_printf("%s %d:%d", osis, chapter, verse);
    if (!gsword_window_set_key_text(self, text))
        gsword_window_sync_reference_chooser(self);
    g_free(text);
    g_free(osis);
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
    if (priv->range_active && priv->range_start > 0 &&
        priv->range_end > 0) {
        gint first = MIN(priv->range_start, priv->range_end);
        gint last = MAX(priv->range_start, priv->range_end);
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
    clipboard = gtk_widget_get_clipboard(GTK_WIDGET(self));
    gdk_clipboard_set_text(clipboard, text != NULL ? text : "");
    g_free(text);
    gsword_window_set_status(self, _("Copied to clipboard"));
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
    adw_dialog_set_title(ADW_DIALOG(dialog), _("Preferences"));
    adw_dialog_set_content_width(ADW_DIALOG(dialog), 620);

    page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
    group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(group, _("Reading"));
    adw_preferences_group_set_description(
        group, _("Reading appearance is applied immediately."));

    font_row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(font_row),
                                  _("Font size"));
    adw_action_row_set_subtitle(font_row, _("Scale the scripture text size"));
    adjustment = gtk_adjustment_new(priv->font_scale, 0.75, 2.5, 0.1, 0.25, 0);
    spin = gtk_spin_button_new(adjustment, 0.1, 1);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    g_signal_connect(spin, "value-changed",
                     G_CALLBACK(on_font_scale_changed), self);
    adw_action_row_add_suffix(font_row, spin);
    adw_action_row_set_activatable_widget(font_row, spin);

    theme_row = ADW_COMBO_ROW(adw_combo_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(theme_row),
                                  _("Color scheme"));
    adw_action_row_set_subtitle(ADW_ACTION_ROW(theme_row),
                                _("Follow the desktop or force light/dark mode"));
    themes = gtk_string_list_new(
        (const gchar *[]) { _("Follow system"), _("Light"), _("Dark"), NULL });
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
    if (priv->reference_chooser != NULL)
        gsword_reference_chooser_begin_edit(priv->reference_chooser);
}

static void
gsword_window_apply_workspace(GswordWindow *self, const gchar *workspace)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (g_strcmp0(workspace, "reading") == 0) {
        if (priv->study_toggle != NULL)
            gtk_toggle_button_set_active(priv->study_toggle, FALSE);
        gsword_window_show_page(self, "reader");
    } else if (g_strcmp0(workspace, "study") == 0) {
        if (priv->study_toggle != NULL)
            gtk_toggle_button_set_active(priv->study_toggle, TRUE);
        if (priv->study_follow_toggle != NULL)
            gtk_check_button_set_active(priv->study_follow_toggle, TRUE);
        if (priv->study_notebook != NULL)
            gtk_notebook_set_current_page(priv->study_notebook, 0);
        gsword_window_update_study(self);
        gsword_window_show_page(self, "reader");
    } else if (g_strcmp0(workspace, "panes") == 0) {
        gsword_window_show_page(self, "panes");
        gsword_window_workspace_sync_from_reader(self);
    } else if (g_strcmp0(workspace, "parallel") == 0) {
        gsword_window_apply_parallel_template(self);
    } else if (g_strcmp0(workspace, "teaching") == 0) {
        gsword_window_teaching_load(self);
        gsword_window_show_page(self, "teaching");
    }
}

static void
on_main_menu_response(AdwAlertDialog *dialog, const gchar *response,
                      gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);

    (void)dialog;
    if (g_strcmp0(response, "reading") == 0 ||
        g_strcmp0(response, "study") == 0 ||
        g_strcmp0(response, "panes") == 0 ||
        g_strcmp0(response, "parallel") == 0 ||
        g_strcmp0(response, "teaching") == 0) {
        gsword_window_apply_workspace(self, response);
    } else if (g_strcmp0(response, "preferences") == 0) {
        gsword_window_show_preferences(self);
    } else if (g_strcmp0(response, "quit") == 0) {
        GtkApplication *application = gtk_window_get_application(
            GTK_WINDOW(self));

        if (application != NULL)
            g_application_quit(G_APPLICATION(application));
    }
}

static void
on_main_menu_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    AdwDialog *dialog = adw_alert_dialog_new(_("Main menu"), NULL);

    (void)button;
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "reading", _("Reading workspace"),
                                   "study", _("Study workspace"),
                                   "panes", _("Multi-pane workspace"),
                                   "parallel", _("Parallel workspace"),
                                   "teaching", _("Teaching workspace"),
                                   "preferences", _("Preferences"),
                                   "quit", _("Quit"),
                                   "close", _("Close"),
                                   NULL);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "close");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "close");
    g_signal_connect(dialog, "response",
                     G_CALLBACK(on_main_menu_response), self);
    adw_dialog_present(dialog, GTK_WIDGET(self));
}

static GtkWidget *
gsword_window_make_sidebar(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *label = gtk_label_new(_("Modules"));
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
    if (priv->settings != NULL)
        g_settings_set_boolean(priv->settings, "study-visible",
                               gtk_toggle_button_get_active(button));
}

static void
on_study_follow_toggled(GtkCheckButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gboolean follow = gtk_check_button_get_active(button);

    priv->commentary_follow = follow;
    g_clear_pointer(&priv->pinned_commentary_key, g_free);
    if (!follow) {
        if (priv->commentary_module != NULL)
            priv->pinned_commentary_key = g_strdup(
                gsw_module_get_key_text(priv->commentary_module));
        else if (priv->module != NULL)
            priv->pinned_commentary_key = g_strdup(
                gsw_module_get_key_text(priv->module));
    }
    if (priv->settings != NULL) {
        g_settings_set_boolean(priv->settings, "study-follow", follow);
        g_settings_set_string(priv->settings, "study-pinned-key",
                              priv->pinned_commentary_key != NULL
                              ? priv->pinned_commentary_key : "");
    }
    gsword_window_update_study(self);
}

static void
on_workspace_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *name = gtk_stack_get_visible_child_name(
        GTK_STACK(object));

    (void)pspec;
    if (priv->settings != NULL && name != NULL && priv->page_restored)
        g_settings_set_string(priv->settings, "active-page", name);
    if (g_strcmp0(name, "panes") == 0) {
        gsword_window_workspace_sync_from_reader(self);
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
    if (g_strcmp0(page, "parallel") == 0) {
        gsword_window_apply_parallel_template(self);
        gsword_window_show_page(self, "panes");
        if (priv->settings != NULL)
            g_settings_set_string(priv->settings, "active-page", "panes");
    } else if (page != NULL &&
               (g_strcmp0(page, "reader") == 0 ||
                g_strcmp0(page, "panes") == 0 ||
                g_strcmp0(page, "search") == 0 ||
                g_strcmp0(page, "plan") == 0 ||
                g_strcmp0(page, "teaching") == 0)) {
        gsword_window_show_page(self, page);
    }
    priv->page_restored = TRUE;
    g_free(page);
}

static void
on_lexicon_lookup(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_lexicon_lookup(GSWORD_WINDOW(user_data));
}

static void
on_lexicon_selected(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint selected = gtk_drop_down_get_selected(priv->lexicon_selector);
    const gchar *name;

    (void)object;
    (void)pspec;
    if (priv->lexicon_names == NULL ||
        selected == GTK_INVALID_LIST_POSITION)
        return;
    name = gtk_string_list_get_string(priv->lexicon_names, selected);
    if (name == NULL || g_strcmp0(name, priv->lexicon_name) == 0)
        return;
    g_free(priv->lexicon_name);
    priv->lexicon_name = g_strdup(name);
    g_clear_object(&priv->lexicon_module);
    priv->lexicon_module = gsw_manager_create_module(priv->manager, name);
    if (priv->settings != NULL)
        g_settings_set_string(priv->settings, "study-lexicon", name);
    gsword_window_set_view_text(priv->lexicon_view, "");
}

static void
gsword_window_refresh_lexicon_modules(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *infos;
    GtkStringList *names;
    guint selected = 0;
    gboolean found = FALSE;
    guint count = 0;
    guint i;

    if (priv->lexicon_selector == NULL || priv->manager == NULL)
        return;
    infos = gsw_manager_get_module_infos(priv->manager);
    names = gtk_string_list_new(NULL);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (gsw_module_info_get_module_type(info) != GSW_MODULE_TYPE_LEXDICTS ||
            module_info_is_devotional(info) || module_info_is_map(info))
            continue;
        gtk_string_list_append(names, gsw_module_info_get_name(info));
        if (priv->lexicon_name != NULL &&
            g_strcmp0(priv->lexicon_name,
                      gsw_module_info_get_name(info)) == 0) {
            selected = count;
            found = TRUE;
        }
        count++;
    }
    g_ptr_array_unref(infos);
    g_clear_object(&priv->lexicon_names);
    priv->lexicon_names = names;
    gtk_drop_down_set_model(priv->lexicon_selector, G_LIST_MODEL(names));
    if (count > 0) {
        if (!found)
            selected = 0;
        gtk_drop_down_set_selected(priv->lexicon_selector, selected);
    }
}

static void
on_strongs_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *key;

    (void)list;
    if (row == NULL)
        return;
    key = g_object_get_data(G_OBJECT(row), "strongs-key");
    if (key == NULL)
        return;
    gtk_editable_set_text(GTK_EDITABLE(priv->lexicon_entry), key);
    gsword_window_lexicon_lookup(self);
    if (priv->study_notebook != NULL)
        gtk_notebook_set_current_page(priv->study_notebook, 3);
}

static void
on_crossref_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    const gchar *reference;

    (void)list;
    if (row == NULL)
        return;
    reference = g_object_get_data(G_OBJECT(row), "crossref-key");
    if (reference != NULL)
        gsword_window_open_reference(GSWORD_WINDOW(user_data), reference);
}

static GtkWidget *
gsword_window_make_study_panel(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *notebook = gtk_notebook_new();
    GtkWidget *commentary = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *commentary_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *study_follow = gtk_check_button_new_with_label(_("Follow"));
    GtkWidget *commentary_scroll;
    GtkWidget *strongs = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *strongs_scroll;
    GtkWidget *strongs_list_scroll = gtk_scrolled_window_new();
    GtkWidget *crossrefs = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *crossrefs_scroll;
    GtkWidget *crossrefs_list_scroll = gtk_scrolled_window_new();
    GtkWidget *lexicon_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *lexicon_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *lexicon_module_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *lexicon_button = gtk_button_new_with_label(_("Lookup"));
    GtkWidget *lexicon;

    commentary_scroll = gsword_window_make_scrolled_text(
        &priv->commentary_view);
    gtk_widget_set_vexpand(commentary_scroll, TRUE);
    priv->commentary_context = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->commentary_context, 0.0f);
    gtk_label_set_ellipsize(priv->commentary_context, PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(GTK_WIDGET(priv->commentary_context),
                             "dim-label");
    gtk_widget_set_margin_start(GTK_WIDGET(priv->commentary_context), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(priv->commentary_context), 6);
    gtk_widget_set_hexpand(GTK_WIDGET(priv->commentary_context), TRUE);
    priv->study_follow_toggle = GTK_CHECK_BUTTON(study_follow);
    gtk_check_button_set_active(priv->study_follow_toggle, TRUE);
    g_signal_connect(study_follow, "toggled",
                     G_CALLBACK(on_study_follow_toggled), self);
    gtk_box_append(GTK_BOX(commentary_header),
                   GTK_WIDGET(priv->commentary_context));
    gtk_box_append(GTK_BOX(commentary_header), study_follow);
    gtk_box_append(GTK_BOX(commentary), commentary_header);
    gtk_box_append(GTK_BOX(commentary), commentary_scroll);
    strongs_scroll = gsword_window_make_scrolled_text(&priv->strongs_view);
    gtk_widget_set_vexpand(strongs_scroll, TRUE);
    priv->strongs_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(priv->strongs_list, "row-selected",
                     G_CALLBACK(on_strongs_selected), self);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(strongs_list_scroll),
                                  GTK_WIDGET(priv->strongs_list));
    gtk_widget_set_size_request(strongs_list_scroll, -1, 110);
    gtk_box_append(GTK_BOX(strongs), strongs_list_scroll);
    gtk_box_append(GTK_BOX(strongs), strongs_scroll);
    crossrefs_scroll = gsword_window_make_scrolled_text(&priv->crossref_view);
    gtk_widget_set_vexpand(crossrefs_scroll, TRUE);
    priv->crossrefs_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(priv->crossrefs_list, "row-selected",
                     G_CALLBACK(on_crossref_selected), self);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(crossrefs_list_scroll),
                                  GTK_WIDGET(priv->crossrefs_list));
    gtk_widget_set_size_request(crossrefs_list_scroll, -1, 110);
    gtk_box_append(GTK_BOX(crossrefs), crossrefs_list_scroll);
    gtk_box_append(GTK_BOX(crossrefs), crossrefs_scroll);
    lexicon = gsword_window_make_scrolled_text(&priv->lexicon_view);
    priv->lexicon_selector = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    gtk_widget_set_hexpand(GTK_WIDGET(priv->lexicon_selector), TRUE);
    g_signal_connect(priv->lexicon_selector, "notify::selected",
                     G_CALLBACK(on_lexicon_selected), self);
    gtk_box_append(GTK_BOX(lexicon_module_row), gtk_label_new(_("Lexicon")));
    gtk_box_append(GTK_BOX(lexicon_module_row),
                   GTK_WIDGET(priv->lexicon_selector));
    gtk_box_append(GTK_BOX(lexicon_box), lexicon_module_row);
    priv->lexicon_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->lexicon_entry, _("Strong's / dictionary key"));
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
                             gtk_label_new(_("Commentary")));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), strongs,
                             gtk_label_new(_("Strong's")));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), crossrefs,
                             gtk_label_new(_("Cross-refs")));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), lexicon_box,
                             gtk_label_new(_("Lexicon")));
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
    GtkWidget *copy = gtk_button_new_from_icon_name("edit-copy-symbolic");
    GtkWidget *note = gtk_button_new_from_icon_name("document-edit-symbolic");
    GtkWidget *highlight = gtk_button_new_from_icon_name(
        "emblem-favorite-symbolic");
    GtkWidget *teaching = gtk_button_new_with_label(_("Teach"));
    GtkWidget *study_button = gtk_toggle_button_new();
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkGesture *reader_click;
    GtkWidget *study_panel = gsword_window_make_study_panel(self);

    gtk_widget_set_margin_top(toolbar, 8);
    gtk_widget_set_margin_bottom(toolbar, 8);
    gtk_widget_set_margin_start(toolbar, 12);
    gtk_widget_set_margin_end(toolbar, 12);

    {
        gchar *locale = gsword_window_locale_name();

        priv->reference_chooser = GSWORD_REFERENCE_CHOOSER(
            gsword_reference_chooser_new("KJV", locale));
        g_free(locale);
    }
    gtk_widget_set_hexpand(GTK_WIDGET(priv->reference_chooser), TRUE);
    g_signal_connect(priv->reference_chooser, "activate",
                     G_CALLBACK(on_reference_chooser_activate), self);
    g_signal_connect(copy, "clicked", G_CALLBACK(on_copy_clicked), self);
    g_signal_connect(note, "clicked", G_CALLBACK(on_note_clicked), self);
    g_signal_connect(highlight, "clicked",
                     G_CALLBACK(on_highlight_clicked), self);
    g_signal_connect(teaching, "clicked",
                     G_CALLBACK(on_teaching_clicked), self);
    gtk_widget_set_tooltip_text(teaching,
                                _("Open teaching layout for this passage"));
    gtk_button_set_icon_name(GTK_BUTTON(study_button),
                             "dialog-information-symbolic");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(study_button), TRUE);
    priv->study_toggle = GTK_TOGGLE_BUTTON(study_button);
    g_signal_connect(study_button, "toggled",
                     G_CALLBACK(on_study_toggled), self);
    gtk_widget_set_tooltip_text(copy, _("Copy entry"));
    gtk_widget_set_tooltip_text(note, _("Open note for this verse"));
    gtk_widget_set_tooltip_text(highlight, _("Toggle verse highlight"));
    gtk_widget_set_tooltip_text(study_button, _("Toggle study panel"));

    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(priv->reference_chooser));
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
    priv->range_tag = gtk_text_buffer_create_tag(
        gtk_text_view_get_buffer(priv->reader), "gsword-range",
        "background", "#ede9fe", NULL);
    gtk_text_tag_set_priority(priv->range_tag, 1);
    priv->active_verse_tag = gtk_text_buffer_create_tag(
        gtk_text_view_get_buffer(priv->reader), "gsword-active-verse",
        "background", "#dbeafe", "weight", PANGO_WEIGHT_SEMIBOLD, NULL);
    gtk_text_tag_set_priority(priv->active_verse_tag, 2);
    reader_click = gtk_gesture_click_new();
    g_signal_connect(reader_click, "pressed",
                     G_CALLBACK(on_reader_clicked), self);
    gtk_widget_add_controller(GTK_WIDGET(priv->reader),
                              GTK_EVENT_CONTROLLER(reader_click));
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

static guint workspace_pane_next_id = 1;

static gboolean
gsword_window_workspace_pane_supported(GswModuleInfo *info)
{
    GswModuleType type;

    if (info == NULL || module_info_is_map(info) ||
        !gsw_module_info_is_supported(info))
        return FALSE;
    type = gsword_window_resource_type(info);
    return type != GSW_MODULE_TYPE_MAPS &&
           type != GSW_MODULE_TYPE_UNKNOWN;
}

static gchar *
gsword_window_workspace_default_module(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *infos;
    gchar *fallback = NULL;
    gchar *result = NULL;
    guint i;

    if (priv->module != NULL)
        return g_strdup(gsw_module_get_name(priv->module));
    if (priv->manager == NULL)
        return NULL;
    infos = gsw_manager_get_module_infos(priv->manager);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (!gsword_window_workspace_pane_supported(info))
            continue;
        if (fallback == NULL)
            fallback = g_strdup(gsw_module_info_get_name(info));
        if (gsword_window_resource_type(info) == GSW_MODULE_TYPE_BIBLES) {
            result = g_strdup(gsw_module_info_get_name(info));
            break;
        }
    }
    if (result == NULL)
        result = fallback;
    else
        g_free(fallback);
    g_ptr_array_unref(infos);
    return result;
}

static void
gsword_window_workspace_pane_fill_modules(GswordWorkspacePane *pane)
{
    GswordWindow *self = g_object_get_data(G_OBJECT(pane->root),
                                           "workspace-window");
    GswordWindowPrivate *priv;
    GPtrArray *infos;
    GtkStringList *names;
    guint i;

    if (self == NULL)
        return;
    priv = gsword_window_get_instance_private(self);
    if (priv->manager == NULL)
        return;

    infos = gsw_manager_get_module_infos(priv->manager);
    names = gtk_string_list_new(NULL);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (gsword_window_workspace_pane_supported(info))
            gtk_string_list_append(names,
                                   gsw_module_info_get_name(info));
    }
    g_ptr_array_unref(infos);
    g_clear_object(&pane->module_names);
    pane->module_names = names;
    pane->updating++;
    gtk_drop_down_set_model(pane->module_selector, G_LIST_MODEL(names));
    pane->updating--;
}

static void
gsword_window_workspace_pane_apply_active(GswordWorkspacePane *pane)
{
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;
    GswKey *key;
    gint active;
    gint line_count;
    gint line;

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
            gtk_text_buffer_apply_tag(buffer, pane->active_tag,
                                      &line_start, &line_end);
            break;
        }
    }
}

static void
gsword_window_workspace_pane_set_context(GswordWorkspacePane *pane)
{
    GswModuleInfo *info;
    const gchar *versification = NULL;

    if (pane->reference_chooser == NULL || pane->module == NULL)
        return;
    info = gsw_module_get_info(pane->module);
    if (info != NULL)
        versification = gsw_module_info_get_versification(info);
    if (versification == NULL || *versification == '\0')
        versification = "KJV";
    if (g_strcmp0(pane->versification, versification) == 0)
        return;
    g_free(pane->versification);
    pane->versification = g_strdup(versification);
    gsword_reference_chooser_set_versification(pane->reference_chooser,
                                               versification);
}

static void
gsword_window_workspace_pane_sync_chooser(GswordWorkspacePane *pane)
{
    GswKey *key;
    GswVerseKey *verse_key;
    gint book;
    gint chapter;
    gint verse;

    if (pane->reference_chooser == NULL)
        return;
    gsword_window_workspace_pane_set_context(pane);
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
gsword_window_workspace_pane_render(GswordWorkspacePane *pane)
{
    GError *error = NULL;
    GswModuleInfo *info;
    GswModuleType type;
    gchar *text = NULL;

    if (pane->module == NULL) {
        gsword_window_set_view_text(pane->view,
                                    _("No module selected."));
        return;
    }
    info = gsw_module_get_info(pane->module);
    type = gsword_window_resource_type(info);
    if (type == GSW_MODULE_TYPE_BIBLES) {
        text = gsword_window_chapter_text_for_module(pane->module);
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
    gsword_window_workspace_pane_sync_chooser(pane);
    gsword_window_workspace_pane_apply_active(pane);
}

static void
gsword_window_workspace_pane_set_reference(GswordWorkspacePane *pane,
                                           const gchar *reference)
{
    GError *error = NULL;

    if (pane->module == NULL || reference == NULL || *reference == '\0')
        return;
    if (!gsw_module_set_key_text(pane->module, reference, &error)) {
        g_clear_error(&error);
        return;
    }
    gsword_window_workspace_pane_render(pane);
}

static void
gsword_window_workspace_pane_set_module(GswordWorkspacePane *pane,
                                        const gchar *name,
                                        const gchar *key)
{
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
    if (selected == GTK_INVALID_LIST_POSITION &&
        pane->module_names != NULL) {
        gtk_string_list_append(pane->module_names, name);
        selected = g_list_model_get_n_items(
                       G_LIST_MODEL(pane->module_names)) - 1;
    }
    pane->updating++;
    if (selected != GTK_INVALID_LIST_POSITION)
        gtk_drop_down_set_selected(pane->module_selector, selected);
    pane->updating--;

    g_free(pane->module_name);
    pane->module_name = g_strdup(name);
    {
        GswordWindow *self = g_object_get_data(G_OBJECT(pane->root),
                                               "workspace-window");
        GswordWindowPrivate *priv = self != NULL
                                  ? gsword_window_get_instance_private(
                                        GSWORD_WINDOW(self)) : NULL;

        g_clear_object(&pane->module);
        if (priv != NULL && priv->manager != NULL)
            pane->module = gsw_manager_create_module(priv->manager, name);
    }
    if (pane->module == NULL) {
        gsword_window_set_view_text(pane->view, _("Cannot open module."));
        return;
    }

    if (key != NULL && *key != '\0' &&
        !gsw_module_set_key_text(pane->module, key, &error)) {
        g_clear_error(&error);
        gsword_window_set_initial_key_for_module(pane->module);
    } else if (key == NULL || *key == '\0') {
        gsword_window_set_initial_key_for_module(pane->module);
    }
    pane->updating++;
    {
        gboolean can_follow =
            gsw_key_get_kind(gsw_module_get_key(pane->module)) ==
            GSW_KEY_KIND_VERSE;

        gtk_widget_set_sensitive(GTK_WIDGET(pane->follow), can_follow);
        if (!can_follow)
            gtk_check_button_set_active(GTK_CHECK_BUTTON(pane->follow), FALSE);
    }
    pane->updating--;
    gsword_window_workspace_pane_set_context(pane);
    gsword_window_workspace_pane_render(pane);
}

static void
gsword_window_workspace_pane_free(gpointer data)
{
    GswordWorkspacePane *pane = data;

    g_clear_object(&pane->module);
    g_clear_object(&pane->module_names);
    g_free(pane->module_name);
    g_free(pane->versification);
    g_free(pane);
}

static void
gsword_window_workspace_reflow(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint columns;
    guint i;

    if (priv->workspace_grid == NULL || priv->workspace_panes == NULL)
        return;
    columns = priv->workspace_columns != NULL
            ? (guint)gtk_spin_button_get_value_as_int(priv->workspace_columns)
            : 2;
    if (columns < 1)
        columns = 1;

    for (i = 0; i < priv->workspace_panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(priv->workspace_panes, i);

        g_object_ref(pane->root);
        if (gtk_widget_get_parent(pane->root) ==
            GTK_WIDGET(priv->workspace_grid))
            gtk_grid_remove(priv->workspace_grid, pane->root);
    }
    for (i = 0; i < priv->workspace_panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(priv->workspace_panes, i);

        gtk_grid_attach(priv->workspace_grid, pane->root,
                        (gint)(i % columns), (gint)(i / columns), 1, 1);
        g_object_unref(pane->root);
    }
    gtk_grid_set_column_homogeneous(priv->workspace_grid, TRUE);
    gtk_grid_set_row_homogeneous(priv->workspace_grid, TRUE);
    if (priv->workspace_stack != NULL) {
        gtk_stack_set_visible_child_name(
            priv->workspace_stack,
            priv->workspace_panes->len > 0 ? "grid" : "empty");
    }
}

static void
gsword_window_workspace_save(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GVariantBuilder builder;
    GVariant *layout;
    gchar *text;
    guint i;

    if (priv->settings == NULL || priv->workspace_panes == NULL ||
        priv->workspace_restoring)
        return;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ssb)"));
    for (i = 0; i < priv->workspace_panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(priv->workspace_panes, i);
        const gchar *key = pane->module != NULL
                         ? gsw_module_get_key_text(pane->module) : NULL;

        g_variant_builder_add(&builder, "(ssb)",
                              pane->module_name != NULL ? pane->module_name : "",
                              key != NULL ? key : "",
                              gtk_check_button_get_active(
                                  GTK_CHECK_BUTTON(pane->follow)));
    }
    layout = g_variant_ref_sink(g_variant_builder_end(&builder));
    text = g_variant_print(layout, FALSE);
    g_settings_set_string(priv->settings, "workspace-layout", text);
    if (priv->workspace_columns != NULL)
        g_settings_set_int(priv->settings, "workspace-columns",
                           gtk_spin_button_get_value_as_int(
                               priv->workspace_columns));
    g_settings_set_boolean(priv->settings, "workspace-sync-scroll",
                           priv->workspace_sync_scroll_enabled);
    g_free(text);
    g_variant_unref(layout);
}

static void
gsword_window_workspace_pane_note_reference(GswordWorkspacePane *pane)
{
    GswordWindow *self = g_object_get_data(G_OBJECT(pane->root),
                                           "workspace-window");
    gchar *reference = NULL;

    if (self == NULL)
        return;
    if (pane->module != NULL &&
        gsw_key_get_kind(gsw_module_get_key(pane->module)) ==
            GSW_KEY_KIND_VERSE) {
        GswVerseKey *key = (GswVerseKey *)gsw_module_get_key(pane->module);

        reference = g_strdup_printf(
            "%s %d:%d", gsw_verse_key_get_book_name(key),
            gsw_verse_key_get_chapter(key), gsw_verse_key_get_verse(key));
    }
    if (reference != NULL)
        gsword_window_workspace_sync_reference(self, reference, pane);
    g_free(reference);
    gsword_window_workspace_save(self);
}

static void
gsword_window_workspace_pane_reference_activate(
    GswordReferenceChooser *chooser, gpointer user_data)
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
    osis = gsword_reference_chooser_dup_osis(chooser);
    if (osis == NULL || *osis == '\0') {
        g_free(osis);
        return;
    }
    text = g_strdup_printf("%s %d:%d", osis, chapter, verse);
    gsword_window_workspace_pane_set_reference(pane, text);
    g_free(text);
    g_free(osis);
    gsword_window_workspace_pane_note_reference(pane);
}

static void
gsword_window_workspace_pane_module_changed(GObject *object,
                                            GParamSpec *pspec,
                                            gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;
    GswordWindow *self;
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
    self = g_object_get_data(G_OBJECT(pane->root), "workspace-window");
    {
        gchar *key = pane->module != NULL
                   ? g_strdup(gsw_module_get_key_text(pane->module)) : NULL;

        gsword_window_workspace_pane_set_module(pane, name, key);
        g_free(key);
    }
    if (self != NULL)
        gsword_window_workspace_save(GSWORD_WINDOW(self));
}

static void
gsword_window_workspace_pane_follow_changed(GtkCheckButton *button,
                                            gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;
    GswordWindow *self = g_object_get_data(G_OBJECT(pane->root),
                                           "workspace-window");

    if (pane->updating || self == NULL)
        return;
    if (gtk_check_button_get_active(button)) {
        GswordWindowPrivate *priv = gsword_window_get_instance_private(
            GSWORD_WINDOW(self));

        if (priv->workspace_reference != NULL)
            gsword_window_workspace_pane_set_reference(
                pane, priv->workspace_reference);
    }
    gsword_window_workspace_save(GSWORD_WINDOW(self));
}

static void
gsword_window_workspace_pane_clicked(GtkGestureClick *gesture, gint n_press,
                                     gdouble x, gdouble y, gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;
    GswordWindow *self = g_object_get_data(G_OBJECT(pane->root),
                                           "workspace-window");
    GswordWindowPrivate *priv;
    GtkTextView *view;
    GtkTextBuffer *buffer;
    GtkTextIter iter;
    GtkTextIter line_start;
    GtkTextIter line_end;
    gchar *line;
    GError *error = NULL;
    gint verse;
    gint bx;
    gint by;
    GswModuleInfo *info;

    (void)n_press;
    if (self == NULL || pane->module == NULL)
        return;
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture)) !=
        GDK_BUTTON_PRIMARY)
        return;
    info = gsw_module_get_info(pane->module);
    if (gsword_window_resource_type(info) != GSW_MODULE_TYPE_BIBLES)
        return;
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
    gsword_window_workspace_pane_render(pane);
    {
        GswVerseKey *key = (GswVerseKey *)gsw_module_get_key(pane->module);
        gchar *reference = g_strdup_printf(
            "%s %d:%d", gsw_verse_key_get_book_name(key),
            gsw_verse_key_get_chapter(key), gsw_verse_key_get_verse(key));

        gsword_window_workspace_sync_reference(self, reference, pane);
        g_free(reference);
    }
    priv = gsword_window_get_instance_private(self);
    if (g_strcmp0(gtk_stack_get_visible_child_name(priv->content_stack),
                  "panes") == 0)
        gsword_window_set_status(self, _("Workspace pane updated"));
    gsword_window_workspace_save(self);
}

static void
gsword_window_workspace_pane_close(GtkButton *button, gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;
    GswordWindow *self = g_object_get_data(G_OBJECT(pane->root),
                                           "workspace-window");
    GswordWindowPrivate *priv;
    guint i;
    GtkWidget *root;

    (void)button;
    if (self == NULL)
        return;
    priv = gsword_window_get_instance_private(self);
    for (i = 0; i < priv->workspace_panes->len; i++) {
        if (g_ptr_array_index(priv->workspace_panes, i) == pane) {
            root = g_object_ref(pane->root);
            if (gtk_widget_get_parent(root) ==
                GTK_WIDGET(priv->workspace_grid))
                gtk_grid_remove(priv->workspace_grid, root);
            g_ptr_array_remove_index(priv->workspace_panes, i);
            g_object_unref(root);
            break;
        }
    }
    gsword_window_workspace_reflow(self);
    gsword_window_workspace_save(self);
}

static void
gsword_window_workspace_pane_drag_begin(GtkGestureDrag *gesture, gdouble x,
                                        gdouble y, gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;
    GswordWindow *self = g_object_get_data(G_OBJECT(pane->root),
                                           "workspace-window");

    (void)gesture;
    (void)x;
    (void)y;
    (void)self;
}

static void
gsword_window_workspace_pane_drag_update(GtkGestureDrag *gesture, gdouble x,
                                         gdouble y, gpointer user_data)
{
    GswordWorkspacePane *pane = user_data;
    GswordWindow *self = g_object_get_data(G_OBJECT(pane->root),
                                           "workspace-window");
    gdouble dx = 0.0;
    gdouble dy = 0.0;

    (void)x;
    (void)y;
    gtk_gesture_drag_get_offset(gesture, &dx, &dy);
    if ((dx * dx + dy * dy) < 16.0 || self == NULL)
        return;
    gtk_widget_add_css_class(pane->root, "dragging");
    gsword_window_set_status(self, _("Drag to another pane to rearrange"));
}

static void
gsword_window_workspace_pane_drag_end(GtkGestureDrag *gesture, gdouble x,
                                      gdouble y, gpointer user_data)
{
    GswordWorkspacePane *source = user_data;
    GswordWindow *self = g_object_get_data(G_OBJECT(source->root),
                                           "workspace-window");
    GswordWindowPrivate *priv;
    graphene_rect_t source_bounds;
    gdouble dx = 0.0;
    gdouble dy = 0.0;
    gfloat px;
    gfloat py;
    guint source_index = G_MAXUINT;
    guint target_index = G_MAXUINT;
    guint i;

    (void)x;
    (void)y;
    gtk_widget_remove_css_class(source->root, "dragging");
    if (self == NULL)
        return;
    priv = gsword_window_get_instance_private(self);
    if (!gtk_widget_compute_bounds(source->root,
                                   GTK_WIDGET(priv->workspace_grid),
                                   &source_bounds))
        return;
    gtk_gesture_drag_get_offset(gesture, &dx, &dy);
    px = source_bounds.origin.x + source_bounds.size.width / 2.0f + (gfloat)dx;
    py = source_bounds.origin.y + source_bounds.size.height / 2.0f + (gfloat)dy;

    for (i = 0; i < priv->workspace_panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(priv->workspace_panes, i);
        graphene_rect_t bounds;

        if (pane == source) {
            source_index = i;
            continue;
        }
        if (!gtk_widget_compute_bounds(pane->root,
                                       GTK_WIDGET(priv->workspace_grid),
                                       &bounds))
            continue;
        if (graphene_rect_contains_point(&bounds,
                                         &GRAPHENE_POINT_INIT(px, py)))
            target_index = i;
    }
    if (source_index == G_MAXUINT || target_index == G_MAXUINT)
        return;
    {
        gpointer tmp = g_ptr_array_index(priv->workspace_panes, source_index);

        g_ptr_array_index(priv->workspace_panes, source_index) =
            g_ptr_array_index(priv->workspace_panes, target_index);
        g_ptr_array_index(priv->workspace_panes, target_index) = tmp;
    }
    gsword_window_workspace_reflow(self);
    gsword_window_workspace_save(self);
    gsword_window_set_status(self, _("Pane order changed"));
}

static void
gsword_window_workspace_pane_setup_drag(GswordWorkspacePane *pane,
                                        GtkWidget *handle)
{
    GtkGesture *gesture = gtk_gesture_drag_new();

    gtk_widget_set_can_target(handle, TRUE);
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture),
                                  GDK_BUTTON_PRIMARY);
    g_signal_connect(gesture, "drag-begin",
                     G_CALLBACK(gsword_window_workspace_pane_drag_begin),
                     pane);
    g_signal_connect(gesture, "drag-update",
                     G_CALLBACK(gsword_window_workspace_pane_drag_update),
                     pane);
    g_signal_connect(gesture, "drag-end",
                     G_CALLBACK(gsword_window_workspace_pane_drag_end), pane);
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(gesture), GTK_PHASE_CAPTURE);
    gtk_widget_add_controller(pane->header, GTK_EVENT_CONTROLLER(gesture));
}

static void
gsword_window_workspace_pane_scrolled(GtkAdjustment *adjustment,
                                      gpointer user_data)
{
    GswordWorkspacePane *source = user_data;
    GswordWindow *self = g_object_get_data(G_OBJECT(source->root),
                                           "workspace-window");
    GswordWindowPrivate *priv;
    guint i;

    if (self == NULL)
        return;
    priv = gsword_window_get_instance_private(self);
    if (priv->workspace_syncing_scroll ||
        !priv->workspace_sync_scroll_enabled)
        return;
    priv->workspace_syncing_scroll = TRUE;
    for (i = 0; i < priv->workspace_panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(priv->workspace_panes, i);
        GtkAdjustment *target;

        if (pane == source || pane->scroll == NULL)
            continue;
        target = gtk_scrolled_window_get_vadjustment(pane->scroll);
        if (target != NULL)
            gsword_window_sync_adjustment(adjustment, target);
    }
    priv->workspace_syncing_scroll = FALSE;
}

static void
gsword_window_workspace_sync_scroll_toggled(GtkCheckButton *button,
                                            gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    priv->workspace_sync_scroll_enabled =
        gtk_check_button_get_active(button);
    if (!priv->workspace_restoring)
        gsword_window_workspace_save(self);
}

static void
gsword_window_workspace_set_sync_scroll(GswordWindow *self, gboolean enabled)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    priv->workspace_sync_scroll_enabled = enabled;
    if (priv->workspace_sync_scroll_toggle != NULL) {
        gboolean restoring = priv->workspace_restoring;

        priv->workspace_restoring = TRUE;
        gtk_check_button_set_active(priv->workspace_sync_scroll_toggle,
                                    enabled);
        priv->workspace_restoring = restoring;
    }
}

static GswordWorkspacePane *
gsword_window_workspace_add_pane(GswordWindow *self, const gchar *module_name,
                                 const gchar *key, gboolean follow,
                                 gboolean save)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswordWorkspacePane *pane = g_new0(GswordWorkspacePane, 1);
    GtkWidget *header;
    GtkWidget *handle;
    GtkWidget *close;
    GtkWidget *key_box;
    GtkWidget *scrolled;
    GtkGesture *click;
    gchar *initial_key = key != NULL ? g_strdup(key)
                                     : gsword_window_current_start_reference(self);

    pane->id = workspace_pane_next_id++;
    pane->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    g_object_set_data(G_OBJECT(pane->root), "workspace-window", self);
    g_object_set_data(G_OBJECT(pane->root), "workspace-pane", pane);
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
    gtk_widget_set_tooltip_text(handle, _("Drag to rearrange pane"));
    gtk_widget_set_cursor_from_name(handle, "move");
    gtk_widget_set_size_request(handle, 20, -1);
    gtk_box_append(GTK_BOX(handle),
                   gtk_image_new_from_icon_name("list-drag-handle-symbolic"));
    gtk_box_append(GTK_BOX(header), handle);

    pane->module_selector = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    gtk_widget_set_hexpand(GTK_WIDGET(pane->module_selector), TRUE);
    g_signal_connect(pane->module_selector, "notify::selected",
                     G_CALLBACK(gsword_window_workspace_pane_module_changed),
                     pane);
    gtk_box_append(GTK_BOX(header), GTK_WIDGET(pane->module_selector));

    pane->follow = GTK_TOGGLE_BUTTON(
        gtk_check_button_new_with_label(_("Follow")));
    gtk_check_button_set_active(GTK_CHECK_BUTTON(pane->follow), follow);
    gtk_widget_set_tooltip_text(GTK_WIDGET(pane->follow),
                                _("Follow the active passage"));
    g_signal_connect(pane->follow, "toggled",
                     G_CALLBACK(gsword_window_workspace_pane_follow_changed),
                     pane);
    gtk_box_append(GTK_BOX(header), GTK_WIDGET(pane->follow));

    close = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(close, "flat");
    gtk_widget_set_tooltip_text(close, _("Close pane"));
    g_signal_connect(close, "clicked",
                     G_CALLBACK(gsword_window_workspace_pane_close), pane);
    gtk_box_append(GTK_BOX(header), close);

    key_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_start(key_box, 6);
    gtk_widget_set_margin_end(key_box, 6);
    gtk_widget_set_margin_bottom(key_box, 4);
    {
        gchar *locale = gsword_window_locale_name();

        pane->reference_chooser = GSWORD_REFERENCE_CHOOSER(
            gsword_reference_chooser_new_compact("KJV", locale));
        g_free(locale);
    }
    gtk_widget_set_hexpand(GTK_WIDGET(pane->reference_chooser), FALSE);
    gtk_widget_set_halign(GTK_WIDGET(pane->reference_chooser),
                          GTK_ALIGN_START);
    g_signal_connect(pane->reference_chooser, "activate",
                     G_CALLBACK(
                         gsword_window_workspace_pane_reference_activate),
                     pane);
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
                     "value-changed",
                     G_CALLBACK(gsword_window_workspace_pane_scrolled), pane);

    click = gtk_gesture_click_new();
    g_signal_connect(click, "pressed",
                     G_CALLBACK(gsword_window_workspace_pane_clicked), pane);
    gtk_widget_add_controller(GTK_WIDGET(pane->view),
                              GTK_EVENT_CONTROLLER(click));

    gtk_box_append(GTK_BOX(pane->root), header);
    gtk_box_append(GTK_BOX(pane->root), key_box);
    gtk_box_append(GTK_BOX(pane->root), scrolled);

    g_ptr_array_add(priv->workspace_panes, pane);
    gsword_window_workspace_pane_fill_modules(pane);
    gsword_window_workspace_pane_set_module(pane, module_name,
                                            initial_key != NULL
                                            ? initial_key : "");
    gsword_window_workspace_pane_setup_drag(pane, handle);
    g_free(initial_key);
    gsword_window_workspace_reflow(self);
    if (save)
        gsword_window_workspace_save(self);
    return pane;
}

static void
gsword_window_workspace_add_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    gchar *name = gsword_window_workspace_default_module(self);
    gchar *reference = gsword_window_current_start_reference(self);

    (void)button;
    gsword_window_workspace_add_pane(self, name, reference, TRUE, TRUE);
    g_free(name);
    g_free(reference);
}

static void
gsword_window_workspace_clear_panes(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    while (priv->workspace_panes->len > 0) {
        GswordWorkspacePane *pane = g_ptr_array_index(
            priv->workspace_panes, priv->workspace_panes->len - 1);
        GtkWidget *root = g_object_ref(pane->root);

        if (gtk_widget_get_parent(root) ==
            GTK_WIDGET(priv->workspace_grid))
            gtk_grid_remove(priv->workspace_grid, root);
        g_ptr_array_remove_index(priv->workspace_panes,
                                  priv->workspace_panes->len - 1);
        g_object_unref(root);
    }
    gsword_window_workspace_reflow(self);
}

static void
gsword_window_workspace_clear_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);

    (void)button;
    gsword_window_workspace_clear_panes(self);
    gsword_window_workspace_save(self);
}

static gboolean
gsword_window_is_bible_module(GswordWindow *self, const gchar *name)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *infos;
    gboolean result = FALSE;
    guint i;

    if (name == NULL || priv->manager == NULL)
        return FALSE;
    infos = gsw_manager_get_module_infos(priv->manager);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (g_strcmp0(gsw_module_info_get_name(info), name) == 0) {
            result = gsword_window_resource_type(info) ==
                     GSW_MODULE_TYPE_BIBLES &&
                     gsword_window_workspace_pane_supported(info);
            break;
        }
    }
    g_ptr_array_unref(infos);
    return result;
}

static gchar *
gsword_window_parallel_module(GswordWindow *self, const gchar *saved,
                              guint index)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *infos;
    gchar *result = NULL;
    guint position = 0;
    guint i;

    if (saved != NULL && gsword_window_is_bible_module(self, saved))
        return g_strdup(saved);
    if (priv->manager == NULL)
        return NULL;
    infos = gsw_manager_get_module_infos(priv->manager);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (gsword_window_resource_type(info) != GSW_MODULE_TYPE_BIBLES ||
            !gsword_window_workspace_pane_supported(info))
            continue;
        if (position == index) {
            result = g_strdup(gsw_module_info_get_name(info));
            break;
        }
        position++;
    }
    g_ptr_array_unref(infos);
    return result;
}

static void
gsword_window_apply_parallel_template(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gchar *saved_a = NULL;
    gchar *saved_b = NULL;
    gchar *module_a;
    gchar *module_b;
    gchar *reference = gsword_window_current_start_reference(self);

    if (priv->workspace_panes == NULL)
        return;
    if (priv->settings != NULL) {
        saved_a = g_settings_get_string(priv->settings, "parallel-a");
        saved_b = g_settings_get_string(priv->settings, "parallel-b");
    }
    module_a = gsword_window_parallel_module(self, saved_a, 0);
    module_b = gsword_window_parallel_module(self, saved_b, 1);
    if (module_a == NULL) {
        gsword_window_set_status(self, _("No Bible text module is installed"));
        g_free(saved_a);
        g_free(saved_b);
        g_free(module_b);
        g_free(reference);
        return;
    }
    if (module_b == NULL)
        module_b = g_strdup(module_a);

    gsword_window_workspace_clear_panes(self);
    gsword_window_workspace_add_pane(self, module_a, reference, TRUE, FALSE);
    gsword_window_workspace_add_pane(self, module_b, reference, TRUE, FALSE);

    priv->workspace_restoring = TRUE;
    if (priv->workspace_columns != NULL)
        gtk_spin_button_set_value(priv->workspace_columns, 2);
    priv->workspace_restoring = FALSE;
    gsword_window_workspace_set_sync_scroll(self, TRUE);
    if (priv->settings != NULL) {
        g_settings_set_string(priv->settings, "parallel-a", module_a);
        g_settings_set_string(priv->settings, "parallel-b", module_b);
    }
    gsword_window_workspace_reflow(self);
    gsword_window_workspace_save(self);
    gsword_window_show_page(self, "panes");
    gsword_window_workspace_sync_from_reader(self);
    gsword_window_set_status(self, _("Parallel workspace ready"));

    g_free(saved_a);
    g_free(saved_b);
    g_free(module_a);
    g_free(module_b);
    g_free(reference);
}

static void
gsword_window_workspace_columns_changed(GtkSpinButton *spin,
                                        gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    (void)spin;
    if (priv->workspace_restoring)
        return;
    gsword_window_workspace_reflow(self);
    gsword_window_workspace_save(self);
}

static void
gsword_window_workspace_sync_reference(GswordWindow *self,
                                       const gchar *reference,
                                       GswordWorkspacePane *source)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint i;

    if (reference == NULL || *reference == '\0' || priv->workspace_syncing)
        return;
    priv->workspace_syncing = TRUE;
    g_free(priv->workspace_reference);
    priv->workspace_reference = g_strdup(reference);
    for (i = 0; i < priv->workspace_panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(priv->workspace_panes, i);

        if (pane == source || pane->module == NULL ||
            !gtk_check_button_get_active(GTK_CHECK_BUTTON(pane->follow)))
            continue;
        if (gsw_key_get_kind(gsw_module_get_key(pane->module)) !=
            GSW_KEY_KIND_VERSE)
            continue;
        gsword_window_workspace_pane_set_reference(pane, reference);
    }
    priv->workspace_syncing = FALSE;
}

static void
gsword_window_workspace_sync_from_reader(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gchar *reference;

    if (priv->workspace_panes == NULL || priv->workspace_panes->len == 0 ||
        priv->module == NULL)
        return;
    reference = gsword_window_current_start_reference(self);
    if (reference != NULL) {
        gsword_window_workspace_sync_reference(self, reference, NULL);
        g_free(reference);
    }
}

static void
gsword_window_workspace_restore(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gchar *layout_text = NULL;
    gint columns = 2;
    gboolean created_default = FALSE;

    if (priv->workspace_grid == NULL)
        return;
    if (priv->settings != NULL) {
        layout_text = g_settings_get_string(priv->settings,
                                            "workspace-layout");
        columns = g_settings_get_int(priv->settings, "workspace-columns");
        priv->workspace_sync_scroll_enabled = g_settings_get_boolean(
            priv->settings, "workspace-sync-scroll");
    }
    if (columns < 1 || columns > 4)
        columns = 2;
    priv->workspace_restoring = TRUE;
    if (priv->workspace_columns != NULL)
        gtk_spin_button_set_value(priv->workspace_columns, columns);
    if (priv->workspace_sync_scroll_toggle != NULL)
        gtk_check_button_set_active(priv->workspace_sync_scroll_toggle,
                                    priv->workspace_sync_scroll_enabled);

    if (layout_text == NULL || *layout_text == '\0') {
        gchar *module = gsword_window_workspace_default_module(self);
        gchar *reference = gsword_window_current_start_reference(self);

        gsword_window_workspace_add_pane(self, module, reference, TRUE, FALSE);
        g_free(module);
        g_free(reference);
        created_default = TRUE;
    } else {
        GVariant *layout;
        GVariantIter *iter;
        const gchar *module;
        const gchar *key;
        gboolean follow;

        layout = g_variant_parse(G_VARIANT_TYPE("a(ssb)"), layout_text,
                                 NULL, NULL, NULL);
        if (layout == NULL) {
            gchar *module = gsword_window_workspace_default_module(self);
            gchar *reference = gsword_window_current_start_reference(self);

            gsword_window_workspace_add_pane(self, module, reference, TRUE,
                                             FALSE);
            g_free(module);
            g_free(reference);
            created_default = TRUE;
        } else {
            g_variant_get(layout, "a(ssb)", &iter);
            for (;;) {
                GVariant *item = g_variant_iter_next_value(iter);

                if (item == NULL)
                    break;
                g_variant_get_child(item, 0, "&s", &module);
                g_variant_get_child(item, 1, "&s", &key);
                g_variant_get_child(item, 2, "b", &follow);
                gsword_window_workspace_add_pane(self, module, key, follow,
                                                 FALSE);
                g_variant_unref(item);
            }
            g_variant_iter_free(iter);
            g_variant_unref(layout);
        }
    }
    priv->workspace_restoring = FALSE;
    gsword_window_workspace_reflow(self);
    if (created_default)
        gsword_window_workspace_save(self);
    g_free(layout_text);
}

static GtkWidget *
gsword_window_make_workspace_page(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *add = gtk_button_new_with_label(_("Add pane"));
    GtkWidget *clear = gtk_button_new_with_label(_("Clear panes"));
    GtkWidget *columns_label = gtk_label_new(_("Columns"));
    GtkWidget *sync_scroll = gtk_check_button_new_with_label(_("Sync scroll"));
    GtkWidget *hint = gtk_label_new(_("No panes yet. Add a pane to begin."));
    GtkWidget *scrolled = gtk_scrolled_window_new();

    gtk_widget_set_margin_top(toolbar, 8);
    gtk_widget_set_margin_bottom(toolbar, 8);
    gtk_widget_set_margin_start(toolbar, 12);
    gtk_widget_set_margin_end(toolbar, 12);
    g_signal_connect(add, "clicked",
                     G_CALLBACK(gsword_window_workspace_add_clicked), self);
    g_signal_connect(clear, "clicked",
                     G_CALLBACK(gsword_window_workspace_clear_clicked), self);
    priv->workspace_columns = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(1, 4, 1));
    gtk_spin_button_set_value(priv->workspace_columns, 2);
    g_signal_connect(priv->workspace_columns, "value-changed",
                     G_CALLBACK(gsword_window_workspace_columns_changed), self);
    priv->workspace_sync_scroll_toggle = GTK_CHECK_BUTTON(sync_scroll);
    gtk_widget_set_tooltip_text(sync_scroll,
                                _("Keep pane scroll positions together"));
    g_signal_connect(sync_scroll, "toggled",
                     G_CALLBACK(gsword_window_workspace_sync_scroll_toggled),
                     self);
    gtk_box_append(GTK_BOX(toolbar), add);
    gtk_box_append(GTK_BOX(toolbar), clear);
    gtk_box_append(GTK_BOX(toolbar), columns_label);
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(priv->workspace_columns));
    gtk_box_append(GTK_BOX(toolbar), sync_scroll);

    priv->workspace_panes = g_ptr_array_new_with_free_func(
        gsword_window_workspace_pane_free);
    priv->workspace_grid = GTK_GRID(gtk_grid_new());
    gtk_widget_set_hexpand(GTK_WIDGET(priv->workspace_grid), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(priv->workspace_grid), TRUE);
    gtk_grid_set_column_spacing(priv->workspace_grid, 4);
    gtk_grid_set_row_spacing(priv->workspace_grid, 4);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(priv->workspace_grid));
    gtk_widget_set_vexpand(scrolled, TRUE);

    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_justify(GTK_LABEL(hint), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(hint, "dim-label");
    priv->workspace_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_add_named(priv->workspace_stack, hint, "empty");
    gtk_stack_add_named(priv->workspace_stack, scrolled, "grid");
    gtk_widget_set_vexpand(GTK_WIDGET(priv->workspace_stack), TRUE);
    gtk_stack_set_visible_child_name(priv->workspace_stack, "empty");

    gtk_box_append(GTK_BOX(box), toolbar);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(priv->workspace_stack));
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
        gsword_window_set_status(self, _("Open a module before searching"));
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
        gsword_window_show_error(self, _("Search failed"), error);
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
        gchar *message = g_strdup_printf(_("Search: %u result(s)"), results->len);

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
    gsword_window_show_page(self, "reader");
}

static void
gsword_window_index_progress(const gchar *module_name, guint processed,
                             guint total, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    gchar *message = g_strdup_printf(_("Indexing %s: %u/%u"),
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
        gtk_label_set_text(priv->index_label, _("Index: no module"));
    else
        gtk_label_set_text(priv->index_label,
            gsw_module_has_search_index(priv->module)
            ? _("Index: available") : _("Index: not built"));
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
        gsword_window_show_error(self, _("Cannot build index"), error);
        return;
    }
    gsword_window_set_status(self, _("Search index built"));
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
        gsword_window_show_error(self, _("Cannot delete index"), error);
        return;
    }
    gsword_window_set_status(self, _("Search index deleted"));
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
    GtkWidget *search = gtk_button_new_with_label(_("Search"));
    GtkWidget *build = gtk_button_new_from_icon_name(
        "document-save-symbolic");
    GtkWidget *delete = gtk_button_new_from_icon_name("edit-delete-symbolic");
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkStringList *types;

    gtk_widget_set_margin_top(controls, 8);
    gtk_widget_set_margin_bottom(controls, 8);
    gtk_widget_set_margin_start(controls, 12);
    gtk_widget_set_margin_end(controls, 12);
    gtk_widget_set_tooltip_text(build, _("Build search index"));
    gtk_widget_set_tooltip_text(delete, _("Delete search index"));

    priv->search_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->search_entry,
                                   _("Search text"));
    gtk_widget_set_hexpand(GTK_WIDGET(priv->search_entry), TRUE);
    priv->search_type = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    types = gtk_string_list_new(
        (const gchar *[]) { _("Phrase"), _("Multiword"), _("Regex"), NULL });
    gtk_drop_down_set_model(priv->search_type, G_LIST_MODEL(types));
    g_object_unref(types);
    priv->search_case = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label(_("Case sensitive")));
    priv->index_label = GTK_LABEL(gtk_label_new(_("Index: no module")));
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
        return _("Not installed");
    if (status & GSW_MODULE_STATUS_UPDATED)
        return _("Update available");
    if (status & GSW_MODULE_STATUS_OLDER)
        return _("Installed - local version is newer");
    if (status & GSW_MODULE_STATUS_SAME_VERSION)
        return _("Installed - up to date");
    return _("Installed");
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
        gsword_window_set_status(self, _("Install manager is not available"));
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
            ? gsw_install_source_get_caption(source) : _("Source"));
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
        gsword_window_module_type_display(info),
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
    gtk_label_set_text(priv->remote_title, _("Select a module"));
    gtk_label_set_text(priv->remote_description, "");
    gtk_label_set_text(priv->remote_status, "");
    gtk_widget_set_sensitive(GTK_WIDGET(priv->install_button), FALSE);
    if (source == NULL) {
        gtk_label_set_text(priv->remote_status,
                           _("No source selected. Refresh the source list first."));
        return;
    }
    infos = gsw_install_manager_get_remote_module_infos(
        priv->installer, source, &error);
    if (infos == NULL) {
        gsword_window_show_error(self, _("Cannot read remote modules"), error);
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
    gsword_window_set_status(self, _("Refreshing install sources..."));
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
        gsword_window_show_error(self, _("Cannot refresh install sources"), error);
    } else {
        gsword_window_set_status(self, _("Install sources refreshed"));
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
    gsword_window_set_status(self, _("Refreshing remote module list..."));
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
        gsword_window_show_error(self, _("Cannot refresh remote modules"), error);
    } else {
        gsword_window_set_status(self, _("Remote module list refreshed"));
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
    gsword_window_set_status(self, _("Installing module..."));
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
        gsword_window_show_error(self, _("Module install failed"), error);
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
    gsword_window_show_page(self, "library");
    if (installed && installed_name != NULL) {
        gchar *message = g_strdup_printf(_("Installed module %s"),
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
    gsword_window_set_status(self, _("Removing module..."));
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
        gsword_window_show_error(self, _("Module removal failed"), error);
    } else {
        gsword_window_set_status(self, _("Module removed"));
    }
    g_clear_object(&priv->install_cancellable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->remove_button), TRUE);
    if (priv->manager != NULL)
        gsw_manager_reload(priv->manager, NULL);
    gsword_window_populate_modules(self);
    gsword_window_refresh_installed_dropdown(self);
    gsword_window_refresh_remote_modules(self);
    gsword_window_show_page(self, "library");
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
    gsword_window_set_status(self, _("Cancelling..."));
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
                       message != NULL ? message : _("Installing..."));
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
        status = g_strdup_printf("%s: %" G_GUINT64_FORMAT
                                 " / %" G_GUINT64_FORMAT " %s",
                                 _("Downloading"), done, total, _("bytes"));
    } else {
        gtk_progress_bar_pulse(priv->install_progress);
        status = g_strdup_printf("%s: %" G_GUINT64_FORMAT " %s",
                                 _("Downloading"), done, _("bytes"));
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
            _("Enter a local SWORD directory and module name"));
        return;
    }
    if (!gsw_install_manager_install_module_from_directory(
            priv->installer, priv->manager, directory, name, &error)) {
        gsword_window_show_error(self, _("Local install failed"), error);
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
    gsword_window_show_page(self, "library");
    gsword_window_set_status(self, _("Module installed from local directory"));
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
    GtkWidget *install = gtk_button_new_with_label(_("Install / Update"));
    GtkWidget *remove = gtk_button_new_with_label(_("Remove"));
    GtkWidget *cancel = gtk_button_new_with_label(_("Cancel"));
    GtkWidget *installed_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *local_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *local_label = gtk_label_new(_("Install from local directory"));

    gtk_widget_set_margin_top(controls, 8);
    gtk_widget_set_margin_bottom(controls, 8);
    gtk_widget_set_margin_start(controls, 12);
    gtk_widget_set_margin_end(controls, 12);

    priv->source_dropdown = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    priv->install_search = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->install_search, _("Filter modules"));
    gtk_widget_set_hexpand(GTK_WIDGET(priv->install_search), TRUE);
    gtk_widget_set_tooltip_text(refresh_sources,
                                _("Refresh the install source list"));
    gtk_widget_set_tooltip_text(refresh_remote,
                                _("Refresh modules from the selected source"));
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

    priv->remote_title = GTK_LABEL(gtk_label_new(_("Select a module")));
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
    gtk_box_append(GTK_BOX(installed_row), gtk_label_new(_("Installed:")));
    gtk_box_append(GTK_BOX(installed_row), GTK_WIDGET(priv->installed_dropdown));
    gtk_box_append(GTK_BOX(installed_row), remove);
    gtk_box_append(GTK_BOX(right), installed_row);

    gtk_label_set_xalign(GTK_LABEL(local_label), 0.0f);
    gtk_widget_add_css_class(local_label, "heading");
    priv->local_dir_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->local_dir_entry,
                                   _("Local SWORD library directory"));
    priv->local_name_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->local_name_entry, _("Module name"));
    priv->local_install_button = GTK_BUTTON(
        gtk_button_new_with_label(_("Install local module")));
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
    header_title = adw_window_title_new("gsword", _("SWORD study"));
    adw_header_bar_set_title_widget(header, header_title);

    menu_button = gtk_button_new_from_icon_name("open-menu-symbolic");
    gtk_widget_add_css_class(menu_button, "flat");
    g_signal_connect(menu_button, "clicked",
                     G_CALLBACK(on_main_menu_clicked), self);
    gtk_widget_set_tooltip_text(menu_button, _("Main menu"));
    adw_header_bar_pack_end(header, menu_button);

    split = ADW_NAVIGATION_SPLIT_VIEW(adw_navigation_split_view_new());
    adw_navigation_split_view_set_sidebar_width_fraction(split, 0.24);
    sidebar_page = adw_navigation_page_new(gsword_window_make_sidebar(self),
                                           _("Library"));

    priv->content_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_reader(self),
                         "reader", _("Reading"));
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_workspace_page(self),
                         "panes", _("Panes"));
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_search_page(self),
                         "search", _("Search"));
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_notes_page(self),
                         "notes", _("Notes"));
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_plan_page(self),
                         "plan", _("Plan"));
    gtk_stack_add_named(priv->content_stack,
                        gsword_window_make_teaching_page(self),
                        "teaching");
    gtk_stack_add_named(priv->content_stack,
                        gsword_window_make_resource_page(self),
                        "resource");
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_library_page(self),
                         "library", _("Library"));
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

    content_page = adw_navigation_page_new(workspace_box, _("Workspace"));
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
    gsword_window_services_init(self, priv->status_label, priv->content_stack);

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
        gchar *study_module;
        gchar *study_lexicon;
        gchar *pinned_key;
        gboolean follow;
        gboolean visible;

        priv->font_scale = g_settings_get_double(priv->settings, "font-scale");
        if (priv->font_scale <= 0.0)
            priv->font_scale = 1.0;
        g_free(priv->theme);
        priv->theme = g_settings_get_string(priv->settings, "theme");
        if (priv->theme == NULL || *priv->theme == '\0') {
            g_free(priv->theme);
            priv->theme = g_strdup("system");
        }

        study_module = g_settings_get_string(priv->settings, "study-module");
        if (study_module != NULL && *study_module != '\0') {
            g_free(priv->commentary_name);
            priv->commentary_name = g_strdup(study_module);
        }
        study_lexicon = g_settings_get_string(priv->settings, "study-lexicon");
        if (study_lexicon != NULL && *study_lexicon != '\0') {
            g_free(priv->lexicon_name);
            priv->lexicon_name = g_strdup(study_lexicon);
        }
        follow = g_settings_get_boolean(priv->settings, "study-follow");
        visible = g_settings_get_boolean(priv->settings, "study-visible");
        pinned_key = g_settings_get_string(priv->settings,
                                           "study-pinned-key");
        priv->commentary_follow = follow;
        if (priv->study_follow_toggle != NULL)
            gtk_check_button_set_active(priv->study_follow_toggle, follow);
        if (priv->study_toggle != NULL)
            gtk_toggle_button_set_active(priv->study_toggle, visible);
        if (!follow && pinned_key != NULL && *pinned_key != '\0') {
            g_free(priv->pinned_commentary_key);
            priv->pinned_commentary_key = g_strdup(pinned_key);
            g_settings_set_string(priv->settings, "study-pinned-key",
                                  pinned_key);
        }
        g_free(study_module);
        g_free(study_lexicon);
        g_free(pinned_key);
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
    g_clear_object(&priv->lexicon_names);
    g_clear_pointer(&priv->devotion_date, g_date_time_unref);
    g_clear_pointer(&priv->memory_items, g_ptr_array_unref);
    g_clear_pointer(&priv->pinned_commentary_key, g_free);
    g_clear_pointer(&priv->memory_module, g_free);
    g_clear_pointer(&priv->memory_key, g_free);
    g_clear_pointer(&priv->memory_display, g_free);
    g_clear_pointer(&priv->memory_text, g_free);
    g_clear_pointer(&priv->teaching_module, g_free);
    g_clear_pointer(&priv->teaching_reference, g_free);
    g_clear_pointer(&priv->workspace_reference, g_free);
    g_clear_pointer(&priv->workspace_panes, g_ptr_array_unref);
    g_clear_object(&priv->resource_module);
    g_clear_pointer(&priv->resource_data_dir, g_free);
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
    gsword_window_refresh_lexicon_modules(self);
    gsword_window_populate_modules(self);
    gsword_window_workspace_restore(self);
    gsword_window_refresh_notes(self);
    gsword_window_refresh_install_sources(self);
    gsword_window_refresh_installed_dropdown(self);
    gsword_window_restore_page(self);
    return GTK_WIDGET(self);
}
