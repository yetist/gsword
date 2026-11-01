/*
 * module-sidebar.c: grouped module list and module selection for gsword
 */

#include "module-sidebar.h"
#include "window-private.h"

#include <glib/gi18n.h>

struct _GswordModuleSidebar {
    GswordWindow *window;
    GtkWidget *root;
    GtkListBox *list;
    gboolean selecting;
    gboolean startup_selecting;
};

static const gchar *
module_group_title(GswModuleType type)
{
    switch (type) {
    case GSW_MODULE_TYPE_BIBLES:       return _("Bibles");
    case GSW_MODULE_TYPE_COMMENTARIES: return _("Commentaries");
    case GSW_MODULE_TYPE_LEXDICTS:     return _("Lexicons / Dictionaries");
    case GSW_MODULE_TYPE_DAILY_DEVOS:  return _("Daily Devotions");
    case GSW_MODULE_TYPE_GENBOOKS:     return _("General Books");
    case GSW_MODULE_TYPE_MAPS:         return _("Maps");
    case GSW_MODULE_TYPE_OTHER:
    case GSW_MODULE_TYPE_UNKNOWN:
    default:                           return _("Other Modules");
    }
}

static GtkWidget *
make_module_group_row(const gchar *title)
{
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *label = gtk_label_new(title);

    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "heading");
    gtk_widget_set_margin_top(label, 10);
    gtk_widget_set_margin_bottom(label, 2);
    gtk_widget_set_margin_start(label, 10);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
    return row;
}

static GtkWidget *
make_module_row(GswModuleInfo *info)
{
    const gchar *name = gsw_module_info_get_name(info);
    const gchar *description = gsw_module_info_get_description(info);
    const gchar *type = gsword_window_module_type_display(info);
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *name_label = gtk_label_new(name);
    GtkWidget *description_label = gtk_label_new(
        description != NULL ? description : "");
    GtkWidget *type_label = gtk_label_new(type != NULL ? type : "");

    gtk_widget_add_css_class(box, "module-row");
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);

    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0f);
    gtk_widget_add_css_class(name_label, "heading");
    gtk_label_set_xalign(GTK_LABEL(description_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(description_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(description_label), 28);
    gtk_widget_add_css_class(description_label, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(type_label), 0.0f);
    gtk_widget_add_css_class(type_label, "caption");
    gtk_widget_add_css_class(type_label, "dim-label");

    gtk_box_append(GTK_BOX(box), name_label);
    if (description != NULL && *description != '\0')
        gtk_box_append(GTK_BOX(box), description_label);
    if (type != NULL && *type != '\0')
        gtk_box_append(GTK_BOX(box), type_label);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    g_object_set_data_full(G_OBJECT(row), "module-name", g_strdup(name),
                           g_free);
    g_object_set_data(G_OBJECT(row), "module-type",
                      GINT_TO_POINTER((gint)
                          gsword_window_module_effective_type(info)));
    return row;
}

static void
on_module_row_selected(GtkListBox *list, GtkListBoxRow *row,
                       gpointer user_data)
{
    GswordModuleSidebar *sidebar = user_data;
    const gchar *name;

    (void)list;
    if (row == NULL || sidebar->selecting)
        return;
    name = g_object_get_data(G_OBJECT(row), "module-name");
    if (name == NULL || sidebar->startup_selecting)
        return;

    {
        gint type = GPOINTER_TO_INT(
            g_object_get_data(G_OBJECT(row), "module-type"));

        if (type == GSW_MODULE_TYPE_COMMENTARIES) {
            gsword_window_open_commentary_module(sidebar->window, name);
            return;
        }
        if (type == GSW_MODULE_TYPE_LEXDICTS) {
            gsword_window_open_lexicon_module(sidebar->window, name);
            return;
        }
        if (type == GSW_MODULE_TYPE_DAILY_DEVOS) {
            gsword_window_open_devotion_module(sidebar->window, name);
            return;
        }
        if (type == GSW_MODULE_TYPE_GENBOOKS ||
            type == GSW_MODULE_TYPE_MAPS) {
            gsword_window_open_resource_module(
                sidebar->window, name, type == GSW_MODULE_TYPE_MAPS);
            return;
        }
    }

    gsword_window_load_module(sidebar->window, name);
    gsword_window_show_page(sidebar->window, "reader");
}

static gboolean
reset_startup_selecting(gpointer user_data)
{
    GswordModuleSidebar *sidebar = g_object_get_data(
        G_OBJECT(user_data), "gsword-module-sidebar");

    if (sidebar != NULL)
        sidebar->startup_selecting = FALSE;
    return G_SOURCE_REMOVE;
}

void
gsword_module_sidebar_select(GswordModuleSidebar *sidebar, const gchar *name)
{
    GtkWidget *child;

    if (sidebar == NULL || sidebar->list == NULL || name == NULL)
        return;
    child = gtk_widget_get_first_child(GTK_WIDGET(sidebar->list));
    sidebar->selecting = TRUE;
    while (child != NULL) {
        const gchar *row_name = g_object_get_data(G_OBJECT(child),
                                                  "module-name");

        if (row_name != NULL && g_strcmp0(row_name, name) == 0) {
            gtk_list_box_select_row(sidebar->list,
                                    GTK_LIST_BOX_ROW(child));
            break;
        }
        child = gtk_widget_get_next_sibling(child);
    }
    sidebar->selecting = FALSE;
}

void
gsword_module_sidebar_populate(GswordModuleSidebar *sidebar)
{
    static const GswModuleType group_types[] = {
        GSW_MODULE_TYPE_BIBLES,
        GSW_MODULE_TYPE_COMMENTARIES,
        GSW_MODULE_TYPE_LEXDICTS,
        GSW_MODULE_TYPE_DAILY_DEVOS,
        GSW_MODULE_TYPE_GENBOOKS,
        GSW_MODULE_TYPE_MAPS,
        GSW_MODULE_TYPE_OTHER,
    };
    GswordWindow *self;
    GswManager *manager;
    GSettings *settings;
    GPtrArray *infos;
    gchar *wanted = NULL;
    GtkListBoxRow *selected = NULL;
    GtkListBoxRow *first = NULL;
    guint group;

    if (sidebar == NULL)
        return;
    self = sidebar->window;
    manager = gsword_window_get_manager(self);
    settings = gsword_window_get_settings(self);
    if (manager == NULL) {
        gsword_window_show_page(self, "reader");
        gsword_window_refresh_reader(self);
        return;
    }

    infos = gsw_manager_get_module_infos(manager);
    if (settings != NULL)
        wanted = g_settings_get_string(settings, "last-module");

    while (gtk_widget_get_first_child(GTK_WIDGET(sidebar->list)) != NULL) {
        GtkWidget *child = gtk_widget_get_first_child(
            GTK_WIDGET(sidebar->list));

        gtk_list_box_remove(sidebar->list, child);
    }

    for (group = 0; group < G_N_ELEMENTS(group_types); group++) {
        gboolean header_added = FALSE;
        guint i;

        for (i = 0; i < infos->len; i++) {
            GswModuleInfo *info = g_ptr_array_index(infos, i);
            GswModuleType type = gsword_window_module_effective_type(info);
            GswModuleType effective_type = type == GSW_MODULE_TYPE_UNKNOWN
                                         ? GSW_MODULE_TYPE_OTHER : type;
            const gchar *name = gsw_module_info_get_name(info);
            GtkWidget *row;

            if (effective_type != group_types[group])
                continue;
            if (!header_added) {
                gtk_list_box_append(
                    sidebar->list,
                    make_module_group_row(
                        module_group_title(group_types[group])));
                header_added = TRUE;
            }
            row = make_module_row(info);
            gtk_list_box_append(sidebar->list, row);
            if (first == NULL)
                first = GTK_LIST_BOX_ROW(row);

            if (type == GSW_MODULE_TYPE_BIBLES && wanted != NULL &&
                g_strcmp0(wanted, name) == 0)
                selected = GTK_LIST_BOX_ROW(row);
        }
    }

    g_ptr_array_unref(infos);
    g_free(wanted);

    if (selected == NULL)
        selected = first;
    if (selected != NULL) {
        const gchar *selected_name = g_object_get_data(G_OBJECT(selected),
                                                       "module-name");

        sidebar->startup_selecting = TRUE;
        gtk_list_box_select_row(sidebar->list, selected);
        if (selected_name != NULL)
            gsword_window_load_module(self, selected_name);
        gsword_window_show_page(self, "reader");
        g_idle_add_full(G_PRIORITY_DEFAULT, reset_startup_selecting,
                        g_object_ref(self), g_object_unref);
    } else {
        gsword_window_show_page(self, "reader");
        gsword_window_refresh_reader(self);
    }
}

GtkWidget *
gsword_module_sidebar_widget(GswordModuleSidebar *sidebar)
{
    g_return_val_if_fail(sidebar != NULL, NULL);
    return sidebar->root;
}

GswordModuleSidebar *
gsword_module_sidebar_new(GswordWindow *window)
{
    GswordModuleSidebar *sidebar = g_new0(GswordModuleSidebar, 1);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *label = gtk_label_new(_("Modules"));
    GtkWidget *scrolled = gtk_scrolled_window_new();

    sidebar->window = window;
    sidebar->root = box;
    g_object_set_data(G_OBJECT(window), "gsword-module-sidebar", sidebar);

    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "heading");
    gtk_widget_set_margin_top(label, 10);
    gtk_widget_set_margin_start(label, 12);
    gtk_widget_set_margin_end(label, 12);

    sidebar->list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_widget_add_css_class(GTK_WIDGET(sidebar->list),
                             "navigation-sidebar");
    gtk_list_box_set_selection_mode(sidebar->list, GTK_SELECTION_SINGLE);
    g_signal_connect(sidebar->list, "row-selected",
                     G_CALLBACK(on_module_row_selected), sidebar);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(sidebar->list));
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), scrolled);
    return sidebar;
}
