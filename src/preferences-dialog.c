/*
 * preferences-dialog.c: application appearance preferences for gsword
 */

#include "preferences-dialog.h"
#include "window-private.h"

#include <glib/gi18n.h>

typedef struct {
    GtkCssProvider *css;
    gchar *theme;
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
        ".gsw-reader { font-size: %.1fpt; }\n"
        ".memory-card { font-size: 16pt; }\n"
        ".teaching-reader { font-size: 18pt; }\n"
        ".workspace-pane { border: 1px solid alpha(currentColor, 0.16);"
        " border-radius: 8px; }\n"
        ".workspace-pane.dragging { opacity: 0.55; }\n"
        ".workspace-pane.active { border: 2px solid alpha(@accent_color, 0.85); }\n"
        ".workspace-pane-header { min-height: 34px; }", points);
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

void
gsword_preferences_show(GswordWindow *window)
{
    GswordPreferences *prefs = get_preferences(window);
    AdwPreferencesDialog *dialog;
    AdwPreferencesPage *page;
    AdwPreferencesGroup *group;
    AdwActionRow *font_row;
    AdwComboRow *theme_row;
    GtkAdjustment *adjustment;
    GtkWidget *spin;
    GtkStringList *themes;
    guint selected = 0;

    if (prefs == NULL)
        return;
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

    adw_preferences_group_add(group, GTK_WIDGET(font_row));
    adw_preferences_group_add(group, GTK_WIDGET(theme_row));
    adw_preferences_page_add(page, group);
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
    }
    g_object_set_data_full(G_OBJECT(window), "gsword-preferences", prefs,
                           preferences_free);
    apply_css(prefs);
    apply_theme(prefs);
}
