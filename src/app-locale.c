/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * app-locale.c: process locale access for the gsword application
 *
 * This is the only application layer that derives a locale from the process
 * environment. Library and content code receive the resulting locale
 * explicitly.
 */

#include "app-locale.h"

#include <string.h>

gchar *
gsword_app_locale_name(void)
{
    const gchar * const *names = g_get_language_names();
    gchar *locale;
    gchar *suffix;

    if (names == NULL || names[0] == NULL || *names[0] == '\0')
        return g_strdup("en");
    locale = g_strdup(names[0]);
    suffix = strchr(locale, '.');
    if (suffix != NULL)
        *suffix = '\0';
    suffix = strchr(locale, '@');
    if (suffix != NULL)
        *suffix = '\0';
    if (g_strcmp0(locale, "C") == 0 || g_strcmp0(locale, "POSIX") == 0) {
        g_free(locale);
        return g_strdup("en");
    }
    return locale;
}
