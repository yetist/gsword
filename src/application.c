#include "application.h"
#include "window.h"
#include "app-locale.h"
#include "app-paths.h"

#include <gsword/gsword.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

struct _GswordApplication {
    AdwApplication parent_instance;
};

typedef struct {
    gchar *library_path;
    GswManager *manager;
    GSettings *settings;
} GswordApplicationPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GswordApplication, gsword_application,
                           ADW_TYPE_APPLICATION)

static void
gsword_application_finalize(GObject *object)
{
    GswordApplication *self = GSWORD_APPLICATION(object);
    GswordApplicationPrivate *priv =
        gsword_application_get_instance_private(self);

    g_clear_pointer(&priv->library_path, g_free);
    g_clear_object(&priv->manager);
    g_clear_object(&priv->settings);

    G_OBJECT_CLASS(gsword_application_parent_class)->finalize(object);
}

static void
gsword_application_quit_action(GSimpleAction *action, GVariant *parameter,
                               gpointer user_data)
{
    (void)action;
    (void)parameter;
    g_application_quit(G_APPLICATION(user_data));
}

static void
gsword_application_startup(GApplication *application)
{
    static const GActionEntry app_actions[] = {
        { "quit", gsword_application_quit_action, NULL, NULL, NULL,
          { 0, 0, 0 } },
    };

    G_APPLICATION_CLASS(gsword_application_parent_class)->startup(application);

    {
        gchar *locale = gsword_app_locale_name();

        gsw_locale_mgr_set_default_locale(gsw_locale_mgr_get_default(), locale);
        g_free(locale);
    }

    g_action_map_add_action_entries(G_ACTION_MAP(application), app_actions,
                                    G_N_ELEMENTS(app_actions), application);
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(application), "app.quit",
        (const gchar * const[]) { "<Control>q", NULL });
}

static gint
gsword_application_handle_local_options(GApplication *application,
                                        GVariantDict *options,
                                        gpointer user_data)
{
    GswordApplication *self = GSWORD_APPLICATION(application);
    GswordApplicationPrivate *priv =
        gsword_application_get_instance_private(self);
    const gchar *library = NULL;

    (void)user_data;
    if (g_variant_dict_lookup(options, "library", "&s", &library) &&
        library != NULL && *library != '\0') {
        g_free(priv->library_path);
        priv->library_path = g_strdup(library);
    }
    return -1;
}

static GSettings *
gsword_application_create_settings(void)
{
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();
    GSettingsSchema *schema;

    if (source == NULL)
        return NULL;
    schema = g_settings_schema_source_lookup(source, "org.gsword.GSword",
                                             TRUE);
    if (schema == NULL)
        return NULL;
    {
        GSettings *settings = g_settings_new_full(schema, NULL, NULL);

        g_settings_schema_unref(schema);
        return settings;
    }
}

static GswManager *
gsword_application_open_library(GswordApplication *self, GError **error)
{
    GswordApplicationPrivate *priv =
        gsword_application_get_instance_private(self);
    gchar *path = priv->library_path != NULL
                ? g_strdup(priv->library_path)
                : gsword_app_default_library_path();
    gchar *canonical;
    gchar *mods;
    GPtrArray *system_dirs;
    gboolean added_fallback = FALSE;
    guint i;

    if (g_mkdir_with_parents(path, 0755) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    _("Cannot create library directory %s"), path);
        g_free(path);
        return NULL;
    }
    mods = g_build_filename(path, "mods.d", NULL);
    g_mkdir_with_parents(mods, 0755);
    g_free(mods);

    priv->manager = gsw_manager_new_for_path(path, error);
    if (priv->manager == NULL) {
        g_free(path);
        return NULL;
    }

    canonical = g_canonicalize_filename(path, NULL);
    system_dirs = gsword_app_system_library_dirs();
    for (i = 0; i < system_dirs->len; i++) {
        const gchar *system_path = g_ptr_array_index(system_dirs, i);
        gchar *system_canonical = g_canonicalize_filename(system_path, NULL);

        if (g_strcmp0(canonical, system_canonical) != 0) {
            gsw_manager_add_fallback_path(priv->manager, system_path);
            added_fallback = TRUE;
        }
        g_free(system_canonical);
    }
    g_ptr_array_unref(system_dirs);
    g_free(canonical);
    g_free(path);

    if (added_fallback && !gsw_manager_reload(priv->manager, error)) {
        g_clear_object(&priv->manager);
        return NULL;
    }
    return priv->manager;
}

static void
gsword_application_activate(GApplication *application)
{
    GswordApplication *self = GSWORD_APPLICATION(application);
    GswordApplicationPrivate *priv =
        gsword_application_get_instance_private(self);
    GtkWindow *window;
    GError *error = NULL;

    window = gtk_application_get_active_window(GTK_APPLICATION(application));
    if (window != NULL) {
        gtk_window_present(window);
        return;
    }

    if (priv->manager == NULL &&
        gsword_application_open_library(self, &error) == NULL) {
        g_printerr(_("gsword: cannot open module library: %s\n"),
                   error != NULL ? error->message : _("unknown error"));
        g_clear_error(&error);
        g_application_quit(application);
        return;
    }

    if (priv->settings == NULL)
        priv->settings = gsword_application_create_settings();

    window = GTK_WINDOW(gsword_window_new(GTK_APPLICATION(application),
                                          priv->manager, priv->settings));
    gtk_window_present(window);
}

static void
gsword_application_class_init(GswordApplicationClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GApplicationClass *application_class = G_APPLICATION_CLASS(klass);

    object_class->finalize = gsword_application_finalize;
    application_class->startup = gsword_application_startup;
    application_class->activate = gsword_application_activate;
}

static void
gsword_application_init(GswordApplication *self)
{
    g_application_add_main_option(
        G_APPLICATION(self), "library", 'l', G_OPTION_FLAG_NONE,
        G_OPTION_ARG_STRING, _("SWORD module library path"), "PATH");
    g_signal_connect(self, "handle-local-options",
                     G_CALLBACK(gsword_application_handle_local_options),
                     NULL);
}

GswordApplication *
gsword_application_new(void)
{
    GApplicationFlags flags = G_APPLICATION_DEFAULT_FLAGS;

    if (g_getenv("GSWORD_NON_UNIQUE") != NULL)
        flags |= G_APPLICATION_NON_UNIQUE;
    return g_object_new(GSWORD_TYPE_APPLICATION,
                        "application-id", "org.gsword.GSword",
                        "flags", flags,
                        NULL);
}
