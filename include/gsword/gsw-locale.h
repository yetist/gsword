/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * gsw-locale.h: SWORD locale/book-name translation
 *
 * This file is part of gsword.
 */

#ifndef __GSW_LOCALE_H__
#define __GSW_LOCALE_H__ 1

#include <glib-object.h>
#include <gsword/gsw-version.h>

G_BEGIN_DECLS

/**
 * GswLocaleMgr:
 *
 * Loads locale data and translates book names, abbreviations, and other
 * localized strings. A process-wide instance is available through
 * gsw_locale_mgr_get_default().
 *
 * An English locale is built in and needs no external files. Additional
 * locale packs are loaded from `GSW_LOCALE_PATH` and from
 * `libgsword/locales.d` below the XDG data directories. The system SWORD
 * `locales.d` directory is not read implicitly.
 */
#define GSW_TYPE_LOCALE_MGR (gsw_locale_mgr_get_type())
G_DECLARE_FINAL_TYPE(GswLocaleMgr, gsw_locale_mgr, GSW, LOCALE_MGR, GObject)

/**
 * gsw_locale_mgr_get_default:
 *
 * Gets the process-wide locale manager.
 *
 * Returns: (transfer none): the process-wide locale manager
 */
GswLocaleMgr *gsw_locale_mgr_get_default(void);

/**
 * gsw_locale_mgr_load_file:
 * @self: a #GswLocaleMgr
 * @path: path to a SWORD locale configuration file
 * @error: return location for a #GError, or %NULL
 *
 * Loads a SWORD locale configuration file.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean      gsw_locale_mgr_load_file(GswLocaleMgr *self, const gchar *path,
                                       GError **error);
/**
 * gsw_locale_mgr_load_directory:
 * @self: a #GswLocaleMgr
 * @path: directory containing locale configuration files
 * @error: return location for a #GError, or %NULL
 *
 * Loads all locale configuration files in @path.
 *
 * Returns: the number of locale files loaded
 */
guint         gsw_locale_mgr_load_directory(GswLocaleMgr *self,
                                            const gchar *path,
                                            GError **error);
/**
 * gsw_locale_mgr_get_locales:
 * @self: a #GswLocaleMgr
 *
 * Gets the names of all loaded locales.
 *
 * Returns: (transfer container) (element-type utf8): a newly allocated array of locale names
 */
GPtrArray    *gsw_locale_mgr_get_locales(GswLocaleMgr *self);

/**
 * gsw_locale_mgr_get_default_locale:
 * @self: a #GswLocaleMgr
 *
 * Gets the default locale name.
 *
 * Returns: (transfer none): the default locale name
 */
const gchar  *gsw_locale_mgr_get_default_locale(GswLocaleMgr *self);
/**
 * gsw_locale_mgr_set_default_locale:
 * @self: a #GswLocaleMgr
 * @name: locale name
 *
 * Sets the default locale used by keys and book-name APIs that do not have an
 * explicit or module-derived locale. The library does not derive this value
 * from the process environment; applications should set it explicitly.
 */
void          gsw_locale_mgr_set_default_locale(GswLocaleMgr *self,
                                                const gchar *name);

/**
 * gsw_locale_mgr_translate:
 * @self: a #GswLocaleMgr
 * @text: text to translate
 * @locale: locale name, or %NULL for the default locale
 *
 * Translates text using a locale. Localization is always explicit; this
 * function does not consult process environment variables.
 *
 * Returns: (transfer none): the translated text, or @text if no translation exists
 */
const gchar  *gsw_locale_mgr_translate(GswLocaleMgr *self, const gchar *text,
                                       const gchar *locale);
/**
 * gsw_locale_mgr_translate_abbreviation:
 * @self: a #GswLocaleMgr
 * @abbreviation: localized book abbreviation from `[Book OSIS].Abbrev` or
 *   `[Book OSIS].Aliases`
 * @locale: locale name, or %NULL for the default locale
 *
 * Looks up a localized book abbreviation and returns its OSIS book name.
 * Matching is case-insensitive for Latin abbreviations.
 *
 * Returns: (transfer none) (nullable): the OSIS book name, or %NULL if the
 *   abbreviation is unknown
 */
const gchar  *gsw_locale_mgr_translate_abbreviation(GswLocaleMgr *self,
                                                    const gchar *abbreviation,
                                                    const gchar *locale);
/**
 * gsw_locale_mgr_get_book_candidates:
 * @self: a #GswLocaleMgr
 * @text: book name, abbreviation, or prefix
 * @locale: locale name, or %NULL for the default locale
 *
 * Returns possible OSIS book names for @text. Exact aliases are returned
 * before prefix matches. If the selected locale and its fallback have no
 * match, other loaded locales are searched as a last resort. Multiple OSIS
 * names may be returned when an abbreviation is ambiguous; the caller should
 * resolve them against the current versification.
 *
 * Returns: (transfer container) (element-type utf8): a newly allocated array
 *   of OSIS names, or an empty array if nothing matches
 */
GPtrArray    *gsw_locale_mgr_get_book_candidates(GswLocaleMgr *self,
                                                 const gchar *text,
                                                 const gchar *locale);
/**
 * gsw_locale_mgr_translate_preferred_abbreviation:
 * @self: a #GswLocaleMgr
 * @abbreviation: preferred OSIS abbreviation, for example `Gen`
 * @locale: locale name, or %NULL for the default locale
 *
 * Looks up the localized preferred abbreviation from `[Book OSIS].Abbrev`.
 *
 * Returns: (transfer none) (nullable): the localized abbreviation, or %NULL
 *   if no preferred abbreviation is available
 */
const gchar  *gsw_locale_mgr_translate_preferred_abbreviation(
                                                GswLocaleMgr *self,
                                                const gchar *abbreviation,
                                                const gchar *locale);
/**
 * gsw_locale_mgr_reverse:
 * @self: a #GswLocaleMgr
 * @translated: localized text to translate back
 * @locale: locale name, or %NULL for the default locale
 *
 * Translates localized text back to its English source.
 *
 * Returns: (transfer none): the English source text, or @translated if unknown
 */
const gchar  *gsw_locale_mgr_reverse(GswLocaleMgr *self,
                                     const gchar *translated,
                                     const gchar *locale);

G_END_DECLS

#endif /* __GSW_LOCALE_H__ */
