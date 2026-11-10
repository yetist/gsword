/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * reference-selection.c: shared module-to-chooser synchronization
 */

#include "reference-selection.h"
#include "window-private.h"

void
gsword_reference_context_sync(GswordReferenceContext *context,
                              GswordReferenceChooser *chooser,
                              GswordWindow *window, GswModule *module)
{
    GswModuleInfo *info;
    const gchar *versification = NULL;
    gchar *locale;

    if (context == NULL || chooser == NULL || window == NULL || module == NULL)
        return;
    info = gsw_module_get_info(module);
    if (info != NULL)
        versification = gsw_module_info_get_versification(info);
    if (versification == NULL || *versification == '\0')
        versification = "KJV";
    if (g_strcmp0(context->versification, versification) != 0) {
        g_free(context->versification);
        context->versification = g_strdup(versification);
        gsword_reference_chooser_set_versification(chooser, versification);
    }
    locale = gsword_window_book_name_locale(window, info);
    if (g_strcmp0(context->locale, locale) != 0) {
        g_free(context->locale);
        context->locale = g_strdup(locale);
        gsword_reference_chooser_set_locale(chooser, locale);
    }
    g_free(locale);
}

void
gsword_reference_context_clear(GswordReferenceContext *context)
{
    if (context == NULL)
        return;
    g_clear_pointer(&context->versification, g_free);
    g_clear_pointer(&context->locale, g_free);
}

void
gsword_reference_chooser_sync_from_module(
    GswordReferenceChooser *chooser, GswordReferenceContext *context,
    GswordWindow *window, GswModule *module, gint range_start, gint range_end)
{
    GswKey *key;
    GswVerseKey *verse_key;
    gint book;
    gint chapter;
    gint verse;
    gint verse_end = 0;

    if (chooser == NULL)
        return;
    gsword_reference_context_sync(context, chooser, window, module);
    if (module == NULL ||
        gsw_key_get_kind(gsw_module_get_key(module)) != GSW_KEY_KIND_VERSE) {
        gtk_widget_set_sensitive(GTK_WIDGET(chooser), FALSE);
        return;
    }
    gtk_widget_set_sensitive(GTK_WIDGET(chooser), TRUE);
    key = gsw_module_get_key(module);
    verse_key = (GswVerseKey *)key;
    book = gsw_verse_key_get_global_book_number(verse_key);
    chapter = gsw_verse_key_get_chapter(verse_key);
    verse = MAX(gsw_verse_key_get_verse(verse_key), 1);
    if (range_start > 0 && range_end > 0) {
        verse = MIN(range_start, range_end);
        verse_end = MAX(range_start, range_end);
    }
    if (book < 0 || chapter < 1) {
        gtk_widget_set_sensitive(GTK_WIDGET(chooser), FALSE);
        return;
    }
    gsword_reference_chooser_set_reference_range(chooser, book, chapter,
                                                 verse, verse_end);
}
