# RtAudio

[![Build Status](https://travis-ci.org/thestk/rtaudio.svg?branch=master)](https://travis-ci.org/thestk/rtaudio)

A set of C++ classes that provide a common API for realtime audio input/output across Linux (native ALSA, JACK, PulseAudio and OSS), Macintosh OS X (CoreAudio and JACK), and Windows (DirectSound, ASIO and WASAPI) operating systems.

By Gary P. Scavone, 2001-2019 (and many other developers!)

This distribution of RtAudio contains the following:

- doc:      RtAudio documentation (see doc/html/index.html)
- tests:    example RtAudio programs
- include:  header and source files necessary for ASIO, DS & OSS compilation
- tests/Windows: Visual C++ .net test program workspace and projects

## Overview

RtAudio is a set of C++ classes that provides a common API (Application Programming Interface) for realtime audio input/output across Linux (native ALSA, JACK, PulseAudio and OSS), Macintosh OS X and Windows (DirectSound, ASIO and WASAPI) operating systems.  RtAudio significantly simplifies the process of interacting with computer audio hardware.  It was designed with the following objectives:

  - object-oriented C++ design
  - simple, common API across all supported platforms
  - only one source and one header file for easy inclusion in programming projects
  - allow simultaneous multi-api support
  - support dynamic connection of devices
  - provide extensive audio device parameter control
  - allow audio device capability probing
  - automatic internal conversion for data format, channel number compensation, (de)interleaving, and byte-swapping

RtAudio incorporates the concept of audio streams, which represent audio output (playback) and/or input (recording).  Available audio devices and their capabilities can be enumerated and then specified when opening a stream.  Where applicable, multiple API support can be compiled and a particular API specified when creating an RtAudio instance.  See the \ref apinotes section for information specific to each of the supported audio APIs.

## Further Reading

For complete documentation on RtAudio, see the doc directory of the distribution or surf to http://www.music.mcgill.ca/~gary/rtaudio/.


## Legal and ethical:

The RtAudio license is similar to the MIT License.  Please see [LICENSE](LICENSE).
