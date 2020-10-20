subdir('libuwsc')

libobsws = static_library(
    'libobsws',
    sources: files(['libobsws.c']),
    dependencies : gtkdeps
)

libobsws_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: gtkdeps + [uws_dep],
  link_with: libobsws,
)