/*
 * reader-toolbar.c: reader toolbar for the gsword window
 */

#include "app-locale.h"
#include "reader-toolbar.h"
#include "reader-page.h"
#include "preferences-dialog.h"
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
    GtkMenuButton *list_button;
    GtkBox *list_menu_box;
    GtkWidget *copy_button;
    GtkWidget *note_button;
    GtkWidget *highlight_button;
    GtkWidget *focus_button;
    GtkWidget *more_button;
    GtkWidget *typography_button;
    GtkToggleButton *study_button;
    gboolean updating_list_menu;
    gboolean updating_focus;
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
on_copy_chapter_clicked(GtkButton *button, gpointer user_data)
{
    GswordReaderToolbar *toolbar = user_data;

    (void)button;
    gsword_reader_page_copy_chapter(toolbar->page);
}

static void
on_focus_toggled(GtkToggleButton *button, gpointer user_data)
{
    GswordReaderToolbar *toolbar = user_data;

    if (toolbar->updating_focus)
        return;
    gsword_reader_page_set_focus_mode(
        toolbar->page, gtk_toggle_button_get_active(button));
}

static void
on_font_scale_changed(GtkSpinButton *spin, gpointer user_data)
{
    GswordReaderToolbar *toolbar = user_data;

    gsword_preferences_set_font_scale(
        toolbar->window, gtk_spin_button_get_value(spin));
}

static void
on_paragraph_spacing_changed(GtkSpinButton *spin, gpointer user_data)
{
    GswordReaderToolbar *toolbar = user_data;

    gsword_reader_page_set_paragraph_spacing(
        toolbar->page, gtk_spin_button_get_value_as_int(spin));
}

static void
on_reading_width_changed(GtkSpinButton *spin, gpointer user_data)
{
    GswordReaderToolbar *toolbar = user_data;

    gsword_reader_page_set_reading_width(
        toolbar->page, gtk_spin_button_get_value_as_int(spin));
}

static GtkWidget *
make_typography_row(GtkGrid *grid, gint row, const gchar *label_text,
                    GtkWidget *spin)
{
    GtkWidget *label = gtk_label_new(label_text);

    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    gtk_grid_attach(grid, spin, 1, row, 1, 1);
    return spin;
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
on_list_check_toggled(GtkCheckButton *button, gpointer user_data)
{
    GswordReaderToolbar *toolbar = user_data;
    GswordNotesStore *store = gsword_window_get_notes_store(toolbar->window);
    GswModule *module = gsword_window_get_module(toolbar->window);
    gchar *storage_key = gsword_window_current_storage_key(toolbar->window);
    gchar *display_key = gsword_window_current_display_key(toolbar->window);
    const gchar *id_text = g_object_get_data(G_OBJECT(button), "list-id");
    gint64 list_id;
    GError *error = NULL;
    gboolean active = gtk_check_button_get_active(button);

    if (toolbar->updating_list_menu || store == NULL || module == NULL ||
        storage_key == NULL || id_text == NULL) {
        g_free(storage_key);
        g_free(display_key);
        return;
    }
    list_id = g_ascii_strtoll(id_text, NULL, 10);
    if (active) {
        if (!gsword_notes_store_add_to_list(
                store, list_id, gsw_module_get_name(module), storage_key,
                display_key, &error)) {
            gsword_window_show_error(toolbar->window,
                                     _("Cannot add verse to list"), error);
            g_free(storage_key);
            g_free(display_key);
            return;
        }
    } else {
        if (!gsword_notes_store_remove_from_list(
                store, list_id, gsw_module_get_name(module), storage_key,
                &error)) {
            gsword_window_show_error(toolbar->window,
                                     _("Cannot remove verse from list"), error);
            g_free(storage_key);
            g_free(display_key);
            return;
        }
    }
    gsword_window_set_status(toolbar->window, active
                             ? _("Verse added to list")
                             : _("Verse removed from list"));
    g_free(storage_key);
    g_free(display_key);
}

static void
rebuild_list_menu(GswordReaderToolbar *toolbar)
{
    GswordNotesStore *store = gsword_window_get_notes_store(toolbar->window);
    GswModule *module = gsword_window_get_module(toolbar->window);
    gchar *storage_key = gsword_window_current_storage_key(toolbar->window);
    GPtrArray *lists;
    GtkWidget *child;
    guint i;

    if (toolbar->list_menu_box == NULL)
        return;
    toolbar->updating_list_menu = TRUE;
    while ((child = gtk_widget_get_first_child(
                GTK_WIDGET(toolbar->list_menu_box))) != NULL)
        gtk_box_remove(toolbar->list_menu_box, child);
    if (store == NULL || module == NULL || storage_key == NULL) {
        GtkWidget *label = gtk_label_new(_("Open a verse first"));

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_set_margin_top(label, 6);
        gtk_widget_set_margin_bottom(label, 6);
        gtk_widget_add_css_class(label, "dim-label");
        gtk_box_append(toolbar->list_menu_box, label);
        g_free(storage_key);
        toolbar->updating_list_menu = FALSE;
        return;
    }
    lists = gsword_notes_store_list_lists(store);
    if (lists->len == 0) {
        GtkWidget *label = gtk_label_new(_("No verse lists yet"));

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_set_margin_top(label, 6);
        gtk_widget_set_margin_bottom(label, 6);
        gtk_widget_add_css_class(label, "dim-label");
        gtk_box_append(toolbar->list_menu_box, label);
    }
    for (i = 0; i < lists->len; i++) {
        GswordVerseList *list = g_ptr_array_index(lists, i);
        GtkWidget *check = gtk_check_button_new_with_label(list->name);
        gchar *id = g_strdup_printf("%" G_GINT64_FORMAT, list->id);

        g_object_set_data_full(G_OBJECT(check), "list-id", id, g_free);
        gtk_check_button_set_active(
            GTK_CHECK_BUTTON(check),
            gsword_notes_store_is_in_list(store, list->id,
                                          gsw_module_get_name(module),
                                          storage_key));
        g_signal_connect(check, "toggled",
                         G_CALLBACK(on_list_check_toggled), toolbar);
        gtk_box_append(toolbar->list_menu_box, check);
    }
    g_ptr_array_unref(lists);
    g_free(storage_key);
    toolbar->updating_list_menu = FALSE;
}

static void
on_list_popover_show(GtkPopover *popover, gpointer user_data)
{
    (void)popover;
    rebuild_list_menu(user_data);
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

void
gsword_reader_toolbar_set_focus_mode(GswordReaderToolbar *toolbar,
                                     gboolean focus)
{
    if (toolbar == NULL || toolbar->updating_focus)
        return;
    toolbar->updating_focus = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->focus_button),
                                 focus);
    gtk_widget_set_visible(toolbar->copy_button, !focus);
    gtk_widget_set_visible(toolbar->note_button, !focus);
    gtk_widget_set_visible(GTK_WIDGET(toolbar->list_button), !focus);
    gtk_widget_set_visible(toolbar->highlight_button, !focus);
    gtk_widget_set_visible(GTK_WIDGET(toolbar->study_button), !focus);
    gtk_widget_set_visible(toolbar->typography_button, !focus);
    gtk_widget_set_visible(toolbar->more_button, !focus);
    toolbar->updating_focus = FALSE;
}

void
gsword_reader_toolbar_set_available(GswordReaderToolbar *toolbar,
                                    gboolean available)
{
    if (toolbar == NULL)
        return;
    gtk_widget_set_sensitive(toolbar->copy_button, available);
    gtk_widget_set_sensitive(toolbar->note_button, available);
    gtk_widget_set_sensitive(GTK_WIDGET(toolbar->list_button), available);
    gtk_widget_set_sensitive(toolbar->highlight_button, available);
    gtk_widget_set_sensitive(GTK_WIDGET(toolbar->study_button), available);
    gtk_widget_set_sensitive(toolbar->more_button, available);
}

GswordReaderToolbar *
gsword_reader_toolbar_new(GswordWindow *window, GswordReaderPage *page)
{
    GswordReaderToolbar *toolbar = g_new0(GswordReaderToolbar, 1);
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *copy = gtk_button_new_from_icon_name("edit-copy-symbolic");
    GtkWidget *note = gtk_button_new_from_icon_name("document-edit-symbolic");
    GtkWidget *highlight = gtk_button_new_with_label(_("Highlight verse"));
    GtkWidget *list_add = gtk_menu_button_new();
    GtkWidget *teaching = gtk_button_new_with_label(_("Teach"));
    GtkWidget *study_button = gtk_toggle_button_new();
    GtkWidget *focus_button = gtk_toggle_button_new();
    GtkWidget *more_button = gtk_menu_button_new();
    GtkWidget *typography = gtk_menu_button_new();
    GtkWidget *more_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *copy_chapter = gtk_button_new_with_label(_("Copy chapter"));
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
    gtk_widget_set_margin_start(bar, 8);
    gtk_widget_set_margin_end(bar, 8);

    back = gtk_button_new_from_icon_name("go-previous-symbolic");
    forward = gtk_button_new_from_icon_name("go-next-symbolic");
    history_button = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(history_button),
                                  "view-list-symbolic");
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(list_add),
                                  "list-add-symbolic");
    gtk_widget_add_css_class(list_add, "flat");
    gtk_widget_set_tooltip_text(list_add, _("Add verse to list"));
    toolbar->list_menu_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
    {
        GtkWidget *popover = gtk_popover_new();

        gtk_popover_set_child(GTK_POPOVER(popover),
                              GTK_WIDGET(toolbar->list_menu_box));
        gtk_menu_button_set_popover(GTK_MENU_BUTTON(list_add), popover);
        g_signal_connect(popover, "show",
                         G_CALLBACK(on_list_popover_show), toolbar);
    }
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(more_button),
                                  "open-menu-symbolic");
    gtk_widget_add_css_class(more_button, "flat");
    gtk_widget_set_tooltip_text(more_button, _("More reading actions"));
    gtk_widget_add_css_class(copy_chapter, "flat");
    gtk_widget_add_css_class(teaching, "flat");
    g_signal_connect(copy_chapter, "clicked",
                     G_CALLBACK(on_copy_chapter_clicked), toolbar);
    gtk_box_append(GTK_BOX(more_box), copy_chapter);
    gtk_box_append(GTK_BOX(more_box), highlight);
    gtk_box_append(GTK_BOX(more_box), teaching);
    {
        GtkWidget *popover = gtk_popover_new();

        gtk_popover_set_child(GTK_POPOVER(popover), more_box);
        gtk_menu_button_set_popover(GTK_MENU_BUTTON(more_button), popover);
    }
    toolbar->more_button = more_button;
    toolbar->list_button = GTK_MENU_BUTTON(list_add);
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

    locale = gsword_app_locale_name();
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
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(study_button), FALSE);
    toolbar->study_toggle = GTK_TOGGLE_BUTTON(study_button);
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(typography),
                                   "font-x-generic-symbolic");
    gtk_widget_add_css_class(typography, "flat");
    gtk_widget_set_tooltip_text(typography, _("Reading appearance"));
    {
        GtkWidget *popover = gtk_popover_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        GtkWidget *title = gtk_label_new(_("Reading appearance"));
        GtkWidget *grid = gtk_grid_new();
        GtkWidget *font_spin;
        GtkWidget *spacing_spin;
        GtkWidget *width_spin;
        GtkAdjustment *adjustment;

        gtk_widget_add_css_class(title, "heading");
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_widget_set_margin_top(box, 12);
        gtk_widget_set_margin_bottom(box, 12);
        gtk_widget_set_margin_start(box, 12);
        gtk_widget_set_margin_end(box, 12);
        gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 12);

        adjustment = gtk_adjustment_new(
            gsword_preferences_get_font_scale(window), 0.75, 2.5, 0.1, 0.25, 0);
        font_spin = gtk_spin_button_new(adjustment, 0.1, 1);
        g_signal_connect(font_spin, "value-changed",
                         G_CALLBACK(on_font_scale_changed), toolbar);

        adjustment = gtk_adjustment_new(
            gsword_reader_page_get_paragraph_spacing(page), 0, 24, 2, 8, 0);
        spacing_spin = gtk_spin_button_new(adjustment, 1, 0);
        g_signal_connect(spacing_spin, "value-changed",
                         G_CALLBACK(on_paragraph_spacing_changed), toolbar);

        adjustment = gtk_adjustment_new(
            gsword_reader_page_get_reading_width(page), 480, 1200, 40, 160, 0);
        width_spin = gtk_spin_button_new(adjustment, 20, 0);
        g_signal_connect(width_spin, "value-changed",
                         G_CALLBACK(on_reading_width_changed), toolbar);

        make_typography_row(GTK_GRID(grid), 0, _("Font size"), font_spin);
        make_typography_row(GTK_GRID(grid), 1, _("Paragraph spacing"),
                            spacing_spin);
        make_typography_row(GTK_GRID(grid), 2, _("Reading width"), width_spin);
        gtk_box_append(GTK_BOX(box), title);
        gtk_box_append(GTK_BOX(box), grid);
        gtk_popover_set_child(GTK_POPOVER(popover), box);
        gtk_menu_button_set_popover(GTK_MENU_BUTTON(typography), popover);
    }
    toolbar->typography_button = typography;
    gtk_button_set_icon_name(GTK_BUTTON(focus_button),
                             "view-fullscreen-symbolic");
    gtk_widget_add_css_class(focus_button, "flat");
    gtk_widget_set_tooltip_text(focus_button,
                                _("Focus reading mode (F11)"));
    g_signal_connect(focus_button, "toggled",
                     G_CALLBACK(on_focus_toggled), toolbar);
    toolbar->focus_button = focus_button;
    toolbar->study_button = GTK_TOGGLE_BUTTON(study_button);
    toolbar->copy_button = copy;
    toolbar->note_button = note;
    toolbar->highlight_button = highlight;
    gtk_widget_set_tooltip_text(copy, _("Copy selection or current verse"));
    gtk_widget_set_tooltip_text(note, _("Open note for this verse"));
    gtk_widget_add_css_class(highlight, "flat");
    gtk_widget_set_tooltip_text(study_button, _("Toggle study panel"));

    gtk_box_append(GTK_BOX(bar), back);
    gtk_box_append(GTK_BOX(bar), forward);
    gtk_box_append(GTK_BOX(bar), history_button);
    gtk_box_append(GTK_BOX(bar), GTK_WIDGET(toolbar->reference_chooser));
    gtk_box_append(GTK_BOX(bar), copy);
    gtk_box_append(GTK_BOX(bar), note);
    gtk_box_append(GTK_BOX(bar), list_add);
    gtk_box_append(GTK_BOX(bar), study_button);
    gtk_box_append(GTK_BOX(bar), typography);
    gtk_box_append(GTK_BOX(bar), focus_button);
    gtk_box_append(GTK_BOX(bar), more_button);

    g_object_set_data_full(G_OBJECT(bar), "gsword-reader-toolbar", toolbar,
                           g_free);
    gsword_reader_toolbar_update_history(toolbar);
    return toolbar;
}
