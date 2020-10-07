project('gtkdeck', 'c')

gnome = import('gnome')

source_root = meson.current_source_dir()
data_dir = join_paths(source_root, '.')
assets_dir = join_paths(source_root, 'assets')

gtkdeps = [
    dependency('gtk+-3.0'),
    dependency('libusb-1.0'),
    dependency('hidapi-libusb'),
    dependency('libwebsockets'),
    dependency('json-glib-1.0')
]

main_files = files([
  'ui/gtkdeck.c',
  'ui/deck_grid.c',
  'ui/image_picker.c'
])

subdir('streamdeck')
subdir('plugins')
subdir('libobsws')

dependencies = gtkdeps + [streamdeck_dep, deck_plugins_dep, libobsws_dep]

resource_data = files(
  'assets/main.glade',
  'assets/image_picker.ui',
  'assets/deck_grid.ui',
  'assets/generic_icon.png'
)


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