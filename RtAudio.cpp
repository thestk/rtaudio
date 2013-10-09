/******************************************/
/*
  RtAudio - realtime sound I/O C++ class
  by Gary P. Scavone, 2001-2002.
*/
/******************************************/

#include "RtAudio.h"
#include <vector>
#include <stdio.h>

// Static variable definitions.
const unsigned int RtAudio :: SAMPLE_RATES[] = {
  4000, 5512, 8000, 9600, 11025, 16000, 22050,
  32000, 44100, 48000, 88200, 96000, 176400, 192000
};
const RtAudio::RTAUDIO_FORMAT RtAudio :: RTAUDIO_SINT8 = 1;
const RtAudio::RTAUDIO_FORMAT RtAudio :: RTAUDIO_SINT16 = 2;
const RtAudio::RTAUDIO_FORMAT RtAudio :: RTAUDIO_SINT24 = 4;
const RtAudio::RTAUDIO_FORMAT RtAudio :: RTAUDIO_SINT32 = 8;
const RtAudio::RTAUDIO_FORMAT RtAudio :: RTAUDIO_FLOAT32 = 16;
const RtAudio::RTAUDIO_FORMAT RtAudio :: RTAUDIO_FLOAT64 = 32;

#if defined(__WINDOWS_DS__)
  #define MUTEX_INITIALIZE(A) InitializeCriticalSection(A)
  #define MUTEX_LOCK(A)       EnterCriticalSection(A)
  #define MUTEX_UNLOCK(A)     LeaveCriticalSection(A)
  typedef unsigned THREAD_RETURN;
  typedef unsigned (__stdcall THREAD_FUNCTION)(void *);
#else // pthread API
  #define MUTEX_INITIALIZE(A) pthread_mutex_init(A, NULL)
  #define MUTEX_LOCK(A)       pthread_mutex_lock(A)
  #define MUTEX_UNLOCK(A)     pthread_mutex_unlock(A)
  typedef void * THREAD_RETURN;
#endif

// *************************************************** //
//
// Public common (OS-independent) methods.
//
// *************************************************** //

RtAudio :: RtAudio()
{
  initialize();

  if (nDevices <= 0) {
    sprintf(message, "RtAudio: no audio devices found!");
    error(RtError::NO_DEVICES_FOUND);
 }
}

RtAudio :: RtAudio(int *streamId,
                   int outputDevice, int outputChannels,
                   int inputDevice, int inputChannels,
                   RTAUDIO_FORMAT format, int sampleRate,
                   int *bufferSize, int numberOfBuffers)
{
  initialize();

  if (nDevices <= 0) {
    sprintf(message, "RtAudio: no audio devices found!");
    error(RtError::NO_DEVICES_FOUND);
  }

  try {
    *streamId = openStream(outputDevice, outputChannels, inputDevice, inputChannels,
                           format, sampleRate, bufferSize, numberOfBuffers);
  }
  catch (RtError &exception) {
    // deallocate the RTAUDIO_DEVICE structures
    if (devices) free(devices);
    error(exception.getType());
  }
}

RtAudio :: ~RtAudio()
{
  // close any existing streams
  while ( streams.size() )
    closeStream( streams.begin()->first );

  // deallocate the RTAUDIO_DEVICE structures
  if (devices) free(devices);
}

int RtAudio :: openStream(int outputDevice, int outputChannels,
                          int inputDevice, int inputChannels,
                          RTAUDIO_FORMAT format, int sampleRate,
                          int *bufferSize, int numberOfBuffers)
{
  static int streamKey = 0; // Unique stream identifier ... OK for multiple instances.

  if (outputChannels < 1 && inputChannels < 1) {
    sprintf(message,"RtAudio: one or both 'channel' parameters must be greater than zero.");
    error(RtError::INVALID_PARAMETER);
  }

  if ( formatBytes(format) == 0 ) {
    sprintf(message,"RtAudio: 'format' parameter value is undefined.");
    error(RtError::INVALID_PARAMETER);
  }

  if ( outputChannels > 0 ) {
    if (outputDevice >= nDevices || outputDevice < 0) {
      sprintf(message,"RtAudio: 'outputDevice' parameter value (%d) is invalid.", outputDevice);
      error(RtError::INVALID_PARAMETER);
    }
  }

  if ( inputChannels > 0 ) {
    if (inputDevice >= nDevices || inputDevice < 0) {
      sprintf(message,"RtAudio: 'inputDevice' parameter value (%d) is invalid.", inputDevice);
      error(RtError::INVALID_PARAMETER);
    }
  }

  // Allocate a new stream structure.
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) calloc(1, sizeof(RTAUDIO_STREAM));
  if (stream == NULL) {
    sprintf(message, "RtAudio: memory allocation error!");
    error(RtError::MEMORY_ERROR);
  }
  streams[++streamKey] = (void *) stream;
  stream->mode = UNINITIALIZED;
  MUTEX_INITIALIZE(&stream->mutex);

  bool result = SUCCESS;
  int device;
  STREAM_MODE mode;
  int channels;
  if ( outputChannels > 0 ) {

    device = outputDevice;
    mode = PLAYBACK;
    channels = outputChannels;

    if (device == 0) { // Try default device first.
      for (int i=0; i<nDevices; i++) {
        if (devices[i].probed == false) {
          // If the device wasn't successfully probed before, try it
          // again now.
          clearDeviceInfo(&devices[i]);
          probeDeviceInfo(&devices[i]);
          if (devices[i].probed == false)
            continue;
        }
        result = probeDeviceOpen(i, stream, mode, channels, sampleRate,
                                 format, bufferSize, numberOfBuffers);
        if (result == SUCCESS)
          break;
      }
    }
    else {
      result = probeDeviceOpen(device, stream, mode, channels, sampleRate,
                               format, bufferSize, numberOfBuffers);
    }
  }

  if ( inputChannels > 0 && result == SUCCESS ) {

    device = inputDevice;
    mode = RECORD;
    channels = inputChannels;

    if (device == 0) { // Try default device first.
      for (int i=0; i<nDevices; i++) {
        if (devices[i].probed == false) {
          // If the device wasn't successfully probed before, try it
          // again now.
          clearDeviceInfo(&devices[i]);
          probeDeviceInfo(&devices[i]);
          if (devices[i].probed == false)
            continue;
        }
        result = probeDeviceOpen(i, stream, mode, channels, sampleRate,
                                 format, bufferSize, numberOfBuffers);
        if (result == SUCCESS)
          break;
      }
    }
    else {
      result = probeDeviceOpen(device, stream, mode, channels, sampleRate,
                               format, bufferSize, numberOfBuffers);
    }
  }

  if ( result == SUCCESS )
    return streamKey;

  // If we get here, all attempted probes failed.  Close any opened
  // devices and delete the allocated stream.
  closeStream(streamKey);
  sprintf(message,"RtAudio: no devices found for given parameters.");
  error(RtError::INVALID_PARAMETER);

  return -1;
}

int RtAudio :: getDeviceCount(void)
{
  return nDevices;
}

void RtAudio :: getDeviceInfo(int device, RTAUDIO_DEVICE *info)
{
  if (device >= nDevices || device < 0) {
    sprintf(message, "RtAudio: invalid device specifier (%d)!", device);
    error(RtError::INVALID_DEVICE);
  }

  // If the device wasn't successfully probed before, try it again.
  if (devices[device].probed == false) {
    clearDeviceInfo(&devices[device]);
    probeDeviceInfo(&devices[device]);
  }

  // Clear the info structure.
  memset(info, 0, sizeof(RTAUDIO_DEVICE));

  strncpy(info->name, devices[device].name, 128);
  info->probed = devices[device].probed;
  if ( info->probed == true ) {
    info->maxOutputChannels = devices[device].maxOutputChannels;
    info->maxInputChannels = devices[device].maxInputChannels;
    info->maxDuplexChannels = devices[device].maxDuplexChannels;
    info->minOutputChannels = devices[device].minOutputChannels;
    info->minInputChannels = devices[device].minInputChannels;
    info->minDuplexChannels = devices[device].minDuplexChannels;
    info->hasDuplexSupport = devices[device].hasDuplexSupport;
    info->nSampleRates = devices[device].nSampleRates;
    if (info->nSampleRates == -1) {
      info->sampleRates[0] = devices[device].sampleRates[0];
      info->sampleRates[1] = devices[device].sampleRates[1];
    }
    else {
      for (int i=0; i<info->nSampleRates; i++)
        info->sampleRates[i] = devices[device].sampleRates[i];
    }
    info->nativeFormats = devices[device].nativeFormats;
  }

  return;
}

char * const RtAudio :: getStreamBuffer(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  return stream->userBuffer;
}

// This global structure is used to pass information to the thread
// function.  I tried other methods but had intermittent errors due to
// variable persistence during thread startup.
struct {
  RtAudio *object;
  int streamId;
} thread_info;

extern "C" THREAD_RETURN THREAD_TYPE callbackHandler(void * ptr);

void RtAudio :: setStreamCallback(int streamId, RTAUDIO_CALLBACK callback, void *userData)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  stream->callback = callback;
  stream->userData = userData;
  stream->usingCallback = true;
  thread_info.object = this;
  thread_info.streamId = streamId;

  int err = 0;
#if defined(__WINDOWS_DS__)
  unsigned thread_id;
  stream->thread = _beginthreadex(NULL, 0, &callbackHandler,
                                  &stream->usingCallback, 0, &thread_id);
  if (stream->thread == 0) err = -1;
  // When spawning multiple threads in quick succession, it appears to be
  // necessary to wait a bit for each to initialize ... another windism!
  Sleep(1);
#else
  err = pthread_create(&stream->thread, NULL, callbackHandler, &stream->usingCallback);
#endif

  if (err) {
    stream->usingCallback = false;
    sprintf(message, "RtAudio: error starting callback thread!");
    error(RtError::THREAD_ERROR);
  }
}

// *************************************************** //
//
// OS/API-specific methods.
//
// *************************************************** //

#if defined(__LINUX_ALSA__)

#define MAX_DEVICES 16

void RtAudio :: initialize(void)
{
  int card, result, device;
  char name[32];
  char deviceNames[MAX_DEVICES][32];
  snd_ctl_t *handle;
  snd_ctl_card_info_t *info;
  snd_ctl_card_info_alloca(&info);

  // Count cards and devices
  nDevices = 0;
  card = -1;
  snd_card_next(&card);
  while ( card >= 0 ) {
    sprintf(name, "hw:%d", card);
    result = snd_ctl_open(&handle, name, 0);
    if (result < 0) {
      sprintf(message, "RtAudio: ALSA control open (%i): %s.", card, snd_strerror(result));
      error(RtError::WARNING);
      goto next_card;
		}
    result = snd_ctl_card_info(handle, info);
		if (result < 0) {
      sprintf(message, "RtAudio: ALSA control hardware info (%i): %s.", card, snd_strerror(result));
      error(RtError::WARNING);
      goto next_card;
		}
		device = -1;
		while (1) {
      result = snd_ctl_pcm_next_device(handle, &device);
			if (result < 0) {
        sprintf(message, "RtAudio: ALSA control next device (%i): %s.", card, snd_strerror(result));
        error(RtError::WARNING);
        break;
      }
			if (device < 0)
        break;
      sprintf( deviceNames[nDevices++], "hw:%d,%d", card, device );
      if ( nDevices > MAX_DEVICES ) break;
    }
    if ( nDevices > MAX_DEVICES ) break;
  next_card:
    snd_ctl_close(handle);
    snd_card_next(&card);
  }

  if (nDevices == 0) return;

  //  Allocate the RTAUDIO_DEVICE structures.
  devices = (RTAUDIO_DEVICE *) calloc(nDevices, sizeof(RTAUDIO_DEVICE));
  if (devices == NULL) {
    sprintf(message, "RtAudio: memory allocation error!");
    error(RtError::MEMORY_ERROR);
  }

  // Write device ascii identifiers to device structures and then
  // probe the device capabilities.
  for (int i=0; i<nDevices; i++) {
    strncpy(devices[i].name, deviceNames[i], 32);
    probeDeviceInfo(&devices[i]);
  }

  return;
}

void RtAudio :: probeDeviceInfo(RTAUDIO_DEVICE *info)
{
  int err;
  int open_mode = SND_PCM_ASYNC;
  snd_pcm_t *handle;
  snd_pcm_stream_t stream;

  // First try for playback
  stream = SND_PCM_STREAM_PLAYBACK;
  err = snd_pcm_open(&handle, info->name, stream, open_mode);
  if (err < 0) {
    sprintf(message, "RtAudio: ALSA pcm playback open (%s): %s.",
            info->name, snd_strerror(err));
    error(RtError::WARNING);
    goto capture_probe;
  }

  snd_pcm_hw_params_t *params;
  snd_pcm_hw_params_alloca(&params);

  // We have an open device ... allocate the parameter structure.
  err = snd_pcm_hw_params_any(handle, params);
  if (err < 0) {
    snd_pcm_close(handle);
    sprintf(message, "RtAudio: ALSA hardware probe error (%s): %s.",
            info->name, snd_strerror(err));
    error(RtError::WARNING);
    goto capture_probe;
  }

  // Get output channel information.
  info->minOutputChannels = snd_pcm_hw_params_get_channels_min(params);
  info->maxOutputChannels = snd_pcm_hw_params_get_channels_max(params);

  snd_pcm_close(handle);

 capture_probe:
  // Now try for capture
  stream = SND_PCM_STREAM_CAPTURE;
  err = snd_pcm_open(&handle, info->name, stream, open_mode);
  if (err < 0) {
    sprintf(message, "RtAudio: ALSA pcm capture open (%s): %s.",
            info->name, snd_strerror(err));
    error(RtError::WARNING);
    if (info->maxOutputChannels == 0)
      // didn't open for playback either ... device invalid
      return;
    goto probe_parameters;
  }

  // We have an open capture device ... allocate the parameter structure.
  err = snd_pcm_hw_params_any(handle, params);
  if (err < 0) {
    snd_pcm_close(handle);
    sprintf(message, "RtAudio: ALSA hardware probe error (%s): %s.",
            info->name, snd_strerror(err));
    error(RtError::WARNING);
    if (info->maxOutputChannels > 0)
      goto probe_parameters;
    else
      return;
  }

  // Get input channel information.
  info->minInputChannels = snd_pcm_hw_params_get_channels_min(params);
  info->maxInputChannels = snd_pcm_hw_params_get_channels_max(params);

  // If device opens for both playback and capture, we determine the channels.
  if (info->maxOutputChannels == 0 || info->maxInputChannels == 0)
    goto probe_parameters;

  info->hasDuplexSupport = true;
  info->maxDuplexChannels = (info->maxOutputChannels > info->maxInputChannels) ?
    info->maxInputChannels : info->maxOutputChannels;
  info->minDuplexChannels = (info->minOutputChannels > info->minInputChannels) ?
    info->minInputChannels : info->minOutputChannels;

  snd_pcm_close(handle);

 probe_parameters:
  // At this point, we just need to figure out the supported data
  // formats and sample rates.  We'll proceed by opening the device in
  // the direction with the maximum number of channels, or playback if
  // they are equal.  This might limit our sample rate options, but so
  // be it.

  if (info->maxOutputChannels >= info->maxInputChannels)
    stream = SND_PCM_STREAM_PLAYBACK;
  else
    stream = SND_PCM_STREAM_CAPTURE;

  err = snd_pcm_open(&handle, info->name, stream, open_mode);
  if (err < 0) {
    sprintf(message, "RtAudio: ALSA pcm (%s) won't reopen during probe: %s.",
            info->name, snd_strerror(err));
    error(RtError::WARNING);
    return;
  }

  // We have an open device ... allocate the parameter structure.
  err = snd_pcm_hw_params_any(handle, params);
  if (err < 0) {
    snd_pcm_close(handle);
    sprintf(message, "RtAudio: ALSA hardware reopen probe error (%s): %s.",
            info->name, snd_strerror(err));
    error(RtError::WARNING);
    return;
  }

  // Test a non-standard sample rate to see if continuous rate is supported.
  int dir = 0;
  if (snd_pcm_hw_params_test_rate(handle, params, 35500, dir) == 0) {
    // It appears that continuous sample rate support is available.
    info->nSampleRates = -1;
    info->sampleRates[0] = snd_pcm_hw_params_get_rate_min(params, &dir);
    info->sampleRates[1] = snd_pcm_hw_params_get_rate_max(params, &dir);
  }
  else {
    // No continuous rate support ... test our discrete set of sample rate values.
    info->nSampleRates = 0;
    for (int i=0; i<MAX_SAMPLE_RATES; i++) {
      if (snd_pcm_hw_params_test_rate(handle, params, SAMPLE_RATES[i], dir) == 0) {
        info->sampleRates[info->nSampleRates] = SAMPLE_RATES[i];
        info->nSampleRates++;
      }
    }
    if (info->nSampleRates == 0) {
      snd_pcm_close(handle);
      return;
    }
  }

  // Probe the supported data formats ... we don't care about endian-ness just yet
  snd_pcm_format_t format;
  info->nativeFormats = 0;
  format = SND_PCM_FORMAT_S8;
  if (snd_pcm_hw_params_test_format(handle, params, format) == 0)
    info->nativeFormats |= RTAUDIO_SINT8;
  format = SND_PCM_FORMAT_S16;
  if (snd_pcm_hw_params_test_format(handle, params, format) == 0)
    info->nativeFormats |= RTAUDIO_SINT16;
  format = SND_PCM_FORMAT_S24;
  if (snd_pcm_hw_params_test_format(handle, params, format) == 0)
    info->nativeFormats |= RTAUDIO_SINT24;
  format = SND_PCM_FORMAT_S32;
  if (snd_pcm_hw_params_test_format(handle, params, format) == 0)
    info->nativeFormats |= RTAUDIO_SINT32;
  format = SND_PCM_FORMAT_FLOAT;
  if (snd_pcm_hw_params_test_format(handle, params, format) == 0)
    info->nativeFormats |= RTAUDIO_FLOAT32;
  format = SND_PCM_FORMAT_FLOAT64;
  if (snd_pcm_hw_params_test_format(handle, params, format) == 0)
    info->nativeFormats |= RTAUDIO_FLOAT64;

  // Check that we have at least one supported format
  if (info->nativeFormats == 0) {
    snd_pcm_close(handle);
    sprintf(message, "RtAudio: ALSA PCM device (%s) data format not supported by RtAudio.",
            info->name);
    error(RtError::WARNING);
    return;
  }

  // That's all ... close the device and return
  snd_pcm_close(handle);
  info->probed = true;
  return;
}

bool RtAudio :: probeDeviceOpen(int device, RTAUDIO_STREAM *stream,
                                STREAM_MODE mode, int channels, 
                                int sampleRate, RTAUDIO_FORMAT format,
                                int *bufferSize, int numberOfBuffers)
{
#if defined(RTAUDIO_DEBUG)
  snd_output_t *out;
  snd_output_stdio_attach(&out, stderr, 0);
#endif

  // I'm not using the "plug" interface ... too much inconsistent behavior.
  const char *name = devices[device].name;

  snd_pcm_stream_t alsa_stream;
  if (mode == PLAYBACK)
    alsa_stream = SND_PCM_STREAM_PLAYBACK;
  else
    alsa_stream = SND_PCM_STREAM_CAPTURE;

  int err;
  snd_pcm_t *handle;
  int alsa_open_mode = SND_PCM_ASYNC;
  err = snd_pcm_open(&handle, name, alsa_stream, alsa_open_mode);
  if (err < 0) {
    sprintf(message,"RtAudio: ALSA pcm device (%s) won't open: %s.",
            name, snd_strerror(err));
    error(RtError::WARNING);
    return FAILURE;
  }

  // Fill the parameter structure.
  snd_pcm_hw_params_t *hw_params;
  snd_pcm_hw_params_alloca(&hw_params);
  err = snd_pcm_hw_params_any(handle, hw_params);
  if (err < 0) {
    snd_pcm_close(handle);
    sprintf(message, "RtAudio: ALSA error getting parameter handle (%s): %s.",
            name, snd_strerror(err));
    error(RtError::WARNING);
    return FAILURE;
  }

#if defined(RTAUDIO_DEBUG)
  fprintf(stderr, "\nRtAudio: ALSA dump hardware params just after device open:\n\n");
  snd_pcm_hw_params_dump(hw_params, out);
#endif

  // Set access ... try interleaved access first, then non-interleaved
  err = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
  if (err < 0) {
    // No interleave support ... try non-interleave.
		err = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED);
    if (err < 0) {
      snd_pcm_close(handle);
      sprintf(message, "RtAudio: ALSA error setting access ( (%s): %s.",
              name, snd_strerror(err));
      error(RtError::WARNING);
      return FAILURE;
    }
    stream->deInterleave[mode] = true;
  }

  // Determine how to set the device format.
  stream->userFormat = format;
  snd_pcm_format_t device_format;

  if (format == RTAUDIO_SINT8)
    device_format = SND_PCM_FORMAT_S8;
  else if (format == RTAUDIO_SINT16)
    device_format = SND_PCM_FORMAT_S16;
  else if (format == RTAUDIO_SINT24)
    device_format = SND_PCM_FORMAT_S24;
  else if (format == RTAUDIO_SINT32)
    device_format = SND_PCM_FORMAT_S32;
  else if (format == RTAUDIO_FLOAT32)
    device_format = SND_PCM_FORMAT_FLOAT;
  else if (format == RTAUDIO_FLOAT64)
    device_format = SND_PCM_FORMAT_FLOAT64;

  if (snd_pcm_hw_params_test_format(handle, hw_params, device_format) == 0) {
    stream->deviceFormat[mode] = format;
    goto set_format;
  }

  // The user requested format is not natively supported by the device.
  device_format = SND_PCM_FORMAT_FLOAT64;
  if (snd_pcm_hw_params_test_format(handle, hw_params, device_format) == 0) {
    stream->deviceFormat[mode] = RTAUDIO_FLOAT64;
    goto set_format;
  }

  device_format = SND_PCM_FORMAT_FLOAT;
  if (snd_pcm_hw_params_test_format(handle, hw_params, device_format) == 0) {
    stream->deviceFormat[mode] = RTAUDIO_FLOAT32;
    goto set_format;
  }

  device_format = SND_PCM_FORMAT_S32;
  if (snd_pcm_hw_params_test_format(handle, hw_params, device_format) == 0) {
    stream->deviceFormat[mode] = RTAUDIO_SINT32;
    goto set_format;
  }

  device_format = SND_PCM_FORMAT_S24;
  if (snd_pcm_hw_params_test_format(handle, hw_params, device_format) == 0) {
    stream->deviceFormat[mode] = RTAUDIO_SINT24;
    goto set_format;
  }

  device_format = SND_PCM_FORMAT_S16;
  if (snd_pcm_hw_params_test_format(handle, hw_params, device_format) == 0) {
    stream->deviceFormat[mode] = RTAUDIO_SINT16;
    goto set_format;
  }

  device_format = SND_PCM_FORMAT_S8;
  if (snd_pcm_hw_params_test_format(handle, hw_params, device_format) == 0) {
    stream->deviceFormat[mode] = RTAUDIO_SINT8;
    goto set_format;
  }

  // If we get here, no supported format was found.
  sprintf(message,"RtAudio: ALSA pcm device (%s) data format not supported by RtAudio.", name);
  snd_pcm_close(handle);
  error(RtError::WARNING);
  return FAILURE;

 set_format:
  err = snd_pcm_hw_params_set_format(handle, hw_params, device_format);
  if (err < 0) {
    snd_pcm_close(handle);
    sprintf(message, "RtAudio: ALSA error setting format (%s): %s.",
            name, snd_strerror(err));
    error(RtError::WARNING);
    return FAILURE;
  }

  // Determine whether byte-swaping is necessary.
  stream->doByteSwap[mode] = false;
  if (device_format != SND_PCM_FORMAT_S8) {
    err = snd_pcm_format_cpu_endian(device_format);
    if (err == 0)
      stream->doByteSwap[mode] = true;
    else if (err < 0) {
      snd_pcm_close(handle);
      sprintf(message, "RtAudio: ALSA error getting format endian-ness (%s): %s.",
              name, snd_strerror(err));
      error(RtError::WARNING);
      return FAILURE;
    }
  }

  // Determine the number of channels for this device.  We support a possible
  // minimum device channel number > than the value requested by the user.
  stream->nUserChannels[mode] = channels;
  int device_channels = snd_pcm_hw_params_get_channels_max(hw_params);
  if (device_channels < channels) {
    snd_pcm_close(handle);
    sprintf(message, "RtAudio: channels (%d) not supported by device (%s).",
            channels, name);
    error(RtError::WARNING);
    return FAILURE;
  }

  device_channels = snd_pcm_hw_params_get_channels_min(hw_params);
  if (device_channels < channels) device_channels = channels;
  stream->nDeviceChannels[mode] = device_channels;

  // Set the device channels.
  err = snd_pcm_hw_params_set_channels(handle, hw_params, device_channels);
  if (err < 0) {
    snd_pcm_close(handle);
    sprintf(message, "RtAudio: ALSA error setting channels (%d) on device (%s): %s.",
            device_channels, name, snd_strerror(err));
    error(RtError::WARNING);
    return FAILURE;
  }

  // Set the sample rate.
  err = snd_pcm_hw_params_set_rate(handle, hw_params, (unsigned int)sampleRate, 0);
  if (err < 0) {
    snd_pcm_close(handle);
    sprintf(message, "RtAudio: ALSA error setting sample rate (%d) on device (%s): %s.",
            sampleRate, name, snd_strerror(err));
    error(RtError::WARNING);
    return FAILURE;
  }

  // Set the buffer number, which in ALSA is referred to as the "period".
  int dir;
  int periods = numberOfBuffers;
  // Even though the hardware might allow 1 buffer, it won't work reliably.
  if (periods < 2) periods = 2;
  err = snd_pcm_hw_params_get_periods_min(hw_params, &dir);
  if (err > periods) periods = err;

  err = snd_pcm_hw_params_set_periods(handle, hw_params, periods, 0);
  if (err < 0) {
    snd_pcm_close(handle);
    sprintf(message, "RtAudio: ALSA error setting periods (%s): %s.",
            name, snd_strerror(err));
    error(RtError::WARNING);
    return FAILURE;
  }

  // Set the buffer (or period) size.
  err = snd_pcm_hw_params_get_period_size_min(hw_params, &dir);
  if (err > *bufferSize) *bufferSize = err;

  err = snd_pcm_hw_params_set_period_size(handle, hw_params, *bufferSize, 0);
  if (err < 0) {
    snd_pcm_close(handle);
    sprintf(message, "RtAudio: ALSA error setting period size (%s): %s.",
            name, snd_strerror(err));
    error(RtError::WARNING);
    return FAILURE;
  }

  stream->bufferSize = *bufferSize;

  // Install the hardware configuration
  err = snd_pcm_hw_params(handle, hw_params);
  if (err < 0) {
    snd_pcm_close(handle);
    sprintf(message, "RtAudio: ALSA error installing hardware configuration (%s): %s.",
            name, snd_strerror(err));
    error(RtError::WARNING);
    return FAILURE;
  }

#if defined(RTAUDIO_DEBUG)
  fprintf(stderr, "\nRtAudio: ALSA dump hardware params after installation:\n\n");
  snd_pcm_hw_params_dump(hw_params, out);
#endif

  /*
  // Install the software configuration
  snd_pcm_sw_params_t *sw_params = NULL;
  snd_pcm_sw_params_alloca(&sw_params);
  snd_pcm_sw_params_current(handle, sw_params);
  err = snd_pcm_sw_params(handle, sw_params);
  if (err < 0) {
    snd_pcm_close(handle);
    sprintf(message, "RtAudio: ALSA error installing software configuration (%s): %s.",
            name, snd_strerror(err));
    error(RtError::WARNING);
    return FAILURE;
  }
  */

  // Set handle and flags for buffer conversion
  stream->handle[mode] = handle;
  stream->doConvertBuffer[mode] = false;
  if (stream->userFormat != stream->deviceFormat[mode])
    stream->doConvertBuffer[mode] = true;
  if (stream->nUserChannels[mode] < stream->nDeviceChannels[mode])
    stream->doConvertBuffer[mode] = true;
  if (stream->nUserChannels[mode] > 1 && stream->deInterleave[mode])
    stream->doConvertBuffer[mode] = true;

  // Allocate necessary internal buffers
  if ( stream->nUserChannels[0] != stream->nUserChannels[1] ) {

    long buffer_bytes;
    if (stream->nUserChannels[0] >= stream->nUserChannels[1])
      buffer_bytes = stream->nUserChannels[0];
    else
      buffer_bytes = stream->nUserChannels[1];

    buffer_bytes *= *bufferSize * formatBytes(stream->userFormat);
    if (stream->userBuffer) free(stream->userBuffer);
    stream->userBuffer = (char *) calloc(buffer_bytes, 1);
    if (stream->userBuffer == NULL)
      goto memory_error;
  }

  if ( stream->doConvertBuffer[mode] ) {

    long buffer_bytes;
    bool makeBuffer = true;
    if ( mode == PLAYBACK )
      buffer_bytes = stream->nDeviceChannels[0] * formatBytes(stream->deviceFormat[0]);
    else { // mode == RECORD
      buffer_bytes = stream->nDeviceChannels[1] * formatBytes(stream->deviceFormat[1]);
      if ( stream->mode == PLAYBACK ) {
        long bytes_out = stream->nDeviceChannels[0] * formatBytes(stream->deviceFormat[0]);
        if ( buffer_bytes > bytes_out )
          buffer_bytes = (buffer_bytes > bytes_out) ? buffer_bytes : bytes_out;
        else
          makeBuffer = false;
      }
    }

    if ( makeBuffer ) {
      buffer_bytes *= *bufferSize;
      if (stream->deviceBuffer) free(stream->deviceBuffer);
      stream->deviceBuffer = (char *) calloc(buffer_bytes, 1);
      if (stream->deviceBuffer == NULL)
        goto memory_error;
    }
  }

  stream->device[mode] = device;
  stream->state = STREAM_STOPPED;
  if ( stream->mode == PLAYBACK && mode == RECORD )
    // We had already set up an output stream.
    stream->mode = DUPLEX;
  else
    stream->mode = mode;
  stream->nBuffers = periods;
  stream->sampleRate = sampleRate;

  return SUCCESS;

 memory_error:
  if (stream->handle[0]) {
    snd_pcm_close(stream->handle[0]);
    stream->handle[0] = 0;
  }
  if (stream->handle[1]) {
    snd_pcm_close(stream->handle[1]);
    stream->handle[1] = 0;
  }
  if (stream->userBuffer) {
    free(stream->userBuffer);
    stream->userBuffer = 0;
  }
  sprintf(message, "RtAudio: ALSA error allocating buffer memory (%s).", name);
  error(RtError::WARNING);
  return FAILURE;
}

void RtAudio :: cancelStreamCallback(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  if (stream->usingCallback) {
    stream->usingCallback = false;
    pthread_cancel(stream->thread);
    pthread_join(stream->thread, NULL);
    stream->thread = 0;
    stream->callback = NULL;
    stream->userData = NULL;
  }
}

void RtAudio :: closeStream(int streamId)
{
  // We don't want an exception to be thrown here because this
  // function is called by our class destructor.  So, do our own
  // streamId check.
  if ( streams.find( streamId ) == streams.end() ) {
    sprintf(message, "RtAudio: invalid stream identifier!");
    error(RtError::WARNING);
    return;
  }

  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) streams[streamId];

  if (stream->usingCallback) {
    pthread_cancel(stream->thread);
    pthread_join(stream->thread, NULL);
  }

  if (stream->state == STREAM_RUNNING) {
    if (stream->mode == PLAYBACK || stream->mode == DUPLEX)
      snd_pcm_drop(stream->handle[0]);
    if (stream->mode == RECORD || stream->mode == DUPLEX)
      snd_pcm_drop(stream->handle[1]);
  }

  pthread_mutex_destroy(&stream->mutex);

  if (stream->handle[0])
    snd_pcm_close(stream->handle[0]);

  if (stream->handle[1])
    snd_pcm_close(stream->handle[1]);

  if (stream->userBuffer)
    free(stream->userBuffer);

  if (stream->deviceBuffer)
    free(stream->deviceBuffer);

  free(stream);
  streams.erase(streamId);
}

void RtAudio :: startStream(int streamId)
{
  // This method calls snd_pcm_prepare if the device isn't already in that state.

  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  if (stream->state == STREAM_RUNNING)
    goto unlock;

  int err;
  snd_pcm_state_t state;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {
    state = snd_pcm_state(stream->handle[0]);
    if (state != SND_PCM_STATE_PREPARED) {
      err = snd_pcm_prepare(stream->handle[0]);
      if (err < 0) {
        sprintf(message, "RtAudio: ALSA error preparing pcm device (%s): %s.",
                devices[stream->device[0]].name, snd_strerror(err));
        MUTEX_UNLOCK(&stream->mutex);
        error(RtError::DRIVER_ERROR);
      }
    }
  }

  if (stream->mode == RECORD || stream->mode == DUPLEX) {
    state = snd_pcm_state(stream->handle[1]);
    if (state != SND_PCM_STATE_PREPARED) {
      err = snd_pcm_prepare(stream->handle[1]);
      if (err < 0) {
        sprintf(message, "RtAudio: ALSA error preparing pcm device (%s): %s.",
                devices[stream->device[1]].name, snd_strerror(err));
        MUTEX_UNLOCK(&stream->mutex);
        error(RtError::DRIVER_ERROR);
      }
    }
  }
  stream->state = STREAM_RUNNING;

 unlock:
  MUTEX_UNLOCK(&stream->mutex);
}

void RtAudio :: stopStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  if (stream->state == STREAM_STOPPED)
    goto unlock;

  int err;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {
    err = snd_pcm_drain(stream->handle[0]);
    if (err < 0) {
      sprintf(message, "RtAudio: ALSA error draining pcm device (%s): %s.",
              devices[stream->device[0]].name, snd_strerror(err));
      MUTEX_UNLOCK(&stream->mutex);
      error(RtError::DRIVER_ERROR);
    }
  }

  if (stream->mode == RECORD || stream->mode == DUPLEX) {
    err = snd_pcm_drain(stream->handle[1]);
    if (err < 0) {
      sprintf(message, "RtAudio: ALSA error draining pcm device (%s): %s.",
              devices[stream->device[1]].name, snd_strerror(err));
      MUTEX_UNLOCK(&stream->mutex);
      error(RtError::DRIVER_ERROR);
    }
  }
  stream->state = STREAM_STOPPED;

 unlock:
  MUTEX_UNLOCK(&stream->mutex);
}

void RtAudio :: abortStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  if (stream->state == STREAM_STOPPED)
    goto unlock;

  int err;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {
    err = snd_pcm_drop(stream->handle[0]);
    if (err < 0) {
      sprintf(message, "RtAudio: ALSA error draining pcm device (%s): %s.",
              devices[stream->device[0]].name, snd_strerror(err));
      MUTEX_UNLOCK(&stream->mutex);
      error(RtError::DRIVER_ERROR);
    }
  }

  if (stream->mode == RECORD || stream->mode == DUPLEX) {
    err = snd_pcm_drop(stream->handle[1]);
    if (err < 0) {
      sprintf(message, "RtAudio: ALSA error draining pcm device (%s): %s.",
              devices[stream->device[1]].name, snd_strerror(err));
      MUTEX_UNLOCK(&stream->mutex);
      error(RtError::DRIVER_ERROR);
    }
  }
  stream->state = STREAM_STOPPED;

 unlock:
  MUTEX_UNLOCK(&stream->mutex);
}

int RtAudio :: streamWillBlock(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  int err = 0, frames = 0;
  if (stream->state == STREAM_STOPPED)
    goto unlock;

  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {
    err = snd_pcm_avail_update(stream->handle[0]);
    if (err < 0) {
      sprintf(message, "RtAudio: ALSA error getting available frames for device (%s): %s.",
              devices[stream->device[0]].name, snd_strerror(err));
      MUTEX_UNLOCK(&stream->mutex);
      error(RtError::DRIVER_ERROR);
    }
  }

  frames = err;

  if (stream->mode == RECORD || stream->mode == DUPLEX) {
    err = snd_pcm_avail_update(stream->handle[1]);
    if (err < 0) {
      sprintf(message, "RtAudio: ALSA error getting available frames for device (%s): %s.",
              devices[stream->device[1]].name, snd_strerror(err));
      MUTEX_UNLOCK(&stream->mutex);
      error(RtError::DRIVER_ERROR);
    }
    if (frames > err) frames = err;
  }

  frames = stream->bufferSize - frames;
  if (frames < 0) frames = 0;

 unlock:
  MUTEX_UNLOCK(&stream->mutex);
  return frames;
}

void RtAudio :: tickStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  int stopStream = 0;
  if (stream->state == STREAM_STOPPED) {
    if (stream->usingCallback) usleep(50000); // sleep 50 milliseconds
    return;
  }
  else if (stream->usingCallback) {
    stopStream = stream->callback(stream->userBuffer, stream->bufferSize, stream->userData);
  }

  MUTEX_LOCK(&stream->mutex);

  // The state might change while waiting on a mutex.
  if (stream->state == STREAM_STOPPED)
    goto unlock;

  int err;
  char *buffer;
  int channels;
  RTAUDIO_FORMAT format;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {

    // Setup parameters and do buffer conversion if necessary.
    if (stream->doConvertBuffer[0]) {
      convertStreamBuffer(stream, PLAYBACK);
      buffer = stream->deviceBuffer;
      channels = stream->nDeviceChannels[0];
      format = stream->deviceFormat[0];
    }
    else {
      buffer = stream->userBuffer;
      channels = stream->nUserChannels[0];
      format = stream->userFormat;
    }

    // Do byte swapping if necessary.
    if (stream->doByteSwap[0])
      byteSwapBuffer(buffer, stream->bufferSize * channels, format);

    // Write samples to device in interleaved/non-interleaved format.
    if (stream->deInterleave[0]) {
      void *bufs[channels];
      size_t offset = stream->bufferSize * formatBytes(format);
      for (int i=0; i<channels; i++)
        bufs[i] = (void *) (buffer + (i * offset));
      err = snd_pcm_writen(stream->handle[0], bufs, stream->bufferSize);
    }
    else
      err = snd_pcm_writei(stream->handle[0], buffer, stream->bufferSize);

    if (err < stream->bufferSize) {
      // Either an error or underrun occured.
      if (err == -EPIPE) {
        snd_pcm_state_t state = snd_pcm_state(stream->handle[0]);
        if (state == SND_PCM_STATE_XRUN) {
          sprintf(message, "RtAudio: ALSA underrun detected.");
          error(RtError::WARNING);
          err = snd_pcm_prepare(stream->handle[0]);
          if (err < 0) {
            sprintf(message, "RtAudio: ALSA error preparing handle after underrun: %s.",
                    snd_strerror(err));
            MUTEX_UNLOCK(&stream->mutex);
            error(RtError::DRIVER_ERROR);
          }
        }
        else {
          sprintf(message, "RtAudio: ALSA error, current state is %s.",
                  snd_pcm_state_name(state));
          MUTEX_UNLOCK(&stream->mutex);
          error(RtError::DRIVER_ERROR);
        }
        goto unlock;
      }
      else {
        sprintf(message, "RtAudio: ALSA audio write error for device (%s): %s.",
                devices[stream->device[0]].name, snd_strerror(err));
        MUTEX_UNLOCK(&stream->mutex);
        error(RtError::DRIVER_ERROR);
      }
    }
  }

  if (stream->mode == RECORD || stream->mode == DUPLEX) {

    // Setup parameters.
    if (stream->doConvertBuffer[1]) {
      buffer = stream->deviceBuffer;
      channels = stream->nDeviceChannels[1];
      format = stream->deviceFormat[1];
    }
    else {
      buffer = stream->userBuffer;
      channels = stream->nUserChannels[1];
      format = stream->userFormat;
    }

    // Read samples from device in interleaved/non-interleaved format.
    if (stream->deInterleave[1]) {
      void *bufs[channels];
      size_t offset = stream->bufferSize * formatBytes(format);
      for (int i=0; i<channels; i++)
        bufs[i] = (void *) (buffer + (i * offset));
      err = snd_pcm_readn(stream->handle[1], bufs, stream->bufferSize);
    }
    else
      err = snd_pcm_readi(stream->handle[1], buffer, stream->bufferSize);

    if (err < stream->bufferSize) {
      // Either an error or underrun occured.
      if (err == -EPIPE) {
        snd_pcm_state_t state = snd_pcm_state(stream->handle[1]);
        if (state == SND_PCM_STATE_XRUN) {
          sprintf(message, "RtAudio: ALSA overrun detected.");
          error(RtError::WARNING);
          err = snd_pcm_prepare(stream->handle[1]);
          if (err < 0) {
            sprintf(message, "RtAudio: ALSA error preparing handle after overrun: %s.",
                    snd_strerror(err));
            MUTEX_UNLOCK(&stream->mutex);
            error(RtError::DRIVER_ERROR);
          }
        }
        else {
          sprintf(message, "RtAudio: ALSA error, current state is %s.",
                  snd_pcm_state_name(state));
          MUTEX_UNLOCK(&stream->mutex);
          error(RtError::DRIVER_ERROR);
        }
        goto unlock;
      }
      else {
        sprintf(message, "RtAudio: ALSA audio read error for device (%s): %s.",
                devices[stream->device[1]].name, snd_strerror(err));
        MUTEX_UNLOCK(&stream->mutex);
        error(RtError::DRIVER_ERROR);
      }
    }

    // Do byte swapping if necessary.
    if (stream->doByteSwap[1])
      byteSwapBuffer(buffer, stream->bufferSize * channels, format);

    // Do buffer conversion if necessary.
    if (stream->doConvertBuffer[1])
      convertStreamBuffer(stream, RECORD);
  }

 unlock:
  MUTEX_UNLOCK(&stream->mutex);

  if (stream->usingCallback && stopStream)
    this->stopStream(streamId);
}

extern "C" void *callbackHandler(void *ptr)
{
  RtAudio *object = thread_info.object;
  int stream = thread_info.streamId;
  bool *usingCallback = (bool *) ptr;

  while ( *usingCallback ) {
    pthread_testcancel();
    try {
      object->tickStream(stream);
    }
    catch (RtError &exception) {
      fprintf(stderr, "\nCallback thread error (%s) ... closing thread.\n\n",
              exception.getMessage());
      break;
    }
  }

  return 0;
}

//******************** End of __LINUX_ALSA__ *********************//

#elif defined(__LINUX_OSS__)

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/soundcard.h>
#include <errno.h>
#include <math.h>

#define DAC_NAME "/dev/dsp"
#define MAX_DEVICES 16
#define MAX_CHANNELS 16

void RtAudio :: initialize(void)
{
  // Count cards and devices
  nDevices = 0;

  // We check /dev/dsp before probing devices.  /dev/dsp is supposed to
  // be a link to the "default" audio device, of the form /dev/dsp0,
  // /dev/dsp1, etc...  However, I've seen one case where /dev/dsp was a
  // real device, so we need to check for that.  Also, sometimes the
  // link is to /dev/dspx and other times just dspx.  I'm not sure how
  // the latter works, but it does.
  char device_name[16];
  struct stat dspstat;
  int dsplink = -1;
  int i = 0;
  if (lstat(DAC_NAME, &dspstat) == 0) {
    if (S_ISLNK(dspstat.st_mode)) {
      i = readlink(DAC_NAME, device_name, sizeof(device_name));
      if (i > 0) {
        device_name[i] = '\0';
        if (i > 8) { // check for "/dev/dspx"
          if (!strncmp(DAC_NAME, device_name, 8))
            dsplink = atoi(&device_name[8]);
        }
        else if (i > 3) { // check for "dspx"
          if (!strncmp("dsp", device_name, 3))
            dsplink = atoi(&device_name[3]);
        }
      }
      else {
        sprintf(message, "RtAudio: cannot read value of symbolic link %s.", DAC_NAME);
        error(RtError::SYSTEM_ERROR);
      }
    }
  }
  else {
    sprintf(message, "RtAudio: cannot stat %s.", DAC_NAME);
    error(RtError::SYSTEM_ERROR);
  }

  // The OSS API doesn't provide a routine for determining the number
  // of devices.  Thus, we'll just pursue a brute force method.  The
  // idea is to start with /dev/dsp(0) and continue with higher device
  // numbers until we reach MAX_DSP_DEVICES.  This should tell us how
  // many devices we have ... it is not a fullproof scheme, but hopefully
  // it will work most of the time.

  int fd = 0;
  char names[MAX_DEVICES][16];
  for (i=-1; i<MAX_DEVICES; i++) {

    // Probe /dev/dsp first, since it is supposed to be the default device.
    if (i == -1)
      sprintf(device_name, "%s", DAC_NAME);
    else if (i == dsplink)
      continue; // We've aready probed this device via /dev/dsp link ... try next device.
    else
      sprintf(device_name, "%s%d", DAC_NAME, i);

    // First try to open the device for playback, then record mode.
    fd = open(device_name, O_WRONLY | O_NONBLOCK);
    if (fd == -1) {
      // Open device for playback failed ... either busy or doesn't exist.
      if (errno != EBUSY && errno != EAGAIN) {
        // Try to open for capture
        fd = open(device_name, O_RDONLY | O_NONBLOCK);
        if (fd == -1) {
          // Open device for record failed.
          if (errno != EBUSY && errno != EAGAIN)
            continue;
          else {
            sprintf(message, "RtAudio: OSS record device (%s) is busy.", device_name);
            error(RtError::WARNING);
            // still count it for now
          }
        }
      }
      else {
        sprintf(message, "RtAudio: OSS playback device (%s) is busy.", device_name);
        error(RtError::WARNING);
        // still count it for now
      }
    }

    if (fd >= 0) close(fd);
    strncpy(names[nDevices], device_name, 16);
    nDevices++;
  }

  if (nDevices == 0) return;

  //  Allocate the RTAUDIO_DEVICE structures.
  devices = (RTAUDIO_DEVICE *) calloc(nDevices, sizeof(RTAUDIO_DEVICE));
  if (devices == NULL) {
    sprintf(message, "RtAudio: memory allocation error!");
    error(RtError::MEMORY_ERROR);
  }

  // Write device ascii identifiers to device control structure and then probe capabilities.
  for (i=0; i<nDevices; i++) {
    strncpy(devices[i].name, names[i], 16);
    probeDeviceInfo(&devices[i]);
  }

  return;
}

void RtAudio :: probeDeviceInfo(RTAUDIO_DEVICE *info)
{
  int i, fd, channels, mask;

  // The OSS API doesn't provide a means for probing the capabilities
  // of devices.  Thus, we'll just pursue a brute force method.

  // First try for playback
  fd = open(info->name, O_WRONLY | O_NONBLOCK);
  if (fd == -1) {
    // Open device failed ... either busy or doesn't exist
    if (errno == EBUSY || errno == EAGAIN)
      sprintf(message, "RtAudio: OSS playback device (%s) is busy and cannot be probed.",
              info->name);
    else
      sprintf(message, "RtAudio: OSS playback device (%s) open error.", info->name);
    error(RtError::WARNING);
    goto capture_probe;
  }

  // We have an open device ... see how many channels it can handle
  for (i=MAX_CHANNELS; i>0; i--) {
    channels = i;
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) == -1) {
      // This would normally indicate some sort of hardware error, but under ALSA's
      // OSS emulation, it sometimes indicates an invalid channel value.  Further,
      // the returned channel value is not changed. So, we'll ignore the possible
      // hardware error.
      continue; // try next channel number
    }
    // Check to see whether the device supports the requested number of channels
    if (channels != i ) continue; // try next channel number
    // If here, we found the largest working channel value
    break;
  }
  info->maxOutputChannels = channels;

  // Now find the minimum number of channels it can handle
  for (i=1; i<=info->maxOutputChannels; i++) {
    channels = i;
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) == -1 || channels != i)
      continue; // try next channel number
    // If here, we found the smallest working channel value
    break;
  }
  info->minOutputChannels = channels;
  close(fd);

 capture_probe:
  // Now try for capture
  fd = open(info->name, O_RDONLY | O_NONBLOCK);
  if (fd == -1) {
    // Open device for capture failed ... either busy or doesn't exist
    if (errno == EBUSY || errno == EAGAIN)
      sprintf(message, "RtAudio: OSS capture device (%s) is busy and cannot be probed.",
              info->name);
    else
      sprintf(message, "RtAudio: OSS capture device (%s) open error.", info->name);
    error(RtError::WARNING);
    if (info->maxOutputChannels == 0)
      // didn't open for playback either ... device invalid
      return;
    goto probe_parameters;
  }

  // We have the device open for capture ... see how many channels it can handle
  for (i=MAX_CHANNELS; i>0; i--) {
    channels = i;
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) == -1 || channels != i) {
      continue; // as above
    }
    // If here, we found a working channel value
    break;
  }
  info->maxInputChannels = channels;

  // Now find the minimum number of channels it can handle
  for (i=1; i<=info->maxInputChannels; i++) {
    channels = i;
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) == -1 || channels != i)
      continue; // try next channel number
    // If here, we found the smallest working channel value
    break;
  }
  info->minInputChannels = channels;
  close(fd);

  // If device opens for both playback and capture, we determine the channels.
  if (info->maxOutputChannels == 0 || info->maxInputChannels == 0)
    goto probe_parameters;

  fd = open(info->name, O_RDWR | O_NONBLOCK);
  if (fd == -1)
    goto probe_parameters;

  ioctl(fd, SNDCTL_DSP_SETDUPLEX, 0);
  ioctl(fd, SNDCTL_DSP_GETCAPS, &mask);
  if (mask & DSP_CAP_DUPLEX) {
    info->hasDuplexSupport = true;
    // We have the device open for duplex ... see how many channels it can handle
    for (i=MAX_CHANNELS; i>0; i--) {
      channels = i;
      if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) == -1 || channels != i)
        continue; // as above
      // If here, we found a working channel value
      break;
    }
    info->maxDuplexChannels = channels;

    // Now find the minimum number of channels it can handle
    for (i=1; i<=info->maxDuplexChannels; i++) {
      channels = i;
      if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) == -1 || channels != i)
        continue; // try next channel number
      // If here, we found the smallest working channel value
      break;
    }
    info->minDuplexChannels = channels;
  }
  close(fd);

 probe_parameters:
  // At this point, we need to figure out the supported data formats
  // and sample rates.  We'll proceed by openning the device in the
  // direction with the maximum number of channels, or playback if
  // they are equal.  This might limit our sample rate options, but so
  // be it.

  if (info->maxOutputChannels >= info->maxInputChannels) {
    fd = open(info->name, O_WRONLY | O_NONBLOCK);
    channels = info->maxOutputChannels;
  }
  else {
    fd = open(info->name, O_RDONLY | O_NONBLOCK);
    channels = info->maxInputChannels;
  }

  if (fd == -1) {
    // We've got some sort of conflict ... abort
    sprintf(message, "RtAudio: OSS device (%s) won't reopen during probe.",
            info->name);
    error(RtError::WARNING);
    return;
  }

  // We have an open device ... set to maximum channels.
  i = channels;
  if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) == -1 || channels != i) {
    // We've got some sort of conflict ... abort
    close(fd);
    sprintf(message, "RtAudio: OSS device (%s) won't revert to previous channel setting.",
            info->name);
    error(RtError::WARNING);
    return;
  }

  if (ioctl(fd, SNDCTL_DSP_GETFMTS, &mask) == -1) {
    close(fd);
    sprintf(message, "RtAudio: OSS device (%s) can't get supported audio formats.",
            info->name);
    error(RtError::WARNING);
    return;
  }

  // Probe the supported data formats ... we don't care about endian-ness just yet.
  int format;
  info->nativeFormats = 0;
#if defined (AFMT_S32_BE)
  // This format does not seem to be in the 2.4 kernel version of OSS soundcard.h
  if (mask & AFMT_S32_BE) {
    format = AFMT_S32_BE;
    info->nativeFormats |= RTAUDIO_SINT32;
  }
#endif
#if defined (AFMT_S32_LE)
  /* This format is not in the 2.4.4 kernel version of OSS soundcard.h */
  if (mask & AFMT_S32_LE) {
    format = AFMT_S32_LE;
    info->nativeFormats |= RTAUDIO_SINT32;
  }
#endif
  if (mask & AFMT_S8) {
    format = AFMT_S8;
    info->nativeFormats |= RTAUDIO_SINT8;
  }
  if (mask & AFMT_S16_BE) {
    format = AFMT_S16_BE;
    info->nativeFormats |= RTAUDIO_SINT16;
  }
  if (mask & AFMT_S16_LE) {
    format = AFMT_S16_LE;
    info->nativeFormats |= RTAUDIO_SINT16;
  }

  // Check that we have at least one supported format
  if (info->nativeFormats == 0) {
    close(fd);
    sprintf(message, "RtAudio: OSS device (%s) data format not supported by RtAudio.",
            info->name);
    error(RtError::WARNING);
    return;
  }

  // Set the format
  i = format;
  if (ioctl(fd, SNDCTL_DSP_SETFMT, &format) == -1 || format != i) {
    close(fd);
    sprintf(message, "RtAudio: OSS device (%s) error setting data format.",
            info->name);
    error(RtError::WARNING);
    return;
  }

  // Probe the supported sample rates ... first get lower limit
  int speed = 1;
  if (ioctl(fd, SNDCTL_DSP_SPEED, &speed) == -1) {
    // If we get here, we're probably using an ALSA driver with OSS-emulation,
    // which doesn't conform to the OSS specification.  In this case,
    // we'll probe our predefined list of sample rates for working values.
    info->nSampleRates = 0;
    for (i=0; i<MAX_SAMPLE_RATES; i++) {
      speed = SAMPLE_RATES[i];
      if (ioctl(fd, SNDCTL_DSP_SPEED, &speed) != -1) {
        info->sampleRates[info->nSampleRates] = SAMPLE_RATES[i];
        info->nSampleRates++;
      }
    }
    if (info->nSampleRates == 0) {
      close(fd);
      return;
    }
    goto finished;
  }
  info->sampleRates[0] = speed;

  // Now get upper limit
  speed = 1000000;
  if (ioctl(fd, SNDCTL_DSP_SPEED, &speed) == -1) {
    close(fd);
    sprintf(message, "RtAudio: OSS device (%s) error setting sample rate.",
            info->name);
    error(RtError::WARNING);
    return;
  }
  info->sampleRates[1] = speed;
  info->nSampleRates = -1;

 finished: // That's all ... close the device and return
  close(fd);
  info->probed = true;
  return;
}

bool RtAudio :: probeDeviceOpen(int device, RTAUDIO_STREAM *stream,
                                STREAM_MODE mode, int channels, 
                                int sampleRate, RTAUDIO_FORMAT format,
                                int *bufferSize, int numberOfBuffers)
{
  int buffers, buffer_bytes, device_channels, device_format;
  int srate, temp, fd;

  const char *name = devices[device].name;

  if (mode == PLAYBACK)
    fd = open(name, O_WRONLY | O_NONBLOCK);
  else { // mode == RECORD
    if (stream->mode == PLAYBACK && stream->device[0] == device) {
      // We just set the same device for playback ... close and reopen for duplex (OSS only).
      close(stream->handle[0]);
      stream->handle[0] = 0;
      // First check that the number previously set channels is the same.
      if (stream->nUserChannels[0] != channels) {
        sprintf(message, "RtAudio: input/output channels must be equal for OSS duplex device (%s).", name);
        goto error;
      }
      fd = open(name, O_RDWR | O_NONBLOCK);
    }
    else
      fd = open(name, O_RDONLY | O_NONBLOCK);
  }

  if (fd == -1) {
    if (errno == EBUSY || errno == EAGAIN)
      sprintf(message, "RtAudio: OSS device (%s) is busy and cannot be opened.",
              name);
    else
      sprintf(message, "RtAudio: OSS device (%s) cannot be opened.", name);
    goto error;
  }

  // Now reopen in blocking mode.
  close(fd);
  if (mode == PLAYBACK)
    fd = open(name, O_WRONLY | O_SYNC);
  else { // mode == RECORD
    if (stream->mode == PLAYBACK && stream->device[0] == device)
      fd = open(name, O_RDWR | O_SYNC);
    else
      fd = open(name, O_RDONLY | O_SYNC);
  }

  if (fd == -1) {
    sprintf(message, "RtAudio: OSS device (%s) cannot be opened.", name);
    goto error;
  }

  // Get the sample format mask
  int mask;
  if (ioctl(fd, SNDCTL_DSP_GETFMTS, &mask) == -1) {
    close(fd);
    sprintf(message, "RtAudio: OSS device (%s) can't get supported audio formats.",
            name);
    goto error;
  }

  // Determine how to set the device format.
  stream->userFormat = format;
  device_format = -1;
  stream->doByteSwap[mode] = false;
  if (format == RTAUDIO_SINT8) {
    if (mask & AFMT_S8) {
      device_format = AFMT_S8;
      stream->deviceFormat[mode] = RTAUDIO_SINT8;
    }
  }
  else if (format == RTAUDIO_SINT16) {
    if (mask & AFMT_S16_NE) {
      device_format = AFMT_S16_NE;
      stream->deviceFormat[mode] = RTAUDIO_SINT16;
    }
#if BYTE_ORDER == LITTLE_ENDIAN
    else if (mask & AFMT_S16_BE) {
      device_format = AFMT_S16_BE;
      stream->deviceFormat[mode] = RTAUDIO_SINT16;
      stream->doByteSwap[mode] = true;
    }
#else
    else if (mask & AFMT_S16_LE) {
      device_format = AFMT_S16_LE;
      stream->deviceFormat[mode] = RTAUDIO_SINT16;
      stream->doByteSwap[mode] = true;
    }
#endif
  }
#if defined (AFMT_S32_NE) && defined (AFMT_S32_LE) && defined (AFMT_S32_BE)
  else if (format == RTAUDIO_SINT32) {
    if (mask & AFMT_S32_NE) {
      device_format = AFMT_S32_NE;
      stream->deviceFormat[mode] = RTAUDIO_SINT32;
    }
#if BYTE_ORDER == LITTLE_ENDIAN
    else if (mask & AFMT_S32_BE) {
      device_format = AFMT_S32_BE;
      stream->deviceFormat[mode] = RTAUDIO_SINT32;
      stream->doByteSwap[mode] = true;
    }
#else
    else if (mask & AFMT_S32_LE) {
      device_format = AFMT_S32_LE;
      stream->deviceFormat[mode] = RTAUDIO_SINT32;
      stream->doByteSwap[mode] = true;
    }
#endif
  }
#endif

  if (device_format == -1) {
    // The user requested format is not natively supported by the device.
    if (mask & AFMT_S16_NE) {
      device_format = AFMT_S16_NE;
      stream->deviceFormat[mode] = RTAUDIO_SINT16;
    }
#if BYTE_ORDER == LITTLE_ENDIAN
    else if (mask & AFMT_S16_BE) {
      device_format = AFMT_S16_BE;
      stream->deviceFormat[mode] = RTAUDIO_SINT16;
      stream->doByteSwap[mode] = true;
    }
#else
    else if (mask & AFMT_S16_LE) {
      device_format = AFMT_S16_LE;
      stream->deviceFormat[mode] = RTAUDIO_SINT16;
      stream->doByteSwap[mode] = true;
    }
#endif
#if defined (AFMT_S32_NE) && defined (AFMT_S32_LE) && defined (AFMT_S32_BE)
    else if (mask & AFMT_S32_NE) {
      device_format = AFMT_S32_NE;
      stream->deviceFormat[mode] = RTAUDIO_SINT32;
    }
#if BYTE_ORDER == LITTLE_ENDIAN
    else if (mask & AFMT_S32_BE) {
      device_format = AFMT_S32_BE;
      stream->deviceFormat[mode] = RTAUDIO_SINT32;
      stream->doByteSwap[mode] = true;
    }
#else
    else if (mask & AFMT_S32_LE) {
      device_format = AFMT_S32_LE;
      stream->deviceFormat[mode] = RTAUDIO_SINT32;
      stream->doByteSwap[mode] = true;
    }
#endif
#endif
    else if (mask & AFMT_S8) {
      device_format = AFMT_S8;
      stream->deviceFormat[mode] = RTAUDIO_SINT8;
    }
  }

  if (stream->deviceFormat[mode] == 0) {
    // This really shouldn't happen ...
    close(fd);
    sprintf(message, "RtAudio: OSS device (%s) data format not supported by RtAudio.",
            name);
    goto error;
  }

  // Determine the number of channels for this device.  Note that the
  // channel value requested by the user might be < min_X_Channels.
  stream->nUserChannels[mode] = channels;
  device_channels = channels;
  if (mode == PLAYBACK) {
    if (channels < devices[device].minOutputChannels)
      device_channels = devices[device].minOutputChannels;
  }
  else { // mode == RECORD
    if (stream->mode == PLAYBACK && stream->device[0] == device) {
      // We're doing duplex setup here.
      if (channels < devices[device].minDuplexChannels)
        device_channels = devices[device].minDuplexChannels;
    }
    else {
      if (channels < devices[device].minInputChannels)
        device_channels = devices[device].minInputChannels;
    }
  }
  stream->nDeviceChannels[mode] = device_channels;

  // Attempt to set the buffer size.  According to OSS, the minimum
  // number of buffers is two.  The supposed minimum buffer size is 16
  // bytes, so that will be our lower bound.  The argument to this
  // call is in the form 0xMMMMSSSS (hex), where the buffer size (in
  // bytes) is given as 2^SSSS and the number of buffers as 2^MMMM.
  // We'll check the actual value used near the end of the setup
  // procedure.
  buffer_bytes = *bufferSize * formatBytes(stream->deviceFormat[mode]) * device_channels;
  if (buffer_bytes < 16) buffer_bytes = 16;
  buffers = numberOfBuffers;
  if (buffers < 2) buffers = 2;
  temp = ((int) buffers << 16) + (int)(log10((double)buffer_bytes)/log10(2.0));
  if (ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &temp)) {
    close(fd);
    sprintf(message, "RtAudio: OSS error setting fragment size for device (%s).",
            name);
    goto error;
  }
  stream->nBuffers = buffers;

  // Set the data format.
  temp = device_format;
  if (ioctl(fd, SNDCTL_DSP_SETFMT, &device_format) == -1 || device_format != temp) {
    close(fd);
    sprintf(message, "RtAudio: OSS error setting data format for device (%s).",
            name);
    goto error;
  }

  // Set the number of channels.
  temp = device_channels;
  if (ioctl(fd, SNDCTL_DSP_CHANNELS, &device_channels) == -1 || device_channels != temp) {
    close(fd);
    sprintf(message, "RtAudio: OSS error setting %d channels on device (%s).",
            temp, name);
    goto error;
  }

  // Set the sample rate.
  srate = sampleRate;
  temp = srate;
  if (ioctl(fd, SNDCTL_DSP_SPEED, &srate) == -1) {
    close(fd);
    sprintf(message, "RtAudio: OSS error setting sample rate = %d on device (%s).",
            temp, name);
    goto error;
  }

  // Verify the sample rate setup worked.
  if (abs(srate - temp) > 100) {
    close(fd);
    sprintf(message, "RtAudio: OSS error ... audio device (%s) doesn't support sample rate of %d.",
            name, temp);
    goto error;
  }
  stream->sampleRate = sampleRate;

  if (ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &buffer_bytes) == -1) {
    close(fd);
    sprintf(message, "RtAudio: OSS error getting buffer size for device (%s).",
            name);
    goto error;
  }

  // Save buffer size (in sample frames).
  *bufferSize = buffer_bytes / (formatBytes(stream->deviceFormat[mode]) * device_channels);
  stream->bufferSize = *bufferSize;

  if (mode == RECORD && stream->mode == PLAYBACK &&
      stream->device[0] == device) {
    // We're doing duplex setup here.
    stream->deviceFormat[0] = stream->deviceFormat[1];
    stream->nDeviceChannels[0] = device_channels;
  }

  // Set flags for buffer conversion
  stream->doConvertBuffer[mode] = false;
  if (stream->userFormat != stream->deviceFormat[mode])
    stream->doConvertBuffer[mode] = true;
  if (stream->nUserChannels[mode] < stream->nDeviceChannels[mode])
    stream->doConvertBuffer[mode] = true;

  // Allocate necessary internal buffers
  if ( stream->nUserChannels[0] != stream->nUserChannels[1] ) {

    long buffer_bytes;
    if (stream->nUserChannels[0] >= stream->nUserChannels[1])
      buffer_bytes = stream->nUserChannels[0];
    else
      buffer_bytes = stream->nUserChannels[1];

    buffer_bytes *= *bufferSize * formatBytes(stream->userFormat);
    if (stream->userBuffer) free(stream->userBuffer);
    stream->userBuffer = (char *) calloc(buffer_bytes, 1);
    if (stream->userBuffer == NULL) {
      close(fd);
      sprintf(message, "RtAudio: OSS error allocating user buffer memory (%s).",
              name);
      goto error;
    }
  }

  if ( stream->doConvertBuffer[mode] ) {

    long buffer_bytes;
    bool makeBuffer = true;
    if ( mode == PLAYBACK )
      buffer_bytes = stream->nDeviceChannels[0] * formatBytes(stream->deviceFormat[0]);
    else { // mode == RECORD
      buffer_bytes = stream->nDeviceChannels[1] * formatBytes(stream->deviceFormat[1]);
      if ( stream->mode == PLAYBACK ) {
        long bytes_out = stream->nDeviceChannels[0] * formatBytes(stream->deviceFormat[0]);
        if ( buffer_bytes > bytes_out )
          buffer_bytes = (buffer_bytes > bytes_out) ? buffer_bytes : bytes_out;
        else
          makeBuffer = false;
      }
    }

    if ( makeBuffer ) {
      buffer_bytes *= *bufferSize;
      if (stream->deviceBuffer) free(stream->deviceBuffer);
      stream->deviceBuffer = (char *) calloc(buffer_bytes, 1);
      if (stream->deviceBuffer == NULL) {
        close(fd);
        free(stream->userBuffer);
        sprintf(message, "RtAudio: OSS error allocating device buffer memory (%s).",
                name);
        goto error;
      }
    }
  }

  stream->device[mode] = device;
  stream->handle[mode] = fd;
  stream->state = STREAM_STOPPED;
  if ( stream->mode == PLAYBACK && mode == RECORD ) {
    stream->mode = DUPLEX;
    if (stream->device[0] == device)
      stream->handle[0] = fd;
  }
  else
    stream->mode = mode;

  return SUCCESS;

 error:
  if (stream->handle[0]) {
    close(stream->handle[0]);
    stream->handle[0] = 0;
  }
  error(RtError::WARNING);
  return FAILURE;
}

void RtAudio :: cancelStreamCallback(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  if (stream->usingCallback) {
    stream->usingCallback = false;
    pthread_cancel(stream->thread);
    pthread_join(stream->thread, NULL);
    stream->thread = 0;
    stream->callback = NULL;
    stream->userData = NULL;
  }
}

void RtAudio :: closeStream(int streamId)
{
  // We don't want an exception to be thrown here because this
  // function is called by our class destructor.  So, do our own
  // streamId check.
  if ( streams.find( streamId ) == streams.end() ) {
    sprintf(message, "RtAudio: invalid stream identifier!");
    error(RtError::WARNING);
    return;
  }

  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) streams[streamId];

  if (stream->usingCallback) {
    pthread_cancel(stream->thread);
    pthread_join(stream->thread, NULL);
  }

  if (stream->state == STREAM_RUNNING) {
    if (stream->mode == PLAYBACK || stream->mode == DUPLEX)
      ioctl(stream->handle[0], SNDCTL_DSP_RESET, 0);
    if (stream->mode == RECORD || stream->mode == DUPLEX)
      ioctl(stream->handle[1], SNDCTL_DSP_RESET, 0);
  }

  pthread_mutex_destroy(&stream->mutex);

  if (stream->handle[0])
    close(stream->handle[0]);

  if (stream->handle[1])
    close(stream->handle[1]);

  if (stream->userBuffer)
    free(stream->userBuffer);

  if (stream->deviceBuffer)
    free(stream->deviceBuffer);

  free(stream);
  streams.erase(streamId);
}

void RtAudio :: startStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  stream->state = STREAM_RUNNING;

  // No need to do anything else here ... OSS automatically starts when fed samples.
}

void RtAudio :: stopStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  if (stream->state == STREAM_STOPPED)
    goto unlock;

  int err;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {
    err = ioctl(stream->handle[0], SNDCTL_DSP_SYNC, 0);
    if (err < -1) {
      sprintf(message, "RtAudio: OSS error stopping device (%s).",
              devices[stream->device[0]].name);
      error(RtError::DRIVER_ERROR);
    }
  }
  else {
    err = ioctl(stream->handle[1], SNDCTL_DSP_SYNC, 0);
    if (err < -1) {
      sprintf(message, "RtAudio: OSS error stopping device (%s).",
              devices[stream->device[1]].name);
      error(RtError::DRIVER_ERROR);
    }
  }
  stream->state = STREAM_STOPPED;

 unlock:
  MUTEX_UNLOCK(&stream->mutex);
}

void RtAudio :: abortStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  if (stream->state == STREAM_STOPPED)
    goto unlock;

  int err;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {
    err = ioctl(stream->handle[0], SNDCTL_DSP_RESET, 0);
    if (err < -1) {
      sprintf(message, "RtAudio: OSS error aborting device (%s).",
              devices[stream->device[0]].name);
      error(RtError::DRIVER_ERROR);
    }
  }
  else {
    err = ioctl(stream->handle[1], SNDCTL_DSP_RESET, 0);
    if (err < -1) {
      sprintf(message, "RtAudio: OSS error aborting device (%s).",
              devices[stream->device[1]].name);
      error(RtError::DRIVER_ERROR);
    }
  }
  stream->state = STREAM_STOPPED;

 unlock:
  MUTEX_UNLOCK(&stream->mutex);
}

int RtAudio :: streamWillBlock(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  int bytes = 0, channels = 0, frames = 0;
  if (stream->state == STREAM_STOPPED)
    goto unlock;

  audio_buf_info info;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {
    ioctl(stream->handle[0], SNDCTL_DSP_GETOSPACE, &info);
    bytes = info.bytes;
    channels = stream->nDeviceChannels[0];
  }

  if (stream->mode == RECORD || stream->mode == DUPLEX) {
    ioctl(stream->handle[1], SNDCTL_DSP_GETISPACE, &info);
    if (stream->mode == DUPLEX ) {
      bytes = (bytes < info.bytes) ? bytes : info.bytes;
      channels = stream->nDeviceChannels[0];
    }
    else {
      bytes = info.bytes;
      channels = stream->nDeviceChannels[1];
    }
  }

  frames = (int) (bytes / (channels * formatBytes(stream->deviceFormat[0])));
  frames -= stream->bufferSize;
  if (frames < 0) frames = 0;

 unlock:
  MUTEX_UNLOCK(&stream->mutex);
  return frames;
}

void RtAudio :: tickStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  int stopStream = 0;
  if (stream->state == STREAM_STOPPED) {
    if (stream->usingCallback) usleep(50000); // sleep 50 milliseconds
    return;
  }
  else if (stream->usingCallback) {
    stopStream = stream->callback(stream->userBuffer, stream->bufferSize, stream->userData);
  }

  MUTEX_LOCK(&stream->mutex);

  // The state might change while waiting on a mutex.
  if (stream->state == STREAM_STOPPED)
    goto unlock;

  int result;
  char *buffer;
  int samples;
  RTAUDIO_FORMAT format;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {

    // Setup parameters and do buffer conversion if necessary.
    if (stream->doConvertBuffer[0]) {
      convertStreamBuffer(stream, PLAYBACK);
      buffer = stream->deviceBuffer;
      samples = stream->bufferSize * stream->nDeviceChannels[0];
      format = stream->deviceFormat[0];
    }
    else {
      buffer = stream->userBuffer;
      samples = stream->bufferSize * stream->nUserChannels[0];
      format = stream->userFormat;
    }

    // Do byte swapping if necessary.
    if (stream->doByteSwap[0])
      byteSwapBuffer(buffer, samples, format);

    // Write samples to device.
    result = write(stream->handle[0], buffer, samples * formatBytes(format));

    if (result == -1) {
      // This could be an underrun, but the basic OSS API doesn't provide a means for determining that.
      sprintf(message, "RtAudio: OSS audio write error for device (%s).",
              devices[stream->device[0]].name);
      error(RtError::DRIVER_ERROR);
    }
  }

  if (stream->mode == RECORD || stream->mode == DUPLEX) {

    // Setup parameters.
    if (stream->doConvertBuffer[1]) {
      buffer = stream->deviceBuffer;
      samples = stream->bufferSize * stream->nDeviceChannels[1];
      format = stream->deviceFormat[1];
    }
    else {
      buffer = stream->userBuffer;
      samples = stream->bufferSize * stream->nUserChannels[1];
      format = stream->userFormat;
    }

    // Read samples from device.
    result = read(stream->handle[1], buffer, samples * formatBytes(format));

    if (result == -1) {
      // This could be an overrun, but the basic OSS API doesn't provide a means for determining that.
      sprintf(message, "RtAudio: OSS audio read error for device (%s).",
              devices[stream->device[1]].name);
      error(RtError::DRIVER_ERROR);
    }

    // Do byte swapping if necessary.
    if (stream->doByteSwap[1])
      byteSwapBuffer(buffer, samples, format);

    // Do buffer conversion if necessary.
    if (stream->doConvertBuffer[1])
      convertStreamBuffer(stream, RECORD);
  }

 unlock:
  MUTEX_UNLOCK(&stream->mutex);

  if (stream->usingCallback && stopStream)
    this->stopStream(streamId);
}

extern "C" void *callbackHandler(void *ptr)
{
  RtAudio *object = thread_info.object;
  int stream = thread_info.streamId;
  bool *usingCallback = (bool *) ptr;

  while ( *usingCallback ) {
    pthread_testcancel();
    try {
      object->tickStream(stream);
    }
    catch (RtError &exception) {
      fprintf(stderr, "\nCallback thread error (%s) ... closing thread.\n\n",
              exception.getMessage());
      break;
    }
  }

  return 0;
}

//******************** End of __LINUX_OSS__ *********************//

#elif defined(__WINDOWS_DS__) // Windows DirectSound API

#include <dsound.h>

// Declarations for utility functions, callbacks, and structures
// specific to the DirectSound implementation.
static bool CALLBACK deviceCountCallback(LPGUID lpguid,
                                         LPCSTR lpcstrDescription,
                                         LPCSTR lpcstrModule,
                                         LPVOID lpContext);

static bool CALLBACK deviceInfoCallback(LPGUID lpguid,
                                        LPCSTR lpcstrDescription,
                                        LPCSTR lpcstrModule,
                                        LPVOID lpContext);

static char* getErrorString(int code);

struct enum_info {
  char name[64];
  LPGUID id;
  bool isInput;
  bool isValid;
};

// RtAudio methods for DirectSound implementation.
void RtAudio :: initialize(void)
{
  int i, ins = 0, outs = 0, count = 0;
  int index = 0;
  HRESULT result;
  nDevices = 0;

  // Count DirectSound devices.
  result = DirectSoundEnumerate((LPDSENUMCALLBACK)deviceCountCallback, &outs);
  if ( FAILED(result) ) {
    sprintf(message, "RtAudio: Unable to enumerate through sound playback devices: %s.",
            getErrorString(result));
    error(RtError::DRIVER_ERROR);
  }

  // Count DirectSoundCapture devices.
  result = DirectSoundCaptureEnumerate((LPDSENUMCALLBACK)deviceCountCallback, &ins);
  if ( FAILED(result) ) {
    sprintf(message, "RtAudio: Unable to enumerate through sound capture devices: %s.",
            getErrorString(result));
    error(RtError::DRIVER_ERROR);
  }

  count = ins + outs;
  if (count == 0) return;

  std::vector<enum_info> info(count);
  for (i=0; i<count; i++) {
    info[i].name[0] = '\0';
    if (i < outs) info[i].isInput = false;
    else info[i].isInput = true;
  }

  // Get playback device info and check capabilities.
  result = DirectSoundEnumerate((LPDSENUMCALLBACK)deviceInfoCallback, &info[0]);
  if ( FAILED(result) ) {
    sprintf(message, "RtAudio: Unable to enumerate through sound playback devices: %s.",
            getErrorString(result));
    error(RtError::DRIVER_ERROR);
  }

  // Get capture device info and check capabilities.
  result = DirectSoundCaptureEnumerate((LPDSENUMCALLBACK)deviceInfoCallback, &info[0]);
  if ( FAILED(result) ) {
    sprintf(message, "RtAudio: Unable to enumerate through sound capture devices: %s.",
            getErrorString(result));
    error(RtError::DRIVER_ERROR);
  }

  // Parse the devices and check validity.  Devices are considered
  // invalid if they cannot be opened, they report no supported data
  // formats, or they report < 1 supported channels.
  for (i=0; i<count; i++) {
    if (info[i].isValid && info[i].id == NULL ) // default device
      nDevices++;
  }

  // We group the default input and output devices together (as one
  // device) .
  if (nDevices > 0) {
    nDevices = 1;
    index = 1;
  }

  // Non-default devices are listed separately.
  for (i=0; i<count; i++) {
    if (info[i].isValid && info[i].id != NULL )
      nDevices++;
  }

  if (nDevices == 0) return;

  //  Allocate the RTAUDIO_DEVICE structures.
  devices = (RTAUDIO_DEVICE *) calloc(nDevices, sizeof(RTAUDIO_DEVICE));
  if (devices == NULL) {
    sprintf(message, "RtAudio: memory allocation error!");
    error(RtError::MEMORY_ERROR);
  }

  // Initialize the GUIDs to NULL for later validation.
  for (i=0; i<nDevices; i++) {
    devices[i].id[0] = NULL;
    devices[i].id[1] = NULL;
  }

  // Rename the default device(s).
  if (index)
    strcpy(devices[0].name, "Default Input/Output Devices");

  // Copy the names and GUIDs to our devices structures.
  for (i=0; i<count; i++) {
    if (info[i].isValid && info[i].id != NULL ) {
      strncpy(devices[index].name, info[i].name, 64);
      if (info[i].isInput)
        devices[index].id[1] = info[i].id;
      else
        devices[index].id[0] = info[i].id;
      index++;
    }
  }

  for (i=0;i<nDevices; i++)
    probeDeviceInfo(&devices[i]);

  return;
}

void RtAudio :: probeDeviceInfo(RTAUDIO_DEVICE *info)
{
  HRESULT result;

  // Get the device index so that we can check the device handle.
  int index;
  for (index=0; index<nDevices; index++)
    if ( info == &devices[index] ) break;

  if ( index >= nDevices ) {
    sprintf(message, "RtAudio: device (%s) indexing error in DirectSound probeDeviceInfo().",
            info->name);
    error(RtError::WARNING);
    return;
  }

  // Do capture probe first.  If this is not the default device (index
  // = 0) _and_ GUID = NULL, then the capture handle is invalid.
  if ( index != 0 && info->id[1] == NULL )
    goto playback_probe;

  LPDIRECTSOUNDCAPTURE  input;
  result = DirectSoundCaptureCreate( info->id[0], &input, NULL );
  if ( FAILED(result) ) {
    sprintf(message, "RtAudio: Could not create DirectSound capture object (%s): %s.",
            info->name, getErrorString(result));
    error(RtError::WARNING);
    goto playback_probe;
  }

  DSCCAPS in_caps;
  in_caps.dwSize = sizeof(in_caps);
  result = input->GetCaps( &in_caps );
  if ( FAILED(result) ) {
    input->Release();
    sprintf(message, "RtAudio: Could not get DirectSound capture capabilities (%s): %s.",
            info->name, getErrorString(result));
    error(RtError::WARNING);
    goto playback_probe;
  }

  // Get input channel information.
  info->minInputChannels = 1;
  info->maxInputChannels = in_caps.dwChannels;

  // Get sample rate and format information.
  if( in_caps.dwChannels == 2 ) {
    if( in_caps.dwFormats & WAVE_FORMAT_1S16 ) info->nativeFormats |= RTAUDIO_SINT16;
    if( in_caps.dwFormats & WAVE_FORMAT_2S16 ) info->nativeFormats |= RTAUDIO_SINT16;
    if( in_caps.dwFormats & WAVE_FORMAT_4S16 ) info->nativeFormats |= RTAUDIO_SINT16;
    if( in_caps.dwFormats & WAVE_FORMAT_1S08 ) info->nativeFormats |= RTAUDIO_SINT8;
    if( in_caps.dwFormats & WAVE_FORMAT_2S08 ) info->nativeFormats |= RTAUDIO_SINT8;
    if( in_caps.dwFormats & WAVE_FORMAT_4S08 ) info->nativeFormats |= RTAUDIO_SINT8;

    if ( info->nativeFormats & RTAUDIO_SINT16 ) {
      if( in_caps.dwFormats & WAVE_FORMAT_1S16 ) info->sampleRates[info->nSampleRates++] = 11025;
      if( in_caps.dwFormats & WAVE_FORMAT_2S16 ) info->sampleRates[info->nSampleRates++] = 22050;
      if( in_caps.dwFormats & WAVE_FORMAT_4S16 ) info->sampleRates[info->nSampleRates++] = 44100;
    }
    else if ( info->nativeFormats & RTAUDIO_SINT8 ) {
      if( in_caps.dwFormats & WAVE_FORMAT_1S08 ) info->sampleRates[info->nSampleRates++] = 11025;
      if( in_caps.dwFormats & WAVE_FORMAT_2S08 ) info->sampleRates[info->nSampleRates++] = 22050;
      if( in_caps.dwFormats & WAVE_FORMAT_4S08 ) info->sampleRates[info->nSampleRates++] = 44100;
    }
  }
  else if ( in_caps.dwChannels == 1 ) {
    if( in_caps.dwFormats & WAVE_FORMAT_1M16 ) info->nativeFormats |= RTAUDIO_SINT16;
    if( in_caps.dwFormats & WAVE_FORMAT_2M16 ) info->nativeFormats |= RTAUDIO_SINT16;
    if( in_caps.dwFormats & WAVE_FORMAT_4M16 ) info->nativeFormats |= RTAUDIO_SINT16;
    if( in_caps.dwFormats & WAVE_FORMAT_1M08 ) info->nativeFormats |= RTAUDIO_SINT8;
    if( in_caps.dwFormats & WAVE_FORMAT_2M08 ) info->nativeFormats |= RTAUDIO_SINT8;
    if( in_caps.dwFormats & WAVE_FORMAT_4M08 ) info->nativeFormats |= RTAUDIO_SINT8;

    if ( info->nativeFormats & RTAUDIO_SINT16 ) {
      if( in_caps.dwFormats & WAVE_FORMAT_1M16 ) info->sampleRates[info->nSampleRates++] = 11025;
      if( in_caps.dwFormats & WAVE_FORMAT_2M16 ) info->sampleRates[info->nSampleRates++] = 22050;
      if( in_caps.dwFormats & WAVE_FORMAT_4M16 ) info->sampleRates[info->nSampleRates++] = 44100;
    }
    else if ( info->nativeFormats & RTAUDIO_SINT8 ) {
      if( in_caps.dwFormats & WAVE_FORMAT_1M08 ) info->sampleRates[info->nSampleRates++] = 11025;
      if( in_caps.dwFormats & WAVE_FORMAT_2M08 ) info->sampleRates[info->nSampleRates++] = 22050;
      if( in_caps.dwFormats & WAVE_FORMAT_4M08 ) info->sampleRates[info->nSampleRates++] = 44100;
    }
  }
  else info->minInputChannels = 0; // technically, this would be an error

  input->Release();

 playback_probe:
  LPDIRECTSOUND  output;
  DSCAPS out_caps;

  // Now do playback probe.  If this is not the default device (index
  // = 0) _and_ GUID = NULL, then the playback handle is invalid.
  if ( index != 0 && info->id[0] == NULL )
    goto check_parameters;

  result = DirectSoundCreate( info->id[0], &output, NULL );
  if ( FAILED(result) ) {
    sprintf(message, "RtAudio: Could not create DirectSound playback object (%s): %s.",
            info->name, getErrorString(result));
    error(RtError::WARNING);
    goto check_parameters;
  }

  out_caps.dwSize = sizeof(out_caps);
  result = output->GetCaps( &out_caps );
  if ( FAILED(result) ) {
    output->Release();
    sprintf(message, "RtAudio: Could not get DirectSound playback capabilities (%s): %s.",
            info->name, getErrorString(result));
    error(RtError::WARNING);
    goto check_parameters;
  }

  // Get output channel information.
  info->minOutputChannels = 1;
  info->maxOutputChannels = ( out_caps.dwFlags & DSCAPS_PRIMARYSTEREO ) ? 2 : 1;

  // Get sample rate information.  Use capture device rate information
  // if it exists.
  if ( info->nSampleRates == 0 ) {
    info->sampleRates[0] = (int) out_caps.dwMinSecondarySampleRate;
    info->sampleRates[1] = (int) out_caps.dwMaxSecondarySampleRate;
    if ( out_caps.dwFlags & DSCAPS_CONTINUOUSRATE )
      info->nSampleRates = -1;
    else if ( out_caps.dwMinSecondarySampleRate == out_caps.dwMaxSecondarySampleRate ) {
      if ( out_caps.dwMinSecondarySampleRate == 0 ) {
        // This is a bogus driver report ... fake the range and cross
        // your fingers.
        info->sampleRates[0] = 11025;
				info->sampleRates[1] = 48000;
        info->nSampleRates = -1; /* continuous range */
        sprintf(message, "RtAudio: bogus sample rates reported by DirectSound driver ... using defaults (%s).",
                info->name);
        error(RtError::WARNING);
      }
      else {
        info->nSampleRates = 1;
			}
    }
    else if ( (out_caps.dwMinSecondarySampleRate < 1000.0) &&
              (out_caps.dwMaxSecondarySampleRate > 50000.0) ) {
      // This is a bogus driver report ... support for only two
      // distant rates.  We'll assume this is a range.
      info->nSampleRates = -1;
      sprintf(message, "RtAudio: bogus sample rates reported by DirectSound driver ... using range (%s).",
              info->name);
      error(RtError::WARNING);
    }
    else info->nSampleRates = 2;
  }
  else {
    // Check input rates against output rate range
    for ( int i=info->nSampleRates-1; i>=0; i-- ) {
      if ( info->sampleRates[i] <= out_caps.dwMaxSecondarySampleRate )
        break;
      info->nSampleRates--;
    }
    while ( info->sampleRates[0] < out_caps.dwMinSecondarySampleRate ) {
      info->nSampleRates--;
      for ( int i=0; i<info->nSampleRates; i++)
        info->sampleRates[i] = info->sampleRates[i+1];
      if ( info->nSampleRates <= 0 ) break;
    }
  }

  // Get format information.
  if ( out_caps.dwFlags & DSCAPS_PRIMARY16BIT ) info->nativeFormats |= RTAUDIO_SINT16;
  if ( out_caps.dwFlags & DSCAPS_PRIMARY8BIT ) info->nativeFormats |= RTAUDIO_SINT8;

  output->Release();

 check_parameters:
  if ( info->maxInputChannels == 0 && info->maxOutputChannels == 0 )
    return;
  if ( info->nSampleRates == 0 || info->nativeFormats == 0 )
    return;

  // Determine duplex status.
  if (info->maxInputChannels < info->maxOutputChannels)
    info->maxDuplexChannels = info->maxInputChannels;
  else
    info->maxDuplexChannels = info->maxOutputChannels;
  if (info->minInputChannels < info->minOutputChannels)
    info->minDuplexChannels = info->minInputChannels;
  else
    info->minDuplexChannels = info->minOutputChannels;

  if ( info->maxDuplexChannels > 0 ) info->hasDuplexSupport = true;
  else info->hasDuplexSupport = false;

  info->probed = true;

  return;
}

bool RtAudio :: probeDeviceOpen(int device, RTAUDIO_STREAM *stream,
                                STREAM_MODE mode, int channels, 
                                int sampleRate, RTAUDIO_FORMAT format,
                                int *bufferSize, int numberOfBuffers)
{
  HRESULT result;
  HWND hWnd = GetForegroundWindow();
  // According to a note in PortAudio, using GetDesktopWindow()
  // instead of GetForegroundWindow() is supposed to avoid problems
  // that occur when the application's window is not the foreground
  // window.  Also, if the application window closes before the
  // DirectSound buffer, DirectSound can crash.  However, for console
  // applications, no sound was produced when using GetDesktopWindow().
  long buffer_size;
  LPVOID audioPtr;
  DWORD dataLen;
  int nBuffers;

  // Check the numberOfBuffers parameter and limit the lowest value to
  // two.  This is a judgement call and a value of two is probably too
  // low for capture, but it should work for playback.
  if (numberOfBuffers < 2)
    nBuffers = 2;
  else
    nBuffers = numberOfBuffers;

  // Define the wave format structure (16-bit PCM, srate, channels)
  WAVEFORMATEX waveFormat;
  ZeroMemory(&waveFormat, sizeof(WAVEFORMATEX));
  waveFormat.wFormatTag = WAVE_FORMAT_PCM;
  waveFormat.nChannels = channels;
  waveFormat.nSamplesPerSec = (unsigned long) sampleRate;

  // Determine the data format.
  if ( devices[device].nativeFormats ) { // 8-bit and/or 16-bit support
    if ( format == RTAUDIO_SINT8 ) {
      if ( devices[device].nativeFormats & RTAUDIO_SINT8 )
        waveFormat.wBitsPerSample = 8;
      else
        waveFormat.wBitsPerSample = 16;
    }
    else {
      if ( devices[device].nativeFormats & RTAUDIO_SINT16 )
        waveFormat.wBitsPerSample = 16;
      else
        waveFormat.wBitsPerSample = 8;
    }
  }
  else {
    sprintf(message, "RtAudio: no reported data formats for DirectSound device (%s).",
            devices[device].name);
    error(RtError::WARNING);
    return FAILURE;
  }

  waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
  waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

  if ( mode == PLAYBACK ) {

    if ( devices[device].maxOutputChannels < channels )
      return FAILURE;

    LPGUID id = devices[device].id[0];
    LPDIRECTSOUND  object;
    LPDIRECTSOUNDBUFFER buffer;
    DSBUFFERDESC bufferDescription;
    
    result = DirectSoundCreate( id, &object, NULL );
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Could not create DirectSound playback object (%s): %s.",
              devices[device].name, getErrorString(result));
      error(RtError::WARNING);
      return FAILURE;
    }

    // Set cooperative level to DSSCL_EXCLUSIVE
    result = object->SetCooperativeLevel(hWnd, DSSCL_EXCLUSIVE);
    if ( FAILED(result) ) {
      object->Release();
      sprintf(message, "RtAudio: Unable to set DirectSound cooperative level (%s): %s.",
              devices[device].name, getErrorString(result));
      error(RtError::WARNING);
      return FAILURE;
    }

    // Even though we will write to the secondary buffer, we need to
    // access the primary buffer to set the correct output format.
    // The default is 8-bit, 22 kHz!
    // Setup the DS primary buffer description.
    ZeroMemory(&bufferDescription, sizeof(DSBUFFERDESC));
    bufferDescription.dwSize = sizeof(DSBUFFERDESC);
    bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
    // Obtain the primary buffer
    result = object->CreateSoundBuffer(&bufferDescription, &buffer, NULL);
    if ( FAILED(result) ) {
      object->Release();
      sprintf(message, "RtAudio: Unable to access DS primary buffer (%s): %s.",
              devices[device].name, getErrorString(result));
      error(RtError::WARNING);
      return FAILURE;
    }

    // Set the primary DS buffer sound format.
    result = buffer->SetFormat(&waveFormat);
    if ( FAILED(result) ) {
      object->Release();
      sprintf(message, "RtAudio: Unable to set DS primary buffer format (%s): %s.",
              devices[device].name, getErrorString(result));
      error(RtError::WARNING);
      return FAILURE;
    }

    // Setup the secondary DS buffer description.
    buffer_size = channels * *bufferSize * nBuffers * waveFormat.wBitsPerSample / 8;
    ZeroMemory(&bufferDescription, sizeof(DSBUFFERDESC));
    bufferDescription.dwSize = sizeof(DSBUFFERDESC);
    bufferDescription.dwFlags = ( DSBCAPS_STICKYFOCUS |
                                  DSBCAPS_GETCURRENTPOSITION2 |
                                  DSBCAPS_LOCHARDWARE );  // Force hardware mixing
    bufferDescription.dwBufferBytes = buffer_size;
    bufferDescription.lpwfxFormat = &waveFormat;

    // Try to create the secondary DS buffer.  If that doesn't work,
    // try to use software mixing.  Otherwise, there's a problem.
    result = object->CreateSoundBuffer(&bufferDescription, &buffer, NULL);
    if ( FAILED(result) ) {
      bufferDescription.dwFlags = ( DSBCAPS_STICKYFOCUS |
                                    DSBCAPS_GETCURRENTPOSITION2 |
                                    DSBCAPS_LOCSOFTWARE );  // Force software mixing
      result = object->CreateSoundBuffer(&bufferDescription, &buffer, NULL);
      if ( FAILED(result) ) {
        object->Release();
        sprintf(message, "RtAudio: Unable to create secondary DS buffer (%s): %s.",
                devices[device].name, getErrorString(result));
        error(RtError::WARNING);
        return FAILURE;
      }
    }

    // Get the buffer size ... might be different from what we specified.
    DSBCAPS dsbcaps;
    dsbcaps.dwSize = sizeof(DSBCAPS);
    buffer->GetCaps(&dsbcaps);
    buffer_size = dsbcaps.dwBufferBytes;

    // Lock the DS buffer
    result = buffer->Lock(0, buffer_size, &audioPtr, &dataLen, NULL, NULL, 0);
    if ( FAILED(result) ) {
      object->Release();
      sprintf(message, "RtAudio: Unable to lock DS buffer (%s): %s.",
              devices[device].name, getErrorString(result));
      error(RtError::WARNING);
      return FAILURE;
    }

    // Zero the DS buffer
    ZeroMemory(audioPtr, dataLen);

    // Unlock the DS buffer
    result = buffer->Unlock(audioPtr, dataLen, NULL, 0);
    if ( FAILED(result) ) {
      object->Release();
      sprintf(message, "RtAudio: Unable to unlock DS buffer(%s): %s.",
              devices[device].name, getErrorString(result));
      error(RtError::WARNING);
      return FAILURE;
    }

    stream->handle[0].object = (void *) object;
    stream->handle[0].buffer = (void *) buffer;
    stream->nDeviceChannels[0] = channels;
  }

  if ( mode == RECORD ) {

    if ( devices[device].maxInputChannels < channels )
      return FAILURE;

    LPGUID id = devices[device].id[1];
    LPDIRECTSOUNDCAPTURE  object;
    LPDIRECTSOUNDCAPTUREBUFFER buffer;
    DSCBUFFERDESC bufferDescription;

    result = DirectSoundCaptureCreate( id, &object, NULL );
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Could not create DirectSound capture object (%s): %s.",
              devices[device].name, getErrorString(result));
      error(RtError::WARNING);
      return FAILURE;
    }

    // Setup the secondary DS buffer description.
    buffer_size = channels * *bufferSize * nBuffers * waveFormat.wBitsPerSample / 8;
    ZeroMemory(&bufferDescription, sizeof(DSCBUFFERDESC));
    bufferDescription.dwSize = sizeof(DSCBUFFERDESC);
    bufferDescription.dwFlags = 0;
    bufferDescription.dwReserved = 0;
    bufferDescription.dwBufferBytes = buffer_size;
    bufferDescription.lpwfxFormat = &waveFormat;

    // Create the capture buffer.
    result = object->CreateCaptureBuffer(&bufferDescription, &buffer, NULL);
    if ( FAILED(result) ) {
      object->Release();
      sprintf(message, "RtAudio: Unable to create DS capture buffer (%s): %s.",
              devices[device].name, getErrorString(result));
      error(RtError::WARNING);
      return FAILURE;
    }

    // Lock the capture buffer
    result = buffer->Lock(0, buffer_size, &audioPtr, &dataLen, NULL, NULL, 0);
    if ( FAILED(result) ) {
      object->Release();
      sprintf(message, "RtAudio: Unable to lock DS capture buffer (%s): %s.",
              devices[device].name, getErrorString(result));
      error(RtError::WARNING);
      return FAILURE;
    }

    // Zero the buffer
    ZeroMemory(audioPtr, dataLen);

    // Unlock the buffer
    result = buffer->Unlock(audioPtr, dataLen, NULL, 0);
    if ( FAILED(result) ) {
      object->Release();
      sprintf(message, "RtAudio: Unable to unlock DS capture buffer (%s): %s.",
              devices[device].name, getErrorString(result));
      error(RtError::WARNING);
      return FAILURE;
    }

    stream->handle[1].object = (void *) object;
    stream->handle[1].buffer = (void *) buffer;
    stream->nDeviceChannels[1] = channels;
  }

  stream->userFormat = format;
  if ( waveFormat.wBitsPerSample == 8 )
    stream->deviceFormat[mode] = RTAUDIO_SINT8;
  else
    stream->deviceFormat[mode] = RTAUDIO_SINT16;
  stream->nUserChannels[mode] = channels;
  *bufferSize = buffer_size / (channels * nBuffers * waveFormat.wBitsPerSample / 8);
  stream->bufferSize = *bufferSize;

  // Set flags for buffer conversion
  stream->doConvertBuffer[mode] = false;
  if (stream->userFormat != stream->deviceFormat[mode])
    stream->doConvertBuffer[mode] = true;
  if (stream->nUserChannels[mode] < stream->nDeviceChannels[mode])
    stream->doConvertBuffer[mode] = true;

  // Allocate necessary internal buffers
  if ( stream->nUserChannels[0] != stream->nUserChannels[1] ) {

    long buffer_bytes;
    if (stream->nUserChannels[0] >= stream->nUserChannels[1])
      buffer_bytes = stream->nUserChannels[0];
    else
      buffer_bytes = stream->nUserChannels[1];

    buffer_bytes *= *bufferSize * formatBytes(stream->userFormat);
    if (stream->userBuffer) free(stream->userBuffer);
    stream->userBuffer = (char *) calloc(buffer_bytes, 1);
    if (stream->userBuffer == NULL)
      goto memory_error;
  }

  if ( stream->doConvertBuffer[mode] ) {

    long buffer_bytes;
    bool makeBuffer = true;
    if ( mode == PLAYBACK )
      buffer_bytes = stream->nDeviceChannels[0] * formatBytes(stream->deviceFormat[0]);
    else { // mode == RECORD
      buffer_bytes = stream->nDeviceChannels[1] * formatBytes(stream->deviceFormat[1]);
      if ( stream->mode == PLAYBACK ) {
        long bytes_out = stream->nDeviceChannels[0] * formatBytes(stream->deviceFormat[0]);
        if ( buffer_bytes > bytes_out )
          buffer_bytes = (buffer_bytes > bytes_out) ? buffer_bytes : bytes_out;
        else
          makeBuffer = false;
      }
    }

    if ( makeBuffer ) {
      buffer_bytes *= *bufferSize;
      if (stream->deviceBuffer) free(stream->deviceBuffer);
      stream->deviceBuffer = (char *) calloc(buffer_bytes, 1);
      if (stream->deviceBuffer == NULL)
        goto memory_error;
    }
  }

  stream->device[mode] = device;
  stream->state = STREAM_STOPPED;
  if ( stream->mode == PLAYBACK && mode == RECORD )
    // We had already set up an output stream.
    stream->mode = DUPLEX;
  else
    stream->mode = mode;
  stream->nBuffers = nBuffers;
  stream->sampleRate = sampleRate;

  return SUCCESS;

 memory_error:
  if (stream->handle[0].object) {
    LPDIRECTSOUND object = (LPDIRECTSOUND) stream->handle[0].object;
    LPDIRECTSOUNDBUFFER buffer = (LPDIRECTSOUNDBUFFER) stream->handle[0].buffer;
    if (buffer) {
      buffer->Release();
      stream->handle[0].buffer = NULL;
    }
    object->Release();
    stream->handle[0].object = NULL;
  }
  if (stream->handle[1].object) {
    LPDIRECTSOUNDCAPTURE object = (LPDIRECTSOUNDCAPTURE) stream->handle[1].object;
    LPDIRECTSOUNDCAPTUREBUFFER buffer = (LPDIRECTSOUNDCAPTUREBUFFER) stream->handle[1].buffer;
    if (buffer) {
      buffer->Release();
      stream->handle[1].buffer = NULL;
    }
    object->Release();
    stream->handle[1].object = NULL;
  }
  if (stream->userBuffer) {
    free(stream->userBuffer);
    stream->userBuffer = 0;
  }
  sprintf(message, "RtAudio: error allocating buffer memory (%s).",
          devices[device].name);
  error(RtError::WARNING);
  return FAILURE;
}

void RtAudio :: cancelStreamCallback(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  if (stream->usingCallback) {
    stream->usingCallback = false;
    WaitForSingleObject( (HANDLE)stream->thread, INFINITE );
    CloseHandle( (HANDLE)stream->thread );
    stream->thread = 0;
    stream->callback = NULL;
    stream->userData = NULL;
  }
}

void RtAudio :: closeStream(int streamId)
{
  // We don't want an exception to be thrown here because this
  // function is called by our class destructor.  So, do our own
  // streamId check.
  if ( streams.find( streamId ) == streams.end() ) {
    sprintf(message, "RtAudio: invalid stream identifier!");
    error(RtError::WARNING);
    return;
  }

  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) streams[streamId];

  if (stream->usingCallback) {
    stream->usingCallback = false;
    WaitForSingleObject( (HANDLE)stream->thread, INFINITE );
    CloseHandle( (HANDLE)stream->thread );
  }

  DeleteCriticalSection(&stream->mutex);

  if (stream->handle[0].object) {
    LPDIRECTSOUND object = (LPDIRECTSOUND) stream->handle[0].object;
    LPDIRECTSOUNDBUFFER buffer = (LPDIRECTSOUNDBUFFER) stream->handle[0].buffer;
    if (buffer) {
      buffer->Stop();
      buffer->Release();
    }
    object->Release();
  }

  if (stream->handle[1].object) {
    LPDIRECTSOUNDCAPTURE object = (LPDIRECTSOUNDCAPTURE) stream->handle[1].object;
    LPDIRECTSOUNDCAPTUREBUFFER buffer = (LPDIRECTSOUNDCAPTUREBUFFER) stream->handle[1].buffer;
    if (buffer) {
      buffer->Stop();
      buffer->Release();
    }
    object->Release();
  }

  if (stream->userBuffer)
    free(stream->userBuffer);

  if (stream->deviceBuffer)
    free(stream->deviceBuffer);

  free(stream);
  streams.erase(streamId);
}

void RtAudio :: startStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  if (stream->state == STREAM_RUNNING)
    goto unlock;

  HRESULT result;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {
    LPDIRECTSOUNDBUFFER buffer = (LPDIRECTSOUNDBUFFER) stream->handle[0].buffer;
    result = buffer->Play(0, 0, DSBPLAY_LOOPING );
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to start DS buffer (%s): %s.",
              devices[stream->device[0]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }
  }

  if (stream->mode == RECORD || stream->mode == DUPLEX) {
    LPDIRECTSOUNDCAPTUREBUFFER buffer = (LPDIRECTSOUNDCAPTUREBUFFER) stream->handle[1].buffer;
    result = buffer->Start(DSCBSTART_LOOPING );
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to start DS capture buffer (%s): %s.",
              devices[stream->device[1]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }
  }
  stream->state = STREAM_RUNNING;

 unlock:
  MUTEX_UNLOCK(&stream->mutex);
}

void RtAudio :: stopStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  if (stream->state == STREAM_STOPPED) {
    MUTEX_UNLOCK(&stream->mutex);
    return;
  }

  // There is no specific DirectSound API call to "drain" a buffer
  // before stopping.  We can hack this for playback by writing zeroes
  // for another bufferSize * nBuffers frames.  For capture, the
  // concept is less clear so we'll repeat what we do in the
  // abortStream() case.
  HRESULT result;
  DWORD dsBufferSize;
  LPVOID buffer1 = NULL;
  LPVOID buffer2 = NULL;
  DWORD bufferSize1 = 0;
  DWORD bufferSize2 = 0;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {

    DWORD currentPos, safePos;
    long buffer_bytes = stream->bufferSize * stream->nDeviceChannels[0];
    buffer_bytes *= formatBytes(stream->deviceFormat[0]);

    LPDIRECTSOUNDBUFFER dsBuffer = (LPDIRECTSOUNDBUFFER) stream->handle[0].buffer;
    UINT nextWritePos = stream->handle[0].bufferPointer;
    dsBufferSize = buffer_bytes * stream->nBuffers;

    // Write zeroes for nBuffer counts.
    for (int i=0; i<stream->nBuffers; i++) {

      // Find out where the read and "safe write" pointers are.
      result = dsBuffer->GetCurrentPosition(&currentPos, &safePos);
      if ( FAILED(result) ) {
        sprintf(message, "RtAudio: Unable to get current DS position (%s): %s.",
                devices[stream->device[0]].name, getErrorString(result));
        error(RtError::DRIVER_ERROR);
      }

      if ( currentPos < nextWritePos ) currentPos += dsBufferSize; // unwrap offset
      DWORD endWrite = nextWritePos + buffer_bytes;

      // Check whether the entire write region is behind the play pointer.
      while ( currentPos < endWrite ) {
        float millis = (endWrite - currentPos) * 900.0;
        millis /= ( formatBytes(stream->deviceFormat[0]) * stream->sampleRate);
        if ( millis < 1.0 ) millis = 1.0;
        Sleep( (DWORD) millis );

        // Wake up, find out where we are now
        result = dsBuffer->GetCurrentPosition( &currentPos, &safePos );
        if ( FAILED(result) ) {
          sprintf(message, "RtAudio: Unable to get current DS position (%s): %s.",
                  devices[stream->device[0]].name, getErrorString(result));
          error(RtError::DRIVER_ERROR);
        }
        if ( currentPos < nextWritePos ) currentPos += dsBufferSize; // unwrap offset
      }

      // Lock free space in the buffer
      result = dsBuffer->Lock (nextWritePos, buffer_bytes, &buffer1,
                               &bufferSize1, &buffer2, &bufferSize2, 0);
      if ( FAILED(result) ) {
        sprintf(message, "RtAudio: Unable to lock DS buffer during playback (%s): %s.",
                devices[stream->device[0]].name, getErrorString(result));
        error(RtError::DRIVER_ERROR);
      }

      // Zero the free space
      ZeroMemory(buffer1, bufferSize1);
      if (buffer2 != NULL) ZeroMemory(buffer2, bufferSize2);

      // Update our buffer offset and unlock sound buffer
      dsBuffer->Unlock (buffer1, bufferSize1, buffer2, bufferSize2);
      if ( FAILED(result) ) {
        sprintf(message, "RtAudio: Unable to unlock DS buffer during playback (%s): %s.",
                devices[stream->device[0]].name, getErrorString(result));
        error(RtError::DRIVER_ERROR);
      }
      nextWritePos = (nextWritePos + bufferSize1 + bufferSize2) % dsBufferSize;
      stream->handle[0].bufferPointer = nextWritePos;
    }

    // If we play again, start at the beginning of the buffer.
    stream->handle[0].bufferPointer = 0;
  }

  if (stream->mode == RECORD || stream->mode == DUPLEX) {
    LPDIRECTSOUNDCAPTUREBUFFER buffer = (LPDIRECTSOUNDCAPTUREBUFFER) stream->handle[1].buffer;
    buffer1 = NULL;
    bufferSize1 = 0;

    result = buffer->Stop();
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to stop DS capture buffer (%s): %s",
              devices[stream->device[1]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    dsBufferSize = stream->bufferSize * stream->nDeviceChannels[1];
    dsBufferSize *= formatBytes(stream->deviceFormat[1]) * stream->nBuffers;

    // Lock the buffer and clear it so that if we start to play again,
    // we won't have old data playing.
    result = buffer->Lock(0, dsBufferSize, &buffer1, &bufferSize1, NULL, NULL, 0);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to lock DS capture buffer (%s): %s.",
              devices[stream->device[1]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    // Zero the DS buffer
    ZeroMemory(buffer1, bufferSize1);

    // Unlock the DS buffer
    result = buffer->Unlock(buffer1, bufferSize1, NULL, 0);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to unlock DS capture buffer (%s): %s.",
              devices[stream->device[1]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    // If we start recording again, we must begin at beginning of buffer.
    stream->handle[1].bufferPointer = 0;
  }
  stream->state = STREAM_STOPPED;

  MUTEX_UNLOCK(&stream->mutex);
}

void RtAudio :: abortStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  if (stream->state == STREAM_STOPPED)
    goto unlock;

  HRESULT result;
  long dsBufferSize;
  LPVOID audioPtr;
  DWORD dataLen;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {
    LPDIRECTSOUNDBUFFER buffer = (LPDIRECTSOUNDBUFFER) stream->handle[0].buffer;
    result = buffer->Stop();
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to stop DS buffer (%s): %s",
              devices[stream->device[0]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    dsBufferSize = stream->bufferSize * stream->nDeviceChannels[0];
    dsBufferSize *= formatBytes(stream->deviceFormat[0]) * stream->nBuffers;

    // Lock the buffer and clear it so that if we start to play again,
    // we won't have old data playing.
    result = buffer->Lock(0, dsBufferSize, &audioPtr, &dataLen, NULL, NULL, 0);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to lock DS buffer (%s): %s.",
              devices[stream->device[0]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    // Zero the DS buffer
    ZeroMemory(audioPtr, dataLen);

    // Unlock the DS buffer
    result = buffer->Unlock(audioPtr, dataLen, NULL, 0);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to unlock DS buffer (%s): %s.",
              devices[stream->device[0]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    // If we start playing again, we must begin at beginning of buffer.
    stream->handle[0].bufferPointer = 0;
  }

  if (stream->mode == RECORD || stream->mode == DUPLEX) {
    LPDIRECTSOUNDCAPTUREBUFFER buffer = (LPDIRECTSOUNDCAPTUREBUFFER) stream->handle[1].buffer;
    audioPtr = NULL;
    dataLen = 0;

    result = buffer->Stop();
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to stop DS capture buffer (%s): %s",
              devices[stream->device[1]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    dsBufferSize = stream->bufferSize * stream->nDeviceChannels[1];
    dsBufferSize *= formatBytes(stream->deviceFormat[1]) * stream->nBuffers;

    // Lock the buffer and clear it so that if we start to play again,
    // we won't have old data playing.
    result = buffer->Lock(0, dsBufferSize, &audioPtr, &dataLen, NULL, NULL, 0);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to lock DS capture buffer (%s): %s.",
              devices[stream->device[1]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    // Zero the DS buffer
    ZeroMemory(audioPtr, dataLen);

    // Unlock the DS buffer
    result = buffer->Unlock(audioPtr, dataLen, NULL, 0);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to unlock DS capture buffer (%s): %s.",
              devices[stream->device[1]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    // If we start recording again, we must begin at beginning of buffer.
    stream->handle[1].bufferPointer = 0;
  }
  stream->state = STREAM_STOPPED;

 unlock:
  MUTEX_UNLOCK(&stream->mutex);
}

int RtAudio :: streamWillBlock(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  int channels;
  int frames = 0;
  if (stream->state == STREAM_STOPPED)
    goto unlock;

  HRESULT result;
  DWORD currentPos, safePos;
  channels = 1;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {

    LPDIRECTSOUNDBUFFER dsBuffer = (LPDIRECTSOUNDBUFFER) stream->handle[0].buffer;
    UINT nextWritePos = stream->handle[0].bufferPointer;
    channels = stream->nDeviceChannels[0];
    DWORD dsBufferSize = stream->bufferSize * channels;
    dsBufferSize *= formatBytes(stream->deviceFormat[0]) * stream->nBuffers;

    // Find out where the read and "safe write" pointers are.
    result = dsBuffer->GetCurrentPosition(&currentPos, &safePos);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to get current DS position (%s): %s.",
              devices[stream->device[0]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    if ( currentPos < nextWritePos ) currentPos += dsBufferSize; // unwrap offset
    frames = currentPos - nextWritePos;
    frames /= channels * formatBytes(stream->deviceFormat[0]);
  }

  if (stream->mode == RECORD || stream->mode == DUPLEX) {

    LPDIRECTSOUNDCAPTUREBUFFER dsBuffer = (LPDIRECTSOUNDCAPTUREBUFFER) stream->handle[1].buffer;
    UINT nextReadPos = stream->handle[1].bufferPointer;
    channels = stream->nDeviceChannels[1];
    DWORD dsBufferSize = stream->bufferSize * channels;
    dsBufferSize *= formatBytes(stream->deviceFormat[1]) * stream->nBuffers;

    // Find out where the write and "safe read" pointers are.
    result = dsBuffer->GetCurrentPosition(&currentPos, &safePos);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to get current DS capture position (%s): %s.",
              devices[stream->device[1]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    if ( safePos < nextReadPos ) safePos += dsBufferSize; // unwrap offset

    if (stream->mode == DUPLEX ) {
      // Take largest value of the two.
      int temp = safePos - nextReadPos;
      temp /= channels * formatBytes(stream->deviceFormat[1]);
      frames = ( temp > frames ) ? temp : frames;
    }
    else {
      frames = safePos - nextReadPos;
      frames /= channels * formatBytes(stream->deviceFormat[1]);
    }
  }

  frames = stream->bufferSize - frames;
  if (frames < 0) frames = 0;

 unlock:
  MUTEX_UNLOCK(&stream->mutex);
  return frames;
}

void RtAudio :: tickStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  int stopStream = 0;
  if (stream->state == STREAM_STOPPED) {
    if (stream->usingCallback) Sleep(50); // sleep 50 milliseconds
    return;
  }
  else if (stream->usingCallback) {
    stopStream = stream->callback(stream->userBuffer, stream->bufferSize, stream->userData);    
  }

  MUTEX_LOCK(&stream->mutex);

  // The state might change while waiting on a mutex.
  if (stream->state == STREAM_STOPPED) {
    MUTEX_UNLOCK(&stream->mutex);
    if (stream->usingCallback && stopStream)
      this->stopStream(streamId);
  }

  HRESULT result;
  DWORD currentPos, safePos;
  LPVOID buffer1 = NULL;
  LPVOID buffer2 = NULL;
  DWORD bufferSize1 = 0;
  DWORD bufferSize2 = 0;
  char *buffer;
  long buffer_bytes;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {

    // Setup parameters and do buffer conversion if necessary.
    if (stream->doConvertBuffer[0]) {
      convertStreamBuffer(stream, PLAYBACK);
      buffer = stream->deviceBuffer;
      buffer_bytes = stream->bufferSize * stream->nDeviceChannels[0];
      buffer_bytes *= formatBytes(stream->deviceFormat[0]);
    }
    else {
      buffer = stream->userBuffer;
      buffer_bytes = stream->bufferSize * stream->nUserChannels[0];
      buffer_bytes *= formatBytes(stream->userFormat);
    }

    // No byte swapping necessary in DirectSound implementation.

    LPDIRECTSOUNDBUFFER dsBuffer = (LPDIRECTSOUNDBUFFER) stream->handle[0].buffer;
    UINT nextWritePos = stream->handle[0].bufferPointer;
    DWORD dsBufferSize = buffer_bytes * stream->nBuffers;

    // Find out where the read and "safe write" pointers are.
    result = dsBuffer->GetCurrentPosition(&currentPos, &safePos);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to get current DS position (%s): %s.",
              devices[stream->device[0]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    if ( currentPos < nextWritePos ) currentPos += dsBufferSize; // unwrap offset
    DWORD endWrite = nextWritePos + buffer_bytes;

    // Check whether the entire write region is behind the play pointer.
    while ( currentPos < endWrite ) {
      // If we are here, then we must wait until the play pointer gets
      // beyond the write region.  The approach here is to use the
      // Sleep() function to suspend operation until safePos catches
      // up. Calculate number of milliseconds to wait as:
      //   time = distance * (milliseconds/second) * fudgefactor /
      //          ((bytes/sample) * (samples/second))
      // A "fudgefactor" less than 1 is used because it was found
      // that sleeping too long was MUCH worse than sleeping for
      // several shorter periods.
      float millis = (endWrite - currentPos) * 900.0;
      millis /= ( formatBytes(stream->deviceFormat[0]) * stream->sampleRate);
      if ( millis < 1.0 ) millis = 1.0;
      Sleep( (DWORD) millis );

      // Wake up, find out where we are now
      result = dsBuffer->GetCurrentPosition( &currentPos, &safePos );
      if ( FAILED(result) ) {
        sprintf(message, "RtAudio: Unable to get current DS position (%s): %s.",
              devices[stream->device[0]].name, getErrorString(result));
        error(RtError::DRIVER_ERROR);
      }
      if ( currentPos < nextWritePos ) currentPos += dsBufferSize; // unwrap offset
    }

    // Lock free space in the buffer
    result = dsBuffer->Lock (nextWritePos, buffer_bytes, &buffer1,
                             &bufferSize1, &buffer2, &bufferSize2, 0);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to lock DS buffer during playback (%s): %s.",
              devices[stream->device[0]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    // Copy our buffer into the DS buffer
    CopyMemory(buffer1, buffer, bufferSize1);
    if (buffer2 != NULL) CopyMemory(buffer2, buffer+bufferSize1, bufferSize2);

    // Update our buffer offset and unlock sound buffer
    dsBuffer->Unlock (buffer1, bufferSize1, buffer2, bufferSize2);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to unlock DS buffer during playback (%s): %s.",
              devices[stream->device[0]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }
    nextWritePos = (nextWritePos + bufferSize1 + bufferSize2) % dsBufferSize;
    stream->handle[0].bufferPointer = nextWritePos;
  }

  if (stream->mode == RECORD || stream->mode == DUPLEX) {

    // Setup parameters.
    if (stream->doConvertBuffer[1]) {
      buffer = stream->deviceBuffer;
      buffer_bytes = stream->bufferSize * stream->nDeviceChannels[1];
      buffer_bytes *= formatBytes(stream->deviceFormat[1]);
    }
    else {
      buffer = stream->userBuffer;
      buffer_bytes = stream->bufferSize * stream->nUserChannels[1];
      buffer_bytes *= formatBytes(stream->userFormat);
    }

    LPDIRECTSOUNDCAPTUREBUFFER dsBuffer = (LPDIRECTSOUNDCAPTUREBUFFER) stream->handle[1].buffer;
    UINT nextReadPos = stream->handle[1].bufferPointer;
    DWORD dsBufferSize = buffer_bytes * stream->nBuffers;

    // Find out where the write and "safe read" pointers are.
    result = dsBuffer->GetCurrentPosition(&currentPos, &safePos);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to get current DS capture position (%s): %s.",
              devices[stream->device[1]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    if ( safePos < nextReadPos ) safePos += dsBufferSize; // unwrap offset
    DWORD endRead = nextReadPos + buffer_bytes;

    // Check whether the entire write region is behind the play pointer.
    while ( safePos < endRead ) {
      // See comments for playback.
      float millis = (endRead - safePos) * 900.0;
      millis /= ( formatBytes(stream->deviceFormat[1]) * stream->sampleRate);
      if ( millis < 1.0 ) millis = 1.0;
      Sleep( (DWORD) millis );

      // Wake up, find out where we are now
      result = dsBuffer->GetCurrentPosition( &currentPos, &safePos );
      if ( FAILED(result) ) {
        sprintf(message, "RtAudio: Unable to get current DS capture position (%s): %s.",
                devices[stream->device[1]].name, getErrorString(result));
        error(RtError::DRIVER_ERROR);
      }
      
      if ( safePos < nextReadPos ) safePos += dsBufferSize; // unwrap offset
    }

    // Lock free space in the buffer
    result = dsBuffer->Lock (nextReadPos, buffer_bytes, &buffer1,
                             &bufferSize1, &buffer2, &bufferSize2, 0);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to lock DS buffer during capture (%s): %s.",
              devices[stream->device[1]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }

    // Copy our buffer into the DS buffer
    CopyMemory(buffer, buffer1, bufferSize1);
    if (buffer2 != NULL) CopyMemory(buffer+bufferSize1, buffer2, bufferSize2);

    // Update our buffer offset and unlock sound buffer
    nextReadPos = (nextReadPos + bufferSize1 + bufferSize2) % dsBufferSize;
    dsBuffer->Unlock (buffer1, bufferSize1, buffer2, bufferSize2);
    if ( FAILED(result) ) {
      sprintf(message, "RtAudio: Unable to unlock DS buffer during capture (%s): %s.",
              devices[stream->device[1]].name, getErrorString(result));
      error(RtError::DRIVER_ERROR);
    }
    stream->handle[1].bufferPointer = nextReadPos;

    // No byte swapping necessary in DirectSound implementation.

    // Do buffer conversion if necessary.
    if (stream->doConvertBuffer[1])
      convertStreamBuffer(stream, RECORD);
  }

  MUTEX_UNLOCK(&stream->mutex);

  if (stream->usingCallback && stopStream)
    this->stopStream(streamId);
}

// Definitions for utility functions and callbacks
// specific to the DirectSound implementation.

extern "C" unsigned __stdcall callbackHandler(void *ptr)
{
  RtAudio *object = thread_info.object;
  int stream = thread_info.streamId;
  bool *usingCallback = (bool *) ptr;

  while ( *usingCallback ) {
    try {
      object->tickStream(stream);
    }
    catch (RtError &exception) {
      fprintf(stderr, "\nCallback thread error (%s) ... closing thread.\n\n",
              exception.getMessage());
      break;
    }
  }

  _endthreadex( 0 );
  return 0;
}

static bool CALLBACK deviceCountCallback(LPGUID lpguid,
                                         LPCSTR lpcstrDescription,
                                         LPCSTR lpcstrModule,
                                         LPVOID lpContext)
{
  int *pointer = ((int *) lpContext);
  (*pointer)++;

  return true;
}

static bool CALLBACK deviceInfoCallback(LPGUID lpguid,
                                        LPCSTR lpcstrDescription,
                                        LPCSTR lpcstrModule,
                                        LPVOID lpContext)
{
  enum_info *info = ((enum_info *) lpContext);
  while (strlen(info->name) > 0) info++;

  strncpy(info->name, lpcstrDescription, 64);
  info->id = lpguid;

	HRESULT    hr;
  info->isValid = false;
  if (info->isInput == true) {
    DSCCAPS               caps;
    LPDIRECTSOUNDCAPTURE  object;

    hr = DirectSoundCaptureCreate(  lpguid, &object,   NULL );
    if( hr != DS_OK ) return true;

    caps.dwSize = sizeof(caps);
    hr = object->GetCaps( &caps );
    if( hr == DS_OK ) {
      if (caps.dwChannels > 0 && caps.dwFormats > 0)
        info->isValid = true;
    }
    object->Release();
  }
  else {
    DSCAPS         caps;
    LPDIRECTSOUND  object;
    hr = DirectSoundCreate(  lpguid, &object,   NULL );
    if( hr != DS_OK ) return true;

    caps.dwSize = sizeof(caps);
    hr = object->GetCaps( &caps );
    if( hr == DS_OK ) {
      if ( caps.dwFlags & DSCAPS_PRIMARYMONO || caps.dwFlags & DSCAPS_PRIMARYSTEREO )
        info->isValid = true;
    }
    object->Release();
  }

  return true;
}

static char* getErrorString(int code)
{
	switch (code) {

  case DSERR_ALLOCATED:
    return "Direct Sound already allocated";

  case DSERR_CONTROLUNAVAIL:
    return "Direct Sound control unavailable";

  case DSERR_INVALIDPARAM:
    return "Direct Sound invalid parameter";

  case DSERR_INVALIDCALL:
    return "Direct Sound invalid call";

  case DSERR_GENERIC:
    return "Direct Sound generic error";

  case DSERR_PRIOLEVELNEEDED:
    return "Direct Sound Priority level needed";

  case DSERR_OUTOFMEMORY:
    return "Direct Sound out of memory";

  case DSERR_BADFORMAT:
    return "Direct Sound bad format";

  case DSERR_UNSUPPORTED:
    return "Direct Sound unsupported error";

  case DSERR_NODRIVER:
    return "Direct Sound no driver error";

  case DSERR_ALREADYINITIALIZED:
    return "Direct Sound already initialized";

  case DSERR_NOAGGREGATION:
    return "Direct Sound no aggregation";

  case DSERR_BUFFERLOST:
    return "Direct Sound buffer lost";

  case DSERR_OTHERAPPHASPRIO:
    return "Direct Sound other app has priority";

  case DSERR_UNINITIALIZED:
    return "Direct Sound uninitialized";

  default:
    return "Direct Sound unknown error";
	}
}

//******************** End of __WINDOWS_DS__ *********************//

#elif defined(__IRIX_AL__) // SGI's AL API for IRIX

#include <unistd.h>
#include <errno.h>

void RtAudio :: initialize(void)
{

  // Count cards and devices
  nDevices = 0;

  // Determine the total number of input and output devices.
  nDevices = alQueryValues(AL_SYSTEM, AL_DEVICES, 0, 0, 0, 0);
  if (nDevices < 0) {
    sprintf(message, "RtAudio: AL error counting devices: %s.",
            alGetErrorString(oserror()));
    error(RtError::DRIVER_ERROR);
  }

  if (nDevices <= 0) return;

  ALvalue *vls = (ALvalue *) new ALvalue[nDevices];

  // Add one for our default input/output devices.
  nDevices++;

  //  Allocate the RTAUDIO_DEVICE structures.
  devices = (RTAUDIO_DEVICE *) calloc(nDevices, sizeof(RTAUDIO_DEVICE));
  if (devices == NULL) {
    sprintf(message, "RtAudio: memory allocation error!");
    error(RtError::MEMORY_ERROR);
  }

  // Write device ascii identifiers to device info structure.
  char name[32];
  int outs, ins, i;
  ALpv pvs[1];
  pvs[0].param = AL_NAME;
  pvs[0].value.ptr = name;
  pvs[0].sizeIn = 32;

  strcpy(devices[0].name, "Default Input/Output Devices");

  outs = alQueryValues(AL_SYSTEM, AL_DEFAULT_OUTPUT, vls, nDevices-1, 0, 0);
  if (outs < 0) {
    sprintf(message, "RtAudio: AL error getting output devices: %s.",
            alGetErrorString(oserror()));
    error(RtError::DRIVER_ERROR);
  }

  for (i=0; i<outs; i++) {
    if (alGetParams(vls[i].i, pvs, 1) < 0) {
      sprintf(message, "RtAudio: AL error querying output devices: %s.",
              alGetErrorString(oserror()));
      error(RtError::DRIVER_ERROR);
    }
    strncpy(devices[i+1].name, name, 32);
    devices[i+1].id[0] = vls[i].i;
  }

  ins = alQueryValues(AL_SYSTEM, AL_DEFAULT_INPUT, &vls[outs], nDevices-outs-1, 0, 0);
  if (ins < 0) {
    sprintf(message, "RtAudio: AL error getting input devices: %s.",
            alGetErrorString(oserror()));
    error(RtError::DRIVER_ERROR);
  }

  for (i=outs; i<ins+outs; i++) {
    if (alGetParams(vls[i].i, pvs, 1) < 0) {
      sprintf(message, "RtAudio: AL error querying input devices: %s.",
              alGetErrorString(oserror()));
      error(RtError::DRIVER_ERROR);
    }
    strncpy(devices[i+1].name, name, 32);
    devices[i+1].id[1] = vls[i].i;
  }

  delete [] vls;

  return;
}

void RtAudio :: probeDeviceInfo(RTAUDIO_DEVICE *info)
{
  int resource, result, i;
  ALvalue value;
  ALparamInfo pinfo;

  // Get output resource ID if it exists.
  if ( !strncmp(info->name, "Default Input/Output Devices", 28) ) {
    result = alQueryValues(AL_SYSTEM, AL_DEFAULT_OUTPUT, &value, 1, 0, 0);
    if (result < 0) {
      sprintf(message, "RtAudio: AL error getting default output device id: %s.",
              alGetErrorString(oserror()));
      error(RtError::WARNING);
    }
    else
      resource = value.i;
  }
  else
    resource = info->id[0];

  if (resource > 0) {

    // Probe output device parameters.
    result = alQueryValues(resource, AL_CHANNELS, &value, 1, 0, 0);
    if (result < 0) {
      sprintf(message, "RtAudio: AL error getting device (%s) channels: %s.",
              info->name, alGetErrorString(oserror()));
      error(RtError::WARNING);
    }
    else {
      info->maxOutputChannels = value.i;
      info->minOutputChannels = 1;
    }

    result = alGetParamInfo(resource, AL_RATE, &pinfo);
    if (result < 0) {
      sprintf(message, "RtAudio: AL error getting device (%s) rates: %s.",
              info->name, alGetErrorString(oserror()));
      error(RtError::WARNING);
    }
    else {
      info->nSampleRates = 0;
      for (i=0; i<MAX_SAMPLE_RATES; i++) {
        if ( SAMPLE_RATES[i] >= pinfo.min.i && SAMPLE_RATES[i] <= pinfo.max.i ) {
          info->sampleRates[info->nSampleRates] = SAMPLE_RATES[i];
          info->nSampleRates++;
        }
      }
    }

    // The AL library supports all our formats, except 24-bit and 32-bit ints.
    info->nativeFormats = (RTAUDIO_FORMAT) 51;
  }

  // Now get input resource ID if it exists.
  if ( !strncmp(info->name, "Default Input/Output Devices", 28) ) {
    result = alQueryValues(AL_SYSTEM, AL_DEFAULT_INPUT, &value, 1, 0, 0);
    if (result < 0) {
      sprintf(message, "RtAudio: AL error getting default input device id: %s.",
              alGetErrorString(oserror()));
      error(RtError::WARNING);
    }
    else
      resource = value.i;
  }
  else
    resource = info->id[1];

  if (resource > 0) {

    // Probe input device parameters.
    result = alQueryValues(resource, AL_CHANNELS, &value, 1, 0, 0);
    if (result < 0) {
      sprintf(message, "RtAudio: AL error getting device (%s) channels: %s.",
              info->name, alGetErrorString(oserror()));
      error(RtError::WARNING);
    }
    else {
      info->maxInputChannels = value.i;
      info->minInputChannels = 1;
    }

    result = alGetParamInfo(resource, AL_RATE, &pinfo);
    if (result < 0) {
      sprintf(message, "RtAudio: AL error getting device (%s) rates: %s.",
              info->name, alGetErrorString(oserror()));
      error(RtError::WARNING);
    }
    else {
      // In the case of the default device, these values will
      // overwrite the rates determined for the output device.  Since
      // the input device is most likely to be more limited than the
      // output device, this is ok.
      info->nSampleRates = 0;
      for (i=0; i<MAX_SAMPLE_RATES; i++) {
        if ( SAMPLE_RATES[i] >= pinfo.min.i && SAMPLE_RATES[i] <= pinfo.max.i ) {
          info->sampleRates[info->nSampleRates] = SAMPLE_RATES[i];
          info->nSampleRates++;
        }
      }
    }

    // The AL library supports all our formats, except 24-bit and 32-bit ints.
    info->nativeFormats = (RTAUDIO_FORMAT) 51;
  }

  if ( info->maxInputChannels == 0 && info->maxOutputChannels == 0 )
    return;
  if ( info->nSampleRates == 0 )
    return;

  // Determine duplex status.
  if (info->maxInputChannels < info->maxOutputChannels)
    info->maxDuplexChannels = info->maxInputChannels;
  else
    info->maxDuplexChannels = info->maxOutputChannels;
  if (info->minInputChannels < info->minOutputChannels)
    info->minDuplexChannels = info->minInputChannels;
  else
    info->minDuplexChannels = info->minOutputChannels;

  if ( info->maxDuplexChannels > 0 ) info->hasDuplexSupport = true;
  else info->hasDuplexSupport = false;

  info->probed = true;

  return;
}

bool RtAudio :: probeDeviceOpen(int device, RTAUDIO_STREAM *stream,
                                STREAM_MODE mode, int channels, 
                                int sampleRate, RTAUDIO_FORMAT format,
                                int *bufferSize, int numberOfBuffers)
{
  int result, resource, nBuffers;
  ALconfig al_config;
  ALport port;
  ALpv pvs[2];

  // Get a new ALconfig structure.
  al_config = alNewConfig();
  if ( !al_config ) {
    sprintf(message,"RtAudio: can't get AL config: %s.",
            alGetErrorString(oserror()));
    error(RtError::WARNING);
    return FAILURE;
  }

  // Set the channels.
  result = alSetChannels(al_config, channels);
  if ( result < 0 ) {
    sprintf(message,"RtAudio: can't set %d channels in AL config: %s.",
            channels, alGetErrorString(oserror()));
    error(RtError::WARNING);
    return FAILURE;
  }

  // Set the queue (buffer) size.
  if ( numberOfBuffers < 1 )
    nBuffers = 1;
  else
    nBuffers = numberOfBuffers;
  long buffer_size = *bufferSize * nBuffers;
  result = alSetQueueSize(al_config, buffer_size); // in sample frames
  if ( result < 0 ) {
    sprintf(message,"RtAudio: can't set buffer size (%ld) in AL config: %s.",
            buffer_size, alGetErrorString(oserror()));
    error(RtError::WARNING);
    return FAILURE;
  }

  // Set the data format.
  stream->userFormat = format;
  stream->deviceFormat[mode] = format;
  if (format == RTAUDIO_SINT8) {
    result = alSetSampFmt(al_config, AL_SAMPFMT_TWOSCOMP);
    result = alSetWidth(al_config, AL_SAMPLE_8);
  }
  else if (format == RTAUDIO_SINT16) {
    result = alSetSampFmt(al_config, AL_SAMPFMT_TWOSCOMP);
    result = alSetWidth(al_config, AL_SAMPLE_16);
  }
  else if (format == RTAUDIO_SINT24) {
    // Our 24-bit format assumes the upper 3 bytes of a 4 byte word.
    // The AL library uses the lower 3 bytes, so we'll need to do our
    // own conversion.
    result = alSetSampFmt(al_config, AL_SAMPFMT_FLOAT);
    stream->deviceFormat[mode] = RTAUDIO_FLOAT32;
  }
  else if (format == RTAUDIO_SINT32) {
    // The AL library doesn't seem to support the 32-bit integer
    // format, so we'll need to do our own conversion.
    result = alSetSampFmt(al_config, AL_SAMPFMT_FLOAT);
    stream->deviceFormat[mode] = RTAUDIO_FLOAT32;
  }
  else if (format == RTAUDIO_FLOAT32)
    result = alSetSampFmt(al_config, AL_SAMPFMT_FLOAT);
  else if (format == RTAUDIO_FLOAT64)
    result = alSetSampFmt(al_config, AL_SAMPFMT_DOUBLE);

  if ( result == -1 ) {
    sprintf(message,"RtAudio: AL error setting sample format in AL config: %s.",
            alGetErrorString(oserror()));
    error(RtError::WARNING);
    return FAILURE;
  }

  if (mode == PLAYBACK) {

    // Set our device.
    if (device == 0)
      resource = AL_DEFAULT_OUTPUT;
    else
      resource = devices[device].id[0];
    result = alSetDevice(al_config, resource);
    if ( result == -1 ) {
      sprintf(message,"RtAudio: AL error setting device (%s) in AL config: %s.",
              devices[device].name, alGetErrorString(oserror()));
      error(RtError::WARNING);
      return FAILURE;
    }

    // Open the port.
    port = alOpenPort("RtAudio Output Port", "w", al_config);
    if( !port ) {
      sprintf(message,"RtAudio: AL error opening output port: %s.",
              alGetErrorString(oserror()));
      error(RtError::WARNING);
      return FAILURE;
    }

    // Set the sample rate
    pvs[0].param = AL_MASTER_CLOCK;
    pvs[0].value.i = AL_CRYSTAL_MCLK_TYPE;
    pvs[1].param = AL_RATE;
    pvs[1].value.ll = alDoubleToFixed((double)sampleRate);
    result = alSetParams(resource, pvs, 2);
    if ( result < 0 ) {
      alClosePort(port);
      sprintf(message,"RtAudio: AL error setting sample rate (%d) for device (%s): %s.",
              sampleRate, devices[device].name, alGetErrorString(oserror()));
      error(RtError::WARNING);
      return FAILURE;
    }
  }
  else { // mode == RECORD

    // Set our device.
    if (device == 0)
      resource = AL_DEFAULT_INPUT;
    else
      resource = devices[device].id[1];
    result = alSetDevice(al_config, resource);
    if ( result == -1 ) {
      sprintf(message,"RtAudio: AL error setting device (%s) in AL config: %s.",
              devices[device].name, alGetErrorString(oserror()));
      error(RtError::WARNING);
      return FAILURE;
    }

    // Open the port.
    port = alOpenPort("RtAudio Output Port", "r", al_config);
    if( !port ) {
      sprintf(message,"RtAudio: AL error opening input port: %s.",
              alGetErrorString(oserror()));
      error(RtError::WARNING);
      return FAILURE;
    }

    // Set the sample rate
    pvs[0].param = AL_MASTER_CLOCK;
    pvs[0].value.i = AL_CRYSTAL_MCLK_TYPE;
    pvs[1].param = AL_RATE;
    pvs[1].value.ll = alDoubleToFixed((double)sampleRate);
    result = alSetParams(resource, pvs, 2);
    if ( result < 0 ) {
      alClosePort(port);
      sprintf(message,"RtAudio: AL error setting sample rate (%d) for device (%s): %s.",
              sampleRate, devices[device].name, alGetErrorString(oserror()));
      error(RtError::WARNING);
      return FAILURE;
    }
  }

  alFreeConfig(al_config);

  stream->nUserChannels[mode] = channels;
  stream->nDeviceChannels[mode] = channels;

  // Set handle and flags for buffer conversion
  stream->handle[mode] = port;
  stream->doConvertBuffer[mode] = false;
  if (stream->userFormat != stream->deviceFormat[mode])
    stream->doConvertBuffer[mode] = true;

  // Allocate necessary internal buffers
  if ( stream->nUserChannels[0] != stream->nUserChannels[1] ) {

    long buffer_bytes;
    if (stream->nUserChannels[0] >= stream->nUserChannels[1])
      buffer_bytes = stream->nUserChannels[0];
    else
      buffer_bytes = stream->nUserChannels[1];

    buffer_bytes *= *bufferSize * formatBytes(stream->userFormat);
    if (stream->userBuffer) free(stream->userBuffer);
    stream->userBuffer = (char *) calloc(buffer_bytes, 1);
    if (stream->userBuffer == NULL)
      goto memory_error;
  }

  if ( stream->doConvertBuffer[mode] ) {

    long buffer_bytes;
    bool makeBuffer = true;
    if ( mode == PLAYBACK )
      buffer_bytes = stream->nDeviceChannels[0] * formatBytes(stream->deviceFormat[0]);
    else { // mode == RECORD
      buffer_bytes = stream->nDeviceChannels[1] * formatBytes(stream->deviceFormat[1]);
      if ( stream->mode == PLAYBACK ) {
        long bytes_out = stream->nDeviceChannels[0] * formatBytes(stream->deviceFormat[0]);
        if ( buffer_bytes > bytes_out )
          buffer_bytes = (buffer_bytes > bytes_out) ? buffer_bytes : bytes_out;
        else
          makeBuffer = false;
      }
    }

    if ( makeBuffer ) {
      buffer_bytes *= *bufferSize;
      if (stream->deviceBuffer) free(stream->deviceBuffer);
      stream->deviceBuffer = (char *) calloc(buffer_bytes, 1);
      if (stream->deviceBuffer == NULL)
        goto memory_error;
    }
  }

  stream->device[mode] = device;
  stream->state = STREAM_STOPPED;
  if ( stream->mode == PLAYBACK && mode == RECORD )
    // We had already set up an output stream.
    stream->mode = DUPLEX;
  else
    stream->mode = mode;
  stream->nBuffers = nBuffers;
  stream->bufferSize = *bufferSize;
  stream->sampleRate = sampleRate;

  return SUCCESS;

 memory_error:
  if (stream->handle[0]) {
    alClosePort(stream->handle[0]);
    stream->handle[0] = 0;
  }
  if (stream->handle[1]) {
    alClosePort(stream->handle[1]);
    stream->handle[1] = 0;
  }
  if (stream->userBuffer) {
    free(stream->userBuffer);
    stream->userBuffer = 0;
  }
  sprintf(message, "RtAudio: ALSA error allocating buffer memory for device (%s).",
          devices[device].name);
  error(RtError::WARNING);
  return FAILURE;
}

void RtAudio :: cancelStreamCallback(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  if (stream->usingCallback) {
    stream->usingCallback = false;
    pthread_cancel(stream->thread);
    pthread_join(stream->thread, NULL);
    stream->thread = 0;
    stream->callback = NULL;
    stream->userData = NULL;
  }
}

void RtAudio :: closeStream(int streamId)
{
  // We don't want an exception to be thrown here because this
  // function is called by our class destructor.  So, do our own
  // streamId check.
  if ( streams.find( streamId ) == streams.end() ) {
    sprintf(message, "RtAudio: invalid stream identifier!");
    error(RtError::WARNING);
    return;
  }

  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) streams[streamId];

  if (stream->usingCallback) {
    pthread_cancel(stream->thread);
    pthread_join(stream->thread, NULL);
  }

  pthread_mutex_destroy(&stream->mutex);

  if (stream->handle[0])
    alClosePort(stream->handle[0]);

  if (stream->handle[1])
    alClosePort(stream->handle[1]);

  if (stream->userBuffer)
    free(stream->userBuffer);

  if (stream->deviceBuffer)
    free(stream->deviceBuffer);

  free(stream);
  streams.erase(streamId);
}

void RtAudio :: startStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  if (stream->state == STREAM_RUNNING)
    return;

  // The AL port is ready as soon as it is opened.
  stream->state = STREAM_RUNNING;
}

void RtAudio :: stopStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  if (stream->state == STREAM_STOPPED)
    goto unlock;

  int result;
  int buffer_size = stream->bufferSize * stream->nBuffers;

  if (stream->mode == PLAYBACK || stream->mode == DUPLEX)
    alZeroFrames(stream->handle[0], buffer_size);

  if (stream->mode == RECORD || stream->mode == DUPLEX) {
    result = alDiscardFrames(stream->handle[1], buffer_size);
    if (result == -1) {
      sprintf(message, "RtAudio: AL error draining stream device (%s): %s.",
              devices[stream->device[1]].name, alGetErrorString(oserror()));
      error(RtError::DRIVER_ERROR);
    }
  }
  stream->state = STREAM_STOPPED;

 unlock:
  MUTEX_UNLOCK(&stream->mutex);
}

void RtAudio :: abortStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  if (stream->state == STREAM_STOPPED)
    goto unlock;

  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {

    int buffer_size = stream->bufferSize * stream->nBuffers;
    int result = alDiscardFrames(stream->handle[0], buffer_size);
    if (result == -1) {
      sprintf(message, "RtAudio: AL error aborting stream device (%s): %s.",
              devices[stream->device[0]].name, alGetErrorString(oserror()));
      error(RtError::DRIVER_ERROR);
    }
  }

  // There is no clear action to take on the input stream, since the
  // port will continue to run in any event.
  stream->state = STREAM_STOPPED;

 unlock:
  MUTEX_UNLOCK(&stream->mutex);
}

int RtAudio :: streamWillBlock(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  MUTEX_LOCK(&stream->mutex);

  int frames = 0;
  if (stream->state == STREAM_STOPPED)
    goto unlock;

  int err = 0;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {
    err = alGetFillable(stream->handle[0]);
    if (err < 0) {
      sprintf(message, "RtAudio: AL error getting available frames for stream (%s): %s.",
              devices[stream->device[0]].name, alGetErrorString(oserror()));
      error(RtError::DRIVER_ERROR);
    }
  }

  frames = err;

  if (stream->mode == RECORD || stream->mode == DUPLEX) {
    err = alGetFilled(stream->handle[1]);
    if (err < 0) {
      sprintf(message, "RtAudio: AL error getting available frames for stream (%s): %s.",
              devices[stream->device[1]].name, alGetErrorString(oserror()));
      error(RtError::DRIVER_ERROR);
    }
    if (frames > err) frames = err;
  }

  frames = stream->bufferSize - frames;
  if (frames < 0) frames = 0;

 unlock:
  MUTEX_UNLOCK(&stream->mutex);
  return frames;
}

void RtAudio :: tickStream(int streamId)
{
  RTAUDIO_STREAM *stream = (RTAUDIO_STREAM *) verifyStream(streamId);

  int stopStream = 0;
  if (stream->state == STREAM_STOPPED) {
    if (stream->usingCallback) usleep(50000); // sleep 50 milliseconds
    return;
  }
  else if (stream->usingCallback) {
    stopStream = stream->callback(stream->userBuffer, stream->bufferSize, stream->userData);    
  }

  MUTEX_LOCK(&stream->mutex);

  // The state might change while waiting on a mutex.
  if (stream->state == STREAM_STOPPED)
    goto unlock;

  char *buffer;
  int channels;
  RTAUDIO_FORMAT format;
  if (stream->mode == PLAYBACK || stream->mode == DUPLEX) {

    // Setup parameters and do buffer conversion if necessary.
    if (stream->doConvertBuffer[0]) {
      convertStreamBuffer(stream, PLAYBACK);
      buffer = stream->deviceBuffer;
      channels = stream->nDeviceChannels[0];
      format = stream->deviceFormat[0];
    }
    else {
      buffer = stream->userBuffer;
      channels = stream->nUserChannels[0];
      format = stream->userFormat;
    }

    // Do byte swapping if necessary.
    if (stream->doByteSwap[0])
      byteSwapBuffer(buffer, stream->bufferSize * channels, format);

    // Write interleaved samples to device.
    alWriteFrames(stream->handle[0], buffer, stream->bufferSize);
  }

  if (stream->mode == RECORD || stream->mode == DUPLEX) {

    // Setup parameters.
    if (stream->doConvertBuffer[1]) {
      buffer = stream->deviceBuffer;
      channels = stream->nDeviceChannels[1];
      format = stream->deviceFormat[1];
    }
    else {
      buffer = stream->userBuffer;
      channels = stream->nUserChannels[1];
      format = stream->userFormat;
    }

    // Read interleaved samples from device.
    alReadFrames(stream->handle[1], buffer, stream->bufferSize);

    // Do byte swapping if necessary.
    if (stream->doByteSwap[1])
      byteSwapBuffer(buffer, stream->bufferSize * channels, format);

    // Do buffer conversion if necessary.
    if (stream->doConvertBuffer[1])
      convertStreamBuffer(stream, RECORD);
  }

 unlock:
  MUTEX_UNLOCK(&stream->mutex);

  if (stream->usingCallback && stopStream)
    this->stopStream(streamId);
}

extern "C" void *callbackHandler(void *ptr)
{
  RtAudio *object = thread_info.object;
  int stream = thread_info.streamId;
  bool *usingCallback = (bool *) ptr;

  while ( *usingCallback ) {
    pthread_testcancel();
    try {
      object->tickStream(stream);
    }
    catch (RtError &exception) {
      fprintf(stderr, "\nCallback thread error (%s) ... closing thread.\n\n",
              exception.getMessage());
      break;
    }
  }

  return 0;
}

//******************** End of __IRIX_AL__ *********************//

#endif


// *************************************************** //
//
// Private common (OS-independent) RtAudio methods.
//
// *************************************************** //

// This method can be modified to control the behavior of error
// message reporting and throwing.
void RtAudio :: error(RtError::TYPE type)
{
  if (type == RtError::WARNING) {
#if defined(RTAUDIO_DEBUG)
    fprintf(stderr, "\n%s\n\n", message);
  else if (type == RtError::DEBUG_WARNING) {
    fprintf(stderr, "\n%s\n\n", message);
#endif
  }
  else {
    fprintf(stderr, "\n%s\n\n", message);
    throw RtError(message, type);
  }
}

void *RtAudio :: verifyStream(int streamId)
{
  // Verify the stream key.
  if ( streams.find( streamId ) == streams.end() ) {
    sprintf(message, "RtAudio: invalid stream identifier!");
    error(RtError::INVALID_STREAM);
  }

  return streams[streamId];
}

void RtAudio :: clearDeviceInfo(RTAUDIO_DEVICE *info)
{
  // Don't clear the name or DEVICE_ID fields here ... they are
  // typically set prior to a call of this function.
  info->probed = false;
  info->maxOutputChannels = 0;
  info->maxInputChannels = 0;
  info->maxDuplexChannels = 0;
  info->minOutputChannels = 0;
  info->minInputChannels = 0;
  info->minDuplexChannels = 0;
  info->hasDuplexSupport = false;
  info->nSampleRates = 0;
  for (int i=0; i<MAX_SAMPLE_RATES; i++)
    info->sampleRates[i] = 0;
  info->nativeFormats = 0;
}

int RtAudio :: formatBytes(RTAUDIO_FORMAT format)
{
  if (format == RTAUDIO_SINT16)
    return 2;
  else if (format == RTAUDIO_SINT24 || format == RTAUDIO_SINT32 ||
           format == RTAUDIO_FLOAT32)
    return 4;
  else if (format == RTAUDIO_FLOAT64)
    return 8;
  else if (format == RTAUDIO_SINT8)
    return 1;

  sprintf(message,"RtAudio: undefined format in formatBytes().");
  error(RtError::WARNING);

  return 0;
}

void RtAudio :: convertStreamBuffer(RTAUDIO_STREAM *stream, STREAM_MODE mode)
{
  // This method does format conversion, input/output channel compensation, and
  // data interleaving/deinterleaving.  24-bit integers are assumed to occupy
  // the upper three bytes of a 32-bit integer.

  int j, channels_in, channels_out, channels;
  RTAUDIO_FORMAT format_in, format_out;
  char *input, *output;

  if (mode == RECORD) { // convert device to user buffer
    input = stream->deviceBuffer;
    output = stream->userBuffer;
    channels_in = stream->nDeviceChannels[1];
    channels_out = stream->nUserChannels[1];
    format_in = stream->deviceFormat[1];
    format_out = stream->userFormat;
  }
  else { // convert user to device buffer
    input = stream->userBuffer;
    output = stream->deviceBuffer;
    channels_in = stream->nUserChannels[0];
    channels_out = stream->nDeviceChannels[0];
    format_in = stream->userFormat;
    format_out = stream->deviceFormat[0];

    // clear our device buffer when in/out duplex device channels are different
    if ( stream->mode == DUPLEX &&
         stream->nDeviceChannels[0] != stream->nDeviceChannels[1] )
      memset(output, 0, stream->bufferSize * channels_out * formatBytes(format_out));
  }

  channels = (channels_in < channels_out) ? channels_in : channels_out;

  // Set up the interleave/deinterleave offsets
  std::vector<int> offset_in(channels);
  std::vector<int> offset_out(channels);
  if (mode == RECORD && stream->deInterleave[1]) {
    for (int k=0; k<channels; k++) {
      offset_in[k] = k * stream->bufferSize;
      offset_out[k] = k;
    }
  }
  else if (mode == PLAYBACK && stream->deInterleave[0]) {
    for (int k=0; k<channels; k++) {
      offset_in[k] = k;
      offset_out[k] = k * stream->bufferSize;
    }
  }
  else {
    for (int k=0; k<channels; k++) {
      offset_in[k] = k;
      offset_out[k] = k;
    }
  }

  if (format_out == RTAUDIO_FLOAT64) {
    FLOAT64 scale;
    FLOAT64 *out = (FLOAT64 *)output;

    if (format_in == RTAUDIO_SINT8) {
      signed char *in = (signed char *)input;
      scale = 1.0 / 128.0;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (FLOAT64) in[offset_in[j]];
          out[offset_out[j]] *= scale;
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT16) {
      INT16 *in = (INT16 *)input;
      scale = 1.0 / 32768.0;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (FLOAT64) in[offset_in[j]];
          out[offset_out[j]] *= scale;
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT24) {
      INT32 *in = (INT32 *)input;
      scale = 1.0 / 2147483648.0;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (FLOAT64) (in[offset_in[j]] & 0xffffff00);
          out[offset_out[j]] *= scale;
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT32) {
      INT32 *in = (INT32 *)input;
      scale = 1.0 / 2147483648.0;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (FLOAT64) in[offset_in[j]];
          out[offset_out[j]] *= scale;
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_FLOAT32) {
      FLOAT32 *in = (FLOAT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (FLOAT64) in[offset_in[j]];
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_FLOAT64) {
      // Channel compensation and/or (de)interleaving only.
      FLOAT64 *in = (FLOAT64 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = in[offset_in[j]];
        }
        in += channels_in;
        out += channels_out;
      }
    }
  }
  else if (format_out == RTAUDIO_FLOAT32) {
    FLOAT32 scale;
    FLOAT32 *out = (FLOAT32 *)output;

    if (format_in == RTAUDIO_SINT8) {
      signed char *in = (signed char *)input;
      scale = 1.0 / 128.0;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (FLOAT32) in[offset_in[j]];
          out[offset_out[j]] *= scale;
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT16) {
      INT16 *in = (INT16 *)input;
      scale = 1.0 / 32768.0;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (FLOAT32) in[offset_in[j]];
          out[offset_out[j]] *= scale;
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT24) {
      INT32 *in = (INT32 *)input;
      scale = 1.0 / 2147483648.0;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (FLOAT32) (in[offset_in[j]] & 0xffffff00);
          out[offset_out[j]] *= scale;
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT32) {
      INT32 *in = (INT32 *)input;
      scale = 1.0 / 2147483648.0;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (FLOAT32) in[offset_in[j]];
          out[offset_out[j]] *= scale;
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_FLOAT32) {
      // Channel compensation and/or (de)interleaving only.
      FLOAT32 *in = (FLOAT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = in[offset_in[j]];
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_FLOAT64) {
      FLOAT64 *in = (FLOAT64 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (FLOAT32) in[offset_in[j]];
        }
        in += channels_in;
        out += channels_out;
      }
    }
  }
  else if (format_out == RTAUDIO_SINT32) {
    INT32 *out = (INT32 *)output;
    if (format_in == RTAUDIO_SINT8) {
      signed char *in = (signed char *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT32) in[offset_in[j]];
          out[offset_out[j]] <<= 24;
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT16) {
      INT16 *in = (INT16 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT32) in[offset_in[j]];
          out[offset_out[j]] <<= 16;
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT24) {
      INT32 *in = (INT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT32) in[offset_in[j]];
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT32) {
      // Channel compensation and/or (de)interleaving only.
      INT32 *in = (INT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = in[offset_in[j]];
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_FLOAT32) {
      FLOAT32 *in = (FLOAT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT32) (in[offset_in[j]] * 2147483647.0);
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_FLOAT64) {
      FLOAT64 *in = (FLOAT64 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT32) (in[offset_in[j]] * 2147483647.0);
        }
        in += channels_in;
        out += channels_out;
      }
    }
  }
  else if (format_out == RTAUDIO_SINT24) {
    INT32 *out = (INT32 *)output;
    if (format_in == RTAUDIO_SINT8) {
      signed char *in = (signed char *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT32) in[offset_in[j]];
          out[offset_out[j]] <<= 24;
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT16) {
      INT16 *in = (INT16 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT32) in[offset_in[j]];
          out[offset_out[j]] <<= 16;
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT24) {
      // Channel compensation and/or (de)interleaving only.
      INT32 *in = (INT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = in[offset_in[j]];
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT32) {
      INT32 *in = (INT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT32) (in[offset_in[j]] & 0xffffff00);
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_FLOAT32) {
      FLOAT32 *in = (FLOAT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT32) (in[offset_in[j]] * 2147483647.0);
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_FLOAT64) {
      FLOAT64 *in = (FLOAT64 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT32) (in[offset_in[j]] * 2147483647.0);
        }
        in += channels_in;
        out += channels_out;
      }
    }
  }
  else if (format_out == RTAUDIO_SINT16) {
    INT16 *out = (INT16 *)output;
    if (format_in == RTAUDIO_SINT8) {
      signed char *in = (signed char *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT16) in[offset_in[j]];
          out[offset_out[j]] <<= 8;
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT16) {
      // Channel compensation and/or (de)interleaving only.
      INT16 *in = (INT16 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = in[offset_in[j]];
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT24) {
      INT32 *in = (INT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT16) ((in[offset_in[j]] >> 16) & 0x0000ffff);
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT32) {
      INT32 *in = (INT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT16) ((in[offset_in[j]] >> 16) & 0x0000ffff);
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_FLOAT32) {
      FLOAT32 *in = (FLOAT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT16) (in[offset_in[j]] * 32767.0);
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_FLOAT64) {
      FLOAT64 *in = (FLOAT64 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (INT16) (in[offset_in[j]] * 32767.0);
        }
        in += channels_in;
        out += channels_out;
      }
    }
  }
  else if (format_out == RTAUDIO_SINT8) {
    signed char *out = (signed char *)output;
    if (format_in == RTAUDIO_SINT8) {
      // Channel compensation and/or (de)interleaving only.
      signed char *in = (signed char *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = in[offset_in[j]];
        }
        in += channels_in;
        out += channels_out;
      }
    }
    if (format_in == RTAUDIO_SINT16) {
      INT16 *in = (INT16 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (signed char) ((in[offset_in[j]] >> 8) & 0x00ff);
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT24) {
      INT32 *in = (INT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (signed char) ((in[offset_in[j]] >> 24) & 0x000000ff);
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_SINT32) {
      INT32 *in = (INT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (signed char) ((in[offset_in[j]] >> 24) & 0x000000ff);
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_FLOAT32) {
      FLOAT32 *in = (FLOAT32 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (signed char) (in[offset_in[j]] * 127.0);
        }
        in += channels_in;
        out += channels_out;
      }
    }
    else if (format_in == RTAUDIO_FLOAT64) {
      FLOAT64 *in = (FLOAT64 *)input;
      for (int i=0; i<stream->bufferSize; i++) {
        for (j=0; j<channels; j++) {
          out[offset_out[j]] = (signed char) (in[offset_in[j]] * 127.0);
        }
        in += channels_in;
        out += channels_out;
      }
    }
  }
}

void RtAudio :: byteSwapBuffer(char *buffer, int samples, RTAUDIO_FORMAT format)
{
  register char val;
  register char *ptr;

  ptr = buffer;
  if (format == RTAUDIO_SINT16) {
    for (int i=0; i<samples; i++) {
      // Swap 1st and 2nd bytes.
      val = *(ptr);
      *(ptr) = *(ptr+1);
      *(ptr+1) = val;

      // Increment 2 bytes.
      ptr += 2;
    }
  }
  else if (format == RTAUDIO_SINT24 ||
           format == RTAUDIO_SINT32 ||
           format == RTAUDIO_FLOAT32) {
    for (int i=0; i<samples; i++) {
      // Swap 1st and 4th bytes.
      val = *(ptr);
      *(ptr) = *(ptr+3);
      *(ptr+3) = val;

      // Swap 2nd and 3rd bytes.
      ptr += 1;
      val = *(ptr);
      *(ptr) = *(ptr+1);
      *(ptr+1) = val;

      // Increment 4 bytes.
      ptr += 4;
    }
  }
  else if (format == RTAUDIO_FLOAT64) {
    for (int i=0; i<samples; i++) {
      // Swap 1st and 8th bytes
      val = *(ptr);
      *(ptr) = *(ptr+7);
      *(ptr+7) = val;

      // Swap 2nd and 7th bytes
      ptr += 1;
      val = *(ptr);
      *(ptr) = *(ptr+5);
      *(ptr+5) = val;

      // Swap 3rd and 6th bytes
      ptr += 1;
      val = *(ptr);
      *(ptr) = *(ptr+3);
      *(ptr+3) = val;

      // Swap 4th and 5th bytes
      ptr += 1;
      val = *(ptr);
      *(ptr) = *(ptr+1);
      *(ptr+1) = val;

      // Increment 8 bytes.
      ptr += 8;
    }
  }
}


// *************************************************** //
//
// RtError class definition.
//
// *************************************************** //

RtError :: RtError(const char *p, TYPE tipe)
{
  type = tipe;
  strncpy(error_message, p, 256);
}

RtError :: ~RtError()
{
}

void RtError :: printMessage()
{
  printf("\n%s\n\n", error_message);
}
