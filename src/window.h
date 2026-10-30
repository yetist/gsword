#ifndef GSWORD_WINDOW_H
#define GSWORD_WINDOW_H 1

#include <adwaita.h>
#include <gsword/gsword.h>

G_BEGIN_DECLS

#define GSWORD_TYPE_WINDOW (gsword_window_get_type())
G_DECLARE_FINAL_TYPE(GswordWindow, gsword_window, GSWORD, WINDOW,
                     AdwApplicationWindow)

GtkWidget *gsword_window_new(GtkApplication *application,
                             GswManager *manager,
                             GSettings *settings);

G_END_DECLS

#endif /* GSWORD_WINDOW_H */
