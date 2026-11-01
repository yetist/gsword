/*
 * reader-page.h: scripture reader page for the gsword window
 */

#ifndef GSWORD_READER_PAGE_H
#define GSWORD_READER_PAGE_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordReaderPage GswordReaderPage;
typedef struct _GswordStudyPanel GswordStudyPanel;

GswordReaderPage *gsword_reader_page_new(GswordWindow *window);
GtkWidget *gsword_reader_page_widget(GswordReaderPage *page);
GswordStudyPanel *gsword_reader_page_get_study_panel(GswordReaderPage *page);
void gsword_reader_page_refresh(GswordReaderPage *page);
gboolean gsword_reader_page_set_key_text(GswordReaderPage *page,
                                          const gchar *text);
void gsword_reader_page_sync_context(GswordReaderPage *page);
void gsword_reader_page_set_initial_key(GswordReaderPage *page);
void gsword_reader_page_begin_reference_edit(GswordReaderPage *page);
void gsword_reader_page_activate_reference(GswordReaderPage *page);
void gsword_reader_page_copy(GswordReaderPage *page);
void gsword_reader_page_toggle_highlight(GswordReaderPage *page);
gchar *gsword_reader_page_current_reference(GswordReaderPage *page);
gchar *gsword_reader_page_current_start_reference(GswordReaderPage *page);
gchar *gsword_reader_page_current_storage_key(GswordReaderPage *page);
gchar *gsword_reader_page_current_display_key(GswordReaderPage *page);

G_END_DECLS

#endif /* GSWORD_READER_PAGE_H */
