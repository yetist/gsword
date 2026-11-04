/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
#include "application.h"

#include <glib/gi18n.h>
#include <locale.h>

int
main(int argc, char **argv)
{
    GswordApplication *application;
    const gchar *localedir;
    int status;

    setlocale(LC_ALL, "");
    g_set_application_name("GSword");
    localedir = g_getenv("GSW_LOCALEDIR");
    if (localedir == NULL || *localedir == '\0')
        localedir = GSW_LOCALEDIR;
    bindtextdomain(GETTEXT_PACKAGE, localedir);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    application = gsword_application_new();
    status = g_application_run(G_APPLICATION(application), argc, argv);
    g_object_unref(application);
    return status;
}
