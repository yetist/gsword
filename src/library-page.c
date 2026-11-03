/*
 * library-page.c: module library and install UI for the gsword window
 */

#include "library-page.h"
#include "window-private.h"
#include "app-paths.h"

#include <glib/gi18n.h>

struct _GswordLibraryPage {
    GswordWindow *window;
    GtkWidget *root;
    GswManager *manager;
    GSettings *settings;
    GswInstallManager *installer;
    GswStatusReporter *install_reporter;
    GPtrArray *install_sources;
    GtkStringList *source_names;
    GtkDropDown *source_dropdown;
    GtkWidget *source_row;
    GtkWidget *filter_row;
    GtkStack *list_stack;
    GtkListBox *installed_list;
    GtkWidget *local_box;
    gint library_view;
    GtkEntry *install_search;
    GtkDropDown *type_filter;
    GtkDropDown *language_filter;
    GtkDropDown *status_filter;
    GtkButton *clear_filters_button;
    GtkLabel *filter_count;
    GPtrArray *language_codes;
    GtkStringList *language_names;
    GPtrArray *remote_infos;
    gboolean updating_filters;
    GtkListBox *remote_list;
    GtkLabel *remote_title;
    GtkLabel *remote_description;
    GtkLabel *remote_status;
    GtkLabel *remote_meta;
    GtkLabel *remote_warning;
    GtkExpander *technical_expander;
    GtkLabel *technical_label;
    GtkExpander *license_expander;
    GtkLabel *license_label;
    GtkExpander *about_expander;
    GtkLabel *about_label;
    GtkProgressBar *install_progress;
    GtkLabel *install_status;
    GtkButton *install_button;
    GtkButton *remove_button;
    GtkButton *cancel_button;
    GtkButton *refresh_source_button;
    GtkButton *refresh_sources_button;
    GtkToggleButton *installed_view_button;
    GtkToggleButton *available_view_button;
    GtkToggleButton *updates_view_button;
    GtkButton *update_all_button;
    GPtrArray *update_queue;
    GswInstallSource *update_source;
    gboolean updating_all;
    GCancellable *install_cancellable;
    GtkEntry *local_dir_entry;
    GtkEntry *local_name_entry;
    GtkButton *local_install_button;
    gchar *selected_remote_name;
    gchar *selected_installed_name;
};

typedef struct {
    GswordLibraryPage *page;
    GtkEntry *caption_entry;
    GtkEntry *host_entry;
    GtkEntry *directory_entry;
    AdwDialog *dialog;
} SourceDialogData;

static GswordLibraryPage *
library_page_get(GswordWindow *self)
{
    return g_object_get_data(G_OBJECT(self), "gsword-library-page");
}

static void gsword_window_refresh_remote_modules(GswordWindow *self);
static void gsword_window_refresh_installed_list(GswordWindow *self);
static void on_refresh_sources_finished(GObject *source, GAsyncResult *result,
                                        gpointer user_data);
static void on_refresh_remote_finished(GObject *source, GAsyncResult *result,
                                       gpointer user_data);
static void on_install_finished(GObject *source, GAsyncResult *result,
                                gpointer user_data);
static void on_remove_finished(GObject *source, GAsyncResult *result,
                               gpointer user_data);

static GswInstallSource *
gsword_window_current_install_source(GswordWindow *self)
{
    GswordLibraryPage *priv = library_page_get(self);
    guint selected = priv->source_dropdown != NULL
                   ? gtk_drop_down_get_selected(priv->source_dropdown)
                   : GTK_INVALID_LIST_POSITION;

    if (priv->install_sources == NULL ||
        selected == GTK_INVALID_LIST_POSITION ||
        selected >= priv->install_sources->len)
        return NULL;
    return g_ptr_array_index(priv->install_sources, selected);
}

static const gchar *module_status_text(guint status);

static const gchar *
language_name(const gchar *code)
{
    if (code == NULL || *code == '\0')
        return _("Unknown");
    if (g_str_has_prefix(code, "zh"))
        return _("Chinese");
    if (g_str_has_prefix(code, "en"))
        return _("English");
    if (g_str_has_prefix(code, "de"))
        return _("German");
    if (g_str_has_prefix(code, "fr"))
        return _("French");
    if (g_str_has_prefix(code, "es"))
        return _("Spanish");
    if (g_str_has_prefix(code, "pt"))
        return _("Portuguese");
    if (g_str_has_prefix(code, "ru"))
        return _("Russian");
    if (g_str_has_prefix(code, "ko"))
        return _("Korean");
    if (g_str_has_prefix(code, "ja"))
        return _("Japanese");
    if (g_str_has_prefix(code, "pl"))
        return _("Polish");
    if (g_str_has_prefix(code, "uk"))
        return _("Ukrainian");
    if (g_str_has_prefix(code, "ar"))
        return _("Arabic");
    return code;
}

static gchar *
language_display_name(const gchar *code)
{
    const gchar *name = language_name(code);

    if (g_strcmp0(name, code) == 0)
        return g_strdup(code != NULL ? code : _("Unknown"));
    return g_strdup_printf("%s (%s)", name, code);
}

static GswModuleType
filter_type_for_info(GswModuleInfo *info)
{
    GswModuleType type;

    if (gsword_window_module_info_is_devotional(info))
        return GSW_MODULE_TYPE_DAILY_DEVOS;
    if (gsword_window_module_info_is_map(info))
        return GSW_MODULE_TYPE_MAPS;
    type = gsw_module_info_get_module_type(info);
    return type == GSW_MODULE_TYPE_UNKNOWN ? GSW_MODULE_TYPE_OTHER : type;
}

static GswModuleType
filter_type_from_index(guint index)
{
    switch (index) {
    case 1: return GSW_MODULE_TYPE_BIBLES;
    case 2: return GSW_MODULE_TYPE_COMMENTARIES;
    case 3: return GSW_MODULE_TYPE_LEXDICTS;
    case 4: return GSW_MODULE_TYPE_DAILY_DEVOS;
    case 5: return GSW_MODULE_TYPE_GENBOOKS;
    case 6: return GSW_MODULE_TYPE_MAPS;
    case 7: return GSW_MODULE_TYPE_OTHER;
    case 0:
    default: return GSW_MODULE_TYPE_UNKNOWN;
    }
}

static gboolean
status_matches(guint status, guint filter)
{
    switch (filter) {
    case 1: return (status & GSW_MODULE_STATUS_NEW) != 0;
    case 2: return (status & GSW_MODULE_STATUS_UPDATED) != 0;
    case 3: return (status & GSW_MODULE_STATUS_NEW) == 0;
    case 4: return (status & GSW_MODULE_STATUS_OLDER) != 0;
    case 5: return (status & GSW_MODULE_STATUS_SAME_VERSION) != 0;
    case 0:
    default: return TRUE;
    }
}

static const gchar *
selected_language_code(GswordLibraryPage *page)
{
    guint selected;

    if (page->language_filter == NULL ||
        page->language_codes == NULL ||
        page->language_codes->len == 0)
        return NULL;
    selected = gtk_drop_down_get_selected(page->language_filter);
    if (selected == GTK_INVALID_LIST_POSITION ||
        selected >= page->language_codes->len)
        return NULL;
    return g_ptr_array_index(page->language_codes, selected);
}

static void apply_filters(GswordWindow *self);
static void clear_remote_details(GswordWindow *self);

static void
rebuild_language_filter(GswordWindow *self, GPtrArray *infos)
{
    GswordLibraryPage *priv = library_page_get(self);
    GtkStringList *names;
    GPtrArray *codes;
    const gchar *current;
    gchar *wanted = NULL;
    guint i;
    guint selected = 0;
    gboolean found = FALSE;

    current = selected_language_code(priv);
    if (current != NULL && *current != '\0')
        wanted = g_strdup(current);
    codes = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(codes, g_strdup(""));
    names = gtk_string_list_new(NULL);
    gtk_string_list_append(names, _("All languages"));
    for (i = 0; infos != NULL && i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);
        const gchar *language = gsw_module_info_get_language(info);
        guint j;
        gboolean exists = FALSE;

        if (language == NULL || *language == '\0')
            continue;
        for (j = 0; j < codes->len; j++) {
            if (g_strcmp0(g_ptr_array_index(codes, j), language) == 0) {
                exists = TRUE;
                break;
            }
        }
        if (exists)
            continue;
        g_ptr_array_add(codes, g_strdup(language));
    }
    for (i = 1; i < codes->len; i++) {
        const gchar *code = g_ptr_array_index(codes, i);
        gchar *display = language_display_name(code);

        gtk_string_list_append(names, display);
        g_free(display);
    }
    if (wanted != NULL) {
        for (i = 1; i < codes->len; i++) {
            if (g_strcmp0(g_ptr_array_index(codes, i), wanted) == 0) {
                selected = i;
                found = TRUE;
                break;
            }
        }
    }
    (void)found;
    g_clear_pointer(&priv->language_codes, g_ptr_array_unref);
    g_clear_object(&priv->language_names);
    priv->language_codes = codes;
    priv->language_names = names;
    priv->updating_filters = TRUE;
    gtk_drop_down_set_model(priv->language_filter, G_LIST_MODEL(names));
    gtk_drop_down_set_selected(priv->language_filter, selected);
    priv->updating_filters = FALSE;
    g_free(wanted);
}

static gboolean
module_matches_filters(GswordLibraryPage *priv, GswModuleInfo *info,
                       const gchar *folded_query, GswModuleType type_filter,
                       const gchar *language_filter, guint status_filter)
{
    const gchar *name = gsw_module_info_get_name(info);
    const gchar *description = gsw_module_info_get_description(info);
    const gchar *language;
    guint status;

    if (type_filter != GSW_MODULE_TYPE_UNKNOWN &&
        filter_type_for_info(info) != type_filter)
        return FALSE;
    language = gsw_module_info_get_language(info);
    if (language_filter != NULL && *language_filter != '\0' &&
        g_strcmp0(language, language_filter) != 0)
        return FALSE;
    status = gsw_install_manager_get_module_status(priv->manager, info);
    if (!status_matches(status, status_filter))
        return FALSE;
    if (folded_query != NULL) {
        gchar *folded_name = g_utf8_casefold(name != NULL ? name : "", -1);
        gchar *folded_description = g_utf8_casefold(
            description != NULL ? description : "", -1);
        gboolean match = strstr(folded_name, folded_query) != NULL ||
                         strstr(folded_description, folded_query) != NULL;

        g_free(folded_name);
        g_free(folded_description);
        return match;
    }
    return TRUE;
}

static void
apply_filters(GswordWindow *self)
{
    GswordLibraryPage *priv = library_page_get(self);
    const gchar *query;
    const gchar *language;
    gchar *folded_query = NULL;
    GswModuleType type_filter;
    guint status_filter;
    guint count = 0;
    guint i;
    gchar *count_text;

    if (priv == NULL || priv->remote_list == NULL)
        return;
    query = gtk_editable_get_text(GTK_EDITABLE(priv->install_search));
    if (query != NULL && *query != '\0')
        folded_query = g_utf8_casefold(query, -1);
    type_filter = filter_type_from_index(
        gtk_drop_down_get_selected(priv->type_filter));
    status_filter = gtk_drop_down_get_selected(priv->status_filter);
    language = selected_language_code(priv);

    gsword_window_clear_list(priv->remote_list);
    g_free(priv->selected_remote_name);
    priv->selected_remote_name = NULL;
    clear_remote_details(self);

    for (i = 0; priv->remote_infos != NULL && i < priv->remote_infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(priv->remote_infos, i);
        const gchar *name = gsw_module_info_get_name(info);
        guint status;
        GtkWidget *row;
        GtkWidget *label;
        gchar *text;

        if (!module_matches_filters(priv, info, folded_query, type_filter,
                                    language, status_filter))
            continue;
        status = gsw_install_manager_get_module_status(priv->manager, info);
        if (priv->library_view == 2 &&
            (status & GSW_MODULE_STATUS_UPDATED) == 0)
            continue;
        text = g_strdup_printf("%s\n%s", name, module_status_text(status));
        row = gtk_list_box_row_new();
        label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "remote-name", g_strdup(name),
                               g_free);
        gtk_list_box_append(priv->remote_list, row);
        count++;
        g_free(text);
    }
    if (count == 0) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(
            _("No modules match the current filters."));

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_add_css_class(label, "dim-label");
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_append(priv->remote_list, row);
    }
    if (priv->library_view != 0) {
        count_text = g_strdup_printf(_("%u module(s)"), count);
        gtk_label_set_text(priv->filter_count, count_text);
        g_free(count_text);
    }
    g_free(folded_query);
}

static const gchar *
module_status_text(guint status)
{
    if (status & GSW_MODULE_STATUS_NEW)
        return _("Not installed");
    if (status & GSW_MODULE_STATUS_UPDATED)
        return _("Update available");
    if (status & GSW_MODULE_STATUS_OLDER)
        return _("Installed - local version is newer");
    if (status & GSW_MODULE_STATUS_SAME_VERSION)
        return _("Installed - up to date");
    return _("Installed");
}

static void
gsword_window_refresh_installed_list(GswordWindow *self)
{
    GswordLibraryPage *priv = library_page_get(self);
    GPtrArray *infos;
    const gchar *query;
    gchar *folded_query = NULL;
    GswModuleType type_filter;
    const gchar *language_filter;
    guint count = 0;
    guint i;
    gchar *count_text;

    if (priv->installed_list == NULL)
        return;
    gsword_window_clear_list(priv->installed_list);
    infos = gsw_manager_get_module_infos(priv->manager);
    query = gtk_editable_get_text(GTK_EDITABLE(priv->install_search));
    if (query != NULL && *query != '\0')
        folded_query = g_utf8_casefold(query, -1);
    type_filter = filter_type_from_index(
        gtk_drop_down_get_selected(priv->type_filter));
    language_filter = selected_language_code(priv);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);
        const gchar *name = gsw_module_info_get_name(info);
        const gchar *description = gsw_module_info_get_description(info);
        const gchar *language = gsw_module_info_get_language(info);
        gchar *language_text;
        gchar *text;
        GtkWidget *row;
        GtkWidget *label;

        if (type_filter != GSW_MODULE_TYPE_UNKNOWN &&
            filter_type_for_info(info) != type_filter)
            continue;
        if (language_filter != NULL && *language_filter != '\0' &&
            g_strcmp0(language, language_filter) != 0)
            continue;
        if (folded_query != NULL) {
            gchar *folded_name = g_utf8_casefold(name != NULL ? name : "", -1);
            gchar *folded_description = g_utf8_casefold(
                description != NULL ? description : "", -1);
            gboolean match = strstr(folded_name, folded_query) != NULL ||
                             strstr(folded_description, folded_query) != NULL;

            g_free(folded_name);
            g_free(folded_description);
            if (!match)
                continue;
        }
        language_text = language_display_name(language);
        text = g_strdup_printf("%s\n%s · %s · %s", name,
                               gsword_window_module_type_display(info),
                               language_text,
                               gsw_module_info_get_version(info) != NULL
                               ? gsw_module_info_get_version(info) : "?");
        row = gtk_list_box_row_new();
        label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "installed-name", g_strdup(name),
                               g_free);
        gtk_list_box_append(priv->installed_list, row);
        g_free(text);
        g_free(language_text);
        count++;
    }
    if (count == 0) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(_("No installed modules."));

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_add_css_class(label, "dim-label");
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_append(priv->installed_list, row);
    }
    if (priv->library_view == 0) {
        count_text = g_strdup_printf(_("%u module(s)"), count);
        gtk_label_set_text(priv->filter_count, count_text);
        g_free(count_text);
    }
    g_free(folded_query);
    g_ptr_array_unref(infos);
}

static void
gsword_window_refresh_install_sources(GswordWindow *self)
{
    GswordLibraryPage *priv = library_page_get(self);
    GPtrArray *sources;
    GtkStringList *names;
    guint i;

    if (priv->installer == NULL) {
        gsword_window_set_status(self, _("Install manager is not available"));
        return;
    }
    sources = gsw_install_manager_get_sources(priv->installer);
    if (priv->install_sources != NULL)
        g_ptr_array_unref(priv->install_sources);
    priv->install_sources = g_ptr_array_new_with_free_func(g_object_unref);
    names = gtk_string_list_new(NULL);
    for (i = 0; i < sources->len; i++) {
        GswInstallSource *source = g_ptr_array_index(sources, i);

        g_ptr_array_add(priv->install_sources, g_object_ref(source));
        gtk_string_list_append(names,
            gsw_install_source_get_caption(source) != NULL
            ? gsw_install_source_get_caption(source) : _("Source"));
    }
    g_ptr_array_unref(sources);
    g_clear_object(&priv->source_names);
    priv->source_names = names;
    gtk_drop_down_set_model(priv->source_dropdown, G_LIST_MODEL(names));
    if (priv->install_sources->len > 0)
        gtk_drop_down_set_selected(priv->source_dropdown, 0);
    gsword_window_refresh_remote_modules(self);
}

static gchar *
format_size(guint64 bytes)
{
    if (bytes >= G_GUINT64_CONSTANT(1024) * 1024 * 1024)
        return g_strdup_printf("%.1f GB", (gdouble)bytes /
                               (1024.0 * 1024.0 * 1024.0));
    if (bytes >= G_GUINT64_CONSTANT(1024) * 1024)
        return g_strdup_printf("%.1f MB", (gdouble)bytes /
                               (1024.0 * 1024.0));
    if (bytes >= G_GUINT64_CONSTANT(1024))
        return g_strdup_printf("%.1f KB", (gdouble)bytes / 1024.0);
    return g_strdup_printf("%" G_GUINT64_FORMAT " B", bytes);
}

static void
clear_remote_details(GswordWindow *self)
{
    GswordLibraryPage *priv = library_page_get(self);

    gtk_label_set_text(priv->remote_title, _("Select a module"));
    gtk_label_set_text(priv->remote_description, "");
    gtk_label_set_text(priv->remote_status, "");
    gtk_label_set_text(priv->remote_meta, "");
    gtk_label_set_text(priv->remote_warning, "");
    gtk_widget_set_visible(GTK_WIDGET(priv->remote_warning), FALSE);
    gtk_label_set_text(priv->technical_label, "");
    gtk_label_set_text(priv->license_label, "");
    gtk_label_set_text(priv->about_label, "");
    gtk_widget_set_visible(GTK_WIDGET(priv->technical_expander), FALSE);
    gtk_widget_set_visible(GTK_WIDGET(priv->license_expander), FALSE);
    gtk_widget_set_visible(GTK_WIDGET(priv->about_expander), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->install_button), FALSE);
    if (priv->remove_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(priv->remove_button), FALSE);
}

static void
gsword_window_update_remote_details(GswordWindow *self, const gchar *name)
{
    GswordLibraryPage *priv = library_page_get(self);
    GswInstallSource *source = gsword_window_current_install_source(self);
    GswModuleInfo *info = NULL;
    GswModuleInfo *local = NULL;
    const gchar *description;
    const gchar *type;
    const gchar *remote_version;
    const gchar *local_version = NULL;
    const gchar *source_caption;
    const gchar *license;
    const gchar *copyright;
    const gchar *about;
    gchar *language;
    gchar *size;
    gchar *version_text;
    gchar *meta;
    gchar *warning = NULL;
    GString *technical;
    GString *license_text;
    guint status;

    if (source == NULL || name == NULL)
        return;
    info = gsw_install_manager_lookup_remote_module_info(
        priv->installer, source, name, NULL);
    if (info == NULL)
        return;
    status = gsw_install_manager_get_module_status(priv->manager, info);
    local = gsw_manager_lookup_module_info(priv->manager, name);
    description = gsw_module_info_get_description(info);
    type = gsword_window_module_type_display(info);
    remote_version = gsw_module_info_get_version(info);
    if (local != NULL)
        local_version = gsw_module_info_get_version(local);
    source_caption = gsw_install_source_get_caption(source);
    license = gsw_module_info_get_distribution_license(info);
    copyright = gsw_module_info_get_copyright(info);
    about = gsw_module_info_get_about(info);
    language = language_display_name(gsw_module_info_get_language(info));
    size = format_size(gsw_module_info_get_install_size(info));

    if (local_version != NULL && remote_version != NULL &&
        g_strcmp0(local_version, remote_version) != 0)
        version_text = g_strdup_printf(_("Local %s / Remote %s"),
                                       local_version, remote_version);
    else
        version_text = g_strdup(remote_version != NULL ? remote_version : "?");

    meta = g_strdup_printf(
        _("Type: %s\nLanguage: %s\nVersion: %s\nSize: %s\nSource: %s\nLicense: %s"),
        type != NULL ? type : _("Unknown"), language, version_text, size,
        source_caption != NULL ? source_caption : _("Unknown"),
        license != NULL && *license != '\0' ? license : _("Not specified"));

    gtk_label_set_text(priv->remote_title, name);
    gtk_label_set_text(priv->remote_status, module_status_text(status));
    gtk_label_set_text(priv->remote_description,
                       description != NULL ? description : "");
    gtk_label_set_text(priv->remote_meta, meta);

    if (!gsw_module_info_is_supported(info))
        warning = g_strdup(_("⚠ This module may not be readable by this version of gsword."));
    if (gsw_module_info_is_ciphered(info)) {
        gchar *cipher_warning = g_strdup(
            _("⚠ This module is encrypted and may require unlocking."));

        if (warning != NULL) {
            gchar *combined = g_strconcat(warning, "\n", cipher_warning, NULL);

            g_free(warning);
            g_free(cipher_warning);
            warning = combined;
        } else {
            warning = cipher_warning;
        }
    }
    gtk_label_set_text(priv->remote_warning,
                       warning != NULL ? warning : "");
    gtk_widget_set_visible(GTK_WIDGET(priv->remote_warning), warning != NULL);

    technical = g_string_new(NULL);
    g_string_append_printf(technical, _("Driver: %s\n"),
                           gsw_module_info_get_driver_name(info) != NULL
                           ? gsw_module_info_get_driver_name(info) : _("Unknown"));
    g_string_append_printf(technical, _("Encoding: %s\n"),
                           gsw_module_info_get_encoding(info) != NULL
                           ? gsw_module_info_get_encoding(info) : _("Unknown"));
    g_string_append_printf(technical, _("Versification: %s\n"),
                           gsw_module_info_get_versification(info) != NULL
                           ? gsw_module_info_get_versification(info) : _("Unknown"));
    g_string_append_printf(technical, _("Minimum SWORD version: %s\n"),
                           gsw_module_info_get_minimum_version(info) != NULL
                           ? gsw_module_info_get_minimum_version(info) : _("Unknown"));
    g_string_append_printf(technical, _("Module ID: %s\n"), name);
    g_string_append_printf(technical, _("Config file: %s\n"),
                           gsw_module_info_get_config_file(info) != NULL
                           ? gsw_module_info_get_config_file(info) : _("Unknown"));
    g_string_append_printf(technical, _("Data path: %s"),
                           gsw_module_info_get_absolute_data_path(info) != NULL
                           ? gsw_module_info_get_absolute_data_path(info) : _("Unknown"));
    gtk_label_set_text(priv->technical_label, technical->str);
    gtk_widget_set_visible(GTK_WIDGET(priv->technical_expander), TRUE);

    license_text = g_string_new(NULL);
    g_string_append_printf(license_text, _("License: %s\n\nCopyright: %s"),
                           license != NULL && *license != '\0'
                           ? license : _("Not specified"),
                           copyright != NULL && *copyright != '\0'
                           ? copyright : _("Not specified"));
    gtk_label_set_text(priv->license_label, license_text->str);
    gtk_widget_set_visible(GTK_WIDGET(priv->license_expander), TRUE);

    gtk_label_set_text(priv->about_label,
                       about != NULL && *about != '\0'
                       ? about : _("No additional information is available."));
    gtk_widget_set_visible(GTK_WIDGET(priv->about_expander), TRUE);

    gtk_widget_set_sensitive(GTK_WIDGET(priv->install_button),
                             (status & GSW_MODULE_STATUS_NEW) ||
                             (status & GSW_MODULE_STATUS_UPDATED));
    if (priv->remove_button != NULL)
        gtk_widget_set_sensitive(GTK_WIDGET(priv->remove_button), FALSE);

    g_free(language);
    g_free(size);
    g_free(version_text);
    g_free(meta);
    g_free(warning);
    g_string_free(technical, TRUE);
    g_string_free(license_text, TRUE);
}

static void
gsword_window_update_installed_details(GswordWindow *self, const gchar *name)
{
    GswordLibraryPage *priv = library_page_get(self);
    GswModuleInfo *info;
    const gchar *description;
    const gchar *license;
    const gchar *copyright;
    const gchar *about;
    gchar *language;
    gchar *size;
    gchar *meta;
    gchar *warning = NULL;
    GString *technical;
    GString *license_text;

    if (name == NULL)
        return;
    info = gsw_manager_lookup_module_info(priv->manager, name);
    if (info == NULL)
        return;
    description = gsw_module_info_get_description(info);
    license = gsw_module_info_get_distribution_license(info);
    copyright = gsw_module_info_get_copyright(info);
    about = gsw_module_info_get_about(info);
    language = language_display_name(gsw_module_info_get_language(info));
    size = format_size(gsw_module_info_get_install_size(info));
    meta = g_strdup_printf(
        _("Type: %s\nLanguage: %s\nVersion: %s\nSize: %s\nSource: %s\nLicense: %s"),
        gsword_window_module_type_display(info), language,
        gsw_module_info_get_version(info) != NULL
        ? gsw_module_info_get_version(info) : "?",
        size, _("Local library"),
        license != NULL && *license != '\0' ? license : _("Not specified"));

    gtk_label_set_text(priv->remote_title, name);
    gtk_label_set_text(priv->remote_status, _("Installed"));
    gtk_label_set_text(priv->remote_description,
                       description != NULL ? description : "");
    gtk_label_set_text(priv->remote_meta, meta);

    if (!gsw_module_info_is_supported(info))
        warning = g_strdup(_("⚠ This module may not be readable by this version of gsword."));
    if (gsw_module_info_is_ciphered(info)) {
        gchar *cipher_warning = g_strdup(
            _("⚠ This module is encrypted and may require unlocking."));

        if (warning != NULL) {
            gchar *combined = g_strconcat(warning, "\n", cipher_warning, NULL);

            g_free(warning);
            g_free(cipher_warning);
            warning = combined;
        } else {
            warning = cipher_warning;
        }
    }
    gtk_label_set_text(priv->remote_warning, warning != NULL ? warning : "");
    gtk_widget_set_visible(GTK_WIDGET(priv->remote_warning), warning != NULL);

    technical = g_string_new(NULL);
    g_string_append_printf(technical, _("Driver: %s\n"),
        gsw_module_info_get_driver_name(info) != NULL
        ? gsw_module_info_get_driver_name(info) : _("Unknown"));
    g_string_append_printf(technical, _("Encoding: %s\n"),
        gsw_module_info_get_encoding(info) != NULL
        ? gsw_module_info_get_encoding(info) : _("Unknown"));
    g_string_append_printf(technical, _("Versification: %s\n"),
        gsw_module_info_get_versification(info) != NULL
        ? gsw_module_info_get_versification(info) : _("Unknown"));
    g_string_append_printf(technical, _("Module ID: %s\n"), name);
    g_string_append_printf(technical, _("Config file: %s\n"),
        gsw_module_info_get_config_file(info) != NULL
        ? gsw_module_info_get_config_file(info) : _("Unknown"));
    g_string_append_printf(technical, _("Data path: %s"),
        gsw_module_info_get_absolute_data_path(info) != NULL
        ? gsw_module_info_get_absolute_data_path(info) : _("Unknown"));
    gtk_label_set_text(priv->technical_label, technical->str);
    gtk_widget_set_visible(GTK_WIDGET(priv->technical_expander), TRUE);

    license_text = g_string_new(NULL);
    g_string_append_printf(license_text, _("License: %s\n\nCopyright: %s"),
        license != NULL && *license != '\0' ? license : _("Not specified"),
        copyright != NULL && *copyright != '\0' ? copyright : _("Not specified"));
    gtk_label_set_text(priv->license_label, license_text->str);
    gtk_widget_set_visible(GTK_WIDGET(priv->license_expander), TRUE);

    gtk_label_set_text(priv->about_label,
        about != NULL && *about != '\0'
        ? about : _("No additional information is available."));
    gtk_widget_set_visible(GTK_WIDGET(priv->about_expander), TRUE);

    gtk_widget_set_sensitive(GTK_WIDGET(priv->install_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->remove_button), TRUE);

    g_free(language);
    g_free(size);
    g_free(meta);
    g_free(warning);
    g_string_free(technical, TRUE);
    g_string_free(license_text, TRUE);
}

static void
gsword_window_refresh_remote_modules(GswordWindow *self)
{
    GswordLibraryPage *priv = library_page_get(self);
    GswInstallSource *source = gsword_window_current_install_source(self);
    GPtrArray *infos;
    GError *error = NULL;

    g_clear_pointer(&priv->remote_infos, g_ptr_array_unref);
    if (source == NULL) {
        rebuild_language_filter(self, NULL);
        if (priv->library_view != 0) {
            apply_filters(self);
            gtk_label_set_text(priv->remote_status,
                _("No source selected. Refresh the source list first."));
        }
        return;
    }
    infos = gsw_install_manager_get_remote_module_infos(
        priv->installer, source, &error);
    if (infos == NULL) {
        gsword_window_show_error(self, _("Cannot read remote modules"), error);
        rebuild_language_filter(self, NULL);
        if (priv->library_view != 0)
            apply_filters(self);
        return;
    }
    priv->remote_infos = infos;
    rebuild_language_filter(self, infos);
    if (priv->library_view != 0)
        apply_filters(self);
}

static void
on_remote_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    const gchar *name;

    (void)list;
    if (row == NULL)
        return;
    name = g_object_get_data(G_OBJECT(row), "remote-name");
    if (name == NULL)
        return;
    g_free(priv->selected_remote_name);
    priv->selected_remote_name = g_strdup(name);
    g_clear_pointer(&priv->selected_installed_name, g_free);
    gsword_window_update_remote_details(self, name);
}

static void
on_installed_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    const gchar *name;

    (void)list;
    if (row == NULL)
        return;
    name = g_object_get_data(G_OBJECT(row), "installed-name");
    if (name == NULL)
        return;
    g_free(priv->selected_installed_name);
    priv->selected_installed_name = g_strdup(name);
    gsword_window_update_installed_details(self, name);
}

static void
source_dialog_data_free(gpointer data)
{
    g_free(data);
}

static void
on_add_source_clicked(GtkButton *button, gpointer user_data)
{
    SourceDialogData *data = user_data;
    GswordLibraryPage *priv = data->page;
    GswordWindow *self = priv->window;
    const gchar *caption = gtk_editable_get_text(
        GTK_EDITABLE(data->caption_entry));
    const gchar *host = gtk_editable_get_text(GTK_EDITABLE(data->host_entry));
    const gchar *directory = gtk_editable_get_text(
        GTK_EDITABLE(data->directory_entry));
    GswInstallSource *source;
    GError *error = NULL;

    (void)button;
    if (caption == NULL || *caption == '\0' || host == NULL || *host == '\0') {
        gsword_window_set_status(self, _("Enter a source name and host"));
        return;
    }
    source = gsw_install_source_new(GSW_SOURCE_HTTPS, caption, host,
                                    directory != NULL ? directory : "",
                                    NULL, NULL, NULL);
    if (source == NULL) {
        gsword_window_set_status(self, _("Invalid source settings"));
        return;
    }
    if (!gsw_install_manager_add_source(priv->installer, source)) {
        gsword_window_set_status(self, _("Cannot add source"));
        g_object_unref(source);
        return;
    }
    if (!gsw_install_manager_save_config(priv->installer, &error)) {
        gsword_window_show_error(self, _("Cannot save source"), error);
        g_object_unref(source);
        return;
    }
    g_object_unref(source);
    gsword_window_refresh_install_sources(self);
    gsword_window_set_status(self, _("Source added"));
    if (data->dialog != NULL)
        adw_dialog_close(data->dialog);
}

static void
on_remove_source_clicked(GtkButton *button, gpointer user_data)
{
    SourceDialogData *data = user_data;
    GswordLibraryPage *priv = data->page;
    GswordWindow *self = priv->window;
    const gchar *caption = g_object_get_data(G_OBJECT(button),
                                             "source-caption");
    GError *error = NULL;

    if (caption == NULL)
        return;
    if (!gsw_install_manager_remove_source(priv->installer, caption)) {
        gsword_window_set_status(self, _("Cannot remove source"));
        return;
    }
    if (!gsw_install_manager_save_config(priv->installer, &error)) {
        gsword_window_show_error(self, _("Cannot save source"), error);
        return;
    }
    gsword_window_refresh_install_sources(self);
    gsword_window_set_status(self, _("Source removed"));
    if (data->dialog != NULL)
        adw_dialog_close(data->dialog);
}

static void
on_manage_sources(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    AdwDialog *dialog;
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *sources_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *add_label;
    GtkWidget *add_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *caption_entry;
    GtkWidget *host_entry;
    GtkWidget *directory_entry;
    GtkWidget *add_button;
    GPtrArray *sources;
    SourceDialogData *data;
    guint i;

    (void)button;
    if (priv->installer == NULL)
        return;
    dialog = ADW_DIALOG(adw_alert_dialog_new(_("Installation sources"), NULL));
    data = g_new0(SourceDialogData, 1);
    data->page = priv;
    data->dialog = dialog;

    add_label = gtk_label_new(_("Configured sources"));
    gtk_label_set_xalign(GTK_LABEL(add_label), 0.0f);
    gtk_widget_add_css_class(add_label, "heading");
    gtk_box_append(GTK_BOX(box), add_label);
    sources = gsw_install_manager_get_sources(priv->installer);
    if (sources->len == 0) {
        GtkWidget *empty = gtk_label_new(_("No sources configured."));

        gtk_label_set_xalign(GTK_LABEL(empty), 0.0f);
        gtk_widget_add_css_class(empty, "dim-label");
        gtk_box_append(GTK_BOX(sources_box), empty);
    }
    for (i = 0; i < sources->len; i++) {
        GswInstallSource *source = g_ptr_array_index(sources, i);
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *label = gtk_label_new(
            gsw_install_source_get_caption(source));
        GtkWidget *remove = gtk_button_new_from_icon_name(
            "edit-delete-symbolic");
        gchar *detail = g_strdup_printf("%s · %s",
            gsw_install_source_get_host(source),
            gsw_install_source_get_directory(source));
        GtkWidget *detail_label = gtk_label_new(detail);

        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_widget_add_css_class(label, "heading");
        gtk_label_set_xalign(GTK_LABEL(detail_label), 0.0f);
        gtk_widget_add_css_class(detail_label, "caption");
        gtk_widget_add_css_class(detail_label, "dim-label");
        gtk_widget_set_hexpand(label, TRUE);
        gtk_widget_add_css_class(remove, "flat");
        g_object_set_data_full(G_OBJECT(remove), "source-caption",
                               g_strdup(gsw_install_source_get_caption(source)),
                               g_free);
        g_signal_connect(remove, "clicked",
                         G_CALLBACK(on_remove_source_clicked), data);
        gtk_box_append(GTK_BOX(row), label);
        gtk_box_append(GTK_BOX(row), detail_label);
        gtk_box_append(GTK_BOX(row), remove);
        gtk_box_append(GTK_BOX(sources_box), row);
        g_free(detail);
    }
    g_ptr_array_unref(sources);
    gtk_box_append(GTK_BOX(box), sources_box);

    add_label = gtk_label_new(_("Add source"));
    gtk_label_set_xalign(GTK_LABEL(add_label), 0.0f);
    gtk_widget_add_css_class(add_label, "heading");
    gtk_box_append(GTK_BOX(box), add_label);
    caption_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(caption_entry),
                                   _("Source name"));
    host_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(host_entry), _("Host"));
    directory_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(directory_entry),
                                   _("Module directory"));
    gtk_editable_set_text(GTK_EDITABLE(directory_entry),
                          "ftpmirror/pub/sword");
    gtk_box_append(GTK_BOX(box), caption_entry);
    gtk_box_append(GTK_BOX(box), host_entry);
    gtk_box_append(GTK_BOX(box), directory_entry);
    add_button = gtk_button_new_with_label(_("Add source"));
    gtk_box_append(GTK_BOX(add_box), add_button);
    gtk_widget_set_halign(add_box, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(box), add_box);
    data->caption_entry = GTK_ENTRY(caption_entry);
    data->host_entry = GTK_ENTRY(host_entry);
    data->directory_entry = GTK_ENTRY(directory_entry);
    g_signal_connect(add_button, "clicked",
                     G_CALLBACK(on_add_source_clicked), data);
    g_object_set_data_full(G_OBJECT(dialog), "source-dialog-data", data,
                           source_dialog_data_free);
    adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(dialog), box);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog),
                                   "close", _("Close"), NULL);
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "close");
    adw_dialog_present(dialog, GTK_WIDGET(self));
}

static void
on_refresh_sources_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    (void)button;

    if (priv->installer == NULL)
        return;
    gtk_widget_set_sensitive(GTK_WIDGET(priv->refresh_sources_button), FALSE);
    gsword_window_set_status(self, _("Refreshing install sources..."));
    gsw_install_manager_refresh_remote_source_configuration_async(
        priv->installer, NULL, on_refresh_sources_finished, self);
}

static void
on_refresh_sources_finished(GObject *source, GAsyncResult *result,
                            gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    GError *error = NULL;

    if (!gsw_install_manager_refresh_remote_source_configuration_finish(
            GSW_INSTALL_MANAGER(source), result, &error)) {
        gsword_window_show_error(self, _("Cannot refresh install sources"), error);
    } else {
        gsword_window_set_status(self, _("Install sources refreshed"));
    }
    gtk_widget_set_sensitive(GTK_WIDGET(priv->refresh_sources_button), TRUE);
    gsword_window_refresh_install_sources(self);
}

static void
on_refresh_remote_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    GswInstallSource *install_source = gsword_window_current_install_source(self);

    (void)button;
    if (install_source == NULL)
        return;
    g_clear_object(&priv->install_cancellable);
    priv->install_cancellable = g_cancellable_new();
    gsw_install_manager_set_cancellable(priv->installer,
                                        priv->install_cancellable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->refresh_source_button), FALSE);
    gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), TRUE);
    gtk_progress_bar_pulse(priv->install_progress);
    gsword_window_set_status(self, _("Refreshing remote module list..."));
    gsw_install_manager_refresh_remote_source_async(
        priv->installer, install_source, priv->install_cancellable,
        on_refresh_remote_finished, self);
}

static gboolean
clear_install_progress_cb(gpointer data)
{
    GswordWindow *self = GSWORD_WINDOW(data);
    GswordLibraryPage *priv = library_page_get(self);

    if (priv->install_cancellable == NULL) {
        gtk_progress_bar_set_fraction(priv->install_progress, 0.0);
        gtk_label_set_text(priv->install_status, "");
    }
    return G_SOURCE_REMOVE;
}

static void
gsword_window_clear_install_progress_later(GswordWindow *self)
{
    /* Progress events are queued on the main context from the worker thread.
     * Clear after those events have been processed so a final progress update
     * cannot leave a stale "Downloading..." label after the operation ends. */
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, clear_install_progress_cb,
                    g_object_ref(self), g_object_unref);
}

static void
on_refresh_remote_finished(GObject *source, GAsyncResult *result,
                           gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    GError *error = NULL;

    if (!gsw_install_manager_refresh_remote_source_finish(
            GSW_INSTALL_MANAGER(source), result, &error)) {
        gsword_window_show_error(self, _("Cannot refresh remote modules"), error);
    } else {
        gsword_window_set_status(self, _("Remote module list refreshed"));
    }
    gtk_widget_set_sensitive(GTK_WIDGET(priv->refresh_source_button), TRUE);
    gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), FALSE);
    gtk_progress_bar_set_fraction(priv->install_progress, 0.0);
    g_clear_object(&priv->install_cancellable);
    gsword_window_refresh_remote_modules(self);
    gsword_window_clear_install_progress_later(self);
}

static void
start_next_update(GswordWindow *self)
{
    GswordLibraryPage *priv = library_page_get(self);
    const gchar *name;

    if (priv->update_queue == NULL || priv->update_queue->len == 0) {
        priv->updating_all = FALSE;
        gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), FALSE);
        gtk_progress_bar_set_fraction(priv->install_progress, 0.0);
        g_clear_object(&priv->install_cancellable);
        g_clear_object(&priv->update_source);
        gsword_window_set_status(self, _("All updates complete"));
        gsword_window_refresh_remote_modules(self);
        return;
    }
    g_free(priv->selected_remote_name);
    priv->selected_remote_name = g_ptr_array_steal_index(priv->update_queue, 0);
    name = priv->selected_remote_name;
    if (priv->install_cancellable == NULL) {
        priv->install_cancellable = g_cancellable_new();
        gsw_install_manager_set_cancellable(priv->installer,
                                            priv->install_cancellable);
    }
    gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), TRUE);
    gtk_progress_bar_pulse(priv->install_progress);
    gsword_window_set_status(self, _("Updating modules..."));
    gsw_install_manager_install_module_async(
        priv->installer, priv->manager, priv->update_source, name,
        priv->install_cancellable, on_install_finished, self);
}

static void
on_update_all_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    GswInstallSource *source = gsword_window_current_install_source(self);
    guint i;

    (void)button;
    if (source == NULL || priv->remote_infos == NULL) {
        gsword_window_set_status(self, _("No source selected"));
        return;
    }
    g_clear_pointer(&priv->update_queue, g_ptr_array_unref);
    priv->update_queue = g_ptr_array_new_with_free_func(g_free);
    for (i = 0; i < priv->remote_infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(priv->remote_infos, i);
        guint status = gsw_install_manager_get_module_status(priv->manager,
                                                             info);

        if (status & GSW_MODULE_STATUS_UPDATED)
            g_ptr_array_add(priv->update_queue,
                            g_strdup(gsw_module_info_get_name(info)));
    }
    if (priv->update_queue->len == 0) {
        gsword_window_set_status(self, _("No updates available"));
        return;
    }
    g_clear_object(&priv->update_source);
    priv->update_source = g_object_ref(source);
    priv->updating_all = TRUE;
    start_next_update(self);
}

static void
on_install_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    GswInstallSource *source = gsword_window_current_install_source(self);

    (void)button;
    if (source == NULL || priv->selected_remote_name == NULL)
        return;
    priv->updating_all = FALSE;
    g_clear_pointer(&priv->update_queue, g_ptr_array_unref);
    g_clear_object(&priv->update_source);
    g_clear_object(&priv->install_cancellable);
    priv->install_cancellable = g_cancellable_new();
    gsw_install_manager_set_cancellable(priv->installer,
                                        priv->install_cancellable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->install_button), FALSE);
    gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), TRUE);
    gsword_window_set_status(self, _("Installing module..."));
    gsw_install_manager_install_module_async(
        priv->installer, priv->manager, source, priv->selected_remote_name,
        priv->install_cancellable, on_install_finished, self);
}

static void
on_install_finished(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    GError *error = NULL;
    gboolean installed;
    gchar *installed_name = g_strdup(priv->selected_remote_name);

    installed = gsw_install_manager_install_module_finish(
        GSW_INSTALL_MANAGER(source), result, &error);
    if (!installed) {
        gsword_window_show_error(self, _("Module install failed"), error);
    }
    gtk_widget_set_visible(GTK_WIDGET(priv->cancel_button), FALSE);
    gtk_progress_bar_set_fraction(priv->install_progress, 0.0);
    g_clear_object(&priv->install_cancellable);
    if (priv->manager != NULL)
        gsw_manager_reload(priv->manager, NULL);
    if (installed && installed_name != NULL && priv->settings != NULL) {
        /* Make the newly installed module visible and selected instead of
         * silently keeping the previously selected module. */
        g_settings_set_string(priv->settings, "last-module", installed_name);
        g_settings_set_string(priv->settings, "last-key", "");
    }
    gsword_window_populate_modules(self);
    gsword_window_refresh_installed_list(self);
    gsword_window_refresh_remote_modules(self);
    gsword_window_show_page(self, "library");
    if (priv->updating_all) {
        g_free(installed_name);
        start_next_update(self);
        return;
    }
    if (installed && installed_name != NULL) {
        gchar *message = g_strdup_printf(_("Installed module %s"),
                                         installed_name);
        gsword_window_set_status(self, message);
        g_free(message);
    }
    g_free(installed_name);
    gsword_window_clear_install_progress_later(self);
}

static void
on_remove_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    const gchar *name = priv->selected_installed_name;

    (void)button;
    if (priv->installer == NULL || name == NULL)
        return;
    g_clear_object(&priv->install_cancellable);
    priv->install_cancellable = g_cancellable_new();
    gsw_install_manager_set_cancellable(priv->installer,
                                        priv->install_cancellable);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->remove_button), FALSE);
    gsword_window_set_status(self, _("Removing module..."));
    gsw_install_manager_remove_module_async(
        priv->installer, priv->manager, name, priv->install_cancellable,
        on_remove_finished, self);
}

static void
on_remove_finished(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    GError *error = NULL;

    if (!gsw_install_manager_remove_module_finish(
            GSW_INSTALL_MANAGER(source), result, &error)) {
        gsword_window_show_error(self, _("Module removal failed"), error);
    } else {
        gsword_window_set_status(self, _("Module removed"));
    }
    g_clear_object(&priv->install_cancellable);
    g_clear_pointer(&priv->selected_installed_name, g_free);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->remove_button), FALSE);
    if (priv->manager != NULL)
        gsw_manager_reload(priv->manager, NULL);
    gsword_window_populate_modules(self);
    gsword_window_refresh_installed_list(self);
    gsword_window_refresh_remote_modules(self);
    gsword_window_show_page(self, "library");
    gsword_window_clear_install_progress_later(self);
}

static void
on_cancel_install_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    (void)button;

    if (priv->install_cancellable != NULL)
        g_cancellable_cancel(priv->install_cancellable);
    priv->updating_all = FALSE;
    g_clear_pointer(&priv->update_queue, g_ptr_array_unref);
    g_clear_object(&priv->update_source);
    gsword_window_set_status(self, _("Cancelling..."));
}

static void
on_install_pre_status(GswStatusReporter *reporter, guint64 total, guint64 done,
                      const gchar *message, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    (void)reporter;

    gtk_progress_bar_set_fraction(priv->install_progress,
                                  total > 0 ? (gdouble)done / total : 0.0);
    gtk_label_set_text(priv->install_status,
                       message != NULL ? message : _("Installing..."));
    (void)self;
}

static void
on_install_progress(GswStatusReporter *reporter, guint64 total, guint64 done,
                    gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    gchar *status;
    (void)reporter;

    if (total > 0) {
        gtk_progress_bar_set_fraction(priv->install_progress,
                                      (gdouble)done / total);
        status = g_strdup_printf("%s: %" G_GUINT64_FORMAT
                                 " / %" G_GUINT64_FORMAT " %s",
                                 _("Downloading"), done, total, _("bytes"));
    } else {
        gtk_progress_bar_pulse(priv->install_progress);
        status = g_strdup_printf("%s: %" G_GUINT64_FORMAT " %s",
                                 _("Downloading"), done, _("bytes"));
    }
    gtk_label_set_text(priv->install_status, status);
    g_free(status);
    (void)self;
}

static void
on_browse_local_finished(GObject *source, GAsyncResult *result,
                         gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    GError *error = NULL;
    GFile *folder = gtk_file_dialog_select_folder_finish(
        GTK_FILE_DIALOG(source), result, &error);

    if (folder == NULL) {
        g_clear_error(&error);
        return;
    }
    {
        gchar *path = g_file_get_path(folder);

        gtk_editable_set_text(GTK_EDITABLE(priv->local_dir_entry),
                              path != NULL ? path : "");
        g_free(path);
    }
    g_object_unref(folder);
}

static void
on_browse_local_dir(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GtkFileDialog *dialog = gtk_file_dialog_new();

    (void)button;
    gtk_file_dialog_set_title(dialog, _("Select local module directory"));
    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(self), NULL,
                                  on_browse_local_finished, self);
    g_object_unref(dialog);
}

static void
on_local_install_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);
    const gchar *directory;
    const gchar *name;
    GError *error = NULL;

    (void)button;
    if (priv->installer == NULL || priv->manager == NULL)
        return;
    directory = gtk_editable_get_text(GTK_EDITABLE(priv->local_dir_entry));
    name = gtk_editable_get_text(GTK_EDITABLE(priv->local_name_entry));
    if (directory == NULL || *directory == '\0' ||
        name == NULL || *name == '\0') {
        gsword_window_set_status(self,
            _("Enter a local SWORD directory and module name"));
        return;
    }
    if (!gsw_install_manager_install_module_from_directory(
            priv->installer, priv->manager, directory, name, &error)) {
        gsword_window_show_error(self, _("Local install failed"), error);
        return;
    }
    gsw_manager_reload(priv->manager, NULL);
    if (priv->settings != NULL) {
        g_settings_set_string(priv->settings, "last-module", name);
        g_settings_set_string(priv->settings, "last-key", "");
    }
    gsword_window_populate_modules(self);
    gsword_window_refresh_installed_list(self);
    gsword_window_refresh_remote_modules(self);
    gsword_window_show_page(self, "library");
    gsword_window_set_status(self, _("Module installed from local directory"));
}

static void
on_filter_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);

    (void)object;
    (void)pspec;
    if (!priv->updating_filters) {
        if (priv->library_view == 0)
            gsword_window_refresh_installed_list(self);
        else
            apply_filters(self);
    }
}

static void
on_remote_search_changed(GtkEditable *editable, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);

    (void)editable;
    if (!priv->updating_filters) {
        if (priv->library_view == 0)
            gsword_window_refresh_installed_list(self);
        else
            apply_filters(self);
    }
}

static void
on_clear_filters(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordLibraryPage *priv = library_page_get(self);

    (void)button;
    priv->updating_filters = TRUE;
    gtk_drop_down_set_selected(priv->type_filter, 0);
    gtk_drop_down_set_selected(priv->language_filter, 0);
    gtk_drop_down_set_selected(priv->status_filter, 0);
    gtk_editable_set_text(GTK_EDITABLE(priv->install_search), "");
    priv->updating_filters = FALSE;
    if (priv->library_view == 0)
        gsword_window_refresh_installed_list(self);
    else
        apply_filters(self);
}

static void
on_source_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    if (library_page_get(GSWORD_WINDOW(user_data))->library_view != 0)
        gsword_window_refresh_remote_modules(GSWORD_WINDOW(user_data));
}

static void
update_library_view(GswordWindow *self, gint view)
{
    GswordLibraryPage *priv = library_page_get(self);

    priv->library_view = view;
    if (priv->source_row != NULL)
        gtk_widget_set_visible(priv->source_row, view != 0);
    if (priv->status_filter != NULL)
        gtk_widget_set_visible(GTK_WIDGET(priv->status_filter), view == 1);
    if (priv->local_box != NULL)
        gtk_widget_set_visible(priv->local_box, view == 0);
    if (priv->update_all_button != NULL)
        gtk_widget_set_visible(GTK_WIDGET(priv->update_all_button), view == 2);
    if (priv->list_stack != NULL)
        gtk_stack_set_visible_child_name(priv->list_stack,
            view == 0 ? "installed" : "remote");
    gtk_widget_set_sensitive(GTK_WIDGET(priv->install_button), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->remove_button), FALSE);
    if (view == 0) {
        g_clear_pointer(&priv->selected_remote_name, g_free);
        clear_remote_details(self);
        gsword_window_refresh_installed_list(self);
    } else {
        g_clear_pointer(&priv->selected_installed_name, g_free);
        gsword_window_refresh_remote_modules(self);
    }
}

static void
on_view_toggled(GtkToggleButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    gint view;

    if (!gtk_toggle_button_get_active(button))
        return;
    view = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "library-view"));
    update_library_view(self, view);
}

static void
library_page_free(gpointer data)
{
    GswordLibraryPage *page = data;

    if (page == NULL)
        return;
    g_clear_object(&page->installer);
    g_clear_object(&page->install_reporter);
    g_clear_pointer(&page->install_sources, g_ptr_array_unref);
    g_clear_object(&page->source_names);
    g_clear_object(&page->install_cancellable);
    g_clear_pointer(&page->update_queue, g_ptr_array_unref);
    g_clear_object(&page->update_source);
    g_clear_pointer(&page->language_codes, g_ptr_array_unref);
    g_clear_object(&page->language_names);
    g_clear_pointer(&page->remote_infos, g_ptr_array_unref);
    g_free(page->selected_remote_name);
    g_free(page->selected_installed_name);
    g_free(page);
}

GswordLibraryPage *
gsword_library_page_new(GswordWindow *self)
{
    GswordLibraryPage *priv = g_new0(GswordLibraryPage, 1);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *view_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *installed_view;
    GtkWidget *available_view;
    GtkWidget *updates_view;
    GtkWidget *update_all = gtk_button_new_with_label(_("Update all"));
    GtkWidget *manage_sources = gtk_button_new_with_label(_("Manage sources"));
    GtkWidget *source_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *filter_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *progress_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *left_scroll = gtk_scrolled_window_new();
    GtkWidget *installed_scroll = gtk_scrolled_window_new();
    GtkWidget *action_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *details_scroll = gtk_scrolled_window_new();
    GtkWidget *details_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *technical_expander;
    GtkWidget *technical_label;
    GtkWidget *license_expander;
    GtkWidget *license_label;
    GtkWidget *about_expander;
    GtkWidget *about_label;
    GtkWidget *refresh_sources = gtk_button_new_from_icon_name(
        "view-refresh-symbolic");
    GtkWidget *refresh_remote = gtk_button_new_from_icon_name(
        "network-transmit-receive-symbolic");
    GtkWidget *install = gtk_button_new_with_label(_("Install / Update"));
    GtkWidget *remove = gtk_button_new_with_label(_("Remove"));
    GtkWidget *cancel = gtk_button_new_with_label(_("Cancel"));
    GtkWidget *local_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *local_label = gtk_label_new(_("Install from local directory"));
    GtkStringList *type_names = gtk_string_list_new(
        (const gchar * const[]) { _("All types"), _("Bibles"),
                                  _("Commentaries"), _("Lexicons / Dictionaries"),
                                  _("Daily Devotions"), _("General Books"),
                                  _("Maps"), _("Other Modules"), NULL });
    GtkStringList *status_names = gtk_string_list_new(
        (const gchar * const[]) { _("All statuses"), _("Not installed"),
                                  _("Update available"), _("Installed"),
                                  _("Installed - local version is newer"),
                                  _("Installed - up to date"), NULL });

    installed_view = gtk_toggle_button_new_with_label(_("Installed"));
    available_view = gtk_toggle_button_new_with_label(_("Available"));
    updates_view = gtk_toggle_button_new_with_label(_("Updates"));
    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(available_view),
                                GTK_TOGGLE_BUTTON(installed_view));
    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(updates_view),
                                GTK_TOGGLE_BUTTON(installed_view));
    g_object_set_data(G_OBJECT(installed_view), "library-view",
                      GINT_TO_POINTER(0));
    g_object_set_data(G_OBJECT(available_view), "library-view",
                      GINT_TO_POINTER(1));
    g_object_set_data(G_OBJECT(updates_view), "library-view",
                      GINT_TO_POINTER(2));
    gtk_widget_add_css_class(manage_sources, "flat");
    g_signal_connect(manage_sources, "clicked",
                     G_CALLBACK(on_manage_sources), self);
    g_signal_connect(installed_view, "toggled",
                     G_CALLBACK(on_view_toggled), self);
    g_signal_connect(available_view, "toggled",
                     G_CALLBACK(on_view_toggled), self);
    g_signal_connect(updates_view, "toggled",
                     G_CALLBACK(on_view_toggled), self);
    gtk_widget_add_css_class(update_all, "flat");
    gtk_widget_set_tooltip_text(update_all, _("Install all available updates"));
    gtk_widget_set_visible(update_all, FALSE);
    g_signal_connect(update_all, "clicked",
                     G_CALLBACK(on_update_all_clicked), self);
    gtk_widget_add_css_class(view_row, "linked");
    gtk_box_append(GTK_BOX(view_row), installed_view);
    gtk_box_append(GTK_BOX(view_row), available_view);
    gtk_box_append(GTK_BOX(view_row), updates_view);
    gtk_box_append(GTK_BOX(view_row), update_all);
    gtk_box_append(GTK_BOX(view_row), manage_sources);

    priv->window = self;
    priv->manager = gsword_window_get_manager(self);
    priv->settings = gsword_window_get_settings(self);
    g_object_set_data_full(G_OBJECT(self), "gsword-library-page", priv,
                           library_page_free);
    {
        GError *error = NULL;
        gchar *data_dir = gsword_app_data_dir();
        gchar *installmgr_dir = g_build_filename(data_dir, "installmgr", NULL);

        priv->installer = gsw_install_manager_new(installmgr_dir, &error);
        if (priv->installer == NULL) {
            g_printerr("gsword: cannot open install manager: %s\n",
                       error != NULL ? error->message : "unknown error");
            g_clear_error(&error);
        } else {
            priv->install_reporter = gsw_status_reporter_new();
            gsw_status_reporter_set_main_context(priv->install_reporter,
                                                 g_main_context_default());
            g_signal_connect(priv->install_reporter, "pre-status",
                             G_CALLBACK(on_install_pre_status), self);
            g_signal_connect(priv->install_reporter, "progress",
                             G_CALLBACK(on_install_progress), self);
            gsw_install_manager_set_status_reporter(priv->installer,
                                                    priv->install_reporter);
            gsw_install_manager_set_user_disclaimer_confirmed(
                priv->installer, TRUE);
        }
        g_free(installmgr_dir);
        g_free(data_dir);
    }
    gtk_widget_set_margin_top(controls, 8);
    gtk_widget_set_margin_bottom(controls, 8);
    gtk_widget_set_margin_start(controls, 12);
    gtk_widget_set_margin_end(controls, 12);

    priv->source_dropdown = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    priv->install_search = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->install_search, _("Filter modules"));
    gtk_widget_set_hexpand(GTK_WIDGET(priv->install_search), TRUE);
    priv->type_filter = GTK_DROP_DOWN(gtk_drop_down_new(
        G_LIST_MODEL(type_names), NULL));
    priv->language_filter = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    priv->status_filter = GTK_DROP_DOWN(gtk_drop_down_new(
        G_LIST_MODEL(status_names), NULL));
    priv->clear_filters_button = GTK_BUTTON(
        gtk_button_new_with_label(_("Clear filters")));
    priv->filter_count = GTK_LABEL(gtk_label_new(_("0 module(s)")));
    gtk_label_set_xalign(priv->filter_count, 1.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->filter_count), "dim-label");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->filter_count), TRUE);
    g_object_unref(type_names);
    g_object_unref(status_names);
    gtk_widget_set_tooltip_text(refresh_sources,
                                _("Refresh the install source list"));
    gtk_widget_set_tooltip_text(refresh_remote,
                                _("Refresh modules from the selected source"));
    g_signal_connect(refresh_sources, "clicked",
                     G_CALLBACK(on_refresh_sources_clicked), self);
    g_signal_connect(refresh_remote, "clicked",
                     G_CALLBACK(on_refresh_remote_clicked), self);
    g_signal_connect(priv->source_dropdown, "notify::selected",
                     G_CALLBACK(on_source_changed), self);
    g_signal_connect(priv->install_search, "changed",
                     G_CALLBACK(on_remote_search_changed), self);
    g_signal_connect(priv->type_filter, "notify::selected",
                     G_CALLBACK(on_filter_changed), self);
    g_signal_connect(priv->language_filter, "notify::selected",
                     G_CALLBACK(on_filter_changed), self);
    g_signal_connect(priv->status_filter, "notify::selected",
                     G_CALLBACK(on_filter_changed), self);
    g_signal_connect(priv->clear_filters_button, "clicked",
                     G_CALLBACK(on_clear_filters), self);

    gtk_box_append(GTK_BOX(source_row), GTK_WIDGET(priv->source_dropdown));
    gtk_box_append(GTK_BOX(source_row), refresh_sources);
    gtk_box_append(GTK_BOX(source_row), refresh_remote);

    gtk_box_append(GTK_BOX(filter_row), GTK_WIDGET(priv->install_search));
    gtk_box_append(GTK_BOX(filter_row), GTK_WIDGET(priv->type_filter));
    gtk_box_append(GTK_BOX(filter_row), GTK_WIDGET(priv->language_filter));
    gtk_box_append(GTK_BOX(filter_row), GTK_WIDGET(priv->status_filter));
    gtk_box_append(GTK_BOX(filter_row), GTK_WIDGET(priv->clear_filters_button));
    gtk_box_append(GTK_BOX(filter_row), GTK_WIDGET(priv->filter_count));

    priv->install_progress = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_progress_bar_set_show_text(priv->install_progress, FALSE);
    gtk_widget_set_hexpand(GTK_WIDGET(priv->install_progress), TRUE);
    priv->install_status = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->install_status, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->install_status), "dim-label");
    gtk_widget_set_visible(cancel, FALSE);
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel_install_clicked),
                     self);
    gtk_box_append(GTK_BOX(progress_row), GTK_WIDGET(priv->install_progress));
    gtk_box_append(GTK_BOX(progress_row), GTK_WIDGET(priv->install_status));
    gtk_box_append(GTK_BOX(progress_row), cancel);
    priv->cancel_button = GTK_BUTTON(cancel);
    priv->refresh_sources_button = GTK_BUTTON(refresh_sources);
    priv->refresh_source_button = GTK_BUTTON(refresh_remote);
    priv->source_row = source_row;
    priv->filter_row = filter_row;
    gtk_box_append(GTK_BOX(controls), view_row);
    gtk_box_append(GTK_BOX(controls), source_row);
    gtk_box_append(GTK_BOX(controls), filter_row);
    gtk_box_append(GTK_BOX(controls), progress_row);

    priv->remote_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(priv->remote_list, "row-selected",
                     G_CALLBACK(on_remote_selected), self);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(left_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(left_scroll),
                                  GTK_WIDGET(priv->remote_list));

    priv->installed_list = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(priv->installed_list, "row-selected",
                     G_CALLBACK(on_installed_selected), self);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(installed_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(installed_scroll),
                                  GTK_WIDGET(priv->installed_list));

    priv->list_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_add_named(priv->list_stack, installed_scroll, "installed");
    gtk_stack_add_named(priv->list_stack, left_scroll, "remote");
    gtk_widget_set_vexpand(GTK_WIDGET(priv->list_stack), TRUE);
    gtk_widget_set_size_request(GTK_WIDGET(priv->list_stack), 320, -1);

    priv->remote_title = GTK_LABEL(gtk_label_new(_("Select a module")));
    gtk_label_set_xalign(priv->remote_title, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->remote_title), "title-2");
    priv->remote_description = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->remote_description, 0.0f);
    gtk_label_set_wrap(priv->remote_description, TRUE);
    priv->remote_status = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->remote_status, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->remote_status), "dim-label");
    priv->remote_meta = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->remote_meta, 0.0f);
    gtk_label_set_wrap(priv->remote_meta, TRUE);
    priv->remote_warning = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(priv->remote_warning, 0.0f);
    gtk_label_set_wrap(priv->remote_warning, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(priv->remote_warning), "warning");
    gtk_widget_set_visible(GTK_WIDGET(priv->remote_warning), FALSE);

    technical_expander = gtk_expander_new(_("Technical details"));
    technical_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(technical_label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(technical_label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(technical_label), TRUE);
    gtk_widget_set_margin_start(technical_label, 12);
    gtk_expander_set_child(GTK_EXPANDER(technical_expander), technical_label);
    priv->technical_expander = GTK_EXPANDER(technical_expander);
    priv->technical_label = GTK_LABEL(technical_label);

    license_expander = gtk_expander_new(_("Copyright & license"));
    license_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(license_label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(license_label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(license_label), TRUE);
    gtk_widget_set_margin_start(license_label, 12);
    gtk_expander_set_child(GTK_EXPANDER(license_expander), license_label);
    priv->license_expander = GTK_EXPANDER(license_expander);
    priv->license_label = GTK_LABEL(license_label);

    about_expander = gtk_expander_new(_("About this module"));
    about_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(about_label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(about_label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(about_label), TRUE);
    gtk_widget_set_margin_start(about_label, 12);
    gtk_expander_set_child(GTK_EXPANDER(about_expander), about_label);
    priv->about_expander = GTK_EXPANDER(about_expander);
    priv->about_label = GTK_LABEL(about_label);

    gtk_widget_set_visible(technical_expander, FALSE);
    gtk_widget_set_visible(license_expander, FALSE);
    gtk_widget_set_visible(about_expander, FALSE);
    gtk_box_append(GTK_BOX(details_box), GTK_WIDGET(priv->remote_meta));
    gtk_box_append(GTK_BOX(details_box), GTK_WIDGET(priv->remote_warning));
    gtk_box_append(GTK_BOX(details_box), technical_expander);
    gtk_box_append(GTK_BOX(details_box), license_expander);
    gtk_box_append(GTK_BOX(details_box), about_expander);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(details_scroll),
                                  details_box);

    gtk_box_append(GTK_BOX(right), GTK_WIDGET(priv->remote_title));
    gtk_box_append(GTK_BOX(right), GTK_WIDGET(priv->remote_status));
    gtk_box_append(GTK_BOX(right), GTK_WIDGET(priv->remote_description));
    gtk_widget_set_vexpand(details_scroll, TRUE);
    gtk_box_append(GTK_BOX(right), details_scroll);
    priv->install_button = GTK_BUTTON(install);
    priv->remove_button = GTK_BUTTON(remove);
    gtk_widget_set_sensitive(install, FALSE);
    gtk_widget_set_sensitive(remove, FALSE);
    g_signal_connect(install, "clicked", G_CALLBACK(on_install_clicked), self);
    g_signal_connect(remove, "clicked", G_CALLBACK(on_remove_clicked), self);
    gtk_box_append(GTK_BOX(action_row), install);
    gtk_box_append(GTK_BOX(action_row), remove);
    gtk_box_append(GTK_BOX(right), action_row);

    gtk_label_set_xalign(GTK_LABEL(local_label), 0.0f);
    gtk_widget_add_css_class(local_label, "heading");
    priv->local_dir_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->local_dir_entry,
                                   _("Local SWORD library directory"));
    priv->local_name_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->local_name_entry, _("Module name"));
    priv->local_install_button = GTK_BUTTON(
        gtk_button_new_with_label(_("Install local module")));
    g_signal_connect(priv->local_install_button, "clicked",
                     G_CALLBACK(on_local_install_clicked), self);
    gtk_box_append(GTK_BOX(local_box), local_label);
    {
        GtkWidget *dir_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *browse = gtk_button_new_with_label(_("Browse…"));

        gtk_widget_set_hexpand(GTK_WIDGET(priv->local_dir_entry), TRUE);
        g_signal_connect(browse, "clicked",
                         G_CALLBACK(on_browse_local_dir), self);
        gtk_box_append(GTK_BOX(dir_row), GTK_WIDGET(priv->local_dir_entry));
        gtk_box_append(GTK_BOX(dir_row), browse);
        gtk_box_append(GTK_BOX(local_box), dir_row);
    }
    gtk_box_append(GTK_BOX(local_box), GTK_WIDGET(priv->local_name_entry));
    gtk_box_append(GTK_BOX(local_box),
                   GTK_WIDGET(priv->local_install_button));
    gtk_box_append(GTK_BOX(right), local_box);
    priv->local_box = local_box;

    gtk_widget_set_vexpand(paned, TRUE);
    gtk_paned_set_start_child(GTK_PANED(paned), GTK_WIDGET(priv->list_stack));
    gtk_paned_set_end_child(GTK_PANED(paned), right);
    gtk_paned_set_position(GTK_PANED(paned), 360);
    gtk_box_append(GTK_BOX(box), controls);
    gtk_box_append(GTK_BOX(box), paned);
    priv->root = box;
    priv->installed_view_button = GTK_TOGGLE_BUTTON(installed_view);
    priv->available_view_button = GTK_TOGGLE_BUTTON(available_view);
    priv->updates_view_button = GTK_TOGGLE_BUTTON(updates_view);
    priv->update_all_button = GTK_BUTTON(update_all);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(installed_view), TRUE);
    update_library_view(self, 0);
    return priv;
}


GtkWidget *
gsword_library_page_widget(GswordLibraryPage *page)
{
    return page != NULL ? page->root : NULL;
}

void
gsword_library_page_refresh_sources(GswordLibraryPage *page)
{
    if (page != NULL)
        gsword_window_refresh_install_sources(page->window);
}

void
gsword_library_page_refresh_installed(GswordLibraryPage *page)
{
    if (page != NULL)
        gsword_window_refresh_installed_list(page->window);
}

void
gsword_library_page_refresh_remote(GswordLibraryPage *page)
{
    if (page != NULL)
        gsword_window_refresh_remote_modules(page->window);
}
