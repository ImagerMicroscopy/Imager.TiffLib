project('MeasurementImageStorage', ['cpp', 'c'],
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20', 'default_library=shared'])

cc = meson.get_compiler('cpp')

incDir = include_directories('include')

lnbtiff_proj = subproject('lnbtiff')
lnbtiff_dep = lnbtiff_proj.get_variable('liblnbtiff_dep')


subdir('src')       # stores files to compile in 'MIMSources' variable
subdir('resources') # stores files to compile in 'igorResource' variable

libnames = ['version']
libraries = []
foreach ln : libnames
    libraries += cc.find_library(ln)
endforeach



library('MeasurementImageStorageDLL',
        MIMSources, 'src/MeasurementImageStorageDLL.cpp',
        include_directories: [incDir],
        dependencies : [libraries, lnbtiff_dep],
        name_suffix : 'dll',
        cpp_args : ['-DNOMINMAX'],
        install : true)
