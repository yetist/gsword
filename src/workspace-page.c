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
    gint columns;
    GtkToggleButton *column_buttons[4];
    GtkCheckButton *sync_scroll_toggle;
    GtkCheckButton *diff_toggle;
    GPtrArray *panes;
    GswordWorkspacePane *active_pane;
    gchar *reference;
    gboolean restoring;
    gboolean syncing;
    gboolean sync_scroll_enabled;
    gboolean highlight_diffs;
    gboolean syncing_scroll;
};

static void update_column_buttons(GswordWorkspacePage *page);

static void
update_column_buttons(GswordWorkspacePage *page)
{
    gint i;

    if (page == NULL)
        return;
    for (i = 0; i < 4; i++) {
        if (page->column_buttons[i] != NULL) {
            gboolean active = page->columns == i + 1;

            page->restoring++;
            gtk_toggle_button_set_active(page->column_buttons[i], active);
            page->restoring--;
        }
    }
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
gsword_workspace_page_set_active_pane(GswordWorkspacePage *page,
                                      GswordWorkspacePane *pane)
{
    if (page == NULL || pane == NULL || page->active_pane == pane)
        return;
    if (page->active_pane != NULL) {
        GtkWidget *old = gsword_workspace_pane_widget(page->active_pane);

        if (old != NULL)
            gtk_widget_remove_css_class(old, "active");
    }
    page->active_pane = pane;
    gtk_widget_add_css_class(gsword_workspace_pane_widget(pane), "active");
}

void
gsword_workspace_page_reflow(GswordWorkspacePage *page)
{
    guint columns;
    guint i;

    if (page == NULL || page->grid == NULL || page->panes == NULL)
        return;
    columns = page->columns > 0 ? (guint)page->columns : 2;
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
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ssi)"));
    for (i = 0; i < page->panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(page->panes, i);
        GswModule *module = gsword_workspace_pane_get_module(pane);
        const gchar *name = gsword_workspace_pane_get_module_name(pane);
        const gchar *key = module != NULL
                         ? gsw_module_get_key_text(module) : NULL;

        g_variant_builder_add(&builder, "(ssi)",
                              name != NULL ? name : "",
                              key != NULL ? key : "",
                              gsword_workspace_pane_get_follow_mode(pane));
    }
    layout = g_variant_ref_sink(g_variant_builder_end(&builder));
    text = g_variant_print(layout, FALSE);
    g_settings_set_string(settings, "workspace-layout", text);
    g_settings_set_int(settings, "workspace-columns", page->columns);
    g_settings_set_boolean(settings, "workspace-sync-scroll",
                           page->sync_scroll_enabled);
    g_settings_set_boolean(settings, "workspace-highlight-diffs",
                           page->highlight_diffs);
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
    if (page->active_pane == NULL)
        gsword_workspace_page_set_active_pane(page, pane);
    gsword_workspace_page_reflow(page);
    gsword_workspace_page_refresh_diff(page);
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
            if (page->active_pane == pane)
                page->active_pane = NULL;
            g_object_unref(root);
            break;
        }
    }
    if (page->active_pane == NULL && page->panes->len > 0)
        gsword_workspace_page_set_active_pane(page,
            g_ptr_array_index(page->panes, 0));
    gsword_workspace_page_reflow(page);
    gsword_workspace_page_refresh_diff(page);
    gsword_workspace_page_save(page);
}

void
gsword_workspace_page_refresh_diff(GswordWorkspacePage *page)
{
    GPtrArray *maps;
    GHashTable *verses;
    GHashTable *diff;
    GHashTableIter iter;
    gpointer key;
    guint i;

    if (page == NULL || page->panes == NULL)
        return;
    if (!page->highlight_diffs) {
        for (i = 0; i < page->panes->len; i++)
            gsword_workspace_pane_set_diff_verses(
                g_ptr_array_index(page->panes, i), NULL);
        return;
    }
    maps = g_ptr_array_new_with_free_func(
        (GDestroyNotify)g_hash_table_unref);
    verses = g_hash_table_new(g_direct_hash, g_direct_equal);
    diff = g_hash_table_new(g_direct_hash, g_direct_equal);
    for (i = 0; i < page->panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(page->panes, i);
        GHashTable *map = gsword_workspace_pane_dup_verse_texts(pane);

        g_ptr_array_add(maps, map);
        g_hash_table_iter_init(&iter, map);
        while (g_hash_table_iter_next(&iter, &key, NULL))
            g_hash_table_add(verses, key);
    }
    if (page->panes->len > 1) {
        g_hash_table_iter_init(&iter, verses);
        while (g_hash_table_iter_next(&iter, &key, NULL)) {
            const gchar *base = NULL;
            gboolean have_base = FALSE;
            gboolean differs = FALSE;

            for (i = 0; i < maps->len; i++) {
                GHashTable *map = g_ptr_array_index(maps, i);
                const gchar *text = g_hash_table_lookup(map, key);

                if (!have_base) {
                    base = text;
                    have_base = TRUE;
                } else if (g_strcmp0(base, text) != 0) {
                    differs = TRUE;
                    break;
                }
            }
            if (differs)
                g_hash_table_add(diff, key);
        }
    }
    for (i = 0; i < page->panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(page->panes, i);

        gsword_workspace_pane_set_diff_verses(pane, diff);
    }
    g_hash_table_unref(diff);
    g_hash_table_unref(verses);
    g_ptr_array_unref(maps);
}

void
gsword_workspace_page_scroll_to_reference(GswordWorkspacePage *page,
                                          GswordWorkspacePane *source)
{
    gboolean syncing;
    guint i;

    if (page == NULL || page->panes == NULL)
        return;
    syncing = page->syncing_scroll;
    page->syncing_scroll = TRUE;
    for (i = 0; i < page->panes->len; i++) {
        GswordWorkspacePane *pane = g_ptr_array_index(page->panes, i);

        if (pane == source || !gsword_workspace_pane_get_follow(pane))
            continue;
        gsword_workspace_pane_scroll_to_active(pane);
    }
    page->syncing_scroll = syncing;
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
    gsword_workspace_page_scroll_to_reference(page, source);
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

static void
clear_panes(GswordWorkspacePage *page)
{
    if (page == NULL)
        return;
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
    page->active_pane = NULL;
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

    clear_panes(page);
    gsword_workspace_page_add_pane(page, module_a, reference, TRUE, FALSE);
    gsword_workspace_page_add_pane(page, module_b, reference, TRUE, FALSE);

    page->restoring = TRUE;
    page->columns = 2;
    update_column_buttons(page);
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
        page->highlight_diffs = g_settings_get_boolean(
            settings, "workspace-highlight-diffs");
    }
    if (columns < 1 || columns > 4)
        columns = 2;
    page->restoring = TRUE;
    page->columns = columns;
    update_column_buttons(page);
    if (page->sync_scroll_toggle != NULL)
        gtk_check_button_set_active(page->sync_scroll_toggle,
                                    page->sync_scroll_enabled);
    if (page->diff_toggle != NULL)
        gtk_check_button_set_active(page->diff_toggle,
                                    page->highlight_diffs);

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

        gboolean new_format = TRUE;
        gint mode = 0;

        layout = g_variant_parse(G_VARIANT_TYPE("a(ssi)"), layout_text,
                                 NULL, NULL, NULL);
        if (layout == NULL) {
            layout = g_variant_parse(G_VARIANT_TYPE("a(ssb)"), layout_text,
                                     NULL, NULL, NULL);
            new_format = FALSE;
        }
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
            g_variant_get(layout, new_format ? "a(ssi)" : "a(ssb)", &iter);
            for (;;) {
                GVariant *item = g_variant_iter_next_value(iter);
                GswordWorkspacePane *pane;

                if (item == NULL)
                    break;
                g_variant_get_child(item, 0, "&s", &module);
                g_variant_get_child(item, 1, "&s", &key);
                if (new_format) {
                    g_variant_get_child(item, 2, "i", &mode);
                } else {
                    gboolean old_follow = TRUE;

                    g_variant_get_child(item, 2, "b", &old_follow);
                    mode = old_follow ? 0 : 1;
                }
                pane = gsword_workspace_page_add_pane(page, module, key,
                                                      mode == 0, FALSE);
                if (pane != NULL)
                    gsword_workspace_pane_set_follow_mode(pane, mode);
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
    (void)adjustment;
    page->syncing_scroll = TRUE;
    {
        gint verse = gsword_workspace_pane_get_top_verse(source);

        for (i = 0; i < page->panes->len; i++) {
            GswordWorkspacePane *pane = g_ptr_array_index(page->panes, i);

            if (pane == source)
                continue;
            gsword_workspace_pane_scroll_to_verse(pane, verse);
        }
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
clear_all_panes(GswordWorkspacePage *page)
{
    clear_panes(page);
    gsword_workspace_page_reflow(page);
    gsword_workspace_page_save(page);
    gsword_window_set_status(page->window, _("Comparisons cleared"));
}

static gboolean
clear_all_panes_idle(gpointer user_data)
{
    clear_all_panes(user_data);
    return G_SOURCE_REMOVE;
}

static void
on_clear_response(AdwAlertDialog *dialog, const gchar *response,
                  gpointer user_data)
{
    GswordWorkspacePage *page = user_data;

    (void)dialog;
    if (g_strcmp0(response, "clear") == 0)
        g_idle_add(clear_all_panes_idle, page);
}

static void
on_clear_clicked(GtkButton *button, gpointer user_data)
{
    GswordWorkspacePage *page = user_data;
    AdwDialog *dialog;

    (void)button;
    dialog = adw_alert_dialog_new(_("Clear comparisons?"), NULL);
    adw_alert_dialog_format_body(
        ADW_ALERT_DIALOG(dialog),
        _("All comparison panes will be removed. This cannot be undone."));
    adw_alert_dialog_add_responses(
        ADW_ALERT_DIALOG(dialog),
        "cancel", _("_Cancel"),
        "clear", _("_Clear"),
        NULL);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_set_response_appearance(
        ADW_ALERT_DIALOG(dialog), "clear", ADW_RESPONSE_DESTRUCTIVE);
    g_signal_connect(dialog, "response", G_CALLBACK(on_clear_response), page);
    adw_dialog_present(dialog, GTK_WIDGET(page->root));
}

static void
on_empty_two_clicked(GtkButton *button, gpointer user_data)
{
    GswordWorkspacePage *page = user_data;

    (void)button;
    gsword_workspace_page_apply_parallel_template(page);
}

static void
on_empty_four_clicked(GtkButton *button, gpointer user_data)
{
    GswordWorkspacePage *page = user_data;
    GSettings *settings = gsword_window_get_settings(page->window);
    gchar *saved_a = NULL;
    gchar *saved_b = NULL;
    gchar *modules[4] = { NULL, NULL, NULL, NULL };
    gchar *reference;
    guint i;

    (void)button;
    reference = gsword_window_current_start_reference(page->window);
    if (settings != NULL) {
        saved_a = g_settings_get_string(settings, "parallel-a");
        saved_b = g_settings_get_string(settings, "parallel-b");
    }
    modules[0] = parallel_module(page, saved_a, 0);
    modules[1] = parallel_module(page, saved_b, 1);
    if (modules[0] == NULL) {
        gsword_window_set_status(page->window,
                                 _("No Bible text module is installed"));
        g_free(saved_a);
        g_free(saved_b);
        g_free(reference);
        return;
    }
    for (i = 2; i < 4; i++)
        modules[i] = parallel_module(page, NULL, i);
    for (i = 2; i < 4; i++) {
        if (modules[i] == NULL)
            modules[i] = g_strdup(modules[0]);
    }

    clear_panes(page);
    for (i = 0; i < 4; i++)
        gsword_workspace_page_add_pane(page, modules[i], reference, TRUE,
                                       FALSE);
    page->restoring = TRUE;
    page->columns = 4;
    update_column_buttons(page);
    page->restoring = FALSE;
    set_sync_scroll(page, TRUE);
    if (settings != NULL) {
        g_settings_set_string(settings, "parallel-a", modules[0]);
        g_settings_set_string(settings, "parallel-b", modules[1]);
    }
    gsword_workspace_page_reflow(page);
    gsword_workspace_page_save(page);
    gsword_window_show_page(page->window, "panes");
    gsword_workspace_page_sync_from_reader(page);
    gsword_window_set_status(page->window, _("Four-way comparison ready"));

    g_free(saved_a);
    g_free(saved_b);
    g_free(reference);
    for (i = 0; i < 4; i++)
        g_free(modules[i]);
}

static void
on_empty_add_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    on_add_clicked(button, user_data);
}

static void
on_column_toggled(GtkToggleButton *button, gpointer user_data)
{
    GswordWorkspacePage *page = user_data;
    gint column = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(button), "column-count"));

    if (!gtk_toggle_button_get_active(button))
        return;
    page->columns = column;
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
on_diff_toggled(GtkCheckButton *button, gpointer user_data)
{
    GswordWorkspacePage *page = user_data;

    page->highlight_diffs = gtk_check_button_get_active(button);
    if (page->restoring)
        return;
    gsword_workspace_page_refresh_diff(page);
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
    GtkWidget *column_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *sync_scroll = gtk_check_button_new_with_label(_("Sync scroll"));
    GtkWidget *diff_toggle = gtk_check_button_new_with_label(
        _("Highlight differences"));
    GtkWidget *empty = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *empty_title = gtk_label_new(_("No comparisons yet"));
    GtkWidget *empty_text = gtk_label_new(
        _("Choose a template or add a comparison pane to begin."));
    GtkWidget *empty_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *two = gtk_button_new_with_label(_("Two translations"));
    GtkWidget *four = gtk_button_new_with_label(_("Four translations"));
    GtkWidget *add_pane = gtk_button_new_with_label(_("Add comparison"));
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
    page->columns = 2;
    gtk_widget_add_css_class(column_box, "linked");
    {
        gint i;

        for (i = 0; i < 4; i++) {
            gchar label[2] = { (gchar)('1' + i), '\0' };
            GtkWidget *button = gtk_toggle_button_new_with_label(label);

            g_object_set_data(G_OBJECT(button), "column-count",
                              GINT_TO_POINTER(i + 1));
            if (i > 0)
                gtk_toggle_button_set_group(
                    GTK_TOGGLE_BUTTON(button),
                    page->column_buttons[0]);
            page->column_buttons[i] = GTK_TOGGLE_BUTTON(button);
            g_signal_connect(button, "toggled",
                             G_CALLBACK(on_column_toggled), page);
            gtk_box_append(GTK_BOX(column_box), button);
        }
        page->restoring = TRUE;
        gtk_toggle_button_set_active(page->column_buttons[1], TRUE);
        page->restoring = FALSE;
    }
    page->sync_scroll_toggle = GTK_CHECK_BUTTON(sync_scroll);
    gtk_widget_set_tooltip_text(sync_scroll,
                                _("Keep comparison scroll positions together"));
    g_signal_connect(sync_scroll, "toggled",
                     G_CALLBACK(on_sync_scroll_toggled), page);
    page->diff_toggle = GTK_CHECK_BUTTON(diff_toggle);
    gtk_widget_set_tooltip_text(
        diff_toggle, _("Highlight verses whose text differs between panes"));
    g_signal_connect(diff_toggle, "toggled",
                     G_CALLBACK(on_diff_toggled), page);
    gtk_box_append(GTK_BOX(toolbar), add);
    gtk_box_append(GTK_BOX(toolbar), clear);
    gtk_box_append(GTK_BOX(toolbar), columns_label);
    gtk_box_append(GTK_BOX(toolbar), column_box);
    gtk_box_append(GTK_BOX(toolbar), sync_scroll);
    gtk_box_append(GTK_BOX(toolbar), diff_toggle);

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

    gtk_label_set_wrap(GTK_LABEL(empty_title), TRUE);
    gtk_widget_add_css_class(empty_title, "title-3");
    gtk_label_set_wrap(GTK_LABEL(empty_text), TRUE);
    gtk_label_set_justify(GTK_LABEL(empty_text), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(empty_text, "dim-label");
    gtk_widget_set_halign(empty_title, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(empty_text, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(empty_buttons, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(empty, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(empty, 32);
    gtk_widget_set_margin_bottom(empty, 32);
    gtk_widget_set_margin_start(empty, 24);
    gtk_widget_set_margin_end(empty, 24);
    g_signal_connect(two, "clicked",
                     G_CALLBACK(on_empty_two_clicked), page);
    g_signal_connect(four, "clicked",
                     G_CALLBACK(on_empty_four_clicked), page);
    g_signal_connect(add_pane, "clicked",
                     G_CALLBACK(on_empty_add_clicked), page);
    gtk_box_append(GTK_BOX(empty_buttons), two);
    gtk_box_append(GTK_BOX(empty_buttons), four);
    gtk_box_append(GTK_BOX(empty_buttons), add_pane);
    gtk_box_append(GTK_BOX(empty), empty_title);
    gtk_box_append(GTK_BOX(empty), empty_text);
    gtk_box_append(GTK_BOX(empty), empty_buttons);
    page->stack = GTK_STACK(gtk_stack_new());
    gtk_stack_add_named(page->stack, empty, "empty");
    gtk_stack_add_named(page->stack, scrolled, "grid");
    gtk_widget_set_vexpand(GTK_WIDGET(page->stack), TRUE);
    gtk_stack_set_visible_child_name(page->stack, "empty");

    gtk_box_append(GTK_BOX(box), toolbar);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(page->stack));
    g_object_set_data_full(G_OBJECT(box), "gsword-workspace-page", page,
                           workspace_page_free);
    return page;
}
