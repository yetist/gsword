gtk4_dep = dependency('gtk4', required: false)

if gtk4_dep.found()
  reference_chooser_sources = files(
    'gsword-reference-chooser.c',
  )

  executable('gsword-reference-chooser-demo',
    ['reference-chooser-demo.c'] + reference_chooser_sources,
    include_directories: include_directories('.'),
    dependencies: [gsword_dep, gtk4_dep],
    install: false,
  )

  test_reference_chooser = executable('test-reference-chooser',
    ['test-reference-chooser.c'] + reference_chooser_sources,
    include_directories: include_directories('.'),
    dependencies: [gsword_dep, gtk4_dep],
    c_args: [
      '-DGSW_TEST_LOCALE_DIR="' +
      meson.project_source_root() + '/data/locales.d"',
    ],
  )
  test('reference-chooser', test_reference_chooser, timeout: 30)
endif
