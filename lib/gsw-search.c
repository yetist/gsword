/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * gsw-search.c: options, results, and module search engine
 */

#include <gsword/gsw-search.h>
#include <gsword/gsw-error.h>
#include <gsword/gsw-verse-list.h>
#include "gsw-lock.h"
#include "gsw-search-private.h"
#include "gsw-utils.h"

#include <errno.h>
#include <glib/gstdio.h>
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

#define GSW_SEARCH_INDEX_MAGIC "gsword-search-index"
#define GSW_SEARCH_INDEX_VERSION 1
#define GSW_SEARCH_INDEX_FILE ".gsword-search-index"
#define GSW_SEARCH_SCAN_GUARD 1000000

typedef struct {
    GHashTable *tokens;   /* case-folded token -> GPtrArray of key strings */
} GswSearchIndex;

static void
search_key_array_free(GPtrArray *array)
{
    if (array != NULL)
        g_ptr_array_unref(array);
}

static GswSearchIndex *
search_index_new(void)
{
    GswSearchIndex *index = g_new0(GswSearchIndex, 1);

    index->tokens = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                          (GDestroyNotify)search_key_array_free);
    return index;
}

static void
search_index_free(GswSearchIndex *index)
{
    if (index == NULL)
        return;
    g_hash_table_unref(index->tokens);
    g_free(index);
}

static gchar *
search_index_path(GswModule *self)
{
    GswModuleInfo *info = gsw_module_get_info(self);
    const gchar *configured = gsw_module_get_search_index_path(self);
    const gchar *data_path;

    if (configured != NULL && *configured != '\0')
        return g_strdup(configured);
    data_path = gsw_module_info_get_absolute_data_path(info);
    if (data_path == NULL || *data_path == '\0')
        return NULL;
    return g_build_filename(data_path, GSW_SEARCH_INDEX_FILE, NULL);
}

static gboolean
search_index_ensure_parent(const gchar *path, GError **error)
{
    gchar *dir = g_path_get_dirname(path);
    gboolean ok = gsw_mkdir_with_parents(dir, error);

    g_free(dir);
    return ok;
}

void
gsw_search_index_remove_for_tree_path(const gchar *tree_path)
{
    gchar *dir;
    gchar *path;
    gchar *tmp;
    gchar *lock_path;
    gint lock_fd = -1;

    if (tree_path == NULL)
        return;
    dir = g_path_get_dirname(tree_path);
    path = g_build_filename(dir, GSW_SEARCH_INDEX_FILE, NULL);
    {
        GError *error = NULL;

        lock_path = g_strconcat(path, ".lock", NULL);
        lock_fd = gsw_file_lock(lock_path, &error);
        g_clear_error(&error);
        g_free(lock_path);
    }
    tmp = g_strconcat(path, ".tmp", NULL);
    g_unlink(path);
    g_unlink(tmp);
    g_free(tmp);
    if (lock_fd >= 0)
        gsw_file_unlock(lock_fd);
    g_free(path);
    g_free(dir);
}

/* Split text into case-folded alphanumeric tokens. CJK runs remain a single
 * token; callers fall back to a full scan when a query token is absent. */
static GPtrArray *
search_tokenize(const gchar *text)
{
    GPtrArray *tokens = g_ptr_array_new_with_free_func(g_free);
    GString *word = g_string_new(NULL);
    gchar *folded;
    const gchar *p;

    if (text == NULL || *text == '\0') {
        g_string_free(word, TRUE);
        return tokens;
    }

    folded = g_utf8_casefold(text, -1);
    p = folded;
    while (*p != '\0') {
        gunichar ch = g_utf8_get_char(p);

        if (g_unichar_isalnum(ch)) {
            g_string_append_unichar(word, ch);
        } else if (word->len > 0) {
            g_ptr_array_add(tokens, g_string_free(word, FALSE));
            word = g_string_new(NULL);
        }
        p = g_utf8_next_char(p);
    }
    if (word->len > 0)
        g_ptr_array_add(tokens, g_string_free(word, FALSE));
    else
        g_string_free(word, TRUE);
    g_free(folded);
    return tokens;
}

static gboolean
search_index_save(GswSearchIndex *index, const gchar *path, GError **error)
{
    GVariantBuilder builder;
    GVariant *tokens;
    GVariant *root;
    GHashTableIter iter;
    gpointer key, value;
    gchar *tmp;
    gboolean ok;

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sas}"));
    g_hash_table_iter_init(&iter, index->tokens);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        GPtrArray *keys = value;
        GVariant *strv = g_variant_new_strv(
            (const gchar * const *)keys->pdata, (gssize)keys->len);

        g_variant_builder_add(&builder, "{s@as}", (const gchar *)key, strv);
    }
    tokens = g_variant_builder_end(&builder);
    root = g_variant_new("(su@a{sas})", GSW_SEARCH_INDEX_MAGIC,
                         (guint32)GSW_SEARCH_INDEX_VERSION, tokens);

    if (!search_index_ensure_parent(path, error)) {
        g_variant_unref(root);
        return FALSE;
    }

    tmp = g_strconcat(path, ".tmp", NULL);
    ok = g_file_set_contents(tmp, g_variant_get_data(root),
                             (gssize)g_variant_get_size(root), error);
    if (ok && g_rename(tmp, path) != 0) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot replace search index %s: %s", path,
                    g_strerror(errno));
        g_unlink(tmp);
        ok = FALSE;
    }
    g_free(tmp);
    g_variant_unref(root);
    return ok;
}

static GswSearchIndex *
search_index_load(const gchar *path, GError **error)
{
    GswSearchIndex *index;
    gchar *data = NULL;
    gsize len = 0;
    GBytes *bytes;
    GVariant *root;
    GVariant *tokens;
    GVariantIter iter;
    gchar *magic = NULL;
    guint32 version = 0;
    gchar *token = NULL;
    GVariant *keys;

    if (!g_file_get_contents(path, &data, &len, NULL))
        return NULL;
    bytes = g_bytes_new_take(data, len);
    root = g_variant_new_from_bytes(G_VARIANT_TYPE("(sua{sas})"), bytes, FALSE);
    g_bytes_unref(bytes);
    if (root == NULL || !g_variant_is_normal_form(root)) {
        if (root != NULL)
            g_variant_unref(root);
        return NULL;
    }

    g_variant_get(root, "(su@a{sas})", &magic, &version, &tokens);
    if (g_strcmp0(magic, GSW_SEARCH_INDEX_MAGIC) != 0 ||
        version != GSW_SEARCH_INDEX_VERSION) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_INVALID_DATA,
                    "Unsupported search index format in %s", path);
        g_free(magic);
        g_variant_unref(tokens);
        g_variant_unref(root);
        return NULL;
    }
    g_free(magic);

    index = search_index_new();
    g_variant_iter_init(&iter, tokens);
    while (g_variant_iter_next(&iter, "{s@as}", &token, &keys)) {
        gsize n = 0;
        gchar **strv = g_variant_dup_strv(keys, &n);
        GPtrArray *array = g_ptr_array_new_with_free_func(g_free);
        gsize i;

        for (i = 0; i < n; i++)
            g_ptr_array_add(array, strv[i]);
        g_free(strv);
        g_hash_table_insert(index->tokens, g_strdup(token), array);
        g_free(token);
        g_variant_unref(keys);
    }
    g_variant_unref(tokens);
    g_variant_unref(root);
    return index;
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

/* Returns NULL when the index cannot answer the query and the caller should
 * fall back to a normal scan. An empty array is a valid "no hits" result. */
static GPtrArray *
search_index_candidates(GswSearchIndex *index, const gchar *text)
{
    GPtrArray *query = search_tokenize(text);
    GPtrArray *arrays = g_ptr_array_new();
    GHashTable *seen = g_hash_table_new(g_str_hash, g_str_equal);
    GPtrArray *result = NULL;
    GPtrArray *base = NULL;
    guint base_index = 0;
    guint i;

    if (query->len == 0)
        goto fallback;
    for (i = 0; i < query->len; i++) {
        const gchar *token = g_ptr_array_index(query, i);
        GPtrArray *keys;

        if (g_hash_table_contains(seen, token))
            continue;
        g_hash_table_add(seen, (gpointer)token);
        keys = g_hash_table_lookup(index->tokens, token);
        if (keys == NULL)
            goto fallback;
        g_ptr_array_add(arrays, keys);
    }
    if (arrays->len == 0)
        goto fallback;

    base = g_ptr_array_index(arrays, 0);
    for (i = 1; i < arrays->len; i++) {
        GPtrArray *candidate = g_ptr_array_index(arrays, i);

        if (candidate->len < base->len) {
            base = candidate;
            base_index = i;
        }
    }

    result = g_ptr_array_new_with_free_func(g_free);
    if (arrays->len == 1) {
        for (i = 0; i < base->len; i++)
            g_ptr_array_add(result, g_strdup(g_ptr_array_index(base, i)));
    } else {
        GHashTable **sets = g_new0(GHashTable *, arrays->len);

        for (i = 0; i < arrays->len; i++) {
            GPtrArray *keys = g_ptr_array_index(arrays, i);
            guint j;

            if (i == base_index)
                continue;
            sets[i] = g_hash_table_new(g_str_hash, g_str_equal);
            for (j = 0; j < keys->len; j++)
                g_hash_table_add(sets[i], g_ptr_array_index(keys, j));
        }
        for (i = 0; i < base->len; i++) {
            const gchar *key = g_ptr_array_index(base, i);
            gboolean match = TRUE;
            guint j;

            for (j = 0; j < arrays->len; j++) {
                if (j != base_index && !g_hash_table_contains(sets[j], key)) {
                    match = FALSE;
                    break;
                }
            }
            if (match)
                g_ptr_array_add(result, g_strdup(key));
        }
        for (i = 0; i < arrays->len; i++)
            g_clear_pointer(&sets[i], g_hash_table_unref);
        g_free(sets);
    }

    g_hash_table_unref(seen);
    g_ptr_array_unref(arrays);
    g_ptr_array_unref(query);
    return result;

fallback:
    g_hash_table_unref(seen);
    g_ptr_array_unref(arrays);
    g_ptr_array_unref(query);
    if (result != NULL)
        g_ptr_array_unref(result);
    return NULL;
}

static gboolean
search_index_can_use(GswSearchOptions *options)
{
    return !options->case_sensitive &&
           options->scope == NULL &&
           options->attribute_level1 == NULL &&
           (options->type == GSW_SEARCH_PHRASE ||
            options->type == GSW_SEARCH_MULTIWORD);
}

static gboolean
search_index_count_entries(GswModule *self, GCancellable *cancellable,
                           guint *count, GError **error)
{
    guint guard = 0;

    *count = 0;
    if (!gsw_module_set_position(self, GSW_POSITION_TOP, error))
        return FALSE;
    for (;;) {
        GError *local_error = NULL;

        if (g_cancellable_is_cancelled(cancellable)) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_CANCELLED,
                        "Index build cancelled");
            return FALSE;
        }
        (*count)++;
        if (guard++ >= GSW_SEARCH_SCAN_GUARD)
            return TRUE;
        if (!gsw_module_next(self, 1, &local_error)) {
            g_clear_error(&local_error);
            break;
        }
    }
    return TRUE;
}

gboolean
gsw_module_has_search_index(GswModule *self)
{
    gchar *path;
    gboolean exists;

    g_return_val_if_fail(GSW_IS_MODULE(self), FALSE);
    path = search_index_path(self);
    if (path == NULL)
        return FALSE;
    exists = g_file_test(path, G_FILE_TEST_IS_REGULAR);
    g_free(path);
    return exists;
}

gboolean
gsw_module_delete_search_index(GswModule *self, GError **error)
{
    gchar *path;
    gchar *tmp;
    gchar *lock_path;
    gint lock_fd = -1;

    g_return_val_if_fail(GSW_IS_MODULE(self), FALSE);
    path = search_index_path(self);
    if (path == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CONFIG,
                    "Module has no data directory");
        return FALSE;
    }
    if (!search_index_ensure_parent(path, error)) {
        g_free(path);
        return FALSE;
    }
    lock_path = g_strconcat(path, ".lock", NULL);
    {
        GError *lock_error = NULL;

        lock_fd = gsw_file_lock(lock_path, &lock_error);
        g_free(lock_path);
        if (lock_fd < 0) {
            if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
                g_clear_error(&lock_error);
                tmp = g_strconcat(path, ".tmp", NULL);
                g_unlink(tmp);
                g_free(tmp);
                g_free(path);
                return TRUE;
            }
            if (error != NULL)
                g_propagate_error(error, lock_error);
            else
                g_clear_error(&lock_error);
            g_free(path);
            return FALSE;
        }
    }
    if (g_unlink(path) != 0 && errno != ENOENT) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot delete search index %s: %s", path,
                    g_strerror(errno));
        gsw_file_unlock(lock_fd);
        g_free(path);
        return FALSE;
    }
    tmp = g_strconcat(path, ".tmp", NULL);
    g_unlink(tmp);
    g_free(tmp);
    gsw_file_unlock(lock_fd);
    g_free(path);
    return TRUE;
}

gboolean
gsw_module_build_search_index(GswModule *self, GCancellable *cancellable,
                              GswSearchProgressFunc progress,
                              gpointer user_data, GError **error)
{
    GswSearchIndex *index = NULL;
    GswKey *saved = NULL;
    gchar *path = NULL;
    gchar *lock_path = NULL;
    const gchar *module_name;
    guint total = 0;
    guint processed = 0;
    guint guard = 0;
    gint lock_fd = -1;
    gboolean ok = FALSE;

    g_return_val_if_fail(GSW_IS_MODULE(self), FALSE);

    module_name = gsw_module_get_name(self);
    saved = gsw_key_clone(gsw_module_get_key(self));
    if (saved == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_IO,
                    "Cannot save module key");
        return FALSE;
    }
    path = search_index_path(self);
    if (path == NULL) {
        g_set_error(error, GSW_ERROR, GSW_ERROR_CONFIG,
                    "Module has no data directory");
        goto out;
    }
    if (!search_index_ensure_parent(path, error))
        goto out;
    lock_path = g_strconcat(path, ".lock", NULL);
    lock_fd = gsw_file_lock(lock_path, error);
    if (lock_fd < 0)
        goto out;
    if (!search_index_count_entries(self, cancellable, &total, error))
        goto out;
    if (!gsw_module_set_position(self, GSW_POSITION_TOP, error))
        goto out;

    index = search_index_new();
    for (;;) {
        gchar *text = NULL;
        GError *local_error = NULL;
        GPtrArray *tokens = NULL;
        GHashTable *seen = NULL;
        const gchar *key_text;
        guint i;

        if (g_cancellable_is_cancelled(cancellable)) {
            g_set_error(error, GSW_ERROR, GSW_ERROR_CANCELLED,
                        "Index build cancelled");
            goto out;
        }
        text = gsw_module_get_text(self, &local_error);
        if (text == NULL && local_error != NULL) {
            if (error != NULL)
                g_propagate_error(error, local_error);
            else
                g_clear_error(&local_error);
            goto out;
        }
        key_text = gsw_module_get_key_text(self);
        if (key_text == NULL)
            key_text = "";

        if (text != NULL && *text != '\0') {
            tokens = search_tokenize(text);
            seen = g_hash_table_new(g_str_hash, g_str_equal);
            for (i = 0; i < tokens->len; i++) {
                const gchar *token = g_ptr_array_index(tokens, i);
                GPtrArray *keys;

                if (g_hash_table_contains(seen, token))
                    continue;
                g_hash_table_add(seen, (gpointer)token);
                keys = g_hash_table_lookup(index->tokens, token);
                if (keys == NULL) {
                    keys = g_ptr_array_new_with_free_func(g_free);
                    g_hash_table_insert(index->tokens, g_strdup(token), keys);
                }
                if (keys->len == 0 ||
                    g_strcmp0(g_ptr_array_index(keys, keys->len - 1),
                              key_text) != 0)
                    g_ptr_array_add(keys, g_strdup(key_text));
            }
            g_hash_table_unref(seen);
            g_ptr_array_unref(tokens);
        }
        g_free(text);

        processed++;
        if (progress != NULL)
            progress(module_name, processed, total, user_data);
        if (guard++ >= GSW_SEARCH_SCAN_GUARD)
            break;
        {
            GError *advance_error = NULL;

            if (!gsw_module_next(self, 1, &advance_error)) {
                g_clear_error(&advance_error);
                break;
            }
        }
    }

    ok = search_index_save(index, path, error);

out:
    if (lock_fd >= 0)
        gsw_file_unlock(lock_fd);
    g_free(lock_path);
    g_free(path);
    search_index_free(index);
    if (saved != NULL)
        gsw_module_set_key(self, saved, NULL);
    g_clear_pointer(&saved, gsw_key_unref);
    return ok;
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

    if (search_index_can_use(options)) {
        gchar *index_path = search_index_path(self);

        if (index_path != NULL) {
            if (g_file_test(index_path, G_FILE_TEST_IS_REGULAR)) {
                GError *index_error = NULL;
                GswSearchIndex *index = search_index_load(index_path,
                                                          &index_error);

                g_clear_error(&index_error);
                if (index != NULL) {
                    GPtrArray *candidates = search_index_candidates(
                        index, gsw_search_options_get_text(options));
                    guint i;

                    search_index_free(index);
                    if (candidates != NULL) {
                        total = candidates->len;
                        for (i = 0; i < candidates->len; i++) {
                            GError *local_error = NULL;

                            if (g_cancellable_is_cancelled(cancellable)) {
                                g_set_error(error, GSW_ERROR,
                                            GSW_ERROR_CANCELLED,
                                            "Search cancelled");
                                ok = FALSE;
                                g_ptr_array_unref(candidates);
                                g_free(index_path);
                                goto out;
                            }
                            if (!gsw_module_set_key_text(
                                    self, g_ptr_array_index(candidates, i),
                                    error)) {
                                ok = FALSE;
                                g_ptr_array_unref(candidates);
                                g_free(index_path);
                                goto out;
                            }
                            if (entry_matches(self, options, regex, words,
                                              needle, &local_error)) {
                                gchar *text = gsw_module_get_text(self, NULL);

                                g_ptr_array_add(results, gsw_search_result_new(
                                    module_name, gsw_module_get_key_text(self),
                                    text != NULL ? text
                                                 : gsw_module_get_key_text(self)));
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
                                g_ptr_array_unref(candidates);
                                g_free(index_path);
                                goto out;
                            }
                            processed++;
                            if (progress != NULL)
                                progress(module_name, processed, total,
                                         user_data);
                        }
                        g_ptr_array_unref(candidates);
                        g_free(index_path);
                        goto out;
                    }
                }
            }
            g_free(index_path);
        }
    }

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
