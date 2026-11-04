/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * reference-chooser-demo.c: standalone demo for GswordReferenceChooser
 *
 * Run with:
 *   GSW_LOCALE_PATH=$PWD/data/locales.d \
 *     ./build/tests/gsword-reference-chooser-demo zh-Hans KJV "创 1:1"
 */

#include "gsword-reference-chooser.h"

static gchar *demo_locale = NULL;
static gchar *demo_versification = NULL;
static gchar *demo_reference = NULL;

static void
show_reference(GtkLabel *label, GswordReferenceChooser *chooser,
               const gchar *prefix)
{
    gint book = 0;
    gint chapter = 1;
    gint verse = 1;
    gchar *osis = NULL;
    gchar *text;

    gsword_reference_chooser_get_reference(chooser, &book, &chapter, &verse);
    osis = gsword_reference_chooser_dup_osis(chooser);
    text = g_strdup_printf("%s  %s %d:%d  (book index %d)",
                           prefix, osis != NULL ? osis : "?",
                           chapter, verse, book);
    gtk_label_set_text(label, text);
    g_free(text);
    g_free(osis);
}

static void
on_changed(GswordReferenceChooser *chooser, gpointer user_data)
{
    show_reference(GTK_LABEL(user_data), chooser, "changed:");
}

static void
on_activate(GswordReferenceChooser *chooser, gpointer user_data)
{
    show_reference(GTK_LABEL(user_data), chooser, "confirmed:");
}

static void
on_preset_clicked(GtkButton *button, gpointer user_data)
{
    GswordReferenceChooser *chooser = GSWORD_REFERENCE_CHOOSER(user_data);
    const gchar *text = g_object_get_data(G_OBJECT(button), "reference");
    GswordReference reference;
    GError *error = NULL;

    if (text == NULL)
        return;
    if (!gsword_reference_parse(text, demo_locale, demo_versification,
                                &reference, &error)) {
        g_printerr("cannot parse preset %s: %s\n", text,
                   error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
        return;
    }
    gsword_reference_chooser_set_reference(
        chooser, reference.book, reference.chapter, reference.verse);
    gsword_reference_clear(&reference);
}

static GtkWidget *
make_preset_button(const gchar *label, const gchar *reference,
                   GswordReferenceChooser *chooser)
{
    GtkWidget *button = gtk_button_new_with_label(label);

    g_object_set_data_full(G_OBJECT(button), "reference",
                           g_strdup(reference), g_free);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(on_preset_clicked), chooser);
    return button;
}

static void
on_activate_app(GtkApplication *application, gpointer user_data)
{
    GtkWidget *window = gtk_application_window_new(application);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *title = gtk_label_new(NULL);
    GtkWidget *help = gtk_label_new(NULL);
    GtkWidget *presets = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *status = gtk_label_new("No selection yet.");
    GswordReferenceChooser *chooser;

    (void)user_data;
    gtk_window_set_title(GTK_WINDOW(window),
                         "GswordReferenceChooser demo");
    gtk_window_set_default_size(GTK_WINDOW(window), 760, 520);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);

    gtk_label_set_markup(GTK_LABEL(title),
                         "<span size='x-large' weight='bold'>"
                         "GswordReferenceChooser</span>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(help), TRUE);
    gtk_label_set_xalign(GTK_LABEL(help), 0.0f);
    gtk_label_set_text(GTK_LABEL(help),
        "输入示例：Gen 1 / Ge1 / 创 1 / 诗23 / 1 Cor 13:4 / 约 3:16。\n"
        "默认是路径栏：点击箭头切换上一卷/下一卷、上一章/下一章、"
        "上一节/下一节；点击书卷名、章节名或节号打开选择器。\n"
        "按 Ctrl+L 变成文本输入框；Enter 确认，Esc 取消。"
        "选择器分为旧约和新约两个书卷分组，并包含章和节网格。");
    gtk_widget_add_css_class(help, "dim-label");

    chooser = GSWORD_REFERENCE_CHOOSER(
        gsword_reference_chooser_new(demo_versification, demo_locale));
    gtk_widget_set_halign(GTK_WIDGET(chooser), GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(GTK_WIDGET(chooser), TRUE);

    gtk_box_append(GTK_BOX(presets),
                   make_preset_button("Gen 1:1", "Gen 1:1", chooser));
    gtk_box_append(GTK_BOX(presets),
                   make_preset_button("Ps 23", "Ps 23", chooser));
    gtk_box_append(GTK_BOX(presets),
                   make_preset_button("John 3:16", "John 3:16", chooser));
    gtk_box_append(GTK_BOX(presets),
                   make_preset_button("林前 13:4", "林前 13:4", chooser));

    gtk_label_set_xalign(GTK_LABEL(status), 0.0f);
    gtk_widget_add_css_class(status, "dim-label");

    g_signal_connect(chooser, "changed", G_CALLBACK(on_changed), status);
    g_signal_connect(chooser, "activate", G_CALLBACK(on_activate), status);

    if (demo_reference != NULL) {
        GswordReference reference;
        GError *error = NULL;

        if (gsword_reference_parse(demo_reference, demo_locale,
                                   demo_versification, &reference, &error)) {
            gsword_reference_chooser_set_reference(
                chooser, reference.book, reference.chapter, reference.verse);
            gsword_reference_clear(&reference);
        } else {
            g_printerr("cannot parse initial reference: %s\n",
                       error != NULL ? error->message : "unknown error");
            g_clear_error(&error);
        }
    }

    gtk_box_append(GTK_BOX(box), title);
    gtk_box_append(GTK_BOX(box), help);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(chooser));
    gtk_box_append(GTK_BOX(box), presets);
    gtk_box_append(GTK_BOX(box), status);
    gtk_window_set_child(GTK_WINDOW(window), box);
    gtk_window_present(GTK_WINDOW(window));
}

int
main(int argc, char **argv)
{
    GtkApplication *application;
    int status;

    demo_locale = g_strdup(argc > 1 ? argv[1] : "en");
    demo_versification = g_strdup(argc > 2 ? argv[2] : "KJV");
    demo_reference = g_strdup(argc > 3 ? argv[3] : "Gen 1:1");

    application = gtk_application_new(
        "org.gsword.ReferenceChooserDemo", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(application, "activate",
                     G_CALLBACK(on_activate_app), NULL);
    status = g_application_run(G_APPLICATION(application), 1, argv);
    g_object_unref(application);
    g_free(demo_locale);
    g_free(demo_versification);
    g_free(demo_reference);
    return status;
}
