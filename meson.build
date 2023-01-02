project('gsword', 'c', 'cpp',
  version : '4.0.5',
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
glib_dep = dependency('glib-2.0')
gobject_dep = dependency('gobject-2.0')

top_srcdir = include_directories('.')
top_libdir = include_directories('./lib')

subdir('gsword')
subdir('examples')
