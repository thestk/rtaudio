project('rtaudio', 'cpp')

src = ['RtAudio.cpp']
incdir = include_directories('include')

compiler = meson.get_compiler('cpp')

deps = []
defines = []
deps += dependency('threads')

alsa_dep = dependency('alsa', required: get_option('alsa'))
if alsa_dep.found()
	defines += '-D__LINUX_ALSA__'
	deps += alsa_dep
endif

jack_dep = dependency('jack', required: get_option('jack'))
if jack_dep.found()
	defines += '-D__UNIX_JACK__'
	deps += jack_dep
endif

if get_option('oss') == true
	defines += '-D__LINUX_OSS__'
endif

pulse_dep = dependency('libpulse', required: get_option('pulse'))
if pulse_dep.found()
	defines += '-D__LINUX_PULSE__'
	deps += pulse_dep
endif

core_dep = dependency('appleframeworks', modules: ['CoreAudio', 'CoreFoundation'], required: get_option('core'))
if core_dep.found()
	defines += '-D__MACOSX_CORE__'
	deps += core_dep
endif

dsound_dep = compiler.find_library('dsound', required: get_option('dsound'))
if dsound_dep.found()
	defines += '-D__WINDOWS_DS__'
	deps += dsound_dep
endif

if get_option('wasapi') == true
	deps += compiler.find_library('mfplat', required: true)
	deps += compiler.find_library('mfuuid', required: true)
	deps += compiler.find_library('ksuser', required: true)
	deps += compiler.find_library('wmcodecdspuuid', required: true)
	defines += '-D__WINDOWS_WASAPI__'
endif

if get_option('asio') == true
	src += ['asio.cpp',
		'asiolist.cpp',
		'asiodrivers.cpp',
		'iasiothiscallresolver.cpp']
	defines += '-D__WINDOWS_ASIO__'
endif


if host_machine.system() == 'windows'
	deps += compiler.find_library('ole32', required: true)
	deps += compiler.find_library('winmm', required: true)
endif

rtaudio = library('rtaudio', src, include_directories: incdir, dependencies: deps)
rtaudio_dep = declare_dependency(include_directories : incdir,
  link_with : rtaudio)
