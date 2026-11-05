# GSword application

This directory contains the GTK4/libadwaita Bible study application **GSword**.

Source repository: <https://github.com/yetist/gsword>

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
- `window.c` / `window.h`: window shell, page composition, three-item main
  navigation (Reading/Compare/Personal), hideable module sidebar (header button
  and `F9`), sidebar/content stack, main menu, shortcuts, shared
  module/reference coordination and page lifetime
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
- `gsword-reference-chooser.c` / `.h`: reusable full and compact
  book/chapter/verse picker used by the reader toolbar and comparison panes
- `gsword-reference-parser.c` / `.h`: locale-aware reference parsing and book
  candidate lookup
- `study-panel.c` / `study-panel.h`: commentary, Strong's, cross-reference and
  lexicon tabs, follow/pin state and study module selection
- `workspace-page.c` / `workspace-page.h`: comparison container, 1–4 columns,
  sync scroll, layout persistence, add/remove/clear and parallel template
- `workspace-pane.c` / `workspace-pane.h`: per-comparison module selection, compact
  reference chooser, rendering, FOLLOW, active verse and drag reorder
- `search-page.c` / `search-page.h`: internal search page opened from the
  header search button or `Ctrl+F`, with asynchronous search/cancel, module and
  range selection, attribute search, recent searches, match highlighting,
  result navigation and per-module index status/actions
- `teaching-page.c` / `teaching-page.h`: teaching workspace, scripture view,
  teaching notes and fullscreen mode
- `resource-page.c` / `resource-page.h`: general book and map viewer, resource
  tree, text/picture rendering and resource navigation
- `notes-page.c` / `notes-page.h`: notes/tags navigation plus an Insights tab,
  Markdown editing with autosave and dirty-state feedback, note search and tag
  filtering, explicit tag menu, Markdown export and a single-verse scripture
  preview; verse-list management now lives in Collections
- `insight-page.c` / `insight-page.h`: free-form insight list/editor, tag
  filter, search, optional verse association and current-verse insertion
- `personal-page.c` / `personal-page.h`: personal center shell with the
  Today/Notes/Prayer/Plans/Collections secondary navigation and adaptive
  switcher bar
- `today-page.c` / `today-page.h`: daily overview cards for continue reading,
  today's plan, active prayers, memory review and recent notes
- `prayer-page.c` / `prayer-page.h`: prayer list, search/filter, editor,
  status transitions, answered notes, related-verse management and prayer-log
  timeline
- `collections-page.c` / `collections-page.h`: Lists/Bookmarks container;
  verse-list and favorites management, including the system favorites list,
  list CRUD and item open/remove
- `bookmark-page.c` / `bookmark-page.h`: bookmark list, add-current-verse,
  open, edit title/note and delete
- `plan-page.c` / `plan-page.h`: reading plan dashboard, template-based
  creation, plan library with rename/duplicate, schedule filters and undo
- `devotion-page.c` / `devotion-page.h`: daily devotion module/date selection
  and text display
- `memory-page.c` / `memory-page.h`: memory verse list, card masking, answer
  checking and review scheduling
- `library-page.c` / `library-page.h`: Installed/Available/Updates library
  views using GtkListView with filter/sort models, installed-module details,
  authenticated source management and connection test, HTTP proxy settings,
  update queue with failed-item retry and pause/resume, disk-space preflight,
  encrypted-module unlock,
  dependency/conflict warnings, local directory picker, install/remove,
  progress and cancellation
- `preferences-dialog.c` / `preferences-dialog.h`: font scale, color scheme and
  application CSS
- `notes-store.c` / `notes-store.h`: SQLite-backed notes, tags, verse lists and
  highlights in `~/.local/share/gsword/app.db`, including updated timestamps,
  tag-filtered note queries and verse-list membership helpers
- `plan-store.c` / `plan-store.h`: SQLite-backed reading plans, plan progress,
  memory review queue, and teaching notes in the same application database
- `prayer-store.c` / `prayer-store.h`: SQLite-backed prayer items, status,
  answered notes, related verses and prayer-log entries in the same application
  database
- `insight-store.c` / `insight-store.h`: SQLite-backed free-form insights,
  optional verse associations and tags in the same application database
- `bookmark-store.c` / `bookmark-store.h`: SQLite-backed reading-position
  bookmarks with title, note and module/key
- `app-db.c` / `app-db.h`: shared SQLite open/exec/transaction helpers and
  `busy_timeout` setup for all application stores
- `app-paths.c` / `app-paths.h`: XDG-based application paths:
  `~/.local/share/gsword` for the database, `~/.local/share/gsword/library`
  for the writable SWORD module library, `~/.config/gsword` for installer
  configuration, and `~/.cache/gsword` for derived data
- `org.gsword.GSword.gschema.xml`: last module/key, active workspace page,
  module sidebar visibility, comparison layout, columns and scroll sync, legacy
  parallel template modules, active plan, devotion module, study panel state,
  font scale, theme

The reader toolbar and each comparison pane use
`gsword-reference-chooser.c`.  The reader uses the full path-bar form;
comparison panes use `gsword_reference_chooser_new_compact()` and open the
same book/chapter/verse picker.

Application UI strings use gettext. The `../po/` directory contains
`POTFILES.in`, `LINGUAS`, and translated catalogs such as `zh_CN.po`.
`GSW_LOCALEDIR` can point at a build or installed locale directory during
development.

The application design is documented in `../docs/app-design.md`. The window
refactor plan and per-stage acceptance records live in
`../docs/window-refactor-plan.md` and `../docs/window-refactor-acceptance.md`.

The earlier minimal GTK4 prototype remains in `../examples/gtk.c` for API
experiments and is separate from the installed application.
