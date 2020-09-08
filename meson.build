project('gtkdeck', 'c')

gnome = import('gnome')

source_root = meson.current_source_dir()
data_dir = join_paths(source_root, '.')

gtkdeps = [
    dependency('gtk+-3.0'),
    dependency('libusb-1.0'),
    dependency('hidapi-libusb'),
]

# Maybe use subdir instead
plugins = [
    static_library('system_plugin',
      files(['system_plugin/system_plugin.c', 'system_plugin/website.c', 'system_plugin/text.c']),
      dependencies : gtkdeps)
]

resource_data = files(
  'main.glade',
)

main_files = files([
    'deck_plugin.c',
    'gtkdeck.c',
    'streamdeck.c',
    'test_plugin.c',
])

main_files += gnome.compile_resources(
  'gtkdeck-resources',
  'gtkdeck.gresource.xml',
  source_dir: data_dir,
  c_name: 'gtkdeck',
  dependencies: resource_data,
  export: true,
)

executable('gtkdeck', main_files, dependencies : gtkdeps, link_with : plugins)