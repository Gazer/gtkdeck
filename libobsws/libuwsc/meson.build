cc = meson.get_compiler('c')

deps = [
    cc.find_library('libev'),
    dependency('openssl'),
]

uws_files = files([
  'log.c',
  'sha1.c',
  'ssl.c',
  'utils.c',
  'uwsc.c',
  'buffer.c',
])

uws = static_library(
    'libuwsc',
    sources: uws_files,
    dependencies : deps
)

uws_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: deps,
  link_with: uws,
)