all_sources = ['ciphercng.c',
'installmgr.c',
'lookup.c',
'parsekey.c',
'flatapisearch.c',
'lastVerseInChapter.cpp',
'simplechapter.cpp',
'swmgrex.cpp',
'versenorm.cpp',
'verseposition.cpp',
'verseranges.cpp' ]

foreach src : all_sources
  p = src.split('.')
  exe = executable(p[0]+'2', 
      sources: src,
      dependencies : [sword_dep, glib_dep],
      link_with : [lib_flat, libgsword],
      include_directories : [
	top_srcdir,
	top_libdir,
	],
      install : false,
      )
  test(p[0]+'2', exe)
endforeach
