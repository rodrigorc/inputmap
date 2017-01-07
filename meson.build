project('inputmap', 'cpp', default_options : ['cpp_std=gnu++11'])

udevdep = meson.get_compiler('cpp').find_library('udev')
includes = include_directories('util')

executable('inputmap',
    ['inputmap.cpp', 'inifile.cpp', 'inputdev.cpp', 'outputdev.cpp'],
    include_directories: includes, 
    dependencies: [udevdep],
    install: 1,
)


