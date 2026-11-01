/*
 * plan-page.c: reading plan page for the gsword window
 */

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
    GtkLabel *today;
    GtkButton *open_button;
    GtkButton *complete_button;
    GtkListBox *days;
    GtkEntry *name_entry;
    GtkEntry *start_entry;
    GtkSpinButton *days_spin;
    GtkSpinButton *chapters_spin;
    gint64 selected_id;
    guint refresh_source;
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
        gtk_label_set_text(page->summary,
                           _("Create a reading plan to get started."));
        gtk_label_set_text(page->today, "");
        gtk_widget_set_sensitive(GTK_WIDGET(page->open_button), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(page->complete_button), FALSE);
        return;
    }
    page->selected_id = plan->id;
    {
        gdouble percent = plan->total_days > 0
                        ? (gdouble)plan->completed_days * 100.0 /
                          plan->total_days : 0.0;
        gchar *summary = g_strdup_printf(
            _("%s · %d/%d days (%.0f%%) · streak %d"),
            plan->name, plan->completed_days, plan->total_days, percent,
            plan->streak);

        gtk_label_set_text(page->summary, summary);
        g_free(summary);
    }
    if (plan->next_day > 0) {
        gchar *today = g_strdup_printf(_("Day %d: %s"), plan->next_day,
                                       plan->next_reference != NULL
                                       ? plan->next_reference : "");
        gtk_label_set_text(page->today, today);
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
    for (i = 0; days != NULL && i < days->len; i++) {
        GswordPlanDay *day = g_ptr_array_index(days, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *check = gtk_check_button_new();
        GtkWidget *label;
        gchar *day_number = g_strdup_printf("%d", day->day);
        gchar *text = g_strdup_printf(_("Day %d   %s"), day->day,
                                      day->reference != NULL
                                      ? day->reference : "");

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
        gtk_button_set_label(GTK_BUTTON(check), _("Done"));
        gtk_box_append(GTK_BOX(box), label);
        gtk_box_append(GTK_BOX(box), check);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        g_object_set_data_full(G_OBJECT(row), "plan-reference",
                               g_strdup(day->reference), g_free);
        gtk_list_box_append(page->days, row);
        g_free(text);
    }
    if (days != NULL)
        g_ptr_array_unref(days);
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
    gsword_window_set_status(page->window, _("Day marked complete"));
    gsword_plan_page_refresh(page);
}

static void
on_create(GtkButton *button, gpointer user_data)
{
    GswordPlanPage *page = user_data;
    GswordPlanStore *plans = gsword_window_get_plan_store(page->window);
    GSettings *settings = gsword_window_get_settings(page->window);
    const gchar *name = gtk_editable_get_text(GTK_EDITABLE(page->name_entry));
    const gchar *start = gtk_editable_get_text(GTK_EDITABLE(page->start_entry));
    gint days = gtk_spin_button_get_value_as_int(page->days_spin);
    gint chapters = gtk_spin_button_get_value_as_int(page->chapters_spin);
    GPtrArray *references;
    GError *error = NULL;
    gint64 plan_id;
    gchar *id;

    (void)button;
    if (plans == NULL)
        return;
    if (name == NULL || *name == '\0' || start == NULL || *start == '\0') {
        gsword_window_set_status(page->window,
                                 _("Enter a plan name and start reference"));
        return;
    }
    references = generate_plan_references(start, days, chapters);
    if (references->len == 0) {
        gsword_window_show_error(page->window, _("Cannot create plan"),
            g_error_new_literal(G_FILE_ERROR, G_FILE_ERROR_INVAL,
                                _("Invalid start reference or empty plan")));
        g_ptr_array_unref(references);
        return;
    }
    plan_id = gsword_plan_store_create_plan(plans, name, start, chapters,
                                            references, &error);
    g_ptr_array_unref(references);
    if (plan_id == 0) {
        gsword_window_show_error(page->window, _("Cannot create plan"), error);
        return;
    }
    id = g_strdup_printf("%" G_GINT64_FORMAT, plan_id);
    if (settings != NULL)
        g_settings_set_string(settings, "active-plan-id", id);
    g_free(id);
    gsword_plan_page_refresh(page);
    gsword_window_set_status(page->window, _("Reading plan created"));
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
        gtk_notebook_set_current_page(page->notebook, 2);
    gsword_plan_page_refresh_devotion(page);
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
    g_clear_pointer(&page->items, g_ptr_array_unref);
    g_clear_object(&page->names);
    g_free(page);
}

GswordPlanPage *
gsword_plan_page_new(GswordWindow *window)
{
    GswordPlanPage *page = g_new0(GswordPlanPage, 1);
    GtkWidget *notebook = gtk_notebook_new();
    GtkWidget *today_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *today_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *plans_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *plan_form = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *form_row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *form_row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *days_scroll = gtk_scrolled_window_new();
    GtkWidget *open = gtk_button_new_with_label(_("Open today's reading"));
    GtkWidget *complete = gtk_button_new_with_label(_("Mark today complete"));
    GtkWidget *create = gtk_button_new_with_label(_("Create plan"));
    GtkWidget *delete = gtk_button_new_with_label(_("Delete plan"));
    GtkWidget *hint;

    page->window = window;
    gtk_widget_set_margin_top(today_page, 10);
    gtk_widget_set_margin_bottom(today_page, 10);
    gtk_widget_set_margin_start(today_page, 12);
    gtk_widget_set_margin_end(today_page, 12);
    gtk_widget_set_margin_top(plans_page, 10);
    gtk_widget_set_margin_bottom(plans_page, 10);
    gtk_widget_set_margin_start(plans_page, 12);
    gtk_widget_set_margin_end(plans_page, 12);

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
    g_signal_connect(open, "clicked", G_CALLBACK(on_open_today), page);
    g_signal_connect(complete, "clicked", G_CALLBACK(on_complete_today), page);
    gtk_box_append(GTK_BOX(today_controls), GTK_WIDGET(page->selector));
    gtk_box_append(GTK_BOX(today_controls), open);
    gtk_box_append(GTK_BOX(today_controls), complete);
    gtk_box_append(GTK_BOX(today_page), today_controls);
    gtk_box_append(GTK_BOX(today_page), GTK_WIDGET(page->summary));
    gtk_box_append(GTK_BOX(today_page), GTK_WIDGET(page->today));
    {
        GtkWidget *label = gtk_label_new(_("Schedule"));

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_add_css_class(label, "heading");
        gtk_box_append(GTK_BOX(today_page), label);
    }
    page->days = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_activate_on_single_click(page->days, FALSE);
    g_signal_connect(page->days, "row-activated",
                     G_CALLBACK(on_day_activated), page);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(days_scroll),
                                  GTK_WIDGET(page->days));
    gtk_widget_set_vexpand(days_scroll, TRUE);
    gtk_box_append(GTK_BOX(today_page), days_scroll);

    page->name_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->name_entry, _("Plan name"));
    page->start_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->start_entry,
                                   _("Start, e.g. Genesis 1"));
    gtk_editable_set_width_chars(GTK_EDITABLE(page->start_entry), 16);
    page->days_spin = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(1, 3650, 1));
    gtk_spin_button_set_value(page->days_spin, 365);
    page->chapters_spin = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(1, 20, 1));
    gtk_spin_button_set_value(page->chapters_spin, 4);
    g_signal_connect(create, "clicked", G_CALLBACK(on_create), page);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_delete), page);
    gtk_widget_add_css_class(delete, "destructive-action");
    gtk_widget_set_hexpand(GTK_WIDGET(page->name_entry), TRUE);
    gtk_box_append(GTK_BOX(form_row1), GTK_WIDGET(page->name_entry));
    gtk_box_append(GTK_BOX(form_row1), GTK_WIDGET(page->start_entry));
    gtk_box_append(GTK_BOX(form_row1), gtk_label_new(_("Days")));
    gtk_box_append(GTK_BOX(form_row1), GTK_WIDGET(page->days_spin));
    gtk_box_append(GTK_BOX(form_row1), gtk_label_new(_("Chapters/day")));
    gtk_box_append(GTK_BOX(form_row1), GTK_WIDGET(page->chapters_spin));
    gtk_box_append(GTK_BOX(form_row2), create);
    gtk_box_append(GTK_BOX(form_row2), delete);
    gtk_box_append(GTK_BOX(plan_form), form_row1);
    gtk_box_append(GTK_BOX(plan_form), form_row2);
    hint = gtk_label_new(
        _("The plan advances through chapters from the start reference. "
          "Reading days can be opened by double-clicking a schedule row."));
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_box_append(GTK_BOX(plans_page), plan_form);
    gtk_box_append(GTK_BOX(plans_page), hint);

    page->devotion = gsword_devotion_page_new(window);
    page->memory = gsword_memory_page_new(window);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), today_page,
                             gtk_label_new(_("Today")));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), plans_page,
                             gtk_label_new(_("Plans")));
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
