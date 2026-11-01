#include "window.h"
#include "notes-store.h"
#include "plan-store.h"
#include "app-paths.h"
#include "gsword-reference-chooser.h"
#include "search-page.h"
#include "teaching-page.h"
#include "resource-page.h"
#include "notes-page.h"
#include "plan-page.h"
#include "library-page.h"
#include "window-private.h"

#include <glib/gi18n.h>

#include <string.h>

struct _GswordWindow {
    AdwApplicationWindow parent_instance;
};

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

    GswordSearchPage *search_page;

    GswordLibraryPage *library_page;

    gboolean selecting_module;

    GswordPlanStore *plans;
    GswordPlanPage *plan_page;

    GswordTeachingPage *teaching_page;
    GswordResourcePage *resource_page;

    GswordNotesStore *notes;
    GswordNotesPage *notes_page;
    GtkTextTag *highlight_tag;
    GtkTextTag *active_verse_tag;
    GtkTextTag *range_tag;
    gint active_verse;
    gint range_start;
    gint range_end;
    gboolean range_active;
} GswordWindowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GswordWindow, gsword_window, ADW_TYPE_APPLICATION_WINDOW)

static void gsword_window_apply_active_verse(GswordWindow *self);
static void gsword_window_clear_range(GswordWindow *self);
static void gsword_window_apply_range(GswordWindow *self);
static void gsword_window_select_verse_range(GswordWindow *self, gint anchor,
                                             gint verse);
static void gsword_window_select_verse(GswordWindow *self, gint verse);
static void gsword_window_update_reference_status(GswordWindow *self);
static void gsword_window_apply_highlights(GswordWindow *self);
static void gsword_window_load_module(GswordWindow *self, const gchar *name);
static void gsword_window_refresh_lexicon_modules(GswordWindow *self);
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

void
gsword_window_clear_list(GtkListBox *list)
{
    while (gtk_widget_get_first_child(GTK_WIDGET(list)) != NULL)
        gtk_list_box_remove(list, gtk_widget_get_first_child(GTK_WIDGET(list)));
}

GswModule *
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

gboolean
gsword_window_open_module_reference(GswordWindow *self,
                                    const gchar *module_name,
                                    const gchar *reference)
{
    GswModule *module;

    if (module_name != NULL && *module_name != '\0') {
        module = gsword_window_get_module(self);
        if (module == NULL ||
            g_strcmp0(gsw_module_get_name(module), module_name) != 0)
            gsword_window_load_module(self, module_name);
    }
    if (reference == NULL || *reference == '\0')
        return FALSE;
    if (!gsword_window_set_key_text(self, reference))
        return FALSE;
    gsword_window_show_page(self, "reader");
    return TRUE;
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
on_teaching_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    (void)button;
    if (priv->teaching_page != NULL)
        gsword_teaching_page_load(priv->teaching_page);
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
        if (priv->search_page != NULL)
            gsword_search_page_refresh_index(priv->search_page);
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
    if (priv->search_page != NULL)
        gsword_search_page_refresh_index(priv->search_page);
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
            if (priv->plan_page != NULL)
                gsword_plan_page_show_devotion(priv->plan_page);
            return;
        }
        if (type == GSW_MODULE_TYPE_GENBOOKS ||
            type == GSW_MODULE_TYPE_MAPS) {
            if (priv->resource_page != NULL)
                gsword_resource_page_open(priv->resource_page, name,
                                          type == GSW_MODULE_TYPE_MAPS);
            return;
        }
    }

    gsword_window_load_module(self, name);
    gsword_window_show_page(self, "reader");
}

const gchar *
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

void
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
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    (void)button;
    if (priv->notes_page != NULL)
        gsword_notes_page_open_current(priv->notes_page);
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
        if (priv->teaching_page != NULL)
            gsword_teaching_page_load(priv->teaching_page);
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
        if (priv->search_page != NULL)
            gsword_search_page_refresh_index(priv->search_page);
    } else if (g_strcmp0(name, "notes") == 0) {
        if (priv->notes_page != NULL) {
            gsword_notes_page_refresh(priv->notes_page);
            if (!gsword_notes_page_has_note_key(priv->notes_page) &&
                priv->module != NULL)
                gsword_notes_page_open_current(priv->notes_page);
        }
    } else if (g_strcmp0(name, "plan") == 0) {
        if (priv->plan_page != NULL) {
            gsword_plan_page_refresh(priv->plan_page);
            gsword_plan_page_refresh_devotion(priv->plan_page);
            gsword_plan_page_refresh_memory(priv->plan_page);
        }
    } else if (g_strcmp0(name, "teaching") == 0) {
        if (priv->teaching_page != NULL)
            gsword_teaching_page_load(priv->teaching_page);
    } else if (g_strcmp0(name, "library") == 0) {
        if (priv->library_page != NULL) {
            gsword_library_page_refresh_sources(priv->library_page);
            gsword_library_page_refresh_installed(priv->library_page);
            gsword_library_page_refresh_remote(priv->library_page);
        }
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

    priv->search_page = gsword_search_page_new(self);
    priv->teaching_page = gsword_teaching_page_new(self);
    priv->resource_page = gsword_resource_page_new(self);
    priv->notes_page = gsword_notes_page_new(self);
    priv->plan_page = gsword_plan_page_new(self);
    priv->library_page = gsword_library_page_new(self);
    priv->content_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_reader(self),
                         "reader", _("Reading"));
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_workspace_page(self),
                         "panes", _("Panes"));
    gtk_stack_add_titled(priv->content_stack,
                         gsword_search_page_widget(priv->search_page),
                         "search", _("Search"));
    gtk_stack_add_titled(priv->content_stack,
                         gsword_notes_page_widget(priv->notes_page),
                         "notes", _("Notes"));
    gtk_stack_add_titled(priv->content_stack,
                         gsword_plan_page_widget(priv->plan_page),
                         "plan", _("Plan"));
    gtk_stack_add_named(priv->content_stack,
                        gsword_teaching_page_widget(priv->teaching_page),
                        "teaching");
    gtk_stack_add_named(priv->content_stack,
                        gsword_resource_page_widget(priv->resource_page),
                        "resource");
    gtk_stack_add_titled(priv->content_stack,
                         gsword_library_page_widget(priv->library_page),
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
    g_clear_object(&priv->lexicon_names);
    g_clear_pointer(&priv->pinned_commentary_key, g_free);
    g_clear_pointer(&priv->workspace_reference, g_free);
    g_clear_pointer(&priv->workspace_panes, g_ptr_array_unref);
    g_free(priv->theme);

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
    gsword_window_build(self);
    gsword_window_load_settings(self);
    gsword_window_refresh_lexicon_modules(self);
    gsword_window_populate_modules(self);
    gsword_window_workspace_restore(self);
    if (priv->notes_page != NULL)
        gsword_notes_page_refresh(priv->notes_page);
    gsword_library_page_refresh_sources(priv->library_page);
    gsword_library_page_refresh_installed(priv->library_page);
    gsword_window_restore_page(self);
    return GTK_WIDGET(self);
}
