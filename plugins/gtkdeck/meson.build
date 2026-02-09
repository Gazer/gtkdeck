gtkdeck_plugin = static_library(
    'gtkdeck_plugin',
    sources: files(['gtkdeck_plugin.c', 'countdown.c']),
    dependencies : gtkdeps,
    include_directories: deck_plugins_inc
)

gtkdeck_plugin_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: gtkdeps,
  link_with: gtkdeck_plugin,
)
