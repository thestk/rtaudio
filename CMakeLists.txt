cmake_minimum_required(VERSION 2.8.10)
project(RtAudio)

include(CTest)
include(CheckFunctionExists)

# Check for Jack (any OS)
find_package(PkgConfig)
pkg_check_modules(jack jack)

# Check for Pulse (any OS)
pkg_check_modules(pulse libpulse-simple)

# Check for known non-Linux unix-likes
if (CMAKE_SYSTEM_NAME MATCHES "kNetBSD.*|NetBSD.*")
  message(STATUS "NetBSD detected, using OSS")
  set(xBSD ON)
elseif(UNIX AND NOT APPLE)
  set(LINUX ON)
endif()

option(BUILD_PYTHON "Build PyRtAudio python bindings" OFF)
option(RTAUDIO_DS "Build DirectSound API" OFF)
option(RTAUDIO_ASIO "Build ASIO API" OFF)
option(RTAUDIO_WASAPI "Build WASAPI API" ${WIN32})
option(RTAUDIO_OSS "Build OSS4 API" ${xBSD})
option(RTAUDIO_ALSA "Build ALSA API" ${LINUX})
option(RTAUDIO_PULSE "Build PulseAudio API" ${pulse_FOUND})
option(RTAUDIO_JACK "Build JACK audio server API" ${jack_FOUND})
option(RTAUDIO_CORE "Build CoreAudio API" ${APPLE})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-D__RTAUDIO_DEBUG__)
endif ()

check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)

if (HAVE_GETTIMEOFDAY)
    add_definitions(-DHAVE_GETTIMEOFDAY)
endif ()

if (CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif (CMAKE_COMPILER_IS_GNUCXX)

set(rtaudio_SOURCES RtAudio.cpp rtaudio_c.cpp)

set(LINKLIBS)
set(PKGCONFIG_REQUIRES)
set(PKGCONFIG_APIS)

# OSS
if (RTAUDIO_OSS)
  set(NEED_PTHREAD ON)
  find_library(OSSAUDIO_LIB ossaudio)
  if (OSSAUDIO_LIB)
    list(APPEND LINKLIBS ossaudio)
    # Note: not an error on some systems
  endif()
  add_definitions(-D__LINUX_OSS__)
endif()

# Pulse
if (RTAUDIO_PULSE)
  set(NEED_PTHREAD ON)
  find_library(PULSE_LIB pulse)
  find_library(PULSESIMPLE_LIB pulse-simple)
  list(APPEND LINKLIBS ${PULSE_LIB} ${PULSESIMPLE_LIB})
  list(APPEND PKGCONFIG_REQUIRES "libpulse-simple")
  list(APPEND PKGCONFIG_APIS "__LINUX_PULSE__")
  add_definitions(-D__LINUX_PULSE__)
endif()

# ALSA
if (RTAUDIO_ALSA)
  set(NEED_PTHREAD ON)
  find_package(ALSA)
  if (NOT ALSA_FOUND)
    message(FATAL_ERROR "ALSA API requested but no ALSA dev libraries found")
  endif()
  include_directories(${ALSA_INCLUDE_DIR})
  list(APPEND LINKLIBS ${ALSA_LIBRARY})
  list(APPEND PKGCONFIG_REQUIRES "alsa")
  list(APPEND PKGCONFIG_APIS "__LINUX_ALSA__")
  add_definitions(-D__LINUX_ALSA__)
endif()

# JACK
if (RTAUDIO_JACK)
  set(NEED_PTHREAD ON)
  find_library(JACK_LIB jack)
  list(APPEND LINKLIBS ${JACK_LIB})
  list(APPEND PKGCONFIG_REQUIRES "jack")
  list(APPEND PKGCONFIG_APIS "__UNIX_JACK__")
  add_definitions(-D__UNIX_JACK__)
endif()

# CoreAudio
if (RTAUDIO_CORE)
  find_library(COREAUDIO_LIB CoreAudio)
  find_library(COREFOUNDATION_LIB CoreFoundation)
  list(APPEND LINKLIBS ${COREAUDIO_LIB} ${COREFOUNDATION_LIB})
  list(APPEND PKGCONFIG_APIS "__MACOSX_CORE__")
  add_definitions(-D__MACOSX_CORE__)
endif()

# ASIO
if (RTAUDIO_ASIO)
  set(NEED_WIN32LIBS ON)
  include_directories(include)
  list(APPEND rtaudio_SOURCES
    include/asio.cpp
    include/asiodrivers.cpp
    include/asiolist.cpp
    include/iasiothiscallresolver.cpp)
  add_definitions(-D__WINDOWS_ASIO__)
  list(APPEND PKGCONFIG_APIS "__WINDOWS_ASIO__")
endif()

# DSound
if (RTAUDIO_DS)
  set(NEED_WIN32LIBS ON)
  add_definitions(-D__WINDOWS_DS__)
  message(STATUS "Using Windows DirectSound")
  list(APPEND LINKLIBS dsound)
  list(APPEND PKGCONFIG_APIS "__WINDOWS_DS__")
endif()

# WASAPI
if (RTAUDIO_WASAPI)
  set(NEED_WIN32LIBS ON)
  add_definitions(-D__WINDOWS_WASAPI__)
  message(STATUS "Using Windows WASAPI")
  list(APPEND LINKLIBS uuid ksuser)
  list(APPEND PKGCONFIG_APIS "__WINDOWS_WASAPI__")
endif()

cmake_policy(SET CMP0042 OLD)
set(PACKAGE_VERSION 5.0.0)
add_library(rtaudio SHARED ${rtaudio_SOURCES})
add_library(rtaudio_static STATIC ${rtaudio_SOURCES})

# Windows libs
if (NEED_WIN32LIBS)
  list(APPEND LINKLIBS winmm ole32)
endif()

# pthread
if (NEED_PTHREAD)
  find_package(Threads REQUIRED
    CMAKE_THREAD_PREFER_PTHREAD
    THREADS_PREFER_PTHREAD_FLAG)
  list(APPEND LINKLIBS Threads::Threads)
endif()

target_link_libraries(rtaudio ${LINKLIBS})

if (BUILD_TESTING)
    add_subdirectory(tests)
endif (BUILD_TESTING)

string(REPLACE ";" " " req "${PKGCONFIG_REQUIRES}")
string(REPLACE ";" " -D" api "${PKGCONFIG_APIS}")
string(PREPEND api "-D")
configure_file("rtaudio.pc.in" "rtaudio.pc" @ONLY)

install(TARGETS rtaudio
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib
      RUNTIME DESTINATION bin)

install(TARGETS rtaudio_static
      ARCHIVE DESTINATION lib)

install(
    FILES RtAudio.h
    DESTINATION include)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/rtaudio.pc
    DESTINATION lib/pkgconfig)
