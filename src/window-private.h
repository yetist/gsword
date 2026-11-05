/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
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
#include "personal-page.h"
#include "prayer-store.h"
#include "insight-store.h"

G_BEGIN_DECLS

void gsword_window_services_init(GswordWindow *self,
                                 GtkLabel *status_label,
                                 AdwViewStack *content_stack);

void gsword_window_set_status(GswordWindow *self, const gchar *message);
void gsword_window_toast(GswordWindow *self, const gchar *message);
void gsword_window_show_error(GswordWindow *self, const gchar *prefix,
                              GError *error);
void gsword_window_show_page(GswordWindow *self, const gchar *page);
gboolean gsword_window_get_sidebar_visible(GswordWindow *self);
void gsword_window_set_sidebar_visible(GswordWindow *self, gboolean visible);
gboolean gsword_window_is_page_visible(GswordWindow *self,
                                       const gchar *page);
void gsword_window_hide_sidebar_overlay(GswordWindow *self);
void gsword_window_close_search(GswordWindow *self);
void gsword_window_open_library(GswordWindow *self);
void gsword_window_close_library(GswordWindow *self);

GswManager *gsword_window_get_manager(GswordWindow *self);
GSettings *gsword_window_get_settings(GswordWindow *self);
GswModule *gsword_window_get_module(GswordWindow *self);
gboolean gsword_window_module_is_writable(GswordWindow *self,
                                          GswModuleInfo *info);
gboolean gsword_window_book_names_follow_ui(GswordWindow *self);
gchar *gsword_window_book_name_locale(GswordWindow *self,
                                      GswModuleInfo *info);
void gsword_window_apply_book_name_locale(GswordWindow *self,
                                          GswModule *module);
GswordNotesStore *gsword_window_get_notes_store(GswordWindow *self);
GswordPlanStore *gsword_window_get_plan_store(GswordWindow *self);
GswordPrayerStore *gsword_window_get_prayer_store(GswordWindow *self);
GswordInsightStore *gsword_window_get_insight_store(GswordWindow *self);
void gsword_window_show_personal(GswordWindow *self,
                                 GswordPersonalSection section);
void gsword_window_show_personal_page(GswordWindow *self,
                                      const gchar *section_name);
void gsword_window_open_prayer_for(GswordWindow *self, const gchar *module,
                                   const gchar *storage_key,
                                   const gchar *display_key);
void gsword_window_open_memory_review(GswordWindow *self);
GswModule *gsword_window_ensure_verse_module(GswordWindow *self);
void gsword_window_populate_modules(GswordWindow *self);
gboolean gsword_window_module_info_is_devotional(GswModuleInfo *info);
gboolean gsword_window_module_info_is_map(GswModuleInfo *info);
GswModuleType gsword_window_module_effective_type(GswModuleInfo *info);
const gchar *gsword_window_module_type_display(GswModuleInfo *info);
void gsword_window_clear_list(GtkListBox *list);

void gsword_window_load_module(GswordWindow *self, const gchar *name);
void gsword_window_refresh_reader(GswordWindow *self);
void gsword_window_refresh_search_index(GswordWindow *self);
void gsword_window_sync_workspace_from_reader(GswordWindow *self);
void gsword_window_focus_reference(GswordWindow *self);
void gsword_window_open_current_note(GswordWindow *self);
void gsword_window_open_note_for(GswordWindow *self, const gchar *module,
                                 const gchar *storage_key,
                                 const gchar *display_key);
void gsword_window_open_compare_reference(GswordWindow *self,
                                          const gchar *module,
                                          const gchar *key);
void gsword_window_open_teaching(GswordWindow *self);
void gsword_window_open_commentary_module(GswordWindow *self,
                                           const gchar *name);
void gsword_window_open_lexicon_module(GswordWindow *self,
                                        const gchar *name);
void gsword_window_open_devotion_module(GswordWindow *self,
                                         const gchar *name);
void gsword_window_open_resource_module(GswordWindow *self,
                                         const gchar *name,
                                         gboolean is_map);

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
gchar *gsword_window_chapter_text_for_module_with_missing(GswModule *module,
                                                          gboolean show_missing);
void gsword_window_set_view_text(GtkTextView *view, const gchar *text);

gchar *gsword_window_normalize_reference(const gchar *text);
gint gsword_window_verse_number_from_line(const gchar *line);

G_END_DECLS

#endif /* GSWORD_WINDOW_PRIVATE_H */
