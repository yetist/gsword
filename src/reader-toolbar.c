/*
 * reader-toolbar.c: reader toolbar for the gsword window
 */

#include "reader-toolbar.h"
#include "reader-page.h"
#include "window-private.h"

#include <glib/gi18n.h>

struct _GswordReaderToolbar {
    GswordWindow *window;
    GswordReaderPage *page;
    GtkWidget *root;
    GswordReferenceChooser *reference_chooser;
    GtkToggleButton *study_toggle;
};

static void
on_reference_activate(GswordReferenceChooser *chooser, gpointer user_data)
{
    GswordReaderToolbar *toolbar = user_data;

    (void)chooser;
    gsword_reader_page_activate_reference(toolbar->page);
}

static void
on_copy_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_reader_page_copy(((GswordReaderToolbar *)user_data)->page);
}

static void
on_note_clicked(GtkButton *button, gpointer user_data)
{
    GswordReaderToolbar *toolbar = user_data;

    (void)button;
    gsword_window_open_current_note(toolbar->window);
}

static void
on_highlight_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_reader_page_toggle_highlight(
        ((GswordReaderToolbar *)user_data)->page);
}

static void
on_teaching_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_open_teaching(((GswordReaderToolbar *)user_data)->window);
}

GtkWidget *
gsword_reader_toolbar_widget(GswordReaderToolbar *toolbar)
{
    g_return_val_if_fail(toolbar != NULL, NULL);
    return toolbar->root;
}

GswordReferenceChooser *
gsword_reader_toolbar_get_reference_chooser(GswordReaderToolbar *toolbar)
{
    return toolbar != NULL ? toolbar->reference_chooser : NULL;
}

GtkToggleButton *
gsword_reader_toolbar_get_study_toggle(GswordReaderToolbar *toolbar)
{
    return toolbar != NULL ? toolbar->study_toggle : NULL;
}

GswordReaderToolbar *
gsword_reader_toolbar_new(GswordWindow *window, GswordReaderPage *page)
{
    GswordReaderToolbar *toolbar = g_new0(GswordReaderToolbar, 1);
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *copy = gtk_button_new_from_icon_name("edit-copy-symbolic");
    GtkWidget *note = gtk_button_new_from_icon_name("document-edit-symbolic");
    GtkWidget *highlight = gtk_button_new_from_icon_name(
        "emblem-favorite-symbolic");
    GtkWidget *teaching = gtk_button_new_with_label(_("Teach"));
    GtkWidget *study_button = gtk_toggle_button_new();
    gchar *locale;

    toolbar->window = window;
    toolbar->page = page;
    toolbar->root = bar;

    gtk_widget_set_margin_top(bar, 8);
    gtk_widget_set_margin_bottom(bar, 8);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);

    locale = gsword_window_locale_name();
    toolbar->reference_chooser = GSWORD_REFERENCE_CHOOSER(
        gsword_reference_chooser_new("KJV", locale));
    g_free(locale);
    gtk_widget_set_hexpand(GTK_WIDGET(toolbar->reference_chooser), TRUE);
    g_signal_connect(toolbar->reference_chooser, "activate",
                     G_CALLBACK(on_reference_activate), toolbar);
    g_signal_connect(copy, "clicked", G_CALLBACK(on_copy_clicked), toolbar);
    g_signal_connect(note, "clicked", G_CALLBACK(on_note_clicked), toolbar);
    g_signal_connect(highlight, "clicked",
                     G_CALLBACK(on_highlight_clicked), toolbar);
    g_signal_connect(teaching, "clicked",
                     G_CALLBACK(on_teaching_clicked), toolbar);
    gtk_widget_set_tooltip_text(teaching,
                                _("Open teaching layout for this passage"));
    gtk_button_set_icon_name(GTK_BUTTON(study_button),
                             "dialog-information-symbolic");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(study_button), TRUE);
    toolbar->study_toggle = GTK_TOGGLE_BUTTON(study_button);
    gtk_widget_set_tooltip_text(copy, _("Copy entry"));
    gtk_widget_set_tooltip_text(note, _("Open note for this verse"));
    gtk_widget_set_tooltip_text(highlight, _("Toggle verse highlight"));
    gtk_widget_set_tooltip_text(study_button, _("Toggle study panel"));

    gtk_box_append(GTK_BOX(bar), GTK_WIDGET(toolbar->reference_chooser));
    gtk_box_append(GTK_BOX(bar), note);
    gtk_box_append(GTK_BOX(bar), highlight);
    gtk_box_append(GTK_BOX(bar), teaching);
    gtk_box_append(GTK_BOX(bar), study_button);
    gtk_box_append(GTK_BOX(bar), copy);

    g_object_set_data_full(G_OBJECT(bar), "gsword-reader-toolbar", toolbar,
                           g_free);
    return toolbar;
}
