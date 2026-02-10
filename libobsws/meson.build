openssl_dep = dependency('openssl', required: true)

libobsws = static_library(
    'libobsws',
    sources: files(['libobsws.c', 'http_parser.c', 'wic.c', 'transport.c']),
    dependencies : [gtkdeps, openssl_dep]
)

libobsws_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: [gtkdeps, openssl_dep],
  link_with: libobsws,
)