project('inputmap', ['c', 'cpp'], default_options : ['cpp_std=gnu++11'])


lemon_sh = find_program('lemon/lemon.sh')
lemon_exe = executable('lemon', ['lemon/lemon.c'])
lemon = generator(lemon_sh,
  dependencies: lemon_exe,
  output  : ['@BASENAME@.cpp', '@BASENAME@.h'],
  arguments : [lemon_exe.full_path(), '@BUILD_DIR@', '@INPUT@', '-T@SOURCE_DIR@/lemon/lempar.c'])

udevdep = meson.get_compiler('cpp').find_library('udev')
includes = include_directories('util')

devinput_src = lemon.process('devinput.lem')

executable('inputmap',
    ['inputmap.cpp', 'inifile.cpp', 'inputdev.cpp', 'outputdev.cpp', 'event-codes.cpp', 'steamcontroller.cpp', 'inputsteam.cpp',
     'devinput-parser.cpp', devinput_src],
    include_directories: includes, 
    dependencies: [udevdep],
    install: true,
)


