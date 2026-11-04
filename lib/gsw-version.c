/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-version.c: version comparison compatible with SWORD SWVersion
 */

#include <gsword/gsw-version.h>

#include <string.h>

#define GSW_VERSION_PARTS 4

static void
gsw_version_parse(const gchar *text, gint parts[GSW_VERSION_PARTS])
{
    const gchar *p;
    gint i;

    for (i = 0; i < GSW_VERSION_PARTS; i++)
        parts[i] = -1;

    if (text == NULL)
        return;

    p = text;
    i = 0;
    while (i < GSW_VERSION_PARTS && p != NULL) {
        const gchar *dot = strchr(p, '.');
        gsize len = (dot != NULL) ? (gsize)(dot - p) : strlen(p);

        if (len > 0) {
            gchar *token = g_strndup(p, len);
            parts[i++] = atoi(token);
            g_free(token);
        }

        p = (dot != NULL) ? dot + 1 : NULL;
    }
}

gint
gsw_version_compare(const gchar *a, const gchar *b)
{
    gint pa[GSW_VERSION_PARTS];
    gint pb[GSW_VERSION_PARTS];
    gint i;

    gsw_version_parse(a, pa);
    gsw_version_parse(b, pb);

    for (i = 0; i < GSW_VERSION_PARTS; i++) {
        if (pa[i] != pb[i])
            return pa[i] - pb[i];
    }
    return 0;
}
