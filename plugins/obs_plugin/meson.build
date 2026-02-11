obs_plugin = static_library(
    'obs_plugin',
    sources: files([
        'obs_plugin.c',
        'obs_action_types.h',
        'actions/obs_change_scene.c',
        'actions/obs_mute_source.c',
        'actions/obs_toggle_recording.c'
    ]),
    dependencies : gtkdeps,
    include_directories: deck_plugins_inc
)

obs_plugin_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: gtkdeps,
  link_with: obs_plugin,
)