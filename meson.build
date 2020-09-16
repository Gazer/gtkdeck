project('gtkdeck', 'c')

gnome = import('gnome')

source_root = meson.current_source_dir()
data_dir = join_paths(source_root, '.')
assets_dir = join_paths(source_root, 'assets')

gtkdeps = [
    dependency('gtk+-3.0'),
    dependency('libusb-1.0'),
    dependency('hidapi-libusb'),
]

subdir('streamdeck')
subdir('plugins')

dependencies = gtkdeps + [streamdeck_dep, deck_plugins_dep]

resource_data = files(
  'assets/main.glade',
  'assets/generic_icon.png'
)

main_files = files([
    'ui/gtkdeck.c',
])

main_includes = include_directories('streamdeck')

main_files += gnome.compile_resources(
  'gtkdeck-resources',
  'gtkdeck.gresource.xml',
  source_dir: assets_dir,
  c_name: 'gtkdeck',
  dependencies: resource_data,
  export: true,
)

executable('gtkdeck', main_files, include_directories: main_includes, dependencies : dependencies)