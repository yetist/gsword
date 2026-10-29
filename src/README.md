# gsword application

This directory is reserved for the GTK4 desktop application named **gsword**.

The reusable SWORD module library lives in `../lib/` and is installed as
`libgsword`. The application will consume the public API from
`../include/gsword/` and must not include private implementation headers from
`../lib/` unless the boundary is explicitly documented.

Planned top-level modules:

- `main.c`
- `application.c` / `application.h`
- `window.c` / `window.h`
- `reader/`
- `search/`
- `notes/`
- `library/`
- `settings/`

The application design is documented in `../docs/app-design.md`.

The earlier minimal GTK4 prototype remains in `../examples/gtk.c` until the
production application structure is introduced here.
