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

#define GSW_TYPE_LOCALE_MGR (gsw_locale_mgr_get_type())
G_DECLARE_FINAL_TYPE(GswLocaleMgr, gsw_locale_mgr, GSW, LOCALE_MGR, GObject)

/**
 * gsw_locale_mgr_get_default:
 *
 * Returns: (transfer none): singleton locale manager
 */
GswLocaleMgr *gsw_locale_mgr_get_default(void);

gboolean      gsw_locale_mgr_load_file(GswLocaleMgr *self, const gchar *path,
                                       GError **error);
guint         gsw_locale_mgr_load_directory(GswLocaleMgr *self,
                                            const gchar *path,
                                            GError **error);
/**
 * gsw_locale_mgr_get_locales:
 *
 * Returns: (transfer container) (element-type utf8): locale names
 */
GPtrArray    *gsw_locale_mgr_get_locales(GswLocaleMgr *self);

const gchar  *gsw_locale_mgr_get_default_locale(GswLocaleMgr *self);
void          gsw_locale_mgr_set_default_locale(GswLocaleMgr *self,
                                                const gchar *name);

const gchar  *gsw_locale_mgr_translate(GswLocaleMgr *self, const gchar *text,
                                       const gchar *locale);
const gchar  *gsw_locale_mgr_reverse(GswLocaleMgr *self,
                                     const gchar *translated,
                                     const gchar *locale);

G_END_DECLS

#endif /* __GSW_LOCALE_H__ */
