project('gsword', 'c',
  version: '0.1.0',
  license: 'GPL-2.0-or-later',
  meson_version: '>=0.63.0',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'buildtype=debugoptimized',
  ])

add_project_arguments('-D_GNU_SOURCE', language: 'c')

gsword_localedir = join_paths(
  get_option('prefix'),
  get_option('datadir'),
  'locale',
)
add_project_arguments(
  '-DGETTEXT_PACKAGE="gsword"',
  '-DGSW_LOCALEDIR="' + gsword_localedir + '"',
  language: 'c',
)

gsword_locale_dir = join_paths(
  get_option('prefix'),
  get_option('datadir'),
  'libgsword',
  'locales.d',
)
add_project_arguments(
  '-DGSW_LOCALE_DIR="' + gsword_locale_dir + '"',
  language: 'c',
)

glib_dep = dependency('glib-2.0', version: '>=2.66')
gobject_dep = dependency('gobject-2.0', version: '>=2.66')
gio_dep = dependency('gio-2.0', version: '>=2.66')
gtk4_dep = dependency('gtk4', required: false)
curl_dep = dependency('libcurl', version: '>=7.68')
zlib_dep = dependency('zlib')

bzip2_dep = dependency('bzip2', required: get_option('bzip2'))
lzma_dep = dependency('liblzma', required: get_option('lzma'))

extra_c_args = []
deps = [glib_dep, gobject_dep, gio_dep, curl_dep, zlib_dep]

if bzip2_dep.found()
  extra_c_args += '-DHAVE_BZIP2=1'
  deps += bzip2_dep
endif
if lzma_dep.found()
  extra_c_args += '-DHAVE_LZMA=1'
  deps += lzma_dep
endif

top_inc = include_directories('include', 'lib')

gsword_sources = files(
  'lib/gsw-error.c',
  'lib/gsw-version.c',
  'lib/gsw-enums.c',
  'lib/gsw-utils.c',
  'lib/gsw-lock.c',
  'lib/gsw-cache.c',
  'lib/gsw-options.c',
  'lib/gsw-render.c',
  'lib/gsw-search.c',
  'lib/gsw-config.c',
  'lib/gsw-module-info.c',
  'lib/gsw-manager.c',
  'lib/gsw-status-reporter.c',
  'lib/gsw-install-source.c',
  'lib/gsw-transport.c',
  'lib/gsw-tar.c',
  'lib/gsw-install-manager.c',
  'lib/gsw-versification-data.c',
  'lib/gsw-versification.c',
  'lib/gsw-key.c',
  'lib/gsw-verse-list.c',
  'lib/gsw-locale.c',
  'lib/gsw-tree-key.c',
  'lib/gsw-codec.c',
  'lib/gsw-encoding.c',
  'lib/gsw-cipher.c',
  'lib/gsw-module-backend.c',
  'lib/gsw-backend-utils.c',
  'lib/gsw-rawverse.c',
  'lib/gsw-rawstr.c',
  'lib/gsw-zstr.c',
  'lib/gsw-rawgenbook.c',
  'lib/gsw-rawfiles.c',
  'lib/gsw-hrefcom.c',
  'lib/gsw-zverse.c',
  'lib/gsw-module.c',
)

gsword_lib = library('gsword',
  gsword_sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: extra_c_args,
  version: meson.project_version(),
  soversion: '0',
  install: true,
)

gsword_dep = declare_dependency(
  link_with: gsword_lib,
  include_directories: top_inc,
  dependencies: deps,
)

gsword_headers = files(
  'include/gsword/gsword.h',
  'include/gsword/gsw-version.h',
  'include/gsword/gsw-error.h',
  'include/gsword/gsw-enums.h',
  'include/gsword/gsw-config.h',
  'include/gsword/gsw-key.h',
  'include/gsword/gsw-locale.h',
  'include/gsword/gsw-verse-key.h',
  'include/gsword/gsw-verse-list.h',
  'include/gsword/gsw-str-key.h',
  'include/gsword/gsw-tree-key.h',
  'include/gsword/gsw-versification.h',
  'include/gsword/gsw-module.h',
  'include/gsword/gsw-module-info.h',
  'include/gsword/gsw-manager.h',
  'include/gsword/gsw-search.h',
  'include/gsword/gsw-status-reporter.h',
  'include/gsword/gsw-transport.h',
  'include/gsword/gsw-install-source.h',
  'include/gsword/gsw-install-manager.h',
)

install_headers(
  gsword_headers,
  subdir: 'gsword',
)


pkg = import('pkgconfig')
pkg.generate(
  gsword_lib,
  name: 'gsword',
  description: 'GObject/C SWORD module library',
  version: meson.project_version(),
  subdirs: 'gsword',
  requires: ['glib-2.0', 'gobject-2.0', 'gio-2.0'],
)


gnome = import('gnome')
gir = []
if get_option('introspection') or get_option('docs')
  gir_dep = dependency('gobject-introspection-1.0')
  gir = gnome.generate_gir(
    gsword_lib,
    sources: gsword_sources + gsword_headers,
    namespace: 'GSword',
    nsversion: '0.1',
    identifier_prefix: 'Gsw',
    symbol_prefix: 'gsw_',
    includes: ['GObject-2.0', 'Gio-2.0'],
    install: get_option('introspection'),
    extra_args: ['--c-include=gsword/gsword.h'],
  )

  if get_option('introspection')
    gnome.generate_vapi(
      'gsword',
      sources: gir[0],
      packages: ['glib-2.0', 'gobject-2.0', 'gio-2.0'],
      install: true,
    )
  endif
endif

if get_option('docs')
  subdir('docs/reference')
endif

subdir('data')
subdir('po')
subdir('tests')
subdir('examples')
subdir('src')
