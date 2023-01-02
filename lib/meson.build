sword_dep = dependency('sword')
glib_dep = dependency('glib-2.0')
gobject_dep = dependency('gobject-2.0')

gsword_lib = shared_library('gsword',
'flatapi.cpp',
'flatapi.h',
dependencies : sword_dep
)

libgsword = shared_library('gsword2',
'gsw-modinfo.c',
'gsw-manager.cpp',
'gsw-module.cpp',
'gsw-search-hit.c',
'gsw-installer.cpp',
'gsw-status-reporter.cpp',
dependencies : [sword_dep, gobject_dep],
)

t=executable(
  'testing',
  sources : 'test.c',
  dependencies : [sword_dep, glib_dep],
  link_with : libgsword,
  install : false,
)

test('testing', t)
