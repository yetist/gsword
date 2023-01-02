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

libgsword = shared_library('gsword',
  source_c,
  gsw_marshaller_c,
  gsw_marshaller_h,
  dependencies : [sword_dep, gobject_dep],
  install:true,
)

pkg.generate(
  name: 'gsword',
  version: meson.project_version(),
  description: 'gobject sword library',
  requires_private: 'gobject-2.0',
  filebase: 'gsword',
  libraries: '-lgsword',
  subdirs: 'gsword',
)

#if get_option('introspection')
#  liblunar_date_gir = gnome.generate_gir(liblunar_date,
#    sources: [source_h, source_c, source_enum.get(1)],
#    nsversion : api_version,
#    namespace : 'LunarDate',
#    symbol_prefix : 'lunar',
#    identifier_prefix : 'Lunar',
#    link_with: liblunar_date,
#    includes : ['GObject-2.0'],
#    install : true,
#    install_dir_gir: girdir,
#    install_dir_typelib: typelibdir,
#    extra_args: [ '--c-include=lunar-date/lunar-date.h' ],
#    )
#  if get_option('vapi')
#    lunar_date_vapi = gnome.generate_vapi('lunar-date-' + api_version,
#      sources: liblunar_date_gir.get(0),
#      packages: 'gio-2.0',
#      install: true,
#      install_dir: vapidir,
#      )
#  endif
#endif
