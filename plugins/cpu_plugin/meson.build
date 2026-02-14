cpu_plugin = static_library(
    'cpu_plugin',
    sources: files(['cpu_plugin.c']),
    dependencies : gtkdeps,
    include_directories: deck_plugins_inc
)

cpu_plugin_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: gtkdeps,
  link_with: cpu_plugin,
)
