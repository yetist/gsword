# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com>

tests = []
python3 = find_program('python3')

reference_chooser_sources = files(
  '../src/gsword-reference-chooser.c',
  '../src/gsword-reference-parser.c',
)

if gtk4_dep.found()
  executable('gsword-reference-chooser-demo',
    ['reference-chooser-demo.c'] + reference_chooser_sources,
    include_directories: include_directories('../src'),
    dependencies: [gsword_dep, gtk4_dep],
    install: false,
  )
endif

if get_option('tests')
  test_config = executable('test-config', 'test-config.c',
    dependencies: [gsword_dep, glib_dep])
  test('config', test_config)

  test_version = executable('test-version', 'test-version.c',
    dependencies: [gsword_dep, glib_dep])
  test('version', test_version)

  test_utils = executable('test-utils', 'test-utils.c',
    dependencies: [gsword_dep, glib_dep])
  test('utils', test_utils)

  test_manager = executable('test-manager', 'test-manager.c',
    dependencies: [gsword_dep, glib_dep])
  test('manager', test_manager)

  test_app_paths = executable('test-app-paths',
    ['test-app-paths.c', '../src/app-paths.c'],
    include_directories: include_directories('../src'),
    dependencies: [gsword_dep, glib_dep])
  test('app-paths', test_app_paths)

  test_install_source = executable('test-install-source', 'test-install-source.c',
    dependencies: [gsword_dep, glib_dep])
  test('install-source', test_install_source)

  test_local_install = executable('test-local-install', 'test-local-install.c',
    dependencies: [gsword_dep, glib_dep])
  test('local-install', test_local_install)

  test_tar = executable('test-tar', 'test-tar.c',
    dependencies: [gsword_dep, glib_dep, zlib_dep])
  test('tar', test_tar)

  test_verse_key = executable('test-verse-key', 'test-verse-key.c',
    dependencies: [gsword_dep, glib_dep])
  test('verse-key', test_verse_key)

  test_verse_list = executable('test-verse-list', 'test-verse-list.c',
    dependencies: [gsword_dep, glib_dep])
  test('verse-list', test_verse_list)

  test_reader = executable('test-reader', 'test-reader.c',
    dependencies: [gsword_dep, glib_dep, zlib_dep],
    c_args: ['-DGSW_TEST_LOCALE_DIR="' + meson.project_source_root() + '/data/locales.d"'])
  test('reader', test_reader)

  test_genbook_write = executable('test-genbook-write', 'test-genbook-write.c',
    dependencies: [gsword_dep, glib_dep])
  test('genbook-write', test_genbook_write)

  test_cache = executable('test-cache', 'test-cache.c',
    dependencies: [gsword_dep, glib_dep])
  test('cache', test_cache)

  test_concurrent = executable('test-concurrent', 'test-concurrent.c',
    dependencies: [gsword_dep, glib_dep])
  test('concurrent', test_concurrent)

  test_prayer_store = executable('test-prayer-store',
    ['test-prayer-store.c', '../src/prayer-store.c', '../src/app-db.c',
     '../src/app-paths.c'],
    include_directories: include_directories('../src'),
    dependencies: [gsword_dep, glib_dep, dependency('sqlite3')])
  test('prayer-store', test_prayer_store)

  test_app_db = executable('test-app-db',
    ['test-app-db.c', '../src/app-db.c', '../src/app-paths.c'],
    include_directories: include_directories('../src'),
    dependencies: [gsword_dep, glib_dep, dependency('sqlite3')])
  test('app-db', test_app_db)

  test_insight_store = executable('test-insight-store',
    ['test-insight-store.c', '../src/insight-store.c', '../src/app-db.c',
     '../src/app-paths.c'],
    include_directories: include_directories('../src'),
    dependencies: [gsword_dep, glib_dep, dependency('sqlite3')])
  test('insight-store', test_insight_store)

  test_codec = executable('test-codec', 'test-codec.c',
    dependencies: [gsword_dep, glib_dep, zlib_dep],
    c_args: extra_c_args)
  test('codec', test_codec)

  test_status = executable('test-status', 'test-status.c',
    dependencies: [gsword_dep, glib_dep])
  test('status', test_status)

  test_async = executable('test-async', 'test-async.c',
    dependencies: [gsword_dep, glib_dep])
  test('async', test_async)

  test_cipher = executable('test-cipher', 'test-cipher.c',
    dependencies: [gsword_dep, glib_dep])
  test('cipher', test_cipher)

  test_render = executable('test-render', 'test-render.c',
    dependencies: [gsword_dep, glib_dep])
  test('render', test_render)

  test_locale = executable('test-locale', 'test-locale.c',
    dependencies: [gsword_dep, glib_dep],
    c_args: ['-DGSW_TEST_LOCALE_DIR="' + meson.project_source_root() + '/data/locales.d"'])
  test('locale', test_locale)

  test_encoding = executable('test-encoding', 'test-encoding.c',
    dependencies: [gsword_dep, glib_dep])
  test('encoding', test_encoding)

  test_search = executable('test-search', 'test-search.c',
    dependencies: [gsword_dep, glib_dep])
  test('search', test_search)

  test('locales', python3,
    args: [files('../tools/validate-locales.py')],
    timeout: 60)

  desktop_file_validate = find_program('desktop-file-validate', required: false)
  if desktop_file_validate.found()
    test('desktop-file', desktop_file_validate,
      args: [meson.project_source_root() / 'data/org.gsword.GSword.desktop'])
  endif

  appstreamcli = find_program('appstreamcli', required: false)
  if appstreamcli.found()
    test('appstream', appstreamcli,
      args: ['validate', '--no-net',
             meson.project_source_root() / 'data/org.gsword.GSword.metainfo.xml'])
  endif

  if get_option('introspection')
    test('introspection', python3,
      args: [files('test-introspection.py')],
      env: [
        'GI_TYPELIB_PATH=' + meson.project_build_root(),
        'LD_LIBRARY_PATH=' + meson.project_build_root(),
      ],
      depends: gir,
      timeout: 60)
  endif

  test_install_http = executable('test-install-http', 'test-install-http.c',
    dependencies: [gsword_dep, glib_dep])
  test_install_http_async = executable('test-install-http-async',
    'test-install-http-async.c',
    dependencies: [gsword_dep, glib_dep])
  test('install-http', python3,
    args: [files('run-http-install-test.py'), test_install_http,
           test_install_http_async],
    timeout: 120)

  if gtk4_dep.found()
    test_reference_chooser = executable('test-reference-chooser',
      ['test-reference-chooser.c'] + reference_chooser_sources,
      include_directories: include_directories('../src'),
      dependencies: [gsword_dep, gtk4_dep],
      c_args: [
        '-DGSW_TEST_LOCALE_DIR="' +
        meson.project_source_root() + '/data/locales.d"',
      ],
    )
    test('reference-chooser', test_reference_chooser, timeout: 30)
  endif
endif
