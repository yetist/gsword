gnome = import('gnome')
gsw_marshallers = gnome.genmarshal('gsw-marshal',
    sources : 'gsw-marshal.list',
    prefix : 'gsw_marshal',
    )
gsw_marshaller_c = gsw_marshallers[0]
gsw_marshaller_h = gsw_marshallers[1]

install_headers(
'gsw-enum.h',
'gsw-installer.h',
'gsw-manager.h',
'gsw-modinfo.h',
'gsw-module.h',
'gsword.h',
'gsw-search-hit.h',
'gsw-status-reporter.h',
'gsw-verse-key.h',
'gsw-versi-mgr.h',
subdir: 'gsword',
)

libgsword = shared_library('gsword',
'gsw-modinfo.c',
'gsw-manager.cpp',
'gsw-module.cpp',
'gsw-search-hit.c',
'gsw-installer.cpp',
'gsw-status-reporter.cpp',
'gsw-verse-key.cpp',
'gsw-versi-mgr.cpp',
gsw_marshaller_c,
gsw_marshaller_h,
dependencies : [sword_dep, gobject_dep],
install:true,
)
