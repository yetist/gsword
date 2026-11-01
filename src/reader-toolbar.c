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
    GtkButton *back;
    GtkButton *forward;
    GtkMenuButton *history_button;
    GtkPopover *history_popover;
    GtkListBox *history_list;
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

static void
populate_history(GswordReaderToolbar *toolbar)
{
    guint count;
    gint current;
    guint i;

    if (toolbar == NULL || toolbar->history_list == NULL)
        return;
    while (gtk_widget_get_first_child(GTK_WIDGET(toolbar->history_list)) != NULL)
        gtk_list_box_remove(toolbar->history_list,
                            gtk_widget_get_first_child(
                                GTK_WIDGET(toolbar->history_list)));
    count = gsword_reader_page_history_count(toolbar->page);
    current = gsword_reader_page_history_current(toolbar->page);
    if (count == 0) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(_("No reading history yet."));

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_add_css_class(label, "dim-label");
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_append(toolbar->history_list, row);
        return;
    }
    for (i = 0; i < count; i++) {
        const GswordReaderHistoryEntry *entry =
            gsword_reader_page_history_entry(toolbar->page, i);
        GtkWidget *row;
        GtkWidget *box;
        GtkWidget *title;
        GtkWidget *detail;
        gchar *when = NULL;
        gchar *detail_text;
        gchar *title_text;

        if (entry == NULL)
            continue;
        if (entry->visited_at > 0) {
            GDateTime *date = g_date_time_new_from_unix_local(
                entry->visited_at);

            if (date != NULL) {
                when = g_date_time_format(date, "%m-%d %H:%M");
                g_date_time_unref(date);
            }
        }
        title_text = g_strdup_printf("%s%s", current == (gint)i ? "● " : "",
                                     entry->display != NULL
                                     ? entry->display : entry->reference);
        if (entry->module != NULL && when != NULL)
            detail_text = g_strdup_printf("%s · %s", entry->module, when);
        else
            detail_text = g_strdup(entry->module != NULL
                                   ? entry->module
                                   : (when != NULL ? when : ""));
        row = gtk_list_box_row_new();
        box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        title = gtk_label_new(title_text);
        detail = gtk_label_new(detail_text);
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_label_set_xalign(GTK_LABEL(detail), 0.0f);
        gtk_widget_add_css_class(detail, "dim-label");
        gtk_widget_add_css_class(detail, "caption");
        if (current == (gint)i)
            gtk_widget_add_css_class(title, "heading");
        gtk_box_append(GTK_BOX(box), title);
        gtk_box_append(GTK_BOX(box), detail);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data(G_OBJECT(row), "history-index",
                          GINT_TO_POINTER((gint)i + 1));
        gtk_list_box_append(toolbar->history_list, row);
        g_free(title_text);
        g_free(detail_text);
        g_free(when);
    }
}

void
gsword_reader_toolbar_update_history(GswordReaderToolbar *toolbar)
{
    if (toolbar == NULL)
        return;
    if (toolbar->back != NULL)
        gtk_widget_set_sensitive(
            GTK_WIDGET(toolbar->back),
            gsword_reader_page_history_can_back(toolbar->page));
    if (toolbar->forward != NULL)
        gtk_widget_set_sensitive(
            GTK_WIDGET(toolbar->forward),
            gsword_reader_page_history_can_forward(toolbar->page));
    if (toolbar->history_popover != NULL &&
        gtk_widget_get_visible(GTK_WIDGET(toolbar->history_popover)))
        populate_history(toolbar);
}

static void
on_history_back(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_reader_page_history_back(((GswordReaderToolbar *)user_data)->page);
}

static void
on_history_forward(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_reader_page_history_forward(((GswordReaderToolbar *)user_data)->page);
}

static void
on_history_popover_show(GtkPopover *popover, gpointer user_data)
{
    (void)popover;
    populate_history(user_data);
}

static void
on_history_row_activated(GtkListBox *list, GtkListBoxRow *row,
                         gpointer user_data)
{
    GswordReaderToolbar *toolbar = user_data;
    gint index;

    (void)list;
    if (row == NULL)
        return;
    index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "history-index"));
    if (index <= 0)
        return;
    if (gsword_reader_page_history_jump(toolbar->page, (guint)index - 1))
        gtk_popover_popdown(toolbar->history_popover);
}

static void
on_history_clear(GtkButton *button, gpointer user_data)
{
    GswordReaderToolbar *toolbar = user_data;

    (void)button;
    gsword_reader_page_history_clear(toolbar->page);
    populate_history(toolbar);
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
    GtkWidget *back;
    GtkWidget *forward;
    GtkWidget *history_button;
    GtkWidget *history_popover;
    GtkWidget *history_box;
    GtkWidget *history_title;
    GtkWidget *history_list;
    GtkWidget *history_scroll;
    GtkWidget *history_clear;
    gchar *locale;

    toolbar->window = window;
    toolbar->page = page;
    toolbar->root = bar;

    gtk_widget_set_margin_top(bar, 8);
    gtk_widget_set_margin_bottom(bar, 8);
    gtk_widget_set_margin_start(bar, 12);
    gtk_widget_set_margin_end(bar, 12);

    back = gtk_button_new_from_icon_name("go-previous-symbolic");
    forward = gtk_button_new_from_icon_name("go-next-symbolic");
    history_button = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(history_button),
                                  "view-list-symbolic");
    gtk_widget_add_css_class(history_button, "flat");
    gtk_widget_set_tooltip_text(history_button, _("Recent reading"));
    history_popover = gtk_popover_new();
    gtk_popover_set_autohide(GTK_POPOVER(history_popover), TRUE);
    history_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    history_title = gtk_label_new(_("Recent reading"));
    gtk_label_set_xalign(GTK_LABEL(history_title), 0.0f);
    gtk_widget_add_css_class(history_title, "heading");
    history_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(history_list),
                                    GTK_SELECTION_NONE);
    history_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(history_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(history_scroll, 320, 260);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(history_scroll),
                                  history_list);
    history_clear = gtk_button_new_with_label(_("Clear history"));
    g_signal_connect(history_clear, "clicked",
                     G_CALLBACK(on_history_clear), toolbar);
    gtk_box_append(GTK_BOX(history_box), history_title);
    gtk_box_append(GTK_BOX(history_box), history_scroll);
    gtk_box_append(GTK_BOX(history_box), history_clear);
    gtk_popover_set_child(GTK_POPOVER(history_popover), history_box);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(history_button),
                                 history_popover);
    toolbar->back = GTK_BUTTON(back);
    toolbar->forward = GTK_BUTTON(forward);
    toolbar->history_button = GTK_MENU_BUTTON(history_button);
    toolbar->history_popover = GTK_POPOVER(history_popover);
    toolbar->history_list = GTK_LIST_BOX(history_list);
    gtk_widget_set_tooltip_text(back, _("Back (Alt+Left)"));
    gtk_widget_set_tooltip_text(forward, _("Forward (Alt+Right)"));
    g_signal_connect(back, "clicked", G_CALLBACK(on_history_back), toolbar);
    g_signal_connect(forward, "clicked", G_CALLBACK(on_history_forward),
                     toolbar);
    g_signal_connect(history_popover, "show",
                     G_CALLBACK(on_history_popover_show), toolbar);
    g_signal_connect(history_list, "row-activated",
                     G_CALLBACK(on_history_row_activated), toolbar);

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

    gtk_box_append(GTK_BOX(bar), back);
    gtk_box_append(GTK_BOX(bar), forward);
    gtk_box_append(GTK_BOX(bar), history_button);
    gtk_box_append(GTK_BOX(bar), GTK_WIDGET(toolbar->reference_chooser));
    gtk_box_append(GTK_BOX(bar), note);
    gtk_box_append(GTK_BOX(bar), highlight);
    gtk_box_append(GTK_BOX(bar), teaching);
    gtk_box_append(GTK_BOX(bar), study_button);
    gtk_box_append(GTK_BOX(bar), copy);

    g_object_set_data_full(G_OBJECT(bar), "gsword-reader-toolbar", toolbar,
                           g_free);
    gsword_reader_toolbar_update_history(toolbar);
    return toolbar;
}
