gnome = import('gnome')
pkg = import('pkgconfig')

gsw_marshallers = gnome.genmarshal('gsw-marshal',
  sources : 'gsw-marshal.list',
  prefix : 'gsw_marshal',
)

gsw_marshaller_c = gsw_marshallers[0]
gsw_marshaller_h = gsw_marshallers[1]

source_h = [
  'gsw-enum.h',
  'gsw-installer.h',
  'gsw-manager.h',
  'gsw-modinfo.h',
  'gsw-module.h',
  'gsw-result.h',
  'gsw-status-reporter.h',
  'gsw-verse-key.h',
  'gsw-versi-mgr.h',
  'gsword.h',
]

source_c = [
  'gsw-installer.cpp',
  'gsw-manager.cpp',
  'gsw-modinfo.c',
  'gsw-module.cpp',
  'gsw-result.c',
  'gsw-status-reporter.cpp',
  'gsw-verse-key.cpp',
  'gsw-versi-mgr.cpp',
  ]

install_headers(
  source_h,
  subdir: 'gsword',
)

pkg = import('pkgconfig')
pkg.generate(
  name: 'gsword',
  version: meson.project_version(),
  description: 'sword gobject binding',
  filebase: 'gsword',
  requires: ['gobject-2.0', 'sword'],
  libraries: '-lgsword',
  subdirs: 'gsword',
)

libgsword = shared_library('gsword',
  source_c,
  gsw_marshaller_c,
  gsw_marshaller_h,
  dependencies : [sword_dep, gobject_dep],
  include_directories: top_srcdir,
  install:true,
)

if get_option('introspection')
  gsword_gir = gnome.generate_gir(libgsword,
    sources: [source_h, source_c, gsw_marshaller_h],
    nsversion : api_version,
    namespace : 'GSword',
    symbol_prefix : 'gsw',
    identifier_prefix : 'Gsw',
    link_with: libgsword,
    includes : ['GObject-2.0'],
    install : true,
    install_dir_gir: girdir,
    install_dir_typelib: typelibdir,
    extra_args: ['--c-include=gsword/gsword.h' ],
    )
  if get_option('vapi')
    gsword_vapi = gnome.generate_vapi('gsword',
      sources: gsword_gir.get(0),
      packages: 'gio-2.0',
      install: true,
      install_dir: vapidir,
      )
  endif
endif
