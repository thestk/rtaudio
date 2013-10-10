/******************************************/
/*
  RtAudio - realtime sound I/O C++ class
  by Gary P. Scavone, 2001-2002.
*/
/******************************************/

#if !defined(__RTAUDIO_H)
#define __RTAUDIO_H

#include <map>

#if defined(__LINUX_ALSA__)
  #include <alsa/asoundlib.h>
  #include <pthread.h>
  #include <unistd.h>

  #define THREAD_TYPE
  typedef snd_pcm_t *AUDIO_HANDLE;
  typedef int DEVICE_ID;
  typedef pthread_t THREAD_HANDLE;
  typedef pthread_mutex_t MUTEX;

#elif defined(__LINUX_OSS__)
  #include <pthread.h>
  #include <unistd.h>

  #define THREAD_TYPE
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

  #define THREAD_TYPE __stdcall
  typedef LPGUID DEVICE_ID;
  typedef unsigned long THREAD_HANDLE;
  typedef CRITICAL_SECTION MUTEX;

#elif defined(__IRIX_AL__)
  #include <dmedia/audio.h>
  #include <pthread.h>
  #include <unistd.h>

  #define THREAD_TYPE
  typedef ALport AUDIO_HANDLE;
  typedef int DEVICE_ID;
  typedef pthread_t THREAD_HANDLE;
  typedef pthread_mutex_t MUTEX;

#endif


// *************************************************** //
//
// RtError class declaration.
//
// *************************************************** //

class RtError
{
public:
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
  static const RTAUDIO_FORMAT RTAUDIO_SINT8;
  static const RTAUDIO_FORMAT RTAUDIO_SINT16;
  static const RTAUDIO_FORMAT RTAUDIO_SINT24; /*!< Upper 3 bytes of 32-bit integer. */
  static const RTAUDIO_FORMAT RTAUDIO_SINT32;
  static const RTAUDIO_FORMAT RTAUDIO_FLOAT32; /*!< Normalized between plus/minus 1.0. */
  static const RTAUDIO_FORMAT RTAUDIO_FLOAT64; /*!< Normalized between plus/minus 1.0. */

  //static const int MAX_SAMPLE_RATES = 14;
  enum { MAX_SAMPLE_RATES = 14 };

  typedef int (*RTAUDIO_CALLBACK)(char *buffer, int bufferSize, void *userData);

  typedef struct {
    char name[128];
    DEVICE_ID id[2];  /*!< No value reported by getDeviceInfo(). */
    bool probed;       /*!< true if the device capabilities were successfully probed. */
    int maxOutputChannels;
    int maxInputChannels;
    int maxDuplexChannels;
    int minOutputChannels;
    int minInputChannels;
    int minDuplexChannels;
    bool hasDuplexSupport; /*!< true if device supports duplex mode. */
    int nSampleRates;      /*!< Number of discrete rates or -1 if range supported. */
    int sampleRates[MAX_SAMPLE_RATES]; /*!< Supported rates or (min, max) if range. */
    RTAUDIO_FORMAT nativeFormats;     /*!< Bit mask of supported data formats. */
  } RTAUDIO_DEVICE;

  //! The default constructor.
  /*!
    Probes the system to make sure at least one audio
    input/output device is available and determines
    the api-specific identifier for each device found.
    An RtError error can be thrown if no devices are
    found or if a memory allocation error occurs.
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
    and/or after a callback is cancelled).  An RtError will be
    thrown for an invalid device argument.
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

  //! Fill a user-supplied RTAUDIO_DEVICE structure for a specified device.
  /*!
    Any device between 0 and getDeviceCount()-1 is valid.  If a
    device is busy or otherwise unavailable, the structure member
    "probed" has a value of "false".  The system default input and
    output devices are referenced by device identifier = 0.  On
    systems which allow dynamic default device settings, the default
    devices are not identified by name (specific device enumerations
    are assigned device identifiers > 0).  An RtError will be
    thrown for an invalid device argument.
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

protected:

private:

  static const unsigned int SAMPLE_RATES[MAX_SAMPLE_RATES];

  enum { FAILURE, SUCCESS };

  enum STREAM_MODE {
    PLAYBACK,
    RECORD,
    DUPLEX,
    UNINITIALIZED = -75
  };

  enum STREAM_STATE {
    STREAM_STOPPED,
    STREAM_RUNNING
  };

  typedef struct {
    int device[2];          // Playback and record, respectively.
    STREAM_MODE mode;       // PLAYBACK, RECORD, or DUPLEX.
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
    bool usingCallback;
    THREAD_HANDLE thread;
    MUTEX mutex;
    RTAUDIO_CALLBACK callback;
    void *userData;
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

// Uncomment the following definition to have extra information spewed to stderr.
//#define RTAUDIO_DEBUG

#endif
