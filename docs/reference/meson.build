gtkdoc = dependency('gtk-doc', required: true)

gsword_doc = gnome.gtkdoc('gsword',
  main_xml: 'gsword-docs.xml',
  src_dir: [
    meson.project_source_root() / 'include' / 'gsword',
  ],
  dependencies: [gsword_dep, glib_dep, gobject_dep, gio_dep],
  gobject_typesfile: 'gsword.types',
  content_files: [
    'gsword-overview.xml',
  ],
  scan_args: [
    '--deprecated-guards=GSW_DISABLE_DEPRECATED',
  ],
  mkdb_args: [
    '--xml-mode',
    '--output-format=xml',
  ],
  install: true,
  install_dir: 'gsword',
)

alias_target('docs', gsword_doc)
