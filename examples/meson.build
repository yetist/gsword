all_sources = ['ciphercng.cpp',
'flatapiinstallmgr.cpp',
'flatapilookup.c',
'flatapiparsekey.c',
'flatapisearch.c',
'lastVerseInChapter.cpp',
'simplechapter.cpp',
'swmgrex.cpp',
'versenorm.cpp',
'verseposition.cpp',
'verseranges.cpp' ]

foreach src : all_sources
  p = src.split('.')
  exe = executable(p[0], 
      sources: src,
      dependencies : [sword_dep],
      link_with : lib_flat,
      include_directories : [
	top_srcdir,
	top_libdir,
	],
      install : false,
      )
  test(p[0], exe)
endforeach
