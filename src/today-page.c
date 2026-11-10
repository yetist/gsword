/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * today-page.c: personal center daily overview for the gsword window
 */

#include "today-page.h"
#include "insight-store.h"
#include "notes-store.h"
#include "plan-store.h"
#include "reference-utils.h"
#include "prayer-store.h"
#include "window-private.h"

#include <glib/gi18n.h>

#include <string.h>

struct _GswordTodayPage {
    GswordPage parent_instance;

    GtkLabel *continue_value;
    GtkLabel *plan_value;
    GtkLabel *devotion_value;
    GtkLabel *prayer_value;
    GtkLabel *memory_value;
    GtkLabel *notes_value;
    gboolean recent_is_insight;
};

G_DEFINE_TYPE(GswordTodayPage, gsword_today_page, GSWORD_TYPE_PAGE)

#define TODAY_WINDOW(page) \
    gsword_page_get_window(GSWORD_PAGE(page))

static void
on_continue_clicked(GtkButton *button, gpointer user_data)
{
    GswordTodayPage *page = user_data;
    GSettings *settings = gsword_window_get_settings(TODAY_WINDOW(page));
    gchar *module = NULL;
    gchar *key = NULL;

    (void)button;
    if (settings != NULL) {
        module = g_settings_get_string(settings, "last-module");
        key = g_settings_get_string(settings, "last-key");
    }
    if (module == NULL || *module == '\0' || key == NULL || *key == '\0')
        gsword_window_set_status(TODAY_WINDOW(page), _("No reading position yet"));
    else if (!gsword_window_open_module_reference(TODAY_WINDOW(page), module, key))
        gsword_window_set_status(TODAY_WINDOW(page), _("Cannot open reading position"));
    g_free(module);
    g_free(key);
}

static void
on_plans_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_show_personal(TODAY_WINDOW(user_data),
                                GSWORD_PERSONAL_SECTION_PLANS);
}

static void
on_devotion_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_open_devotion(TODAY_WINDOW(user_data));
}

static void
on_prayer_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_show_personal(TODAY_WINDOW(user_data),
                                GSWORD_PERSONAL_SECTION_PRAYER);
}

static void
on_recent_clicked(GtkButton *button, gpointer user_data)
{
    GswordTodayPage *page = user_data;

    (void)button;
    if (page->recent_is_insight)
        gsword_window_open_insights(TODAY_WINDOW(page));
    else
        gsword_window_show_personal(TODAY_WINDOW(page),
                                    GSWORD_PERSONAL_SECTION_NOTES);
}

static void
on_memory_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_open_memory_review(TODAY_WINDOW(user_data));
}

static GtkWidget *
create_card(const gchar *title, const gchar *action, GCallback callback,
            gpointer user_data, GtkLabel **value_out)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *title_label = gtk_label_new(title);
    GtkWidget *value = gtk_label_new("");
    GtkWidget *button = gtk_button_new_with_label(action);
    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    gtk_widget_add_css_class(card, "card");
    gtk_widget_set_margin_top(card, 4);
    gtk_widget_set_margin_bottom(card, 4);
    gtk_widget_set_margin_start(card, 12);
    gtk_widget_set_margin_end(card, 12);

    gtk_label_set_xalign(GTK_LABEL(title_label), 0.0f);
    gtk_widget_add_css_class(title_label, "heading");
    gtk_label_set_xalign(GTK_LABEL(value), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(value), TRUE);
    gtk_label_set_selectable(GTK_LABEL(value), TRUE);
    gtk_widget_add_css_class(value, "dim-label");

    gtk_widget_set_halign(button, GTK_ALIGN_START);
    gtk_widget_add_css_class(button, "flat");
    g_signal_connect(button, "clicked", callback, user_data);
    gtk_box_append(GTK_BOX(footer), button);
    gtk_widget_set_halign(footer, GTK_ALIGN_END);

    gtk_box_append(GTK_BOX(card), title_label);
    gtk_box_append(GTK_BOX(card), value);
    gtk_box_append(GTK_BOX(card), footer);
    *value_out = GTK_LABEL(value);
    return card;
}

static void
gsword_today_page_class_init(GswordTodayPageClass *klass)
{
    (void)klass;
}

static void
gsword_today_page_init(GswordTodayPage *page)
{
    (void)page;
}

GswordTodayPage *
gsword_today_page_new(GswordWindow *window)
{
    GswordTodayPage *page = g_object_new(gsword_today_page_get_type(), NULL);
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *clamp = adw_clamp_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *title = gtk_label_new(_("Today"));
    GtkWidget *subtitle = gtk_label_new(
        _("Continue reading and catch up with your personal study."));

    g_return_val_if_fail(GSWORD_IS_WINDOW(window), NULL);
    gsword_page_set_window(GSWORD_PAGE(page), window);

    gtk_widget_set_margin_top(box, 18);
    gtk_widget_set_margin_bottom(box, 18);
    gtk_widget_set_margin_start(box, 18);
    gtk_widget_set_margin_end(box, 18);
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_widget_add_css_class(title, "title-1");
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(subtitle), TRUE);
    gtk_widget_add_css_class(subtitle, "dim-label");
    gtk_box_append(GTK_BOX(box), title);
    gtk_box_append(GTK_BOX(box), subtitle);

    gtk_box_append(GTK_BOX(box),
                   create_card(_("Continue reading"), _("Continue"),
                               G_CALLBACK(on_continue_clicked), page,
                               &page->continue_value));
    gtk_box_append(GTK_BOX(box),
                   create_card(_("Today's plan"), _("Open plans"),
                               G_CALLBACK(on_plans_clicked), page,
                               &page->plan_value));
    gtk_box_append(GTK_BOX(box),
                   create_card(_("Today's devotion"), _("Open devotion"),
                               G_CALLBACK(on_devotion_clicked), page,
                               &page->devotion_value));
    gtk_box_append(GTK_BOX(box),
                   create_card(_("Prayer"), _("Open prayer"),
                               G_CALLBACK(on_prayer_clicked), page,
                               &page->prayer_value));
    gtk_box_append(GTK_BOX(box),
                   create_card(_("Memory review"), _("Review"),
                               G_CALLBACK(on_memory_clicked), page,
                               &page->memory_value));
    gtk_box_append(GTK_BOX(box),
                   create_card(_("Recent notes & insights"), _("Open"),
                               G_CALLBACK(on_recent_clicked), page,
                               &page->notes_value));

    adw_clamp_set_maximum_size(ADW_CLAMP(clamp), 760);
    adw_clamp_set_child(ADW_CLAMP(clamp), box);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), clamp);
    gtk_widget_set_vexpand(scroll, TRUE);
    gsword_page_set_widget(GSWORD_PAGE(page), scroll);
    gsword_today_page_refresh(page);
    return page;
}

GtkWidget *
gsword_today_page_widget(GswordTodayPage *page)
{
    g_return_val_if_fail(GSWORD_IS_TODAY_PAGE(page), NULL);
    return gsword_page_get_widget(GSWORD_PAGE(page));
}

static gchar *
short_text(const gchar *text, glong maximum)
{
    gchar *result;
    gchar *p;

    if (text == NULL || *text == '\0')
        return NULL;
    if (g_utf8_strlen(text, -1) > maximum) {
        const gchar *end = g_utf8_offset_to_pointer(text, maximum);
        gchar *truncated = g_strndup(text, (gsize)(end - text));

        result = g_strconcat(truncated, "…", NULL);
        g_free(truncated);
    } else {
        result = g_strdup(text);
    }
    for (p = result; *p != '\0'; p++) {
        if (*p == '\n' || *p == '\r' || *p == '\t')
            *p = ' ';
    }
    return result;
}

static gchar *
load_devotion_preview(GswordWindow *window, const gchar *name)
{
    GswManager *manager = gsword_window_get_manager(window);
    GDateTime *now;
    gchar *text = NULL;
    const gchar *formats[] = {
        "%m.%d", "%m%d", "%m-%d", "%m/%d", "%B %-d", "%b %-d", "%B %d",
        "%b %d", NULL
    };
    guint i;

    if (manager == NULL || name == NULL || *name == '\0')
        return NULL;
    now = g_date_time_new_now_local();
    for (i = 0; formats[i] != NULL; i++) {
        gchar *key = g_date_time_format(now, formats[i]);
        GswModule *module = gsw_manager_create_module(manager, name);

        if (module != NULL &&
            gsw_module_set_key_text(module, key, NULL) &&
            gsw_module_has_entry(module)) {
            text = gsw_module_get_text(module, NULL);
            if (text != NULL && *text == '\0')
                g_clear_pointer(&text, g_free);
        }
        g_clear_object(&module);
        g_free(key);
        if (text != NULL)
            break;
    }
    g_date_time_unref(now);
    if (text != NULL) {
        gchar *preview = short_text(text, 120);

        g_free(text);
        return preview;
    }
    return NULL;
}

void
gsword_today_page_refresh(GswordTodayPage *page)
{
    GSettings *settings;
    GswordPlanStore *plans;
    GswordNotesStore *notes;
    GswordInsightStore *insights;
    GswordPrayerStore *prayers;
    gchar *module = NULL;
    gchar *key = NULL;
    gchar *plan_id = NULL;
    gchar *devotion_name = NULL;
    gchar *value;
    GswordPlan *plan = NULL;
    GPtrArray *note_items = NULL;
    GPtrArray *insight_items = NULL;
    GPtrArray *memory;
    gint active_prayers;
    gint due_memory = 0;
    guint i;

    g_return_if_fail(page != NULL);
    settings = gsword_window_get_settings(TODAY_WINDOW(page));
    plans = gsword_window_get_plan_store(TODAY_WINDOW(page));
    notes = gsword_window_get_notes_store(TODAY_WINDOW(page));
    insights = gsword_window_get_insight_store(TODAY_WINDOW(page));
    prayers = gsword_window_get_prayer_store(TODAY_WINDOW(page));

    if (settings != NULL) {
        module = g_settings_get_string(settings, "last-module");
        key = g_settings_get_string(settings, "last-key");
        plan_id = g_settings_get_string(settings, "active-plan-id");
        devotion_name = g_settings_get_string(settings, "devotion-module");
    }
    if (module != NULL && *module != '\0' && key != NULL && *key != '\0') {
        gchar *reference = gsword_reference_localize(key, TRUE);

        value = g_strdup_printf("%s · %s", module, reference);
        g_free(reference);
    } else {
        value = g_strdup(_("No reading position yet"));
    }
    gtk_label_set_text(page->continue_value, value);
    g_free(value);

    if (plans != NULL && plan_id != NULL && *plan_id != '\0')
        plan = gsword_plan_store_get_plan(
            plans, g_ascii_strtoll(plan_id, NULL, 10));
    if (plan == NULL) {
        gtk_label_set_text(page->plan_value, _("No active plan"));
    } else if (plan->next_day > 0 && plan->next_reference != NULL) {
        gchar *reference = gsword_reference_localize(plan->next_reference, FALSE);

        value = g_strdup_printf(_("%s · Day %d · %s"), plan->name,
                                plan->next_day, reference);
        gtk_label_set_text(page->plan_value, value);
        g_free(value);
        g_free(reference);
    } else {
        value = g_strdup_printf(_("%s · Complete"), plan->name);
        gtk_label_set_text(page->plan_value, value);
        g_free(value);
    }

    {
        gchar *preview = NULL;

        if (devotion_name != NULL && *devotion_name != '\0')
            preview = load_devotion_preview(TODAY_WINDOW(page), devotion_name);
        if (preview != NULL) {
            value = g_strdup_printf("%s · %s", devotion_name, preview);
            gtk_label_set_text(page->devotion_value, value);
            g_free(value);
            g_free(preview);
        } else if (devotion_name != NULL && *devotion_name != '\0') {
            gtk_label_set_text(page->devotion_value,
                               _("No devotional entry today"));
        } else {
            gtk_label_set_text(page->devotion_value,
                               _("No devotional module"));
        }
    }

    active_prayers = prayers != NULL
                   ? gsword_prayer_store_count(
                         prayers, GSWORD_PRAYER_STATUS_ACTIVE) : 0;
    if (active_prayers > 0)
        value = g_strdup_printf(
            g_dngettext(GETTEXT_PACKAGE, "%d active prayer",
                        "%d active prayers", active_prayers),
            active_prayers);
    else
        value = g_strdup(_("No active prayers"));
    gtk_label_set_text(page->prayer_value, value);
    g_free(value);

    if (plans != NULL) {
        GDateTime *now = g_date_time_new_now_local();
        gchar *today = g_date_time_format(now, "%Y-%m-%d");

        memory = gsword_plan_store_list_memory_verses(plans);
        for (i = 0; i < memory->len; i++) {
            GswordMemoryVerse *verse = g_ptr_array_index(memory, i);

            if (verse->due == NULL || *verse->due == '\0' ||
                g_strcmp0(verse->due, today) <= 0)
                due_memory++;
        }
        g_ptr_array_unref(memory);
        g_free(today);
        g_date_time_unref(now);
    }
    if (due_memory > 0)
        value = g_strdup_printf(
            g_dngettext(GETTEXT_PACKAGE, "%d verse to review",
                        "%d verses to review", due_memory), due_memory);
    else
        value = g_strdup(_("No verses due"));
    gtk_label_set_text(page->memory_value, value);
    g_free(value);

    if (notes != NULL)
        note_items = gsword_notes_store_list_notes(notes, NULL, NULL);
    if (insights != NULL)
        insight_items = gsword_insight_store_list(insights, NULL, NULL);

    {
        GswordNote *note = note_items != NULL && note_items->len > 0
                         ? g_ptr_array_index(note_items, 0) : NULL;
        GswordInsight *insight = insight_items != NULL &&
                                 insight_items->len > 0
                               ? g_ptr_array_index(insight_items, 0) : NULL;

        if (note != NULL &&
            (insight == NULL || note->updated >= insight->updated)) {
            gchar *preview = short_text(note->text, 100);

            page->recent_is_insight = FALSE;
            value = g_strdup_printf("%s · %s",
                                    note->display_key != NULL
                                        ? note->display_key : "",
                                    preview != NULL ? preview : "");
            gtk_label_set_text(page->notes_value, value);
            g_free(value);
            g_free(preview);
        } else if (insight != NULL) {
            const gchar *title = insight->title != NULL &&
                                 *insight->title != '\0'
                               ? insight->title : insight->body;
            gchar *preview = short_text(title, 100);
            gchar *body_preview = short_text(insight->body, 80);

            page->recent_is_insight = TRUE;
            value = g_strdup_printf("%s · %s",
                                    preview != NULL ? preview : "",
                                    body_preview != NULL ? body_preview : "");
            gtk_label_set_text(page->notes_value, value);
            g_free(value);
            g_free(body_preview);
            g_free(preview);
        } else {
            page->recent_is_insight = FALSE;
            gtk_label_set_text(page->notes_value,
                               _("No notes or insights yet"));
        }
    }
    g_clear_pointer(&note_items, g_ptr_array_unref);
    g_clear_pointer(&insight_items, g_ptr_array_unref);

    g_clear_pointer(&plan, gsword_plan_free);
    g_free(devotion_name);
    g_free(plan_id);
    g_free(key);
    g_free(module);
}
