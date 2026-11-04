/*
 * memory-page.c: memory verse review page for the gsword window
 */

#include "memory-page.h"
#include "window-private.h"

#include <glib/gi18n.h>

struct _GswordMemoryPage {
    GswordWindow *window;
    GtkWidget *root;
    GtkListBox *list;
    GPtrArray *items;
    GtkTextView *card;
    GtkLabel *title;
    GtkLabel *due;
    GtkEntry *input;
    GtkLabel *result;
    GtkDropDown *mode;
    gchar *module;
    gchar *key;
    gchar *display;
    gchar *text;
    gint64 id;
};

static gchar *
masked_text(const gchar *text, gboolean first_letters)
{
    GString *result = g_string_new(NULL);
    const gchar *p = text != NULL ? text : "";
    gboolean in_word = FALSE;

    while (*p != '\0') {
        gunichar c = g_utf8_get_char(p);

        p = g_utf8_next_char(p);
        if (g_unichar_isspace(c)) {
            g_string_append_unichar(result, c);
            in_word = FALSE;
        } else if (g_unichar_isalnum(c)) {
            if (first_letters && !in_word)
                g_string_append_unichar(result, c);
            else
                g_string_append_c(result, '_');
            in_word = TRUE;
        } else {
            g_string_append_unichar(result, c);
            in_word = FALSE;
        }
    }
    return g_string_free(result, FALSE);
}

static gchar *
normalized_text(const gchar *text)
{
    GString *result = g_string_new(NULL);
    const gchar *p = text != NULL ? text : "";

    while (*p != '\0') {
        gunichar c = g_utf8_get_char(p);

        p = g_utf8_next_char(p);
        if (g_unichar_isalnum(c))
            g_string_append_unichar(result, g_unichar_tolower(c));
    }
    return g_string_free(result, FALSE);
}

static void
update_card(GswordMemoryPage *page)
{
    guint mode;
    gchar *masked;

    if (page->card == NULL)
        return;
    if (page->text == NULL || *page->text == '\0') {
        gsword_window_set_view_text(page->card, "");
        return;
    }
    mode = page->mode != NULL ? gtk_drop_down_get_selected(page->mode) : 0;
    if (mode == 1)
        masked = masked_text(page->text, TRUE);
    else if (mode == 2)
        masked = masked_text(page->text, FALSE);
    else
        masked = g_strdup(page->text);
    gsword_window_set_view_text(page->card, masked);
    g_free(masked);
}

static void
clear_selection(GswordMemoryPage *page)
{
    page->id = 0;
    g_clear_pointer(&page->module, g_free);
    g_clear_pointer(&page->key, g_free);
    g_clear_pointer(&page->display, g_free);
    g_clear_pointer(&page->text, g_free);
    gtk_label_set_text(page->title, _("Select a verse card"));
    gtk_label_set_text(page->due, "");
    gtk_editable_set_text(GTK_EDITABLE(page->input), "");
    gtk_label_set_text(page->result, "");
    gsword_window_set_view_text(page->card, "");
}

static void
on_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordMemoryPage *page = user_data;
    const gchar *id_text;
    gint64 id;
    guint i;

    (void)list;
    if (row == NULL || page->items == NULL) {
        clear_selection(page);
        return;
    }
    id_text = g_object_get_data(G_OBJECT(row), "memory-id");
    if (id_text == NULL)
        return;
    id = g_ascii_strtoll(id_text, NULL, 10);
    for (i = 0; i < page->items->len; i++) {
        GswordMemoryVerse *verse = g_ptr_array_index(page->items, i);
        gchar *due;

        if (verse->id != id)
            continue;
        page->id = verse->id;
        g_free(page->module);
        g_free(page->key);
        g_free(page->display);
        g_free(page->text);
        page->module = g_strdup(verse->module);
        page->key = g_strdup(verse->key);
        page->display = g_strdup(verse->display_key);
        page->text = g_strdup(verse->text);
        due = g_strdup_printf(_("Due %s  ·  box %d"), verse->due, verse->box);
        gtk_label_set_text(page->title, verse->display_key);
        gtk_label_set_text(page->due, due);
        g_free(due);
        gtk_editable_set_text(GTK_EDITABLE(page->input), "");
        gtk_label_set_text(page->result, "");
        update_card(page);
        return;
    }
}

void
gsword_memory_page_refresh(GswordMemoryPage *page)
{
    GswordPlanStore *plans;
    guint i;
    GtkListBoxRow *wanted = NULL;
    GtkListBoxRow *first = NULL;

    g_return_if_fail(page != NULL);
    plans = gsword_window_get_plan_store(page->window);
    if (page->list == NULL || plans == NULL)
        return;
    while (gtk_widget_get_first_child(GTK_WIDGET(page->list)) != NULL)
        gtk_list_box_remove(page->list,
                            gtk_widget_get_first_child(GTK_WIDGET(page->list)));
    g_clear_pointer(&page->items, g_ptr_array_unref);
    page->items = gsword_plan_store_list_memory_verses(plans);

    for (i = 0; i < page->items->len; i++) {
        GswordMemoryVerse *verse = g_ptr_array_index(page->items, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label;
        gchar *text = g_strdup_printf(_("%s\nDue %s  ·  box %d"),
                                      verse->display_key, verse->due,
                                      verse->box);
        gchar *id = g_strdup_printf("%" G_GINT64_FORMAT, verse->id);

        label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "memory-id", id, g_free);
        gtk_list_box_append(page->list, row);
        if (first == NULL)
            first = GTK_LIST_BOX_ROW(row);
        if (verse->id == page->id)
            wanted = GTK_LIST_BOX_ROW(row);
        g_free(text);
    }

    if (wanted == NULL)
        wanted = first;
    if (wanted != NULL)
        gtk_list_box_select_row(page->list, wanted);
    else
        clear_selection(page);
}

static void
on_mode_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    update_card(user_data);
}

static void
on_add_current(GtkButton *button, gpointer user_data)
{
    GswordMemoryPage *page = user_data;
    GswordPlanStore *plans = gsword_window_get_plan_store(page->window);
    GswModule *module;
    gchar *storage_key;
    gchar *display_key;
    gchar *text;
    GError *error = NULL;
    GPtrArray *items;
    guint i;

    (void)button;
    if (plans == NULL)
        return;
    module = gsword_window_ensure_verse_module(page->window);
    if (module == NULL)
        return;
    storage_key = gsword_window_current_storage_key(page->window);
    display_key = gsword_window_current_display_key(page->window);
    text = gsw_module_get_text(module, &error);
    if (storage_key == NULL || display_key == NULL || text == NULL) {
        gsword_window_show_error(page->window,
                                 _("Cannot add memory verse"), error);
        g_free(storage_key);
        g_free(display_key);
        g_free(text);
        return;
    }
    if (!gsword_plan_store_add_memory_verse(
            plans, gsw_module_get_name(module), storage_key, display_key,
            text, &error)) {
        gsword_window_show_error(page->window,
                                 _("Cannot save memory verse"), error);
        g_free(storage_key);
        g_free(display_key);
        g_free(text);
        return;
    }
    items = gsword_plan_store_list_memory_verses(plans);
    for (i = 0; i < items->len; i++) {
        GswordMemoryVerse *verse = g_ptr_array_index(items, i);

        if (g_strcmp0(verse->module, gsw_module_get_name(module)) == 0 &&
            g_strcmp0(verse->key, storage_key) == 0) {
            page->id = verse->id;
            break;
        }
    }
    g_ptr_array_unref(items);
    g_free(storage_key);
    g_free(display_key);
    g_free(text);
    gsword_memory_page_refresh(page);
    gsword_window_set_status(page->window,
                             _("Verse added to memory review"));
}

static void
on_review(GtkButton *button, gpointer user_data)
{
    GswordMemoryPage *page = user_data;
    GswordPlanStore *plans = gsword_window_get_plan_store(page->window);
    gboolean remembered = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(button), "remembered"));
    GError *error = NULL;

    if (plans == NULL || page->id == 0)
        return;
    if (!gsword_plan_store_review_memory_verse(plans, page->id, remembered,
                                               &error)) {
        gsword_window_show_error(page->window,
                                 _("Cannot schedule review"), error);
        return;
    }
    gsword_memory_page_refresh(page);
    gsword_window_set_status(page->window, remembered
                             ? _("Review scheduled: nice work")
                             : _("Card will be shown again today"));
}

static void
on_check(GtkButton *button, gpointer user_data)
{
    GswordMemoryPage *page = user_data;
    const gchar *answer;
    gchar *normalized_answer;
    gchar *normalized;

    (void)button;
    if (page->text == NULL)
        return;
    answer = gtk_editable_get_text(GTK_EDITABLE(page->input));
    normalized_answer = normalized_text(answer);
    normalized = normalized_text(page->text);
    if (*normalized_answer != '\0' &&
        g_strcmp0(normalized_answer, normalized) == 0) {
        gtk_label_set_text(page->result, _("Correct"));
        gsword_window_set_status(page->window,
                                 _("Memory verse recited correctly"));
    } else {
        gtk_label_set_text(page->result, _("Keep practicing"));
    }
    g_free(normalized_answer);
    g_free(normalized);
}

static void
on_delete(GtkButton *button, gpointer user_data)
{
    GswordMemoryPage *page = user_data;
    GswordPlanStore *plans = gsword_window_get_plan_store(page->window);
    GError *error = NULL;

    (void)button;
    if (plans == NULL || page->id == 0)
        return;
    if (!gsword_plan_store_delete_memory_verse(plans, page->id, &error)) {
        gsword_window_show_error(page->window,
                                 _("Cannot delete memory verse"), error);
        return;
    }
    page->id = 0;
    gsword_memory_page_refresh(page);
    gsword_window_set_status(page->window, _("Memory verse removed"));
}

static void
memory_page_free(gpointer data)
{
    GswordMemoryPage *page = data;

    if (page == NULL)
        return;
    g_clear_pointer(&page->items, g_ptr_array_unref);
    g_free(page->module);
    g_free(page->key);
    g_free(page->display);
    g_free(page->text);
    g_free(page);
}

GswordMemoryPage *
gsword_memory_page_new(GswordWindow *window)
{
    GswordMemoryPage *page = g_new0(GswordMemoryPage, 1);
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *card_scroll = gtk_scrolled_window_new();
    GtkWidget *mode_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *review_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *add_current = gtk_button_new_with_label(_("Add current verse"));
    GtkWidget *delete = gtk_button_new_with_label(_("Delete"));
    GtkWidget *check = gtk_button_new_with_label(_("Check answer"));
    GtkWidget *again = gtk_button_new_with_label(_("Again"));
    GtkWidget *good = gtk_button_new_with_label(_("Good"));
    GtkStringList *modes = gtk_string_list_new(
        (const gchar * const[]) { _("Full text"), _("First letters"),
                                  _("Hide words"), NULL });

    page->window = window;
    gtk_widget_set_margin_top(left, 10);
    gtk_widget_set_margin_bottom(left, 10);
    gtk_widget_set_margin_start(left, 12);
    gtk_widget_set_margin_end(left, 12);
    gtk_widget_set_margin_top(right, 10);
    gtk_widget_set_margin_bottom(right, 10);
    gtk_widget_set_margin_start(right, 12);
    gtk_widget_set_margin_end(right, 12);

    page->list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(page->list, "row-selected",
                     G_CALLBACK(on_selected), page);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
                                  GTK_WIDGET(page->list));
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_size_request(scroll, 260, -1);
    g_signal_connect(add_current, "clicked", G_CALLBACK(on_add_current), page);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_delete), page);
    gtk_box_append(GTK_BOX(buttons), add_current);
    gtk_box_append(GTK_BOX(buttons), delete);
    gtk_box_append(GTK_BOX(left), scroll);
    gtk_box_append(GTK_BOX(left), buttons);

    page->title = GTK_LABEL(gtk_label_new(_("Select a verse card")));
    gtk_label_set_xalign(page->title, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->title), "title-2");
    page->due = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->due, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->due), "dim-label");
    page->card = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(page->card, FALSE);
    gtk_text_view_set_cursor_visible(page->card, FALSE);
    gtk_text_view_set_wrap_mode(page->card, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(page->card, 12);
    gtk_text_view_set_right_margin(page->card, 12);
    gtk_text_view_set_top_margin(page->card, 12);
    gtk_text_view_set_bottom_margin(page->card, 12);
    gtk_widget_add_css_class(GTK_WIDGET(page->card), "memory-card");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(card_scroll),
                                  GTK_WIDGET(page->card));
    gtk_widget_set_vexpand(card_scroll, TRUE);
    page->mode = GTK_DROP_DOWN(gtk_drop_down_new(G_LIST_MODEL(modes), NULL));
    g_signal_connect(page->mode, "notify::selected",
                     G_CALLBACK(on_mode_changed), page);
    gtk_box_append(GTK_BOX(mode_row), gtk_label_new(_("Card mode")));
    gtk_box_append(GTK_BOX(mode_row), GTK_WIDGET(page->mode));
    page->input = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(page->input, _("Type the verse"));
    gtk_widget_set_hexpand(GTK_WIDGET(page->input), TRUE);
    g_signal_connect(check, "clicked", G_CALLBACK(on_check), page);
    gtk_box_append(GTK_BOX(input_row), GTK_WIDGET(page->input));
    gtk_box_append(GTK_BOX(input_row), check);
    page->result = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->result, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->result), "dim-label");
    g_object_set_data(G_OBJECT(again), "remembered", GINT_TO_POINTER(0));
    g_object_set_data(G_OBJECT(good), "remembered", GINT_TO_POINTER(1));
    g_signal_connect(again, "clicked", G_CALLBACK(on_review), page);
    g_signal_connect(good, "clicked", G_CALLBACK(on_review), page);
    gtk_box_append(GTK_BOX(review_row), again);
    gtk_box_append(GTK_BOX(review_row), good);
    gtk_box_append(GTK_BOX(right), GTK_WIDGET(page->title));
    gtk_box_append(GTK_BOX(right), GTK_WIDGET(page->due));
    gtk_box_append(GTK_BOX(right), card_scroll);
    gtk_box_append(GTK_BOX(right), mode_row);
    gtk_box_append(GTK_BOX(right), input_row);
    gtk_box_append(GTK_BOX(right), GTK_WIDGET(page->result));
    gtk_box_append(GTK_BOX(right), review_row);
    gtk_paned_set_start_child(GTK_PANED(paned), left);
    gtk_paned_set_end_child(GTK_PANED(paned), right);
    gtk_paned_set_position(GTK_PANED(paned), 280);
    page->root = paned;
    g_object_set_data_full(G_OBJECT(paned), "gsword-memory-page", page,
                           memory_page_free);
    gsword_memory_page_refresh(page);
    return page;
}

GtkWidget *
gsword_memory_page_widget(GswordMemoryPage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}
