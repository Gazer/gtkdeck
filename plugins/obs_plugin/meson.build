obs_plugin = static_library(
    'obs_plugin',
    sources: files(['obs_plugin.c', 'change_scene.c']),
    dependencies : gtkdeps,
    include_directories: deck_plugins_inc
)

obs_plugin_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: gtkdeps,
  link_with: obs_plugin,
)