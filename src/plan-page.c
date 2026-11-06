/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * plan-page.c: reading plan page for the gsword window
 */

#include "app-locale.h"
#include "plan-page.h"
#include "devotion-page.h"
#include "memory-page.h"
#include "window-private.h"

#include <glib/gi18n.h>

static void gsword_plan_page_day_toggled(GtkCheckButton *button,
                                         gpointer user_data);

struct _GswordPlanPage {
    GswordWindow *window;
    GtkWidget *root;
    GtkNotebook *notebook;
    GswordDevotionPage *devotion;
    GswordMemoryPage *memory;
    GtkDropDown *selector;
    GtkStringList *names;
    GPtrArray *items;
    GtkLabel *summary;
    GtkLabel *progress_text;
    GtkProgressBar *progress;
    GtkStack *today_stack;
    GtkLabel *today;
    GtkButton *open_button;
    GtkButton *complete_button;
    GtkButton *jump_today_button;
    GtkButton *undo_button;
    GtkDropDown *day_filter;
    GtkListBox *days;
    GtkListBox *plans_list;
    GtkButton *rename_plan_button;
    GtkButton *duplicate_plan_button;
    GtkButton *delete_plan_button;
    AdwDialog *create_dialog;
    GtkLabel *preview;
    GtkEntry *name_entry;
    GtkEntry *start_entry;
    GtkSpinButton *days_spin;
    GtkSpinButton *chapters_spin;
    gint64 selected_id;
    gint64 undo_plan_id;
    gint undo_day;
    gboolean undo_available;
    guint refresh_source;
    guint preview_source;
};

static GPtrArray *
generate_plan_references(const gchar *start_ref, gint days,
                         gint chapters_per_day)
{
    GPtrArray *references = g_ptr_array_new_with_free_func(g_free);
    GswVerseKey *key;
    GswVersificationMgr *mgr;
    GswVersification *versification;
    gint global_book;
    gint chapter;
    gint book_count;
    gint total_chapters = 0;
    gint per_day;
    gint extra;
    gint day;

    key = gsw_verse_key_new(start_ref, NULL);
    if (key == NULL)
        return references;
    global_book = gsw_verse_key_get_global_book_number(key);
    chapter = gsw_verse_key_get_chapter(key);
    if (global_book < 0 || chapter < 1) {
        gsw_key_unref(key);
        return references;
    }

    mgr = gsw_versification_mgr_get_default();
    versification = gsw_versification_mgr_get_system(
        mgr, gsw_verse_key_get_versification(key));
    if (versification == NULL) {
        gsw_key_unref(key);
        return references;
    }
    book_count = gsw_versification_get_book_count(versification);

    {
        gint book_index;
        gint first_chapter = chapter;

        for (book_index = global_book; book_index < book_count; book_index++) {
            GswBook *book = gsw_versification_get_book(versification,
                                                       book_index);
            gint maximum;
            gint first;

            if (book == NULL)
                break;
            maximum = gsw_book_get_chapter_max(book);
            first = book_index == global_book ? first_chapter : 1;
            if (maximum >= first)
                total_chapters += maximum - first + 1;
        }
    }

    if (total_chapters <= 0) {
        gsw_key_unref(key);
        return references;
    }

    if (total_chapters <= days * chapters_per_day) {
        per_day = total_chapters / days;
        extra = total_chapters % days;
        if (per_day == 0) {
            per_day = 1;
            extra = 0;
            days = MIN(days, total_chapters);
        }
    } else {
        per_day = chapters_per_day;
        extra = 0;
    }

    for (day = 0; day < days && global_book < book_count; day++) {
        GswBook *book = gsw_versification_get_book(versification,
                                                   global_book);
        gint count = per_day + (day < extra ? 1 : 0);
        gint c;

        if (book == NULL)
            break;
        /* Plans store canonical references; rendering uses the UI locale. */
        g_ptr_array_add(references,
            g_strdup_printf("%s %d", gsw_book_get_name(book), chapter));

        for (c = 0; c < count; c++) {
            gint maximum = gsw_book_get_chapter_max(book);

            if (chapter < maximum) {
                chapter++;
            } else if (global_book + 1 < book_count) {
                global_book++;
                chapter = 1;
                book = gsw_versification_get_book(versification, global_book);
            } else {
                global_book = book_count;
                break;
            }
        }
    }
    gsw_key_unref(key);
    return references;
}

static GswordPlan *
selected_plan(GswordPlanPage *page)
{
    guint i;

    if (page->items == NULL)
        return NULL;
    for (i = 0; i < page->items->len; i++) {
        GswordPlan *plan = g_ptr_array_index(page->items, i);

        if (plan->id == page->selected_id)
            return plan;
    }
    return page->items->len > 0
         ? g_ptr_array_index(page->items, 0) : NULL;
}

static gchar *
localize_plan_reference(const gchar *reference)
{
    GswVerseKey *key;
    const gchar *book;
    gint chapter;
    gint verse;
    gchar *locale;
    gchar *result;

    if (reference == NULL || *reference == '\0')
        return g_strdup("");
    key = gsw_verse_key_new(reference, NULL);
    locale = gsword_app_locale_name();
    if (key != NULL)
        gsw_key_set_locale((GswKey *)key, locale);
    g_free(locale);
    if (key == NULL || gsw_verse_key_get_book(key) <= 0 ||
        gsw_verse_key_get_chapter(key) <= 0) {
        g_clear_pointer(&key, gsw_key_unref);
        return g_strdup(reference);
    }
    book = gsw_verse_key_get_book_name(key);
    chapter = gsw_verse_key_get_chapter(key);
    verse = gsw_verse_key_get_verse(key);
    if (verse > 1)
        result = g_strdup_printf("%s %d:%d", book, chapter, verse);
    else
        result = g_strdup_printf("%s %d", book, chapter);
    gsw_key_unref((GswKey *)key);
    return result;
}

static void
update_undo_button(GswordPlanPage *page, GswordPlan *plan)
{
    gboolean visible = page->undo_available && plan != NULL &&
                       page->undo_plan_id == plan->id;

    if (page->undo_button != NULL)
        gtk_widget_set_visible(GTK_WIDGET(page->undo_button), visible);
}

static GtkWidget *
make_plan_placeholder(const gchar *text)
{
    GtkWidget *label = gtk_label_new(text);

    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_margin_top(label, 12);
    gtk_widget_set_margin_bottom(label, 12);
    gtk_widget_set_margin_start(label, 8);
    gtk_widget_set_margin_end(label, 8);
    gtk_widget_add_css_class(label, "dim-label");
    gtk_widget_add_css_class(label, "caption");
    return label;
}

static void
refresh_plan_list(GswordPlanPage *page)
{
    GtkWidget *child;

    if (page->plans_list == NULL)
        return;
    while ((child = gtk_widget_get_first_child(
                GTK_WIDGET(page->plans_list))) != NULL)
        gtk_list_box_remove(page->plans_list, child);
    if (page->items == NULL || page->items->len == 0) {
        gtk_list_box_set_placeholder(page->plans_list,
            make_plan_placeholder(_("No reading plans yet")));
    }
    for (guint i = 0; page->items != NULL && i < page->items->len; i++) {
        GswordPlan *plan = g_ptr_array_index(page->items, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *name = gtk_label_new(plan->name);
        gdouble percent = plan->total_days > 0
                        ? (gdouble)plan->completed_days * 100.0 /
                          plan->total_days : 0.0;
        gchar *detail_text = g_strdup_printf(
            _("%d/%d days (%.0f%%) · streak %d"), plan->completed_days,
            plan->total_days, percent, plan->streak);
        GtkWidget *detail = gtk_label_new(detail_text);

        gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
        gtk_widget_add_css_class(name, "heading");
        if (plan->id == page->selected_id)
            gtk_widget_add_css_class(name, "accent");
        gtk_label_set_xalign(GTK_LABEL(detail), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(detail), PANGO_ELLIPSIZE_END);
        gtk_widget_add_css_class(detail, "caption");
        gtk_widget_add_css_class(detail, "dim-label");
        gtk_box_append(GTK_BOX(box), name);
        gtk_box_append(GTK_BOX(box), detail);
        gtk_widget_set_margin_top(box, 4);
        gtk_widget_set_margin_bottom(box, 4);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data_full(G_OBJECT(row), "plan-id",
                               g_strdup_printf("%" G_GINT64_FORMAT, plan->id),
                               g_free);
        gtk_list_box_append(page->plans_list, row);
        g_free(detail_text);
    }
    if (page->rename_plan_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->rename_plan_button),
                                 page->selected_id != 0);
    if (page->duplicate_plan_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->duplicate_plan_button),
                                 page->selected_id != 0);
    if (page->delete_plan_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(page->delete_plan_button),
                                 page->selected_id != 0);
}

static void
refresh_views(GswordPlanPage *page)
{
    GswordPlanStore *plans = gsword_window_get_plan_store(page->window);
    GswordPlan *plan = selected_plan(page);
    GPtrArray *days;
    guint i;

    if (page->days == NULL)
        return;
    while (gtk_widget_get_first_child(GTK_WIDGET(page->days)) != NULL)
        gtk_list_box_remove(page->days,
                            gtk_widget_get_first_child(GTK_WIDGET(page->days)));
    if (plan == NULL) {
        if (page->today_stack != NULL)
            gtk_stack_set_visible_child_name(page->today_stack, "empty");
        gtk_label_set_text(page->summary, "");
        gtk_label_set_text(page->progress_text, "");
        gtk_label_set_text(page->today, "");
        gtk_progress_bar_set_fraction(page->progress, 0.0);
        gtk_widget_set_sensitive(GTK_WIDGET(page->open_button), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(page->complete_button), FALSE);
        update_undo_button(page, NULL);
        refresh_plan_list(page);
        return;
    }
    if (page->today_stack != NULL)
        gtk_stack_set_visible_child_name(page->today_stack, "dashboard");
    page->selected_id = plan->id;
    update_undo_button(page, plan);
    {
        gdouble percent = plan->total_days > 0
                        ? (gdouble)plan->completed_days * 100.0 /
                          plan->total_days : 0.0;
        gchar *summary = g_strdup_printf(
            _("%d/%d days (%.0f%%) · streak %d"),
            plan->completed_days, plan->total_days, percent, plan->streak);

        gtk_label_set_text(page->summary, plan->name);
        gtk_label_set_text(page->progress_text, summary);
        gtk_progress_bar_set_fraction(page->progress,
                                      CLAMP(percent / 100.0, 0.0, 1.0));
        g_free(summary);
    }
    if (plan->next_day > 0) {
        gchar *reference = localize_plan_reference(plan->next_reference);
        gchar *today = g_strdup_printf(_("Day %d: %s"), plan->next_day,
                                       reference);

        gtk_label_set_text(page->today, today);
        g_free(reference);
        g_free(today);
        gtk_widget_set_sensitive(GTK_WIDGET(page->open_button), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(page->complete_button), TRUE);
    } else {
        gtk_label_set_text(page->today, _("Plan complete"));
        gtk_widget_set_sensitive(GTK_WIDGET(page->open_button), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(page->complete_button), FALSE);
    }
    days = plans != NULL
         ? gsword_plan_store_get_plan_days(plans, plan->id) : NULL;
    {
        guint filter = page->day_filter != NULL
                     ? gtk_drop_down_get_selected(page->day_filter) : 0;

        for (i = 0; days != NULL && i < days->len; i++) {
            GswordPlanDay *day = g_ptr_array_index(days, i);
            GtkWidget *row;
            GtkWidget *box;
            GtkWidget *check;
            GtkWidget *label;
            gchar *day_number;
            gchar *text;
            gboolean is_today;

            if (filter == 1 && day->completed_on != NULL)
                continue;
            if (filter == 2 && day->completed_on == NULL)
                continue;
            row = gtk_list_box_row_new();
            box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            check = gtk_check_button_new();
            gchar *localized = localize_plan_reference(day->reference);

            is_today = day->day == plan->next_day;
            day_number = g_strdup_printf("%d", day->day);
            text = g_strdup_printf(_("Day %d   %s%s"), day->day,
                                   localized,
                                   is_today ? _("   · today") : "");
            g_free(localized);

            gtk_check_button_set_active(GTK_CHECK_BUTTON(check),
                                        day->completed_on != NULL);
        g_object_set_data_full(G_OBJECT(check), "plan-id",
                               g_strdup_printf("%" G_GINT64_FORMAT,
                                               plan->id), g_free);
        g_object_set_data_full(G_OBJECT(check), "plan-day", day_number, g_free);
        g_signal_connect(check, "toggled",
                         G_CALLBACK(gsword_plan_page_day_toggled), page);
        label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_set_hexpand(label, TRUE);
        gtk_check_button_set_label(GTK_CHECK_BUTTON(check), _("Done"));
        gtk_box_append(GTK_BOX(box), label);
        gtk_box_append(GTK_BOX(box), check);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data_full(G_OBJECT(row), "plan-reference",
                               g_strdup(day->reference), g_free);
            gtk_list_box_append(page->days, row);
            if (is_today)
                gtk_widget_add_css_class(label, "accent");
            g_free(text);
        }
    }
    if (days != NULL)
        g_ptr_array_unref(days);
    refresh_plan_list(page);
}

void
gsword_plan_page_refresh(GswordPlanPage *page)
{
    GswordPlanStore *plans;
    GSettings *settings;
    gchar *active = NULL;
    guint selected = 0;
    guint i;

    g_return_if_fail(page != NULL);
    plans = gsword_window_get_plan_store(page->window);
    settings = gsword_window_get_settings(page->window);
    if (plans == NULL || page->selector == NULL)
        return;
    g_clear_pointer(&page->items, g_ptr_array_unref);
    page->items = gsword_plan_store_list_plans(plans);
    if (settings != NULL)
        active = g_settings_get_string(settings, "active-plan-id");
    g_clear_object(&page->names);
    page->names = gtk_string_list_new(NULL);
    for (i = 0; i < page->items->len; i++) {
        GswordPlan *plan = g_ptr_array_index(page->items, i);

        gtk_string_list_append(page->names, plan->name);
        if (active != NULL &&
            plan->id == g_ascii_strtoll(active, NULL, 10))
            selected = i;
    }
    gtk_drop_down_set_model(page->selector, G_LIST_MODEL(page->names));
    if (page->items->len > 0) {
        if (selected >= page->items->len)
            selected = 0;
        page->selected_id = ((GswordPlan *)g_ptr_array_index(page->items,
                                                             selected))->id;
        gtk_drop_down_set_selected(page->selector, selected);
    } else {
        page->selected_id = 0;
    }
    refresh_views(page);
    g_free(active);
}

static gboolean
refresh_idle(gpointer data)
{
    GswordPlanPage *page = data;

    page->refresh_source = 0;
    gsword_plan_page_refresh(page);
    return G_SOURCE_REMOVE;
}

static void
schedule_refresh(GswordPlanPage *page)
{
    if (page->refresh_source != 0)
        return;
    page->refresh_source = g_idle_add_full(
        G_PRIORITY_DEFAULT_IDLE, refresh_idle, page, NULL);
}

static void
on_plan_selected(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordPlanPage *page = user_data;
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    GswordPlan *plan;
    GSettings *settings;

    (void)pspec;
    if (page->items == NULL || selected == GTK_INVALID_LIST_POSITION ||
        selected >= page->items->len)
        return;
    plan = g_ptr_array_index(page->items, selected);
    page->selected_id = plan->id;
    settings = gsword_window_get_settings(page->window);
    if (settings != NULL) {
        gchar *id = g_strdup_printf("%" G_GINT64_FORMAT, plan->id);

        g_settings_set_string(settings, "active-plan-id", id);
        g_free(id);
    }
    refresh_views(page);
}

static void
on_plan_list_activated(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordPlanPage *page = user_data;
    GSettings *settings = gsword_window_get_settings(page->window);
    const gchar *id;

    (void)list;
    if (row == NULL)
        return;
    id = g_object_get_data(G_OBJECT(row), "plan-id");
    if (id == NULL)
        return;
    page->selected_id = g_ascii_strtoll(id, NULL, 10);
    if (settings != NULL)
        g_settings_set_string(settings, "active-plan-id", id);
    gsword_plan_page_refresh(page);
}

static void
on_rename_plan_response(AdwAlertDialog *dialog, const gchar *response,
                        gpointer user_data)
{
    GswordPlanPage *page = user_data;
    GswordPlanStore *plans = gsword_window_get_plan_store(page->window);
    GtkEntry *entry = g_object_get_data(G_OBJECT(dialog), "rename-entry");
    GswordPlan *plan = selected_plan(page);
    GError *error = NULL;
    gchar *name;

    if (g_strcmp0(response, "rename") != 0 || plans == NULL ||
        plan == NULL || entry == NULL)
        return;
    name = g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry)));
    g_strstrip(name);
    if (*name == '\0' || g_strcmp0(name, plan->name) == 0) {
        g_free(name);
        return;
    }
    if (!gsword_plan_store_rename_plan(plans, plan->id, name, &error)) {
        gsword_window_show_error(page->window, _("Cannot rename plan"), error);
        g_free(name);
        return;
    }
    g_free(name);
    gsword_plan_page_refresh(page);
    gsword_window_set_status(page->window, _("Reading plan renamed"));
}

static void
on_rename_plan(GtkButton *button, gpointer user_data)
{
    GswordPlanPage *page = user_data;
    GswordPlan *plan = selected_plan(page);
    GtkWidget *entry;
    AdwDialog *dialog;

    (void)button;
    if (plan == NULL)
        return;
    dialog = adw_alert_dialog_new(_("Rename reading plan"), NULL);
    entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(entry), plan->name);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    gtk_widget_set_hexpand(entry, TRUE);
    adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(dialog), entry);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "cancel", _("Cancel"),
                                   "rename", _("Rename"), NULL);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "rename");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    g_object_set_data(G_OBJECT(dialog), "rename-entry", entry);
    g_signal_connect(dialog, "response",
                     G_CALLBACK(on_rename_plan_response), page);
    adw_dialog_present(dialog, GTK_WIDGET(page->window));
}

static void
on_duplicate_plan(GtkButton *button, gpointer user_data)
{
    GswordPlanPage *page = user_data;
    GswordPlanStore *plans = gsword_window_get_plan_store(page->window);
    GSettings *settings = gsword_window_get_settings(page->window);
    GswordPlan *plan = selected_plan(page);
    GError *error = NULL;
    gint64 new_id;

    (void)button;
    if (plans == NULL || plan == NULL)
        return;
    new_id = gsword_plan_store_duplicate_plan(plans, plan->id, &error);
    if (new_id == 0) {
        gsword_window_show_error(page->window, _("Cannot duplicate plan"),
                                 error);
        return;
    }
    page->selected_id = new_id;
    if (settings != NULL) {
        gchar *id = g_strdup_printf("%" G_GINT64_FORMAT, new_id);

        g_settings_set_string(settings, "active-plan-id", id);
        g_free(id);
    }
    gsword_plan_page_refresh(page);
    gsword_window_set_status(page->window, _("Reading plan duplicated"));
}

static gboolean
update_plan_preview(gpointer data)
{
    GswordPlanPage *page = data;
    const gchar *start;
    gint days;
    gint chapters;
    GPtrArray *references;
    gchar *text;

    page->preview_source = 0;
    if (page->preview == NULL || page->start_entry == NULL ||
        page->days_spin == NULL || page->chapters_spin == NULL)
        return G_SOURCE_REMOVE;
    start = gtk_editable_get_text(GTK_EDITABLE(page->start_entry));
    days = gtk_spin_button_get_value_as_int(page->days_spin);
    chapters = gtk_spin_button_get_value_as_int(page->chapters_spin);
    if (start == NULL || *start == '\0') {
        gtk_label_set_text(page->preview, "");
        return G_SOURCE_REMOVE;
    }
    references = generate_plan_references(start, days, chapters);
    if (references->len == 0) {
        text = g_strdup(_("Preview unavailable for this reference."));
    } else {
        gchar *first = localize_plan_reference(
            g_ptr_array_index(references, 0));

        if (references->len == 1) {
            text = g_strdup_printf(_("Day 1: %s"), first);
        } else {
            gchar *last = localize_plan_reference(
                g_ptr_array_index(references, references->len - 1));

            text = g_strdup_printf(_("Day 1: %s\nDay %u: %s"),
                                   first, references->len, last);
            g_free(last);
        }
        g_free(first);
    }
    gtk_label_set_text(page->preview, text);
    g_free(text);
    g_ptr_array_unref(references);
    return G_SOURCE_REMOVE;
}

static void
schedule_preview(GswordPlanPage *page)
{
    if (page->preview == NULL)
        return;
    if (page->preview_source == 0)
        page->preview_source = g_idle_add(update_plan_preview, page);
}

static void
update_create_response(GswordPlanPage *page)
{
    const gchar *name;
    const gchar *start;
    gboolean enabled;

    if (page->create_dialog == NULL || page->name_entry == NULL ||
        page->start_entry == NULL)
        return;
    name = gtk_editable_get_text(GTK_EDITABLE(page->name_entry));
    start = gtk_editable_get_text(GTK_EDITABLE(page->start_entry));
    enabled = name != NULL && *name != '\0' &&
              start != NULL && *start != '\0';
    adw_alert_dialog_set_response_enabled(
        ADW_ALERT_DIALOG(page->create_dialog), "create", enabled);
}

static void
on_preview_changed(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    schedule_preview(user_data);
    update_create_response(user_data);
}

static void
gsword_plan_page_day_toggled(GtkCheckButton *button, gpointer user_data)
{
    GswordPlanPage *page = user_data;
    GswordPlanStore *plans = gsword_window_get_plan_store(page->window);
    const gchar *plan_id = g_object_get_data(G_OBJECT(button), "plan-id");
    const gchar *day = g_object_get_data(G_OBJECT(button), "plan-day");
    GError *error = NULL;

    if (plans == NULL || plan_id == NULL || day == NULL)
        return;
    if (!gsword_plan_store_complete_day(
            plans, g_ascii_strtoll(plan_id, NULL, 10),
            (gint)g_ascii_strtoll(day, NULL, 10),
            gtk_check_button_get_active(button), &error)) {
        gsword_window_show_error(page->window, _("Cannot update plan"), error);
        return;
    }
    gsword_window_set_status(page->window, _("Reading plan updated"));
    schedule_refresh(page);
}

static void
on_day_activated(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordPlanPage *page = user_data;
    const gchar *reference;

    (void)list;
    if (row == NULL)
        return;
    reference = g_object_get_data(G_OBJECT(row), "plan-reference");
    if (reference != NULL)
        gsword_window_open_reference(page->window, reference);
}

static void
on_open_today(GtkButton *button, gpointer user_data)
{
    GswordPlanPage *page = user_data;
    GswordPlan *plan = selected_plan(page);

    (void)button;
    if (plan != NULL && plan->next_reference != NULL)
        gsword_window_open_reference(page->window, plan->next_reference);
}

static void
on_complete_today(GtkButton *button, gpointer user_data)
{
    GswordPlanPage *page = user_data;
    GswordPlanStore *plans = gsword_window_get_plan_store(page->window);
    GswordPlan *plan = selected_plan(page);
    GError *error = NULL;

    (void)button;
    if (plans == NULL || plan == NULL || plan->next_day <= 0)
        return;
    if (!gsword_plan_store_complete_day(plans, plan->id, plan->next_day,
                                        TRUE, &error)) {
        gsword_window_show_error(page->window, _("Cannot update plan"), error);
        return;
    }
    page->undo_plan_id = plan->id;
    page->undo_day = plan->next_day;
    page->undo_available = TRUE;
    gsword_window_set_status(page->window, _("Day marked complete"));
    gsword_plan_page_refresh(page);
}

static void
on_undo_complete(GtkButton *button, gpointer user_data)
{
    GswordPlanPage *page = user_data;
    GswordPlanStore *plans = gsword_window_get_plan_store(page->window);
    GswordPlan *plan = selected_plan(page);
    GError *error = NULL;

    (void)button;
    if (plans == NULL || plan == NULL || !page->undo_available ||
        page->undo_plan_id != plan->id)
        return;
    if (!gsword_plan_store_complete_day(plans, plan->id, page->undo_day,
                                        FALSE, &error)) {
        gsword_window_show_error(page->window, _("Cannot update plan"), error);
        return;
    }
    page->undo_available = FALSE;
    gsword_window_set_status(page->window, _("Completion undone"));
    gsword_plan_page_refresh(page);
}

static void
on_day_filter_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordPlanPage *page = user_data;

    (void)object;
    (void)pspec;
    refresh_views(page);
}

static void
on_jump_today(GtkButton *button, gpointer user_data)
{
    GswordPlanPage *page = user_data;
    GswordPlan *plan = selected_plan(page);
    GtkWidget *child;

    (void)button;
    if (page->days == NULL || plan == NULL || plan->next_day <= 0)
        return;
    for (child = gtk_widget_get_first_child(GTK_WIDGET(page->days));
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        const gchar *day = g_object_get_data(G_OBJECT(child), "plan-day");

        if (day != NULL && g_ascii_strtoll(day, NULL, 10) == plan->next_day) {
            gtk_widget_grab_focus(child);
            return;
        }
    }
}

static gboolean
create_plan_from_values(GswordPlanPage *page, const gchar *name,
                        const gchar *start, gint days, gint chapters)
{
    GswordPlanStore *plans = gsword_window_get_plan_store(page->window);
    GSettings *settings = gsword_window_get_settings(page->window);
    GPtrArray *references;
    GError *error = NULL;
    gint64 plan_id;
    gchar *id;

    if (plans == NULL)
        return FALSE;
    references = generate_plan_references(start, days, chapters);
    if (references->len == 0) {
        gsword_window_show_error(page->window, _("Cannot create plan"),
            g_error_new_literal(G_FILE_ERROR, G_FILE_ERROR_INVAL,
                                _("Invalid start reference or empty plan")));
        g_ptr_array_unref(references);
        return FALSE;
    }
    plan_id = gsword_plan_store_create_plan(plans, name, start, chapters,
                                            references, &error);
    g_ptr_array_unref(references);
    if (plan_id == 0) {
        gsword_window_show_error(page->window, _("Cannot create plan"), error);
        return FALSE;
    }
    id = g_strdup_printf("%" G_GINT64_FORMAT, plan_id);
    if (settings != NULL)
        g_settings_set_string(settings, "active-plan-id", id);
    g_free(id);
    gsword_plan_page_refresh(page);
    gsword_window_set_status(page->window, _("Reading plan created"));
    return TRUE;
}

static void
on_template_clicked(GtkButton *button, gpointer user_data)
{
    GswordPlanPage *page = user_data;
    const gchar *template = g_object_get_data(G_OBJECT(button), "template");

    if (g_strcmp0(template, "nt90") == 0) {
        create_plan_from_values(page, _("90-day New Testament"), "Matt 1",
                                90, 3);
    } else if (g_strcmp0(template, "psalms30") == 0) {
        create_plan_from_values(page, _("30-day Psalms"), "Ps 1", 30, 5);
    }
}

static void
clear_create_form(GswordPlanPage *page)
{
    if (page->preview_source != 0) {
        g_source_remove(page->preview_source);
        page->preview_source = 0;
    }
    page->create_dialog = NULL;
    page->name_entry = NULL;
    page->start_entry = NULL;
    page->days_spin = NULL;
    page->chapters_spin = NULL;
    page->preview = NULL;
}

static void
on_create_dialog_response(AdwAlertDialog *dialog, const gchar *response,
                          gpointer user_data)
{
    GswordPlanPage *page = user_data;

    (void)dialog;
    if (g_strcmp0(response, "create") == 0 && page->name_entry != NULL &&
        page->start_entry != NULL && page->days_spin != NULL &&
        page->chapters_spin != NULL) {
        const gchar *name = gtk_editable_get_text(
            GTK_EDITABLE(page->name_entry));
        const gchar *start = gtk_editable_get_text(
            GTK_EDITABLE(page->start_entry));
        gint days = gtk_spin_button_get_value_as_int(page->days_spin);
        gint chapters = gtk_spin_button_get_value_as_int(
            page->chapters_spin);

        create_plan_from_values(page, name, start, days, chapters);
    }
    clear_create_form(page);
}

static void
open_create_plan_dialog(GswordPlanPage *page)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *grid = gtk_grid_new();
    GtkWidget *name_label = gtk_label_new(_("Plan name"));
    GtkWidget *start_label = gtk_label_new(_("Start"));
    GtkWidget *days_label = gtk_label_new(_("Days"));
    GtkWidget *chapters_label = gtk_label_new(_("Chapters/day"));
    GtkWidget *hint;

    if (page->create_dialog != NULL) {
        adw_dialog_present(page->create_dialog, GTK_WIDGET(page->window));
        return;
    }
    page->create_dialog = adw_alert_dialog_new(_("Create a plan"), NULL);
    page->name_entry = GTK_ENTRY(gtk_entry_new());
    page->start_entry = GTK_ENTRY(gtk_entry_new());
    page->days_spin = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(1, 3650, 1));
    page->chapters_spin = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(1, 20, 1));
    page->preview = GTK_LABEL(gtk_label_new(""));

    gtk_entry_set_placeholder_text(page->name_entry, _("Plan name"));
    gtk_entry_set_placeholder_text(page->start_entry,
                                   _("Start, e.g. Genesis 1"));
    gtk_spin_button_set_value(page->days_spin, 365);
    gtk_spin_button_set_value(page->chapters_spin, 4);
    gtk_widget_set_hexpand(GTK_WIDGET(page->name_entry), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(page->start_entry), TRUE);
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(start_label), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(days_label), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(chapters_label), 0.0f);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_attach(GTK_GRID(grid), name_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(page->name_entry), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), start_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(page->start_entry), 1, 1,
                    1, 1);
    gtk_grid_attach(GTK_GRID(grid), days_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(page->days_spin), 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), chapters_label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(page->chapters_spin), 1, 3,
                    1, 1);
    gtk_label_set_wrap(page->preview, TRUE);
    gtk_label_set_xalign(page->preview, 0.0f);
    gtk_label_set_selectable(page->preview, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(page->preview), "caption");
    gtk_widget_add_css_class(GTK_WIDGET(page->preview), "dim-label");
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(page->preview), 0, 4, 2, 1);
    hint = gtk_label_new(
        _("The plan advances through chapters from the start reference."));
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_widget_set_margin_top(box, 4);
    gtk_widget_set_margin_bottom(box, 4);
    gtk_widget_set_size_request(box, 460, -1);
    gtk_box_append(GTK_BOX(box), grid);
    gtk_box_append(GTK_BOX(box), hint);
    adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(page->create_dialog),
                                     box);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(page->create_dialog),
                                   "cancel", _("Cancel"),
                                   "create", _("Create plan"), NULL);
    adw_alert_dialog_set_default_response(
        ADW_ALERT_DIALOG(page->create_dialog), "create");
    adw_alert_dialog_set_close_response(
        ADW_ALERT_DIALOG(page->create_dialog), "cancel");
    g_signal_connect(page->create_dialog, "response",
                     G_CALLBACK(on_create_dialog_response), page);
    g_signal_connect(page->name_entry, "changed",
                     G_CALLBACK(on_preview_changed), page);
    g_signal_connect(page->start_entry, "changed",
                     G_CALLBACK(on_preview_changed), page);
    g_signal_connect(page->days_spin, "value-changed",
                     G_CALLBACK(on_preview_changed), page);
    g_signal_connect(page->chapters_spin, "value-changed",
                     G_CALLBACK(on_preview_changed), page);
    update_create_response(page);
    adw_dialog_present(page->create_dialog, GTK_WIDGET(page->window));
    gtk_widget_grab_focus(GTK_WIDGET(page->name_entry));
}

static void
on_create_plan_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    open_create_plan_dialog(user_data);
}

static void
on_empty_create(GtkButton *button, gpointer user_data)
{
    GswordPlanPage *page = user_data;

    (void)button;
    gtk_notebook_set_current_page(page->notebook, 0);
    open_create_plan_dialog(page);
}

static void
on_delete_response(AdwAlertDialog *dialog, const gchar *response,
                   gpointer user_data)
{
    GswordPlanPage *page = user_data;
    GswordPlanStore *plans = gsword_window_get_plan_store(page->window);
    GswordPlan *plan = selected_plan(page);
    GSettings *settings = gsword_window_get_settings(page->window);
    GError *error = NULL;

    (void)dialog;
    if (g_strcmp0(response, "delete") != 0 || plan == NULL || plans == NULL)
        return;
    if (!gsword_plan_store_delete_plan(plans, plan->id, &error)) {
        gsword_window_show_error(page->window, _("Cannot delete plan"), error);
        return;
    }
    if (settings != NULL)
        g_settings_set_string(settings, "active-plan-id", "");
    page->selected_id = 0;
    gsword_plan_page_refresh(page);
    gsword_window_set_status(page->window, _("Reading plan deleted"));
}

static void
on_delete(GtkButton *button, gpointer user_data)
{
    GswordPlanPage *page = user_data;
    GswordPlan *plan = selected_plan(page);
    AdwDialog *dialog;

    (void)button;
    if (plan == NULL)
        return;
    dialog = adw_alert_dialog_new(_("Delete reading plan"), NULL);
    adw_alert_dialog_format_body(ADW_ALERT_DIALOG(dialog),
                                 _("Delete “%s” and its progress?"),
                                 plan->name);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "cancel", _("Cancel"),
                                   "delete", _("Delete"), NULL);
    adw_alert_dialog_set_response_appearance(
        ADW_ALERT_DIALOG(dialog), "delete", ADW_RESPONSE_DESTRUCTIVE);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    g_signal_connect(dialog, "response", G_CALLBACK(on_delete_response), page);
    adw_dialog_present(dialog, GTK_WIDGET(page->window));
}

void
gsword_plan_page_refresh_devotion(GswordPlanPage *page)
{
    g_return_if_fail(page != NULL);
    if (page->devotion != NULL)
        gsword_devotion_page_refresh(page->devotion);
}

void
gsword_plan_page_refresh_memory(GswordPlanPage *page)
{
    g_return_if_fail(page != NULL);
    if (page->memory != NULL)
        gsword_memory_page_refresh(page->memory);
}

void
gsword_plan_page_show_devotion(GswordPlanPage *page)
{
    g_return_if_fail(page != NULL);
    if (page->notebook != NULL)
        gtk_notebook_set_current_page(page->notebook, 1);
    gsword_plan_page_refresh_devotion(page);
    gsword_window_show_page(page->window, "plan");
}

void
gsword_plan_page_show_memory(GswordPlanPage *page)
{
    g_return_if_fail(page != NULL);
    if (page->notebook != NULL)
        gtk_notebook_set_current_page(page->notebook, 2);
    gsword_plan_page_refresh_memory(page);
    gsword_window_show_page(page->window, "plan");
}

static void
plan_page_free(gpointer data)
{
    GswordPlanPage *page = data;

    if (page == NULL)
        return;
    if (page->refresh_source != 0)
        g_source_remove(page->refresh_source);
    if (page->preview_source != 0)
        g_source_remove(page->preview_source);
    g_clear_pointer(&page->items, g_ptr_array_unref);
    g_clear_object(&page->names);
    g_free(page);
}

static void
on_management_height_changed(GObject *object, GParamSpec *pspec,
                             gpointer user_data)
{
    gint height = gtk_widget_get_height(GTK_WIDGET(object));

    (void)pspec;
    (void)user_data;
    if (height <= 0)
        return;
    gtk_paned_set_position(GTK_PANED(object), CLAMP(height - 150, 285, 380));
}

GswordPlanPage *
gsword_plan_page_new(GswordWindow *window)
{
    GswordPlanPage *page = g_new0(GswordPlanPage, 1);
    GtkWidget *notebook = gtk_notebook_new();
    GtkWidget *today_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *today_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *dashboard = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *empty_title;
    GtkWidget *empty_desc;
    GtkWidget *empty_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *empty_create;
    GtkWidget *empty_nt;
    GtkWidget *empty_psalms;
    GtkWidget *plans_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *plans_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *plans_label = gtk_label_new(_("Your plans"));
    GtkWidget *plans_scroll = gtk_scrolled_window_new();
    GtkWidget *rename_plan = gtk_button_new_from_icon_name(
        "document-edit-symbolic");
    GtkWidget *duplicate_plan = gtk_button_new_from_icon_name(
        "edit-copy-symbolic");
    GtkWidget *create = gtk_button_new_with_label(_("Create plan"));
    GtkWidget *days_scroll = gtk_scrolled_window_new();
    GtkWidget *open = gtk_button_new_with_label(_("Open today's reading"));
    GtkWidget *complete = gtk_button_new_with_label(_("Mark today complete"));
    GtkWidget *undo = gtk_button_new_with_label(_("Undo"));
    GtkWidget *jump_today = gtk_button_new_with_label(_("Today"));
    GtkWidget *schedule_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *schedule_label = gtk_label_new(_("Schedule"));
    GtkStringList *filters;
    GtkWidget *delete = gtk_button_new_from_icon_name("edit-delete-symbolic");

    page->window = window;
    gtk_widget_set_margin_top(today_page, 10);
    gtk_widget_set_margin_bottom(today_page, 10);
    gtk_widget_set_margin_start(today_page, 12);
    gtk_widget_set_margin_end(today_page, 12);
    gtk_widget_set_margin_top(plans_page, 10);
    gtk_widget_set_margin_bottom(plans_page, 10);
    gtk_widget_set_margin_start(plans_page, 12);
    gtk_widget_set_margin_end(plans_page, 12);

    page->plans_list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(page->plans_list, GTK_SELECTION_NONE);
    gtk_list_box_set_activate_on_single_click(page->plans_list, TRUE);
    g_signal_connect(page->plans_list, "row-activated",
                     G_CALLBACK(on_plan_list_activated), page);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(plans_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(plans_scroll),
                                  GTK_WIDGET(page->plans_list));
    gtk_widget_set_vexpand(plans_scroll, TRUE);
    gtk_widget_set_size_request(plans_scroll, -1, 80);
    gtk_label_set_xalign(GTK_LABEL(plans_label), 0.0f);
    gtk_widget_set_hexpand(plans_label, TRUE);
    gtk_widget_add_css_class(plans_label, "heading");
    gtk_widget_add_css_class(rename_plan, "flat");
    gtk_widget_add_css_class(duplicate_plan, "flat");
    gtk_widget_set_tooltip_text(rename_plan, _("Rename plan"));
    gtk_widget_set_tooltip_text(duplicate_plan, _("Duplicate plan"));
    gtk_widget_set_sensitive(rename_plan, FALSE);
    gtk_widget_set_sensitive(duplicate_plan, FALSE);
    g_signal_connect(rename_plan, "clicked", G_CALLBACK(on_rename_plan), page);
    g_signal_connect(duplicate_plan, "clicked",
                     G_CALLBACK(on_duplicate_plan), page);
    page->rename_plan_button = GTK_BUTTON(rename_plan);
    page->duplicate_plan_button = GTK_BUTTON(duplicate_plan);
    gtk_box_append(GTK_BOX(plans_header), plans_label);
    gtk_box_append(GTK_BOX(plans_header), rename_plan);
    gtk_box_append(GTK_BOX(plans_header), duplicate_plan);

    page->selector = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    gtk_widget_set_hexpand(GTK_WIDGET(page->selector), TRUE);
    g_signal_connect(page->selector, "notify::selected",
                     G_CALLBACK(on_plan_selected), page);
    page->summary = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->summary, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->summary), "heading");
    page->today = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->today, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->today), "title-2");
    page->open_button = GTK_BUTTON(open);
    page->complete_button = GTK_BUTTON(complete);
    page->undo_button = GTK_BUTTON(undo);
    gtk_widget_set_visible(undo, FALSE);
    g_signal_connect(open, "clicked", G_CALLBACK(on_open_today), page);
    g_signal_connect(complete, "clicked", G_CALLBACK(on_complete_today), page);
    g_signal_connect(undo, "clicked", G_CALLBACK(on_undo_complete), page);
    page->progress = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_progress_bar_set_show_text(page->progress, TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(page->progress), TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(page->progress), "plan-progress");
    page->progress_text = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->progress_text, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->progress_text), "caption");
    gtk_widget_add_css_class(GTK_WIDGET(page->progress_text), "dim-label");

    gtk_box_append(GTK_BOX(today_controls), GTK_WIDGET(page->selector));
    gtk_box_append(GTK_BOX(today_controls), open);
    gtk_box_append(GTK_BOX(today_controls), complete);
    gtk_box_append(GTK_BOX(today_controls), undo);
    gtk_box_append(GTK_BOX(dashboard), today_controls);
    gtk_box_append(GTK_BOX(dashboard), GTK_WIDGET(page->summary));
    gtk_box_append(GTK_BOX(dashboard), GTK_WIDGET(page->progress));
    gtk_box_append(GTK_BOX(dashboard), GTK_WIDGET(page->progress_text));
    gtk_box_append(GTK_BOX(dashboard), GTK_WIDGET(page->today));
    gtk_label_set_xalign(GTK_LABEL(schedule_label), 0.0f);
    gtk_widget_set_hexpand(schedule_label, TRUE);
    gtk_widget_add_css_class(schedule_label, "heading");
    page->day_filter = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    filters = gtk_string_list_new(
        (const gchar *[]) { _("All days"), _("Upcoming"), _("Completed"),
                            NULL });
    gtk_drop_down_set_model(page->day_filter, G_LIST_MODEL(filters));
    g_object_unref(filters);
    g_signal_connect(page->day_filter, "notify::selected",
                     G_CALLBACK(on_day_filter_changed), page);
    page->jump_today_button = GTK_BUTTON(jump_today);
    gtk_widget_add_css_class(jump_today, "flat");
    gtk_widget_set_tooltip_text(jump_today, _("Jump to today"));
    g_signal_connect(jump_today, "clicked", G_CALLBACK(on_jump_today), page);
    gtk_box_append(GTK_BOX(schedule_header), schedule_label);
    gtk_box_append(GTK_BOX(schedule_header), GTK_WIDGET(page->day_filter));
    gtk_box_append(GTK_BOX(schedule_header), jump_today);
    gtk_box_append(GTK_BOX(dashboard), schedule_header);
    page->days = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_activate_on_single_click(page->days, FALSE);
    g_signal_connect(page->days, "row-activated",
                     G_CALLBACK(on_day_activated), page);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(days_scroll),
                                  GTK_WIDGET(page->days));
    gtk_widget_set_vexpand(days_scroll, TRUE);
    gtk_box_append(GTK_BOX(dashboard), days_scroll);

    empty_title = gtk_label_new(_("Start a reading plan"));
    gtk_label_set_xalign(GTK_LABEL(empty_title), 0.0f);
    gtk_widget_add_css_class(empty_title, "title-2");
    empty_desc = gtk_label_new(
        _("Choose a template or create a custom plan to begin."));
    gtk_label_set_wrap(GTK_LABEL(empty_desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(empty_desc), 0.0f);
    gtk_widget_add_css_class(empty_desc, "dim-label");
    empty_create = gtk_button_new_with_label(_("Custom plan"));
    empty_nt = gtk_button_new_with_label(_("90-day New Testament"));
    empty_psalms = gtk_button_new_with_label(_("30-day Psalms"));
    gtk_widget_add_css_class(empty_create, "suggested-action");
    g_object_set_data(G_OBJECT(empty_nt), "template", "nt90");
    g_object_set_data(G_OBJECT(empty_psalms), "template", "psalms30");
    g_signal_connect(empty_create, "clicked",
                     G_CALLBACK(on_empty_create), page);
    g_signal_connect(empty_nt, "clicked",
                     G_CALLBACK(on_template_clicked), page);
    g_signal_connect(empty_psalms, "clicked",
                     G_CALLBACK(on_template_clicked), page);
    gtk_box_append(GTK_BOX(empty_buttons), empty_nt);
    gtk_box_append(GTK_BOX(empty_buttons), empty_psalms);
    gtk_box_append(GTK_BOX(empty_buttons), empty_create);
    gtk_widget_set_halign(empty_buttons, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(empty_box), empty_title);
    gtk_box_append(GTK_BOX(empty_box), empty_desc);
    gtk_box_append(GTK_BOX(empty_box), empty_buttons);
    gtk_widget_set_valign(empty_box, GTK_ALIGN_START);
    gtk_widget_set_margin_top(empty_box, 24);

    page->today_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_set_transition_type(page->today_stack,
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_add_named(page->today_stack, empty_box, "empty");
    gtk_stack_add_named(page->today_stack, dashboard, "dashboard");
    gtk_widget_set_vexpand(GTK_WIDGET(page->today_stack), TRUE);
    gtk_box_append(GTK_BOX(today_page), GTK_WIDGET(page->today_stack));

    page->delete_plan_button = GTK_BUTTON(delete);
    g_signal_connect(create, "clicked", G_CALLBACK(on_create_plan_clicked),
                     page);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_delete), page);
    gtk_widget_add_css_class(create, "suggested-action");
    gtk_widget_add_css_class(delete, "flat");
    gtk_widget_add_css_class(delete, "destructive-action");
    gtk_widget_set_tooltip_text(delete, _("Delete plan"));
    gtk_widget_set_sensitive(delete, FALSE);
    gtk_box_append(GTK_BOX(plans_header), create);
    gtk_box_append(GTK_BOX(plans_header), delete);
    gtk_box_append(GTK_BOX(plans_page), plans_header);
    gtk_box_append(GTK_BOX(plans_page), plans_scroll);

    page->devotion = gsword_devotion_page_new(window);
    page->memory = gsword_memory_page_new(window);
    {
        GtkWidget *management_page = gtk_paned_new(
            GTK_ORIENTATION_VERTICAL);

        gtk_paned_set_start_child(GTK_PANED(management_page), today_page);
        gtk_paned_set_end_child(GTK_PANED(management_page), plans_page);
        gtk_paned_set_position(GTK_PANED(management_page), 285);
        gtk_paned_set_resize_start_child(GTK_PANED(management_page), FALSE);
        gtk_paned_set_shrink_start_child(GTK_PANED(management_page), FALSE);
        gtk_paned_set_shrink_end_child(GTK_PANED(management_page), FALSE);
        g_signal_connect(management_page, "notify::height",
                         G_CALLBACK(on_management_height_changed), NULL);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), management_page,
                                 gtk_label_new(_("Plan management")));
    }
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             gsword_devotion_page_widget(page->devotion),
                             gtk_label_new(_("Devotion")));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             gsword_memory_page_widget(page->memory),
                             gtk_label_new(_("Memorize")));
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    page->notebook = GTK_NOTEBOOK(notebook);
    page->root = notebook;
    g_object_set_data_full(G_OBJECT(notebook), "gsword-plan-page", page,
                           plan_page_free);
    gsword_plan_page_refresh(page);
    return page;
}

GtkWidget *
gsword_plan_page_widget(GswordPlanPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}
