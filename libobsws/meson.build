libobsws = static_library(
    'libobsws',
    sources: files(['libobsws.c', 'http_parser.c', 'wic.c', 'transport.c']),
    dependencies : gtkdeps
)

libobsws_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: gtkdeps,
  link_with: libobsws,
)