/*
 * resource-page.c: general book and map viewer for the gsword window
 */

#include "resource-page.h"
#include "window-private.h"

#include <glib/gi18n.h>

#include <string.h>

typedef struct {
    gchar *path;
    gchar *name;
    gint depth;
} GswordResourceEntry;

struct _GswordResourcePage {
    GswordWindow *window;
    GtkWidget *root;
    GswModule *module;
    gchar *data_dir;
    gboolean map_mode;
    GtkLabel *title;
    GtkLabel *path;
    GtkListBox *tree;
    GtkStack *content;
    GtkTextView *text;
    GtkPicture *picture;
};

static void
resource_entry_free(gpointer data)
{
    GswordResourceEntry *entry = data;

    if (entry == NULL)
        return;
    g_free(entry->path);
    g_free(entry->name);
    g_free(entry);
}

static void
clear_list(GtkListBox *list)
{
    while (gtk_widget_get_first_child(GTK_WIDGET(list)) != NULL)
        gtk_list_box_remove(list,
                            gtk_widget_get_first_child(GTK_WIDGET(list)));
}

static void
collect_entries(GswTreeKey *node, GPtrArray *entries, gint depth)
{
    GswKey *cursor;

    if (!gsw_tree_key_has_children(node))
        return;
    cursor = gsw_key_clone((GswKey *)node);
    if (cursor == NULL)
        return;
    if (!gsw_tree_key_first_child((GswTreeKey *)cursor)) {
        gsw_key_unref(cursor);
        return;
    }
    do {
        GswordResourceEntry *entry = g_new0(GswordResourceEntry, 1);

        entry->path = g_strdup(gsw_tree_key_get_text((GswTreeKey *)cursor));
        entry->name = g_strdup(
            gsw_tree_key_get_local_name((GswTreeKey *)cursor));
        entry->depth = depth;
        g_ptr_array_add(entries, entry);
        if (gsw_tree_key_has_children((GswTreeKey *)cursor)) {
            GswKey *descendant = gsw_key_clone(cursor);

            if (descendant != NULL) {
                collect_entries((GswTreeKey *)descendant, entries, depth + 1);
                gsw_key_unref(descendant);
            }
        }
    } while (gsw_tree_key_next_sibling((GswTreeKey *)cursor));
    gsw_key_unref(cursor);
}

static void
show_path(GswordResourcePage *page, const gchar *path)
{
    GError *error = NULL;
    gchar *text;
    const gchar *raw;

    if (page->module == NULL || path == NULL)
        return;
    if (!gsw_module_set_key_text(page->module, path, &error)) {
        gsword_window_show_error(page->window,
                                 _("Cannot open resource entry"), error);
        return;
    }
    gtk_label_set_text(page->path, path);
    raw = gsw_module_peek_raw(page->module, NULL);
    if (page->map_mode && raw != NULL) {
        const gchar *image = strstr(raw, "<img");
        const gchar *src = image != NULL ? strstr(image, "src=\"") : NULL;

        if (src != NULL) {
            const gchar *end = strchr(src + 5, '"');

            if (end != NULL) {
                gchar *relative = g_strndup(src + 5,
                                            (gsize)(end - (src + 5)));
                gchar *file_path;
                GFile *file;

                while (g_str_has_prefix(relative, "/"))
                    memmove(relative, relative + 1, strlen(relative));
                file_path = g_build_filename(page->data_dir, relative, NULL);
                g_free(relative);
                if (g_file_test(file_path, G_FILE_TEST_IS_REGULAR)) {
                    file = g_file_new_for_path(file_path);
                    gtk_picture_set_file(page->picture, file);
                    g_object_unref(file);
                    gtk_stack_set_visible_child_name(page->content, "picture");
                    g_free(file_path);
                    return;
                }
                g_free(file_path);
            }
        }
    }

    text = gsw_module_get_text(page->module, &error);
    if (text == NULL) {
        gsword_window_show_error(page->window,
                                 _("Cannot read resource entry"), error);
        return;
    }
    gsword_window_set_view_text(page->text, text);
    g_free(text);
    gtk_stack_set_visible_child_name(page->content, "text");
}

static void
on_resource_selected(GtkListBox *list, GtkListBoxRow *row,
                     gpointer user_data)
{
    GswordResourcePage *page = user_data;
    const gchar *path;

    (void)list;
    if (row == NULL)
        return;
    path = g_object_get_data(G_OBJECT(row), "resource-path");
    if (path != NULL)
        show_path(page, path);
}

static void
fill_entries(GswordResourcePage *page, GPtrArray *entries)
{
    GtkListBoxRow *first = NULL;
    guint i;

    clear_list(page->tree);
    for (i = 0; i < entries->len; i++) {
        GswordResourceEntry *entry = g_ptr_array_index(entries, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label;
        gchar *indent = g_strnfill((gsize)(entry->depth * 2), ' ');
        gchar *text = g_strconcat(indent, entry->name, NULL);

        label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "resource-path",
                               g_strdup(entry->path), g_free);
        gtk_list_box_append(page->tree, row);
        if (first == NULL)
            first = GTK_LIST_BOX_ROW(row);
        g_free(text);
        g_free(indent);
    }
    if (first != NULL)
        gtk_list_box_select_row(page->tree, first);
}

void
gsword_resource_page_open(GswordResourcePage *page, const gchar *name,
                          gboolean map)
{
    GswManager *manager;
    GswModuleInfo *info;
    GPtrArray *entries;
    gchar *root_text;
    GError *error = NULL;

    g_return_if_fail(page != NULL);
    manager = gsword_window_get_manager(page->window);
    if (manager == NULL || name == NULL)
        return;
    info = gsw_manager_lookup_module_info(manager, name);
    if (info == NULL) {
        gsword_window_set_status(page->window, _("Cannot open module"));
        return;
    }
    g_clear_object(&page->module);
    page->module = gsw_manager_create_module(manager, name);
    if (page->module == NULL) {
        gsword_window_set_status(page->window, _("Cannot open module"));
        return;
    }
    g_free(page->data_dir);
    page->data_dir = g_strdup(gsw_module_info_get_absolute_data_path(info));
    page->map_mode = map;
    gtk_label_set_text(page->title, name);
    gtk_label_set_text(page->path, "");

    gsw_module_set_position(page->module, GSW_POSITION_TOP, NULL);
    entries = g_ptr_array_new_with_free_func(resource_entry_free);
    collect_entries((GswTreeKey *)gsw_module_get_key(page->module),
                    entries, 0);

    root_text = gsw_module_get_text(page->module, &error);
    if (root_text != NULL) {
        gsword_window_set_view_text(page->text, root_text);
        gtk_stack_set_visible_child_name(page->content, "text");
        g_free(root_text);
    }
    g_clear_error(&error);
    fill_entries(page, entries);
    g_ptr_array_unref(entries);
    gsword_window_show_page(page->window, "resource");
}

static void
on_resource_back(GtkButton *button, gpointer user_data)
{
    GswordResourcePage *page = user_data;

    (void)button;
    gsword_window_show_page(page->window, "reader");
}

static void
resource_page_free(gpointer data)
{
    GswordResourcePage *page = data;

    if (page == NULL)
        return;
    g_clear_object(&page->module);
    g_free(page->data_dir);
    g_free(page);
}

GswordResourcePage *
gsword_resource_page_new(GswordWindow *window)
{
    GswordResourcePage *page = g_new0(GswordResourcePage, 1);
    GtkWidget *box;
    GtkWidget *header;
    GtkWidget *paned;
    GtkWidget *tree_scroll;
    GtkWidget *content_scroll;
    GtkWidget *picture_scroll;
    GtkWidget *back;
    GtkWidget *text_box;

    page->window = window;
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    tree_scroll = gtk_scrolled_window_new();
    content_scroll = gtk_scrolled_window_new();
    picture_scroll = gtk_scrolled_window_new();
    back = gtk_button_new_with_label(_("Back to reading"));
    text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_widget_set_margin_top(header, 8);
    gtk_widget_set_margin_bottom(header, 8);
    gtk_widget_set_margin_start(header, 12);
    gtk_widget_set_margin_end(header, 12);
    page->title = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->title, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->title), "heading");
    gtk_widget_set_hexpand(GTK_WIDGET(page->title), TRUE);
    page->path = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->path, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(page->path), "dim-label");
    gtk_box_append(GTK_BOX(header), GTK_WIDGET(page->title));
    gtk_box_append(GTK_BOX(header), GTK_WIDGET(page->path));
    gtk_box_append(GTK_BOX(header), back);
    g_signal_connect(back, "clicked", G_CALLBACK(on_resource_back), page);

    page->tree = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(page->tree, "row-selected",
                     G_CALLBACK(on_resource_selected), page);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tree_scroll),
                                  GTK_WIDGET(page->tree));
    gtk_widget_set_size_request(tree_scroll, 280, -1);

    page->text = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(page->text, FALSE);
    gtk_text_view_set_cursor_visible(page->text, FALSE);
    gtk_text_view_set_wrap_mode(page->text, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(page->text, 16);
    gtk_text_view_set_right_margin(page->text, 16);
    gtk_text_view_set_top_margin(page->text, 16);
    gtk_text_view_set_bottom_margin(page->text, 16);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(content_scroll),
                                  GTK_WIDGET(page->text));
    gtk_widget_set_vexpand(content_scroll, TRUE);
    gtk_box_append(GTK_BOX(text_box), content_scroll);

    page->picture = GTK_PICTURE(gtk_picture_new());
    gtk_picture_set_can_shrink(page->picture, TRUE);
    gtk_picture_set_content_fit(page->picture, GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_vexpand(GTK_WIDGET(page->picture), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(page->picture), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(picture_scroll),
                                  GTK_WIDGET(page->picture));

    page->content = GTK_STACK(gtk_stack_new());
    gtk_stack_add_named(page->content, text_box, "text");
    gtk_stack_add_named(page->content, picture_scroll, "picture");
    gtk_widget_set_vexpand(GTK_WIDGET(page->content), TRUE);

    gtk_paned_set_start_child(GTK_PANED(paned), tree_scroll);
    gtk_paned_set_end_child(GTK_PANED(paned), GTK_WIDGET(page->content));
    gtk_paned_set_position(GTK_PANED(paned), 300);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_box_append(GTK_BOX(box), header);
    gtk_box_append(GTK_BOX(box), paned);
    page->root = box;
    g_object_set_data_full(G_OBJECT(box), "gsword-resource-page", page,
                           resource_page_free);
    return page;
}

GtkWidget *
gsword_resource_page_widget(GswordResourcePage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}
