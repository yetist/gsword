project('gsword', 'c', 'cpp',
  version : '0.1.5',
  license : 'LGPLv2.1+',
  default_options : [
    'warning_level=1',
    'buildtype=debugoptimized',
    ],
  meson_version : '>=0.40.1'
)

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())

sword_dep = dependency('sword')
gobject_dep = dependency('gobject-2.0')

# Setup various paths that subdirectory meson.build files need
package_subdir = get_option('package_subdir')
girdir = join_paths(get_option('datadir'), package_subdir, 'gir-1.0')
typelibdir = join_paths(get_option('libdir'), package_subdir, 'girepository-1.0')
libdir = join_paths(get_option('libdir'), package_subdir)
if package_subdir != ''
  vapidir = join_paths(get_option('datadir'), package_subdir, 'vapi')
else
  vapidir = join_paths(get_option('datadir'), 'vala', 'vapi')
endif

top_srcdir = include_directories('.')
top_libdir = include_directories('./gsword')

subdir('gsword')
subdir('examples')

pkg = import('pkgconfig')
pkg.generate(
  name: 'gsword',
  version: meson.project_version(),
  description: 'sword Gobject binding',
  filebase: 'gsword',
  requires: ['gobject-2.0', 'sword'],
  libraries: '-lgsword',
  subdirs: 'gsword',
)
