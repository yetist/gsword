tests = []

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

  test_install_source = executable('test-install-source', 'test-install-source.c',
    dependencies: [gsword_dep, glib_dep])
  test('install-source', test_install_source)

  test_tar = executable('test-tar', 'test-tar.c',
    dependencies: [gsword_dep, glib_dep, zlib_dep])
  test('tar', test_tar)

  test_verse_key = executable('test-verse-key', 'test-verse-key.c',
    dependencies: [gsword_dep, glib_dep])
  test('verse-key', test_verse_key)

  test_reader = executable('test-reader', 'test-reader.c',
    dependencies: [gsword_dep, glib_dep, zlib_dep])
  test('reader', test_reader)

  test_codec = executable('test-codec', 'test-codec.c',
    dependencies: [gsword_dep, glib_dep, zlib_dep])
  test('codec', test_codec)

  test_install_http = executable('test-install-http', 'test-install-http.c',
    dependencies: [gsword_dep, glib_dep])
  python3 = find_program('python3')
  test('install-http', python3,
    args: [files('run-http-install-test.py'), test_install_http],
    timeout: 120)
endif
