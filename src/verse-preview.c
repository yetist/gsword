/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * verse-preview.c: hover preview widget for saved verse references
 */

#include "verse-preview.h"
#include "window-private.h"

#include <glib/gi18n.h>

struct _GswordVersePreview {
    GswordWindow *window;
    GtkWidget *root;
    GtkLabel *reference;
    GtkLabel *text;
    GswModule *module;
    gchar *module_name;
};

GswordVersePreview *
gsword_verse_preview_new(GswordWindow *window)
{
    GswordVersePreview *preview = g_new0(GswordVersePreview, 1);

    preview->window = window;
    preview->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    preview->reference = GTK_LABEL(gtk_label_new(""));
    preview->text = GTK_LABEL(gtk_label_new(
        _("Hover a verse to preview it.")));

    gtk_label_set_xalign(preview->reference, 0.0f);
    gtk_label_set_ellipsize(preview->reference, PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(GTK_WIDGET(preview->reference), "heading");
    gtk_label_set_xalign(preview->text, 0.0f);
    gtk_label_set_yalign(preview->text, 0.0f);
    gtk_label_set_wrap(preview->text, TRUE);
    gtk_label_set_lines(preview->text, 3);
    gtk_label_set_ellipsize(preview->text, PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(preview->root, "card");
    gtk_widget_set_margin_top(preview->root, 4);
    gtk_widget_set_margin_bottom(preview->root, 2);
    gtk_widget_set_margin_start(preview->root, 10);
    gtk_widget_set_margin_end(preview->root, 10);
    gtk_widget_set_margin_top(GTK_WIDGET(preview->reference), 8);
    gtk_widget_set_margin_start(GTK_WIDGET(preview->reference), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(preview->reference), 8);
    gtk_widget_set_margin_start(GTK_WIDGET(preview->text), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(preview->text), 8);
    gtk_widget_set_margin_bottom(GTK_WIDGET(preview->text), 8);
    gtk_box_append(GTK_BOX(preview->root),
                   GTK_WIDGET(preview->reference));
    gtk_box_append(GTK_BOX(preview->root), GTK_WIDGET(preview->text));
    return preview;
}

GtkWidget *
gsword_verse_preview_widget(GswordVersePreview *preview)
{
    g_return_val_if_fail(preview != NULL, NULL);

    return preview->root;
}

void
gsword_verse_preview_reset(GswordVersePreview *preview)
{
    g_return_if_fail(preview != NULL);

    gtk_label_set_text(preview->reference, "");
    gtk_label_set_text(preview->text, _("Hover a verse to preview it."));
}

static gboolean
verse_preview_ensure_module(GswordVersePreview *preview,
                            const gchar *module_name)
{
    GswManager *manager;

    if (module_name == NULL || *module_name == '\0')
        return FALSE;
    if (preview->module != NULL &&
        g_strcmp0(preview->module_name, module_name) == 0)
        return TRUE;
    g_clear_object(&preview->module);
    g_free(preview->module_name);
    preview->module_name = g_strdup(module_name);
    manager = gsword_window_get_manager(preview->window);
    if (manager == NULL)
        return FALSE;
    preview->module = gsw_manager_create_module(manager, module_name);
    return preview->module != NULL;
}

void
gsword_verse_preview_show(GswordVersePreview *preview,
                          const gchar *module_name, const gchar *key,
                          const gchar *display_key)
{
    gchar *text;
    gchar *reference;
    gchar *cursor;

    g_return_if_fail(preview != NULL);

    if (!verse_preview_ensure_module(preview, module_name) ||
        key == NULL ||
        !gsw_module_set_key_text(preview->module, key, NULL)) {
        gtk_label_set_text(preview->reference,
                           display_key != NULL ? display_key
                                               : (key != NULL ? key : ""));
        gtk_label_set_text(preview->text,
                           _("Preview unavailable for this reference."));
        return;
    }
    if (display_key != NULL && *display_key != '\0' && module_name != NULL)
        reference = g_strdup_printf("%s · %s", module_name, display_key);
    else if (display_key != NULL && *display_key != '\0')
        reference = g_strdup(display_key);
    else
        reference = g_strdup(key);
    text = gsw_module_get_text(preview->module, NULL);
    if (text != NULL) {
        g_strstrip(text);
        for (cursor = text; *cursor != '\0'; cursor++) {
            if (*cursor == '\n' || *cursor == '\r' || *cursor == '\t')
                *cursor = ' ';
        }
    }
    gtk_label_set_text(preview->reference, reference);
    gtk_label_set_text(preview->text,
                       text != NULL && *text != '\0'
                           ? text
                           : _("Preview unavailable for this reference."));
    g_free(reference);
    g_free(text);
}

void
gsword_verse_preview_free(GswordVersePreview *preview)
{
    if (preview == NULL)
        return;
    g_clear_object(&preview->module);
    g_free(preview->module_name);
    g_free(preview);
}
