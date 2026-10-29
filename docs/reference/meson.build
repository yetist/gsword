gidocgen = find_program('gi-docgen', required: true)

gsword_doc = custom_target('gsword-doc',
  input: [
    gir[0],
    files('GSword.toml'),
    files('overview.md'),
  ],
  output: 'gsword',
  command: [
    gidocgen, 'generate',
    '--quiet',
    '--config', '@INPUT1@',
    '--content-dir', meson.current_source_dir(),
    '--output-dir', '@OUTDIR@/gsword',
    '--no-namespace-dir',
    '@INPUT0@',
  ],
  install: true,
  install_dir: get_option('datadir') / 'doc',
  build_by_default: true,
)

alias_target('docs', gsword_doc)
