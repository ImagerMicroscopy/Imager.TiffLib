project('MeasurementImageStorage', ['cpp', 'c'],
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20', 'default_library=shared'])

cc = meson.get_compiler('cpp')

incDir = include_directories('include')

lnbtiff_dep = dependency('lnbtiff', required : true)


subdir('src')       # stores files to compile in 'MIMSources' variable


# Do not export functions by default on non-Windows.
cpp_args_common = ['-DNOMINMAX']
if host_machine.system() != 'windows' and cc.has_argument('-fvisibility=hidden')
  cpp_args_common += ['-fvisibility=hidden']
endif

library('MeasurementImageStorageDLL',
  MIMSources, 'src/MeasurementImageStorageDLL.cpp',
  include_directories: [incDir],
  dependencies : [lnbtiff_dep],
  name_suffix : 'dll',
  cpp_args : cpp_args_common,
  install : true)
