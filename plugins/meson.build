deck_plugins_inc = include_directories('.')

source_root = meson.current_source_dir()
data_dir = join_paths(source_root, '.')
assets_dir = join_paths(source_root, 'assets')

subdir('system_plugin')
subdir('obs_plugin')

dependencies = gtkdeps + [system_plugin_dep, obs_plugin_dep]

resource_data = files(
  'assets/icons8-text-72.png',
  'assets/icons8-website-72.png'
)

plugin_files = files(['deck_plugin.c', 'test_plugin.c'])

main_files += gnome.compile_resources(
  'plugins-resources',
  'plugins.gresource.xml',
  source_dir: assets_dir,
  c_name: 'deck_plugins',
  dependencies: resource_data,
  export: true,
)

deck_plugins = static_library(
    'deck_plugins',
    sources: plugin_files,
    dependencies : gtkdeps
)

deck_plugins_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: dependencies,
  link_with: deck_plugins,
)