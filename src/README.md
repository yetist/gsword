# gsword application

This directory contains the GTK4/libadwaita application named **gsword**.

The reusable SWORD module library lives in `../lib/` and is installed as
`libgsword`. The application consumes only the public API from
`../include/gsword/`.

The window is assembled from page objects. `window.c` is the shell: it owns the
header bar, sidebar, content stack, status bar, global actions, module and
store lifetime, and cross-page navigation. Page modules own their own widgets,
callbacks and local state, and use the narrow services in
`window-private.h` instead of accessing window private data.

Current implementation:

- `main.c`: application entry point
- `application.c` / `application.h`: `AdwApplication`, library-path handling,
  settings fallback, quit action
- `window.c` / `window.h`: window shell, page composition, sidebar/content stack,
  main menu, shortcuts, shared module/reference coordination and page lifetime
- `window-private.h`: internal shared service API for window pages; does not
  expose `GswordWindowPrivate`
- `window-services.c`: shared status/error/page navigation, module metadata,
  store/reference accessors, chapter rendering and view helpers
- `module-sidebar.c` / `module-sidebar.h`: grouped module list, module
  population and type-specific module selection
- `reader-page.c` / `reader-page.h`: scripture text, active verse, range
  selection, highlights, click handling, current-reference publication and
  previous-position persistence
- `reader-toolbar.c` / `reader-toolbar.h`: reference chooser, copy, note,
  highlight, teaching and study-panel controls
- `study-panel.c` / `study-panel.h`: commentary, Strong's, cross-reference and
  lexicon tabs, follow/pin state and study module selection
- `workspace-page.c` / `workspace-page.h`: comparison container, 1–4 columns,
  sync scroll, layout persistence, add/remove/clear and parallel template
- `workspace-pane.c` / `workspace-pane.h`: per-comparison module selection, compact
  reference chooser, rendering, FOLLOW, active verse and drag reorder
- `search-page.c` / `search-page.h`: search UI, result navigation and search
  index actions
- `teaching-page.c` / `teaching-page.h`: teaching workspace, scripture view,
  teaching notes and fullscreen mode
- `resource-page.c` / `resource-page.h`: general book and map viewer, resource
  tree, text/picture rendering and resource navigation
- `notes-page.c` / `notes-page.h`: notes, note search, tags, verse lists and
  Markdown export
- `plan-page.c` / `plan-page.h`: reading plan notebook, plan selection,
  schedule and plan creation/deletion
- `devotion-page.c` / `devotion-page.h`: daily devotion module/date selection
  and text display
- `memory-page.c` / `memory-page.h`: memory verse list, card masking, answer
  checking and review scheduling
- `library-page.c` / `library-page.h`: module library, install sources, remote
  modules, install/update/remove, progress, cancellation and local install
- `preferences-dialog.c` / `preferences-dialog.h`: font scale, color scheme and
  application CSS
- `notes-store.c` / `notes-store.h`: SQLite-backed notes, tags, verse lists and
  highlights in `~/.local/share/gsword/app.db`
- `plan-store.c` / `plan-store.h`: SQLite-backed reading plans, plan progress,
  memory review queue, and teaching notes in the same application database
- `app-paths.c` / `app-paths.h`: unified application data directory
  (`~/.local/share/gsword`) and one-time migration from the old
  `~/.local/share/gsword-app` location
- `org.gsword.GSword.gschema.xml`: last module/key, active workspace page,
  comparison layout, columns and scroll sync, legacy parallel template modules,
  active plan, devotion module, study panel state, font scale, theme

The reader toolbar and each comparison pane uses
`../widgets/gsword-reference-chooser.c`.  The reader uses the full path-bar
form; comparison panes use `gsword_reference_chooser_new_compact()` and open the same
book/chapter/verse picker.

Application UI strings use gettext. The `../po/` directory contains
`POTFILES.in`, `LINGUAS`, and translated catalogs such as `zh_CN.po`.
`GSW_LOCALEDIR` can point at a build or installed locale directory during
development.

The application design is documented in `../docs/app-design.md`. The window
refactor plan and per-stage acceptance records live in
`../docs/window-refactor-plan.md` and `../docs/window-refactor-acceptance.md`.

The earlier minimal GTK4 prototype remains in `../examples/gtk.c` for API
experiments and is separate from the installed application.
