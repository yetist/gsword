/*
 * window-private.h: internal services shared by gsword window pages
 *
 * This header intentionally does not expose GswordWindowPrivate.  Page
 * modules should use these functions instead of reaching into window state.
 */

#ifndef GSWORD_WINDOW_PRIVATE_H
#define GSWORD_WINDOW_PRIVATE_H 1

#include "window.h"
#include "notes-store.h"
#include "plan-store.h"

G_BEGIN_DECLS

void gsword_window_services_init(GswordWindow *self,
                                 GtkLabel *status_label,
                                 GtkStack *content_stack);

void gsword_window_set_status(GswordWindow *self, const gchar *message);
void gsword_window_show_error(GswordWindow *self, const gchar *prefix,
                              GError *error);
void gsword_window_show_page(GswordWindow *self, const gchar *page);

GswManager *gsword_window_get_manager(GswordWindow *self);
GSettings *gsword_window_get_settings(GswordWindow *self);
GswModule *gsword_window_get_module(GswordWindow *self);
GswordNotesStore *gsword_window_get_notes_store(GswordWindow *self);
GswordPlanStore *gsword_window_get_plan_store(GswordWindow *self);

gboolean gsword_window_set_key_text(GswordWindow *self, const gchar *text);
gboolean gsword_window_open_reference(GswordWindow *self,
                                      const gchar *reference);
gboolean gsword_window_open_module_reference(GswordWindow *self,
                                             const gchar *module_name,
                                             const gchar *reference);

gchar *gsword_window_current_reference(GswordWindow *self);
gchar *gsword_window_current_start_reference(GswordWindow *self);
gchar *gsword_window_current_storage_key(GswordWindow *self);
gchar *gsword_window_current_display_key(GswordWindow *self);

gchar *gsword_window_chapter_text_for_module(GswModule *module);
void gsword_window_set_view_text(GtkTextView *view, const gchar *text);

gchar *gsword_window_normalize_reference(const gchar *text);
gint gsword_window_verse_number_from_line(const gchar *line);
gchar *gsword_window_locale_name(void);

G_END_DECLS

#endif /* GSWORD_WINDOW_PRIVATE_H */
