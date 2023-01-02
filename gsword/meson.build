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
