/*
 * reader-toolbar.h: reader toolbar for the gsword window
 */

#ifndef GSWORD_READER_TOOLBAR_H
#define GSWORD_READER_TOOLBAR_H 1

#include "window.h"
#include "gsword-reference-chooser.h"

G_BEGIN_DECLS

typedef struct _GswordReaderPage GswordReaderPage;
typedef struct _GswordReaderToolbar GswordReaderToolbar;

GswordReaderToolbar *gsword_reader_toolbar_new(GswordWindow *window,
                                                GswordReaderPage *page);
GtkWidget *gsword_reader_toolbar_widget(GswordReaderToolbar *toolbar);
GswordReferenceChooser *gsword_reader_toolbar_get_reference_chooser(
    GswordReaderToolbar *toolbar);
GtkToggleButton *gsword_reader_toolbar_get_study_toggle(
    GswordReaderToolbar *toolbar);

G_END_DECLS

#endif /* GSWORD_READER_TOOLBAR_H */
