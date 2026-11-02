/*
 * search-page.h: search page for the gsword window
 */

#ifndef GSWORD_SEARCH_PAGE_H
#define GSWORD_SEARCH_PAGE_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordSearchPage GswordSearchPage;

GswordSearchPage *gsword_search_page_new(GswordWindow *window);
GtkWidget *gsword_search_page_widget(GswordSearchPage *page);
void gsword_search_page_refresh_index(GswordSearchPage *page);
void gsword_search_page_select_current(GswordSearchPage *page);
void gsword_search_page_focus(GswordSearchPage *page);

G_END_DECLS

#endif /* GSWORD_SEARCH_PAGE_H */
