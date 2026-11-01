gtk4_dep = dependency('gtk4', required: false)
adw_dep = dependency('libadwaita-1', required: false)
sqlite3_dep = dependency('sqlite3', required: false)

if gtk4_dep.found() and adw_dep.found() and sqlite3_dep.found()
  gsword_app = executable('gsword',
    ['main.c', 'application.c', 'window.c', 'window-services.c',
     'search-page.c', 'teaching-page.c', 'resource-page.c',
     'notes-page.c', 'plan-page.c', 'devotion-page.c',
     'memory-page.c', 'library-page.c', 'module-sidebar.c',
     'study-panel.c',
     'notes-store.c', 'plan-store.c', 'app-paths.c',
     '../widgets/gsword-reference-chooser.c'],
    include_directories: include_directories('../widgets'),
    dependencies: [gsword_dep, gtk4_dep, adw_dep, sqlite3_dep],
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
