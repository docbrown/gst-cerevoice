project('gst-cerevoice', 'c',
  version : '1.0',
  meson_version : '>= 0.36.0',
  default_options : [ 'buildtype=debugoptimized' ])

glib_req = '>= 2.44.0'
gst_req = '>= 1.8.0'

cc = meson.get_compiler('c')

glib_deps = [dependency('glib-2.0', version : glib_req),
             dependency('gobject-2.0', version : glib_req)]
gst_dep = dependency('gstreamer-1.0', version : gst_req)

cerevoice_sdk = get_option('cerevoice_sdk')
cerevoice_lib_dirs = [
  join_paths(cerevoice_sdk, 'cerehts', 'lib'),
  join_paths(cerevoice_sdk, 'cerevoice', 'lib'),
  join_paths(cerevoice_sdk, 'cerevoice_eng', 'lib'),
  join_paths(cerevoice_sdk, 'cerevoice_pmod', 'lib')
]
cerevoice_dep = declare_dependency(
  dependencies : get_option('cerevoice_shared') ? [
    cc.find_library('cerevoice_shared', dirs : cerevoice_lib_dirs),
    cc.find_library('cerevoice_eng_shared', dirs : cerevoice_lib_dirs)
  ] : [
    # The order of these libraries is important.
    cc.find_library('cerevoice_eng', dirs : cerevoice_lib_dirs),
    cc.find_library('cerevoice_pmod', dirs : cerevoice_lib_dirs),
    cc.find_library('cerehts', dirs : cerevoice_lib_dirs),
    cc.find_library('cerevoice', dirs : cerevoice_lib_dirs),
    cc.find_library('m'),
    cc.find_library('stdc++')
  ],
  include_directories :
    include_directories(join_paths(cerevoice_sdk, 'cerevoice_eng', 'include'))
)

shared_library('gstcerevoice', 'gstcerevoice.c',
  dependencies : [ glib_deps, gst_dep, cerevoice_dep ])

