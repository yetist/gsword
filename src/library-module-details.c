/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * library-module-details.c: module detail formatting helpers
 */

#include "library-module-details.h"
#include "library-module-row.h"
#include "window-private.h"

#include <glib/gi18n.h>

#include <stdarg.h>

gchar *
gsword_library_format_size(guint64 bytes)
{
    if (bytes >= G_GUINT64_CONSTANT(1024) * 1024 * 1024)
        return g_strdup_printf("%.1f GB", (gdouble)bytes /
                               (1024.0 * 1024.0 * 1024.0));
    if (bytes >= G_GUINT64_CONSTANT(1024) * 1024)
        return g_strdup_printf("%.1f MB", (gdouble)bytes /
                               (1024.0 * 1024.0));
    if (bytes >= G_GUINT64_CONSTANT(1024))
        return g_strdup_printf("%.1f KB", (gdouble)bytes / 1024.0);
    return g_strdup_printf("%" G_GUINT64_FORMAT " B", bytes);
}

static void
append_module_warning(gchar **warning, const gchar *format, ...)
{
    va_list args;
    gchar *line;
    gchar *combined;

    va_start(args, format);
    line = g_strdup_vprintf(format, args);
    va_end(args);
    if (*warning == NULL) {
        *warning = line;
    } else {
        combined = g_strconcat(*warning, "\n", line, NULL);
        g_free(*warning);
        g_free(line);
        *warning = combined;
    }
}

gchar *
gsword_library_format_module_meta(GswModuleInfo *info,
                                  const gchar *version_text,
                                  const gchar *source_text)
{
    gchar *language = gsword_library_language_display_name(
        gsw_module_info_get_language(info));
    gchar *size = gsword_library_format_size(
        gsw_module_info_get_install_size(info));
    const gchar *license = gsw_module_info_get_distribution_license(info);
    const gchar *type = gsword_window_module_type_display(info);
    gchar *meta = g_strdup_printf(
        _("Type: %s\nLanguage: %s\nVersion: %s\nSize: %s\nSource: %s\nLicense: %s"),
        type != NULL ? type : _("Unknown"), language,
        version_text != NULL ? version_text : "?", size,
        source_text != NULL ? source_text : _("Unknown"),
        license != NULL && *license != '\0' ? license : _("Not specified"));

    g_free(language);
    g_free(size);
    return meta;
}

gchar *
gsword_library_format_module_warning(GswModuleInfo *info)
{
    gchar *warning = NULL;

    if (!gsw_module_info_is_supported(info))
        warning = g_strdup(
            _("⚠ This module may not be readable by this version of gsword."));
    if (gsw_module_info_get_value(info, "Requires") != NULL)
        append_module_warning(&warning, _("Requires: %s"),
                              gsw_module_info_get_value(info, "Requires"));
    if (gsw_module_info_get_value(info, "Conflicts") != NULL)
        append_module_warning(&warning, _("Conflicts: %s"),
                              gsw_module_info_get_value(info, "Conflicts"));
    if (gsw_module_info_is_ciphered(info))
        append_module_warning(
            &warning, "%s",
            _("⚠ This module is encrypted and may require unlocking."));
    return warning;
}

gchar *
gsword_library_format_module_technical(GswModuleInfo *info,
                                       const gchar *name,
                                       gboolean include_minimum)
{
    GString *technical = g_string_new(NULL);
    const gchar *value;

    value = gsw_module_info_get_driver_name(info);
    g_string_append_printf(technical, _("Driver: %s\n"),
                           value != NULL ? value : _("Unknown"));
    value = gsw_module_info_get_encoding(info);
    g_string_append_printf(technical, _("Encoding: %s\n"),
                           value != NULL ? value : _("Unknown"));
    value = gsw_module_info_get_versification(info);
    g_string_append_printf(technical, _("Versification: %s\n"),
                           value != NULL ? value : _("Unknown"));
    if (include_minimum) {
        value = gsw_module_info_get_minimum_version(info);
        g_string_append_printf(technical, _("Minimum SWORD version: %s\n"),
                               value != NULL ? value : _("Unknown"));
    }
    g_string_append_printf(technical, _("Module ID: %s\n"), name);
    value = gsw_module_info_get_config_file(info);
    g_string_append_printf(technical, _("Config file: %s\n"),
                           value != NULL ? value : _("Unknown"));
    value = gsw_module_info_get_absolute_data_path(info);
    g_string_append_printf(technical, _("Data path: %s"),
                           value != NULL ? value : _("Unknown"));
    return g_string_free(technical, FALSE);
}

gchar *
gsword_library_format_module_license(GswModuleInfo *info)
{
    const gchar *license = gsw_module_info_get_distribution_license(info);
    const gchar *copyright = gsw_module_info_get_copyright(info);

    return g_strdup_printf(
        _("License: %s\n\nCopyright: %s"),
        license != NULL && *license != '\0' ? license : _("Not specified"),
        copyright != NULL && *copyright != '\0'
        ? copyright : _("Not specified"));
}
