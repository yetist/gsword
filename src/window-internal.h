/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * window-internal.h: private window state shared by window modules
 */

#ifndef GSWORD_WINDOW_INTERNAL_H
#define GSWORD_WINDOW_INTERNAL_H 1

#include "window.h"
#include "window-private.h"
#include "search-page.h"
#include "teaching-page.h"
#include "resource-page.h"
#include "notes-page.h"
#include "plan-page.h"
#include "personal-page.h"
#include "prayer-page.h"
#include "today-page.h"
#include "collections-page.h"
#include "library-page.h"
#include "workspace-page.h"
#include "reader-page.h"
#include "module-sidebar.h"
#include "notes-store.h"
#include "plan-store.h"
#include "prayer-store.h"
#include "insight-store.h"
#include "bookmark-store.h"

G_BEGIN_DECLS

typedef struct {
    GswManager *manager;
    GSettings *settings;
    GswModule *module;
    GswordModuleSidebar *module_sidebar;
    AdwOverlaySplitView *split_view;
    GtkToggleButton *sidebar_button;
    gboolean sidebar_visible;
    gboolean sidebar_updating;
    GswordWorkspacePage *workspace_page;
    GswordReaderPage *reader_page;
    AdwViewStack *content_stack;
    gboolean page_restored;
    GtkLabel *status_label;
    AdwToastOverlay *toast_overlay;

    GswordSearchPage *search_page;
    gchar *search_return_page;

    GswordLibraryPage *library_page;
    gchar *library_return_page;

    GswordPlanStore *plans;
    GswordPlanPage *plan_page;
    GswordPrayerStore *prayers;
    GswordInsightStore *insights;
    GswordBookmarkStore *bookmarks;
    GswordPrayerPage *prayer_page;
    GswordTodayPage *today_page;
    GswordCollectionsPage *collections_page;
    GswordPersonalPage *personal_page;

    GswordTeachingPage *teaching_page;
    GswordResourcePage *resource_page;

    GswordNotesStore *notes;
    GswordNotesPage *notes_page;
} GswordWindowPrivate;

GswordWindowPrivate *gsword_window_get_private(GswordWindow *self);
gchar *gsword_window_current_page_id(GswordWindow *self);
void gsword_window_toggle_sidebar(GswordWindow *self);
void gsword_window_open_search(GswordWindow *self);
void gsword_window_install_actions(GswordWindow *self);
void gsword_window_build_pages(GswordWindow *self);
void gsword_window_dispose_pages(GswordWindow *self);

G_END_DECLS

#endif /* GSWORD_WINDOW_INTERNAL_H */
