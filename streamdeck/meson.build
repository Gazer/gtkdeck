streamdeck = static_library(
    'streamdeck',
    sources: files(['streamdeck.c']),
    dependencies : gtkdeps
)

streamdeck_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: gtkdeps,
  link_with: streamdeck,
)