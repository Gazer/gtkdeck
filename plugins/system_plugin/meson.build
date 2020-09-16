system_plugin = static_library(
    'system_plugin',
    sources: files(['system_plugin.c', 'website.c', 'text.c']),
    dependencies : gtkdeps,
    include_directories: deck_plugins_inc
)

system_plugin_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: gtkdeps,
  link_with: system_plugin,
)