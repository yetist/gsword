/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * resource-page.c: general book and map viewer for the gsword window
 */

#include "resource-page.h"
#include "app-paths.h"
#include "image-utils.h"
#include "image-view.h"
#include "window-private.h"

#include <glib/gi18n.h>
#include <string.h>

typedef struct {
    gchar *path;
    gchar *name;
    gint depth;
} GswordResourceEntry;

struct _GswordResourcePage {
    GswordPage parent_instance;

    GswModule *module;
    gchar *data_dir;
    gboolean map_mode;
    GtkLabel *title;
    GtkLabel *path;
    GtkListBox *tree;
    GtkStack *content;
    GtkTextView *text;
    GswordImageView *image_view;
    GtkWidget *image_controls;
    GtkButton *zoom_out;
    GtkButton *zoom_in;
    GtkLabel *zoom_label;
    GtkPopover *image_context_menu;
};

G_DEFINE_TYPE(GswordResourcePage, gsword_resource_page, GSWORD_TYPE_PAGE)

#define RESOURCE_WINDOW(page) \
    gsword_page_get_window(GSWORD_PAGE(page))

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
set_icon_button_label(GtkWidget *button, const gchar *label)
{
    gtk_widget_set_tooltip_text(button, label);
    gtk_accessible_update_property(GTK_ACCESSIBLE(button),
                                   GTK_ACCESSIBLE_PROPERTY_LABEL, label, -1);
}

static void
update_image_controls(GswordResourcePage *page)
{
    gdouble zoom;
    gchar *label;

    if (!gsword_image_view_has_image(page->image_view)) {
        gtk_label_set_text(page->zoom_label, "100%");
        gtk_widget_set_sensitive(GTK_WIDGET(page->zoom_out), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(page->zoom_in), FALSE);
        return;
    }
    zoom = gsword_image_view_get_zoom(page->image_view);
    label = g_strdup_printf("%d%%", (gint)(zoom * 100.0 + 0.5));
    gtk_label_set_text(page->zoom_label, label);
    g_free(label);
    gtk_widget_set_sensitive(GTK_WIDGET(page->zoom_out),
                             zoom > GSWORD_IMAGE_MIN_ZOOM + 1e-6);
    gtk_widget_set_sensitive(GTK_WIDGET(page->zoom_in),
                             zoom < GSWORD_IMAGE_MAX_ZOOM - 1e-6);
}

static void
clear_image(GswordResourcePage *page)
{
    if (page->image_view != NULL)
        gsword_image_view_clear(page->image_view);
    if (page->zoom_label != NULL)
        gtk_label_set_text(page->zoom_label, "100%");
    if (page->image_controls != NULL)
        gtk_widget_set_visible(page->image_controls, FALSE);
}

static void
on_image_zoom_changed(GswordImageView *view, gpointer user_data)
{
    GswordResourcePage *page = user_data;

    (void)view;
    update_image_controls(page);
}

static gboolean
image_texture_same_extension(const gchar *source, const gchar *destination)
{
    const gchar *source_dot = strrchr(source, '.');
    const gchar *destination_dot = strrchr(destination, '.');
    const gchar *source_slash = strrchr(source, '/');
    const gchar *destination_slash = strrchr(destination, '/');

    if (source_dot == NULL || destination_dot == NULL)
        return FALSE;
    if (source_slash != NULL && source_dot < source_slash)
        return FALSE;
    if (destination_slash != NULL && destination_dot < destination_slash)
        return FALSE;
    return g_ascii_strcasecmp(source_dot, destination_dot) == 0;
}

static gboolean
image_texture_has_png_extension(const gchar *path)
{
    const gchar *dot = strrchr(path, '.');
    const gchar *slash = strrchr(path, '/');

    if (dot == NULL || (slash != NULL && dot < slash))
        return FALSE;
    return g_ascii_strcasecmp(dot, ".png") == 0;
}

static void
copy_image_to_clipboard(GswordResourcePage *page)
{
    GdkTexture *texture = gsword_image_view_get_texture(page->image_view);
    GdkClipboard *clipboard;

    if (texture == NULL)
        return;
    clipboard = gtk_widget_get_clipboard(GTK_WIDGET(RESOURCE_WINDOW(page)));
    gdk_clipboard_set_texture(clipboard, texture);
    gsword_window_toast(RESOURCE_WINDOW(page), _("Image copied to clipboard"));
}

static void
copy_image_path_to_clipboard(GswordResourcePage *page)
{
    const gchar *path = gsword_image_view_get_path(page->image_view);
    GdkClipboard *clipboard;

    if (path == NULL)
        return;
    clipboard = gtk_widget_get_clipboard(GTK_WIDGET(RESOURCE_WINDOW(page)));
    gdk_clipboard_set_text(clipboard, path);
    gsword_window_toast(RESOURCE_WINDOW(page),
                        _("Image path copied to clipboard"));
}

static void
on_save_image_finish(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GswordResourcePage *page = user_data;
    const gchar *source_path = g_object_get_data(source, "source-path");
    GdkTexture *texture = gsword_image_view_get_texture(page->image_view);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result,
                                              &error);

    if (file == NULL) {
        g_clear_error(&error);
        return;
    }
    if (source_path == NULL || texture == NULL) {
        g_set_error_literal(&error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                            _("No image is selected"));
    } else {
        gchar *destination_path = g_file_get_path(file);

        if (destination_path == NULL) {
            g_set_error_literal(&error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                                _("The selected destination is not a local file"));
        } else if (image_texture_same_extension(source_path,
                                                destination_path)) {
            GFile *source_file = g_file_new_for_path(source_path);

            if (!g_file_copy(source_file, file, G_FILE_COPY_OVERWRITE,
                             NULL, NULL, NULL, &error)) {
                /* Keep the error from g_file_copy. */
            }
            g_object_unref(source_file);
        } else if (image_texture_has_png_extension(destination_path)) {
            GBytes *bytes = gdk_texture_save_to_png_bytes(texture);

            if (bytes == NULL) {
                g_set_error_literal(&error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                                    _("Cannot convert image to PNG"));
            } else {
                gconstpointer data = g_bytes_get_data(bytes, NULL);
                gsize size = g_bytes_get_size(bytes);

                g_file_replace_contents(file, data, size, NULL, FALSE,
                                        G_FILE_CREATE_REPLACE_DESTINATION,
                                        NULL, NULL, &error);
                g_bytes_unref(bytes);
            }
        } else {
            g_set_error_literal(&error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                                _("Choose the original image extension or PNG"));
        }
        g_free(destination_path);
    }

    if (error != NULL) {
        gsword_window_show_error(RESOURCE_WINDOW(page), _("Cannot save image"), error);
        g_clear_error(&error);
    } else {
        gchar *name = g_file_get_basename(file);
        gchar *message = g_strdup_printf(_("Image saved as %s"), name);

        g_free(name);
        gsword_window_toast(RESOURCE_WINDOW(page), message);
        g_free(message);
    }
    g_object_unref(file);
}

static void
save_image_dialog(GswordResourcePage *page)
{
    const gchar *source_path = gsword_image_view_get_path(page->image_view);
    GtkFileDialog *dialog;
    GFile *folder = NULL;
    const gchar *pictures;
    gchar *basename;
    gchar *suggested_name;
    const gchar *module_name;

    if (source_path == NULL)
        return;
    dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, _("Save map image"));

    pictures = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
    if (pictures != NULL && *pictures != '\0') {
        folder = g_file_new_for_path(pictures);
    } else {
        gchar *exports_dir = gsword_app_exports_dir();

        g_mkdir_with_parents(exports_dir, 0755);
        folder = g_file_new_for_path(exports_dir);
        g_free(exports_dir);
    }
    if (folder != NULL) {
        gtk_file_dialog_set_initial_folder(dialog, folder);
        g_object_unref(folder);
    }

    module_name = page->module != NULL ? gsw_module_get_name(page->module)
                                       : "gsword-map";
    basename = g_path_get_basename(source_path);
    suggested_name = g_strdup_printf("%s-%s", module_name, basename);
    gtk_file_dialog_set_initial_name(dialog, suggested_name);
    g_free(suggested_name);
    g_free(basename);
    g_object_set_data_full(G_OBJECT(dialog), "source-path",
                           g_strdup(source_path), g_free);
    gtk_file_dialog_save(dialog, GTK_WINDOW(RESOURCE_WINDOW(page)), NULL,
                         on_save_image_finish, page);
    g_object_unref(dialog);
}

static void
on_zoom_out_clicked(GtkButton *button, gpointer user_data)
{
    GswordResourcePage *page = user_data;

    (void)button;
    gsword_image_view_zoom_out(page->image_view);
}

static void
on_zoom_in_clicked(GtkButton *button, gpointer user_data)
{
    GswordResourcePage *page = user_data;

    (void)button;
    gsword_image_view_zoom_in(page->image_view);
}

static void
on_fit_width_clicked(GtkButton *button, gpointer user_data)
{
    GswordResourcePage *page = user_data;

    (void)button;
    gsword_image_view_fit_width(page->image_view);
}

static void
close_image_popover(GtkWidget *button)
{
    GtkWidget *popover = gtk_widget_get_ancestor(button, GTK_TYPE_POPOVER);

    if (popover != NULL)
        gtk_popover_popdown(GTK_POPOVER(popover));
}

static void
on_menu_fit_width(GtkButton *button, gpointer user_data)
{
    GswordResourcePage *page = user_data;

    close_image_popover(GTK_WIDGET(button));
    gsword_image_view_fit_width(page->image_view);
}

static void
on_menu_fit_window(GtkButton *button, gpointer user_data)
{
    GswordResourcePage *page = user_data;

    close_image_popover(GTK_WIDGET(button));
    gsword_image_view_fit_window(page->image_view);
}

static void
on_menu_actual_size(GtkButton *button, gpointer user_data)
{
    GswordResourcePage *page = user_data;

    close_image_popover(GTK_WIDGET(button));
    gsword_image_view_actual_size(page->image_view);
}

static void
on_menu_copy_image(GtkButton *button, gpointer user_data)
{
    close_image_popover(GTK_WIDGET(button));
    copy_image_to_clipboard(user_data);
}

static void
on_menu_copy_image_path(GtkButton *button, gpointer user_data)
{
    close_image_popover(GTK_WIDGET(button));
    copy_image_path_to_clipboard(user_data);
}

static void
on_menu_save_image(GtkButton *button, gpointer user_data)
{
    close_image_popover(GTK_WIDGET(button));
    save_image_dialog(user_data);
}

static GtkWidget *
image_menu_item(const gchar *label, GCallback callback,
                GswordResourcePage *page)
{
    GtkWidget *button = gtk_button_new_with_label(label);

    gtk_widget_add_css_class(button, "flat");
    gtk_widget_set_halign(button, GTK_ALIGN_FILL);
    g_signal_connect(button, "clicked", callback, page);
    return button;
}

static GtkWidget *
build_image_popover(GswordResourcePage *page)
{
    GtkWidget *popover = gtk_popover_new();
    GtkWidget *menu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);

    gtk_popover_set_autohide(GTK_POPOVER(popover), TRUE);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), TRUE);
    gtk_widget_set_margin_top(menu, 6);
    gtk_widget_set_margin_bottom(menu, 6);
    gtk_widget_set_margin_start(menu, 6);
    gtk_widget_set_margin_end(menu, 6);
    gtk_box_append(GTK_BOX(menu),
                   image_menu_item(_("Fit width"),
                                   G_CALLBACK(on_menu_fit_width), page));
    gtk_box_append(GTK_BOX(menu),
                   image_menu_item(_("Fit window"),
                                   G_CALLBACK(on_menu_fit_window), page));
    gtk_box_append(GTK_BOX(menu),
                   image_menu_item(_("Actual size"),
                                   G_CALLBACK(on_menu_actual_size), page));
    gtk_box_append(GTK_BOX(menu), separator);
    gtk_box_append(GTK_BOX(menu),
                   image_menu_item(_("Copy image"),
                                   G_CALLBACK(on_menu_copy_image), page));
    gtk_box_append(GTK_BOX(menu),
                   image_menu_item(_("Copy image path"),
                                   G_CALLBACK(on_menu_copy_image_path),
                                   page));
    gtk_box_append(GTK_BOX(menu),
                   image_menu_item(_("Save image as…"),
                                   G_CALLBACK(on_menu_save_image), page));
    gtk_popover_set_child(GTK_POPOVER(popover), menu);
    return popover;
}

static gboolean
show_image(GswordResourcePage *page, const gchar *raw, GError **error)
{
    gchar *source = gsword_image_extract_src(raw);
    gchar *path;
    gchar *description;

    if (source == NULL)
        return FALSE;
    path = gsword_image_resolve_path(page->data_dir, source, error);
    g_free(source);
    if (path == NULL)
        return FALSE;
    if (!gsword_image_view_load(page->image_view, path, error)) {
        g_free(path);
        return FALSE;
    }
    description = g_path_get_basename(path);
    gtk_accessible_update_property(
        GTK_ACCESSIBLE(page->image_view),
        GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, description, -1);
    g_free(description);
    g_free(path);
    gtk_widget_set_visible(page->image_controls, TRUE);
    update_image_controls(page);
    return TRUE;
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
        gsword_window_show_error(RESOURCE_WINDOW(page),
                                 _("Cannot open resource entry"), error);
        g_clear_error(&error);
        return;
    }
    gtk_label_set_text(page->path, path);
    gtk_widget_set_tooltip_text(GTK_WIDGET(page->path), path);
    raw = gsw_module_peek_raw(page->module, NULL);
    if (page->map_mode && raw != NULL) {
        GError *image_error = NULL;

        if (show_image(page, raw, &image_error)) {
            gtk_stack_set_visible_child_name(page->content, "picture");
            return;
        }
        if (image_error != NULL) {
            gsword_window_show_error(RESOURCE_WINDOW(page),
                                     _("Cannot load map image"), image_error);
            g_clear_error(&image_error);
        }
    }

    clear_image(page);
    text = gsw_module_get_text(page->module, &error);
    if (text == NULL) {
        gsword_window_show_error(RESOURCE_WINDOW(page),
                                 _("Cannot read resource entry"), error);
        g_clear_error(&error);
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

    g_return_if_fail(GSWORD_IS_RESOURCE_PAGE(page));
    manager = gsword_window_get_manager(RESOURCE_WINDOW(page));
    if (manager == NULL || name == NULL)
        return;
    info = gsw_manager_lookup_module_info(manager, name);
    if (info == NULL) {
        gsword_window_set_status(RESOURCE_WINDOW(page), _("Cannot open module"));
        return;
    }
    g_clear_object(&page->module);
    page->module = gsw_manager_create_module(manager, name);
    if (page->module == NULL) {
        gsword_window_set_status(RESOURCE_WINDOW(page), _("Cannot open module"));
        return;
    }
    g_free(page->data_dir);
    page->data_dir = g_strdup(gsw_module_info_get_absolute_data_path(info));
    page->map_mode = map;
    gtk_label_set_text(page->title, name);
    gtk_label_set_text(page->path, "");
    gtk_widget_set_tooltip_text(GTK_WIDGET(page->path), NULL);
    clear_image(page);

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
    gsword_window_show_page(RESOURCE_WINDOW(page), "resource");
}

static void
on_resource_back(GtkButton *button, gpointer user_data)
{
    GswordResourcePage *page = user_data;

    (void)button;
    gsword_window_show_page(RESOURCE_WINDOW(page), "reader");
}

static void
on_image_context_clicked(GtkGestureClick *gesture, gint n_press,
                         gdouble x, gdouble y, gpointer user_data)
{
    GswordResourcePage *page = user_data;
    GdkRectangle rect;

    (void)gesture;
    (void)n_press;
    if (!gsword_image_view_has_image(page->image_view) ||
        page->image_context_menu == NULL)
        return;
    rect.x = (gint)(x + 0.5);
    rect.y = (gint)(y + 0.5);
    rect.width = 1;
    rect.height = 1;
    gtk_popover_set_pointing_to(page->image_context_menu, &rect);
    gtk_popover_popup(page->image_context_menu);
}

static gboolean
on_image_key_pressed(GtkEventControllerKey *controller, guint keyval,
                     guint keycode, GdkModifierType state, gpointer user_data)
{
    GswordResourcePage *page = user_data;

    (void)controller;
    (void)keycode;
    if ((state & GDK_CONTROL_MASK) != 0 && keyval == GDK_KEY_c) {
        copy_image_to_clipboard(page);
        return TRUE;
    }
    if ((state & GDK_CONTROL_MASK) != 0 &&
        (state & GDK_SHIFT_MASK) != 0 &&
        (keyval == GDK_KEY_s || keyval == GDK_KEY_S)) {
        save_image_dialog(page);
        return TRUE;
    }
    return FALSE;
}

static void
on_image_view_destroy(GtkWidget *widget, gpointer user_data)
{
    GswordResourcePage *page = user_data;

    (void)widget;
    if (page->image_context_menu != NULL &&
        gtk_widget_get_parent(GTK_WIDGET(page->image_context_menu)) != NULL)
        gtk_widget_unparent(GTK_WIDGET(page->image_context_menu));
    page->image_context_menu = NULL;
}

static void
gsword_resource_page_dispose(GObject *object)
{
    GswordResourcePage *page = GSWORD_RESOURCE_PAGE(object);

    g_clear_object(&page->module);
    gsword_page_dispose_popover(&page->image_context_menu);

    G_OBJECT_CLASS(gsword_resource_page_parent_class)->dispose(object);
}

static void
gsword_resource_page_finalize(GObject *object)
{
    GswordResourcePage *page = GSWORD_RESOURCE_PAGE(object);

    g_clear_pointer(&page->data_dir, g_free);

    G_OBJECT_CLASS(gsword_resource_page_parent_class)->finalize(object);
}

static void
gsword_resource_page_class_init(GswordResourcePageClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = gsword_resource_page_dispose;
    G_OBJECT_CLASS(klass)->finalize = gsword_resource_page_finalize;
}

static void
gsword_resource_page_init(GswordResourcePage *page)
{
    (void)page;
}

GswordResourcePage *
gsword_resource_page_new(GswordWindow *window)
{
    GswordResourcePage *page = g_object_new(gsword_resource_page_get_type(), NULL);
    GtkWidget *box;
    GtkWidget *header;
    GtkWidget *paned;
    GtkWidget *tree_scroll;
    GtkWidget *content_scroll;
    GtkWidget *back;
    GtkWidget *text_box;
    GtkWidget *view_widget;
    GtkWidget *zoom_out;
    GtkWidget *zoom_in;
    GtkWidget *fit_width;
    GtkWidget *menu_button;
    GtkWidget *menu_popover;
    GtkWidget *context_menu;
    GtkGesture *context_gesture;
    GtkEventController *key_controller;

    gsword_page_set_window(GSWORD_PAGE(page), window);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    tree_scroll = gtk_scrolled_window_new();
    content_scroll = gtk_scrolled_window_new();
    back = gtk_button_new_with_label(_("Back to reading"));
    text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_widget_set_margin_top(header, 8);
    gtk_widget_set_margin_bottom(header, 8);
    gtk_widget_set_margin_start(header, 12);
    gtk_widget_set_margin_end(header, 12);
    page->title = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->title, 0.0f);
    gtk_label_set_ellipsize(page->title, PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(GTK_WIDGET(page->title), "heading");
    gtk_widget_set_hexpand(GTK_WIDGET(page->title), TRUE);
    page->path = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(page->path, 0.0f);
    gtk_label_set_ellipsize(page->path, PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_max_width_chars(page->path, 32);
    gtk_widget_add_css_class(GTK_WIDGET(page->path), "dim-label");
    gtk_box_append(GTK_BOX(header), GTK_WIDGET(page->title));
    gtk_box_append(GTK_BOX(header), GTK_WIDGET(page->path));

    page->image_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_visible(page->image_controls, FALSE);
    zoom_out = gtk_button_new_from_icon_name("zoom-out-symbolic");
    zoom_in = gtk_button_new_from_icon_name("zoom-in-symbolic");
    fit_width = gtk_button_new_from_icon_name("zoom-fit-best-symbolic");
    page->zoom_out = GTK_BUTTON(zoom_out);
    page->zoom_in = GTK_BUTTON(zoom_in);
    gtk_widget_add_css_class(zoom_out, "flat");
    gtk_widget_add_css_class(zoom_in, "flat");
    gtk_widget_add_css_class(fit_width, "flat");
    set_icon_button_label(zoom_out, _("Zoom out"));
    set_icon_button_label(zoom_in, _("Zoom in"));
    set_icon_button_label(fit_width, _("Fit width"));
    g_signal_connect(zoom_out, "clicked", G_CALLBACK(on_zoom_out_clicked),
                     page);
    g_signal_connect(zoom_in, "clicked", G_CALLBACK(on_zoom_in_clicked),
                     page);
    g_signal_connect(fit_width, "clicked", G_CALLBACK(on_fit_width_clicked),
                     page);
    page->zoom_label = GTK_LABEL(gtk_label_new("100%"));
    gtk_label_set_xalign(page->zoom_label, 0.5f);
    gtk_widget_set_size_request(GTK_WIDGET(page->zoom_label), 52, -1);
    menu_button = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button),
                                  "view-more-symbolic");
    set_icon_button_label(menu_button, _("Image actions"));
    gtk_box_append(GTK_BOX(page->image_controls), zoom_out);
    gtk_box_append(GTK_BOX(page->image_controls), GTK_WIDGET(page->zoom_label));
    gtk_box_append(GTK_BOX(page->image_controls), zoom_in);
    gtk_box_append(GTK_BOX(page->image_controls), fit_width);
    gtk_box_append(GTK_BOX(page->image_controls), menu_button);
    gtk_box_append(GTK_BOX(header), page->image_controls);
    gtk_box_append(GTK_BOX(header), back);
    g_signal_connect(back, "clicked", G_CALLBACK(on_resource_back), page);

    page->tree = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(page->tree, "row-selected",
                     G_CALLBACK(on_resource_selected), page);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tree_scroll),
                                  GTK_WIDGET(page->tree));
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tree_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(tree_scroll, 220, -1);

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

    page->image_view = GSWORD_IMAGE_VIEW(gsword_image_view_new());
    view_widget = GTK_WIDGET(page->image_view);
    g_signal_connect_object(page->image_view, "zoom-changed",
                            G_CALLBACK(on_image_zoom_changed), page, 0);
    gtk_widget_set_hexpand(view_widget, TRUE);
    gtk_widget_set_vexpand(view_widget, TRUE);

    context_menu = build_image_popover(page);
    page->image_context_menu = GTK_POPOVER(context_menu);
    gtk_popover_set_has_arrow(page->image_context_menu, TRUE);
    gtk_widget_set_parent(context_menu, view_widget);
    g_signal_connect_object(view_widget, "destroy",
                            G_CALLBACK(on_image_view_destroy), page, 0);
    context_gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(context_gesture),
                                  GDK_BUTTON_SECONDARY);
    g_signal_connect_object(context_gesture, "pressed",
                            G_CALLBACK(on_image_context_clicked), page, 0);
    gtk_widget_add_controller(view_widget,
                              GTK_EVENT_CONTROLLER(context_gesture));
    key_controller = gtk_event_controller_key_new();
    g_signal_connect_object(key_controller, "key-pressed",
                            G_CALLBACK(on_image_key_pressed), page, 0);
    gtk_widget_add_controller(view_widget, key_controller);

    menu_popover = build_image_popover(page);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu_button), menu_popover);

    page->content = GTK_STACK(gtk_stack_new());
    gtk_stack_add_named(page->content, text_box, "text");
    gtk_stack_add_named(page->content, view_widget, "picture");
    gtk_widget_set_vexpand(GTK_WIDGET(page->content), TRUE);

    gtk_paned_set_start_child(GTK_PANED(paned), tree_scroll);
    gtk_paned_set_end_child(GTK_PANED(paned), GTK_WIDGET(page->content));
    gtk_paned_set_position(GTK_PANED(paned), 240);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(paned), TRUE);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_box_append(GTK_BOX(box), header);
    gtk_box_append(GTK_BOX(box), paned);
    gsword_page_set_widget(GSWORD_PAGE(page), box);
    return page;
}

GtkWidget *
gsword_resource_page_widget(GswordResourcePage *page)
{
    g_return_val_if_fail(GSWORD_IS_RESOURCE_PAGE(page), NULL);
    return gsword_page_get_widget(GSWORD_PAGE(page));
}
