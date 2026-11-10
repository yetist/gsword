/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * library-source-dialog.c: install source management dialog
 */

#include "library-page-private.h"
#include "window-private.h"

#include <glib/gi18n.h>

typedef struct {
    GswordLibraryPage *page;
    GtkEntry *caption_entry;
    GtkEntry *host_entry;
    GtkEntry *directory_entry;
    GtkEntry *user_entry;
    GtkEntry *password_entry;
    AdwDialog *dialog;
} SourceDialogData;

static void
source_dialog_data_free(gpointer data)
{
    g_free(data);
}

static void
on_add_source_clicked(GtkButton *button, gpointer user_data)
{
    SourceDialogData *data = user_data;
    GswordLibraryPage *priv = data->page;
    GswordWindow *self = LIBRARY_WINDOW(priv);
    GswInstallManager *installer = gsword_library_page_get_installer(priv);
    const gchar *caption = gtk_editable_get_text(
        GTK_EDITABLE(data->caption_entry));
    const gchar *host = gtk_editable_get_text(GTK_EDITABLE(data->host_entry));
    const gchar *directory = gtk_editable_get_text(
        GTK_EDITABLE(data->directory_entry));
    const gchar *user = gtk_editable_get_text(GTK_EDITABLE(data->user_entry));
    const gchar *password = gtk_editable_get_text(
        GTK_EDITABLE(data->password_entry));
    GswInstallSource *source;
    GError *error = NULL;

    (void)button;
    if (caption == NULL || *caption == '\0' || host == NULL || *host == '\0') {
        gsword_window_set_status(self, _("Enter a source name and host"));
        return;
    }
    source = gsw_install_source_new(GSW_SOURCE_HTTPS, caption, host,
                                    directory != NULL ? directory : "",
                                    user, password, NULL);
    if (source == NULL) {
        gsword_window_set_status(self, _("Invalid source settings"));
        return;
    }
    if (!gsw_install_manager_add_source(installer, source)) {
        gsword_window_set_status(self, _("Cannot add source"));
        g_object_unref(source);
        return;
    }
    if (!gsw_install_manager_save_config(installer, &error)) {
        gsword_window_show_error(self, _("Cannot save source"), error);
        g_object_unref(source);
        return;
    }
    g_object_unref(source);
    gsword_window_refresh_install_sources(self);
    gsword_window_set_status(self, _("Source added"));
    if (data->dialog != NULL)
        adw_dialog_close(data->dialog);
}

static void
on_remove_source_clicked(GtkButton *button, gpointer user_data)
{
    SourceDialogData *data = user_data;
    GswordLibraryPage *priv = data->page;
    GswordWindow *self = LIBRARY_WINDOW(priv);
    GswInstallManager *installer = gsword_library_page_get_installer(priv);
    const gchar *caption = g_object_get_data(G_OBJECT(button),
                                             "source-caption");
    GError *error = NULL;

    if (caption == NULL)
        return;
    if (!gsw_install_manager_remove_source(installer, caption)) {
        gsword_window_set_status(self, _("Cannot remove source"));
        return;
    }
    if (!gsw_install_manager_save_config(installer, &error)) {
        gsword_window_show_error(self, _("Cannot save source"), error);
        return;
    }
    gsword_window_refresh_install_sources(self);
    gsword_window_set_status(self, _("Source removed"));
    if (data->dialog != NULL)
        adw_dialog_close(data->dialog);
}

void
gsword_library_page_show_sources_dialog(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    GswInstallManager *installer = gsword_library_page_get_installer(priv);
    AdwDialog *dialog;
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *sources_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *add_label;
    GtkWidget *add_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *caption_entry;
    GtkWidget *host_entry;
    GtkWidget *directory_entry;
    GtkWidget *user_entry;
    GtkWidget *password_entry;
    GtkWidget *add_button;
    GPtrArray *sources;
    SourceDialogData *data;
    guint i;

    (void)button;
    if (installer == NULL)
        return;
    dialog = ADW_DIALOG(adw_alert_dialog_new(_("Installation sources"), NULL));
    data = g_new0(SourceDialogData, 1);
    data->page = priv;
    data->dialog = dialog;

    add_label = gtk_label_new(_("Configured sources"));
    gtk_label_set_xalign(GTK_LABEL(add_label), 0.0f);
    gtk_widget_add_css_class(add_label, "heading");
    gtk_box_append(GTK_BOX(box), add_label);
    sources = gsw_install_manager_get_sources(installer);
    if (sources->len == 0) {
        GtkWidget *empty = gtk_label_new(_("No sources configured."));

        gtk_label_set_xalign(GTK_LABEL(empty), 0.0f);
        gtk_widget_add_css_class(empty, "dim-label");
        gtk_box_append(GTK_BOX(sources_box), empty);
    }
    for (i = 0; i < sources->len; i++) {
        GswInstallSource *source = g_ptr_array_index(sources, i);
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *label = gtk_label_new(
            gsw_install_source_get_caption(source));
        GtkWidget *remove = gtk_button_new_from_icon_name(
            "edit-delete-symbolic");
        gchar *detail = g_strdup_printf("%s · %s",
            gsw_install_source_get_host(source),
            gsw_install_source_get_directory(source));
        GtkWidget *detail_label = gtk_label_new(detail);

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_add_css_class(label, "heading");
        gtk_label_set_xalign(GTK_LABEL(detail_label), 0.0f);
        gtk_widget_add_css_class(detail_label, "caption");
        gtk_widget_add_css_class(detail_label, "dim-label");
        gtk_widget_set_hexpand(label, TRUE);
        gtk_widget_add_css_class(remove, "flat");
        g_object_set_data_full(G_OBJECT(remove), "source-caption",
                               g_strdup(gsw_install_source_get_caption(source)),
                               g_free);
        g_signal_connect(remove, "clicked",
                         G_CALLBACK(on_remove_source_clicked), data);
        gtk_box_append(GTK_BOX(row), label);
        gtk_box_append(GTK_BOX(row), detail_label);
        gtk_box_append(GTK_BOX(row), remove);
        gtk_box_append(GTK_BOX(sources_box), row);
        g_free(detail);
    }
    g_ptr_array_unref(sources);
    gtk_box_append(GTK_BOX(box), sources_box);

    add_label = gtk_label_new(_("Add source"));
    gtk_label_set_xalign(GTK_LABEL(add_label), 0.0f);
    gtk_widget_add_css_class(add_label, "heading");
    gtk_box_append(GTK_BOX(box), add_label);
    caption_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(caption_entry),
                                   _("Source name"));
    host_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(host_entry), _("Host"));
    directory_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(directory_entry),
                                   _("Module directory"));
    gtk_editable_set_text(GTK_EDITABLE(directory_entry),
                          "ftpmirror/pub/sword");
    user_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(user_entry),
                                   _("Username (optional)"));
    password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry),
                                   _("Password (optional)"));
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_entry_set_input_purpose(GTK_ENTRY(password_entry),
                                GTK_INPUT_PURPOSE_PASSWORD);
    gtk_box_append(GTK_BOX(box), caption_entry);
    gtk_box_append(GTK_BOX(box), host_entry);
    gtk_box_append(GTK_BOX(box), directory_entry);
    gtk_box_append(GTK_BOX(box), user_entry);
    gtk_box_append(GTK_BOX(box), password_entry);
    add_button = gtk_button_new_with_label(_("Add source"));
    gtk_box_append(GTK_BOX(add_box), add_button);
    gtk_widget_set_halign(add_box, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(box), add_box);
    data->caption_entry = GTK_ENTRY(caption_entry);
    data->host_entry = GTK_ENTRY(host_entry);
    data->directory_entry = GTK_ENTRY(directory_entry);
    data->user_entry = GTK_ENTRY(user_entry);
    data->password_entry = GTK_ENTRY(password_entry);
    g_signal_connect(add_button, "clicked",
                     G_CALLBACK(on_add_source_clicked), data);
    g_object_set_data_full(G_OBJECT(dialog), "source-dialog-data", data,
                           source_dialog_data_free);
    adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(dialog), box);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "close", _("Close"), NULL);
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "close");
    adw_dialog_present(dialog, GTK_WIDGET(self));
}
