if (WIN32)
	include_directories(include/)
	add_definitions(-D__WINDOWS_DS__)
	#-D__WINDOWS_ASIO__
endif (WIN32)

if (UNIX AND NOT APPLE)
	add_definitions(-D__LINUX_PULSE__)
endif (UNIX AND NOT APPLE)

if (APPLE)
	add_definitions(-D__MACOSX_CORE__)
endif (APPLE)

set(rtaudio_SOURCES RtAudio.cpp)

add_library(rtaudio STATIC ${rtaudio_SOURCES})
