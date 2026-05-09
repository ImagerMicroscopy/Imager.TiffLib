project('MeasurementImageStorage', ['cpp', 'c'],
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20', 'default_library=shared'])

# Custom option to toggle Igor plugin build
# Usage: meson setup builddir -Dbuild_igor_plugin=true
build_igor_plugin = get_option('build_igor_plugin')

cc = meson.get_compiler('cpp')
incDir = include_directories('include')

lnbtiff_dep = dependency('lnbtiff', required : true)

subdir('src')  # MIMSources variable defined here

# Common compiler args
cpp_args_common = ['-DNOMINMAX']
if host_machine.system() != 'windows' and cc.has_argument('-fvisibility=hidden')
  cpp_args_common += ['-fvisibility=hidden']
endif

# Causes compiler to report an error if a switch statement
# does not cover all cases for an enum type and does not provide a default.
cpp = meson.get_compiler('cpp')
if cpp.get_id() == 'msvc'
    add_project_arguments('/we4062', language: 'cpp')
else
    add_project_arguments('-Werror=switch', language: 'cpp')
endif

# Always build the standalone DLL
library('MeasurementImageStorageDLL',
  MIMSources, 'src/MeasurementImageStorageDLL.cpp',
  include_directories: [incDir],
  dependencies : [lnbtiff_dep],
  name_suffix : 'dll',
  cpp_args : cpp_args_common,
  install : true)

# Conditionally build Igor XOP plugin
if build_igor_plugin
    subdir('resources')  # igorResource variable defined here

    libnames = ['version']
    libraries = []
    foreach ln : libnames
        libraries += cc.find_library(ln)
    endforeach

    igorincdir = include_directories('../XOPSupport/')
    igorlibdir = join_paths(meson.project_source_root(), '../XOPSupport')
    igorSupportLibDir = join_paths(meson.project_source_root(), '../XOPSupport/VC')
    igorlibs = [cc.find_library('Igor64', dirs:[igorlibdir]),
                cc.find_library('XOPSupport64', dirs:[igorSupportLibDir])]

    library('MeasurementImageStorage-64',
      MIMSources, igor_sources, igorResource,
      include_directories: [incDir, igorincdir],
      dependencies : [libraries, igorlibs, lnbtiff_dep],
      name_suffix : 'xop',
      cpp_args : ['-DNOMINMAX'],
      install : true)
endif
