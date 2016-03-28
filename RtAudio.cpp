/************************************************************************/
/*! \class RtAudio
    \brief Realtime audio i/o C++ classes.

    RtAudio provides a common API (Application Programming Interface)
    for realtime audio input/output across Linux (native ALSA, Jack,
    and OSS), Macintosh OS X (CoreAudio and Jack), and Windows
    (DirectSound, ASIO and WASAPI) operating systems.

    RtAudio WWW site: http://www.music.mcgill.ca/~gary/rtaudio/

    RtAudio: realtime audio i/o C++ classes
    Copyright (c) 2001-2016 Gary P. Scavone

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation files
    (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge,
    publish, distribute, sublicense, and/or sell copies of the Software,
    and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    Any person wishing to distribute modifications to the Software is
    asked to send the modifications to the original developer so that
    they can be incorporated into the canonical version.  This is,
    however, not a binding provision of this license.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/************************************************************************/

// RtAudio: Version 4.1.2

#include "RtAudio.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <algorithm>

// Static variable definitions.
const unsigned int RtApi::MAX_SAMPLE_RATES = 14;
const unsigned int RtApi::SAMPLE_RATES[] = {
  4000, 5512, 8000, 9600, 11025, 16000, 22050,
  32000, 44100, 48000, 88200, 96000, 176400, 192000
};

#if defined(__WINDOWS_DS__) || defined(__WINDOWS_ASIO__) || defined(__WINDOWS_WASAPI__)
  #define MUTEX_INITIALIZE(A) InitializeCriticalSection(A)
  #define MUTEX_DESTROY(A)    DeleteCriticalSection(A)
  #define MUTEX_LOCK(A)       EnterCriticalSection(A)
  #define MUTEX_UNLOCK(A)     LeaveCriticalSection(A)

  #include "tchar.h"

  static std::string convertCharPointerToStdString(const char *text)
  {
    return std::string(text);
  }

  static std::string convertCharPointerToStdString(const wchar_t *text)
  {
    int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    std::string s( length-1, '\0' );
    WideCharToMultiByte(CP_UTF8, 0, text, -1, &s[0], length, NULL, NULL);
    return s;
  }

#elif defined(__LINUX_ALSA__) || defined(__LINUX_PULSE__) || defined(__UNIX_JACK__) || defined(__LINUX_OSS__) || defined(__MACOSX_CORE__)
  // pthread API
  #define MUTEX_INITIALIZE(A) pthread_mutex_init(A, NULL)
  #define MUTEX_DESTROY(A)    pthread_mutex_destroy(A)
  #define MUTEX_LOCK(A)       pthread_mutex_lock(A)
  #define MUTEX_UNLOCK(A)     pthread_mutex_unlock(A)
#else
  #define MUTEX_INITIALIZE(A) abs(*A) // dummy definitions
  #define MUTEX_DESTROY(A)    abs(*A) // dummy definitions
#endif

// *************************************************** //
//
// RtAudio definitions.
//
// *************************************************** //

std::string RtAudio :: getVersion( void ) throw()
{
  return RTAUDIO_VERSION;
}

void RtAudio :: getCompiledApi( std::vector<RtAudio::Api> &apis ) throw()
{
  apis.clear();

  // The order here will control the order of RtAudio's API search in
  // the constructor.
#if defined(__UNIX_JACK__)
  apis.push_back( UNIX_JACK );
#endif
#if defined(__LINUX_ALSA__)
  apis.push_back( LINUX_ALSA );
#endif
#if defined(__LINUX_PULSE__)
  apis.push_back( LINUX_PULSE );
#endif
#if defined(__LINUX_OSS__)
  apis.push_back( LINUX_OSS );
#endif
#if defined(__WINDOWS_ASIO__)
  apis.push_back( WINDOWS_ASIO );
#endif
#if defined(__WINDOWS_WASAPI__)
  apis.push_back( WINDOWS_WASAPI );
#endif
#if defined(__WINDOWS_DS__)
  apis.push_back( WINDOWS_DS );
#endif
#if defined(__MACOSX_CORE__)
  apis.push_back( MACOSX_CORE );
#endif
#if defined(__RTAUDIO_DUMMY__)
  apis.push_back( RTAUDIO_DUMMY );
#endif
}

void RtAudio :: openRtApi( RtAudio::Api api )
{
  if ( rtapi_ )
    delete rtapi_;
  rtapi_ = 0;

#if defined(__UNIX_JACK__)
  if ( api == UNIX_JACK )
    rtapi_ = new RtApiJack();
#endif
#if defined(__LINUX_ALSA__)
  if ( api == LINUX_ALSA )
    rtapi_ = new RtApiAlsa();
#endif
#if defined(__LINUX_PULSE__)
  if ( api == LINUX_PULSE )
    rtapi_ = new RtApiPulse();
#endif
#if defined(__LINUX_OSS__)
  if ( api == LINUX_OSS )
    rtapi_ = new RtApiOss();
#endif
#if defined(__WINDOWS_ASIO__)
  if ( api == WINDOWS_ASIO )
    rtapi_ = new RtApiAsio();
#endif
#if defined(__WINDOWS_WASAPI__)
  if ( api == WINDOWS_WASAPI )
    rtapi_ = new RtApiWasapi();
#endif
#if defined(__WINDOWS_DS__)
  if ( api == WINDOWS_DS )
    rtapi_ = new RtApiDs();
#endif
#if defined(__MACOSX_CORE__)
  if ( api == MACOSX_CORE )
    rtapi_ = new RtApiCore();
#endif
#if defined(__RTAUDIO_DUMMY__)
  if ( api == RTAUDIO_DUMMY )
    rtapi_ = new RtApiDummy();
#endif
}

RtAudio :: RtAudio( RtAudio::Api api )
{
  rtapi_ = 0;

  if ( api != UNSPECIFIED ) {
    // Attempt to open the specified API.
    openRtApi( api );
    if ( rtapi_ ) return;

    // No compiled support for specified API value.  Issue a debug
    // warning and continue as if no API was specified.
    std::cerr << "\nRtAudio: no compiled support for specified API argument!\n" << std::endl;
  }

  // Iterate through the compiled APIs and return as soon as we find
  // one with at least one device or we reach the end of the list.
  std::vector< RtAudio::Api > apis;
  getCompiledApi( apis );
  for ( unsigned int i=0; i<apis.size(); i++ ) {
    openRtApi( apis[i] );
    if ( rtapi_ && rtapi_->getDeviceCount() ) break;
  }

  if ( rtapi_ ) return;

  // It should not be possible to get here because the preprocessor
  // definition __RTAUDIO_DUMMY__ is automatically defined if no
  // API-specific definitions are passed to the compiler. But just in
  // case something weird happens, we'll thow an error.
  std::string errorText = "\nRtAudio: no compiled API support found ... critical error!!\n\n";
  throw( RtAudioError( errorText, RtAudioError::UNSPECIFIED ) );
}

RtAudio :: ~RtAudio() throw()
{
  if ( rtapi_ )
    delete rtapi_;
}

void RtAudio :: openStream( RtAudio::StreamParameters *outputParameters,
                            RtAudio::StreamParameters *inputParameters,
                            RtAudioFormat format, unsigned int sampleRate,
                            unsigned int *bufferFrames,
                            RtAudioCallback callback, void *userData,
                            RtAudio::StreamOptions *options,
                            RtAudioErrorCallback errorCallback )
{
  return rtapi_->openStream( outputParameters, inputParameters, format,
                             sampleRate, bufferFrames, callback,
                             userData, options, errorCallback );
}

// *************************************************** //
//
// Public RtApi definitions (see end of file for
// private or protected utility functions).
//
// *************************************************** //

RtApi :: RtApi()
{
  stream_.state = STREAM_CLOSED;
  stream_.mode = UNINITIALIZED;
  stream_.apiHandle = 0;
  stream_.userBuffer[0] = 0;
  stream_.userBuffer[1] = 0;
  MUTEX_INITIALIZE( &stream_.mutex );
  showWarnings_ = true;
  firstErrorOccurred_ = false;
}

RtApi :: ~RtApi()
{
  MUTEX_DESTROY( &stream_.mutex );
}

void RtApi :: openStream( RtAudio::StreamParameters *oParams,
                          RtAudio::StreamParameters *iParams,
                          RtAudioFormat format, unsigned int sampleRate,
                          unsigned int *bufferFrames,
                          RtAudioCallback callback, void *userData,
                          RtAudio::StreamOptions *options,
                          RtAudioErrorCallback errorCallback )
{
  if ( stream_.state != STREAM_CLOSED ) {
    errorText_ = "RtApi::openStream: a stream is already open!";
    error( RtAudioError::INVALID_USE );
    return;
  }

  // Clear stream information potentially left from a previously open stream.
  clearStreamInfo();

  if ( oParams && oParams->nChannels < 1 ) {
    errorText_ = "RtApi::openStream: a non-NULL output StreamParameters structure cannot have an nChannels value less than one.";
    error( RtAudioError::INVALID_USE );
    return;
  }

  if ( iParams && iParams->nChannels < 1 ) {
    errorText_ = "RtApi::openStream: a non-NULL input StreamParameters structure cannot have an nChannels value less than one.";
    error( RtAudioError::INVALID_USE );
    return;
  }

  if ( oParams == NULL && iParams == NULL ) {
    errorText_ = "RtApi::openStream: input and output StreamParameters structures are both NULL!";
    error( RtAudioError::INVALID_USE );
    return;
  }

  if ( formatBytes(format) == 0 ) {
    errorText_ = "RtApi::openStream: 'format' parameter value is undefined.";
    error( RtAudioError::INVALID_USE );
    return;
  }

  unsigned int nDevices = getDeviceCount();
  unsigned int oChannels = 0;
  if ( oParams ) {
    oChannels = oParams->nChannels;
    if ( oParams->deviceId >= nDevices ) {
      errorText_ = "RtApi::openStream: output device parameter value is invalid.";
      error( RtAudioError::INVALID_USE );
      return;
    }
  }

  unsigned int iChannels = 0;
  if ( iParams ) {
    iChannels = iParams->nChannels;
    if ( iParams->deviceId >= nDevices ) {
      errorText_ = "RtApi::openStream: input device parameter value is invalid.";
      error( RtAudioError::INVALID_USE );
      return;
    }
  }

  bool result;

  if ( oChannels > 0 ) {

    result = probeDeviceOpen( oParams->deviceId, OUTPUT, oChannels, oParams->firstChannel,
                              sampleRate, format, bufferFrames, options );
    if ( result == false ) {
      error( RtAudioError::SYSTEM_ERROR );
      return;
    }
  }

  if ( iChannels > 0 ) {

    result = probeDeviceOpen( iParams->deviceId, INPUT, iChannels, iParams->firstChannel,
                              sampleRate, format, bufferFrames, options );
    if ( result == false ) {
      if ( oChannels > 0 ) closeStream();
      error( RtAudioError::SYSTEM_ERROR );
      return;
    }
  }

  stream_.callbackInfo.callback = (void *) callback;
  stream_.callbackInfo.userData = userData;
  stream_.callbackInfo.errorCallback = (void *) errorCallback;

  if ( options ) options->numberOfBuffers = stream_.nBuffers;
  stream_.state = STREAM_STOPPED;
}

unsigned int RtApi :: getDefaultInputDevice( void )
{
  // Should be implemented in subclasses if possible.
  return 0;
}

unsigned int RtApi :: getDefaultOutputDevice( void )
{
  // Should be implemented in subclasses if possible.
  return 0;
}

void RtApi :: closeStream( void )
{
  // MUST be implemented in subclasses!
  return;
}

bool RtApi :: probeDeviceOpen( unsigned int /*device*/, StreamMode /*mode*/, unsigned int /*channels*/,
                               unsigned int /*firstChannel*/, unsigned int /*sampleRate*/,
                               RtAudioFormat /*format*/, unsigned int * /*bufferSize*/,
                               RtAudio::StreamOptions * /*options*/ )
{
  // MUST be implemented in subclasses!
  return FAILURE;
}

void RtApi :: tickStreamTime( void )
{
  // Subclasses that do not provide their own implementation of
  // getStreamTime should call this function once per buffer I/O to
  // provide basic stream time support.

  stream_.streamTime += ( stream_.bufferSize * 1.0 / stream_.sampleRate );

#if defined( HAVE_GETTIMEOFDAY )
  gettimeofday( &stream_.lastTickTimestamp, NULL );
#endif
}

long RtApi :: getStreamLatency( void )
{
  verifyStream();

  long totalLatency = 0;
  if ( stream_.mode == OUTPUT || stream_.mode == DUPLEX )
    totalLatency = stream_.latency[0];
  if ( stream_.mode == INPUT || stream_.mode == DUPLEX )
    totalLatency += stream_.latency[1];

  return totalLatency;
}

double RtApi :: getStreamTime( void )
{
  verifyStream();

#if defined( HAVE_GETTIMEOFDAY )
  // Return a very accurate estimate of the stream time by
  // adding in the elapsed time since the last tick.
  struct timeval then;
  struct timeval now;

  if ( stream_.state != STREAM_RUNNING || stream_.streamTime == 0.0 )
    return stream_.streamTime;

  gettimeofday( &now, NULL );
  then = stream_.lastTickTimestamp;
  return stream_.streamTime +
    ((now.tv_sec + 0.000001 * now.tv_usec) -
     (then.tv_sec + 0.000001 * then.tv_usec));     
#else
  return stream_.streamTime;
#endif
}

void RtApi :: setStreamTime( double time )
{
  verifyStream();

  if ( time >= 0.0 )
    stream_.streamTime = time;
}

unsigned int RtApi :: getStreamSampleRate( void )
{
 verifyStream();

 return stream_.sampleRate;
}

// Indentation settings for Vim and Emacs
//
// Local Variables:
// c-basic-offset: 2
// indent-tabs-mode: nil
// End:
//
// vim: et sts=2 sw=2