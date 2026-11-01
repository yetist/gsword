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

struct _GswordWindow {
    AdwApplicationWindow parent_instance;
};

typedef struct {
    GswManager *manager;
    GSettings *settings;
    GswModule *module;
    GswordModuleSidebar *module_sidebar;
    GswordWorkspacePage *workspace_page;
    GswordReaderPage *reader_page;
    GtkStack *content_stack;
    gboolean page_restored;
    GtkLabel *status_label;

    GswordSearchPage *search_page;

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

void
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
    if (priv->reader_page != NULL)
        gsword_reader_page_sync_context(priv->reader_page);

    if (priv->settings != NULL) {
        saved_module = g_settings_get_string(priv->settings, "last-module");
        saved_key = g_settings_get_string(priv->settings, "last-key");
    }
    if (saved_module != NULL && *saved_module != '\0' &&
        g_strcmp0(saved_module, name) == 0 &&
        saved_key != NULL && *saved_key != '\0')
        restored = gsw_module_set_key_text(priv->module, saved_key, NULL);
    if (!restored && priv->reader_page != NULL)
        gsword_reader_page_set_initial_key(priv->reader_page);

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
    const gchar *name = gtk_stack_get_visible_child_name(
        GTK_STACK(object));

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
                g_strcmp0(page, "plan") == 0 ||
                g_strcmp0(page, "teaching") == 0)) {
        gsword_window_show_page(self, page);
    }
    priv->page_restored = TRUE;
    g_free(page);
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

    gsword_preferences_init(self);

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
    priv->module_sidebar = gsword_module_sidebar_new(self);
    sidebar_page = adw_navigation_page_new(
        gsword_module_sidebar_widget(priv->module_sidebar), _("Library"));

    priv->search_page = gsword_search_page_new(self);
    priv->teaching_page = gsword_teaching_page_new(self);
    priv->resource_page = gsword_resource_page_new(self);
    priv->notes_page = gsword_notes_page_new(self);
    priv->plan_page = gsword_plan_page_new(self);
    priv->library_page = gsword_library_page_new(self);
    priv->content_stack = GTK_STACK(gtk_stack_new());
    priv->reader_page = gsword_reader_page_new(self);
    gtk_stack_add_titled(priv->content_stack,
                         gsword_reader_page_widget(priv->reader_page),
                         "reader", _("Reading"));
    priv->workspace_page = gsword_workspace_page_new(self);
    gtk_stack_add_titled(priv->content_stack,
                         gsword_workspace_page_widget(priv->workspace_page),
                         "panes", _("Compare"));
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
