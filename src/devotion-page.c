/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * devotion-page.c: daily devotion page for the gsword window
 */

#include "devotion-page.h"
#include "window-private.h"

#include <glib/gi18n.h>

struct _GswordDevotionPage {
    GswordWindow *window;
    GtkWidget *root;
    GtkDropDown *selector;
    GtkStringList *names;
    GDateTime *date;
    GtkTextView *view;
    GtkLabel *title;
};

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
load_devotion(GswordDevotionPage *page)
{
    GswManager *manager = gsword_window_get_manager(page->window);
    GSettings *settings = gsword_window_get_settings(page->window);
    guint selected = page->selector != NULL
                   ? gtk_drop_down_get_selected(page->selector)
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

    if (page->view == NULL || page->names == NULL ||
        selected == GTK_INVALID_LIST_POSITION)
        return;
    name = gtk_string_list_get_string(page->names, selected);
    if (name == NULL || *name == '\0')
        return;

    if (page->date == NULL)
        page->date = g_date_time_new_now_local();
    now = g_date_time_ref(page->date);
    date_text = g_date_time_format(now, "%B %-d, %Y");
    for (i = 0; formats[i] != NULL; i++) {
        gchar *key = g_date_time_format(now, formats[i]);
        GswModule *module = gsw_manager_create_module(manager, name);

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
            page->view,
            _("No devotional entry is available for today in this module."));
    } else {
        gsword_window_set_view_text(page->view, text);
        g_free(text);
    }
    title = g_strdup_printf(_("Daily Devotion · %s · %s"), name,
                            date_text != NULL ? date_text : "");
    gtk_label_set_text(page->title, title);
    g_free(title);
    g_free(date_text);
    g_clear_error(&error);
    if (settings != NULL)
        g_settings_set_string(settings, "devotion-module", name);
}

void
gsword_devotion_page_refresh(GswordDevotionPage *page)
{
    GswManager *manager;
    GSettings *settings;
    GPtrArray *infos;
    GtkStringList *names;
    gchar *wanted = NULL;
    guint selected = 0;
    gboolean found = FALSE;
    guint i;
    guint count = 0;

    g_return_if_fail(page != NULL);
    if (page->selector == NULL)
        return;
    manager = gsword_window_get_manager(page->window);
    settings = gsword_window_get_settings(page->window);
    infos = gsw_manager_get_module_infos(manager);
    names = gtk_string_list_new(NULL);
    if (settings != NULL)
        wanted = g_settings_get_string(settings, "devotion-module");
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
    g_clear_object(&page->names);
    page->names = names;
    gtk_drop_down_set_model(page->selector, G_LIST_MODEL(names));
    if (count > 0) {
        gtk_drop_down_set_selected(page->selector, selected);
        load_devotion(page);
    } else {
        gtk_label_set_text(page->title, _("Daily Devotion"));
        gsword_window_set_view_text(
            page->view,
            _("No daily devotional module is installed.\n\n"
              "Install a module such as Daily Light or Morning and Evening "
              "from the Library page."));
    }
    g_free(wanted);
}

static void
on_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    load_devotion(user_data);
}

static void
on_refresh(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_devotion_page_refresh(user_data);
}

static void
on_shift(GtkButton *button, gpointer user_data)
{
    GswordDevotionPage *page = user_data;
    gint direction = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button),
                                                       "direction"));
    GDateTime *date;

    if (page->date == NULL)
        page->date = g_date_time_new_now_local();
    if (direction == 0) {
        g_date_time_unref(page->date);
        page->date = g_date_time_new_now_local();
    } else {
        date = g_date_time_add_days(page->date, direction);
        g_date_time_unref(page->date);
        page->date = date;
    }
    load_devotion(page);
}

static void
devotion_page_free(gpointer data)
{
    GswordDevotionPage *page = data;

    if (page == NULL)
        return;
    g_clear_object(&page->names);
    g_clear_pointer(&page->date, g_date_time_unref);
    g_free(page);
}

GswordDevotionPage *
gsword_devotion_page_new(GswordWindow *window)
{
    GswordDevotionPage *page = g_new0(GswordDevotionPage, 1);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *previous = gtk_button_new_from_icon_name(
        "go-previous-symbolic");
    GtkWidget *next = gtk_button_new_from_icon_name("go-next-symbolic");
    GtkWidget *today = gtk_button_new_with_label(_("Today"));
    GtkWidget *refresh = gtk_button_new_with_label(_("Refresh"));
    GtkWidget *scroll = gtk_scrolled_window_new();

    page->window = window;
    gtk_widget_set_margin_top(box, 10);
    gtk_widget_set_margin_bottom(box, 10);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);

    page->selector = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    gtk_widget_set_hexpand(GTK_WIDGET(page->selector), TRUE);
    g_signal_connect(page->selector, "notify::selected",
                     G_CALLBACK(on_changed), page);
    g_signal_connect(refresh, "clicked", G_CALLBACK(on_refresh), page);
    g_object_set_data(G_OBJECT(previous), "direction", GINT_TO_POINTER(-1));
    g_object_set_data(G_OBJECT(next), "direction", GINT_TO_POINTER(1));
    g_object_set_data(G_OBJECT(today), "direction", GINT_TO_POINTER(0));
    g_signal_connect(previous, "clicked", G_CALLBACK(on_shift), page);
    g_signal_connect(next, "clicked", G_CALLBACK(on_shift), page);
    g_signal_connect(today, "clicked", G_CALLBACK(on_shift), page);
    gtk_widget_set_tooltip_text(previous, _("Previous day"));
    gtk_widget_set_tooltip_text(next, _("Next day"));
    page->title = GTK_LABEL(gtk_label_new(_("Daily Devotion")));
    gtk_label_set_xalign(page->title, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->title), "heading");
    page->view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(page->view, FALSE);
    gtk_text_view_set_cursor_visible(page->view, FALSE);
    gtk_text_view_set_wrap_mode(page->view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(page->view, 12);
    gtk_text_view_set_right_margin(page->view, 12);
    gtk_text_view_set_top_margin(page->view, 12);
    gtk_text_view_set_bottom_margin(page->view, 12);
    gtk_box_append(GTK_BOX(controls), GTK_WIDGET(page->selector));
    gtk_box_append(GTK_BOX(controls), previous);
    gtk_box_append(GTK_BOX(controls), today);
    gtk_box_append(GTK_BOX(controls), next);
    gtk_box_append(GTK_BOX(controls), refresh);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
                                  GTK_WIDGET(page->view));
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(box), controls);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(page->title));
    gtk_box_append(GTK_BOX(box), scroll);
    page->root = box;
    g_object_set_data_full(G_OBJECT(box), "gsword-devotion-page", page,
                           devotion_page_free);
    gsword_devotion_page_refresh(page);
    return page;
}

GtkWidget *
gsword_devotion_page_widget(GswordDevotionPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}
