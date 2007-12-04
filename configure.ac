# Process this file with autoconf to produce a configure script.
AC_INIT(RtAudio, 4.0, gary@music.mcgill.ca, rtaudio)
AC_CONFIG_SRCDIR(RtAudio.cpp)
AC_CONFIG_FILES([rtaudio-config Makefile tests/Makefile])

# Fill GXX with something before test.
AC_SUBST( GXX, ["no"] )

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX(g++ CC c++ cxx)
AC_PROG_RANLIB
AC_PATH_PROG(AR, ar, no)
if [[ $AR = "no" ]] ; then
    AC_MSG_ERROR("Could not find ar - needed to create a library");
fi

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(sys/ioctl.h unistd.h)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Check for debug
AC_MSG_CHECKING(whether to compile debug version)
AC_ARG_ENABLE(debug,
  [  --enable-debug = enable various debug output],
  [AC_SUBST( debug, [-D__RTAUDIO_DEBUG__] ) AC_SUBST( cflags, [-g] ) AC_SUBST( object_path, [Debug] ) AC_MSG_RESULT(yes)],
  [AC_SUBST( debug, [] ) AC_SUBST( cflags, [-O2] ) AC_SUBST( object_path, [Release] ) AC_MSG_RESULT(no)])

# Checks for functions
AC_CHECK_FUNC(gettimeofday, [cflags=$cflags" -DHAVE_GETTIMEOFDAY"], )

# Check compiler and use -Wall if gnu.
if [test $GXX = "yes" ;] then
  AC_SUBST( warn, [-Wall] )
fi

# Checks for package options and external software
AC_CANONICAL_HOST
AC_MSG_CHECKING(for audio API)
case $host in
  *-*-netbsd*)
    AC_SUBST( sound_api, [-D__LINUX_OSS__] )
    AC_MSG_RESULT(using OSS)
    AC_SUBST( audio_apis, [-D__LINUX_OSS__] )
    cflags=$cflags" -lossaudio"
    AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR(RtAudio requires the pthread library!))
  ;;

  *-*-linux*)
  AC_SUBST( sound_api, [_NO_API_] )
  AC_ARG_WITH(jack, [  --with-jack = choose JACK server support (mac and linux only)], [AC_SUBST( sound_api, [-D__UNIX_JACK__] ) AC_MSG_RESULT(using JACK)], )
  if [test $sound_api = -D__UNIX_JACK__;] then
    TEMP_LIBS=$LIBS
    AC_CHECK_LIB(jack, jack_client_new, , AC_MSG_ERROR(JACK support requires the jack library!))
    AC_CHECK_LIB(asound, snd_pcm_open, , AC_MSG_ERROR(Jack support also requires the asound library!))
    LIBS="`pkg-config --cflags --libs jack` $TEMP_LIBS -lasound"
    audio_apis="-D__UNIX_JACK__"
  fi

  # Look for ALSA flag
  AC_ARG_WITH(alsa, [  --with-alsa = choose native ALSA API support (linux only)], [AC_SUBST( sound_api, [-D__LINUX_ALSA__] ) AC_MSG_RESULT(using ALSA)], )
  if [test $sound_api = -D__LINUX_ALSA__;] then
    AC_CHECK_LIB(asound, snd_pcm_open, , AC_MSG_ERROR(ALSA support requires the asound library!))
    audio_apis="-D__LINUX_ALSA__ $audio_apis"
  fi

  # Look for OSS flag
  AC_ARG_WITH(oss, [  --with-oss = choose OSS API support (linux only)], [AC_SUBST( sound_api, [-D__LINUX_OSS__] ) AC_MSG_RESULT(using OSS)], )
  if test $sound_api = -D__LINUX_OSS__; then
    audio_apis="-D__LINUX_OSS__ $audio_apis"
  fi

  # If no audio api flags specified, use ALSA
  if [test $sound_api = _NO_API_;] then
    AC_MSG_RESULT(using ALSA)
    AC_SUBST( audio_apis, [-D__LINUX_ALSA__] )
    AC_CHECK_LIB(asound, snd_pcm_open, , AC_MSG_ERROR(ALSA support requires the asound library!))
  fi

  AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR(RtAudio requires the pthread library!))
  ;;

  *-apple*)
  AC_SUBST( sound_api, [_NO_API_] )
  AC_ARG_WITH(jack, [  --with-jack = choose JACK server support (unix only)], [AC_SUBST( sound_api, [-D__UNIX_JACK__] ) AC_MSG_RESULT(using JACK)], )
  if [test $sound_api = -D__UNIX_JACK__;] then
    AC_CHECK_LIB(jack, jack_client_new, , AC_MSG_ERROR(JACK support requires the jack library!))
    audio_apis="-D__UNIX_JACK__"
  fi

  # Look for Core flag
  AC_ARG_WITH(core, [  --with-core = choose CoreAudio API support (mac only)], [AC_SUBST( sound_api, [-D__MACOSX_CORE__] ) AC_MSG_RESULT(using CoreAudio)], )
  if test $sound_api = -D__MACOSX_CORE__; then
    AC_CHECK_HEADER(CoreAudio/CoreAudio.h, [], [AC_MSG_ERROR(CoreAudio header files not found!)] )
    AC_SUBST( frameworks, ["-framework CoreAudio -framework CoreFoundation"] )
    audio_apis="-D__MACOSX_CORE__ $audio_apis"
  fi

  # If no audio api flags specified, use CoreAudio
  if [test $sound_api = _NO_API_;] then
    AC_SUBST( sound_api, [-D__MACOSX_CORE__] )
    AC_MSG_RESULT(using CoreAudio)
    AC_CHECK_HEADER(CoreAudio/CoreAudio.h,
      [AC_SUBST( audio_apis, [-D__MACOSX_CORE__] )],
      [AC_MSG_ERROR(CoreAudio header files not found!)] )
    AC_SUBST( frameworks, ["-framework CoreAudio -framework CoreFoundation"] )
  fi

  AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR(RtAudio requires the pthread library!))
  ;;

  *-mingw32*)
  AC_SUBST( sound_api, [_NO_API_] )
  AC_ARG_WITH(asio, [  --with-asio = choose ASIO API support (windoze only)], [AC_SUBST( sound_api, [-D__WINDOWS_ASIO__] ) AC_MSG_RESULT(using ASIO)], )
  if [test $sound_api = -D__WINDOWS_ASIO__;] then
    audio_apis="-D__WINDOWS_ASIO__"
    AC_SUBST( objects, ["asio.o asiodrivers.o asiolist.o iasiothiscallresolver.o"] )
  fi

  # Look for DirectSound flag
  AC_ARG_WITH(ds, [  --with-ds = choose DirectSound API support (windoze only)], [AC_SUBST( sound_api, [-D__WINDOWS_DS__] ) AC_MSG_RESULT(using DirectSound)], )
  if test $sound_api = -D__WINDOWS_DS__; then
    audio_apis="-D__WINDOWS_DS__ $audio_apis"
    LIBS="-ldsound -lwinmm $LIBS"
  fi

  # If no audio api flags specified, use DirectSound
  if [test $sound_api = _NO_API_;] then
    AC_SUBST( sound_api, [-D__WINDOWS_DS__] )
    AC_MSG_RESULT(using DirectSound)
    audio_apis="-D__WINDOWS_DS__"
    LIBS="-ldsound -lwinmm $LIBS"
  fi

  LIBS="-lole32 $LIBS"
  ;;

  *)
  # Default case for unknown realtime systems.
  AC_MSG_ERROR(Unknown system type for realtime support!)
  ;;
esac

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL

AC_OUTPUT

chmod oug+x rtaudio-config
