SUBDIRS = . tests
if MAKE_DOC
SUBDIRS += doc
endif

lib_LTLIBRARIES = %D%/librtaudio.la
%C%_librtaudio_la_LDFLAGS = -no-undefined -export-dynamic -version-info @SO_VERSION@
%C%_librtaudio_la_SOURCES = \
  %D%/RtAudio.cpp \
  %D%/RtAudioLinuxALSA.cpp \
  %D%/RtAudioLinuxOSS.cpp \
  %D%/RtAudioLinuxPulseAudio.cpp \
  %D%/RtAudioOSXCoreAudio.cpp \
  %D%/RtAudioUnixJACK.cpp \
  %D%/RtAudioWindowsASIO.cpp \
  %D%/RtAudioWindowsDS.cpp \
  %D%/RtAudioWindowsWASAPI.cpp \
  %D%/RtAudioCommonProtected.cpp

rtaudio_incdir = $(includedir)/rtaudio
rtaudio_inc_HEADERS = \
  %D%/RtAudio.h

pkgconfigdatadir = $(libdir)/pkgconfig
pkgconfigdata_DATA = rtaudio.pc

EXTRA_DIST = autogen.sh readme install.txt contrib include

unify: $(%C%_librtaudio_la_SOURCES)
	@cat $^ >%D%/RtAudioUnified.cpp