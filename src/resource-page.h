/*
 * resource-page.h: general book and map viewer for the gsword window
 */

#ifndef GSWORD_RESOURCE_PAGE_H
#define GSWORD_RESOURCE_PAGE_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordResourcePage GswordResourcePage;

GswordResourcePage *gsword_resource_page_new(GswordWindow *window);
GtkWidget *gsword_resource_page_widget(GswordResourcePage *page);
void gsword_resource_page_open(GswordResourcePage *page, const gchar *name,
                               gboolean map);

G_END_DECLS

#endif /* GSWORD_RESOURCE_PAGE_H */
