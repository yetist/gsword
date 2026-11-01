/*
 * library-page.c: module library and install UI for the gsword window
 */

#include "library-page.h"
#include "window-private.h"
#include "app-paths.h"

#include <glib/gi18n.h>

struct _GswordLibraryPage {
    GswordWindow *window;
    GtkWidget *root;
    GswManager *manager;
    GSettings *settings;
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
};

static GswordLibraryPage *
library_page_get(GswordWindow *self)
{
    return g_object_get_data(G_OBJECT(self), "gsword-library-page");
}

static void gsword_window_refresh_remote_modules(GswordWindow *self);
static void on_refresh_sources_finished(GObject *source, GAsyncResult *result,
                                        gpointer user_data);
static void on_refresh_remote_finished(GObject *source, GAsyncResult *result,
                                       gpointer user_data);
static void on_install_finished(GObject *source, GAsyncResult *result,
                                gpointer user_data);
static void on_remove_finished(GObject *source, GAsyncResult *result,
                               gpointer user_data);

static GswInstallSource *
gsword_window_current_install_source(GswordWindow *self)
{
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);

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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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
    GswordLibraryPage *priv = library_page_get(self);
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

static void
library_page_free(gpointer data)
{
    GswordLibraryPage *page = data;

    if (page == NULL)
        return;
    g_clear_object(&page->installer);
    g_clear_object(&page->install_reporter);
    g_clear_pointer(&page->install_sources, g_ptr_array_unref);
    g_clear_object(&page->source_names);
    g_clear_object(&page->installed_names);
    g_clear_object(&page->install_cancellable);
    g_free(page->selected_remote_name);
    g_free(page);
}

GswordLibraryPage *
gsword_library_page_new(GswordWindow *self)
{
    GswordLibraryPage *priv = g_new0(GswordLibraryPage, 1);
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

    priv->window = self;
    priv->manager = gsword_window_get_manager(self);
    priv->settings = gsword_window_get_settings(self);
    g_object_set_data_full(G_OBJECT(self), "gsword-library-page", priv,
                           library_page_free);
    {
        GError *error = NULL;
        gchar *data_dir = gsword_app_data_dir();
        gchar *installmgr_dir = g_build_filename(data_dir, "installmgr", NULL);

        priv->installer = gsw_install_manager_new(installmgr_dir, &error);
        if (priv->installer == NULL) {
            g_printerr("gsword: cannot open install manager: %s\n",
                       error != NULL ? error->message : "unknown error");
            g_clear_error(&error);
        } else {
            priv->install_reporter = gsw_status_reporter_new();
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
    priv->root = box;
    return priv;
}


GtkWidget *
gsword_library_page_widget(GswordLibraryPage *page)
{
    return page != NULL ? page->root : NULL;
}

void
gsword_library_page_refresh_sources(GswordLibraryPage *page)
{
    if (page != NULL)
        gsword_window_refresh_install_sources(page->window);
}

void
gsword_library_page_refresh_installed(GswordLibraryPage *page)
{
    if (page != NULL)
        gsword_window_refresh_installed_dropdown(page->window);
}

void
gsword_library_page_refresh_remote(GswordLibraryPage *page)
{
    if (page != NULL)
        gsword_window_refresh_remote_modules(page->window);
}
