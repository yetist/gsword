#include "window.h"
#include "notes-store.h"
#include "plan-store.h"
#include "search-page.h"
#include "teaching-page.h"
#include "resource-page.h"
#include "notes-page.h"
#include "plan-page.h"
#include "library-page.h"
#include "module-sidebar.h"
#include "study-panel.h"
#include "workspace-page.h"
#include "reader-page.h"
#include "preferences-dialog.h"
#include "window-private.h"

#include <glib/gi18n.h>

#define GSWORD_WINDOW_MIN_WIDTH 1100

struct _GswordWindow {
    AdwApplicationWindow parent_instance;
};

typedef struct {
    GswManager *manager;
    GSettings *settings;
    GswModule *module;
    GswordModuleSidebar *module_sidebar;
    AdwOverlaySplitView *split_view;
    GtkToggleButton *sidebar_button;
    gboolean sidebar_visible;
    gboolean sidebar_updating;
    GswordWorkspacePage *workspace_page;
    GswordReaderPage *reader_page;
    AdwViewStack *content_stack;
    gboolean page_restored;
    GtkLabel *status_label;

    GswordSearchPage *search_page;
    gchar *search_return_page;

    GswordLibraryPage *library_page;

    GswordPlanStore *plans;
    GswordPlanPage *plan_page;

    GswordTeachingPage *teaching_page;
    GswordResourcePage *resource_page;

    GswordNotesStore *notes;
    GswordNotesPage *notes_page;
} GswordWindowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GswordWindow, gsword_window, ADW_TYPE_APPLICATION_WINDOW)

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
update_sidebar_button(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gboolean visible;

    if (priv->split_view == NULL || priv->sidebar_button == NULL)
        return;
    visible = adw_overlay_split_view_get_show_sidebar(priv->split_view);
    priv->sidebar_updating = TRUE;
    gtk_toggle_button_set_active(priv->sidebar_button, visible);
    priv->sidebar_updating = FALSE;
    gtk_widget_set_tooltip_text(
        GTK_WIDGET(priv->sidebar_button),
        visible ? _("Hide module sidebar") : _("Show module sidebar"));
}

static void
set_sidebar_visible(GswordWindow *self, gboolean visible, gboolean persist)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->split_view == NULL)
        return;
    adw_overlay_split_view_set_show_sidebar(priv->split_view, visible);
    if (persist &&
        !adw_overlay_split_view_get_collapsed(priv->split_view)) {
        priv->sidebar_visible = visible;
        if (priv->settings != NULL)
            g_settings_set_boolean(priv->settings, "sidebar-visible",
                                   visible);
    }
    update_sidebar_button(self);
}

static void
on_sidebar_visibility_changed(GObject *object, GParamSpec *pspec,
                              gpointer user_data)
{
    (void)object;
    (void)pspec;
    update_sidebar_button(GSWORD_WINDOW(user_data));
}

static void
on_sidebar_collapsed_changed(GObject *object, GParamSpec *pspec,
                             gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    AdwOverlaySplitView *split = ADW_OVERLAY_SPLIT_VIEW(object);
    gboolean collapsed = adw_overlay_split_view_get_collapsed(split);

    (void)pspec;
    if (collapsed)
        set_sidebar_visible(self, FALSE, FALSE);
    else
        set_sidebar_visible(self, priv->sidebar_visible, FALSE);
}

static void
on_sidebar_toggled(GtkToggleButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gboolean collapsed;

    if (priv->sidebar_updating || priv->split_view == NULL)
        return;
    collapsed = adw_overlay_split_view_get_collapsed(priv->split_view);
    set_sidebar_visible(self, gtk_toggle_button_get_active(button),
                        !collapsed);
}

static void
on_toggle_sidebar_action(GSimpleAction *action, GVariant *parameter,
                         gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gboolean visible;
    gboolean collapsed;

    (void)action;
    (void)parameter;
    if (priv->split_view == NULL)
        return;
    visible = adw_overlay_split_view_get_show_sidebar(priv->split_view);
    collapsed = adw_overlay_split_view_get_collapsed(priv->split_view);
    set_sidebar_visible(self, !visible, !collapsed);
}

void
gsword_window_hide_sidebar_overlay(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->split_view == NULL ||
        !adw_overlay_split_view_get_collapsed(priv->split_view) ||
        !adw_overlay_split_view_get_show_sidebar(priv->split_view))
        return;
    adw_overlay_split_view_set_show_sidebar(priv->split_view, FALSE);
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

void
gsword_window_refresh_reader(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->reader_page != NULL)
        gsword_reader_page_refresh(priv->reader_page);
}

gboolean
gsword_window_set_key_text(GswordWindow *self, const gchar *text)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->reader_page == NULL)
        return FALSE;
    return gsword_reader_page_set_key_text(priv->reader_page, text);
}

gchar *
gsword_window_current_storage_key(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    return priv->reader_page != NULL
         ? gsword_reader_page_current_storage_key(priv->reader_page) : NULL;
}

gchar *
gsword_window_current_display_key(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    return priv->reader_page != NULL
         ? gsword_reader_page_current_display_key(priv->reader_page) : NULL;
}

gchar *
gsword_window_current_reference(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    return priv->reader_page != NULL
         ? gsword_reader_page_current_reference(priv->reader_page) : NULL;
}

gchar *
gsword_window_current_start_reference(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    return priv->reader_page != NULL
         ? gsword_reader_page_current_start_reference(priv->reader_page)
         : NULL;
}

void
gsword_window_refresh_search_index(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->search_page != NULL)
        gsword_search_page_refresh_index(priv->search_page);
}

void
gsword_window_sync_workspace_from_reader(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->workspace_page != NULL)
        gsword_workspace_page_sync_from_reader(priv->workspace_page);
}

void
gsword_window_focus_reference(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->reader_page != NULL)
        gsword_reader_page_begin_reference_edit(priv->reader_page);
}

void
gsword_window_open_current_note(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->notes_page != NULL)
        gsword_notes_page_open_current(priv->notes_page);
}

void
gsword_window_open_teaching(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->teaching_page != NULL)
        gsword_teaching_page_load(priv->teaching_page);
}

static gchar *
reference_book_first_verse(const gchar *reference)
{
    const gchar *dot;

    if (reference == NULL || *reference == '\0')
        return NULL;
    dot = strchr(reference, '.');
    if (dot == NULL)
        return NULL;
    return g_strdup_printf("%.*s.1.1", (gint)(dot - reference), reference);
}

void
gsword_window_load_module(GswordWindow *self, const gchar *name)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswModule *module;
    gchar *saved_module = NULL;
    gchar *saved_key = NULL;
    gchar *previous_reference = NULL;
    gboolean restored = FALSE;

    if (priv->module != NULL &&
        g_strcmp0(gsw_module_get_name(priv->module), name) != 0 &&
        gsw_key_get_kind(gsw_module_get_key(priv->module)) ==
            GSW_KEY_KIND_VERSE)
        previous_reference = gsw_verse_key_get_osis_ref(
            (GswVerseKey *)gsw_module_get_key(priv->module));

    module = gsw_manager_create_module(priv->manager, name);
    if (module == NULL) {
        g_free(previous_reference);
        gsword_window_set_status(self, _("Cannot open module"));
        return;
    }

    g_clear_object(&priv->module);
    priv->module = module;
    if (priv->reader_page != NULL)
        gsword_reader_page_sync_context(priv->reader_page);

    /*
     * Switching Bible versions should keep the current passage when the new
     * versification contains it.  If it does not, stay in the same book at
     * its first chapter and verse.  The initial/saved key is only used when
     * there is no previous reference (for example, application startup).
     */
    if (previous_reference != NULL) {
        restored = gsw_module_set_key_text(priv->module, previous_reference,
                                           NULL);
        if (!restored) {
            gchar *fallback = reference_book_first_verse(previous_reference);

            restored = fallback != NULL &&
                       gsw_module_set_key_text(priv->module, fallback, NULL);
            g_free(fallback);
        }
    }

    if (priv->settings != NULL) {
        saved_module = g_settings_get_string(priv->settings, "last-module");
        saved_key = g_settings_get_string(priv->settings, "last-key");
    }
    if (!restored && previous_reference == NULL &&
        saved_module != NULL && *saved_module != '\0' &&
        g_strcmp0(saved_module, name) == 0 &&
        saved_key != NULL && *saved_key != '\0')
        restored = gsw_module_set_key_text(priv->module, saved_key, NULL);
    if (!restored && priv->reader_page != NULL)
        gsword_reader_page_set_initial_key(priv->reader_page);

    g_free(previous_reference);
    g_free(saved_module);
    g_free(saved_key);
    if (priv->module_sidebar != NULL)
        gsword_module_sidebar_select(priv->module_sidebar, name);
    gsword_window_refresh_reader(self);
}

void
gsword_window_open_commentary_module(GswordWindow *self, const gchar *name)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswordStudyPanel *panel = priv->reader_page != NULL
                            ? gsword_reader_page_get_study_panel(
                                  priv->reader_page) : NULL;

    if (panel == NULL)
        return;
    gsword_study_panel_set_commentary_module(panel, name);
    if (priv->settings != NULL)
        g_settings_set_string(priv->settings, "study-module", name);
    gsword_study_panel_set_visible(panel, TRUE);
    gsword_study_panel_show_commentary(panel);
    gsword_study_panel_refresh(panel);
    gsword_window_show_page(self, "reader");
    gsword_window_set_status(self, _("Commentary opened in study panel"));
}

void
gsword_window_open_lexicon_module(GswordWindow *self, const gchar *name)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswordStudyPanel *panel = priv->reader_page != NULL
                            ? gsword_reader_page_get_study_panel(
                                  priv->reader_page) : NULL;

    if (panel == NULL)
        return;
    gsword_study_panel_set_lexicon_module(panel, name);
    if (priv->settings != NULL)
        g_settings_set_string(priv->settings, "study-lexicon", name);
    gsword_study_panel_refresh_lexicons(panel);
    gsword_study_panel_set_visible(panel, TRUE);
    gsword_study_panel_show_lexicon(panel);
    gsword_window_show_page(self, "reader");
    gsword_window_set_status(self, _("Lexicon opened in study panel"));
}

void
gsword_window_open_devotion_module(GswordWindow *self, const gchar *name)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->settings != NULL)
        g_settings_set_string(priv->settings, "devotion-module", name);
    if (priv->plan_page != NULL)
        gsword_plan_page_show_devotion(priv->plan_page);
}

void
gsword_window_open_resource_module(GswordWindow *self, const gchar *name,
                                   gboolean is_map)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->resource_page != NULL)
        gsword_resource_page_open(priv->resource_page, name, is_map);
}

void
gsword_window_populate_modules(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->module_sidebar != NULL)
        gsword_module_sidebar_populate(priv->module_sidebar);
}

static void
on_preferences_action(GSimpleAction *action, GVariant *parameter,
                      gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_preferences_show(GSWORD_WINDOW(user_data));
}

static void
on_focus_key_action(GSimpleAction *action, GVariant *parameter,
                    gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);

    (void)action;
    (void)parameter;
    gsword_window_focus_reference(self);
}

static void
on_history_back_action(GSimpleAction *action, GVariant *parameter,
                       gpointer user_data)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(
        GSWORD_WINDOW(user_data));

    (void)action;
    (void)parameter;
    if (priv->reader_page != NULL)
        gsword_reader_page_history_back(priv->reader_page);
}

static void
on_history_forward_action(GSimpleAction *action, GVariant *parameter,
                          gpointer user_data)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(
        GSWORD_WINDOW(user_data));

    (void)action;
    (void)parameter;
    if (priv->reader_page != NULL)
        gsword_reader_page_history_forward(priv->reader_page);
}

static void
gsword_window_apply_workspace(GswordWindow *self, const gchar *workspace)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswordStudyPanel *panel = priv->reader_page != NULL
                            ? gsword_reader_page_get_study_panel(
                                  priv->reader_page) : NULL;

    if (g_strcmp0(workspace, "reading") == 0) {
        if (panel != NULL)
            gsword_study_panel_set_visible(panel, FALSE);
        gsword_window_show_page(self, "reader");
    } else if (g_strcmp0(workspace, "study") == 0) {
        if (panel != NULL) {
            gsword_study_panel_set_visible(panel, TRUE);
            gsword_study_panel_set_follow(panel, TRUE);
            gsword_study_panel_show_commentary(panel);
            gsword_study_panel_refresh(panel);
        }
        gsword_window_show_page(self, "reader");
    } else if (g_strcmp0(workspace, "panes") == 0) {
        gsword_window_show_page(self, "panes");
        gsword_workspace_page_sync_from_reader(priv->workspace_page);
    } else if (g_strcmp0(workspace, "parallel") == 0) {
        gsword_workspace_page_apply_parallel_template(priv->workspace_page);
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
        gsword_preferences_show(self);
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
                                   "panes", _("Comparison workspace"),
                                   "parallel", _("Parallel comparison"),
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

static void
on_workspace_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *name = adw_view_stack_get_visible_child_name(
        ADW_VIEW_STACK(object));

    (void)pspec;
    if (priv->settings != NULL && name != NULL && priv->page_restored)
        g_settings_set_string(priv->settings, "active-page", name);
    if (g_strcmp0(name, "panes") == 0) {
        gsword_workspace_page_sync_from_reader(priv->workspace_page);
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
        gsword_workspace_page_apply_parallel_template(priv->workspace_page);
        gsword_window_show_page(self, "panes");
        if (priv->settings != NULL)
            g_settings_set_string(priv->settings, "active-page", "panes");
    } else if (page != NULL &&
               (g_strcmp0(page, "reader") == 0 ||
                g_strcmp0(page, "panes") == 0 ||
                g_strcmp0(page, "search") == 0 ||
                g_strcmp0(page, "notes") == 0 ||
                g_strcmp0(page, "plan") == 0 ||
                g_strcmp0(page, "library") == 0 ||
                g_strcmp0(page, "teaching") == 0)) {
        gsword_window_show_page(self, page);
    }
    priv->page_restored = TRUE;
    g_free(page);
}

static void
gsword_window_open_search(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *current = adw_view_stack_get_visible_child_name(
        priv->content_stack);

    if (g_strcmp0(current, "search") != 0) {
        g_free(priv->search_return_page);
        priv->search_return_page = g_strdup(current != NULL
                                            ? current : "reader");
    }
    gsword_window_show_page(self, "search");
    if (priv->search_page != NULL)
        gsword_search_page_focus(priv->search_page);
}

static void
on_search_action(GSimpleAction *action, GVariant *parameter,
                 gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_open_search(GSWORD_WINDOW(user_data));
}

static void
on_search_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_open_search(GSWORD_WINDOW(user_data));
}

void
gsword_window_close_search(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    gsword_window_show_page(self, priv->search_return_page != NULL
                                  ? priv->search_return_page : "reader");
}

static void
gsword_window_build(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    AdwToolbarView *toolbar_view;
    AdwHeaderBar *header;
    AdwOverlaySplitView *split;
    GtkWidget *switcher;
    GtkWidget *switcher_bar;
    GtkWidget *search_button;
    GtkWidget *header_actions;
    GtkWidget *menu_button;
    AdwBreakpoint *breakpoint;
    AdwBreakpointCondition *condition;
    GValue value = G_VALUE_INIT;
    GSimpleAction *preferences;
    GSimpleAction *focus_key;
    GSimpleAction *search_action;
    GSimpleAction *history_back;
    GSimpleAction *history_forward;
    GSimpleAction *toggle_sidebar;
    GtkWidget *status;

    gsword_preferences_init(self);

    priv->sidebar_visible = TRUE;
    gtk_window_set_title(GTK_WINDOW(self), "gsword");
    gtk_window_set_default_size(GTK_WINDOW(self), GSWORD_WINDOW_MIN_WIDTH, 760);
    gtk_widget_set_size_request(GTK_WIDGET(self), GSWORD_WINDOW_MIN_WIDTH, -1);

    toolbar_view = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
    header = ADW_HEADER_BAR(adw_header_bar_new());

    priv->sidebar_button = GTK_TOGGLE_BUTTON(gtk_toggle_button_new());
    gtk_button_set_icon_name(GTK_BUTTON(priv->sidebar_button),
                             "sidebar-show-symbolic");
    gtk_widget_add_css_class(GTK_WIDGET(priv->sidebar_button), "flat");
    gtk_widget_set_tooltip_text(GTK_WIDGET(priv->sidebar_button),
                                _("Hide module sidebar"));
    g_signal_connect(priv->sidebar_button, "toggled",
                     G_CALLBACK(on_sidebar_toggled), self);
    adw_header_bar_pack_start(header, GTK_WIDGET(priv->sidebar_button));

    search_button = gtk_button_new_from_icon_name("system-search-symbolic");
    gtk_widget_add_css_class(search_button, "flat");
    g_signal_connect(search_button, "clicked",
                     G_CALLBACK(on_search_clicked), self);
    gtk_widget_set_tooltip_text(search_button, _("Search"));

    menu_button = gtk_button_new_from_icon_name("open-menu-symbolic");
    gtk_widget_add_css_class(menu_button, "flat");
    g_signal_connect(menu_button, "clicked",
                     G_CALLBACK(on_main_menu_clicked), self);
    gtk_widget_set_tooltip_text(menu_button, _("Main menu"));

    header_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(header_actions), search_button);
    gtk_box_append(GTK_BOX(header_actions), menu_button);
    adw_header_bar_pack_end(header, header_actions);

    split = ADW_OVERLAY_SPLIT_VIEW(adw_overlay_split_view_new());
    priv->split_view = split;
    adw_overlay_split_view_set_sidebar_width_fraction(split, 0.24);
    priv->module_sidebar = gsword_module_sidebar_new(self);
    adw_overlay_split_view_set_sidebar(
        split, gsword_module_sidebar_widget(priv->module_sidebar));
    g_signal_connect(priv->split_view, "notify::collapsed",
                     G_CALLBACK(on_sidebar_collapsed_changed), self);
    g_signal_connect(priv->split_view, "notify::show-sidebar",
                     G_CALLBACK(on_sidebar_visibility_changed), self);

    priv->search_page = gsword_search_page_new(self);
    priv->teaching_page = gsword_teaching_page_new(self);
    priv->resource_page = gsword_resource_page_new(self);
    priv->notes_page = gsword_notes_page_new(self);
    priv->plan_page = gsword_plan_page_new(self);
    priv->library_page = gsword_library_page_new(self);
    priv->content_stack = ADW_VIEW_STACK(adw_view_stack_new());
    priv->reader_page = gsword_reader_page_new(self);
    adw_view_stack_add_titled_with_icon(priv->content_stack,
                                        gsword_reader_page_widget(
                                            priv->reader_page),
                                        "reader", _("Reading"),
                                        "x-office-document-symbolic");
    priv->workspace_page = gsword_workspace_page_new(self);
    adw_view_stack_add_titled_with_icon(priv->content_stack,
                                        gsword_workspace_page_widget(
                                            priv->workspace_page),
                                        "panes", _("Compare"),
                                        "view-dual-symbolic");
    {
        AdwViewStackPage *page;

        page = adw_view_stack_add_named(priv->content_stack,
                                        gsword_search_page_widget(
                                            priv->search_page),
                                        "search");
        adw_view_stack_page_set_visible(page, FALSE);
    }
    adw_view_stack_add_titled_with_icon(priv->content_stack,
                                        gsword_notes_page_widget(
                                            priv->notes_page),
                                        "notes", _("Notes"),
                                        "accessories-text-editor-symbolic");
    adw_view_stack_add_titled_with_icon(priv->content_stack,
                                        gsword_plan_page_widget(
                                            priv->plan_page),
                                        "plan", _("Plan"),
                                        "x-office-calendar-symbolic");
    {
        AdwViewStackPage *page;

        page = adw_view_stack_add_named(priv->content_stack,
                                        gsword_teaching_page_widget(
                                            priv->teaching_page),
                                        "teaching");
        adw_view_stack_page_set_visible(page, FALSE);
        page = adw_view_stack_add_named(priv->content_stack,
                                        gsword_resource_page_widget(
                                            priv->resource_page),
                                        "resource");
        adw_view_stack_page_set_visible(page, FALSE);
    }
    adw_view_stack_add_titled_with_icon(priv->content_stack,
                                        gsword_library_page_widget(
                                            priv->library_page),
                                        "library", _("Library"),
                                        "folder-documents-symbolic");
    adw_view_stack_set_visible_child_name(priv->content_stack, "reader");
    g_signal_connect(priv->content_stack, "notify::visible-child",
                     G_CALLBACK(on_workspace_changed), self);

    switcher = adw_view_switcher_new();
    adw_view_switcher_set_stack(ADW_VIEW_SWITCHER(switcher),
                                priv->content_stack);
    adw_view_switcher_set_policy(ADW_VIEW_SWITCHER(switcher),
                                 ADW_VIEW_SWITCHER_POLICY_WIDE);
    adw_header_bar_set_title_widget(header, switcher);

    gtk_widget_set_vexpand(GTK_WIDGET(priv->content_stack), TRUE);
    adw_overlay_split_view_set_content(split, GTK_WIDGET(priv->content_stack));

    adw_toolbar_view_add_top_bar(toolbar_view, GTK_WIDGET(header));
    adw_toolbar_view_set_content(toolbar_view, GTK_WIDGET(split));

    switcher_bar = adw_view_switcher_bar_new();
    adw_view_switcher_bar_set_stack(ADW_VIEW_SWITCHER_BAR(switcher_bar),
                                    priv->content_stack);
    adw_view_switcher_bar_set_reveal(ADW_VIEW_SWITCHER_BAR(switcher_bar),
                                     FALSE);
    adw_toolbar_view_add_bottom_bar(toolbar_view, switcher_bar);

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

    condition = adw_breakpoint_condition_parse("max-width: 700sp");
    if (condition != NULL) {
        breakpoint = adw_breakpoint_new(condition);
        g_value_init(&value, G_TYPE_BOOLEAN);
        g_value_set_boolean(&value, TRUE);
        adw_breakpoint_add_setter(breakpoint, G_OBJECT(switcher_bar),
                                  "reveal", &value);
        adw_breakpoint_add_setter(breakpoint, G_OBJECT(split),
                                  "collapsed", &value);
        g_value_unset(&value);
        g_value_init(&value, GTK_TYPE_WIDGET);
        g_value_set_object(&value, NULL);
        adw_breakpoint_add_setter(breakpoint, G_OBJECT(header),
                                  "title-widget", &value);
        g_value_unset(&value);
        adw_application_window_add_breakpoint(ADW_APPLICATION_WINDOW(self),
                                              breakpoint);
    }

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

    search_action = g_simple_action_new("search", NULL);
    g_signal_connect(search_action, "activate",
                     G_CALLBACK(on_search_action), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(search_action));
    g_object_unref(search_action);

    history_back = g_simple_action_new("history-back", NULL);
    g_signal_connect(history_back, "activate",
                     G_CALLBACK(on_history_back_action), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(history_back));
    g_object_unref(history_back);
    history_forward = g_simple_action_new("history-forward", NULL);
    g_signal_connect(history_forward, "activate",
                     G_CALLBACK(on_history_forward_action), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(history_forward));
    g_object_unref(history_forward);

    toggle_sidebar = g_simple_action_new("toggle-sidebar", NULL);
    g_signal_connect(toggle_sidebar, "activate",
                     G_CALLBACK(on_toggle_sidebar_action), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(toggle_sidebar));
    g_object_unref(toggle_sidebar);

    update_sidebar_button(self);

    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.preferences",
        (const gchar * const[]) { "<Control>comma", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.focus-key",
        (const gchar * const[]) { "<Control>l", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.search",
        (const gchar * const[]) { "<Control>f", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.history-back",
        (const gchar * const[]) { "<Alt>Left", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.history-forward",
        (const gchar * const[]) { "<Alt>Right", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.toggle-sidebar",
        (const gchar * const[]) { "F9", NULL });
}

static void
gsword_window_load_settings(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->settings != NULL)
        priv->sidebar_visible = g_settings_get_boolean(
            priv->settings, "sidebar-visible");
    else
        priv->sidebar_visible = TRUE;
    if (priv->split_view != NULL) {
        gboolean collapsed = adw_overlay_split_view_get_collapsed(
            priv->split_view);

        adw_overlay_split_view_set_show_sidebar(
            priv->split_view,
            collapsed ? FALSE : priv->sidebar_visible);
    }
    update_sidebar_button(self);
    if (priv->reader_page != NULL)
        gsword_study_panel_load_settings(
            gsword_reader_page_get_study_panel(priv->reader_page));
}

static void
gsword_window_finalize(GObject *object)
{
    GswordWindow *self = GSWORD_WINDOW(object);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    g_clear_object(&priv->module);
    g_clear_object(&priv->settings);
    g_clear_pointer(&priv->notes, gsword_notes_store_free);
    g_clear_pointer(&priv->plans, gsword_plan_store_free);
    g_free(priv->search_return_page);

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
    if (priv->reader_page != NULL)
        gsword_study_panel_refresh_lexicons(
            gsword_reader_page_get_study_panel(priv->reader_page));
    gsword_window_populate_modules(self);
    gsword_workspace_page_restore(priv->workspace_page);
    if (priv->notes_page != NULL)
        gsword_notes_page_refresh(priv->notes_page);
    gsword_library_page_refresh_sources(priv->library_page);
    gsword_library_page_refresh_installed(priv->library_page);
    gsword_window_restore_page(self);
    return GTK_WIDGET(self);
}
