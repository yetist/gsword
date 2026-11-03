executable('gsword-list-modules', 'list-modules.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-lookup', 'lookup.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-module-info', 'module-info.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-install-module', 'install-module.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-install-sources', 'install-sources.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-async-install', 'async-install.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-dict-lookup', 'dict-lookup.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-genbook-walk', 'genbook-walk.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-chapter', 'chapter.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-parse-key', 'parse-key.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-render-options', 'render-options.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-html', 'html.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-locale', 'locale.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-search', 'search.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-index-module', 'index-module.c',
  dependencies: gsword_dep,
  install: false)

executable('gsword-parallel', 'parallel.c',
  dependencies: gsword_dep,
  install: false)

gtk4_dep = dependency('gtk4', required: false)
if gtk4_dep.found()
  executable('gsword-gtk', 'gtk.c',
    dependencies: [gsword_dep, gtk4_dep],
    c_args: ['-Wno-deprecated-declarations'],
    install: false)
endif
