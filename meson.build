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

glib_dep = dependency('glib-2.0', version: '>=2.66')
gobject_dep = dependency('gobject-2.0', version: '>=2.66')
gio_dep = dependency('gio-2.0', version: '>=2.66')
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

top_inc = include_directories('include', 'src')

gsword_sources = files(
  'src/gsw-error.c',
  'src/gsw-version.c',
  'src/gsw-enums.c',
  'src/gsw-utils.c',
  'src/gsw-config.c',
  'src/gsw-module-info.c',
  'src/gsw-manager.c',
  'src/gsw-status-reporter.c',
  'src/gsw-install-source.c',
  'src/gsw-transport.c',
  'src/gsw-tar.c',
  'src/gsw-install-manager.c',
  'src/gsw-versification-data.c',
  'src/gsw-versification.c',
  'src/gsw-key.c',
  'src/gsw-locale.c',
  'src/gsw-tree-key.c',
  'src/gsw-codec.c',
  'src/gsw-encoding.c',
  'src/gsw-cipher.c',
  'src/gsw-module-backend.c',
  'src/gsw-backend-utils.c',
  'src/gsw-rawverse.c',
  'src/gsw-rawstr.c',
  'src/gsw-zstr.c',
  'src/gsw-rawgenbook.c',
  'src/gsw-rawfiles.c',
  'src/gsw-hrefcom.c',
  'src/gsw-zverse.c',
  'src/gsw-module.c',
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
  'include/gsword/gsw-str-key.h',
  'include/gsword/gsw-tree-key.h',
  'include/gsword/gsw-versification.h',
  'include/gsword/gsw-module.h',
  'include/gsword/gsw-module-info.h',
  'include/gsword/gsw-manager.h',
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
    namespace: 'Gsw',
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

subdir('tests')
subdir('examples')
