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
  navigation, parallel reading, study panel, search/index UI, notes/tags/verse
  lists/highlights/export, module install/update/remove UI, reading plans,
  daily devotion, memory review, teaching layout, workspace persistence,
  preferences dialog
- `notes-store.c` / `notes-store.h`: SQLite-backed notes, tags, verse lists and
  highlights in `~/.local/share/gsword/app.db`
- `plan-store.c` / `plan-store.h`: SQLite-backed reading plans, plan progress,
  memory review queue, and teaching notes in the same application database
- `app-paths.c` / `app-paths.h`: unified application data directory
  (`~/.local/share/gsword`) and one-time migration from the old
  `~/.local/share/gsword-app` location
- `org.gsword.GSword.gschema.xml`: last module/key, active workspace page,
  parallel modules, active plan, devotion module, font scale, theme

Application UI strings use gettext. The `../po/` directory contains
`POTFILES.in`, `LINGUAS`, and translated catalogs such as `zh_CN.po`.
`GSW_LOCALEDIR` can point at a build or installed locale directory during
development.

The application design is documented in `../docs/app-design.md`.

The earlier minimal GTK4 prototype remains in `../examples/gtk.c` for API
experiments and is separate from the installed application.
