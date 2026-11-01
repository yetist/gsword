/*
 * workspace-page.c: comparison workspace container for the gsword window
 */

#include "workspace-page.h"
#include "workspace-pane.h"
#include "window-private.h"

#include <glib/gi18n.h>

struct _GswordWorkspacePage {
    GswordWindow *window;
    GtkWidget *root;
    GtkStack *stack;
    GtkGrid *grid;
    GtkSpinButton *columns;
    GtkCheckButton *sync_scroll_toggle;
    GPtrArray *panes;
    gchar *reference;
    gboolean restoring;
    gboolean syncing;
    gboolean sync_scroll_enabled;
    gboolean syncing_scroll;
};

static void
sync_adjustment(GtkAdjustment *from, GtkAdjustment *to)
{
    gdouble from_span = gtk_adjustment_get_upper(from) -
                        gtk_adjustment_get_page_size(from);
    gdouble to_span = gtk_adjustment_get_upper(to) -
                      gtk_adjustment_get_page_size(to);
    gdouble fraction = from_span > 0
                     ? gtk_adjustment_get_value(from) / from_span : 0.0;

    gtk_adjustment_set_value(to, fraction * MAX(to_span, 0.0));
}

static gchar *
default_module(GswordWorkspacePage *page)
{
    GswordWindow *self = page->window;
    GswManager *manager = gsword_window_get_manager(self);
    GswModule *module = gsword_window_get_module(self);
    GPtrArray *infos;
    gchar *fallback = NULL;
    gchar *result = NULL;
    guint i;

    if (module != NULL) {
        GswModuleInfo *info = gsw_module_get_info(module);

        if (info != NULL && gsword_workspace_pane_supports_module(info))
            return g_strdup(gsw_module_get_name(module));
    }
    if (manager == NULL)
        return NULL;
    infos = gsw_manager_get_module_infos(manager);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (!gsword_workspace_pane_supports_module(info))
            continue;
        if (fallback == NULL)
            fallback = g_strdup(gsw_module_info_get_name(info));
        if (gsword_window_module_effective_type(info) ==
            GSW_MODULE_TYPE_BIBLES) {
            result = g_strdup(gsw_module_info_get_name(info));
            break;
        }
    }
    if (result == NULL)
        result = fallback;
    else
        g_free(fallback);
    g_ptr_array_unref(infos);
    return result;
}

void
gsword_workspace_page_reflow(GswordWorkspacePage *page)
{
    guint columns;
    guint i;

    if (page == NULL || page->grid == NULL || page->panes == NULL)
        return;
    columns = page->columns != NULL
            ? (guint)gtk_spin_button_get_value_as_int(page->columns)
            : 2;
    if (columns < 1)
        columns = 1;

    for (i = 0; i < page->panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(page->panes, i);
        GtkWidget *root = gsword_workspace_pane_widget(pane);

        g_object_ref(root);
        if (gtk_widget_get_parent(root) == GTK_WIDGET(page->grid))
            gtk_grid_remove(page->grid, root);
    }
    for (i = 0; i < page->panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(page->panes, i);
        GtkWidget *root = gsword_workspace_pane_widget(pane);

        gtk_grid_attach(page->grid, root,
                        (gint)(i % columns), (gint)(i / columns), 1, 1);
        g_object_unref(root);
    }
    gtk_grid_set_column_homogeneous(page->grid, TRUE);
    gtk_grid_set_row_homogeneous(page->grid, TRUE);
    if (page->stack != NULL) {
        gtk_stack_set_visible_child_name(
            page->stack, page->panes->len > 0 ? "grid" : "empty");
    }
}

void
gsword_workspace_page_save(GswordWorkspacePage *page)
{
    GSettings *settings;
    GVariantBuilder builder;
    GVariant *layout;
    gchar *text;
    guint i;

    if (page == NULL)
        return;
    settings = gsword_window_get_settings(page->window);
    if (settings == NULL || page->panes == NULL || page->restoring)
        return;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ssb)"));
    for (i = 0; i < page->panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(page->panes, i);
        GswModule *module = gsword_workspace_pane_get_module(pane);
        const gchar *name = gsword_workspace_pane_get_module_name(pane);
        const gchar *key = module != NULL
                         ? gsw_module_get_key_text(module) : NULL;

        g_variant_builder_add(&builder, "(ssb)",
                              name != NULL ? name : "",
                              key != NULL ? key : "",
                              gsword_workspace_pane_get_follow(pane));
    }
    layout = g_variant_ref_sink(g_variant_builder_end(&builder));
    text = g_variant_print(layout, FALSE);
    g_settings_set_string(settings, "workspace-layout", text);
    if (page->columns != NULL)
        g_settings_set_int(settings, "workspace-columns",
                           gtk_spin_button_get_value_as_int(page->columns));
    g_settings_set_boolean(settings, "workspace-sync-scroll",
                           page->sync_scroll_enabled);
    g_free(text);
    g_variant_unref(layout);
}

GswordWorkspacePane *
gsword_workspace_page_add_pane(GswordWorkspacePage *page,
                               const gchar *module_name, const gchar *key,
                               gboolean follow, gboolean save)
{
    GswordWorkspacePane *pane;

    if (page == NULL)
        return NULL;
    pane = gsword_workspace_pane_new(page->window, page, module_name, key,
                                     follow, FALSE);
    g_ptr_array_add(page->panes, pane);
    gsword_workspace_page_reflow(page);
    if (save)
        gsword_workspace_page_save(page);
    return pane;
}

void
gsword_workspace_page_remove_pane(GswordWorkspacePage *page,
                                  GswordWorkspacePane *pane)
{
    guint i;

    if (page == NULL || pane == NULL)
        return;
    for (i = 0; i < page->panes->len; i++) {
        if (g_ptr_array_index(page->panes, i) == pane) {
            GtkWidget *root = gsword_workspace_pane_widget(pane);

            g_object_ref(root);
            if (gtk_widget_get_parent(root) == GTK_WIDGET(page->grid))
                gtk_grid_remove(page->grid, root);
            g_ptr_array_remove_index(page->panes, i);
            g_object_unref(root);
            break;
        }
    }
    gsword_workspace_page_reflow(page);
    gsword_workspace_page_save(page);
}

void
gsword_workspace_page_sync_reference(GswordWorkspacePage *page,
                                     const gchar *reference,
                                     GswordWorkspacePane *source)
{
    guint i;

    if (page == NULL || reference == NULL || *reference == '\0' ||
        page->syncing)
        return;
    page->syncing = TRUE;
    g_free(page->reference);
    page->reference = g_strdup(reference);
    for (i = 0; i < page->panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(page->panes, i);
        GswModule *module;

        if (pane == source)
            continue;
        if (!gsword_workspace_pane_get_follow(pane))
            continue;
        module = gsword_workspace_pane_get_module(pane);
        if (module == NULL ||
            gsw_key_get_kind(gsw_module_get_key(module)) !=
                GSW_KEY_KIND_VERSE)
            continue;
        gsword_workspace_pane_set_reference(pane, reference);
    }
    page->syncing = FALSE;
}

const gchar *
gsword_workspace_page_get_reference(GswordWorkspacePage *page)
{
    return page != NULL ? page->reference : NULL;
}

void
gsword_workspace_page_sync_from_reader(GswordWorkspacePage *page)
{
    gchar *reference;

    if (page == NULL || page->panes == NULL || page->panes->len == 0 ||
        gsword_window_get_module(page->window) == NULL)
        return;
    reference = gsword_window_current_start_reference(page->window);
    if (reference != NULL) {
        gsword_workspace_page_sync_reference(page, reference, NULL);
        g_free(reference);
    }
}

static gboolean
is_bible_module(GswordWorkspacePage *page, const gchar *name)
{
    GswManager *manager = gsword_window_get_manager(page->window);
    GPtrArray *infos;
    gboolean result = FALSE;
    guint i;

    if (name == NULL || manager == NULL)
        return FALSE;
    infos = gsw_manager_get_module_infos(manager);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (g_strcmp0(gsw_module_info_get_name(info), name) == 0) {
            result = gsword_window_module_effective_type(info) ==
                     GSW_MODULE_TYPE_BIBLES &&
                     gsword_workspace_pane_supports_module(info);
            break;
        }
    }
    g_ptr_array_unref(infos);
    return result;
}

static gchar *
parallel_module(GswordWorkspacePage *page, const gchar *saved, guint index)
{
    GswManager *manager = gsword_window_get_manager(page->window);
    GPtrArray *infos;
    gchar *result = NULL;
    guint position = 0;
    guint i;

    if (saved != NULL && is_bible_module(page, saved))
        return g_strdup(saved);
    if (manager == NULL)
        return NULL;
    infos = gsw_manager_get_module_infos(manager);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (gsword_window_module_effective_type(info) !=
                GSW_MODULE_TYPE_BIBLES ||
            !gsword_workspace_pane_supports_module(info))
            continue;
        if (position == index) {
            result = g_strdup(gsw_module_info_get_name(info));
            break;
        }
        position++;
    }
    g_ptr_array_unref(infos);
    return result;
}

static void
set_sync_scroll(GswordWorkspacePage *page, gboolean enabled)
{
    page->sync_scroll_enabled = enabled;
    if (page->sync_scroll_toggle != NULL) {
        gboolean restoring = page->restoring;

        page->restoring = TRUE;
        gtk_check_button_set_active(page->sync_scroll_toggle, enabled);
        page->restoring = restoring;
    }
}

void
gsword_workspace_page_apply_parallel_template(GswordWorkspacePage *page)
{
    GSettings *settings;
    gchar *saved_a = NULL;
    gchar *saved_b = NULL;
    gchar *module_a;
    gchar *module_b;
    gchar *reference;

    if (page == NULL)
        return;
    settings = gsword_window_get_settings(page->window);
    reference = gsword_window_current_start_reference(page->window);
    if (settings != NULL) {
        saved_a = g_settings_get_string(settings, "parallel-a");
        saved_b = g_settings_get_string(settings, "parallel-b");
    }
    module_a = parallel_module(page, saved_a, 0);
    module_b = parallel_module(page, saved_b, 1);
    if (module_a == NULL) {
        gsword_window_set_status(page->window,
                                 _("No Bible text module is installed"));
        g_free(saved_a);
        g_free(saved_b);
        g_free(module_b);
        g_free(reference);
        return;
    }
    if (module_b == NULL)
        module_b = g_strdup(module_a);

    while (page->panes->len > 0) {
        GswordWorkspacePane *pane = g_ptr_array_index(
            page->panes, page->panes->len - 1);
        GtkWidget *root = gsword_workspace_pane_widget(pane);

        g_object_ref(root);
        if (gtk_widget_get_parent(root) == GTK_WIDGET(page->grid))
            gtk_grid_remove(page->grid, root);
        g_ptr_array_remove_index(page->panes, page->panes->len - 1);
        g_object_unref(root);
    }
    gsword_workspace_page_add_pane(page, module_a, reference, TRUE, FALSE);
    gsword_workspace_page_add_pane(page, module_b, reference, TRUE, FALSE);

    page->restoring = TRUE;
    if (page->columns != NULL)
        gtk_spin_button_set_value(page->columns, 2);
    page->restoring = FALSE;
    set_sync_scroll(page, TRUE);
    if (settings != NULL) {
        g_settings_set_string(settings, "parallel-a", module_a);
        g_settings_set_string(settings, "parallel-b", module_b);
    }
    gsword_workspace_page_reflow(page);
    gsword_workspace_page_save(page);
    gsword_window_show_page(page->window, "panes");
    gsword_workspace_page_sync_from_reader(page);
    gsword_window_set_status(page->window, _("Parallel comparison ready"));

    g_free(saved_a);
    g_free(saved_b);
    g_free(module_a);
    g_free(module_b);
    g_free(reference);
}

void
gsword_workspace_page_restore(GswordWorkspacePage *page)
{
    GSettings *settings;
    gchar *layout_text = NULL;
    gint columns = 2;
    gboolean created_default = FALSE;

    if (page == NULL || page->grid == NULL)
        return;
    settings = gsword_window_get_settings(page->window);
    if (settings != NULL) {
        layout_text = g_settings_get_string(settings, "workspace-layout");
        columns = g_settings_get_int(settings, "workspace-columns");
        page->sync_scroll_enabled = g_settings_get_boolean(
            settings, "workspace-sync-scroll");
    }
    if (columns < 1 || columns > 4)
        columns = 2;
    page->restoring = TRUE;
    if (page->columns != NULL)
        gtk_spin_button_set_value(page->columns, columns);
    if (page->sync_scroll_toggle != NULL)
        gtk_check_button_set_active(page->sync_scroll_toggle,
                                    page->sync_scroll_enabled);

    if (layout_text == NULL || *layout_text == '\0') {
        gchar *module = default_module(page);
        gchar *reference =
            gsword_window_current_start_reference(page->window);

        gsword_workspace_page_add_pane(page, module, reference, TRUE, FALSE);
        g_free(module);
        g_free(reference);
        created_default = TRUE;
    } else {
        GVariant *layout;
        GVariantIter *iter;
        const gchar *module;
        const gchar *key;
        gboolean follow;

        layout = g_variant_parse(G_VARIANT_TYPE("a(ssb)"), layout_text,
                                 NULL, NULL, NULL);
        if (layout == NULL) {
            gchar *module = default_module(page);
            gchar *reference =
                gsword_window_current_start_reference(page->window);

            gsword_workspace_page_add_pane(page, module, reference, TRUE,
                                           FALSE);
            g_free(module);
            g_free(reference);
            created_default = TRUE;
        } else {
            g_variant_get(layout, "a(ssb)", &iter);
            for (;;) {
                GVariant *item = g_variant_iter_next_value(iter);

                if (item == NULL)
                    break;
                g_variant_get_child(item, 0, "&s", &module);
                g_variant_get_child(item, 1, "&s", &key);
                g_variant_get_child(item, 2, "b", &follow);
                gsword_workspace_page_add_pane(page, module, key, follow,
                                               FALSE);
                g_variant_unref(item);
            }
            g_variant_iter_free(iter);
            g_variant_unref(layout);
        }
    }
    page->restoring = FALSE;
    gsword_workspace_page_reflow(page);
    if (created_default)
        gsword_workspace_page_save(page);
    g_free(layout_text);
}

void
gsword_workspace_page_sync_scroll(GswordWorkspacePage *page,
                                  GswordWorkspacePane *source,
                                  GtkAdjustment *adjustment)
{
    guint i;

    if (page == NULL || page->syncing_scroll ||
        !page->sync_scroll_enabled)
        return;
    page->syncing_scroll = TRUE;
    for (i = 0; i < page->panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(page->panes, i);
        GtkScrolledWindow *scroll;
        GtkAdjustment *target;

        if (pane == source)
            continue;
        scroll = gsword_workspace_pane_get_scroll(pane);
        if (scroll == NULL)
            continue;
        target = gtk_scrolled_window_get_vadjustment(scroll);
        if (target != NULL)
            sync_adjustment(adjustment, target);
    }
    page->syncing_scroll = FALSE;
}

void
gsword_workspace_page_reorder_from_drag(GswordWorkspacePage *page,
                                        GswordWorkspacePane *source,
                                        gdouble dx, gdouble dy)
{
    GtkWidget *source_root;
    graphene_rect_t source_bounds;
    gfloat px;
    gfloat py;
    guint source_index = G_MAXUINT;
    guint target_index = G_MAXUINT;
    guint i;

    if (page == NULL || source == NULL)
        return;
    source_root = gsword_workspace_pane_widget(source);
    if (!gtk_widget_compute_bounds(source_root, GTK_WIDGET(page->grid),
                                   &source_bounds))
        return;
    px = source_bounds.origin.x + source_bounds.size.width / 2.0f + (gfloat)dx;
    py = source_bounds.origin.y + source_bounds.size.height / 2.0f + (gfloat)dy;

    for (i = 0; i < page->panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(page->panes, i);
        GtkWidget *root;
        graphene_rect_t bounds;

        if (pane == source) {
            source_index = i;
            continue;
        }
        root = gsword_workspace_pane_widget(pane);
        if (!gtk_widget_compute_bounds(root, GTK_WIDGET(page->grid), &bounds))
            continue;
        if (graphene_rect_contains_point(&bounds,
                                         &GRAPHENE_POINT_INIT(px, py)))
            target_index = i;
    }
    if (source_index == G_MAXUINT || target_index == G_MAXUINT)
        return;
    {
        gpointer tmp = g_ptr_array_index(page->panes, source_index);

        g_ptr_array_index(page->panes, source_index) =
            g_ptr_array_index(page->panes, target_index);
        g_ptr_array_index(page->panes, target_index) = tmp;
    }
    gsword_workspace_page_reflow(page);
    gsword_workspace_page_save(page);
    gsword_window_set_status(page->window, _("Comparison order changed"));
}

static void
on_add_clicked(GtkButton *button, gpointer user_data)
{
    GswordWorkspacePage *page = user_data;
    gchar *name = default_module(page);
    gchar *reference = gsword_window_current_start_reference(page->window);

    (void)button;
    gsword_workspace_page_add_pane(page, name, reference, TRUE, TRUE);
    g_free(name);
    g_free(reference);
}

static void
on_clear_clicked(GtkButton *button, gpointer user_data)
{
    GswordWorkspacePage *page = user_data;

    (void)button;
    while (page->panes->len > 0) {
        GswordWorkspacePane *pane = g_ptr_array_index(
            page->panes, page->panes->len - 1);
        GtkWidget *root = gsword_workspace_pane_widget(pane);

        g_object_ref(root);
        if (gtk_widget_get_parent(root) == GTK_WIDGET(page->grid))
            gtk_grid_remove(page->grid, root);
        g_ptr_array_remove_index(page->panes, page->panes->len - 1);
        g_object_unref(root);
    }
    gsword_workspace_page_reflow(page);
    gsword_workspace_page_save(page);
}

static void
on_columns_changed(GtkSpinButton *spin, gpointer user_data)
{
    GswordWorkspacePage *page = user_data;

    (void)spin;
    if (page->restoring)
        return;
    gsword_workspace_page_reflow(page);
    gsword_workspace_page_save(page);
}

static void
on_sync_scroll_toggled(GtkCheckButton *button, gpointer user_data)
{
    GswordWorkspacePage *page = user_data;

    page->sync_scroll_enabled = gtk_check_button_get_active(button);
    if (!page->restoring)
        gsword_workspace_page_save(page);
}

static void
workspace_page_free(gpointer data)
{
    GswordWorkspacePage *page = data;

    g_clear_pointer(&page->panes, g_ptr_array_unref);
    g_free(page->reference);
    g_free(page);
}

GtkWidget *
gsword_workspace_page_widget(GswordWorkspacePage *page)
{
    g_return_val_if_fail(page != NULL, NULL);
    return page->root;
}

GswordWorkspacePage *
gsword_workspace_page_new(GswordWindow *window)
{
    GswordWorkspacePage *page = g_new0(GswordWorkspacePage, 1);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *add = gtk_button_new_with_label(_("Add comparison"));
    GtkWidget *clear = gtk_button_new_with_label(_("Clear comparisons"));
    GtkWidget *columns_label = gtk_label_new(_("Columns"));
    GtkWidget *sync_scroll = gtk_check_button_new_with_label(_("Sync scroll"));
    GtkWidget *hint = gtk_label_new(_("No comparisons yet. Add a comparison to begin."));
    GtkWidget *scrolled = gtk_scrolled_window_new();

    page->window = window;
    page->root = box;
    page->panes = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsword_workspace_pane_free);

    gtk_widget_set_margin_top(toolbar, 8);
    gtk_widget_set_margin_bottom(toolbar, 8);
    gtk_widget_set_margin_start(toolbar, 12);
    gtk_widget_set_margin_end(toolbar, 12);
    g_signal_connect(add, "clicked", G_CALLBACK(on_add_clicked), page);
    g_signal_connect(clear, "clicked", G_CALLBACK(on_clear_clicked), page);
    page->columns = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(1, 4, 1));
    gtk_spin_button_set_value(page->columns, 2);
    g_signal_connect(page->columns, "value-changed",
                     G_CALLBACK(on_columns_changed), page);
    page->sync_scroll_toggle = GTK_CHECK_BUTTON(sync_scroll);
    gtk_widget_set_tooltip_text(sync_scroll,
                                _("Keep comparison scroll positions together"));
    g_signal_connect(sync_scroll, "toggled",
                     G_CALLBACK(on_sync_scroll_toggled), page);
    gtk_box_append(GTK_BOX(toolbar), add);
    gtk_box_append(GTK_BOX(toolbar), clear);
    gtk_box_append(GTK_BOX(toolbar), columns_label);
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(page->columns));
    gtk_box_append(GTK_BOX(toolbar), sync_scroll);

    page->grid = GTK_GRID(gtk_grid_new());
    gtk_widget_set_hexpand(GTK_WIDGET(page->grid), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(page->grid), TRUE);
    gtk_grid_set_column_spacing(page->grid, 4);
    gtk_grid_set_row_spacing(page->grid, 4);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(page->grid));
    gtk_widget_set_vexpand(scrolled, TRUE);

    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_justify(GTK_LABEL(hint), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(hint, "dim-label");
    page->stack = GTK_STACK(gtk_stack_new());
    gtk_stack_add_named(page->stack, hint, "empty");
    gtk_stack_add_named(page->stack, scrolled, "grid");
    gtk_widget_set_vexpand(GTK_WIDGET(page->stack), TRUE);
    gtk_stack_set_visible_child_name(page->stack, "empty");

    gtk_box_append(GTK_BOX(box), toolbar);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(page->stack));
    g_object_set_data_full(G_OBJECT(box), "gsword-workspace-page", page,
                           workspace_page_free);
    return page;
}
