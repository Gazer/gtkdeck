system_monitor_plugin = static_library(
    'system_monitor_plugin',
    sources: files(['system_monitor_plugin.c']),
    dependencies : gtkdeps,
    include_directories: deck_plugins_inc
)

system_monitor_plugin_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: gtkdeps,
  link_with: system_monitor_plugin,
)
