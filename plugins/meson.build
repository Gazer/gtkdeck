deck_plugins_inc = include_directories('.')

subdir('system_plugin')

dependencies = gtkdeps + [system_plugin_dep]

deck_plugins = static_library(
    'deck_plugins',
    sources: files(['deck_plugin.c', 'test_plugin.c']),
    dependencies : gtkdeps
)

deck_plugins_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: dependencies,
  link_with: deck_plugins,
)