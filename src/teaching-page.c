/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * teaching-page.c: teaching workspace for the gsword window
 */

#include "teaching-page.h"
#include "window-private.h"

#include <glib/gi18n.h>

#include <string.h>

struct _GswordTeachingPage {
    GswordPage parent_instance;

    GtkTextView *view;
    GtkTextView *notes;
    GtkEntry *ref_entry;
    GtkButton *fullscreen_button;
    gchar *module_name;
    gchar *reference;
    gboolean fullscreen;
};

G_DEFINE_TYPE(GswordTeachingPage, gsword_teaching_page, GSWORD_TYPE_PAGE)

#define TEACHING_WINDOW(page) \
    gsword_page_get_window(GSWORD_PAGE(page))

static gchar *
current_chapter_reference(GswordWindow *window)
{
    GswModule *module = gsword_window_get_module(window);
    GswKey *key;
    gchar *reference;
    gchar *dot;

    if (module == NULL)
        return NULL;
    key = gsw_module_get_key(module);
    if (gsw_key_get_kind(key) != GSW_KEY_KIND_VERSE)
        return g_strdup(gsw_module_get_key_text(module));
    reference = gsw_verse_key_get_osis_ref((GswVerseKey *)key);
    if (reference == NULL)
        return NULL;
    dot = strrchr(reference, '.');
    if (dot != NULL)
        *dot = '\0';
    return reference;
}

static gchar *
current_chapter_display(GswordWindow *window)
{
    GswModule *module = gsword_window_get_module(window);

    if (module == NULL)
        return g_strdup("");
    if (gsw_key_get_kind(gsw_module_get_key(module)) ==
        GSW_KEY_KIND_VERSE) {
        GswVerseKey *key = (GswVerseKey *)gsw_module_get_key(module);

        return g_strdup_printf("%s %d", gsw_verse_key_get_book_name(key),
                               gsw_verse_key_get_chapter(key));
    }
    return g_strdup(gsw_module_get_key_text(module));
}

void
gsword_teaching_page_load(GswordTeachingPage *page)
{
    GswordPlanStore *plans;
    GswModule *module;
    gchar *reference;
    gchar *display;
    gchar *text;
    gchar *notes;
    GError *error = NULL;

    g_return_if_fail(GSWORD_IS_TEACHING_PAGE(page));
    if (page->view == NULL)
        return;
    module = gsword_window_ensure_verse_module(TEACHING_WINDOW(page));
    if (module == NULL)
        return;
    reference = current_chapter_reference(TEACHING_WINDOW(page));
    display = current_chapter_display(TEACHING_WINDOW(page));
    if (reference == NULL) {
        g_free(display);
        return;
    }

    text = gsword_window_chapter_text_for_module(module);
    if (text == NULL)
        text = gsw_module_get_text(module, &error);
    if (text == NULL) {
        gsword_window_show_error(TEACHING_WINDOW(page),
                                 _("Cannot read teaching passage"), error);
        g_free(reference);
        g_free(display);
        return;
    }
    gsword_window_set_view_text(page->view, text);
    g_free(text);
    gtk_editable_set_text(GTK_EDITABLE(page->ref_entry),
                          display != NULL ? display : reference);
    g_clear_pointer(&page->module_name, g_free);
    g_clear_pointer(&page->reference, g_free);
    page->module_name = g_strdup(gsw_module_get_name(module));
    page->reference = g_strdup(reference);
    plans = gsword_window_get_plan_store(TEACHING_WINDOW(page));
    notes = plans != NULL
          ? gsword_plan_store_get_teaching_note(
                plans, page->module_name, reference)
          : NULL;
    gtk_text_buffer_set_text(
        gtk_text_view_get_buffer(page->notes),
        notes != NULL ? notes : "", -1);
    g_free(notes);
    g_free(reference);
    g_free(display);
    gsword_window_show_page(TEACHING_WINDOW(page), "teaching");
}

static void
on_load(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_teaching_page_load(user_data);
}

static void
on_save(GtkButton *button, gpointer user_data)
{
    GswordTeachingPage *page = user_data;
    GswordPlanStore *plans = gsword_window_get_plan_store(TEACHING_WINDOW(page));
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;
    gchar *notes;
    GError *error = NULL;

    (void)button;
    if (plans == NULL || page->module_name == NULL || page->reference == NULL)
        return;
    buffer = gtk_text_view_get_buffer(page->notes);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    notes = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (!gsword_plan_store_save_teaching_note(
            plans, page->module_name, page->reference, notes, &error)) {
        gsword_window_show_error(TEACHING_WINDOW(page),
                                 _("Cannot save teaching notes"), error);
        g_free(notes);
        return;
    }
    g_free(notes);
    gsword_window_set_status(TEACHING_WINDOW(page), _("Teaching notes saved"));
}

static void
on_fullscreen(GtkButton *button, gpointer user_data)
{
    GswordTeachingPage *page = user_data;

    (void)button;
    page->fullscreen = !page->fullscreen;
    if (page->fullscreen)
        gtk_window_fullscreen(GTK_WINDOW(TEACHING_WINDOW(page)));
    else
        gtk_window_unfullscreen(GTK_WINDOW(TEACHING_WINDOW(page)));
    gtk_button_set_label(page->fullscreen_button,
                         page->fullscreen
                         ? _("Exit fullscreen") : _("Fullscreen"));
}

static void
on_close(GtkButton *button, gpointer user_data)
{
    GswordTeachingPage *page = user_data;

    (void)button;
    if (page->fullscreen) {
        page->fullscreen = FALSE;
        gtk_window_unfullscreen(GTK_WINDOW(TEACHING_WINDOW(page)));
        gtk_button_set_label(page->fullscreen_button, _("Fullscreen"));
    }
    gsword_window_show_page(TEACHING_WINDOW(page), "reader");
}

static void
gsword_teaching_page_finalize(GObject *object)
{
    GswordTeachingPage *page = GSWORD_TEACHING_PAGE(object);

    g_clear_pointer(&page->module_name, g_free);
    g_clear_pointer(&page->reference, g_free);

    G_OBJECT_CLASS(gsword_teaching_page_parent_class)->finalize(object);
}

static void
gsword_teaching_page_class_init(GswordTeachingPageClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = gsword_teaching_page_finalize;
}

static void
gsword_teaching_page_init(GswordTeachingPage *page)
{
    (void)page;
}

GswordTeachingPage *
gsword_teaching_page_new(GswordWindow *window)
{
    GswordTeachingPage *page = g_object_new(gsword_teaching_page_get_type(), NULL);
    GtkWidget *box;
    GtkWidget *toolbar;
    GtkWidget *paned;
    GtkWidget *scripture_box;
    GtkWidget *notes_box;
    GtkWidget *scripture_scroll;
    GtkWidget *notes_scroll;
    GtkWidget *scripture_label;
    GtkWidget *notes_label;
    GtkWidget *load;
    GtkWidget *save;
    GtkWidget *back;
    GtkWidget *fullscreen;

    gsword_page_set_window(GSWORD_PAGE(page), window);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    scripture_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    notes_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    scripture_scroll = gtk_scrolled_window_new();
    notes_scroll = gtk_scrolled_window_new();
    scripture_label = gtk_label_new(_("Scripture"));
    notes_label = gtk_label_new(_("Teaching notes"));
    load = gtk_button_new_with_label(_("Load current passage"));
    save = gtk_button_new_with_label(_("Save notes"));
    back = gtk_button_new_with_label(_("Back to reading"));
    fullscreen = gtk_button_new_with_label(_("Fullscreen"));

    gtk_widget_set_margin_top(toolbar, 8);
    gtk_widget_set_margin_bottom(toolbar, 8);
    gtk_widget_set_margin_start(toolbar, 12);
    gtk_widget_set_margin_end(toolbar, 12);
    page->ref_entry = GTK_ENTRY(gtk_entry_new());
    gtk_editable_set_editable(GTK_EDITABLE(page->ref_entry), FALSE);
    gtk_widget_set_hexpand(GTK_WIDGET(page->ref_entry), TRUE);
    gtk_entry_set_placeholder_text(page->ref_entry, _("Current passage"));
    page->view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(page->view, FALSE);
    gtk_text_view_set_cursor_visible(page->view, FALSE);
    gtk_text_view_set_wrap_mode(page->view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(page->view, 24);
    gtk_text_view_set_right_margin(page->view, 24);
    gtk_text_view_set_top_margin(page->view, 18);
    gtk_text_view_set_bottom_margin(page->view, 18);
    gtk_widget_add_css_class(GTK_WIDGET(page->view), "teaching-reader");
    page->notes = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_wrap_mode(page->notes, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(page->notes, 12);
    gtk_text_view_set_right_margin(page->notes, 12);
    gtk_text_view_set_top_margin(page->notes, 12);
    gtk_text_view_set_bottom_margin(page->notes, 12);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scripture_scroll),
                                  GTK_WIDGET(page->view));
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(notes_scroll),
                                  GTK_WIDGET(page->notes));
    gtk_widget_set_vexpand(scripture_scroll, TRUE);
    gtk_widget_set_vexpand(notes_scroll, TRUE);
    gtk_widget_set_size_request(notes_scroll, 240, -1);
    gtk_label_set_xalign(GTK_LABEL(scripture_label), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(notes_label), 0.0f);
    gtk_widget_add_css_class(scripture_label, "heading");
    gtk_widget_add_css_class(notes_label, "heading");
    gtk_widget_set_margin_start(scripture_label, 12);
    gtk_widget_set_margin_start(notes_label, 12);
    gtk_box_append(GTK_BOX(scripture_box), scripture_label);
    gtk_box_append(GTK_BOX(scripture_box), scripture_scroll);
    gtk_box_append(GTK_BOX(notes_box), notes_label);
    gtk_box_append(GTK_BOX(notes_box), notes_scroll);
    g_signal_connect(load, "clicked", G_CALLBACK(on_load), page);
    g_signal_connect(save, "clicked", G_CALLBACK(on_save), page);
    g_signal_connect(back, "clicked", G_CALLBACK(on_close), page);
    g_signal_connect(fullscreen, "clicked", G_CALLBACK(on_fullscreen), page);
    page->fullscreen_button = GTK_BUTTON(fullscreen);
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(page->ref_entry));
    gtk_box_append(GTK_BOX(toolbar), load);
    gtk_box_append(GTK_BOX(toolbar), save);
    gtk_box_append(GTK_BOX(toolbar), fullscreen);
    gtk_box_append(GTK_BOX(toolbar), back);
    gtk_paned_set_start_child(GTK_PANED(paned), scripture_box);
    gtk_paned_set_end_child(GTK_PANED(paned), notes_box);
    gtk_paned_set_position(GTK_PANED(paned), 520);
    gtk_box_append(GTK_BOX(box), toolbar);
    gtk_box_append(GTK_BOX(box), paned);
    gsword_page_set_widget(GSWORD_PAGE(page), box);
    return page;
}

GtkWidget *
gsword_teaching_page_widget(GswordTeachingPage *page)
{
    g_return_val_if_fail(GSWORD_IS_TEACHING_PAGE(page), NULL);
    return gsword_page_get_widget(GSWORD_PAGE(page));
}
