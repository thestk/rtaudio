if (WIN32)
	include_directories(include/)
	add_definitions(-D__WINDOWS_DS__)
	#-D__WINDOWS_ASIO__
endif (WIN32)
cmake_minimum_required(VERSION 2.8.10)
project(RtAudio)

include(CTest)
include(CheckFunctionExists)

option(BUILD_PYTHON "Build PyRtAudio python bindings" OFF)
option(AUDIO_WINDOWS_DS "Build Windows DirectSound API" OFF)
option(AUDIO_WINDOWS_ASIO "Build Windows ASIO API" OFF)
option(AUDIO_LINUX_OSS "Build Linux OSS API" OFF)
option(AUDIO_LINUX_ALSA "Build Linux ALSA API" OFF)
option(AUDIO_LINUX_PULSE "Build Linux PulseAudio API" OFF)
option(AUDIO_UNIX_JACK "Build Unix JACK audio server API" OFF)
option(AUDIO_OSX_CORE "Build Mac OSX CoreAudio API" OFF)


if (UNIX AND NOT APPLE)
	add_definitions(-D__LINUX_PULSE__)
endif (UNIX AND NOT APPLE)

if (APPLE)
	add_definitions(-D__MACOSX_CORE__)
endif (APPLE)

set(rtaudio_SOURCES RtAudio.cpp)

add_library(rtaudio STATIC ${rtaudio_SOURCES})
