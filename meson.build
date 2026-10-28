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

install_headers(
  'include/gsword/gsword.h',
  'include/gsword/gsw-version.h',
  'include/gsword/gsw-error.h',
  'include/gsword/gsw-enums.h',
  'include/gsword/gsw-config.h',
  'include/gsword/gsw-module-info.h',
  'include/gsword/gsw-manager.h',
  'include/gsword/gsw-status-reporter.h',
  'include/gsword/gsw-transport.h',
  'include/gsword/gsw-install-source.h',
  'include/gsword/gsw-install-manager.h',
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

subdir('tests')
subdir('examples')
