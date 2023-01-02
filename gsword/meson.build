gnome = import('gnome')
gsw_marshallers = gnome.genmarshal('gsw-marshal',
    sources : 'marshal.list',
    prefix : 'gsw_marshal',
    )
gsw_marshaller_c = gsw_marshallers[0]
gsw_marshaller_h = gsw_marshallers[1]

lib_flat= shared_library('flatapi',
'flatapi.cpp',
'flatapi.h',
dependencies : sword_dep
)

libgsword = shared_library('gsword',
'gsw-modinfo.c',
'gsw-manager.cpp',
'gsw-module.cpp',
'gsw-search-hit.c',
'gsw-installer.cpp',
'gsw-status-reporter.cpp',
'gsw-verse-key.cpp',
gsw_marshaller_c,
gsw_marshaller_h,
dependencies : [sword_dep, gobject_dep],
)

t=executable(
  'testing',
  sources : 'test.c',
  dependencies : [sword_dep, gobject_dep],
  link_with : libgsword,
  install : false,
)

test('testing', t)
