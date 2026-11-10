/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * test-ui-smoke.c: basic GTK UI smoke test for the reference chooser
 */

#include "gsword-reference-chooser.h"

#include <gtk/gtk.h>

static guint changed_count;
static guint activate_count;

static void
on_changed(GswordReferenceChooser *chooser, gpointer user_data)
{
    (void)chooser;
    (void)user_data;
    changed_count++;
}

static void
on_activate(GswordReferenceChooser *chooser, gpointer user_data)
{
    (void)chooser;
    (void)user_data;
    activate_count++;
}

static GtkWidget *
find_entry(GtkWidget *widget)
{
    GtkWidget *child;

    if (GTK_IS_ENTRY(widget))
        return widget;
    for (child = gtk_widget_get_first_child(widget); child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *found = find_entry(child);

        if (found != NULL)
            return found;
    }
    return NULL;
}

static void
pump_main_context(void)
{
    while (g_main_context_iteration(NULL, FALSE))
        ;
}

static GswordReferenceChooser *
make_chooser(GtkWidget **window_out, gboolean compact)
{
    GtkWidget *window = gtk_window_new();
    GswordReferenceChooser *chooser = compact
        ? GSWORD_REFERENCE_CHOOSER(
              gsword_reference_chooser_new_compact("KJV", "en"))
        : GSWORD_REFERENCE_CHOOSER(
              gsword_reference_chooser_new("KJV", "en"));

    gtk_window_set_default_size(GTK_WINDOW(window), 760, 420);
    gtk_window_set_child(GTK_WINDOW(window), GTK_WIDGET(chooser));
    gtk_window_present(GTK_WINDOW(window));
    pump_main_context();
    *window_out = window;
    return chooser;
}

static void
test_full_chooser(void)
{
    GtkWidget *window;
    GswordReferenceChooser *chooser = make_chooser(&window, FALSE);
    GtkWidget *entry;
    gint book = -1;
    gint chapter = -1;
    gint verse = -1;

    changed_count = 0;
    activate_count = 0;
    g_signal_connect(chooser, "changed", G_CALLBACK(on_changed), NULL);
    g_signal_connect(chooser, "activate", G_CALLBACK(on_activate), NULL);

    g_assert_true(gsword_reference_chooser_get_reference(
        chooser, &book, &chapter, &verse));
    g_assert_cmpint(chapter, >=, 1);
    g_assert_cmpint(verse, >=, 1);

    gsword_reference_chooser_set_reference(chooser, 0, 1, 1);
    gsword_reference_chooser_set_reference_range(chooser, 0, 1, 1, 6);
    g_assert_cmpint(gsword_reference_chooser_get_verse_end(chooser), ==, 6);

    gsword_reference_chooser_set_versification(chooser, "KJVA");
    gsword_reference_chooser_set_locale(chooser, "zh-Hans");
    g_assert_true(gsword_reference_chooser_get_reference(
        chooser, &book, &chapter, &verse));
    g_assert_true(gsword_reference_chooser_get_verse_end(chooser) >= 0);
    gsword_reference_chooser_set_locale(chooser, "en");

    gsword_reference_chooser_begin_edit(chooser);
    pump_main_context();
    entry = find_entry(GTK_WIDGET(chooser));
    g_assert_nonnull(entry);
    gtk_editable_set_text(GTK_EDITABLE(entry), "John 3:16");
    g_signal_emit_by_name(entry, "activate");
    pump_main_context();

    g_assert_true(gsword_reference_chooser_get_reference(
        chooser, &book, &chapter, &verse));
    g_assert_cmpint(chapter, ==, 3);
    g_assert_cmpint(verse, ==, 16);
    g_assert_cmpuint(changed_count, >, 0);
    g_assert_cmpuint(activate_count, >, 0);

    gtk_window_destroy(GTK_WINDOW(window));
    pump_main_context();
}

static void
test_compact_chooser(void)
{
    GtkWidget *window;
    GswordReferenceChooser *chooser = make_chooser(&window, TRUE);
    gint book = -1;
    gint chapter = -1;
    gint verse = -1;

    g_assert_true(gsword_reference_chooser_get_reference(
        chooser, &book, &chapter, &verse));
    gsword_reference_chooser_set_reference(chooser, 18, 23, 1);
    g_assert_true(gsword_reference_chooser_get_reference(
        chooser, &book, &chapter, &verse));
    g_assert_cmpint(chapter, ==, 23);
    g_assert_cmpint(verse, ==, 1);
    {
        gchar *osis = gsword_reference_chooser_dup_osis(chooser);

        g_assert_nonnull(osis);
        g_free(osis);
    }

    gtk_window_destroy(GTK_WINDOW(window));
    pump_main_context();
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    gsw_locale_mgr_load_directory(gsw_locale_mgr_get_default(),
                                  GSW_TEST_LOCALE_DIR, NULL);
    gtk_init();

    g_test_add_func("/ui/reference-chooser/full", test_full_chooser);
    g_test_add_func("/ui/reference-chooser/compact", test_compact_chooser);
    return g_test_run();
}
