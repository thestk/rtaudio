/************************************************************************/
/*! \class RtAudio
    \brief Realtime audio i/o C++ class.

    RtAudio provides a common API (Application Programming Interface)
    for realtime audio input/output across Linux (native ALSA and
    OSS), SGI, Macintosh OS X (CoreAudio), and Windows (DirectSound
    and ASIO) operating systems.

    RtAudio WWW site: http://www-ccrma.stanford.edu/~gary/rtaudio/

    RtAudio: a realtime audio i/o C++ class
    Copyright (c) 2001-2002 Gary P. Scavone

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
    requested to send the modifications to the original developer so that
    they can be incorporated into the canonical version.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/************************************************************************/

#if !defined(__RTAUDIO_H)
#define __RTAUDIO_H

#include <map>

#if defined(__LINUX_ALSA__)
  #include <alsa/asoundlib.h>
  #include <pthread.h>
  #include <unistd.h>

  typedef snd_pcm_t *AUDIO_HANDLE;
  typedef int DEVICE_ID;
  typedef pthread_t THREAD_HANDLE;
  typedef pthread_mutex_t MUTEX;

#elif defined(__LINUX_OSS__)
  #include <pthread.h>
  #include <unistd.h>

  typedef int AUDIO_HANDLE;
  typedef int DEVICE_ID;
  typedef pthread_t THREAD_HANDLE;
  typedef pthread_mutex_t MUTEX;

#elif defined(__WINDOWS_DS__)
  #include <windows.h>
  #include <process.h>

  // The following struct is used to hold the extra variables
  // specific to the DirectSound implementation.
  typedef struct {
    void * object;
    void * buffer;
    UINT bufferPointer;
  } AUDIO_HANDLE;

  typedef LPGUID DEVICE_ID;
  typedef unsigned long THREAD_HANDLE;
  typedef CRITICAL_SECTION MUTEX;

#elif defined(__WINDOWS_ASIO__)
  #include <windows.h>
  #include <process.h>

  typedef int AUDIO_HANDLE;
  typedef int DEVICE_ID;
  typedef unsigned long THREAD_HANDLE;
  typedef CRITICAL_SECTION MUTEX;

#elif defined(__IRIX_AL__)
  #include <dmedia/audio.h>
  #include <pthread.h>
  #include <unistd.h>

  typedef ALport AUDIO_HANDLE;
  typedef long DEVICE_ID;
  typedef pthread_t THREAD_HANDLE;
  typedef pthread_mutex_t MUTEX;

#elif defined(__MACOSX_CORE__)

  #include <CoreAudio/AudioHardware.h>
  #include <pthread.h>

  typedef unsigned int AUDIO_HANDLE;
  typedef AudioDeviceID DEVICE_ID;
  typedef pthread_t THREAD_HANDLE;
  typedef pthread_mutex_t MUTEX;

#endif


/************************************************************************/
/*! \class RtError
    \brief Exception handling class for RtAudio.

    The RtError class is quite simple but it does allow errors to be
    "caught" by RtError::TYPE. Almost all RtAudio methods can "throw"
    an RtError, most typically if an invalid stream identifier is
    supplied to a method or a driver error occurs. There are a number
    of cases within RtAudio where warning messages may be displayed
    but an exception is not thrown. There is a private RtAudio method,
    error(), which can be modified to globally control how these
    messages are handled and reported.
*/
/************************************************************************/

class RtError
{
public:
  //! Defined RtError types.
  enum TYPE {
    WARNING,
    DEBUG_WARNING,
    UNSPECIFIED,
    NO_DEVICES_FOUND,
    INVALID_DEVICE,
    INVALID_STREAM,
    MEMORY_ERROR,
    INVALID_PARAMETER,
    DRIVER_ERROR,
    SYSTEM_ERROR,
    THREAD_ERROR
  };

protected:
  char error_message[256];
  TYPE type;

public:
  //! The constructor.
  RtError(const char *p, TYPE tipe = RtError::UNSPECIFIED);

  //! The destructor.
  virtual ~RtError(void);

  //! Prints "thrown" error message to stdout.
  virtual void printMessage(void);

  //! Returns the "thrown" error message TYPE.
  virtual const TYPE& getType(void) { return type; }

  //! Returns the "thrown" error message string.
  virtual const char *getMessage(void) { return error_message; }
};


// This public structure type is used to pass callback information
// between the private RtAudio stream structure and global callback
// handling functions.
typedef struct {
  void *object;  // Used as a "this" pointer.
  int streamId;
  DEVICE_ID device[2];
  THREAD_HANDLE thread;
  void *callback;
  void *buffers;
  unsigned long waitTime;
  bool blockTick;
  bool stopStream;
  bool usingCallback;
  void *userData;
} CALLBACK_INFO;


// *************************************************** //
//
// RtAudio class declaration.
//
// *************************************************** //

class RtAudio
{
public:

  // Support for signed integers and floats.  Audio data fed to/from
  // the tickStream() routine is assumed to ALWAYS be in host
  // byte order.  The internal routines will automatically take care of
  // any necessary byte-swapping between the host format and the
  // soundcard.  Thus, endian-ness is not a concern in the following
  // format definitions.
  typedef unsigned long RTAUDIO_FORMAT;
  static const RTAUDIO_FORMAT RTAUDIO_SINT8; /*!< 8-bit signed integer. */
  static const RTAUDIO_FORMAT RTAUDIO_SINT16; /*!< 16-bit signed integer. */
  static const RTAUDIO_FORMAT RTAUDIO_SINT24; /*!< Upper 3 bytes of 32-bit signed integer. */
  static const RTAUDIO_FORMAT RTAUDIO_SINT32; /*!< 32-bit signed integer. */
  static const RTAUDIO_FORMAT RTAUDIO_FLOAT32; /*!< Normalized between plus/minus 1.0. */
  static const RTAUDIO_FORMAT RTAUDIO_FLOAT64; /*!< Normalized between plus/minus 1.0. */

  //static const int MAX_SAMPLE_RATES = 14;
  enum { MAX_SAMPLE_RATES = 14 };

  typedef int (*RTAUDIO_CALLBACK)(char *buffer, int bufferSize, void *userData);

  //! The public device information structure for passing queried values.
  typedef struct {
    char name[128];    /*!< Character string device identifier. */
    DEVICE_ID id[2];  /* No value reported by getDeviceInfo(). */
    bool probed;       /*!< true if the device capabilities were successfully probed. */
    int maxOutputChannels; /*!< Maximum output channels supported by device. */
    int maxInputChannels;  /*!< Maximum input channels supported by device. */
    int maxDuplexChannels; /*!< Maximum simultaneous input/output channels supported by device. */
    int minOutputChannels; /*!< Minimum output channels supported by device. */
    int minInputChannels;  /*!< Minimum input channels supported by device. */
    int minDuplexChannels; /*!< Minimum simultaneous input/output channels supported by device. */
    bool hasDuplexSupport; /*!< true if device supports duplex mode. */
    bool isDefault;        /*!< true if this is the default output or input device. */
    int nSampleRates;      /*!< Number of discrete rates or -1 if range supported. */
    int sampleRates[MAX_SAMPLE_RATES]; /*!< Supported rates or (min, max) if range. */
    RTAUDIO_FORMAT nativeFormats;     /*!< Bit mask of supported data formats. */
  } RTAUDIO_DEVICE;

  //! The default constructor.
  /*!
    Probes the system to make sure at least one audio input/output
    device is available and determines the api-specific identifier for
    each device found.  An RtError error can be thrown if no devices
    are found or if a memory allocation error occurs.
  */
  RtAudio();

  //! A constructor which can be used to open a stream during instantiation.
  /*!
    The specified output and/or input device identifiers correspond
    to those enumerated via the getDeviceInfo() method.  If device =
    0, the default or first available devices meeting the given
    parameters is selected.  If an output or input channel value is
    zero, the corresponding device value is ignored.  When a stream is
    successfully opened, its identifier is returned via the "streamId"
    pointer.  An RtError can be thrown if no devices are found
    for the given parameters, if a memory allocation error occurs, or
    if a driver error occurs. \sa openStream()
  */
  RtAudio(int *streamId,
          int outputDevice, int outputChannels,
          int inputDevice, int inputChannels,
          RTAUDIO_FORMAT format, int sampleRate,
          int *bufferSize, int numberOfBuffers);

  //! The destructor.
  /*!
    Stops and closes any open streams and devices and deallocates
    buffer and structure memory.
  */
  ~RtAudio();

  //! A public method for opening a stream with the specified parameters.
  /*!
    If successful, the opened stream ID is returned.  Otherwise, an
    RtError is thrown.

    \param outputDevice: If equal to 0, the default or first device
           found meeting the given parameters is opened.  Otherwise, the
           device number should correspond to one of those enumerated via
           the getDeviceInfo() method.
    \param outputChannels: The desired number of output channels.  If
           equal to zero, the outputDevice identifier is ignored.
    \param inputDevice: If equal to 0, the default or first device
           found meeting the given parameters is opened.  Otherwise, the
           device number should correspond to one of those enumerated via
           the getDeviceInfo() method.
    \param inputChannels: The desired number of input channels.  If
           equal to zero, the inputDevice identifier is ignored.
    \param format: An RTAUDIO_FORMAT specifying the desired sample data format.
    \param sampleRate: The desired sample rate (sample frames per second).
    \param *bufferSize: A pointer value indicating the desired internal buffer
           size in sample frames.  The actual value used by the device is
           returned via the same pointer.  A value of zero can be specified,
           in which case the lowest allowable value is determined.
    \param numberOfBuffers: A value which can be used to help control device
           latency.  More buffers typically result in more robust performance,
           though at a cost of greater latency.  A value of zero can be
           specified, in which case the lowest allowable value is used.
  */
  int openStream(int outputDevice, int outputChannels,
                 int inputDevice, int inputChannels,
                 RTAUDIO_FORMAT format, int sampleRate,
                 int *bufferSize, int numberOfBuffers);

  //! A public method which sets a user-defined callback function for a given stream.
  /*!
    This method assigns a callback function to a specific,
    previously opened stream for non-blocking stream functionality.  A
    separate process is initiated, though the user function is called
    only when the stream is "running" (between calls to the
    startStream() and stopStream() methods, respectively).  The
    callback process remains active for the duration of the stream and
    is automatically shutdown when the stream is closed (via the
    closeStream() method or by object destruction).  The callback
    process can also be shutdown and the user function de-referenced
    through an explicit call to the cancelStreamCallback() method.
    Note that a single stream can use only blocking or callback
    functionality at the same time, though it is possible to alternate
    modes on the same stream through the use of the
    setStreamCallback() and cancelStreamCallback() methods (the
    blocking tickStream() method can be used before a callback is set
    and/or after a callback is cancelled).  An RtError will be thrown
    for an invalid device argument.
  */
  void setStreamCallback(int streamId, RTAUDIO_CALLBACK callback, void *userData);

  //! A public method which cancels a callback process and function for a given stream.
  /*!
    This method shuts down a callback process and de-references the
    user function for a specific stream.  Callback functionality can
    subsequently be restarted on the stream via the
    setStreamCallback() method.  An RtError will be thrown for an
    invalid device argument.
   */
  void cancelStreamCallback(int streamId);

  //! A public method which returns the number of audio devices found.
  int getDeviceCount(void);

  //! Fill a user-supplied RTAUDIO_DEVICE structure for a specified device number.
  /*!
    Any device integer between 1 and getDeviceCount() is valid.  If
    a device is busy or otherwise unavailable, the structure member
    "probed" will have a value of "false" and all other members are
    undefined.  If the specified device is the current default input
    or output device, the "isDefault" member will have a value of
    "true".  An RtError will be thrown for an invalid device argument.
  */
  void getDeviceInfo(int device, RTAUDIO_DEVICE *info);

  //! A public method which returns a pointer to the buffer for an open stream.
  /*!
    The user should fill and/or read the buffer data in interleaved format
    and then call the tickStream() method.  An RtError will be
    thrown for an invalid stream identifier.
  */
  char * const getStreamBuffer(int streamId);

  //! Public method used to trigger processing of input/output data for a stream.
  /*!
    This method blocks until all buffer data is read/written.  An
    RtError will be thrown for an invalid stream identifier or if
    a driver error occurs.
  */
  void tickStream(int streamId);

  //! Public method which closes a stream and frees any associated buffers.
  /*!
    If an invalid stream identifier is specified, this method
    issues a warning and returns (an RtError is not thrown).
  */
  void closeStream(int streamId);

  //! Public method which starts a stream.
  /*!
    An RtError will be thrown for an invalid stream identifier
    or if a driver error occurs.
  */
  void startStream(int streamId);

  //! Stop a stream, allowing any samples remaining in the queue to be played out and/or read in.
  /*!
    An RtError will be thrown for an invalid stream identifier
    or if a driver error occurs.
  */
  void stopStream(int streamId);

  //! Stop a stream, discarding any samples remaining in the input/output queue.
  /*!
    An RtError will be thrown for an invalid stream identifier
    or if a driver error occurs.
  */
  void abortStream(int streamId);

  //! Queries a stream to determine whether a call to the tickStream() method will block.
  /*!
    A return value of 0 indicates that the stream will NOT block.  A positive
    return value indicates the number of sample frames that cannot yet be
    processed without blocking.
  */
  int streamWillBlock(int streamId);

#if (defined(__MACOSX_CORE__) || defined(__WINDOWS_ASIO__))
  // This function is intended for internal use only.  It must be
  // public because it is called by the internal callback handler,
  // which is not a member of RtAudio.  External use of this function
  // will most likely produce highly undesireable results!
  void callbackEvent(int streamId, DEVICE_ID deviceId, void *inData, void *outData);
#endif

protected:

private:

  static const unsigned int SAMPLE_RATES[MAX_SAMPLE_RATES];

  enum { FAILURE, SUCCESS };

  enum STREAM_MODE {
    OUTPUT,
    INPUT,
    DUPLEX,
    UNINITIALIZED = -75
  };

  enum STREAM_STATE {
    STREAM_STOPPED,
    STREAM_RUNNING
  };

  typedef struct {
    int device[2];          // Playback and record, respectively.
    STREAM_MODE mode;       // OUTPUT, INPUT, or DUPLEX.
    AUDIO_HANDLE handle[2]; // Playback and record handles, respectively.
    STREAM_STATE state;     // STOPPED or RUNNING
    char *userBuffer;
    char *deviceBuffer;
    bool doConvertBuffer[2]; // Playback and record, respectively.
    bool deInterleave[2];    // Playback and record, respectively.
    bool doByteSwap[2];      // Playback and record, respectively.
    int sampleRate;
    int bufferSize;
    int nBuffers;
    int nUserChannels[2];    // Playback and record, respectively.
    int nDeviceChannels[2];  // Playback and record channels, respectively.
    RTAUDIO_FORMAT userFormat;
    RTAUDIO_FORMAT deviceFormat[2]; // Playback and record, respectively.
    MUTEX mutex;
    CALLBACK_INFO callbackInfo;
  } RTAUDIO_STREAM;

  typedef signed short INT16;
  typedef signed int INT32;
  typedef float FLOAT32;
  typedef double FLOAT64;

  char message[256];
  int nDevices;
  RTAUDIO_DEVICE *devices;

  std::map<int, void *> streams;

  //! Private error method to allow global control over error handling.
  void error(RtError::TYPE type);

  /*!
    Private method to count the system audio devices, allocate the
    RTAUDIO_DEVICE structures, and probe the device capabilities.
  */
  void initialize(void);

  /*!
    Private method which returns the index in the devices array to
    the default input device.
  */
  int getDefaultInputDevice(void);

  /*!
    Private method which returns the index in the devices array to
    the default output device.
  */
  int getDefaultOutputDevice(void);

  //! Private method to clear an RTAUDIO_DEVICE structure.
  void clearDeviceInfo(RTAUDIO_DEVICE *info);

  /*!
    Private method which attempts to fill an RTAUDIO_DEVICE
    structure for a given device.  If an error is encountered during
    the probe, a "warning" message is reported and the value of
    "probed" remains false (no exception is thrown).  A successful
    probe is indicated by probed = true.
  */
  void probeDeviceInfo(RTAUDIO_DEVICE *info);

  /*!
    Private method which attempts to open a device with the given parameters.
    If an error is encountered during the probe, a "warning" message is
    reported and FAILURE is returned (no exception is thrown). A
    successful probe is indicated by a return value of SUCCESS.
  */
  bool probeDeviceOpen(int device, RTAUDIO_STREAM *stream,
                       STREAM_MODE mode, int channels, 
                       int sampleRate, RTAUDIO_FORMAT format,
                       int *bufferSize, int numberOfBuffers);

  /*!
    Private common method used to check validity of a user-passed
    stream ID.  When the ID is valid, this method returns a pointer to
    an RTAUDIO_STREAM structure (in the form of a void pointer).
    Otherwise, an "invalid identifier" exception is thrown.
  */
  void *verifyStream(int streamId);

  /*!
    Private method used to perform format, channel number, and/or interleaving
    conversions between the user and device buffers.
  */
  void convertStreamBuffer(RTAUDIO_STREAM *stream, STREAM_MODE mode);

  //! Private method used to perform byte-swapping on buffers.
  void byteSwapBuffer(char *buffer, int samples, RTAUDIO_FORMAT format);

  //! Private method which returns the number of bytes for a given format.
  int formatBytes(RTAUDIO_FORMAT format);
};

// Define the following flag to have extra information spewed to stderr.
//#define __RTAUDIO_DEBUG__

#endif
