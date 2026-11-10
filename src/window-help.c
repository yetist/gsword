/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * window-help.c: window menu, shortcuts, and about dialogs
 */

#include "window-help.h"

#include <glib/gi18n.h>

static void
add_shortcut_row(GtkGrid *grid, gint row, const gchar *keys,
                 const gchar *description)
{
    GtkWidget *key_label = gtk_label_new(keys);
    GtkWidget *description_label = gtk_label_new(description);

    gtk_label_set_xalign(GTK_LABEL(key_label), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(description_label), 0.0f);
    gtk_widget_add_css_class(key_label, "dim-label");
    gtk_widget_add_css_class(key_label, "caption");
    gtk_grid_attach(grid, key_label, 0, row, 1, 1);
    gtk_grid_attach(grid, description_label, 1, row, 1, 1);
}

void
gsword_window_show_shortcuts(GswordWindow *window)
{
    AdwDialog *dialog;
    GtkWidget *grid;

    g_return_if_fail(GSWORD_IS_WINDOW(window));

    dialog = adw_alert_dialog_new(_("Keyboard shortcuts"), NULL);
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 18);
    gtk_widget_set_margin_start(grid, 4);
    gtk_widget_set_margin_end(grid, 4);
    add_shortcut_row(GTK_GRID(grid), 0, "Ctrl+F", _("Search"));
    add_shortcut_row(GTK_GRID(grid), 1, "Ctrl+L", _("Focus reference"));
    add_shortcut_row(GTK_GRID(grid), 2, "Ctrl+Shift+I", _("Module library"));
    add_shortcut_row(GTK_GRID(grid), 3, "Ctrl+,", _("Preferences"));
    add_shortcut_row(GTK_GRID(grid), 4, "Alt+Left", _("Back"));
    add_shortcut_row(GTK_GRID(grid), 5, "Alt+Right", _("Forward"));
    add_shortcut_row(GTK_GRID(grid), 6, "Ctrl+Shift+N", _("Open current note"));
    add_shortcut_row(GTK_GRID(grid), 7, "Ctrl+Shift+P", _("Open prayer"));
    add_shortcut_row(GTK_GRID(grid), 8, "Ctrl+Shift+B", _("Open bookmarks"));
    add_shortcut_row(GTK_GRID(grid), 9, "Ctrl+PageUp", _("Previous page"));
    add_shortcut_row(GTK_GRID(grid), 10, "Ctrl+PageDown", _("Next page"));
    add_shortcut_row(GTK_GRID(grid), 11, "F9", _("Toggle sidebar"));
    adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(dialog), grid);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "close", _("Close"), NULL);
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "close");
    adw_dialog_present(dialog, GTK_WIDGET(window));
}

void
gsword_window_show_about(GswordWindow *window)
{
    AdwAboutDialog *about;

    g_return_if_fail(GSWORD_IS_WINDOW(window));

    about = ADW_ABOUT_DIALOG(adw_about_dialog_new());
    adw_about_dialog_set_application_name(about, "GSword");
    adw_about_dialog_set_application_icon(about, "org.gsword.GSword");
    adw_about_dialog_set_developer_name(about, "yetist");
    adw_about_dialog_set_developers(about, (const gchar *[]) { "yetist", NULL });
    adw_about_dialog_set_version(about, GSW_VERSION_S);
    adw_about_dialog_set_comments(about,
        _("A GTK4 Bible study application powered by libgsword, a "
          "GObject-based SWORD module library."));
    adw_about_dialog_set_website(about, "https://github.com/yetist/gsword");
    adw_about_dialog_set_issue_url(about,
                                   "https://github.com/yetist/gsword/issues");
    adw_about_dialog_add_link(about, "GitHub",
                              "https://github.com/yetist/gsword");
    adw_about_dialog_set_license_type(about, GTK_LICENSE_GPL_2_0);
    adw_about_dialog_set_copyright(about, "© 2026 yetist");
    adw_dialog_present(ADW_DIALOG(about), GTK_WIDGET(window));
}

GMenuModel *
gsword_window_build_main_menu(void)
{
    GMenu *menu = g_menu_new();
    GMenu *workspace = g_menu_new();
    GMenu *settings = g_menu_new();
    GMenu *help = g_menu_new();
    GMenu *quit = g_menu_new();
    GMenuItem *item;

    g_menu_append(workspace, _("Show study panel"), "win.study-panel");
    g_menu_append(workspace, _("Comparison"), "win.compare");
    g_menu_append(workspace, _("Teaching"), "win.teaching");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(workspace));

    item = g_menu_item_new(_("Module library"), "win.library");
    g_menu_item_set_attribute(item, "accel", "s", "<Control><Shift>i");
    g_menu_append_item(settings, item);
    g_object_unref(item);
    item = g_menu_item_new(_("Preferences"), "win.preferences");
    g_menu_item_set_attribute(item, "accel", "s", "<Control>comma");
    g_menu_append_item(settings, item);
    g_object_unref(item);
    g_menu_append_section(menu, NULL, G_MENU_MODEL(settings));

    g_menu_append(help, _("Keyboard shortcuts"), "win.shortcuts");
    g_menu_append(help, _("About GSword"), "win.about");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(help));

    g_menu_append(quit, _("Quit"), "app.quit");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(quit));

    g_object_unref(workspace);
    g_object_unref(settings);
    g_object_unref(help);
    g_object_unref(quit);
    return G_MENU_MODEL(menu);
}
