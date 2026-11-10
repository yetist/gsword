/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com> */
/*
 * library-page-private.h: internal helpers shared by library modules
 */

#ifndef GSWORD_LIBRARY_PAGE_PRIVATE_H
#define GSWORD_LIBRARY_PAGE_PRIVATE_H 1

#include "library-page.h"

G_BEGIN_DECLS

struct _GswordLibraryPage {
    GswordPage parent_instance;

    AdwNavigationSplitView *split_view;
    GtkWidget *detail_back;
    GswManager *manager;
    GSettings *settings;
    GswInstallManager *installer;
    GswStatusReporter *install_reporter;
    GPtrArray *install_sources;
    GtkStringList *source_names;
    GtkDropDown *source_dropdown;
    GtkWidget *source_row;
    GtkWidget *filter_row;
    GtkStack *list_stack;
    GtkListView *installed_view;
    GtkListView *remote_view;
    GtkWidget *installed_empty;
    GtkWidget *remote_empty;
    GListStore *installed_store;
    GListStore *remote_store;
    GtkFilterListModel *installed_filter_model;
    GtkFilterListModel *remote_filter_model;
    GtkSortListModel *installed_sort_model;
    GtkSortListModel *remote_sort_model;
    GtkSingleSelection *installed_selection;
    GtkSingleSelection *remote_selection;
    GtkCustomFilter *installed_filter;
    GtkCustomFilter *remote_filter;
    GtkCustomSorter *module_sorter;
    GtkDropDown *sort_dropdown;
    gint sort_mode;
    GtkWidget *local_box;
    gint library_view;
    GtkEntry *install_search;
    GtkDropDown *type_filter;
    GtkDropDown *language_filter;
    GtkDropDown *status_filter;
    GtkButton *clear_filters_button;
    GtkLabel *filter_count;
    GPtrArray *language_codes;
    GtkStringList *language_names;
    GPtrArray *remote_infos;
    gboolean updating_filters;
    GtkLabel *remote_title;
    GtkLabel *remote_description;
    GtkLabel *remote_status;
    GtkLabel *remote_meta;
    GtkLabel *remote_warning;
    GtkExpander *technical_expander;
    GtkLabel *technical_label;
    GtkExpander *license_expander;
    GtkLabel *license_label;
    GtkExpander *about_expander;
    GtkLabel *about_label;
    GtkProgressBar *install_progress;
    GtkLabel *install_status;
    GtkListBox *install_queue_list;
    GtkWidget *install_queue_scroll;
    GtkButton *install_button;
    GtkButton *remove_button;
    GtkButton *unlock_button;
    GtkButton *cancel_button;
    GtkButton *refresh_source_button;
    GtkButton *refresh_sources_button;
    GtkButton *test_source_button;
    GtkToggleButton *installed_view_button;
    GtkToggleButton *available_view_button;
    GtkToggleButton *updates_view_button;
    GtkButton *update_all_button;
    GtkButton *retry_failed_button;
    GtkButton *pause_queue_button;
    gboolean queue_paused;
    GPtrArray *update_queue;
    GPtrArray *failed_updates;
    GswInstallSource *update_source;
    gboolean updating_all;
    GCancellable *install_cancellable;
    GtkEntry *local_dir_entry;
    GtkEntry *local_name_entry;
    GtkButton *local_install_button;
    gchar *selected_remote_name;
    gchar *selected_installed_name;
};

#define LIBRARY_WINDOW(page) \
    gsword_page_get_window(GSWORD_PAGE(page))

GswordLibraryPage *library_page_get(GswordWindow *self);
GswInstallManager *gsword_library_page_get_installer(
    GswordLibraryPage *page);
void gsword_library_refresh_installed_list(GswordWindow *self);
void gsword_library_refresh_remote_modules(GswordWindow *self);
void gsword_library_show_module_details(GswordWindow *self);
void gsword_library_apply_filters(GswordWindow *self);
void gsword_library_clear_remote_details(GswordWindow *self);
void gsword_library_update_library_view(GswordWindow *self, gint view);
GswInstallSource *gsword_library_current_install_source(GswordWindow *self);
void gsword_library_clear_install_progress_later(GswordWindow *self);
void gsword_library_on_refresh_remote_finished(GObject *source,
                                               GAsyncResult *result,
                                               gpointer user_data);
void gsword_library_on_update_all_clicked(GtkButton *button, gpointer user_data);
void gsword_library_on_retry_failed_clicked(GtkButton *button, gpointer user_data);
void gsword_library_on_pause_queue_clicked(GtkButton *button, gpointer user_data);
void gsword_library_on_install_clicked(GtkButton *button, gpointer user_data);
void gsword_library_on_remove_clicked(GtkButton *button, gpointer user_data);
void gsword_library_on_cancel_install_clicked(GtkButton *button, gpointer user_data);
void gsword_library_on_install_pre_status(GswStatusReporter *reporter,
                                          guint64 total, guint64 done,
                                          const gchar *message, gpointer user_data);
void gsword_library_on_install_progress(GswStatusReporter *reporter,
                                        guint64 total, guint64 done,
                                        gpointer user_data);
void gsword_window_refresh_install_sources(GswordWindow *self);
void gsword_library_page_show_sources_dialog(GtkButton *button,
                                             gpointer user_data);

G_END_DECLS

#endif /* GSWORD_LIBRARY_PAGE_PRIVATE_H */
