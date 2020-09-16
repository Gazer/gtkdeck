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

# Maybe use subdir instead
plugins = [
    static_library('system_plugin',
      files(['system_plugin/system_plugin.c', 'system_plugin/website.c', 'system_plugin/text.c']),
      dependencies : gtkdeps),
  static_library('streamdeck',
      files(['streamdeck/streamdeck.c']),
      dependencies : gtkdeps)
]

resource_data = files(
  'assets/main.glade',
  'assets/generic_icon.png'
)

main_files = files([
    'deck_plugin.c',
    'ui/gtkdeck.c',
    'test_plugin.c',
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

executable('gtkdeck', main_files, include_directories: main_includes, dependencies : gtkdeps, link_with : plugins)