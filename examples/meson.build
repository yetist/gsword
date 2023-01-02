all_sources = [
	'ciphercng.c',
'installmod.c',
'lastVerseInChapter.c',
'lookup.c',
'parsekey.c',
'search.c',
'simplechapter.c',
'swmgrex.c',
'signal.c',
'versenorm.c',
'verseposition.c',
'verseranges.cpp',
]

foreach src : all_sources
  p = src.split('.')
  exe = executable(p[0], 
      sources: src,
      dependencies : [sword_dep, gobject_dep],
      link_with : [libgsword],
      include_directories : [
	top_srcdir,
	top_libdir,
	],
      install : false,
      )
  test(p[0], exe)
endforeach
