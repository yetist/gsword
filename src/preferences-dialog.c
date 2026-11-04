/*
 * preferences-dialog.c: application appearance preferences for gsword
 */

#include "preferences-dialog.h"
#include "app-paths.h"
#include "window-private.h"

#include <glib/gi18n.h>

typedef struct {
    GtkCssProvider *css;
    gchar *theme;
    gchar *font_family;
    gdouble font_scale;
    AdwDialog *dialog;
} GswordPreferences;

static GswordPreferences *
get_preferences(GswordWindow *window)
{
    return g_object_get_data(G_OBJECT(window), "gsword-preferences");
}

static void
apply_css(GswordPreferences *prefs)
{
    gchar *css_text;
    gdouble points = 14.0 * prefs->font_scale;

    if (prefs->css == NULL)
        return;
    css_text = g_strdup_printf(
        ".gsw-reader { font-size: %.1fpt; font-family: \"%s\"; }\n"
        ".memory-card { font-size: 16pt; }\n"
        ".teaching-reader { font-size: 18pt; }\n"
        ".workspace-pane { border: 1px solid alpha(currentColor, 0.16);"
        " border-radius: 8px; }\n"
        ".workspace-pane.dragging { opacity: 0.55; }\n"
        ".workspace-pane.active { border: 2px solid alpha(@accent_color, 0.85); }\n"
        ".workspace-pane-header { min-height: 34px; }",
        points, g_strcmp0(prefs->font_family, "serif") == 0
                ? "Serif" : "Sans");
    gtk_css_provider_load_from_string(prefs->css, css_text);
    g_free(css_text);
}

static void
apply_theme(GswordPreferences *prefs)
{
    AdwStyleManager *manager = adw_style_manager_get_default();
    AdwColorScheme scheme = ADW_COLOR_SCHEME_DEFAULT;

    if (g_strcmp0(prefs->theme, "light") == 0)
        scheme = ADW_COLOR_SCHEME_FORCE_LIGHT;
    else if (g_strcmp0(prefs->theme, "dark") == 0)
        scheme = ADW_COLOR_SCHEME_FORCE_DARK;
    adw_style_manager_set_color_scheme(manager, scheme);
}

static void
on_font_scale_changed(GtkSpinButton *spin, gpointer user_data)
{
    GswordWindow *window = GSWORD_WINDOW(user_data);
    GswordPreferences *prefs = get_preferences(window);
    GSettings *settings = gsword_window_get_settings(window);

    prefs->font_scale = gtk_spin_button_get_value(spin);
    if (settings != NULL)
        g_settings_set_double(settings, "font-scale", prefs->font_scale);
    apply_css(prefs);
}

static void
on_theme_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWindow *window = GSWORD_WINDOW(user_data);
    GswordPreferences *prefs = get_preferences(window);
    GSettings *settings = gsword_window_get_settings(window);
    guint selected = adw_combo_row_get_selected(ADW_COMBO_ROW(object));

    (void)pspec;
    g_free(prefs->theme);
    prefs->theme = g_strdup(selected == 1 ? "light"
                           : selected == 2 ? "dark" : "system");
    if (settings != NULL)
        g_settings_set_string(settings, "theme", prefs->theme);
    apply_theme(prefs);
}

static void
on_font_family_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWindow *window = GSWORD_WINDOW(user_data);
    GswordPreferences *prefs = get_preferences(window);
    GSettings *settings = gsword_window_get_settings(window);
    guint selected = adw_combo_row_get_selected(ADW_COMBO_ROW(object));

    (void)pspec;
    g_free(prefs->font_family);
    prefs->font_family = g_strdup(selected == 1 ? "serif" : "sans");
    if (settings != NULL)
        g_settings_set_string(settings, "reader-font-family",
                              prefs->font_family);
    apply_css(prefs);
}

static void
on_book_name_language_changed(GObject *object, GParamSpec *pspec,
                              gpointer user_data)
{
    GswordWindow *window = GSWORD_WINDOW(user_data);
    GSettings *settings = gsword_window_get_settings(window);
    guint selected = adw_combo_row_get_selected(ADW_COMBO_ROW(object));

    (void)pspec;
    if (settings != NULL)
        g_settings_set_string(settings, "book-name-language",
                              selected == 1 ? "ui" : "module");
}

gdouble
gsword_preferences_get_font_scale(GswordWindow *window)
{
    GswordPreferences *prefs = get_preferences(window);

    return prefs != NULL ? prefs->font_scale : 1.0;
}

void
gsword_preferences_set_font_scale(GswordWindow *window, gdouble font_scale)
{
    GswordPreferences *prefs = get_preferences(window);
    GSettings *settings = gsword_window_get_settings(window);

    if (prefs == NULL)
        return;
    prefs->font_scale = CLAMP(font_scale, 0.75, 2.5);
    if (settings != NULL)
        g_settings_set_double(settings, "font-scale", prefs->font_scale);
    apply_css(prefs);
}

static void
on_open_library_folder(GtkButton *button, gpointer user_data)
{
    const gchar *path = g_object_get_data(G_OBJECT(button), "library-path");
    GFile *file;
    GtkFileLauncher *launcher;

    if (path == NULL || *path == '\0')
        return;
    file = g_file_new_for_path(path);
    launcher = gtk_file_launcher_new(file);
    gtk_file_launcher_launch(launcher, GTK_WINDOW(user_data), NULL, NULL, NULL);
    g_object_unref(launcher);
    g_object_unref(file);
}

static void
set_pending_library_path(GswordWindow *window, const gchar *path)
{
    GSettings *settings = gsword_window_get_settings(window);

    if (settings == NULL || path == NULL || *path == '\0')
        return;
    g_settings_set_string(settings, "library-path", path);
    gsword_window_toast(window,
                        _("Restart gsword to use the selected module library"));
}

static void
on_library_folder_selected(GObject *source, GAsyncResult *result,
                           gpointer user_data)
{
    GswordWindow *window = GSWORD_WINDOW(user_data);
    GError *error = NULL;
    GFile *folder = gtk_file_dialog_select_folder_finish(
        GTK_FILE_DIALOG(source), result, &error);

    if (folder == NULL) {
        g_clear_error(&error);
        return;
    }
    {
        gchar *path = g_file_get_path(folder);

        set_pending_library_path(window, path);
        g_free(path);
    }
    g_object_unref(folder);
}

static void
on_choose_library(GtkButton *button, gpointer user_data)
{
    GswordWindow *window = GSWORD_WINDOW(user_data);
    GtkFileDialog *dialog = gtk_file_dialog_new();

    (void)button;
    gtk_file_dialog_set_title(dialog, _("Choose module library"));
    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(window), NULL,
                                  on_library_folder_selected, window);
    g_object_unref(dialog);
}

static void
on_use_legacy_library(GtkButton *button, gpointer user_data)
{
    const gchar *path = g_object_get_data(G_OBJECT(button), "library-path");

    set_pending_library_path(GSWORD_WINDOW(user_data), path);
}

void
gsword_preferences_show(GswordWindow *window)
{
    GswordPreferences *prefs = get_preferences(window);
    AdwPreferencesDialog *dialog;
    AdwPreferencesPage *page;
    AdwPreferencesGroup *group;
    AdwPreferencesGroup *library_group;
    AdwActionRow *font_row;
    AdwActionRow *library_row;
    AdwActionRow *system_row;
    AdwActionRow *legacy_row;
    AdwComboRow *theme_row;
    AdwComboRow *font_family_row;
    AdwComboRow *book_name_row;
    GtkAdjustment *adjustment;
    GtkWidget *spin;
    GtkWidget *library_actions;
    GtkWidget *open_library;
    GtkWidget *choose_library;
    GtkWidget *legacy_button;
    GtkStringList *themes;
    GtkStringList *families;
    GtkStringList *book_languages;
    gchar *book_policy;
    gchar *library_path;
    gchar *legacy_path;
    GPtrArray *system_dirs;
    GString *system_text;
    GSettings *settings;
    guint i;
    guint selected = 0;

    if (prefs == NULL)
        return;
    settings = gsword_window_get_settings(window);
    if (prefs->dialog != NULL) {
        adw_dialog_present(prefs->dialog, GTK_WIDGET(window));
        return;
    }

    dialog = ADW_PREFERENCES_DIALOG(adw_preferences_dialog_new());
    adw_dialog_set_title(ADW_DIALOG(dialog), _("Preferences"));
    adw_dialog_set_content_width(ADW_DIALOG(dialog), 620);

    page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
    group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(group, _("Reading"));
    adw_preferences_group_set_description(
        group, _("Reading appearance is applied immediately."));

    font_row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(font_row),
                                  _("Font size"));
    adw_action_row_set_subtitle(font_row, _("Scale the scripture text size"));
    adjustment = gtk_adjustment_new(prefs->font_scale, 0.75, 2.5, 0.1, 0.25, 0);
    spin = gtk_spin_button_new(adjustment, 0.1, 1);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    g_signal_connect(spin, "value-changed",
                     G_CALLBACK(on_font_scale_changed), window);
    adw_action_row_add_suffix(font_row, spin);
    adw_action_row_set_activatable_widget(font_row, spin);

    theme_row = ADW_COMBO_ROW(adw_combo_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(theme_row),
                                  _("Color scheme"));
    adw_action_row_set_subtitle(ADW_ACTION_ROW(theme_row),
                                _("Follow the desktop or force light/dark mode"));
    themes = gtk_string_list_new(
        (const gchar *[]) { _("Follow system"), _("Light"), _("Dark"), NULL });
    adw_combo_row_set_model(theme_row, G_LIST_MODEL(themes));
    g_object_unref(themes);
    if (g_strcmp0(prefs->theme, "light") == 0)
        selected = 1;
    else if (g_strcmp0(prefs->theme, "dark") == 0)
        selected = 2;
    adw_combo_row_set_selected(theme_row, selected);
    g_signal_connect(theme_row, "notify::selected",
                     G_CALLBACK(on_theme_changed), window);

    font_family_row = ADW_COMBO_ROW(adw_combo_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(font_family_row),
                                  _("Scripture font"));
    adw_action_row_set_subtitle(ADW_ACTION_ROW(font_family_row),
                                _("Choose a sans or serif reading font"));
    families = gtk_string_list_new(
        (const gchar *[]) { _("Sans"), _("Serif"), NULL });
    adw_combo_row_set_model(font_family_row, G_LIST_MODEL(families));
    g_object_unref(families);
    adw_combo_row_set_selected(font_family_row,
                               g_strcmp0(prefs->font_family, "serif") == 0
                               ? 1 : 0);
    g_signal_connect(font_family_row, "notify::selected",
                     G_CALLBACK(on_font_family_changed), window);

    book_name_row = ADW_COMBO_ROW(adw_combo_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(book_name_row),
                                  _("Book names"));
    adw_action_row_set_subtitle(ADW_ACTION_ROW(book_name_row),
                                _("Choose the language for Bible book names"));
    book_languages = gtk_string_list_new(
        (const gchar *[]) { _("Follow module"), _("Follow interface"), NULL });
    adw_combo_row_set_model(book_name_row, G_LIST_MODEL(book_languages));
    g_object_unref(book_languages);
    book_policy = settings != NULL
                ? g_settings_get_string(settings, "book-name-language")
                : NULL;
    adw_combo_row_set_selected(book_name_row,
                               g_strcmp0(book_policy, "ui") == 0 ? 1 : 0);
    g_free(book_policy);
    g_signal_connect(book_name_row, "notify::selected",
                     G_CALLBACK(on_book_name_language_changed), window);

    adw_preferences_group_add(group, GTK_WIDGET(font_row));
    adw_preferences_group_add(group, GTK_WIDGET(theme_row));
    adw_preferences_group_add(group, GTK_WIDGET(font_family_row));
    adw_preferences_group_add(group, GTK_WIDGET(book_name_row));
    adw_preferences_page_add(page, group);

    library_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(library_group, _("Module library"));
    adw_preferences_group_set_description(
        library_group,
        _("Use --library or GSWORD_LIBRARY to start gsword with another "
          "writable library."));

    library_path = gsword_app_library_dir();
    library_row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(library_row),
                                  _("User library"));
    adw_action_row_set_subtitle(library_row, library_path);
    library_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    open_library = gtk_button_new_with_label(_("Open"));
    gtk_widget_add_css_class(open_library, "flat");
    g_object_set_data_full(G_OBJECT(open_library), "library-path",
                           g_strdup(library_path), g_free);
    g_signal_connect(open_library, "clicked",
                     G_CALLBACK(on_open_library_folder), window);
    choose_library = gtk_button_new_with_label(_("Change…"));
    gtk_widget_add_css_class(choose_library, "flat");
    g_signal_connect(choose_library, "clicked",
                     G_CALLBACK(on_choose_library), window);
    gtk_box_append(GTK_BOX(library_actions), open_library);
    gtk_box_append(GTK_BOX(library_actions), choose_library);
    adw_action_row_add_suffix(library_row, library_actions);
    adw_action_row_set_activatable_widget(library_row, open_library);
    adw_preferences_group_add(library_group, GTK_WIDGET(library_row));

    system_text = g_string_new(NULL);
    system_dirs = gsword_app_system_library_dirs();
    for (i = 0; i < system_dirs->len; i++) {
        if (system_text->len > 0)
            g_string_append_c(system_text, '\n');
        g_string_append(system_text, g_ptr_array_index(system_dirs, i));
    }
    system_row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(system_row),
                                  _("System libraries"));
    adw_action_row_set_subtitle(system_row,
                                system_text->len > 0
                                ? system_text->str
                                : _("No system module library found"));
    adw_preferences_group_add(library_group, GTK_WIDGET(system_row));

    legacy_path = g_build_filename(g_get_home_dir(), ".sword", NULL);
    if (g_file_test(legacy_path, G_FILE_TEST_IS_DIR)) {
        legacy_row = ADW_ACTION_ROW(adw_action_row_new());
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(legacy_row),
                                      _("Legacy SWORD library"));
        adw_action_row_set_subtitle(legacy_row, legacy_path);
        legacy_button = gtk_button_new_with_label(_("Use"));
        gtk_widget_add_css_class(legacy_button, "flat");
        g_object_set_data_full(G_OBJECT(legacy_button), "library-path",
                               g_strdup(legacy_path), g_free);
        g_signal_connect(legacy_button, "clicked",
                         G_CALLBACK(on_use_legacy_library), window);
        adw_action_row_add_suffix(legacy_row, legacy_button);
        adw_action_row_set_activatable_widget(legacy_row, legacy_button);
        adw_preferences_group_add(library_group, GTK_WIDGET(legacy_row));
    }
    g_free(legacy_path);

    adw_preferences_page_add(page, library_group);

    g_string_free(system_text, TRUE);
    g_ptr_array_unref(system_dirs);
    g_free(library_path);
    adw_preferences_dialog_add(dialog, page);

    prefs->dialog = ADW_DIALOG(g_object_ref_sink(dialog));
    adw_dialog_present(prefs->dialog, GTK_WIDGET(window));
}

static void
preferences_free(gpointer data)
{
    GswordPreferences *prefs = data;

    g_clear_object(&prefs->dialog);
    g_clear_pointer(&prefs->css, g_object_unref);
    g_free(prefs->theme);
    g_free(prefs->font_family);
    g_free(prefs);
}

void
gsword_preferences_init(GswordWindow *window)
{
    GswordPreferences *prefs;
    GSettings *settings = gsword_window_get_settings(window);

    g_return_if_fail(GSWORD_IS_WINDOW(window));
    prefs = g_new0(GswordPreferences, 1);
    prefs->font_scale = 1.0;
    prefs->theme = g_strdup("system");
    prefs->font_family = g_strdup("sans");
    prefs->css = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(prefs->css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    if (settings != NULL) {
        prefs->font_scale = g_settings_get_double(settings, "font-scale");
        if (prefs->font_scale <= 0.0)
            prefs->font_scale = 1.0;
        g_free(prefs->theme);
        prefs->theme = g_settings_get_string(settings, "theme");
        if (prefs->theme == NULL || *prefs->theme == '\0') {
            g_free(prefs->theme);
            prefs->theme = g_strdup("system");
        }
        g_free(prefs->font_family);
        prefs->font_family = g_settings_get_string(settings,
                                                   "reader-font-family");
        if (g_strcmp0(prefs->font_family, "serif") != 0) {
            g_free(prefs->font_family);
            prefs->font_family = g_strdup("sans");
        }
    }
    g_object_set_data_full(G_OBJECT(window), "gsword-preferences", prefs,
                           preferences_free);
    apply_css(prefs);
    apply_theme(prefs);
}
