project('libsmarter', meson_version: '>=1.1')

incl = include_directories('include/')

if not get_option('no_install')
	install_headers('include/smarter.hpp')

	pkgconfig = import('pkgconfig')
	pkgconfig.generate(
		name: 'libsmarter',
		description: 'A library for freestanding smart pointers in C++',
		version: meson.project_version(),
	)
endif

libsmarter_dep = declare_dependency(include_directories: incl)
