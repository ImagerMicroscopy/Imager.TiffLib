project('lnbtiff', ['c', 'cpp'])

include = include_directories('zlib', '.')

sources = [
    'TIFFCompression.cpp',
    'TIFFFile.cpp',
    'TIFFIFD.cpp',
    'TIFFTag.cpp',
    'TIFFUtils.cpp',
    'TIFFWriter.cpp',
]

subdir('zlib')  # stores files to compile in 'zlibSources' variable

liblnbtiff = static_library('lnbtiff',
    [sources, zlibSources],
    include_directories: include,
    cpp_args: ['-DLNBTIFF_CACHEIFDOFFSETS', '-DLNBTIFF_READIFDOFFSETSFROMTAG']
)

liblnbtiff_dep = declare_dependency(
    link_with: liblnbtiff,
    include_directories: include
)
