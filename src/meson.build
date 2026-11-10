# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com>

adw_dep = dependency('libadwaita-1', version: '>=1.4', required: false)
sqlite3_dep = dependency('sqlite3', required: false)
gtksourceview_dep = dependency('gtksourceview-5', required: false)

if gtk4_dep.found() and adw_dep.found() and sqlite3_dep.found()
  app_deps = [gsword_dep, gtk4_dep, adw_dep, sqlite3_dep]
  app_c_args = [
    '-DGSW_ICON_DIR="' + meson.project_source_root() + '/data/icons"',
  ]
  if gtksourceview_dep.found()
    app_deps += gtksourceview_dep
    app_c_args += '-DHAVE_GTKSOURCEVIEW=1'
  endif

  gsword_app = executable('gsword',
    ['main.c', 'application.c', 'window.c', 'window-services.c', 'reader-history.c',
     'search-page.c', 'teaching-page.c', 'resource-page.c', 'image-utils.c', 'image-view.c',
     'notes-page.c', 'plan-page.c', 'devotion-page.c',
     'memory-page.c', 'library-page.c', 'module-sidebar.c',
     'study-panel.c', 'workspace-page.c', 'workspace-pane.c',
     'reader-page.c', 'reader-toolbar.c', 'preferences-dialog.c',
     'notes-store.c', 'plan-store.c', 'prayer-store.c', 'insight-store.c',
     'bookmark-store.c', 'app-db.c', 'app-schema.c', 'personal-page.c',
     'today-page.c',
     'prayer-page.c', 'insight-page.c', 'bookmark-page.c',
     'collections-page.c', 'app-paths.c', 'app-locale.c',
     'reference-utils.c', 'reference-selection.c', 'verse-preview.c',
     'gsword-page.c', 'window-help.c', 'window-pages.c', 'window-actions.c', 'window-session.c', 'reader-context-menu.c', 'reader-copy.c', 'reader-range.c', 'reader-reference-sync.c', 'notes-editor.c', 'library-module-row.c', 'library-module-details.c', 'library-source-dialog.c', 'library-install-queue.c',
     'gsword-reference-chooser.c', 'gsword-reference-parser.c'],
    dependencies: app_deps,
    c_args: app_c_args,
    install: true,
  )

  install_data(
    'org.gsword.GSword.gschema.xml',
    install_dir: get_option('datadir') / 'glib-2.0' / 'schemas',
  )

  gnome.compile_schemas(
    depend_files: files('org.gsword.GSword.gschema.xml'),
  )
endif
