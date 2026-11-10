/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * library-install-queue.c: module install, update, and removal queue
 */

#include "library-page-private.h"
#include "library-module-details.h"
#include "window-private.h"

#include <glib/gi18n.h>

static void on_install_finished(GObject *source, GAsyncResult *result,
                                gpointer user_data);
static void on_remove_finished(GObject *source, GAsyncResult *result,
                               gpointer user_data);

static gboolean
clear_install_progress_cb(gpointer data)
{
    GswordWindow *self = GSWORD_WINDOW(data);
    GswordLibraryPage *priv = library_page_get(self);

    if (priv == NULL)
        return G_SOURCE_REMOVE;
    if (priv->install_cancellable == NULL) {
        gtk_progress_bar_set_fraction(priv->install_progress, 0.0);
        gtk_label_set_text(priv->install_status, "");
    }
    return G_SOURCE_REMOVE;
}

void
gsword_library_clear_install_progress_later(GswordWindow *self)
{
    /* Progress events are queued on the main context from the worker thread.
     * Clear after those events have been processed so a final progress update
     * cannot leave a stale "Downloading..." label after the operation ends. */
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, clear_install_progress_cb,
                    g_object_ref(self), g_object_unref);
}

void
gsword_library_on_refresh_remote_finished(GObject *source,
                                          GAsyncResult *result,
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
    if (priv == NULL)
        return;
    gtk_widget_set_sensitive(GTK_WIDGET(priv->refresh_source_button), TRUE);
    gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), FALSE);
    gtk_progress_bar_set_fraction(priv->install_progress, 0.0);
    g_clear_object(&priv->install_cancellable);
    gsword_library_refresh_remote_modules(self);
    gsword_library_clear_install_progress_later(self);
}

static void
rebuild_queue_view(GswordWindow *self)
{
    GswordLibraryPage *priv = library_page_get(self);
    guint i;

    if (priv->install_queue_list == NULL)
        return;
    gsword_window_clear_list(priv->install_queue_list);
    if (priv->update_queue != NULL) {
        for (i = 0; i < priv->update_queue->len; i++) {
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *label = gtk_label_new(
                g_ptr_array_index(priv->update_queue, i));

            gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
            gtk_widget_add_css_class(label, "caption");
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
            gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
            gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
            gtk_list_box_append(priv->install_queue_list, row);
        }
    }
    if (priv->install_queue_scroll != NULL)
        gtk_widget_set_visible(priv->install_queue_scroll,
            priv->update_queue != NULL && priv->update_queue->len > 0);
    if (priv->pause_queue_button != NULL) {
        gtk_widget_set_visible(GTK_WIDGET(priv->pause_queue_button),
            priv->update_queue != NULL && priv->update_queue->len > 0);
        gtk_button_set_label(priv->pause_queue_button,
            priv->queue_paused ? _("Resume") : _("Pause"));
    }
}

static void
update_retry_button(GswordLibraryPage *priv)
{
    if (priv->retry_failed_button != NULL)
        gtk_widget_set_visible(GTK_WIDGET(priv->retry_failed_button),
            priv->failed_updates != NULL && priv->failed_updates->len > 0);
}

static void
start_next_update(GswordWindow *self)
{
    GswordLibraryPage *priv = library_page_get(self);
    const gchar *name;

    if (priv->update_queue == NULL || priv->update_queue->len == 0) {
        priv->updating_all = FALSE;
        gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), FALSE);
        gtk_progress_bar_set_fraction(priv->install_progress, 0.0);
        g_clear_object(&priv->install_cancellable);
        g_clear_object(&priv->update_source);
        rebuild_queue_view(self);
        update_retry_button(priv);
        gsword_window_set_status(self, priv->failed_updates != NULL &&
                                 priv->failed_updates->len > 0
                                 ? _("Updates finished with errors")
                                 : _("All updates complete"));
        gsword_library_refresh_remote_modules(self);
        return;
    }
    g_free(priv->selected_remote_name);
    priv->selected_remote_name = g_ptr_array_steal_index(priv->update_queue, 0);
    name = priv->selected_remote_name;
    rebuild_queue_view(self);
    if (priv->install_cancellable == NULL) {
        priv->install_cancellable = g_cancellable_new();
        gsw_install_manager_set_cancellable(priv->installer,
                                            priv->install_cancellable);
    }
    gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), TRUE);
    gtk_progress_bar_pulse(priv->install_progress);
    gsword_window_set_status(self, _("Updating modules..."));
    gsw_install_manager_install_module_async(
        priv->installer, priv->manager, priv->update_source, name,
        priv->install_cancellable, on_install_finished, self);
}

void
gsword_library_on_update_all_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    GswInstallSource *source = gsword_library_current_install_source(self);
    guint i;

    (void)button;
    if (source == NULL || priv->remote_infos == NULL) {
        gsword_window_set_status(self, _("No source selected"));
        return;
    }
    g_clear_pointer(&priv->update_queue, g_ptr_array_unref);
    g_clear_pointer(&priv->failed_updates, g_ptr_array_unref);
    priv->failed_updates = g_ptr_array_new_with_free_func(g_free);
    priv->update_queue = g_ptr_array_new_with_free_func(g_free);
    update_retry_button(priv);
    for (i = 0; i < priv->remote_infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(priv->remote_infos, i);
        guint status = gsw_install_manager_get_module_status(priv->manager,
                                                             info);

        if (status & GSW_MODULE_STATUS_UPDATED)
            g_ptr_array_add(priv->update_queue,
                            g_strdup(gsw_module_info_get_name(info)));
    }
    if (priv->update_queue->len == 0) {
        rebuild_queue_view(self);
        gsword_window_set_status(self, _("No updates available"));
        return;
    }
    g_clear_object(&priv->update_source);
    priv->update_source = g_object_ref(source);
    priv->updating_all = TRUE;
    priv->queue_paused = FALSE;
    rebuild_queue_view(self);
    start_next_update(self);
}


static gboolean
check_install_space(GswordWindow *self, GswModuleInfo *info)
{
    GswordLibraryPage *priv = library_page_get(self);
    const gchar *prefix = gsw_manager_get_prefix_path(priv->manager);
    guint64 needed = gsw_module_info_get_install_size(info);
    GFile *file;
    GFileInfo *fs_info;
    GError *error = NULL;
    guint64 available = 0;
    gboolean ok = TRUE;

    if (needed == 0 || prefix == NULL)
        return TRUE;
    file = g_file_new_for_path(prefix);
    fs_info = g_file_query_filesystem_info(file,
        G_FILE_ATTRIBUTE_FILESYSTEM_FREE, NULL, &error);
    if (fs_info == NULL) {
        gsword_window_show_error(self, _("Cannot check disk space"), error);
        g_object_unref(file);
        return FALSE;
    }
    if (g_file_info_has_attribute(fs_info,
                                  G_FILE_ATTRIBUTE_FILESYSTEM_FREE))
        available = g_file_info_get_attribute_uint64(
            fs_info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
    if (available > 0 && needed > available) {
        gchar *need_text = gsword_library_format_size(needed);
        gchar *free_text = gsword_library_format_size(available);

        gsword_window_set_status(self, _("Not enough disk space"));
        {
            GtkAlertDialog *dialog = gtk_alert_dialog_new(
                _("Not enough disk space"));
            gchar *detail = g_strdup_printf(
                _("This module needs %s, but only %s is available."),
                need_text, free_text);

            gtk_alert_dialog_set_detail(dialog, detail);
            gtk_alert_dialog_show(dialog, GTK_WINDOW(self));
            g_object_unref(dialog);
            g_free(detail);
        }
        g_free(need_text);
        g_free(free_text);
        ok = FALSE;
    }
    g_object_unref(fs_info);
    g_object_unref(file);
    return ok;
}

void
gsword_library_on_retry_failed_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    GswInstallSource *source = gsword_library_current_install_source(self);
    guint i;

    (void)button;
    if (source == NULL || priv->failed_updates == NULL ||
        priv->failed_updates->len == 0)
        return;
    g_clear_pointer(&priv->update_queue, g_ptr_array_unref);
    priv->update_queue = g_ptr_array_new_with_free_func(g_free);
    for (i = 0; i < priv->failed_updates->len; i++)
        g_ptr_array_add(priv->update_queue,
                        g_strdup(g_ptr_array_index(priv->failed_updates, i)));
    g_clear_pointer(&priv->failed_updates, g_ptr_array_unref);
    priv->failed_updates = g_ptr_array_new_with_free_func(g_free);
    update_retry_button(priv);
    g_clear_object(&priv->update_source);
    priv->update_source = g_object_ref(source);
    priv->updating_all = TRUE;
    rebuild_queue_view(self);
    start_next_update(self);
}


void
gsword_library_on_pause_queue_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);

    (void)button;
    if (priv->queue_paused) {
        priv->queue_paused = FALSE;
        priv->updating_all = TRUE;
        gtk_button_set_label(GTK_BUTTON(button), _("Pause"));
        gsword_window_set_status(self, _("Resuming updates"));
        start_next_update(self);
    } else {
        priv->queue_paused = TRUE;
        gtk_button_set_label(GTK_BUTTON(button), _("Resume"));
        gsword_window_set_status(self, _("Pausing after current module..."));
    }
}

void
gsword_library_on_install_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    GswInstallSource *source = gsword_library_current_install_source(self);

    (void)button;
    if (source == NULL || priv->selected_remote_name == NULL)
        return;
    {
        GswModuleInfo *info = gsw_install_manager_lookup_remote_module_info(
            priv->installer, source, priv->selected_remote_name, NULL);

        if (info != NULL && !check_install_space(self, info))
            return;
    }
    priv->updating_all = FALSE;
    priv->queue_paused = FALSE;
    g_clear_pointer(&priv->update_queue, g_ptr_array_unref);
    g_clear_object(&priv->update_source);
    rebuild_queue_view(self);
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
    gchar *installed_name = NULL;

    installed = gsw_install_manager_install_module_finish(
        GSW_INSTALL_MANAGER(source), result, &error);
    if (priv == NULL) {
        g_clear_error(&error);
        return;
    }
    installed_name = g_strdup(priv->selected_remote_name);
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
    gsword_library_refresh_installed_list(self);
    gsword_library_refresh_remote_modules(self);
    gsword_window_show_page(self, "library");
    if (priv->updating_all) {
        if (!installed && installed_name != NULL && priv->failed_updates != NULL)
            g_ptr_array_add(priv->failed_updates, g_strdup(installed_name));
        g_free(installed_name);
        if (priv->queue_paused) {
            priv->updating_all = FALSE;
            gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), FALSE);
            gtk_progress_bar_set_fraction(priv->install_progress, 0.0);
            g_clear_object(&priv->install_cancellable);
            gsword_window_set_status(self, _("Updates paused"));
            return;
        }
        start_next_update(self);
        return;
    }
    if (installed && installed_name != NULL) {
        gchar *message = g_strdup_printf(_("Installed module %s"),
                                         installed_name);
        gsword_window_set_status(self, message);
        g_free(message);
    }
    g_free(installed_name);
    gsword_library_clear_install_progress_later(self);
}


static void
remove_module(GswordWindow *self, const gchar *name)
{
    GswordLibraryPage *priv = library_page_get(self);

    if (priv->installer == NULL || name == NULL)
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
on_remove_response(AdwAlertDialog *dialog, const gchar *response,
                   gpointer user_data)
{
    GswordWindow *self = user_data;
    const gchar *name = g_object_get_data(G_OBJECT(dialog), "module-name");

    if (g_strcmp0(response, "remove") == 0)
        remove_module(self, name);
}

void
gsword_library_on_remove_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    const gchar *name = priv->selected_installed_name;
    AdwDialog *dialog;

    (void)button;
    if (priv->installer == NULL || name == NULL)
        return;
    dialog = ADW_DIALOG(adw_alert_dialog_new(_("Remove module?"), NULL));
    adw_alert_dialog_format_body(
        ADW_ALERT_DIALOG(dialog),
        _("Remove “%s” from your module library? This cannot be undone."),
        name);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "cancel", _("Cancel"),
                                   "remove", _("Remove"), NULL);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_set_response_appearance(
        ADW_ALERT_DIALOG(dialog), "remove", ADW_RESPONSE_DESTRUCTIVE);
    g_object_set_data_full(G_OBJECT(dialog), "module-name", g_strdup(name),
                           g_free);
    g_signal_connect(dialog, "response", G_CALLBACK(on_remove_response), self);
    adw_dialog_present(dialog, GTK_WIDGET(self));
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
    if (priv == NULL)
        return;
    g_clear_object(&priv->install_cancellable);
    g_clear_pointer(&priv->selected_installed_name, g_free);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->remove_button), FALSE);
    if (priv->manager != NULL)
        gsw_manager_reload(priv->manager, NULL);
    gsword_window_populate_modules(self);
    gsword_library_refresh_installed_list(self);
    gsword_library_refresh_remote_modules(self);
    gsword_window_show_page(self, "library");
    gsword_library_clear_install_progress_later(self);
}

void
gsword_library_on_cancel_install_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    (void)button;

    if (priv->install_cancellable != NULL)
        g_cancellable_cancel(priv->install_cancellable);
    priv->updating_all = FALSE;
    g_clear_pointer(&priv->update_queue, g_ptr_array_unref);
    g_clear_object(&priv->update_source);
    rebuild_queue_view(self);
    gsword_window_set_status(self, _("Cancelling..."));
}


void
gsword_library_on_install_pre_status(GswStatusReporter *reporter, guint64 total, guint64 done,
                      const gchar *message, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    (void)reporter;

    if (priv == NULL)
        return;
    gtk_progress_bar_set_fraction(priv->install_progress,
                                  total > 0 ? (gdouble)done / total : 0.0);
    gtk_label_set_text(priv->install_status,
                       message != NULL ? message : _("Installing..."));
    (void)self;
}

void
gsword_library_on_install_progress(GswStatusReporter *reporter, guint64 total, guint64 done,
                    gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    gchar *status;
    (void)reporter;

    if (priv == NULL)
        return;

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

