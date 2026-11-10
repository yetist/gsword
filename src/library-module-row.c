/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * library-module-row.c: module row model and display helpers
 */

#include "library-module-row.h"
#include "window-private.h"

#include <glib/gi18n.h>

#include <string.h>

typedef struct _GswordModuleRowClass {
    GObjectClass parent_class;
} GswordModuleRowClass;

G_DEFINE_TYPE(GswordModuleRow, gsword_module_row, G_TYPE_OBJECT)

static void
gsword_module_row_finalize(GObject *object)
{
    GswordModuleRow *row = (GswordModuleRow *)object;

    g_free(row->name);
    g_free(row->description);
    g_free(row->detail);
    g_free(row->language);
    G_OBJECT_CLASS(gsword_module_row_parent_class)->finalize(object);
}

static void
gsword_module_row_class_init(GswordModuleRowClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = gsword_module_row_finalize;
}

static void
gsword_module_row_init(GswordModuleRow *row)
{
    (void)row;
}

static const gchar *
language_name(const gchar *code)
{
    if (code == NULL || *code == '\0')
        return _("Unknown");
    if (g_str_has_prefix(code, "zh"))
        return _("Chinese");
    if (g_str_has_prefix(code, "en"))
        return _("English");
    if (g_str_has_prefix(code, "de"))
        return _("German");
    if (g_str_has_prefix(code, "fr"))
        return _("French");
    if (g_str_has_prefix(code, "es"))
        return _("Spanish");
    if (g_str_has_prefix(code, "pt"))
        return _("Portuguese");
    if (g_str_has_prefix(code, "ru"))
        return _("Russian");
    if (g_str_has_prefix(code, "ko"))
        return _("Korean");
    if (g_str_has_prefix(code, "ja"))
        return _("Japanese");
    if (g_str_has_prefix(code, "pl"))
        return _("Polish");
    if (g_str_has_prefix(code, "uk"))
        return _("Ukrainian");
    if (g_str_has_prefix(code, "ar"))
        return _("Arabic");
    return code;
}

gchar *
gsword_library_language_display_name(const gchar *code)
{
    const gchar *name = language_name(code);

    if (g_strcmp0(name, code) == 0)
        return g_strdup(code != NULL ? code : _("Unknown"));
    return g_strdup_printf("%s (%s)", name, code);
}

GswModuleType
gsword_library_filter_type_for_info(GswModuleInfo *info)
{
    GswModuleType type;

    if (gsword_window_module_info_is_devotional(info))
        return GSW_MODULE_TYPE_DAILY_DEVOS;
    if (gsword_window_module_info_is_map(info))
        return GSW_MODULE_TYPE_MAPS;
    type = gsw_module_info_get_module_type(info);
    return type == GSW_MODULE_TYPE_UNKNOWN ? GSW_MODULE_TYPE_OTHER : type;
}

const gchar *
gsword_library_module_status_text(guint status)
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

GswordModuleRow *
gsword_module_row_new(GswModuleInfo *info, guint status, gboolean remote)
{
    GswordModuleRow *row = g_object_new(gsword_module_row_get_type(), NULL);
    gchar *language = gsword_library_language_display_name(
        gsw_module_info_get_language(info));

    row->name = g_strdup(gsw_module_info_get_name(info));
    row->description = g_strdup(gsw_module_info_get_description(info));
    row->language = g_strdup(gsw_module_info_get_language(info));
    row->type = gsword_library_filter_type_for_info(info);
    row->status = status;
    row->remote = remote;
    row->detail = remote
        ? g_strdup_printf("%s · %s · %s · %s",
            gsword_window_module_type_display(info), language,
            gsw_module_info_get_version(info) != NULL
            ? gsw_module_info_get_version(info) : "?",
            gsword_library_module_status_text(status))
        : g_strdup_printf("%s · %s · %s",
            gsword_window_module_type_display(info), language,
            gsw_module_info_get_version(info) != NULL
            ? gsw_module_info_get_version(info) : "?");
    g_free(language);
    return row;
}

gboolean
gsword_module_row_matches_query(GswordModuleRow *row,
                                const gchar *folded_query)
{
    gchar *folded_name;
    gchar *folded_description;
    gboolean match;

    if (folded_query == NULL)
        return TRUE;
    folded_name = g_utf8_casefold(row->name != NULL ? row->name : "", -1);
    folded_description = g_utf8_casefold(
        row->description != NULL ? row->description : "", -1);
    match = strstr(folded_name, folded_query) != NULL ||
            strstr(folded_description, folded_query) != NULL;
    g_free(folded_name);
    g_free(folded_description);
    return match;
}
