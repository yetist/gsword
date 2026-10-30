# gsword application

This directory contains the GTK4/libadwaita application named **gsword**.

The reusable SWORD module library lives in `../lib/` and is installed as
`libgsword`. The application consumes only the public API from
`../include/gsword/`.

Current implementation:

- `main.c`: application entry point
- `application.c` / `application.h`: `AdwApplication`, library-path handling,
  settings fallback, quit action
- `window.c` / `window.h`: main window, module sidebar, reader, chapter
  navigation, copy action, preferences dialog, empty state
- `org.gsword.GSword.gschema.xml`: last module/key, font scale, theme

The application design is documented in `../docs/app-design.md`.

The earlier minimal GTK4 prototype remains in `../examples/gtk.c` for API
experiments and is separate from the installed application.
