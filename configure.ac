# Process this file with autoconf to produce a configure script.
AC_INIT(RtAudio, 5.0.0, gary@music.mcgill.ca, rtaudio)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR(RtAudio.cpp)
AC_CONFIG_FILES([rtaudio.pc Makefile tests/Makefile doc/Makefile doc/doxygen/Doxyfile])
AM_INIT_AUTOMAKE([1.14 -Wall -Werror foreign subdir-objects])

# libtool version: current:revision:age
#
# If the library source code has changed at all since the last update, then
# increment revision (`c:r:a' becomes `c:r+1:a').
#
# If any interfaces have been added, removed, or changed since the last update,
# increment current, and set revision to 0.
#
# If any interfaces have been added since the last public release, then
# increment age.
#
# If any interfaces have been removed since the last public release, then set
# age to 0.
m4_define([lt_current], 6)
m4_define([lt_revision], 0)
m4_define([lt_age], 0)

m4_define([lt_version_info], [lt_current:lt_revision:lt_age])
m4_define([lt_current_minus_age], [m4_eval(lt_current - lt_age)])

SO_VERSION=lt_version_info
AC_SUBST(SO_VERSION)

# Check version number coherency between RtAudio.h and configure.ac
AC_MSG_CHECKING([that version numbers are coherent])
AC_RUN_IFELSE(
   [AC_LANG_PROGRAM([#include <string.h>
                     `grep "define RTAUDIO_VERSION" $srcdir/RtAudio.h`],
                    [return strcmp(RTAUDIO_VERSION, PACKAGE_VERSION);])],
   [AC_MSG_RESULT([yes])],
   [AC_MSG_FAILURE([testing RTAUDIO_VERSION==PACKAGE_VERSION failed, check that RtAudio.h defines RTAUDIO_VERSION as "$PACKAGE_VERSION" or that the first line of configure.ac has been updated.])])

# Enable some nice automake features if they are available
m4_ifdef([AM_MAINTAINER_MODE], [AM_MAINTAINER_MODE])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Fill GXX with something before test.
AC_SUBST( GXX, ["no"] )

dnl Check for pkg-config program, used for configuring some libraries.
m4_define_default([PKG_PROG_PKG_CONFIG],
[AC_MSG_CHECKING([pkg-config])
AC_MSG_RESULT([no])])

PKG_PROG_PKG_CONFIG

dnl If the pkg-config autoconf support isn't installed, define its
dnl autoconf macro to disable any packages depending on it.
m4_define_default([PKG_CHECK_MODULES],
[AC_MSG_CHECKING([$1])
AC_MSG_RESULT([no])
$4])

# Check for debug
AC_MSG_CHECKING(whether to compile debug version)
AC_ARG_ENABLE(debug,
  [AS_HELP_STRING([--enable-debug],[enable various debug output])],
  [: ${CXXFLAGS="-g -O0"};] [AC_DEFINE([__RTAUDIO_DEBUG__])],
  [: ${CXXFLAGS="-O3"}])

# Checks for programs.
AC_PROG_CXX(g++ CC c++ cxx)
AM_PROG_AR
AC_PATH_PROG(AR, ar, no)
if [[ $AR = "no" ]] ; then
    AC_MSG_ERROR("Could not find ar - needed to create a library");
fi

# Initialize libtool
LT_INIT([win32-dll])
AC_CONFIG_MACRO_DIR([m4])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(sys/ioctl.h unistd.h)

# Check compiler and use -Wall if gnu
if test x"$GXX" = "xyes"; then
  CXXFLAGS="${CXXFLAGS} -Wall -Wextra"
  # Add -Werror in debug mode
  if test x"${enable_debug+set}" = xset; then
    CXXFLAGS="${CXXFLAGS} -Werror"
  fi
fi

# Checks for functions
AC_CHECK_FUNC(gettimeofday, [cppflag="$cppflag -DHAVE_GETTIMEOFDAY"], )

# Checks for doxygen
AC_CHECK_PROG( DOXYGEN, [doxygen], [doxygen] )
AM_CONDITIONAL( MAKE_DOC, [test "x${DOXYGEN}" != x] )

# Copy doc files to build dir if necessary
AC_CONFIG_LINKS( [doc/release.txt:doc/release.txt] )
AC_CONFIG_LINKS( [doc/doxygen/footer.html:doc/doxygen/footer.html] )
AC_CONFIG_LINKS( [doc/doxygen/error.txt:doc/doxygen/error.txt] )
AC_CONFIG_LINKS( [doc/doxygen/tutorial.txt:doc/doxygen/tutorial.txt] )
AC_CONFIG_LINKS( [doc/doxygen/compiling.txt:doc/doxygen/compiling.txt] )
AC_CONFIG_LINKS( [doc/doxygen/acknowledge.txt:doc/doxygen/acknowledge.txt] )
AC_CONFIG_LINKS( [doc/doxygen/license.txt:doc/doxygen/license.txt] )
AC_CONFIG_LINKS( [doc/doxygen/header.html:doc/doxygen/header.html] )
AC_CONFIG_LINKS( [doc/doxygen/duplex.txt:doc/doxygen/duplex.txt] )
AC_CONFIG_LINKS( [doc/doxygen/settings.txt:doc/doxygen/settings.txt] )
AC_CONFIG_LINKS( [doc/doxygen/probe.txt:doc/doxygen/probe.txt] )
AC_CONFIG_LINKS( [doc/doxygen/playback.txt:doc/doxygen/playback.txt] )
AC_CONFIG_LINKS( [doc/doxygen/multi.txt:doc/doxygen/multi.txt] )
AC_CONFIG_LINKS( [doc/doxygen/recording.txt:doc/doxygen/recording.txt] )
AC_CONFIG_LINKS( [doc/doxygen/apinotes.txt:doc/doxygen/apinotes.txt] )
AC_CONFIG_LINKS( [doc/images/mcgill.gif:doc/images/mcgill.gif] )
AC_CONFIG_LINKS( [doc/images/ccrma.gif:doc/images/ccrma.gif] )

# Checks for package options and external software
AC_CANONICAL_HOST

AC_SUBST( api, [""] )
AC_SUBST( req, [""] )
AC_MSG_CHECKING(for audio API)

AC_ARG_WITH(jack, [  --with-jack = choose JACK server support (mac and linux only)])
AS_IF([test "x$with_jack" == "xyes"], [
  api="$api -D__UNIX_JACK__"
  AC_MSG_RESULT(using JACK)
  AC_CHECK_LIB(jack, jack_client_open, , AC_MSG_ERROR(JACK support requires the jack library!))])

case $host in
  *-*-netbsd*)
  AS_IF([test "$api" == ""], [
    AC_MSG_RESULT(using OSS)
    api="$api -D__LINUX_OSS__"
    LIBS="$LIBS -lossaudio"
    AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR(RtAudio requires the pthread library!))])
  ;;

  *-*-freebsd*)
  AS_IF([test "$api" == ""], [
    AC_MSG_RESULT(using OSS)
    api="$api -D__LINUX_OSS__"
    LIBS="$LIBS -lossaudio"
    AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR(RtAudio requires the pthread library!))])
  ;;

  *-*-linux*)
  # Look for ALSA flag
  AC_ARG_WITH(alsa, [  --with-alsa = choose native ALSA API support (linux only)])
  AS_IF([test "x$with_alsa" == "xyes"], [
    api="$api -D__LINUX_ALSA__"
    req="$req alsa"
    AC_MSG_RESULT(using ALSA)
    AC_CHECK_LIB(asound, snd_pcm_open, , AC_MSG_ERROR(ALSA support requires the asound library!))])

  # Look for PULSE flag
  AC_ARG_WITH(pulse, [  --with-pulse = choose PulseAudio API support (linux only)])
  AS_IF([test "x$with_pulse" == "xyes"], [
    api="$api -D__LINUX_PULSE__"
    req="$req libpulse-simple"
    AC_MSG_RESULT(using PulseAudio)
    AC_CHECK_LIB(pulse-simple, pa_simple_flush, , AC_MSG_ERROR(PulseAudio support requires the pulse-simple library!))])

  # Look for OSS flag
  AC_ARG_WITH(oss, [  --with-oss = choose OSS API support (unixes)])
  AS_IF([test "x$with_oss" == "xyes"], [
    api="$api -D__LINUX_OSS__"
    AC_MSG_RESULT(using OSS)])

  # If no audio api flags specified, use ALSA
  if [test "$api" == "";] then
    AC_MSG_RESULT(using ALSA)
    AC_SUBST( api, [-D__LINUX_ALSA__] )
    req="$req alsa"
    AC_CHECK_LIB(asound, snd_pcm_open, , AC_MSG_ERROR(ALSA support requires the asound library!))
  fi

  AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR(RtAudio requires the pthread library!))
  ;;

  *-apple*)
  # Look for Core flag
  AC_ARG_WITH(core, [  --with-core = choose CoreAudio API support (mac only)])
  AS_IF([test "x$with_core" == "xyes"], [
    api="$api -D__MACOSX_CORE__"
    AC_MSG_RESULT(using CoreAudio)
    AC_CHECK_HEADER(CoreAudio/CoreAudio.h, [], [AC_MSG_ERROR(CoreAudio header files not found!)] )
    LIBS="$LIBS -framework CoreAudio -framework CoreFoundation" ])

  # If no audio api flags specified, use CoreAudio
  if [test "$api" == ""; ] then
    AC_SUBST( api, [-D__MACOSX_CORE__] )
    AC_MSG_RESULT(using CoreAudio)
    AC_CHECK_HEADER(CoreAudio/CoreAudio.h,
      [],
      [AC_MSG_ERROR(CoreAudio header files not found!)] )
    AC_SUBST( LIBS, ["-framework CoreAudio -framework CoreFoundation"] )
  fi

  AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR(RtAudio requires the pthread library!))
  ;;

  *-mingw32*)
  AC_ARG_WITH(asio, [  --with-asio = choose ASIO API support (windoze only)])
  AS_IF([test "x$with_asio" == "xyes"], [
    api="$api -D__WINDOWS_ASIO__"
    AC_MSG_RESULT(using ASIO)
    AC_SUBST( objects, ["asio.o asiodrivers.o asiolist.o iasiothiscallresolver.o"] ) ])

  # Look for DirectSound flag
  AC_ARG_WITH(ds, [  --with-ds = choose DirectSound API support (windoze only)])
  AS_IF([test "x$with_ds" == "xyes"], [
    api="$api -D__WINDOWS_DS__"
    AC_MSG_RESULT(using DirectSound)
    LIBS="-ldsound -lwinmm $LIBS" ])

  # Look for WASAPI flag
  AC_ARG_WITH(wasapi, [  --with-wasapi = choose Windows Audio Session API support (windoze only)])
  AS_IF([test "x$with_wasapi" == "xyes"], [
    api="$api -D__WINDOWS_WASAPI__"
    AC_MSG_RESULT(using WASAPI)
    LIBS="-lwinmm -luuid -lksuser $LIBS" ])

  # If no audio api flags specified, use DS
  if [test "$api" == "";] then
    AC_SUBST( api, [-D__WINDOWS_DS__] )
    AC_MSG_RESULT(using DirectSound)
    LIBS="-ldsound -lwinmm $LIBS"
  fi

  LIBS="-lole32 $LIBS"
  ;;

  *)
  # Default case for unknown realtime systems.
  AC_MSG_ERROR(Unknown system type for realtime support!)
  ;;
esac

CPPFLAGS="$CPPFLAGS $api"

AC_OUTPUT
