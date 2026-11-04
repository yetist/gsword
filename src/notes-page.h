/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * notes-page.h: notes, tags and verse lists for the gsword window
 */

#ifndef GSWORD_NOTES_PAGE_H
#define GSWORD_NOTES_PAGE_H 1

#include "window.h"

G_BEGIN_DECLS

typedef struct _GswordNotesPage GswordNotesPage;

GswordNotesPage *gsword_notes_page_new(GswordWindow *window);
GtkWidget *gsword_notes_page_widget(GswordNotesPage *page);
void gsword_notes_page_refresh(GswordNotesPage *page);
void gsword_notes_page_open_current(GswordNotesPage *page);
void gsword_notes_page_open_reference(GswordNotesPage *page,
                                      const gchar *module,
                                      const gchar *storage_key,
                                      const gchar *display_key);
gboolean gsword_notes_page_has_note_key(GswordNotesPage *page);

G_END_DECLS

#endif /* GSWORD_NOTES_PAGE_H */
