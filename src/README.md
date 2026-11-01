# gsword application

This directory contains the GTK4/libadwaita application named **gsword**.

The reusable SWORD module library lives in `../lib/` and is installed as
`libgsword`. The application consumes only the public API from
`../include/gsword/`.

Current implementation:

- `main.c`: application entry point
- `application.c` / `application.h`: `AdwApplication`, library-path handling,
  settings fallback, quit action
- `window.c` / `window.h`: main window, module sidebar, reader, book/chapter/
  verse reference picker, parallel Bible template, study panel,
  highlights/export, module install/update/remove UI,
  reading plans,
  daily devotion, memory review, teaching layout, grouped resource sidebar,
  linked/pinned study panel, type-specific dictionary, general book, map and
  daily devotion viewers, Strong's/cross-reference navigation, range
  selection, workspace templates, multi-pane editor and persistence,
  preferences dialog
- `window-private.h`: internal shared service API for window pages; does not
  expose `GswordWindowPrivate`
- `window-services.c`: shared status/error/page navigation, module/store
  accessors, reference normalization, chapter rendering and view helpers
- `search-page.c` / `search-page.h`: search UI, result navigation and search
  index actions
- `teaching-page.c` / `teaching-page.h`: teaching workspace, scripture view,
  teaching notes and fullscreen mode
- `resource-page.c` / `resource-page.h`: general book and map viewer, resource
  tree, text/picture rendering and resource navigation
- `notes-page.c` / `notes-page.h`: notes, note search, tags, verse lists and
  Markdown export
- `notes-store.c` / `notes-store.h`: SQLite-backed notes, tags, verse lists and
  highlights in `~/.local/share/gsword/app.db`
- `plan-store.c` / `plan-store.h`: SQLite-backed reading plans, plan progress,
  memory review queue, and teaching notes in the same application database
- `app-paths.c` / `app-paths.h`: unified application data directory
  (`~/.local/share/gsword`) and one-time migration from the old
  `~/.local/share/gsword-app` location
- `org.gsword.GSword.gschema.xml`: last module/key, active workspace page,
  multi-pane layout, columns and scroll sync, legacy parallel template modules,
  active plan, devotion
  module, font scale, theme

The reader toolbar and each multi-pane workspace pane use
`../widgets/gsword-reference-chooser.c`.  The reader uses the full path-bar
form; panes use `gsword_reference_chooser_new_compact()` and open the same
book/chapter/verse picker.

Application UI strings use gettext. The `../po/` directory contains
`POTFILES.in`, `LINGUAS`, and translated catalogs such as `zh_CN.po`.
`GSW_LOCALEDIR` can point at a build or installed locale directory during
development.

The application design is documented in `../docs/app-design.md`.

The earlier minimal GTK4 prototype remains in `../examples/gtk.c` for API
experiments and is separate from the installed application.
