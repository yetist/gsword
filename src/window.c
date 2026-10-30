#include "window.h"

#include <string.h>

struct _GswordWindow {
    AdwApplicationWindow parent_instance;
};

typedef struct {
    GswManager *manager;
    GSettings *settings;
    GswModule *module;
    GtkListBox *module_list;
    GtkStack *content_stack;
    GtkTextView *reader;
    GtkEntry *key_entry;
    GtkLabel *status_label;
    AdwDialog *preferences_dialog;
    GtkCssProvider *css;
    gchar *theme;
    gdouble font_scale;

    GtkWidget *study_panel;
    GtkToggleButton *study_toggle;
    GtkNotebook *study_notebook;
    GtkTextView *commentary_view;
    GtkTextView *strongs_view;
    GtkTextView *crossref_view;
    GtkTextView *lexicon_view;
    GtkEntry *lexicon_entry;
    gchar *commentary_name;
    gchar *lexicon_name;
    GswModule *commentary_module;
    GswModule *lexicon_module;

    GtkStringList *module_names;
    GtkDropDown *parallel_a;
    GtkDropDown *parallel_b;
    GtkEntry *parallel_key;
    GtkTextView *parallel_view_a;
    GtkTextView *parallel_view_b;
    GtkScrolledWindow *parallel_scroll_a;
    GtkScrolledWindow *parallel_scroll_b;
    GtkCheckButton *parallel_sync;
    GswModule *parallel_module_a;
    GswModule *parallel_module_b;
    gboolean syncing_scroll;

    GtkEntry *search_entry;
    GtkDropDown *search_type;
    GtkCheckButton *search_case;
    GtkListBox *search_results;
    GtkLabel *index_label;
} GswordWindowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GswordWindow, gsword_window, ADW_TYPE_APPLICATION_WINDOW)

static void gsword_window_update_index_label(GswordWindow *self);
static void gsword_window_parallel_load(GswordWindow *self);

static void
gsword_window_set_status(GswordWindow *self, const gchar *message)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    gtk_label_set_text(priv->status_label, message != NULL ? message : "");
}

static void
gsword_window_set_text(GswordWindow *self, const gchar *text)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(priv->reader);

    gtk_text_buffer_set_text(buffer, text != NULL ? text : "", -1);
}

static void
gsword_window_show_error(GswordWindow *self, const gchar *prefix,
                         GError *error)
{
    gchar *message = g_strdup_printf("%s: %s", prefix,
                                     error != NULL ? error->message
                                                   : "unknown error");

    gsword_window_set_status(self, message);
    g_free(message);
    g_clear_error(&error);
}

static void
gsword_window_apply_css(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gchar *css_text;
    gdouble points = 14.0 * priv->font_scale;

    if (priv->css == NULL)
        return;
    css_text = g_strdup_printf(
        ".gsw-reader { font-size: %.1fpt; }", points);
    gtk_css_provider_load_from_string(priv->css, css_text);
    g_free(css_text);
}

static void
gsword_window_apply_theme(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    AdwStyleManager *manager = adw_style_manager_get_default();
    AdwColorScheme scheme = ADW_COLOR_SCHEME_DEFAULT;

    if (g_strcmp0(priv->theme, "light") == 0)
        scheme = ADW_COLOR_SCHEME_FORCE_LIGHT;
    else if (g_strcmp0(priv->theme, "dark") == 0)
        scheme = ADW_COLOR_SCHEME_FORCE_DARK;
    adw_style_manager_set_color_scheme(manager, scheme);
}

static void
gsword_window_save_last_position(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *name;
    const gchar *key;

    if (priv->settings == NULL || priv->module == NULL)
        return;
    name = gsw_module_get_name(priv->module);
    key = gsw_module_get_key_text(priv->module);
    if (name != NULL)
        g_settings_set_string(priv->settings, "last-module", name);
    if (key != NULL)
        g_settings_set_string(priv->settings, "last-key", key);
}

static gchar *
gsword_window_chapter_text_for_module(GswModule *module)
{
    GswVerseKey *key;
    gchar *original;
    gchar *book_name;
    gint chapter;
    gint maximum;
    gint verse;
    GString *result;

    if (module == NULL ||
        gsw_key_get_kind(gsw_module_get_key(module)) != GSW_KEY_KIND_VERSE)
        return NULL;

    key = (GswVerseKey *)gsw_module_get_key(module);
    if (gsw_verse_key_get_book(key) < 1 ||
        gsw_verse_key_get_chapter(key) < 1)
        return NULL;

    chapter = gsw_verse_key_get_chapter(key);
    maximum = gsw_verse_key_get_verse_max(key);
    if (maximum < 1)
        return NULL;

    original = g_strdup(gsw_module_get_key_text(module));
    book_name = g_strdup(gsw_verse_key_get_book_name(key));
    result = g_string_new(NULL);
    g_string_append_printf(result, "%s %d\n\n", book_name, chapter);

    for (verse = 1; verse <= maximum; verse++) {
        GError *error = NULL;
        gchar *text;

        gsw_verse_key_set_verse(key, verse);
        if (!gsw_module_set_key_text(module,
                                     gsw_module_get_key_text(module),
                                     &error)) {
            g_clear_error(&error);
            break;
        }
        text = gsw_module_get_text(module, &error);
        if (text != NULL && *text != '\0') {
            g_string_append_printf(result, "%d  %s\n\n", verse, text);
        }
        g_free(text);
        g_clear_error(&error);
    }

    if (original != NULL) {
        gsw_module_set_key_text(module, original, NULL);
        g_free(original);
    }
    g_free(book_name);
    return g_string_free(result, FALSE);
}

static void
gsword_window_set_view_text(GtkTextView *view, const gchar *text)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);

    gtk_text_buffer_set_text(buffer, text != NULL ? text : "", -1);
}

static gchar *
gsword_window_find_module_by_type(GswordWindow *self, GswModuleType type)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *infos;
    gchar *result = NULL;
    guint i;

    if (priv->manager == NULL)
        return NULL;
    infos = gsw_manager_get_module_infos(priv->manager);
    for (i = 0; i < infos->len; i++) {
        GswModuleInfo *info = g_ptr_array_index(infos, i);

        if (gsw_module_info_get_module_type(info) == type) {
            result = g_strdup(gsw_module_info_get_name(info));
            break;
        }
    }
    g_ptr_array_unref(infos);
    return result;
}

static void
gsword_window_ensure_study_modules(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->commentary_name == NULL)
        priv->commentary_name = gsword_window_find_module_by_type(
            self, GSW_MODULE_TYPE_COMMENTARIES);
    if (priv->lexicon_name == NULL)
        priv->lexicon_name = gsword_window_find_module_by_type(
            self, GSW_MODULE_TYPE_LEXDICTS);
    if (priv->commentary_module == NULL && priv->commentary_name != NULL)
        priv->commentary_module = gsw_manager_create_module(
            priv->manager, priv->commentary_name);
    if (priv->lexicon_module == NULL && priv->lexicon_name != NULL)
        priv->lexicon_module = gsw_manager_create_module(
            priv->manager, priv->lexicon_name);
}

static void
gsword_window_update_study(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;
    const gchar *key_text;
    GString *text;

    gsword_window_ensure_study_modules(self);
    if (priv->module == NULL)
        return;
    key_text = gsw_module_get_key_text(priv->module);

    if (priv->commentary_module != NULL) {
        gchar *commentary = NULL;

        if (gsw_module_set_key_text(priv->commentary_module, key_text,
                                    &error)) {
            commentary = gsw_module_get_text(priv->commentary_module, &error);
        }
        gsword_window_set_view_text(priv->commentary_view,
                                    commentary != NULL
                                    ? commentary : "No commentary for this verse.");
        g_free(commentary);
        g_clear_error(&error);
    } else {
        gsword_window_set_view_text(priv->commentary_view,
                                    "No commentary module is installed.");
    }

    text = g_string_new(NULL);
    {
        GPtrArray *keys = gsw_module_get_entry_attribute_keys(
            priv->module, "Word", "Lemma", &error);

        if (keys != NULL) {
            guint i;

            for (i = 0; i < keys->len; i++) {
                const gchar *key = g_ptr_array_index(keys, i);
                gchar *value = gsw_module_get_entry_attribute(
                    priv->module, "Word", "Lemma", key, &error);

                if (value != NULL) {
                    g_string_append_printf(text, "%s: %s\n", key, value);
                    g_free(value);
                }
            }
            g_ptr_array_unref(keys);
        }
        g_clear_error(&error);
    }
    if (text->len == 0)
        g_string_append(text, "No Strong's data for this verse.");
    gsword_window_set_view_text(priv->strongs_view, text->str);
    g_string_free(text, TRUE);

    text = g_string_new(NULL);
    {
        GPtrArray *keys = gsw_module_get_entry_attribute_keys(
            priv->module, "Crossref", "body", &error);

        if (keys != NULL) {
            guint i;

            for (i = 0; i < keys->len; i++) {
                const gchar *key = g_ptr_array_index(keys, i);
                gchar *value = gsw_module_get_entry_attribute(
                    priv->module, "Crossref", "body", key, &error);

                if (value != NULL) {
                    g_string_append_printf(text, "%s\n", value);
                    g_free(value);
                }
            }
            g_ptr_array_unref(keys);
        }
        g_clear_error(&error);
    }
    if (text->len == 0)
        g_string_append(text, "No cross-references for this verse.");
    gsword_window_set_view_text(priv->crossref_view, text->str);
    g_string_free(text, TRUE);
}

static void
gsword_window_lexicon_lookup(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;
    const gchar *key;
    gchar *text;

    gsword_window_ensure_study_modules(self);
    if (priv->lexicon_module == NULL) {
        gsword_window_set_view_text(priv->lexicon_view,
                                    "No lexicon module is installed.");
        return;
    }
    key = gtk_editable_get_text(GTK_EDITABLE(priv->lexicon_entry));
    if (!gsw_module_set_key_text(priv->lexicon_module, key, &error)) {
        gsword_window_show_error(self, "Cannot look up lexicon entry", error);
        return;
    }
    text = gsw_module_get_text(priv->lexicon_module, &error);
    if (text == NULL) {
        gsword_window_show_error(self, "Cannot read lexicon entry", error);
        return;
    }
    gsword_window_set_view_text(priv->lexicon_view, text);
    g_free(text);
}

static void
gsword_window_refresh_reader(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;
    gchar *text;
    gchar *display_key = NULL;
    gchar *status;

    if (priv->module == NULL) {
        gsword_window_set_text(self,
            "No modules found.\n\nInstall SWORD modules or start gsword with "
            "--library PATH.");
        gtk_editable_set_text(GTK_EDITABLE(priv->key_entry), "");
        gsword_window_set_status(self, "No modules found");
        gtk_stack_set_visible_child_name(priv->content_stack, "reader");
        gsword_window_update_index_label(self);
        return;
    }

    if (gsw_key_get_kind(gsw_module_get_key(priv->module)) ==
        GSW_KEY_KIND_VERSE) {
        GswVerseKey *key = (GswVerseKey *)gsw_module_get_key(priv->module);

        if (gsw_verse_key_get_book(key) >= 1 &&
            gsw_verse_key_get_chapter(key) >= 1) {
            text = gsword_window_chapter_text_for_module(priv->module);
            display_key = g_strdup_printf("%s %d",
                                          gsw_verse_key_get_book_name(key),
                                          gsw_verse_key_get_chapter(key));
        } else {
            text = gsw_module_get_text(priv->module, &error);
        }
    } else {
        text = gsw_module_get_text(priv->module, &error);
    }

    if (text == NULL) {
        gsword_window_show_error(self, "Cannot read entry", error);
        g_free(display_key);
        return;
    }
    gsword_window_set_text(self, text);
    g_free(text);

    if (display_key == NULL)
        display_key = g_strdup(gsw_module_get_key_text(priv->module));
    gtk_editable_set_text(GTK_EDITABLE(priv->key_entry),
                          display_key != NULL ? display_key : "");

    status = g_strdup_printf("%s  |  %s",
                             gsw_module_get_name(priv->module),
                             display_key != NULL ? display_key : "");
    gsword_window_set_status(self, status);
    g_free(status);
    g_free(display_key);

    gtk_stack_set_visible_child_name(priv->content_stack, "reader");
    gsword_window_update_study(self);
    gsword_window_update_index_label(self);
    gsword_window_save_last_position(self);
}

static gboolean
gsword_window_set_key_text(GswordWindow *self, const gchar *text)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;

    if (priv->module == NULL)
        return FALSE;
    if (!gsw_module_set_key_text(priv->module, text, &error)) {
        gsword_window_show_error(self, "Cannot set key", error);
        return FALSE;
    }
    gsword_window_refresh_reader(self);
    return TRUE;
}

static void
gsword_window_set_initial_key(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->module == NULL)
        return;

    if (gsw_key_get_kind(gsw_module_get_key(priv->module)) ==
        GSW_KEY_KIND_VERSE) {
        if (gsw_module_set_key_text(priv->module, "Gen 1:1", NULL))
            return;
        if (gsw_module_set_key_text(priv->module, "Matt 1:1", NULL))
            return;
    }
    gsw_module_set_position(priv->module, GSW_POSITION_TOP, NULL);
}

static void
gsword_window_load_module(GswordWindow *self, const gchar *name)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswModule *module;
    gchar *saved_module = NULL;
    gchar *saved_key = NULL;
    gboolean restored = FALSE;

    module = gsw_manager_create_module(priv->manager, name);
    if (module == NULL) {
        gsword_window_set_status(self, "Cannot open module");
        return;
    }

    g_clear_object(&priv->module);
    priv->module = module;

    if (priv->settings != NULL) {
        saved_module = g_settings_get_string(priv->settings, "last-module");
        saved_key = g_settings_get_string(priv->settings, "last-key");
    }
    if (saved_module != NULL && *saved_module != '\0' &&
        g_strcmp0(saved_module, name) == 0 &&
        saved_key != NULL && *saved_key != '\0')
        restored = gsw_module_set_key_text(priv->module, saved_key, NULL);
    if (!restored)
        gsword_window_set_initial_key(self);

    g_free(saved_module);
    g_free(saved_key);
    gsword_window_refresh_reader(self);
}

static void
on_module_row_selected(GtkListBox *list, GtkListBoxRow *row,
                       gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *name;

    (void)list;
    if (row == NULL)
        return;
    name = g_object_get_data(G_OBJECT(row), "module-name");
    if (name != NULL) {
        gsword_window_load_module(self, name);
        gtk_stack_set_visible_child_name(priv->content_stack, "reader");
    }
}

static GtkWidget *
gsword_window_make_module_row(GswModuleInfo *info)
{
    const gchar *name = gsw_module_info_get_name(info);
    const gchar *description = gsw_module_info_get_description(info);
    const gchar *type = gsw_module_info_get_type_string(info);
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
    return row;
}

static void
gsword_window_populate_modules(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GPtrArray *infos;
    gchar *wanted = NULL;
    GtkListBoxRow *selected = NULL;
    GtkListBoxRow *first = NULL;
    guint i;

    if (priv->manager == NULL) {
        gtk_stack_set_visible_child_name(priv->content_stack, "reader");
        gsword_window_refresh_reader(self);
        return;
    }

    infos = gsw_manager_get_module_infos(priv->manager);
    if (priv->settings != NULL)
        wanted = g_settings_get_string(priv->settings, "last-module");

    while (gtk_widget_get_first_child(GTK_WIDGET(priv->module_list)) != NULL) {
        GtkWidget *child = gtk_widget_get_first_child(
            GTK_WIDGET(priv->module_list));

        gtk_list_box_remove(priv->module_list, child);
    }

    {
        GtkStringList *names = gtk_string_list_new(NULL);
        gchar *parallel_a = priv->settings != NULL
            ? g_settings_get_string(priv->settings, "parallel-a") : NULL;
        gchar *parallel_b = priv->settings != NULL
            ? g_settings_get_string(priv->settings, "parallel-b") : NULL;
        guint selected_a = 0;
        guint selected_b = infos->len > 1 ? 1 : 0;

        for (i = 0; i < infos->len; i++) {
            GswModuleInfo *info = g_ptr_array_index(infos, i);
            GtkWidget *row = gsword_window_make_module_row(info);
            const gchar *name = gsw_module_info_get_name(info);

            gtk_string_list_append(names, name);
            gtk_list_box_append(priv->module_list, row);
            if (first == NULL)
                first = GTK_LIST_BOX_ROW(row);
            if (wanted != NULL && g_strcmp0(wanted, name) == 0)
                selected = GTK_LIST_BOX_ROW(row);
            if (parallel_a != NULL && g_strcmp0(parallel_a, name) == 0)
                selected_a = i;
            if (parallel_b != NULL && g_strcmp0(parallel_b, name) == 0)
                selected_b = i;
        }

        g_clear_object(&priv->module_names);
        priv->module_names = names;
        gtk_drop_down_set_model(priv->parallel_a, G_LIST_MODEL(names));
        gtk_drop_down_set_model(priv->parallel_b, G_LIST_MODEL(names));
        gtk_drop_down_set_selected(priv->parallel_a, selected_a);
        gtk_drop_down_set_selected(priv->parallel_b, selected_b);
        g_free(parallel_a);
        g_free(parallel_b);
    }
    g_ptr_array_unref(infos);
    g_free(wanted);

    if (selected == NULL)
        selected = first;
    if (selected != NULL) {
        gtk_list_box_select_row(priv->module_list, selected);
    } else {
        gtk_stack_set_visible_child_name(priv->content_stack, "reader");
        gsword_window_refresh_reader(self);
    }
}

static void
on_key_activate(GtkEntry *entry, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);

    gsword_window_set_key_text(self, gtk_editable_get_text(GTK_EDITABLE(entry)));
}

static void
on_go_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    (void)button;
    gsword_window_set_key_text(
        self, gtk_editable_get_text(GTK_EDITABLE(priv->key_entry)));
}

static void
on_chapter_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gint direction = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button),
                                                       "direction"));
    GswVerseKey *key;
    gint chapter;
    gint maximum;
    GError *error = NULL;

    if (priv->module == NULL ||
        gsw_key_get_kind(gsw_module_get_key(priv->module)) !=
            GSW_KEY_KIND_VERSE)
        return;
    key = (GswVerseKey *)gsw_module_get_key(priv->module);
    chapter = gsw_verse_key_get_chapter(key);
    maximum = gsw_verse_key_get_chapter_max(key);
    if (chapter < 1 || maximum < 1)
        return;
    if ((direction < 0 && chapter <= 1) ||
        (direction > 0 && chapter >= maximum))
        return;

    gsw_verse_key_set_chapter(key, chapter + direction);
    if (!gsw_module_set_key_text(priv->module,
                                 gsw_module_get_key_text(priv->module),
                                 &error)) {
        gsword_window_show_error(self, "Cannot change chapter", error);
        return;
    }
    gsword_window_refresh_reader(self);
}

static void
on_copy_clicked(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(priv->reader);
    GtkTextIter start;
    GtkTextIter end;
    gchar *text;
    GdkClipboard *clipboard;

    (void)button;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    clipboard = gtk_widget_get_clipboard(GTK_WIDGET(self));
    gdk_clipboard_set_text(clipboard, text != NULL ? text : "");
    g_free(text);
    gsword_window_set_status(self, "Copied to clipboard");
}

static void
on_font_scale_changed(GtkSpinButton *spin, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    priv->font_scale = gtk_spin_button_get_value(spin);
    if (priv->settings != NULL)
        g_settings_set_double(priv->settings, "font-scale", priv->font_scale);
    gsword_window_apply_css(self);
}

static void
on_theme_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint selected = adw_combo_row_get_selected(ADW_COMBO_ROW(object));

    (void)pspec;
    g_free(priv->theme);
    priv->theme = g_strdup(selected == 1 ? "light"
                          : selected == 2 ? "dark" : "system");
    if (priv->settings != NULL)
        g_settings_set_string(priv->settings, "theme", priv->theme);
    gsword_window_apply_theme(self);
}

static void
gsword_window_show_preferences(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    AdwPreferencesDialog *dialog;
    AdwPreferencesPage *page;
    AdwPreferencesGroup *group;
    AdwActionRow *font_row;
    AdwComboRow *theme_row;
    GtkAdjustment *adjustment;
    GtkWidget *spin;
    GtkStringList *themes;
    guint selected = 0;

    if (priv->preferences_dialog != NULL) {
        adw_dialog_present(priv->preferences_dialog, GTK_WIDGET(self));
        return;
    }

    dialog = ADW_PREFERENCES_DIALOG(adw_preferences_dialog_new());
    adw_dialog_set_title(ADW_DIALOG(dialog), "Preferences");
    adw_dialog_set_content_width(ADW_DIALOG(dialog), 620);

    page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
    group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(group, "Reading");
    adw_preferences_group_set_description(
        group, "Reading appearance is applied immediately.");

    font_row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(font_row),
                                  "Font size");
    adw_action_row_set_subtitle(font_row, "Scale the scripture text size");
    adjustment = gtk_adjustment_new(priv->font_scale, 0.75, 2.5, 0.1, 0.25, 0);
    spin = gtk_spin_button_new(adjustment, 0.1, 1);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    g_signal_connect(spin, "value-changed",
                     G_CALLBACK(on_font_scale_changed), self);
    adw_action_row_add_suffix(font_row, spin);
    adw_action_row_set_activatable_widget(font_row, spin);

    theme_row = ADW_COMBO_ROW(adw_combo_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(theme_row),
                                  "Color scheme");
    adw_action_row_set_subtitle(ADW_ACTION_ROW(theme_row),
                                "Follow the desktop or force light/dark mode");
    themes = gtk_string_list_new(
        (const gchar *[]) { "Follow system", "Light", "Dark", NULL });
    adw_combo_row_set_model(theme_row, G_LIST_MODEL(themes));
    g_object_unref(themes);
    if (g_strcmp0(priv->theme, "light") == 0)
        selected = 1;
    else if (g_strcmp0(priv->theme, "dark") == 0)
        selected = 2;
    adw_combo_row_set_selected(theme_row, selected);
    g_signal_connect(theme_row, "notify::selected",
                     G_CALLBACK(on_theme_changed), self);

    adw_preferences_group_add(group, GTK_WIDGET(font_row));
    adw_preferences_group_add(group, GTK_WIDGET(theme_row));
    adw_preferences_page_add(page, group);
    adw_preferences_dialog_add(dialog, page);

    priv->preferences_dialog = ADW_DIALOG(g_object_ref_sink(dialog));
    adw_dialog_present(priv->preferences_dialog, GTK_WIDGET(self));
}

static void
on_preferences_action(GSimpleAction *action, GVariant *parameter,
                      gpointer user_data)
{
    (void)action;
    (void)parameter;
    gsword_window_show_preferences(GSWORD_WINDOW(user_data));
}

static void
on_focus_key_action(GSimpleAction *action, GVariant *parameter,
                    gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    (void)action;
    (void)parameter;
    gtk_widget_grab_focus(GTK_WIDGET(priv->key_entry));
}

static GtkWidget *
gsword_window_make_sidebar(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *label = gtk_label_new("Modules");
    GtkWidget *scrolled = gtk_scrolled_window_new();

    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "heading");
    gtk_widget_set_margin_top(label, 10);
    gtk_widget_set_margin_start(label, 12);
    gtk_widget_set_margin_end(label, 12);

    priv->module_list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_widget_add_css_class(GTK_WIDGET(priv->module_list),
                             "navigation-sidebar");
    gtk_list_box_set_selection_mode(priv->module_list, GTK_SELECTION_SINGLE);
    g_signal_connect(priv->module_list, "row-selected",
                     G_CALLBACK(on_module_row_selected), self);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(priv->module_list));
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), scrolled);
    return box;
}

static GtkWidget *
gsword_window_make_scrolled_text(GtkTextView **view)
{
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkWidget *text = gtk_text_view_new();

    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 10);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(text), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(text), 10);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text);
    *view = GTK_TEXT_VIEW(text);
    return scrolled;
}

static void
on_study_toggled(GtkToggleButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    gtk_widget_set_visible(priv->study_panel,
                           gtk_toggle_button_get_active(button));
}

static void
on_workspace_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *name = gtk_stack_get_visible_child_name(
        GTK_STACK(object));

    (void)pspec;
    if (priv->settings != NULL && name != NULL)
        g_settings_set_string(priv->settings, "active-page", name);
    if (g_strcmp0(name, "parallel") == 0 && priv->parallel_key != NULL) {
        if (priv->module != NULL)
            gtk_editable_set_text(
                GTK_EDITABLE(priv->parallel_key),
                gsw_module_get_key_text(priv->module));
        gsword_window_parallel_load(self);
    } else if (g_strcmp0(name, "search") == 0) {
        gsword_window_update_index_label(self);
    }
}

static void
gsword_window_restore_page(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    gchar *page = NULL;

    if (priv->settings != NULL)
        page = g_settings_get_string(priv->settings, "active-page");
    if (page != NULL &&
        (g_strcmp0(page, "reader") == 0 ||
         g_strcmp0(page, "parallel") == 0 ||
         g_strcmp0(page, "search") == 0))
        gtk_stack_set_visible_child_name(priv->content_stack, page);
    g_free(page);
}

static void
on_lexicon_lookup(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_lexicon_lookup(GSWORD_WINDOW(user_data));
}

static GtkWidget *
gsword_window_make_study_panel(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *notebook = gtk_notebook_new();
    GtkWidget *commentary;
    GtkWidget *strongs;
    GtkWidget *crossrefs;
    GtkWidget *lexicon_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *lexicon_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *lexicon_button = gtk_button_new_with_label("Lookup");
    GtkWidget *lexicon;

    commentary = gsword_window_make_scrolled_text(&priv->commentary_view);
    strongs = gsword_window_make_scrolled_text(&priv->strongs_view);
    crossrefs = gsword_window_make_scrolled_text(&priv->crossref_view);
    lexicon = gsword_window_make_scrolled_text(&priv->lexicon_view);
    priv->lexicon_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->lexicon_entry, "Strong's / dictionary key");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->lexicon_entry), TRUE);
    g_signal_connect(priv->lexicon_entry, "activate",
                     G_CALLBACK(on_lexicon_lookup), self);
    g_signal_connect(lexicon_button, "clicked",
                     G_CALLBACK(on_lexicon_lookup), self);
    gtk_box_append(GTK_BOX(lexicon_controls), GTK_WIDGET(priv->lexicon_entry));
    gtk_box_append(GTK_BOX(lexicon_controls), lexicon_button);
    gtk_box_append(GTK_BOX(lexicon_box), lexicon_controls);
    gtk_widget_set_vexpand(lexicon, TRUE);
    gtk_box_append(GTK_BOX(lexicon_box), lexicon);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), commentary,
                             gtk_label_new("Commentary"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), strongs,
                             gtk_label_new("Strong's"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), crossrefs,
                             gtk_label_new("Cross-refs"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), lexicon_box,
                             gtk_label_new("Lexicon"));
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    gtk_widget_set_size_request(notebook, 300, -1);
    priv->study_notebook = GTK_NOTEBOOK(notebook);
    priv->study_panel = notebook;
    return notebook;
}

static GtkWidget *
gsword_window_make_reader(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *reader_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *previous = gtk_button_new_from_icon_name(
        "go-previous-symbolic");
    GtkWidget *next = gtk_button_new_from_icon_name("go-next-symbolic");
    GtkWidget *go = gtk_button_new_with_label("Go");
    GtkWidget *copy = gtk_button_new_from_icon_name("edit-copy-symbolic");
    GtkWidget *study_button = gtk_toggle_button_new();
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *study_panel = gsword_window_make_study_panel(self);

    gtk_widget_set_margin_top(toolbar, 8);
    gtk_widget_set_margin_bottom(toolbar, 8);
    gtk_widget_set_margin_start(toolbar, 12);
    gtk_widget_set_margin_end(toolbar, 12);

    priv->key_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->key_entry,
                                   "Genesis 1:1 / 启示录 1:1");
    gtk_editable_set_width_chars(GTK_EDITABLE(priv->key_entry), 24);
    gtk_widget_set_hexpand(GTK_WIDGET(priv->key_entry), TRUE);
    g_signal_connect(priv->key_entry, "activate",
                     G_CALLBACK(on_key_activate), self);
    g_signal_connect(go, "clicked", G_CALLBACK(on_go_clicked), self);

    g_object_set_data(G_OBJECT(previous), "direction", GINT_TO_POINTER(-1));
    g_object_set_data(G_OBJECT(next), "direction", GINT_TO_POINTER(1));
    g_signal_connect(previous, "clicked", G_CALLBACK(on_chapter_clicked),
                     self);
    g_signal_connect(next, "clicked", G_CALLBACK(on_chapter_clicked), self);
    g_signal_connect(copy, "clicked", G_CALLBACK(on_copy_clicked), self);
    gtk_button_set_icon_name(GTK_BUTTON(study_button),
                             "dialog-information-symbolic");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(study_button), TRUE);
    g_signal_connect(study_button, "toggled",
                     G_CALLBACK(on_study_toggled), self);
    gtk_widget_set_tooltip_text(previous, "Previous chapter");
    gtk_widget_set_tooltip_text(next, "Next chapter");
    gtk_widget_set_tooltip_text(copy, "Copy entry");
    gtk_widget_set_tooltip_text(study_button, "Toggle study panel");

    gtk_box_append(GTK_BOX(toolbar), previous);
    gtk_box_append(GTK_BOX(toolbar), next);
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(priv->key_entry));
    gtk_box_append(GTK_BOX(toolbar), go);
    gtk_box_append(GTK_BOX(toolbar), study_button);
    gtk_box_append(GTK_BOX(toolbar), copy);

    priv->reader = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(priv->reader, FALSE);
    gtk_text_view_set_cursor_visible(priv->reader, FALSE);
    gtk_text_view_set_wrap_mode(priv->reader, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(priv->reader, 28);
    gtk_text_view_set_right_margin(priv->reader, 28);
    gtk_text_view_set_top_margin(priv->reader, 22);
    gtk_text_view_set_bottom_margin(priv->reader, 22);
    gtk_widget_add_css_class(GTK_WIDGET(priv->reader), "gsw-reader");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(priv->reader));
    gtk_widget_set_vexpand(content_box, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(study_panel, FALSE);
    gtk_widget_set_size_request(study_panel, 320, -1);
    gtk_box_append(GTK_BOX(content_box), scrolled);
    gtk_box_append(GTK_BOX(content_box), study_panel);

    gtk_box_append(GTK_BOX(reader_box), toolbar);
    gtk_box_append(GTK_BOX(reader_box), content_box);
    return reader_box;
}

static const gchar *
gsword_window_dropdown_name(GswordWindow *self, GtkDropDown *dropdown)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    guint selected = gtk_drop_down_get_selected(dropdown);

    if (priv->module_names == NULL || selected == GTK_INVALID_LIST_POSITION)
        return NULL;
    return gtk_string_list_get_string(priv->module_names, selected);
}

static void
gsword_window_set_initial_key_for_module(GswModule *module)
{
    if (module == NULL)
        return;
    if (gsw_key_get_kind(gsw_module_get_key(module)) == GSW_KEY_KIND_VERSE) {
        if (gsw_module_set_key_text(module, "Gen 1:1", NULL))
            return;
        if (gsw_module_set_key_text(module, "Matt 1:1", NULL))
            return;
    }
    gsw_module_set_position(module, GSW_POSITION_TOP, NULL);
}

static void
gsword_window_parallel_set_text(GtkTextView *view, GswModule *module,
                                const gchar *key_text)
{
    GError *error = NULL;
    gchar *text;

    if (module == NULL) {
        gsword_window_set_view_text(view, "No module selected.");
        return;
    }
    if (key_text != NULL && *key_text != '\0' &&
        !gsw_module_set_key_text(module, key_text, &error)) {
        g_clear_error(&error);
        gsword_window_set_initial_key_for_module(module);
    } else if (key_text == NULL || *key_text == '\0') {
        gsword_window_set_initial_key_for_module(module);
    }

    text = gsword_window_chapter_text_for_module(module);
    if (text == NULL)
        text = gsw_module_get_text(module, &error);
    if (text == NULL) {
        gsword_window_set_view_text(view, "Cannot read this entry.");
        g_clear_error(&error);
        return;
    }
    gsword_window_set_view_text(view, text);
    g_free(text);
}

static void
gsword_window_parallel_load(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *name_a = gsword_window_dropdown_name(self, priv->parallel_a);
    const gchar *name_b = gsword_window_dropdown_name(self, priv->parallel_b);
    const gchar *entry_text = gtk_editable_get_text(
        GTK_EDITABLE(priv->parallel_key));
    gchar *key_text = entry_text != NULL && *entry_text != '\0'
                    ? g_strdup(entry_text)
                    : g_strdup(priv->module != NULL
                               ? gsw_module_get_key_text(priv->module)
                               : "Gen 1:1");

    if (key_text != NULL)
        gtk_editable_set_text(GTK_EDITABLE(priv->parallel_key), key_text);

    g_clear_object(&priv->parallel_module_a);
    g_clear_object(&priv->parallel_module_b);
    if (name_a != NULL)
        priv->parallel_module_a = gsw_manager_create_module(priv->manager,
                                                           name_a);
    if (name_b != NULL)
        priv->parallel_module_b = gsw_manager_create_module(priv->manager,
                                                           name_b);
    gsword_window_parallel_set_text(priv->parallel_view_a,
                                   priv->parallel_module_a, key_text);
    gsword_window_parallel_set_text(priv->parallel_view_b,
                                   priv->parallel_module_b, key_text);

    if (priv->settings != NULL) {
        if (name_a != NULL)
            g_settings_set_string(priv->settings, "parallel-a", name_a);
        if (name_b != NULL)
            g_settings_set_string(priv->settings, "parallel-b", name_b);
    }
    g_free(key_text);
}

static void
on_parallel_load(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_parallel_load(GSWORD_WINDOW(user_data));
}

static void
gsword_window_sync_adjustment(GtkAdjustment *from, GtkAdjustment *to)
{
    gdouble from_span = gtk_adjustment_get_upper(from) -
                        gtk_adjustment_get_page_size(from);
    gdouble to_span = gtk_adjustment_get_upper(to) -
                      gtk_adjustment_get_page_size(to);
    gdouble fraction = from_span > 0
                     ? gtk_adjustment_get_value(from) / from_span : 0.0;

    gtk_adjustment_set_value(to, fraction * MAX(to_span, 0.0));
}

static void
on_parallel_scroll_a(GtkAdjustment *adjustment, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->syncing_scroll ||
        !gtk_check_button_get_active(priv->parallel_sync))
        return;
    priv->syncing_scroll = TRUE;
    gsword_window_sync_adjustment(
        adjustment,
        gtk_scrolled_window_get_vadjustment(priv->parallel_scroll_b));
    priv->syncing_scroll = FALSE;
}

static void
on_parallel_scroll_b(GtkAdjustment *adjustment, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->syncing_scroll ||
        !gtk_check_button_get_active(priv->parallel_sync))
        return;
    priv->syncing_scroll = TRUE;
    gsword_window_sync_adjustment(
        adjustment,
        gtk_scrolled_window_get_vadjustment(priv->parallel_scroll_a));
    priv->syncing_scroll = FALSE;
}

static GtkWidget *
gsword_window_make_parallel_page(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *module_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *key_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *load = gtk_button_new_with_label("Load both");
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *scrolled_a;
    GtkWidget *scrolled_b;

    gtk_widget_set_margin_top(controls, 8);
    gtk_widget_set_margin_bottom(controls, 8);
    gtk_widget_set_margin_start(controls, 12);
    gtk_widget_set_margin_end(controls, 12);

    priv->parallel_a = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    priv->parallel_b = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    priv->parallel_key = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->parallel_key,
                                   "Shared chapter/key");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->parallel_key), TRUE);
    priv->parallel_sync = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label("Sync scroll"));
    gtk_check_button_set_active(priv->parallel_sync, TRUE);
    g_signal_connect(load, "clicked", G_CALLBACK(on_parallel_load), self);
    g_signal_connect(priv->parallel_key, "activate",
                     G_CALLBACK(on_parallel_load), self);

    gtk_box_append(GTK_BOX(module_row), GTK_WIDGET(priv->parallel_a));
    gtk_box_append(GTK_BOX(module_row), GTK_WIDGET(priv->parallel_b));
    gtk_box_append(GTK_BOX(module_row), load);
    gtk_widget_set_hexpand(GTK_WIDGET(priv->parallel_key), TRUE);
    gtk_box_append(GTK_BOX(key_row), GTK_WIDGET(priv->parallel_key));
    gtk_box_append(GTK_BOX(key_row), GTK_WIDGET(priv->parallel_sync));
    gtk_box_append(GTK_BOX(controls), module_row);
    gtk_box_append(GTK_BOX(controls), key_row);

    scrolled_a = gsword_window_make_scrolled_text(&priv->parallel_view_a);
    scrolled_b = gsword_window_make_scrolled_text(&priv->parallel_view_b);
    priv->parallel_scroll_a = GTK_SCROLLED_WINDOW(scrolled_a);
    priv->parallel_scroll_b = GTK_SCROLLED_WINDOW(scrolled_b);
    g_signal_connect(gtk_scrolled_window_get_vadjustment(
                         priv->parallel_scroll_a),
                     "value-changed", G_CALLBACK(on_parallel_scroll_a), self);
    g_signal_connect(gtk_scrolled_window_get_vadjustment(
                         priv->parallel_scroll_b),
                     "value-changed", G_CALLBACK(on_parallel_scroll_b), self);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_paned_set_start_child(GTK_PANED(paned), scrolled_a);
    gtk_paned_set_end_child(GTK_PANED(paned), scrolled_b);

    gtk_box_append(GTK_BOX(box), controls);
    gtk_box_append(GTK_BOX(box), paned);
    return box;
}

static void
gsword_window_run_search(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GswSearchOptions *options;
    GPtrArray *results;
    GError *error = NULL;
    guint i;
    guint selected = gtk_drop_down_get_selected(priv->search_type);

    if (priv->module == NULL) {
        gsword_window_set_status(self, "Open a module before searching");
        return;
    }
    while (gtk_widget_get_first_child(GTK_WIDGET(priv->search_results)) != NULL)
        gtk_list_box_remove(priv->search_results,
                            gtk_widget_get_first_child(
                                GTK_WIDGET(priv->search_results)));

    options = gsw_search_options_new();
    gsw_search_options_set_text(
        options, gtk_editable_get_text(GTK_EDITABLE(priv->search_entry)));
    gsw_search_options_set_search_type(
        options, selected == 1 ? GSW_SEARCH_MULTIWORD
                             : selected == 2 ? GSW_SEARCH_REGEX
                                             : GSW_SEARCH_PHRASE);
    gsw_search_options_set_case_sensitive(
        options, gtk_check_button_get_active(priv->search_case));
    gsw_search_options_set_max_results(options, 200);
    results = gsw_module_search_full(priv->module, options, NULL, NULL, NULL,
                                     &error);
    gsw_search_options_unref(options);
    if (results == NULL) {
        gsword_window_show_error(self, "Search failed", error);
        return;
    }

    for (i = 0; i < results->len; i++) {
        GswSearchResult *result = g_ptr_array_index(results, i);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label;
        gchar *text = g_strdup_printf("%s\n%s",
            gsw_search_result_get_key(result),
            gsw_search_result_get_snippet(result));

        label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_set_margin_top(label, 4);
        gtk_widget_set_margin_bottom(label, 4);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data_full(G_OBJECT(row), "result-module",
                               g_strdup(gsw_search_result_get_module_name(result)),
                               g_free);
        g_object_set_data_full(G_OBJECT(row), "result-key",
                               g_strdup(gsw_search_result_get_key(result)),
                               g_free);
        gtk_list_box_append(priv->search_results, row);
        g_free(text);
    }
    {
        gchar *message = g_strdup_printf("Search: %u result(s)", results->len);

        gsword_window_set_status(self, message);
        g_free(message);
    }
    g_ptr_array_unref(results);
}

static void
on_search_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    gsword_window_run_search(GSWORD_WINDOW(user_data));
}

static void
on_search_activate(GtkEntry *entry, gpointer user_data)
{
    (void)entry;
    gsword_window_run_search(GSWORD_WINDOW(user_data));
}

static void
on_search_result_selected(GtkListBox *list, GtkListBoxRow *row,
                          gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    const gchar *module_name;
    const gchar *key;

    (void)list;
    if (row == NULL)
        return;
    module_name = g_object_get_data(G_OBJECT(row), "result-module");
    key = g_object_get_data(G_OBJECT(row), "result-key");
    if (module_name != NULL && g_strcmp0(module_name,
                                         gsw_module_get_name(priv->module)) != 0)
        gsword_window_load_module(self, module_name);
    if (key != NULL)
        gsword_window_set_key_text(self, key);
    gtk_stack_set_visible_child_name(priv->content_stack, "reader");
}

static void
gsword_window_index_progress(const gchar *module_name, guint processed,
                             guint total, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    gchar *message = g_strdup_printf("Indexing %s: %u/%u",
                                     module_name, processed, total);

    gsword_window_set_status(self, message);
    g_free(message);
    while (g_main_context_iteration(NULL, FALSE))
        ;
}

static void
gsword_window_update_index_label(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    if (priv->module == NULL)
        gtk_label_set_text(priv->index_label, "Index: no module");
    else
        gtk_label_set_text(priv->index_label,
            gsw_module_has_search_index(priv->module)
            ? "Index: available" : "Index: not built");
}

static void
on_build_index(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;

    (void)button;
    if (priv->module == NULL)
        return;
    if (!gsw_module_build_search_index(priv->module, NULL,
                                       gsword_window_index_progress, self,
                                       &error)) {
        gsword_window_show_error(self, "Cannot build index", error);
        return;
    }
    gsword_window_set_status(self, "Search index built");
    gsword_window_update_index_label(self);
}

static void
on_delete_index(GtkButton *button, gpointer user_data)
{
    GswordWindow *self = GSWORD_WINDOW(user_data);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GError *error = NULL;

    (void)button;
    if (priv->module == NULL)
        return;
    if (!gsw_module_delete_search_index(priv->module, &error)) {
        gsword_window_show_error(self, "Cannot delete index", error);
        return;
    }
    gsword_window_set_status(self, "Search index deleted");
    gsword_window_update_index_label(self);
}

static GtkWidget *
gsword_window_make_search_page(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *search_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *options_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *search = gtk_button_new_with_label("Search");
    GtkWidget *build = gtk_button_new_from_icon_name(
        "document-save-symbolic");
    GtkWidget *delete = gtk_button_new_from_icon_name("edit-delete-symbolic");
    GtkWidget *scrolled = gtk_scrolled_window_new();
    GtkStringList *types;

    gtk_widget_set_margin_top(controls, 8);
    gtk_widget_set_margin_bottom(controls, 8);
    gtk_widget_set_margin_start(controls, 12);
    gtk_widget_set_margin_end(controls, 12);
    gtk_widget_set_tooltip_text(build, "Build search index");
    gtk_widget_set_tooltip_text(delete, "Delete search index");

    priv->search_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(priv->search_entry,
                                   "Search text");
    gtk_widget_set_hexpand(GTK_WIDGET(priv->search_entry), TRUE);
    priv->search_type = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    types = gtk_string_list_new(
        (const gchar *[]) { "Phrase", "Multiword", "Regex", NULL });
    gtk_drop_down_set_model(priv->search_type, G_LIST_MODEL(types));
    g_object_unref(types);
    priv->search_case = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label("Case sensitive"));
    priv->index_label = GTK_LABEL(gtk_label_new("Index: no module"));
    gtk_label_set_xalign(priv->index_label, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(priv->index_label), "dim-label");
    gtk_widget_add_css_class(GTK_WIDGET(priv->index_label), "caption");

    g_signal_connect(search, "clicked", G_CALLBACK(on_search_clicked), self);
    g_signal_connect(priv->search_entry, "activate",
                     G_CALLBACK(on_search_activate), self);
    g_signal_connect(build, "clicked", G_CALLBACK(on_build_index), self);
    g_signal_connect(delete, "clicked", G_CALLBACK(on_delete_index), self);

    gtk_box_append(GTK_BOX(search_row), GTK_WIDGET(priv->search_entry));
    gtk_box_append(GTK_BOX(search_row), search);
    gtk_box_append(GTK_BOX(options_row), GTK_WIDGET(priv->search_type));
    gtk_box_append(GTK_BOX(options_row), GTK_WIDGET(priv->search_case));
    gtk_box_append(GTK_BOX(options_row), build);
    gtk_box_append(GTK_BOX(options_row), delete);
    gtk_box_append(GTK_BOX(controls), search_row);
    gtk_box_append(GTK_BOX(controls), options_row);
    gtk_box_append(GTK_BOX(controls), GTK_WIDGET(priv->index_label));

    priv->search_results = GTK_LIST_BOX(gtk_list_box_new());
    g_signal_connect(priv->search_results, "row-selected",
                     G_CALLBACK(on_search_result_selected), self);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_WIDGET(priv->search_results));
    gtk_widget_set_vexpand(scrolled, TRUE);

    gtk_box_append(GTK_BOX(box), controls);
    gtk_box_append(GTK_BOX(box), scrolled);
    return box;
}

static void
gsword_window_build(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);
    AdwToolbarView *toolbar_view;
    AdwHeaderBar *header;
    AdwNavigationSplitView *split;
    AdwNavigationPage *sidebar_page;
    AdwNavigationPage *content_page;
    GtkWidget *workspace_box;
    GtkWidget *switcher_bar;
    GtkWidget *switcher;
    GtkWidget *menu_button;
    GtkWidget *header_title;
    GMenu *menu;
    GSimpleAction *preferences;
    GSimpleAction *focus_key;
    GtkWidget *status;

    priv->css = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(priv->css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_window_set_title(GTK_WINDOW(self), "gsword");
    gtk_window_set_default_size(GTK_WINDOW(self), 1100, 760);

    toolbar_view = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
    header = ADW_HEADER_BAR(adw_header_bar_new());
    header_title = adw_window_title_new("gsword", "SWORD study");
    adw_header_bar_set_title_widget(header, header_title);

    menu = g_menu_new();
    g_menu_append(menu, "Preferences", "win.preferences");
    g_menu_append(menu, "Quit", "app.quit");
    menu_button = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button),
                                  "open-menu-symbolic");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button),
                                   G_MENU_MODEL(menu));
    gtk_widget_set_tooltip_text(menu_button, "Main menu");
    adw_header_bar_pack_end(header, menu_button);
    g_object_unref(menu);

    split = ADW_NAVIGATION_SPLIT_VIEW(adw_navigation_split_view_new());
    adw_navigation_split_view_set_sidebar_width_fraction(split, 0.24);
    sidebar_page = adw_navigation_page_new(gsword_window_make_sidebar(self),
                                           "Library");

    priv->content_stack = GTK_STACK(gtk_stack_new());
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_reader(self),
                         "reader", "Reading");
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_parallel_page(self),
                         "parallel", "Parallel");
    gtk_stack_add_titled(priv->content_stack,
                         gsword_window_make_search_page(self),
                         "search", "Search");
    gtk_stack_set_visible_child_name(priv->content_stack, "reader");
    g_signal_connect(priv->content_stack, "notify::visible-child",
                     G_CALLBACK(on_workspace_changed), self);

    workspace_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    switcher_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    switcher = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher),
                                  priv->content_stack);
    gtk_widget_set_margin_top(switcher, 4);
    gtk_widget_set_margin_bottom(switcher, 4);
    gtk_widget_set_halign(switcher, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(switcher_bar), switcher);
    gtk_widget_set_vexpand(GTK_WIDGET(priv->content_stack), TRUE);
    gtk_box_append(GTK_BOX(workspace_box), switcher_bar);
    gtk_box_append(GTK_BOX(workspace_box),
                   GTK_WIDGET(priv->content_stack));

    content_page = adw_navigation_page_new(workspace_box, "Workspace");
    adw_navigation_split_view_set_sidebar(split, sidebar_page);
    adw_navigation_split_view_set_content(split, content_page);

    adw_toolbar_view_add_top_bar(toolbar_view, GTK_WIDGET(header));
    adw_toolbar_view_set_content(toolbar_view, GTK_WIDGET(split));

    status = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(status), 0.0f);
    gtk_widget_add_css_class(status, "caption");
    gtk_widget_add_css_class(status, "dim-label");
    gtk_widget_set_margin_top(status, 4);
    gtk_widget_set_margin_bottom(status, 4);
    gtk_widget_set_margin_start(status, 10);
    gtk_widget_set_margin_end(status, 10);
    adw_toolbar_view_add_bottom_bar(toolbar_view, status);
    priv->status_label = GTK_LABEL(status);

    adw_application_window_set_content(ADW_APPLICATION_WINDOW(self),
                                       GTK_WIDGET(toolbar_view));

    preferences = g_simple_action_new("preferences", NULL);
    g_signal_connect(preferences, "activate",
                     G_CALLBACK(on_preferences_action), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(preferences));
    g_object_unref(preferences);

    focus_key = g_simple_action_new("focus-key", NULL);
    g_signal_connect(focus_key, "activate",
                     G_CALLBACK(on_focus_key_action), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(focus_key));
    g_object_unref(focus_key);

    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.preferences",
        (const gchar * const[]) { "<Control>comma", NULL });
    gtk_application_set_accels_for_action(
        gtk_window_get_application(GTK_WINDOW(self)), "win.focus-key",
        (const gchar * const[]) { "<Control>l", NULL });
}

static void
gsword_window_load_settings(GswordWindow *self)
{
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    priv->font_scale = 1.0;
    priv->theme = g_strdup("system");
    if (priv->settings != NULL) {
        priv->font_scale = g_settings_get_double(priv->settings, "font-scale");
        if (priv->font_scale <= 0.0)
            priv->font_scale = 1.0;
        g_free(priv->theme);
        priv->theme = g_settings_get_string(priv->settings, "theme");
        if (priv->theme == NULL || *priv->theme == '\0') {
            g_free(priv->theme);
            priv->theme = g_strdup("system");
        }
    }
    gsword_window_apply_css(self);
    gsword_window_apply_theme(self);
}

static void
gsword_window_finalize(GObject *object)
{
    GswordWindow *self = GSWORD_WINDOW(object);
    GswordWindowPrivate *priv = gsword_window_get_instance_private(self);

    g_clear_object(&priv->module);
    g_clear_object(&priv->settings);
    g_clear_object(&priv->preferences_dialog);
    g_clear_pointer(&priv->css, g_object_unref);
    g_free(priv->theme);

    G_OBJECT_CLASS(gsword_window_parent_class)->finalize(object);
}

static void
gsword_window_class_init(GswordWindowClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = gsword_window_finalize;
}

static void
gsword_window_init(GswordWindow *self)
{
    gsword_window_get_instance_private(self);
}

GtkWidget *
gsword_window_new(GtkApplication *application, GswManager *manager,
                  GSettings *settings)
{
    GswordWindow *self;
    GswordWindowPrivate *priv;

    g_return_val_if_fail(GTK_IS_APPLICATION(application), NULL);
    self = g_object_new(GSWORD_TYPE_WINDOW, "application", application, NULL);
    priv = gsword_window_get_instance_private(self);
    priv->manager = manager;
    priv->settings = settings != NULL ? g_object_ref(settings) : NULL;

    gsword_window_build(self);
    gsword_window_load_settings(self);
    gsword_window_populate_modules(self);
    gsword_window_restore_page(self);
    return GTK_WIDGET(self);
}
