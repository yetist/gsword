/*
 * gsw-search.c: options, results, and module search engine
 */

#include <gsword/gsw-search.h>
#include <gsword/gsw-error.h>
#include <gsword/gsw-verse-list.h>

#include <string.h>

struct _GswSearchOptions {
    gatomicrefcount ref_count;
    gchar *text;
    GswSearchType type;
    gboolean case_sensitive;
    guint max_results;
    GswVerseList *scope;
    gchar *attribute_level1;
    gchar *attribute_level2;
};

struct _GswSearchResult {
    gatomicrefcount ref_count;
    gchar *module_name;
    gchar *key;
    gchar *preview;
    gchar *snippet;
};

GType
gsw_search_options_get_type(void)
{
    static gsize type = 0;

    if (g_once_init_enter(&type)) {
        GType registered = g_boxed_type_register_static(
            "GswSearchOptions", (GBoxedCopyFunc)gsw_search_options_ref,
            (GBoxedFreeFunc)gsw_search_options_unref);
        g_once_init_leave(&type, registered);
    }
    return (GType)type;
}

GType
gsw_search_result_get_type(void)
{
    static gsize type = 0;

    if (g_once_init_enter(&type)) {
        GType registered = g_boxed_type_register_static(
            "GswSearchResult", (GBoxedCopyFunc)gsw_search_result_ref,
            (GBoxedFreeFunc)gsw_search_result_unref);
        g_once_init_leave(&type, registered);
    }
    return (GType)type;
}

GswSearchOptions *
gsw_search_options_new(void)
{
    GswSearchOptions *self = g_new0(GswSearchOptions, 1);

    g_atomic_ref_count_init(&self->ref_count);
    self->type = GSW_SEARCH_PHRASE;
    return self;
}

GswSearchOptions *
gsw_search_options_ref(GswSearchOptions *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_atomic_ref_count_inc(&self->ref_count);
    return self;
}

void
gsw_search_options_unref(GswSearchOptions *self)
{
    if (self == NULL)
        return;
    if (g_atomic_ref_count_dec(&self->ref_count)) {
        g_free(self->text);
        g_free(self->attribute_level1);
        g_free(self->attribute_level2);
        g_clear_pointer(&self->scope, gsw_verse_list_unref);
        g_free(self);
    }
}

void
gsw_search_options_set_text(GswSearchOptions *self, const gchar *text)
{
    g_return_if_fail(self != NULL);
    g_free(self->text);
    self->text = g_strdup(text != NULL ? text : "");
}

const gchar *
gsw_search_options_get_text(GswSearchOptions *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->text != NULL ? self->text : "";
}

void
gsw_search_options_set_search_type(GswSearchOptions *self, GswSearchType type)
{
    g_return_if_fail(self != NULL);
    self->type = type;
}

GswSearchType
gsw_search_options_get_search_type(GswSearchOptions *self)
{
    g_return_val_if_fail(self != NULL, GSW_SEARCH_PHRASE);
    return self->type;
}

void
gsw_search_options_set_case_sensitive(GswSearchOptions *self,
                                      gboolean case_sensitive)
{
    g_return_if_fail(self != NULL);
    self->case_sensitive = case_sensitive;
}

gboolean
gsw_search_options_get_case_sensitive(GswSearchOptions *self)
{
    g_return_val_if_fail(self != NULL, FALSE);
    return self->case_sensitive;
}

void
gsw_search_options_set_max_results(GswSearchOptions *self, guint max_results)
{
    g_return_if_fail(self != NULL);
    self->max_results = max_results;
}

guint
gsw_search_options_get_max_results(GswSearchOptions *self)
{
    g_return_val_if_fail(self != NULL, 0);
    return self->max_results;
}

void
gsw_search_options_set_scope(GswSearchOptions *self, GswVerseList *scope)
{
    g_return_if_fail(self != NULL);
    g_clear_pointer(&self->scope, gsw_verse_list_unref);
    if (scope != NULL)
        self->scope = gsw_verse_list_ref(scope);
}

GswVerseList *
gsw_search_options_get_scope(GswSearchOptions *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->scope;
}

void
gsw_search_options_set_attribute(GswSearchOptions *self, const gchar *level1,
                                 const gchar *level2)
{
    g_return_if_fail(self != NULL);
    g_free(self->attribute_level1);
    g_free(self->attribute_level2);
    self->attribute_level1 = NULL;
    self->attribute_level2 = NULL;
    if (level1 != NULL && *level1 != '\0' && level2 != NULL && *level2 != '\0') {
        self->attribute_level1 = g_strdup(level1);
        self->attribute_level2 = g_strdup(level2);
    }
}

const gchar *
gsw_search_options_get_attribute_level1(GswSearchOptions *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->attribute_level1;
}

const gchar *
gsw_search_options_get_attribute_level2(GswSearchOptions *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->attribute_level2;
}

GswSearchResult *
gsw_search_result_ref(GswSearchResult *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    g_atomic_ref_count_inc(&self->ref_count);
    return self;
}

void
gsw_search_result_unref(GswSearchResult *self)
{
    if (self == NULL)
        return;
    if (g_atomic_ref_count_dec(&self->ref_count)) {
        g_free(self->module_name);
        g_free(self->key);
        g_free(self->preview);
        g_free(self->snippet);
        g_free(self);
    }
}

const gchar *
gsw_search_result_get_module_name(GswSearchResult *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->module_name;
}

const gchar *
gsw_search_result_get_key(GswSearchResult *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->key;
}

const gchar *
gsw_search_result_get_preview(GswSearchResult *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->preview;
}

const gchar *
gsw_search_result_get_snippet(GswSearchResult *self)
{
    g_return_val_if_fail(self != NULL, NULL);
    return self->snippet;
}

static GswSearchResult *
gsw_search_result_new(const gchar *module_name, const gchar *key,
                      const gchar *text)
{
    GswSearchResult *self = g_new0(GswSearchResult, 1);
    gchar *clean = g_strdup(text != NULL ? text : "");
    gchar *preview;

    g_strstrip(clean);
    if (g_utf8_strlen(clean, -1) > 160) {
        gchar *head = g_utf8_substring(clean, 0, 160);
        preview = g_strconcat(head, "...", NULL);
        g_free(head);
    } else {
        preview = g_strdup(clean);
    }
    g_free(clean);

    g_atomic_ref_count_init(&self->ref_count);
    self->module_name = g_strdup(module_name != NULL ? module_name : "");
    self->key = g_strdup(key != NULL ? key : "");
    self->preview = preview;
    self->snippet = g_strdup(preview);
    return self;
}

static gboolean
text_matches(const gchar *text, GswSearchOptions *options, GRegex *regex,
             gchar **words, const gchar *needle)
{
    if (text == NULL)
        return FALSE;

    if (options->type == GSW_SEARCH_REGEX)
        return regex != NULL && g_regex_match(regex, text, 0, NULL);

    if (options->type == GSW_SEARCH_MULTIWORD) {
        gchar *folded = options->case_sensitive ? g_strdup(text)
                                                : g_utf8_casefold(text, -1);
        gboolean match = TRUE;
        guint i;

        for (i = 0; words != NULL && words[i] != NULL; i++) {
            if (strstr(folded, words[i]) == NULL) {
                match = FALSE;
                break;
            }
        }
        g_free(folded);
        return match;
    }

    if (needle == NULL)
        return FALSE;
    if (options->case_sensitive)
        return strstr(text, needle) != NULL;

    {
        gchar *folded = g_utf8_casefold(text, -1);
        gboolean match = strstr(folded, needle) != NULL;

        g_free(folded);
        return match;
    }
}

static gboolean
entry_matches(GswModule *self, GswSearchOptions *options, GRegex *regex,
              gchar **words, const gchar *needle, GError **error)
{
    if (options->attribute_level1 != NULL && options->attribute_level2 != NULL) {
        GPtrArray *keys = gsw_module_get_entry_attribute_keys(
            self, options->attribute_level1, options->attribute_level2, error);
        guint i;
        gboolean match = FALSE;

        if (keys == NULL)
            return FALSE;
        for (i = 0; i < keys->len; i++) {
            gchar *value = gsw_module_get_entry_attribute(
                self, options->attribute_level1, options->attribute_level2,
                g_ptr_array_index(keys, i), error);
            if (value != NULL) {
                if (text_matches(value, options, regex, words, needle))
                    match = TRUE;
                g_free(value);
            }
            if (match)
                break;
        }
        g_ptr_array_unref(keys);
        return match;
    }

    {
        gchar *text = gsw_module_get_text(self, error);
        gboolean match;

        if (text == NULL)
            return FALSE;
        match = text_matches(text, options, regex, words, needle);
        g_free(text);
        return match;
    }
}

static gboolean
search_options_prepare(GswSearchOptions *options, GRegex **regex,
                       gchar ***words, gchar **needle, GError **error)
{
    guint i;

    *regex = NULL;
    *words = NULL;
    *needle = NULL;

    if (options->type == GSW_SEARCH_REGEX) {
        GRegexCompileFlags flags = G_REGEX_OPTIMIZE;

        if (!options->case_sensitive)
            flags |= G_REGEX_CASELESS;
        *regex = g_regex_new(gsw_search_options_get_text(options), flags, 0,
                             error);
        return *regex != NULL;
    }

    if (options->type == GSW_SEARCH_MULTIWORD) {
        gchar **raw = g_strsplit_set(gsw_search_options_get_text(options),
                                     " \t", -1);
        guint count = 0;
        guint n = g_strv_length(raw);
        *words = g_new0(gchar *, n + 1);
        for (i = 0; raw[i] != NULL; i++) {
            if (raw[i][0] == '\0')
                continue;
            (*words)[count++] = options->case_sensitive
                ? g_strdup(raw[i]) : g_utf8_casefold(raw[i], -1);
        }
        g_strfreev(raw);
        return TRUE;
    }

    if (options->type == GSW_SEARCH_PHRASE) {
        *needle = options->case_sensitive
            ? g_strdup(gsw_search_options_get_text(options))
            : g_utf8_casefold(gsw_search_options_get_text(options), -1);
        return TRUE;
    }

    g_set_error(error, GSW_ERROR, GSW_ERROR_UNSUPPORTED,
                "Unsupported search type");
    return FALSE;
}

static void
search_options_cleanup(GRegex *regex, gchar **words, gchar *needle)
{
    guint i;

    g_clear_pointer(&regex, g_regex_unref);
    g_free(needle);
    if (words != NULL) {
        for (i = 0; words[i] != NULL; i++)
            g_free(words[i]);
        g_free(words);
    }
}

GPtrArray *
gsw_module_search_full(GswModule *self, GswSearchOptions *options,
                       GCancellable *cancellable,
                       GswSearchProgressFunc progress, gpointer user_data,
                       GError **error)
{
    GPtrArray *results;
    GswKey *saved;
    GRegex *regex = NULL;
    gchar **words = NULL;
    gchar *needle = NULL;
    gboolean ok = TRUE;
    const gchar *module_name;
    guint processed = 0;
    guint total = 0;
    guint guard = 0;

    g_return_val_if_fail(GSW_IS_MODULE(self), NULL);
    g_return_val_if_fail(options != NULL, NULL);

    if (*gsw_search_options_get_text(options) == '\0')
        return g_ptr_array_new_with_free_func(
            (GDestroyNotify)gsw_search_result_unref);

    if (!search_options_prepare(options, &regex, &words, &needle, error))
        return NULL;

    saved = gsw_key_clone(gsw_module_get_key(self));
    if (saved == NULL) {
        search_options_cleanup(regex, words, needle);
        return NULL;
    }

    module_name = gsw_module_get_name(self);
    results = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsw_search_result_unref);

    if (options->scope != NULL &&
        gsw_key_get_kind(gsw_module_get_key(self)) == GSW_KEY_KIND_VERSE) {
        GPtrArray *keys = gsw_verse_list_expand(options->scope, error);
        guint i;

        if (keys == NULL) {
            ok = FALSE;
            goto out;
        }
        total = keys->len;
        for (i = 0; i < keys->len; i++) {
            GError *local_error = NULL;

            if (g_cancellable_is_cancelled(cancellable)) {
                g_set_error(error, GSW_ERROR, GSW_ERROR_CANCELLED,
                            "Search cancelled");
                ok = FALSE;
                g_ptr_array_unref(keys);
                goto out;
            }
            if (!gsw_module_set_key(self, g_ptr_array_index(keys, i),
                                    error)) {
                ok = FALSE;
                g_ptr_array_unref(keys);
                goto out;
            }
            if (entry_matches(self, options, regex, words, needle,
                              &local_error)) {
                gchar *text = gsw_module_get_text(self, NULL);
                g_ptr_array_add(results, gsw_search_result_new(
                    module_name, gsw_module_get_key_text(self),
                    text != NULL ? text : gsw_module_get_key_text(self)));
                g_free(text);
                if (options->max_results > 0 &&
                    results->len >= options->max_results)
                    break;
            }
            if (local_error != NULL) {
                if (error != NULL)
                    g_propagate_error(error, local_error);
                else
                    g_clear_error(&local_error);
                ok = FALSE;
                g_ptr_array_unref(keys);
                goto out;
            }
            processed++;
            if (progress != NULL)
                progress(module_name, processed, total, user_data);
            if (error != NULL && *error != NULL) {
                ok = FALSE;
                g_ptr_array_unref(keys);
                goto out;
            }
        }
        g_ptr_array_unref(keys);
    } else {
        if (!gsw_module_set_position(self, GSW_POSITION_TOP, error)) {
            ok = FALSE;
            goto out;
        }

        for (;;) {
            gchar *text;
            GError *local_error = NULL;

            if (g_cancellable_is_cancelled(cancellable)) {
                g_set_error(error, GSW_ERROR, GSW_ERROR_CANCELLED,
                            "Search cancelled");
                ok = FALSE;
                goto out;
            }
            if (guard++ >= 1000000)
                break;

            if (entry_matches(self, options, regex, words, needle,
                              &local_error)) {
                text = gsw_module_get_text(self, NULL);
                g_ptr_array_add(results, gsw_search_result_new(
                    module_name, gsw_module_get_key_text(self),
                    text != NULL ? text : gsw_module_get_key_text(self)));
                g_free(text);
                if (options->max_results > 0 &&
                    results->len >= options->max_results)
                    break;
            }
            if (local_error != NULL) {
                if (error != NULL)
                    g_propagate_error(error, local_error);
                else
                    g_clear_error(&local_error);
                ok = FALSE;
                break;
            }
            processed++;
            if (progress != NULL)
                progress(module_name, processed, total, user_data);
            if (error != NULL && *error != NULL) {
                ok = FALSE;
                break;
            }
            {
                GError *advance_error = NULL;
                if (!gsw_module_next(self, 1, &advance_error)) {
                    g_clear_error(&advance_error);
                    break;
                }
            }
        }
    }

out:
    gsw_module_set_key(self, saved, NULL);
    gsw_key_unref(saved);
    search_options_cleanup(regex, words, needle);
    if (!ok) {
        g_ptr_array_unref(results);
        return NULL;
    }
    return results;
}

GPtrArray *
gsw_manager_search(GswManager *self, const gchar * const *module_names,
                   GswSearchOptions *options, GCancellable *cancellable,
                   GswSearchProgressFunc progress, gpointer user_data,
                   GError **error)
{
    GPtrArray *results;
    GPtrArray *names = NULL;
    guint i;

    g_return_val_if_fail(GSW_IS_MANAGER(self), NULL);
    g_return_val_if_fail(options != NULL, NULL);

    results = g_ptr_array_new_with_free_func(
        (GDestroyNotify)gsw_search_result_unref);

    if (module_names == NULL) {
        GPtrArray *infos = gsw_manager_get_module_infos(self);
        names = g_ptr_array_new_with_free_func(g_free);
        for (i = 0; i < infos->len; i++) {
            GswModuleInfo *info = g_ptr_array_index(infos, i);
            g_ptr_array_add(names, g_strdup(gsw_module_info_get_name(info)));
        }
        g_ptr_array_unref(infos);
    } else {
        names = g_ptr_array_new_with_free_func(g_free);
        for (i = 0; module_names[i] != NULL; i++)
            g_ptr_array_add(names, g_strdup(module_names[i]));
    }

    for (i = 0; i < names->len; i++) {
        const gchar *name = g_ptr_array_index(names, i);
        GswModule *module;
        GPtrArray *found;
        guint j;

        if (g_cancellable_is_cancelled(cancellable)) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_CANCELLED,
                        "Search cancelled");
            g_ptr_array_unref(results);
            g_ptr_array_unref(names);
            return NULL;
        }

        module = gsw_manager_create_module(self, name);
        if (module == NULL) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_NOT_FOUND,
                        "Module [%s] not found", name);
            g_ptr_array_unref(results);
            g_ptr_array_unref(names);
            return NULL;
        }
        if (!gsw_module_is_supported(module)) {
            g_object_unref(module);
            continue;
        }
        found = gsw_module_search_full(module, options, cancellable, progress,
                                       user_data, error);
        g_object_unref(module);
        if (found == NULL) {
            g_ptr_array_unref(results);
            g_ptr_array_unref(names);
            return NULL;
        }
        for (j = 0; j < found->len; j++)
            g_ptr_array_add(results,
                            gsw_search_result_ref(g_ptr_array_index(found, j)));
        g_ptr_array_unref(found);
        if (options->max_results > 0 &&
            results->len >= options->max_results)
            break;
    }

    g_ptr_array_unref(names);
    return results;
}
