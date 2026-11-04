/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-search.h: module search options and results
 *
 * This file is part of gsword.
 */

#ifndef __GSW_SEARCH_H__
#define __GSW_SEARCH_H__ 1

#include <gio/gio.h>
#include <gsword/gsw-manager.h>
#include <gsword/gsw-verse-list.h>

G_BEGIN_DECLS

/**
 * GswSearchOptions:
 *
 * Options for module and multi-module searches. The object is
 * reference-counted. A maximum result count of 0 means unlimited.
 */
typedef struct _GswSearchOptions GswSearchOptions;

/**
 * GswSearchResult:
 *
 * A single search hit. The result stores the module name, key, preview text,
 * and a short match snippet. It is reference-counted.
 */
typedef struct _GswSearchResult GswSearchResult;

/**
 * GswSearchProgressFunc:
 * @module_name: module being searched
 * @processed: number of entries processed
 * @total: total entries, or 0 when unknown
 * @user_data: user data
 *
 * Progress callback for a search operation.
 */
typedef void (*GswSearchProgressFunc)(const gchar *module_name,
                                      guint processed,
                                      guint total,
                                      gpointer user_data);

#define GSW_TYPE_SEARCH_OPTIONS (gsw_search_options_get_type())
#define GSW_TYPE_SEARCH_RESULT (gsw_search_result_get_type())

/**
 * gsw_search_options_get_type:
 *
 * Gets the #GType for #GswSearchOptions.
 *
 * Returns: the #GType for #GswSearchOptions
 */
GType            gsw_search_options_get_type(void);
/**
 * gsw_search_result_get_type:
 *
 * Gets the #GType for #GswSearchResult.
 *
 * Returns: the #GType for #GswSearchResult
 */
GType            gsw_search_result_get_type(void);

/**
 * gsw_search_options_new:
 *
 * Creates empty search options with phrase search and case-insensitive
 * matching.
 *
 * Returns: (transfer full): a new #GswSearchOptions
 */
GswSearchOptions *gsw_search_options_new(void);
/**
 * gsw_search_options_ref:
 * @self: a #GswSearchOptions
 *
 * Increases the reference count of @self.
 *
 * Returns: (transfer full): @self
 */
GswSearchOptions *gsw_search_options_ref(GswSearchOptions *self);
/**
 * gsw_search_options_unref:
 * @self: a #GswSearchOptions
 *
 * Decreases the reference count of @self.
 */
void              gsw_search_options_unref(GswSearchOptions *self);
/**
 * gsw_search_options_set_text:
 * @self: a #GswSearchOptions
 * @text: search text
 *
 * Sets the search text.
 */
void              gsw_search_options_set_text(GswSearchOptions *self,
                                              const gchar *text);
/**
 * gsw_search_options_get_text:
 * @self: a #GswSearchOptions
 *
 * Gets the search text.
 *
 * Returns: (transfer none): the search text
 */
const gchar      *gsw_search_options_get_text(GswSearchOptions *self);
/**
 * gsw_search_options_set_search_type:
 * @self: a #GswSearchOptions
 * @type: search algorithm
 *
 * Sets the search type.
 */
void              gsw_search_options_set_search_type(GswSearchOptions *self,
                                                      GswSearchType type);
/**
 * gsw_search_options_get_search_type:
 * @self: a #GswSearchOptions
 *
 * Gets the search type.
 *
 * Returns: the search type
 */
GswSearchType     gsw_search_options_get_search_type(GswSearchOptions *self);
/**
 * gsw_search_options_set_case_sensitive:
 * @self: a #GswSearchOptions
 * @case_sensitive: whether matching is case-sensitive
 *
 * Sets case sensitivity.
 */
void              gsw_search_options_set_case_sensitive(GswSearchOptions *self,
                                                         gboolean case_sensitive);
/**
 * gsw_search_options_get_case_sensitive:
 * @self: a #GswSearchOptions
 *
 * Checks whether matching is case-sensitive.
 *
 * Returns: %TRUE if matching is case-sensitive
 */
gboolean          gsw_search_options_get_case_sensitive(GswSearchOptions *self);
/**
 * gsw_search_options_set_max_results:
 * @self: a #GswSearchOptions
 * @max_results: maximum results, or 0 for unlimited
 *
 * Sets the result limit.
 */
void              gsw_search_options_set_max_results(GswSearchOptions *self,
                                                      guint max_results);
/**
 * gsw_search_options_get_max_results:
 * @self: a #GswSearchOptions
 *
 * Gets the result limit.
 *
 * Returns: the maximum number of results, or 0 for unlimited
 */
guint             gsw_search_options_get_max_results(GswSearchOptions *self);
/**
 * gsw_search_options_set_scope:
 * @self: a #GswSearchOptions
 * @scope: (nullable): verse ranges to search, or %NULL for the whole module
 *
 * Sets the search scope.
 */
void              gsw_search_options_set_scope(GswSearchOptions *self,
                                               GswVerseList *scope);
/**
 * gsw_search_options_get_scope:
 * @self: a #GswSearchOptions
 *
 * Gets the search scope.
 *
 * Returns: (transfer none) (nullable): the scope, or %NULL
 */
GswVerseList     *gsw_search_options_get_scope(GswSearchOptions *self);
/**
 * gsw_search_options_set_attribute:
 * @self: a #GswSearchOptions
 * @level1: (nullable): first attribute level, for example `Word`
 * @level2: (nullable): second attribute level, for example `Lemma`
 *
 * Configures attribute-based search. Pass %NULL for both levels to disable
 * attribute search.
 */
void              gsw_search_options_set_attribute(GswSearchOptions *self,
                                                    const gchar *level1,
                                                    const gchar *level2);
/**
 * gsw_search_options_get_attribute_level1:
 * @self: a #GswSearchOptions
 *
 * Gets the first attribute search level.
 *
 * Returns: (transfer none) (nullable): the level, or %NULL
 */
const gchar      *gsw_search_options_get_attribute_level1(
                                                    GswSearchOptions *self);
/**
 * gsw_search_options_get_attribute_level2:
 * @self: a #GswSearchOptions
 *
 * Gets the second attribute search level.
 *
 * Returns: (transfer none) (nullable): the level, or %NULL
 */
const gchar      *gsw_search_options_get_attribute_level2(
                                                    GswSearchOptions *self);

/**
 * gsw_search_result_ref:
 * @self: a #GswSearchResult
 *
 * Increases the reference count of @self.
 *
 * Returns: (transfer full): @self
 */
GswSearchResult  *gsw_search_result_ref(GswSearchResult *self);
/**
 * gsw_search_result_unref:
 * @self: a #GswSearchResult
 *
 * Decreases the reference count of @self.
 */
void              gsw_search_result_unref(GswSearchResult *self);
/**
 * gsw_search_result_get_module_name:
 * @self: a #GswSearchResult
 *
 * Gets the module name.
 *
 * Returns: (transfer none): the module name
 */
const gchar      *gsw_search_result_get_module_name(GswSearchResult *self);
/**
 * gsw_search_result_get_key:
 * @self: a #GswSearchResult
 *
 * Gets the matching key.
 *
 * Returns: (transfer none): the key
 */
const gchar      *gsw_search_result_get_key(GswSearchResult *self);
/**
 * gsw_search_result_get_preview:
 * @self: a #GswSearchResult
 *
 * Gets a plain-text preview of the matching entry.
 *
 * Returns: (transfer none): the preview text
 */
const gchar      *gsw_search_result_get_preview(GswSearchResult *self);
/**
 * gsw_search_result_get_snippet:
 * @self: a #GswSearchResult
 *
 * Gets a short match snippet.
 *
 * Returns: (transfer none): the snippet
 */
const gchar      *gsw_search_result_get_snippet(GswSearchResult *self);

/**
 * gsw_module_set_search_index_path:
 * @self: a #GswModule
 * @path: (nullable): explicit index file path, or %NULL to use the module data
 *   directory
 *
 * Sets where the persistent search index is stored. Applications should use an
 * XDG cache path for writable indexes, including for read-only system modules.
 */
void             gsw_module_set_search_index_path(GswModule *self,
                                                  const gchar *path);
/**
 * gsw_module_get_search_index_path:
 * @self: a #GswModule
 *
 * Gets the explicitly configured search index path. A %NULL return means the
 * index is stored in the module data directory.
 *
 * Returns: (transfer none) (nullable): the explicit index path
 */
const gchar     *gsw_module_get_search_index_path(GswModule *self);

/**
 * gsw_module_has_search_index:
 * @self: a #GswModule
 *
 * Checks whether a persistent search index exists for @self.
 *
 * Returns: %TRUE if an index file is present
 */
gboolean         gsw_module_has_search_index(GswModule *self);
/**
 * gsw_module_build_search_index:
 * @self: a #GswModule
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @progress: (nullable) (scope call): progress callback
 * @user_data: user data for @progress
 * @error: return location for a #GError, or %NULL
 *
 * Builds or rebuilds a persistent inverted index for @self. The index is
 * stored at the path configured by gsw_module_set_search_index_path(), or in
 * the module data directory when no explicit path is set. It is replaced
 * atomically and the module's current key is preserved.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean         gsw_module_build_search_index(GswModule *self,
                                               GCancellable *cancellable,
                                               GswSearchProgressFunc progress,
                                               gpointer user_data,
                                               GError **error);
/**
 * gsw_module_delete_search_index:
 * @self: a #GswModule
 * @error: return location for a #GError, or %NULL
 *
 * Deletes the persistent search index for @self, if present.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean         gsw_module_delete_search_index(GswModule *self,
                                                GError **error);

/**
 * gsw_module_search_full:
 * @self: a #GswModule
 * @options: search options
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @progress: (nullable) (scope call): progress callback
 * @user_data: user data for @progress
 * @error: return location for a #GError, or %NULL
 *
 * Searches one module using the supplied options. The module's current key is
 * preserved.
 *
 * Returns: (transfer container) (element-type GswSearchResult): an array of
 *   results, or %NULL on error
 */
GPtrArray        *gsw_module_search_full(GswModule *self,
                                         GswSearchOptions *options,
                                         GCancellable *cancellable,
                                         GswSearchProgressFunc progress,
                                         gpointer user_data,
                                         GError **error);
/**
 * gsw_manager_search:
 * @self: a #GswManager
 * @module_names: (nullable) (array zero-terminated=1) (element-type utf8):
 *   modules to search, or %NULL for all modules
 * @options: search options
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @progress: (nullable) (scope call): progress callback
 * @user_data: user data for @progress
 * @error: return location for a #GError, or %NULL
 *
 * Searches several modules and aggregates their results.
 *
 * Returns: (transfer container) (element-type GswSearchResult): an array of
 *   results, or %NULL on error
 */
GPtrArray        *gsw_manager_search(GswManager *self,
                                     const gchar * const *module_names,
                                     GswSearchOptions *options,
                                     GCancellable *cancellable,
                                     GswSearchProgressFunc progress,
                                     gpointer user_data,
                                     GError **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswSearchOptions, gsw_search_options_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GswSearchResult, gsw_search_result_unref)

G_END_DECLS

#endif /* __GSW_SEARCH_H__ */
