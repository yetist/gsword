gtk4_dep = dependency('gtk4', required: false)
adw_dep = dependency('libadwaita-1', required: false)
sqlite3_dep = dependency('sqlite3', required: false)

if gtk4_dep.found() and adw_dep.found() and sqlite3_dep.found()
  gsword_app = executable('gsword',
    ['main.c', 'application.c', 'window.c', 'notes-store.c'],
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
