/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * window-services.c: shared services for the gsword window and its pages
 */

#include "window-private.h"
#include "app-locale.h"

#include <glib/gi18n.h>

#include <string.h>

typedef struct {
    GtkLabel *status_label;
    AdwViewStack *content_stack;
} GswordWindowServices;

static GswordWindowServices *
get_services(GswordWindow *self)
{
    return g_object_get_data(G_OBJECT(self), "gsword-window-services");
}

static gboolean
path_is_within_prefix(const gchar *prefix, const gchar *path)
{
    gchar *canonical_prefix;
    gchar *canonical_path;
    gsize prefix_len;
    gboolean inside;

    if (prefix == NULL || *prefix == '\0' || path == NULL || *path == '\0')
        return FALSE;
    canonical_prefix = g_canonicalize_filename(prefix, NULL);
    canonical_path = g_canonicalize_filename(path, NULL);
    prefix_len = strlen(canonical_prefix);
    inside = g_strcmp0(canonical_path, canonical_prefix) == 0 ||
             (g_str_has_prefix(canonical_path, canonical_prefix) &&
              canonical_path[prefix_len] == G_DIR_SEPARATOR);
    g_free(canonical_path);
    g_free(canonical_prefix);
    return inside;
}

gboolean
gsword_window_module_is_writable(GswordWindow *self, GswModuleInfo *info)
{
    GswManager *manager = gsword_window_get_manager(self);
    const gchar *prefix;

    if (manager == NULL || info == NULL)
        return FALSE;
    prefix = gsw_manager_get_prefix_path(manager);
    return path_is_within_prefix(prefix,
                                 gsw_module_info_get_config_file(info)) ||
           path_is_within_prefix(prefix,
                                 gsw_module_info_get_absolute_data_path(info));
}

gboolean
gsword_window_book_names_follow_ui(GswordWindow *self)
{
    GSettings *settings = gsword_window_get_settings(self);
    gchar *policy;
    gboolean follow_ui;

    if (settings == NULL)
        return FALSE;
    policy = g_settings_get_string(settings, "book-name-language");
    follow_ui = g_strcmp0(policy, "ui") == 0;
    g_free(policy);
    return follow_ui;
}

gchar *
gsword_window_book_name_locale(GswordWindow *self, GswModuleInfo *info)
{
    const gchar *language = info != NULL
                          ? gsw_module_info_get_language(info) : NULL;

    if (!gsword_window_book_names_follow_ui(self) &&
        language != NULL && *language != '\0')
        return g_strdup(language);
    return gsword_app_locale_name();
}

void
gsword_window_apply_book_name_locale(GswordWindow *self, GswModule *module)
{
    GswKey *key;
    gchar *locale;

    if (module == NULL)
        return;
    key = gsw_module_get_key(module);
    if (gsword_window_book_names_follow_ui(self)) {
        locale = gsword_app_locale_name();
        gsw_key_set_locale(key, locale);
        g_free(locale);
    } else {
        gsw_key_set_locale(key, NULL);
    }
}

void
gsword_window_services_init(GswordWindow *self, GtkLabel *status_label,
                            AdwViewStack *content_stack)
{
    GswordWindowServices *services;

    g_return_if_fail(GSWORD_IS_WINDOW(self));
    services = get_services(self);
    if (services == NULL) {
        services = g_new0(GswordWindowServices, 1);
        g_object_set_data_full(G_OBJECT(self), "gsword-window-services",
                               services, g_free);
    }
    services->status_label = status_label;
    services->content_stack = content_stack;
}

void
gsword_window_set_status(GswordWindow *self, const gchar *message)
{
    GswordWindowServices *services = get_services(self);

    if (services == NULL || services->status_label == NULL)
        return;
    gtk_label_set_text(services->status_label, message != NULL ? message : "");
}

void
gsword_window_toast(GswordWindow *self, const gchar *message)
{
    AdwToastOverlay *overlay = g_object_get_data(G_OBJECT(self),
                                                 "gsword-toast-overlay");

    if (overlay == NULL || message == NULL || *message == '\0')
        return;
    adw_toast_overlay_add_toast(overlay, adw_toast_new(message));
}

void
gsword_window_show_error(GswordWindow *self, const gchar *prefix,
                         GError *error)
{
    const gchar *detail = error != NULL ? error->message : _("unknown error");
    gchar *message = g_strdup_printf("%s: %s", prefix, detail);

    gsword_window_set_status(self, message);
    g_free(message);

    if (gtk_widget_get_mapped(GTK_WIDGET(self))) {
        AdwDialog *dialog = adw_alert_dialog_new(prefix, NULL);

        adw_alert_dialog_format_body(ADW_ALERT_DIALOG(dialog), "%s", detail);
        adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog),
                                      "close", _("Close"));
        adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog),
                                              "close");
        adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog),
                                            "close");
        /* AdwDialog is floating; adw_dialog_present() takes ownership. */
        adw_dialog_present(dialog, GTK_WIDGET(self));
    }
    g_clear_error(&error);
}

void
gsword_window_show_page(GswordWindow *self, const gchar *page)
{
    GswordWindowServices *services = get_services(self);

    if (services == NULL || services->content_stack == NULL || page == NULL)
        return;
    adw_view_stack_set_visible_child_name(services->content_stack,
                                           page);
}

gboolean
gsword_window_is_page_visible(GswordWindow *self, const gchar *page)
{
    GswordWindowServices *services = get_services(self);
    const gchar *visible;

    if (services == NULL || services->content_stack == NULL || page == NULL)
        return FALSE;
    visible = adw_view_stack_get_visible_child_name(
        services->content_stack);
    return g_strcmp0(visible, page) == 0;
}

gboolean
gsword_window_module_info_is_devotional(GswModuleInfo *info)
{
    gchar **features;
    const gchar *category;
    gboolean result = FALSE;
    guint i;

    if (info == NULL)
        return FALSE;
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

gboolean
gsword_window_module_info_is_map(GswModuleInfo *info)
{
    const gchar *category;

    if (info == NULL)
        return FALSE;
    if (gsw_module_info_get_module_type(info) == GSW_MODULE_TYPE_MAPS)
        return TRUE;
    category = gsw_module_info_get_value(info, "Category");
    return category != NULL &&
           g_ascii_strncasecmp(category, "Maps", 4) == 0;
}

GswModuleType
gsword_window_module_effective_type(GswModuleInfo *info)
{
    if (gsword_window_module_info_is_devotional(info))
        return GSW_MODULE_TYPE_DAILY_DEVOS;
    if (gsword_window_module_info_is_map(info))
        return GSW_MODULE_TYPE_MAPS;
    return gsw_module_info_get_module_type(info);
}

const gchar *
gsword_window_module_type_display(GswModuleInfo *info)
{
    if (gsword_window_module_info_is_devotional(info))
        return _("Daily Devotional");
    if (gsword_window_module_info_is_map(info))
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

gchar *
gsword_window_chapter_text_for_module_with_missing(GswModule *module,
                                                   gboolean show_missing)
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
            if (!show_missing)
                break;
            g_string_append_printf(result, "%d  %s\n\n", verse,
                                   _("Verse not present in this module."));
            continue;
        }
        text = gsw_module_get_text(module, &error);
        if (text != NULL && *text != '\0') {
            g_string_append_printf(result, "%d  %s\n\n", verse, text);
        } else if (show_missing) {
            g_string_append_printf(result, "%d  %s\n\n", verse,
                                   _("Verse not present in this module."));
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

gchar *
gsword_window_chapter_text_for_module(GswModule *module)
{
    return gsword_window_chapter_text_for_module_with_missing(module, FALSE);
}

void
gsword_window_set_view_text(GtkTextView *view, const gchar *text)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);

    gtk_text_buffer_set_text(buffer, text != NULL ? text : "", -1);
}

gchar *
gsword_window_normalize_reference(const gchar *text)
{
    const gchar *colon;
    const gchar *dash;

    if (text == NULL)
        return NULL;
    colon = strrchr(text, ':');
    if (colon == NULL)
        return g_strdup(text);
    dash = strchr(colon, '-');
    if (dash == NULL)
        return g_strdup(text);
    return g_strndup(text, (gsize)(dash - text));
}

gint
gsword_window_verse_number_from_line(const gchar *line)
{
    const gchar *p = line;

    if (p == NULL)
        return 0;
    while (*p == ' ' || *p == '\t')
        p++;
    if (!g_ascii_isdigit(*p))
        return 0;
    return (gint)g_ascii_strtoll(p, NULL, 10);
}
