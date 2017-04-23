#if defined(__WINDOWS_WASAPI__) // Windows WASAPI API
#include "RtAudio.h"

// Authored by Marcus Tomlinson <themarcustomlinson@gmail.com>, April 2014
// - Introduces support for the Windows WASAPI API
// - Aims to deliver bit streams to and from hardware at the lowest possible latency, via the absolute minimum buffer sizes required
// - Provides flexible stream configuration to an otherwise strict and inflexible WASAPI interface
// - Includes automatic internal conversion of sample rate and buffer size between hardware and the user

#ifndef INITGUID
  #define INITGUID
#endif
#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

//=============================================================================

#define SAFE_RELEASE( objectPtr )\
if ( objectPtr )\
{\
  objectPtr->Release();\
  objectPtr = NULL;\
}

typedef HANDLE ( __stdcall *TAvSetMmThreadCharacteristicsPtr )( LPCWSTR TaskName, LPDWORD TaskIndex );

//-----------------------------------------------------------------------------

// WASAPI dictates stream sample rate, format, channel count, and in some cases, buffer size.
// Therefore we must perform all necessary conversions to user buffers in order to satisfy these
// requirements. WasapiBuffer ring buffers are used between HwIn->UserIn and UserOut->HwOut to
// provide intermediate storage for read / write synchronization.
class WasapiBuffer
{
public:
  WasapiBuffer()
    : buffer_( NULL ),
      bufferSize_( 0 ),
      inIndex_( 0 ),
      outIndex_( 0 ) {}

  ~WasapiBuffer() {
    free( buffer_ );
  }

  // sets the length of the internal ring buffer
  void setBufferSize( unsigned int bufferSize, unsigned int formatBytes ) {
    free( buffer_ );

    buffer_ = ( char* ) calloc( bufferSize, formatBytes );

    bufferSize_ = bufferSize;
    inIndex_ = 0;
    outIndex_ = 0;
  }

  // attempt to push a buffer into the ring buffer at the current "in" index
  bool pushBuffer( char* buffer, unsigned int bufferSize, RtAudioFormat format )
  {
    if ( !buffer ||                 // incoming buffer is NULL
         bufferSize == 0 ||         // incoming buffer has no data
         bufferSize > bufferSize_ ) // incoming buffer too large
    {
      return false;
    }

    unsigned int relOutIndex = outIndex_;
    unsigned int inIndexEnd = inIndex_ + bufferSize;
    if ( relOutIndex < inIndex_ && inIndexEnd >= bufferSize_ ) {
      relOutIndex += bufferSize_;
    }

    // "in" index can end on the "out" index but cannot begin at it
    if ( inIndex_ <= relOutIndex && inIndexEnd > relOutIndex ) {
      return false; // not enough space between "in" index and "out" index
    }

    // copy buffer from external to internal
    int fromZeroSize = inIndex_ + bufferSize - bufferSize_;
    fromZeroSize = fromZeroSize < 0 ? 0 : fromZeroSize;
    int fromInSize = bufferSize - fromZeroSize;

    switch( format )
      {
      case RTAUDIO_SINT8:
        memcpy( &( ( char* ) buffer_ )[inIndex_], buffer, fromInSize * sizeof( char ) );
        memcpy( buffer_, &( ( char* ) buffer )[fromInSize], fromZeroSize * sizeof( char ) );
        break;
      case RTAUDIO_SINT16:
        memcpy( &( ( short* ) buffer_ )[inIndex_], buffer, fromInSize * sizeof( short ) );
        memcpy( buffer_, &( ( short* ) buffer )[fromInSize], fromZeroSize * sizeof( short ) );
        break;
      case RTAUDIO_SINT24:
        memcpy( &( ( S24* ) buffer_ )[inIndex_], buffer, fromInSize * sizeof( S24 ) );
        memcpy( buffer_, &( ( S24* ) buffer )[fromInSize], fromZeroSize * sizeof( S24 ) );
        break;
      case RTAUDIO_SINT32:
        memcpy( &( ( int* ) buffer_ )[inIndex_], buffer, fromInSize * sizeof( int ) );
        memcpy( buffer_, &( ( int* ) buffer )[fromInSize], fromZeroSize * sizeof( int ) );
        break;
      case RTAUDIO_FLOAT32:
        memcpy( &( ( float* ) buffer_ )[inIndex_], buffer, fromInSize * sizeof( float ) );
        memcpy( buffer_, &( ( float* ) buffer )[fromInSize], fromZeroSize * sizeof( float ) );
        break;
      case RTAUDIO_FLOAT64:
        memcpy( &( ( double* ) buffer_ )[inIndex_], buffer, fromInSize * sizeof( double ) );
        memcpy( buffer_, &( ( double* ) buffer )[fromInSize], fromZeroSize * sizeof( double ) );
        break;
    }

    // update "in" index
    inIndex_ += bufferSize;
    inIndex_ %= bufferSize_;

    return true;
  }

  // attempt to pull a buffer from the ring buffer from the current "out" index
  bool pullBuffer( char* buffer, unsigned int bufferSize, RtAudioFormat format )
  {
    if ( !buffer ||                 // incoming buffer is NULL
         bufferSize == 0 ||         // incoming buffer has no data
         bufferSize > bufferSize_ ) // incoming buffer too large
    {
      return false;
    }

    unsigned int relInIndex = inIndex_;
    unsigned int outIndexEnd = outIndex_ + bufferSize;
    if ( relInIndex < outIndex_ && outIndexEnd >= bufferSize_ ) {
      relInIndex += bufferSize_;
    }

    // "out" index can begin at and end on the "in" index
    if ( outIndex_ < relInIndex && outIndexEnd > relInIndex ) {
      return false; // not enough space between "out" index and "in" index
    }

    // copy buffer from internal to external
    int fromZeroSize = outIndex_ + bufferSize - bufferSize_;
    fromZeroSize = fromZeroSize < 0 ? 0 : fromZeroSize;
    int fromOutSize = bufferSize - fromZeroSize;

    switch( format )
    {
      case RTAUDIO_SINT8:
        memcpy( buffer, &( ( char* ) buffer_ )[outIndex_], fromOutSize * sizeof( char ) );
        memcpy( &( ( char* ) buffer )[fromOutSize], buffer_, fromZeroSize * sizeof( char ) );
        break;
      case RTAUDIO_SINT16:
        memcpy( buffer, &( ( short* ) buffer_ )[outIndex_], fromOutSize * sizeof( short ) );
        memcpy( &( ( short* ) buffer )[fromOutSize], buffer_, fromZeroSize * sizeof( short ) );
        break;
      case RTAUDIO_SINT24:
        memcpy( buffer, &( ( S24* ) buffer_ )[outIndex_], fromOutSize * sizeof( S24 ) );
        memcpy( &( ( S24* ) buffer )[fromOutSize], buffer_, fromZeroSize * sizeof( S24 ) );
        break;
      case RTAUDIO_SINT32:
        memcpy( buffer, &( ( int* ) buffer_ )[outIndex_], fromOutSize * sizeof( int ) );
        memcpy( &( ( int* ) buffer )[fromOutSize], buffer_, fromZeroSize * sizeof( int ) );
        break;
      case RTAUDIO_FLOAT32:
        memcpy( buffer, &( ( float* ) buffer_ )[outIndex_], fromOutSize * sizeof( float ) );
        memcpy( &( ( float* ) buffer )[fromOutSize], buffer_, fromZeroSize * sizeof( float ) );
        break;
      case RTAUDIO_FLOAT64:
        memcpy( buffer, &( ( double* ) buffer_ )[outIndex_], fromOutSize * sizeof( double ) );
        memcpy( &( ( double* ) buffer )[fromOutSize], buffer_, fromZeroSize * sizeof( double ) );
        break;
    }

    // update "out" index
    outIndex_ += bufferSize;
    outIndex_ %= bufferSize_;

    return true;
  }

private:
  char* buffer_;
  unsigned int bufferSize_;
  unsigned int inIndex_;
  unsigned int outIndex_;
};

//-----------------------------------------------------------------------------

// In order to satisfy WASAPI's buffer requirements, we need a means of converting sample rate
// between HW and the user. The convertBufferWasapi function is used to perform this conversion
// between HwIn->UserIn and UserOut->HwOut during the stream callback loop.
// This sample rate converter favors speed over quality, and works best with conversions between
// one rate and its multiple.
void convertBufferWasapi( char* outBuffer,
                          const char* inBuffer,
                          const unsigned int& channelCount,
                          const unsigned int& inSampleRate,
                          const unsigned int& outSampleRate,
                          const unsigned int& inSampleCount,
                          unsigned int& outSampleCount,
                          const RtAudioFormat& format )
{
  // calculate the new outSampleCount and relative sampleStep
  float sampleRatio = ( float ) outSampleRate / inSampleRate;
  float sampleStep = 1.0f / sampleRatio;
  float inSampleFraction = 0.0f;

  outSampleCount = ( unsigned int ) roundf( inSampleCount * sampleRatio );

  // frame-by-frame, copy each relative input sample into it's corresponding output sample
  for ( unsigned int outSample = 0; outSample < outSampleCount; outSample++ )
  {
    unsigned int inSample = ( unsigned int ) inSampleFraction;

    switch ( format )
    {
      case RTAUDIO_SINT8:
        memcpy( &( ( char* ) outBuffer )[ outSample * channelCount ], &( ( char* ) inBuffer )[ inSample * channelCount ], channelCount * sizeof( char ) );
        break;
      case RTAUDIO_SINT16:
        memcpy( &( ( short* ) outBuffer )[ outSample * channelCount ], &( ( short* ) inBuffer )[ inSample * channelCount ], channelCount * sizeof( short ) );
        break;
      case RTAUDIO_SINT24:
        memcpy( &( ( S24* ) outBuffer )[ outSample * channelCount ], &( ( S24* ) inBuffer )[ inSample * channelCount ], channelCount * sizeof( S24 ) );
        break;
      case RTAUDIO_SINT32:
        memcpy( &( ( int* ) outBuffer )[ outSample * channelCount ], &( ( int* ) inBuffer )[ inSample * channelCount ], channelCount * sizeof( int ) );
        break;
      case RTAUDIO_FLOAT32:
        memcpy( &( ( float* ) outBuffer )[ outSample * channelCount ], &( ( float* ) inBuffer )[ inSample * channelCount ], channelCount * sizeof( float ) );
        break;
      case RTAUDIO_FLOAT64:
        memcpy( &( ( double* ) outBuffer )[ outSample * channelCount ], &( ( double* ) inBuffer )[ inSample * channelCount ], channelCount * sizeof( double ) );
        break;
    }

    // jump to next in sample
    inSampleFraction += sampleStep;
  }
}

//-----------------------------------------------------------------------------

// A structure to hold various information related to the WASAPI implementation.
struct WasapiHandle
{
  IAudioClient* captureAudioClient;
  IAudioClient* renderAudioClient;
  IAudioCaptureClient* captureClient;
  IAudioRenderClient* renderClient;
  HANDLE captureEvent;
  HANDLE renderEvent;

  WasapiHandle()
  : captureAudioClient( NULL ),
    renderAudioClient( NULL ),
    captureClient( NULL ),
    renderClient( NULL ),
    captureEvent( NULL ),
    renderEvent( NULL ) {}
};

//=============================================================================

RtApiWasapi::RtApiWasapi()
  : coInitialized_( false ), deviceEnumerator_( NULL )
{
  // WASAPI can run either apartment or multi-threaded
  HRESULT hr = CoInitialize( NULL );
  if ( !FAILED( hr ) )
    coInitialized_ = true;

  // Instantiate device enumerator
  hr = CoCreateInstance( __uuidof( MMDeviceEnumerator ), NULL,
                         CLSCTX_ALL, __uuidof( IMMDeviceEnumerator ),
                         ( void** ) &deviceEnumerator_ );

  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::RtApiWasapi: Unable to instantiate device enumerator";
    error( RtAudioError::DRIVER_ERROR );
  }
}

//-----------------------------------------------------------------------------

RtApiWasapi::~RtApiWasapi()
{
  if ( stream_.state != STREAM_CLOSED )
    closeStream();

  SAFE_RELEASE( deviceEnumerator_ );

  // If this object previously called CoInitialize()
  if ( coInitialized_ )
    CoUninitialize();
}

//=============================================================================

unsigned int RtApiWasapi::getDeviceCount( void )
{
  unsigned int captureDeviceCount = 0;
  unsigned int renderDeviceCount = 0;

  IMMDeviceCollection* captureDevices = NULL;
  IMMDeviceCollection* renderDevices = NULL;

  // Count capture devices
  errorText_.clear();
  HRESULT hr = deviceEnumerator_->EnumAudioEndpoints( eCapture, DEVICE_STATE_ACTIVE, &captureDevices );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceCount: Unable to retrieve capture device collection.";
    goto Exit;
  }

  hr = captureDevices->GetCount( &captureDeviceCount );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceCount: Unable to retrieve capture device count.";
    goto Exit;
  }

  // Count render devices
  hr = deviceEnumerator_->EnumAudioEndpoints( eRender, DEVICE_STATE_ACTIVE, &renderDevices );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceCount: Unable to retrieve render device collection.";
    goto Exit;
  }

  hr = renderDevices->GetCount( &renderDeviceCount );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceCount: Unable to retrieve render device count.";
    goto Exit;
  }

Exit:
  // release all references
  SAFE_RELEASE( captureDevices );
  SAFE_RELEASE( renderDevices );

  if ( errorText_.empty() )
    return captureDeviceCount + renderDeviceCount;

  error( RtAudioError::DRIVER_ERROR );
  return 0;
}

//-----------------------------------------------------------------------------

RtAudio::DeviceInfo RtApiWasapi::getDeviceInfo( unsigned int device )
{
  RtAudio::DeviceInfo info;
  unsigned int captureDeviceCount = 0;
  unsigned int renderDeviceCount = 0;
  std::string defaultDeviceName;
  bool isCaptureDevice = false;

  PROPVARIANT deviceNameProp;
  PROPVARIANT defaultDeviceNameProp;

  IMMDeviceCollection* captureDevices = NULL;
  IMMDeviceCollection* renderDevices = NULL;
  IMMDevice* devicePtr = NULL;
  IMMDevice* defaultDevicePtr = NULL;
  IAudioClient* audioClient = NULL;
  IPropertyStore* devicePropStore = NULL;
  IPropertyStore* defaultDevicePropStore = NULL;

  WAVEFORMATEX* deviceFormat = NULL;
  WAVEFORMATEX* closestMatchFormat = NULL;

  // probed
  info.probed = false;

  // Count capture devices
  errorText_.clear();
  RtAudioError::Type errorType = RtAudioError::DRIVER_ERROR;
  HRESULT hr = deviceEnumerator_->EnumAudioEndpoints( eCapture, DEVICE_STATE_ACTIVE, &captureDevices );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceInfo: Unable to retrieve capture device collection.";
    goto Exit;
  }

  hr = captureDevices->GetCount( &captureDeviceCount );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceInfo: Unable to retrieve capture device count.";
    goto Exit;
  }

  // Count render devices
  hr = deviceEnumerator_->EnumAudioEndpoints( eRender, DEVICE_STATE_ACTIVE, &renderDevices );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceInfo: Unable to retrieve render device collection.";
    goto Exit;
  }

  hr = renderDevices->GetCount( &renderDeviceCount );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceInfo: Unable to retrieve render device count.";
    goto Exit;
  }

  // validate device index
  if ( device >= captureDeviceCount + renderDeviceCount ) {
    errorText_ = "RtApiWasapi::getDeviceInfo: Invalid device index.";
    errorType = RtAudioError::INVALID_USE;
    goto Exit;
  }

  // determine whether index falls within capture or render devices
  if ( device >= renderDeviceCount ) {
    hr = captureDevices->Item( device - renderDeviceCount, &devicePtr );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::getDeviceInfo: Unable to retrieve capture device handle.";
      goto Exit;
    }
    isCaptureDevice = true;
  }
  else {
    hr = renderDevices->Item( device, &devicePtr );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::getDeviceInfo: Unable to retrieve render device handle.";
      goto Exit;
    }
    isCaptureDevice = false;
  }

  // get default device name
  if ( isCaptureDevice ) {
    hr = deviceEnumerator_->GetDefaultAudioEndpoint( eCapture, eConsole, &defaultDevicePtr );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::getDeviceInfo: Unable to retrieve default capture device handle.";
      goto Exit;
    }
  }
  else {
    hr = deviceEnumerator_->GetDefaultAudioEndpoint( eRender, eConsole, &defaultDevicePtr );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::getDeviceInfo: Unable to retrieve default render device handle.";
      goto Exit;
    }
  }

  hr = defaultDevicePtr->OpenPropertyStore( STGM_READ, &defaultDevicePropStore );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceInfo: Unable to open default device property store.";
    goto Exit;
  }
  PropVariantInit( &defaultDeviceNameProp );

  hr = defaultDevicePropStore->GetValue( PKEY_Device_FriendlyName, &defaultDeviceNameProp );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceInfo: Unable to retrieve default device property: PKEY_Device_FriendlyName.";
    goto Exit;
  }

  defaultDeviceName = convertCharPointerToStdString(defaultDeviceNameProp.pwszVal);

  // name
  hr = devicePtr->OpenPropertyStore( STGM_READ, &devicePropStore );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceInfo: Unable to open device property store.";
    goto Exit;
  }

  PropVariantInit( &deviceNameProp );

  hr = devicePropStore->GetValue( PKEY_Device_FriendlyName, &deviceNameProp );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceInfo: Unable to retrieve device property: PKEY_Device_FriendlyName.";
    goto Exit;
  }

  info.name =convertCharPointerToStdString(deviceNameProp.pwszVal);

  // is default
  if ( isCaptureDevice ) {
    info.isDefaultInput = info.name == defaultDeviceName;
    info.isDefaultOutput = false;
  }
  else {
    info.isDefaultInput = false;
    info.isDefaultOutput = info.name == defaultDeviceName;
  }

  // channel count
  hr = devicePtr->Activate( __uuidof( IAudioClient ), CLSCTX_ALL, NULL, ( void** ) &audioClient );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceInfo: Unable to retrieve device audio client.";
    goto Exit;
  }

  hr = audioClient->GetMixFormat( &deviceFormat );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::getDeviceInfo: Unable to retrieve device mix format.";
    goto Exit;
  }

  if ( isCaptureDevice ) {
    info.inputChannels = deviceFormat->nChannels;
    info.outputChannels = 0;
    info.duplexChannels = 0;
  }
  else {
    info.inputChannels = 0;
    info.outputChannels = deviceFormat->nChannels;
    info.duplexChannels = 0;
  }

  // sample rates
  info.sampleRates.clear();

  // allow support for all sample rates as we have a built-in sample rate converter
  for ( unsigned int i = 0; i < MAX_SAMPLE_RATES; i++ ) {
    info.sampleRates.push_back( SAMPLE_RATES[i] );
  }
  info.preferredSampleRate = deviceFormat->nSamplesPerSec;

  // native format
  info.nativeFormats = 0;

  if ( deviceFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
       ( deviceFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
         ( ( WAVEFORMATEXTENSIBLE* ) deviceFormat )->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT ) )
  {
    if ( deviceFormat->wBitsPerSample == 32 ) {
      info.nativeFormats |= RTAUDIO_FLOAT32;
    }
    else if ( deviceFormat->wBitsPerSample == 64 ) {
      info.nativeFormats |= RTAUDIO_FLOAT64;
    }
  }
  else if ( deviceFormat->wFormatTag == WAVE_FORMAT_PCM ||
           ( deviceFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
             ( ( WAVEFORMATEXTENSIBLE* ) deviceFormat )->SubFormat == KSDATAFORMAT_SUBTYPE_PCM ) )
  {
    if ( deviceFormat->wBitsPerSample == 8 ) {
      info.nativeFormats |= RTAUDIO_SINT8;
    }
    else if ( deviceFormat->wBitsPerSample == 16 ) {
      info.nativeFormats |= RTAUDIO_SINT16;
    }
    else if ( deviceFormat->wBitsPerSample == 24 ) {
      info.nativeFormats |= RTAUDIO_SINT24;
    }
    else if ( deviceFormat->wBitsPerSample == 32 ) {
      info.nativeFormats |= RTAUDIO_SINT32;
    }
  }

  // probed
  info.probed = true;

Exit:
  // release all references
  PropVariantClear( &deviceNameProp );
  PropVariantClear( &defaultDeviceNameProp );

  SAFE_RELEASE( captureDevices );
  SAFE_RELEASE( renderDevices );
  SAFE_RELEASE( devicePtr );
  SAFE_RELEASE( defaultDevicePtr );
  SAFE_RELEASE( audioClient );
  SAFE_RELEASE( devicePropStore );
  SAFE_RELEASE( defaultDevicePropStore );

  CoTaskMemFree( deviceFormat );
  CoTaskMemFree( closestMatchFormat );

  if ( !errorText_.empty() )
    error( errorType );
  return info;
}

//-----------------------------------------------------------------------------

unsigned int RtApiWasapi::getDefaultOutputDevice( void )
{
  for ( unsigned int i = 0; i < getDeviceCount(); i++ ) {
    if ( getDeviceInfo( i ).isDefaultOutput ) {
      return i;
    }
  }

  return 0;
}

//-----------------------------------------------------------------------------

unsigned int RtApiWasapi::getDefaultInputDevice( void )
{
  for ( unsigned int i = 0; i < getDeviceCount(); i++ ) {
    if ( getDeviceInfo( i ).isDefaultInput ) {
      return i;
    }
  }

  return 0;
}

//-----------------------------------------------------------------------------

void RtApiWasapi::closeStream( void )
{
  if ( stream_.state == STREAM_CLOSED ) {
    errorText_ = "RtApiWasapi::closeStream: No open stream to close.";
    error( RtAudioError::WARNING );
    return;
  }

  if ( stream_.state != STREAM_STOPPED )
    stopStream();

  // clean up stream memory
  SAFE_RELEASE( ( ( WasapiHandle* ) stream_.apiHandle )->captureAudioClient )
  SAFE_RELEASE( ( ( WasapiHandle* ) stream_.apiHandle )->renderAudioClient )

  SAFE_RELEASE( ( ( WasapiHandle* ) stream_.apiHandle )->captureClient )
  SAFE_RELEASE( ( ( WasapiHandle* ) stream_.apiHandle )->renderClient )

  if ( ( ( WasapiHandle* ) stream_.apiHandle )->captureEvent )
    CloseHandle( ( ( WasapiHandle* ) stream_.apiHandle )->captureEvent );

  if ( ( ( WasapiHandle* ) stream_.apiHandle )->renderEvent )
    CloseHandle( ( ( WasapiHandle* ) stream_.apiHandle )->renderEvent );

  delete ( WasapiHandle* ) stream_.apiHandle;
  stream_.apiHandle = NULL;

  for ( int i = 0; i < 2; i++ ) {
    if ( stream_.userBuffer[i] ) {
      free( stream_.userBuffer[i] );
      stream_.userBuffer[i] = 0;
    }
  }

  if ( stream_.deviceBuffer ) {
    free( stream_.deviceBuffer );
    stream_.deviceBuffer = 0;
  }

  // update stream state
  stream_.state = STREAM_CLOSED;
}

//-----------------------------------------------------------------------------

void RtApiWasapi::startStream( void )
{
  verifyStream();

  if ( stream_.state == STREAM_RUNNING ) {
    errorText_ = "RtApiWasapi::startStream: The stream is already running.";
    error( RtAudioError::WARNING );
    return;
  }

  // update stream state
  stream_.state = STREAM_RUNNING;

  // create WASAPI stream thread
  stream_.callbackInfo.thread = ( ThreadHandle ) CreateThread( NULL, 0, runWasapiThread, this, CREATE_SUSPENDED, NULL );

  if ( !stream_.callbackInfo.thread ) {
    errorText_ = "RtApiWasapi::startStream: Unable to instantiate callback thread.";
    error( RtAudioError::THREAD_ERROR );
  }
  else {
    SetThreadPriority( ( void* ) stream_.callbackInfo.thread, stream_.callbackInfo.priority );
    ResumeThread( ( void* ) stream_.callbackInfo.thread );
  }
}

//-----------------------------------------------------------------------------

void RtApiWasapi::stopStream( void )
{
  verifyStream();

  if ( stream_.state == STREAM_STOPPED ) {
    errorText_ = "RtApiWasapi::stopStream: The stream is already stopped.";
    error( RtAudioError::WARNING );
    return;
  }

  // inform stream thread by setting stream state to STREAM_STOPPING
  stream_.state = STREAM_STOPPING;

  // wait until stream thread is stopped
  while( stream_.state != STREAM_STOPPED ) {
    Sleep( 1 );
  }

  // Wait for the last buffer to play before stopping.
  Sleep( 1000 * stream_.bufferSize / stream_.sampleRate );

  // stop capture client if applicable
  if ( ( ( WasapiHandle* ) stream_.apiHandle )->captureAudioClient ) {
    HRESULT hr = ( ( WasapiHandle* ) stream_.apiHandle )->captureAudioClient->Stop();
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::stopStream: Unable to stop capture stream.";
      error( RtAudioError::DRIVER_ERROR );
      return;
    }
  }

  // stop render client if applicable
  if ( ( ( WasapiHandle* ) stream_.apiHandle )->renderAudioClient ) {
    HRESULT hr = ( ( WasapiHandle* ) stream_.apiHandle )->renderAudioClient->Stop();
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::stopStream: Unable to stop render stream.";
      error( RtAudioError::DRIVER_ERROR );
      return;
    }
  }

  // close thread handle
  if ( stream_.callbackInfo.thread && !CloseHandle( ( void* ) stream_.callbackInfo.thread ) ) {
    errorText_ = "RtApiWasapi::stopStream: Unable to close callback thread.";
    error( RtAudioError::THREAD_ERROR );
    return;
  }

  stream_.callbackInfo.thread = (ThreadHandle) NULL;
}

//-----------------------------------------------------------------------------

void RtApiWasapi::abortStream( void )
{
  verifyStream();

  if ( stream_.state == STREAM_STOPPED ) {
    errorText_ = "RtApiWasapi::abortStream: The stream is already stopped.";
    error( RtAudioError::WARNING );
    return;
  }

  // inform stream thread by setting stream state to STREAM_STOPPING
  stream_.state = STREAM_STOPPING;

  // wait until stream thread is stopped
  while ( stream_.state != STREAM_STOPPED ) {
    Sleep( 1 );
  }

  // stop capture client if applicable
  if ( ( ( WasapiHandle* ) stream_.apiHandle )->captureAudioClient ) {
    HRESULT hr = ( ( WasapiHandle* ) stream_.apiHandle )->captureAudioClient->Stop();
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::abortStream: Unable to stop capture stream.";
      error( RtAudioError::DRIVER_ERROR );
      return;
    }
  }

  // stop render client if applicable
  if ( ( ( WasapiHandle* ) stream_.apiHandle )->renderAudioClient ) {
    HRESULT hr = ( ( WasapiHandle* ) stream_.apiHandle )->renderAudioClient->Stop();
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::abortStream: Unable to stop render stream.";
      error( RtAudioError::DRIVER_ERROR );
      return;
    }
  }

  // close thread handle
  if ( stream_.callbackInfo.thread && !CloseHandle( ( void* ) stream_.callbackInfo.thread ) ) {
    errorText_ = "RtApiWasapi::abortStream: Unable to close callback thread.";
    error( RtAudioError::THREAD_ERROR );
    return;
  }

  stream_.callbackInfo.thread = (ThreadHandle) NULL;
}

//-----------------------------------------------------------------------------

bool RtApiWasapi::probeDeviceOpen( unsigned int device, StreamMode mode, unsigned int channels,
                                   unsigned int firstChannel, unsigned int sampleRate,
                                   RtAudioFormat format, unsigned int* bufferSize,
                                   RtAudio::StreamOptions* options )
{
  bool methodResult = FAILURE;
  unsigned int captureDeviceCount = 0;
  unsigned int renderDeviceCount = 0;

  IMMDeviceCollection* captureDevices = NULL;
  IMMDeviceCollection* renderDevices = NULL;
  IMMDevice* devicePtr = NULL;
  WAVEFORMATEX* deviceFormat = NULL;
  unsigned int bufferBytes;
  stream_.state = STREAM_STOPPED;

  // create API Handle if not already created
  if ( !stream_.apiHandle )
    stream_.apiHandle = ( void* ) new WasapiHandle();

  // Count capture devices
  errorText_.clear();
  RtAudioError::Type errorType = RtAudioError::DRIVER_ERROR;
  HRESULT hr = deviceEnumerator_->EnumAudioEndpoints( eCapture, DEVICE_STATE_ACTIVE, &captureDevices );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::probeDeviceOpen: Unable to retrieve capture device collection.";
    goto Exit;
  }

  hr = captureDevices->GetCount( &captureDeviceCount );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::probeDeviceOpen: Unable to retrieve capture device count.";
    goto Exit;
  }

  // Count render devices
  hr = deviceEnumerator_->EnumAudioEndpoints( eRender, DEVICE_STATE_ACTIVE, &renderDevices );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::probeDeviceOpen: Unable to retrieve render device collection.";
    goto Exit;
  }

  hr = renderDevices->GetCount( &renderDeviceCount );
  if ( FAILED( hr ) ) {
    errorText_ = "RtApiWasapi::probeDeviceOpen: Unable to retrieve render device count.";
    goto Exit;
  }

  // validate device index
  if ( device >= captureDeviceCount + renderDeviceCount ) {
    errorType = RtAudioError::INVALID_USE;
    errorText_ = "RtApiWasapi::probeDeviceOpen: Invalid device index.";
    goto Exit;
  }

  // determine whether index falls within capture or render devices
  if ( device >= renderDeviceCount ) {
    if ( mode != INPUT ) {
      errorType = RtAudioError::INVALID_USE;
      errorText_ = "RtApiWasapi::probeDeviceOpen: Capture device selected as output device.";
      goto Exit;
    }

    // retrieve captureAudioClient from devicePtr
    IAudioClient*& captureAudioClient = ( ( WasapiHandle* ) stream_.apiHandle )->captureAudioClient;

    hr = captureDevices->Item( device - renderDeviceCount, &devicePtr );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::probeDeviceOpen: Unable to retrieve capture device handle.";
      goto Exit;
    }

    hr = devicePtr->Activate( __uuidof( IAudioClient ), CLSCTX_ALL,
                              NULL, ( void** ) &captureAudioClient );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::probeDeviceOpen: Unable to retrieve device audio client.";
      goto Exit;
    }

    hr = captureAudioClient->GetMixFormat( &deviceFormat );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::probeDeviceOpen: Unable to retrieve device mix format.";
      goto Exit;
    }

    stream_.nDeviceChannels[mode] = deviceFormat->nChannels;
    captureAudioClient->GetStreamLatency( ( long long* ) &stream_.latency[mode] );
  }
  else {
    if ( mode != OUTPUT ) {
      errorType = RtAudioError::INVALID_USE;
      errorText_ = "RtApiWasapi::probeDeviceOpen: Render device selected as input device.";
      goto Exit;
    }

    // retrieve renderAudioClient from devicePtr
    IAudioClient*& renderAudioClient = ( ( WasapiHandle* ) stream_.apiHandle )->renderAudioClient;

    hr = renderDevices->Item( device, &devicePtr );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::probeDeviceOpen: Unable to retrieve render device handle.";
      goto Exit;
    }

    hr = devicePtr->Activate( __uuidof( IAudioClient ), CLSCTX_ALL,
                              NULL, ( void** ) &renderAudioClient );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::probeDeviceOpen: Unable to retrieve device audio client.";
      goto Exit;
    }

    hr = renderAudioClient->GetMixFormat( &deviceFormat );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::probeDeviceOpen: Unable to retrieve device mix format.";
      goto Exit;
    }

    stream_.nDeviceChannels[mode] = deviceFormat->nChannels;
    renderAudioClient->GetStreamLatency( ( long long* ) &stream_.latency[mode] );
  }

  // fill stream data
  if ( ( stream_.mode == OUTPUT && mode == INPUT ) ||
       ( stream_.mode == INPUT && mode == OUTPUT ) ) {
    stream_.mode = DUPLEX;
  }
  else {
    stream_.mode = mode;
  }

  stream_.device[mode] = device;
  stream_.doByteSwap[mode] = false;
  stream_.sampleRate = sampleRate;
  stream_.bufferSize = *bufferSize;
  stream_.nBuffers = 1;
  stream_.nUserChannels[mode] = channels;
  stream_.channelOffset[mode] = firstChannel;
  stream_.userFormat = format;
  stream_.deviceFormat[mode] = getDeviceInfo( device ).nativeFormats;

  if ( options && options->flags & RTAUDIO_NONINTERLEAVED )
    stream_.userInterleaved = false;
  else
    stream_.userInterleaved = true;
  stream_.deviceInterleaved[mode] = true;

  // Set flags for buffer conversion.
  stream_.doConvertBuffer[mode] = false;
  if ( stream_.userFormat != stream_.deviceFormat[mode] ||
       stream_.nUserChannels != stream_.nDeviceChannels )
    stream_.doConvertBuffer[mode] = true;
  else if ( stream_.userInterleaved != stream_.deviceInterleaved[mode] &&
            stream_.nUserChannels[mode] > 1 )
    stream_.doConvertBuffer[mode] = true;

  if ( stream_.doConvertBuffer[mode] )
    setConvertInfo( mode, 0 );

  // Allocate necessary internal buffers
  bufferBytes = stream_.nUserChannels[mode] * stream_.bufferSize * formatBytes( stream_.userFormat );

  stream_.userBuffer[mode] = ( char* ) calloc( bufferBytes, 1 );
  if ( !stream_.userBuffer[mode] ) {
    errorType = RtAudioError::MEMORY_ERROR;
    errorText_ = "RtApiWasapi::probeDeviceOpen: Error allocating user buffer memory.";
    goto Exit;
  }

  if ( options && options->flags & RTAUDIO_SCHEDULE_REALTIME )
    stream_.callbackInfo.priority = 15;
  else
    stream_.callbackInfo.priority = 0;

  ///! TODO: RTAUDIO_MINIMIZE_LATENCY // Provide stream buffers directly to callback
  ///! TODO: RTAUDIO_HOG_DEVICE       // Exclusive mode

  methodResult = SUCCESS;

Exit:
  //clean up
  SAFE_RELEASE( captureDevices );
  SAFE_RELEASE( renderDevices );
  SAFE_RELEASE( devicePtr );
  CoTaskMemFree( deviceFormat );

  // if method failed, close the stream
  if ( methodResult == FAILURE )
    closeStream();

  if ( !errorText_.empty() )
    error( errorType );
  return methodResult;
}

//=============================================================================

DWORD WINAPI RtApiWasapi::runWasapiThread( void* wasapiPtr )
{
  if ( wasapiPtr )
    ( ( RtApiWasapi* ) wasapiPtr )->wasapiThread();

  return 0;
}

DWORD WINAPI RtApiWasapi::stopWasapiThread( void* wasapiPtr )
{
  if ( wasapiPtr )
    ( ( RtApiWasapi* ) wasapiPtr )->stopStream();

  return 0;
}

DWORD WINAPI RtApiWasapi::abortWasapiThread( void* wasapiPtr )
{
  if ( wasapiPtr )
    ( ( RtApiWasapi* ) wasapiPtr )->abortStream();

  return 0;
}

//-----------------------------------------------------------------------------

void RtApiWasapi::wasapiThread()
{
  // as this is a new thread, we must CoInitialize it
  CoInitialize( NULL );

  HRESULT hr;

  IAudioClient* captureAudioClient = ( ( WasapiHandle* ) stream_.apiHandle )->captureAudioClient;
  IAudioClient* renderAudioClient = ( ( WasapiHandle* ) stream_.apiHandle )->renderAudioClient;
  IAudioCaptureClient* captureClient = ( ( WasapiHandle* ) stream_.apiHandle )->captureClient;
  IAudioRenderClient* renderClient = ( ( WasapiHandle* ) stream_.apiHandle )->renderClient;
  HANDLE captureEvent = ( ( WasapiHandle* ) stream_.apiHandle )->captureEvent;
  HANDLE renderEvent = ( ( WasapiHandle* ) stream_.apiHandle )->renderEvent;

  WAVEFORMATEX* captureFormat = NULL;
  WAVEFORMATEX* renderFormat = NULL;
  float captureSrRatio = 0.0f;
  float renderSrRatio = 0.0f;
  WasapiBuffer captureBuffer;
  WasapiBuffer renderBuffer;

  // declare local stream variables
  RtAudioCallback callback = ( RtAudioCallback ) stream_.callbackInfo.callback;
  BYTE* streamBuffer = NULL;
  unsigned long captureFlags = 0;
  unsigned int bufferFrameCount = 0;
  unsigned int numFramesPadding = 0;
  unsigned int convBufferSize = 0;
  bool callbackPushed = false;
  bool callbackPulled = false;
  bool callbackStopped = false;
  int callbackResult = 0;

  // convBuffer is used to store converted buffers between WASAPI and the user
  char* convBuffer = NULL;
  unsigned int convBuffSize = 0;
  unsigned int deviceBuffSize = 0;

  errorText_.clear();
  RtAudioError::Type errorType = RtAudioError::DRIVER_ERROR;

  // Attempt to assign "Pro Audio" characteristic to thread
  HMODULE AvrtDll = LoadLibrary( (LPCTSTR) "AVRT.dll" );
  if ( AvrtDll ) {
    DWORD taskIndex = 0;
    TAvSetMmThreadCharacteristicsPtr AvSetMmThreadCharacteristicsPtr = ( TAvSetMmThreadCharacteristicsPtr ) GetProcAddress( AvrtDll, "AvSetMmThreadCharacteristicsW" );
    AvSetMmThreadCharacteristicsPtr( L"Pro Audio", &taskIndex );
    FreeLibrary( AvrtDll );
  }

  // start capture stream if applicable
  if ( captureAudioClient ) {
    hr = captureAudioClient->GetMixFormat( &captureFormat );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::wasapiThread: Unable to retrieve device mix format.";
      goto Exit;
    }

    captureSrRatio = ( ( float ) captureFormat->nSamplesPerSec / stream_.sampleRate );

    // initialize capture stream according to desire buffer size
    float desiredBufferSize = stream_.bufferSize * captureSrRatio;
    REFERENCE_TIME desiredBufferPeriod = ( REFERENCE_TIME ) ( ( float ) desiredBufferSize * 10000000 / captureFormat->nSamplesPerSec );

    if ( !captureClient ) {
      hr = captureAudioClient->Initialize( AUDCLNT_SHAREMODE_SHARED,
                                           AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                           desiredBufferPeriod,
                                           desiredBufferPeriod,
                                           captureFormat,
                                           NULL );
      if ( FAILED( hr ) ) {
        errorText_ = "RtApiWasapi::wasapiThread: Unable to initialize capture audio client.";
        goto Exit;
      }

      hr = captureAudioClient->GetService( __uuidof( IAudioCaptureClient ),
                                           ( void** ) &captureClient );
      if ( FAILED( hr ) ) {
        errorText_ = "RtApiWasapi::wasapiThread: Unable to retrieve capture client handle.";
        goto Exit;
      }

      // configure captureEvent to trigger on every available capture buffer
      captureEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
      if ( !captureEvent ) {
        errorType = RtAudioError::SYSTEM_ERROR;
        errorText_ = "RtApiWasapi::wasapiThread: Unable to create capture event.";
        goto Exit;
      }

      hr = captureAudioClient->SetEventHandle( captureEvent );
      if ( FAILED( hr ) ) {
        errorText_ = "RtApiWasapi::wasapiThread: Unable to set capture event handle.";
        goto Exit;
      }

      ( ( WasapiHandle* ) stream_.apiHandle )->captureClient = captureClient;
      ( ( WasapiHandle* ) stream_.apiHandle )->captureEvent = captureEvent;
    }

    unsigned int inBufferSize = 0;
    hr = captureAudioClient->GetBufferSize( &inBufferSize );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::wasapiThread: Unable to get capture buffer size.";
      goto Exit;
    }

    // scale outBufferSize according to stream->user sample rate ratio
    unsigned int outBufferSize = ( unsigned int ) ( stream_.bufferSize * captureSrRatio ) * stream_.nDeviceChannels[INPUT];
    inBufferSize *= stream_.nDeviceChannels[INPUT];

    // set captureBuffer size
    captureBuffer.setBufferSize( inBufferSize + outBufferSize, formatBytes( stream_.deviceFormat[INPUT] ) );

    // reset the capture stream
    hr = captureAudioClient->Reset();
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::wasapiThread: Unable to reset capture stream.";
      goto Exit;
    }

    // start the capture stream
    hr = captureAudioClient->Start();
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::wasapiThread: Unable to start capture stream.";
      goto Exit;
    }
  }

  // start render stream if applicable
  if ( renderAudioClient ) {
    hr = renderAudioClient->GetMixFormat( &renderFormat );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::wasapiThread: Unable to retrieve device mix format.";
      goto Exit;
    }

    renderSrRatio = ( ( float ) renderFormat->nSamplesPerSec / stream_.sampleRate );

    // initialize render stream according to desire buffer size
    float desiredBufferSize = stream_.bufferSize * renderSrRatio;
    REFERENCE_TIME desiredBufferPeriod = ( REFERENCE_TIME ) ( ( float ) desiredBufferSize * 10000000 / renderFormat->nSamplesPerSec );

    if ( !renderClient ) {
      hr = renderAudioClient->Initialize( AUDCLNT_SHAREMODE_SHARED,
                                          AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                          desiredBufferPeriod,
                                          desiredBufferPeriod,
                                          renderFormat,
                                          NULL );
      if ( FAILED( hr ) ) {
        errorText_ = "RtApiWasapi::wasapiThread: Unable to initialize render audio client.";
        goto Exit;
      }

      hr = renderAudioClient->GetService( __uuidof( IAudioRenderClient ),
                                          ( void** ) &renderClient );
      if ( FAILED( hr ) ) {
        errorText_ = "RtApiWasapi::wasapiThread: Unable to retrieve render client handle.";
        goto Exit;
      }

      // configure renderEvent to trigger on every available render buffer
      renderEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
      if ( !renderEvent ) {
        errorType = RtAudioError::SYSTEM_ERROR;
        errorText_ = "RtApiWasapi::wasapiThread: Unable to create render event.";
        goto Exit;
      }

      hr = renderAudioClient->SetEventHandle( renderEvent );
      if ( FAILED( hr ) ) {
        errorText_ = "RtApiWasapi::wasapiThread: Unable to set render event handle.";
        goto Exit;
      }

      ( ( WasapiHandle* ) stream_.apiHandle )->renderClient = renderClient;
      ( ( WasapiHandle* ) stream_.apiHandle )->renderEvent = renderEvent;
    }

    unsigned int outBufferSize = 0;
    hr = renderAudioClient->GetBufferSize( &outBufferSize );
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::wasapiThread: Unable to get render buffer size.";
      goto Exit;
    }

    // scale inBufferSize according to user->stream sample rate ratio
    unsigned int inBufferSize = ( unsigned int ) ( stream_.bufferSize * renderSrRatio ) * stream_.nDeviceChannels[OUTPUT];
    outBufferSize *= stream_.nDeviceChannels[OUTPUT];

    // set renderBuffer size
    renderBuffer.setBufferSize( inBufferSize + outBufferSize, formatBytes( stream_.deviceFormat[OUTPUT] ) );

    // reset the render stream
    hr = renderAudioClient->Reset();
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::wasapiThread: Unable to reset render stream.";
      goto Exit;
    }

    // start the render stream
    hr = renderAudioClient->Start();
    if ( FAILED( hr ) ) {
      errorText_ = "RtApiWasapi::wasapiThread: Unable to start render stream.";
      goto Exit;
    }
  }

  if ( stream_.mode == INPUT ) {
    convBuffSize = ( size_t ) ( stream_.bufferSize * captureSrRatio ) * stream_.nDeviceChannels[INPUT] * formatBytes( stream_.deviceFormat[INPUT] );
    deviceBuffSize = stream_.bufferSize * stream_.nDeviceChannels[INPUT] * formatBytes( stream_.deviceFormat[INPUT] );
  }
  else if ( stream_.mode == OUTPUT ) {
    convBuffSize = ( size_t ) ( stream_.bufferSize * renderSrRatio ) * stream_.nDeviceChannels[OUTPUT] * formatBytes( stream_.deviceFormat[OUTPUT] );
    deviceBuffSize = stream_.bufferSize * stream_.nDeviceChannels[OUTPUT] * formatBytes( stream_.deviceFormat[OUTPUT] );
  }
  else if ( stream_.mode == DUPLEX ) {
    convBuffSize = std::max( ( size_t ) ( stream_.bufferSize * captureSrRatio ) * stream_.nDeviceChannels[INPUT] * formatBytes( stream_.deviceFormat[INPUT] ),
                             ( size_t ) ( stream_.bufferSize * renderSrRatio ) * stream_.nDeviceChannels[OUTPUT] * formatBytes( stream_.deviceFormat[OUTPUT] ) );
    deviceBuffSize = std::max( stream_.bufferSize * stream_.nDeviceChannels[INPUT] * formatBytes( stream_.deviceFormat[INPUT] ),
                               stream_.bufferSize * stream_.nDeviceChannels[OUTPUT] * formatBytes( stream_.deviceFormat[OUTPUT] ) );
  }

  convBuffer = ( char* ) malloc( convBuffSize );
  stream_.deviceBuffer = ( char* ) malloc( deviceBuffSize );
  if ( !convBuffer || !stream_.deviceBuffer ) {
    errorType = RtAudioError::MEMORY_ERROR;
    errorText_ = "RtApiWasapi::wasapiThread: Error allocating device buffer memory.";
    goto Exit;
  }

  // stream process loop
  while ( stream_.state != STREAM_STOPPING ) {
    if ( !callbackPulled ) {
      // Callback Input
      // ==============
      // 1. Pull callback buffer from inputBuffer
      // 2. If 1. was successful: Convert callback buffer to user sample rate and channel count
      //                          Convert callback buffer to user format

      if ( captureAudioClient ) {
        // Pull callback buffer from inputBuffer
        callbackPulled = captureBuffer.pullBuffer( convBuffer,
                                                   ( unsigned int ) ( stream_.bufferSize * captureSrRatio ) * stream_.nDeviceChannels[INPUT],
                                                   stream_.deviceFormat[INPUT] );

        if ( callbackPulled ) {
          // Convert callback buffer to user sample rate
          convertBufferWasapi( stream_.deviceBuffer,
                               convBuffer,
                               stream_.nDeviceChannels[INPUT],
                               captureFormat->nSamplesPerSec,
                               stream_.sampleRate,
                               ( unsigned int ) ( stream_.bufferSize * captureSrRatio ),
                               convBufferSize,
                               stream_.deviceFormat[INPUT] );

          if ( stream_.doConvertBuffer[INPUT] ) {
            // Convert callback buffer to user format
            convertBuffer( stream_.userBuffer[INPUT],
                           stream_.deviceBuffer,
                           stream_.convertInfo[INPUT] );
          }
          else {
            // no further conversion, simple copy deviceBuffer to userBuffer
            memcpy( stream_.userBuffer[INPUT],
                    stream_.deviceBuffer,
                    stream_.bufferSize * stream_.nUserChannels[INPUT] * formatBytes( stream_.userFormat ) );
          }
        }
      }
      else {
        // if there is no capture stream, set callbackPulled flag
        callbackPulled = true;
      }

      // Execute Callback
      // ================
      // 1. Execute user callback method
      // 2. Handle return value from callback

      // if callback has not requested the stream to stop
      if ( callbackPulled && !callbackStopped ) {
        // Execute user callback method
        callbackResult = callback( stream_.userBuffer[OUTPUT],
                                   stream_.userBuffer[INPUT],
                                   stream_.bufferSize,
                                   getStreamTime(),
                                   captureFlags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY ? RTAUDIO_INPUT_OVERFLOW : 0,
                                   stream_.callbackInfo.userData );

        // Handle return value from callback
        if ( callbackResult == 1 ) {
          // instantiate a thread to stop this thread
          HANDLE threadHandle = CreateThread( NULL, 0, stopWasapiThread, this, 0, NULL );
          if ( !threadHandle ) {
            errorType = RtAudioError::THREAD_ERROR;
            errorText_ = "RtApiWasapi::wasapiThread: Unable to instantiate stream stop thread.";
            goto Exit;
          }
          else if ( !CloseHandle( threadHandle ) ) {
            errorType = RtAudioError::THREAD_ERROR;
            errorText_ = "RtApiWasapi::wasapiThread: Unable to close stream stop thread handle.";
            goto Exit;
          }

          callbackStopped = true;
        }
        else if ( callbackResult == 2 ) {
          // instantiate a thread to stop this thread
          HANDLE threadHandle = CreateThread( NULL, 0, abortWasapiThread, this, 0, NULL );
          if ( !threadHandle ) {
            errorType = RtAudioError::THREAD_ERROR;
            errorText_ = "RtApiWasapi::wasapiThread: Unable to instantiate stream abort thread.";
            goto Exit;
          }
          else if ( !CloseHandle( threadHandle ) ) {
            errorType = RtAudioError::THREAD_ERROR;
            errorText_ = "RtApiWasapi::wasapiThread: Unable to close stream abort thread handle.";
            goto Exit;
          }

          callbackStopped = true;
        }
      }
    }

    // Callback Output
    // ===============
    // 1. Convert callback buffer to stream format
    // 2. Convert callback buffer to stream sample rate and channel count
    // 3. Push callback buffer into outputBuffer

    if ( renderAudioClient && callbackPulled ) {
      if ( stream_.doConvertBuffer[OUTPUT] ) {
        // Convert callback buffer to stream format
        convertBuffer( stream_.deviceBuffer,
                       stream_.userBuffer[OUTPUT],
                       stream_.convertInfo[OUTPUT] );

      }

      // Convert callback buffer to stream sample rate
      convertBufferWasapi( convBuffer,
                           stream_.deviceBuffer,
                           stream_.nDeviceChannels[OUTPUT],
                           stream_.sampleRate,
                           renderFormat->nSamplesPerSec,
                           stream_.bufferSize,
                           convBufferSize,
                           stream_.deviceFormat[OUTPUT] );

      // Push callback buffer into outputBuffer
      callbackPushed = renderBuffer.pushBuffer( convBuffer,
                                                convBufferSize * stream_.nDeviceChannels[OUTPUT],
                                                stream_.deviceFormat[OUTPUT] );
    }
    else {
      // if there is no render stream, set callbackPushed flag
      callbackPushed = true;
    }

    // Stream Capture
    // ==============
    // 1. Get capture buffer from stream
    // 2. Push capture buffer into inputBuffer
    // 3. If 2. was successful: Release capture buffer

    if ( captureAudioClient ) {
      // if the callback input buffer was not pulled from captureBuffer, wait for next capture event
      if ( !callbackPulled ) {
        WaitForSingleObject( captureEvent, INFINITE );
      }

      // Get capture buffer from stream
      hr = captureClient->GetBuffer( &streamBuffer,
                                     &bufferFrameCount,
                                     &captureFlags, NULL, NULL );
      if ( FAILED( hr ) ) {
        errorText_ = "RtApiWasapi::wasapiThread: Unable to retrieve capture buffer.";
        goto Exit;
      }

      if ( bufferFrameCount != 0 ) {
        // Push capture buffer into inputBuffer
        if ( captureBuffer.pushBuffer( ( char* ) streamBuffer,
                                       bufferFrameCount * stream_.nDeviceChannels[INPUT],
                                       stream_.deviceFormat[INPUT] ) )
        {
          // Release capture buffer
          hr = captureClient->ReleaseBuffer( bufferFrameCount );
          if ( FAILED( hr ) ) {
            errorText_ = "RtApiWasapi::wasapiThread: Unable to release capture buffer.";
            goto Exit;
          }
        }
        else
        {
          // Inform WASAPI that capture was unsuccessful
          hr = captureClient->ReleaseBuffer( 0 );
          if ( FAILED( hr ) ) {
            errorText_ = "RtApiWasapi::wasapiThread: Unable to release capture buffer.";
            goto Exit;
          }
        }
      }
      else
      {
        // Inform WASAPI that capture was unsuccessful
        hr = captureClient->ReleaseBuffer( 0 );
        if ( FAILED( hr ) ) {
          errorText_ = "RtApiWasapi::wasapiThread: Unable to release capture buffer.";
          goto Exit;
        }
      }
    }

    // Stream Render
    // =============
    // 1. Get render buffer from stream
    // 2. Pull next buffer from outputBuffer
    // 3. If 2. was successful: Fill render buffer with next buffer
    //                          Release render buffer

    if ( renderAudioClient ) {
      // if the callback output buffer was not pushed to renderBuffer, wait for next render event
      if ( callbackPulled && !callbackPushed ) {
        WaitForSingleObject( renderEvent, INFINITE );
      }

      // Get render buffer from stream
      hr = renderAudioClient->GetBufferSize( &bufferFrameCount );
      if ( FAILED( hr ) ) {
        errorText_ = "RtApiWasapi::wasapiThread: Unable to retrieve render buffer size.";
        goto Exit;
      }

      hr = renderAudioClient->GetCurrentPadding( &numFramesPadding );
      if ( FAILED( hr ) ) {
        errorText_ = "RtApiWasapi::wasapiThread: Unable to retrieve render buffer padding.";
        goto Exit;
      }

      bufferFrameCount -= numFramesPadding;

      if ( bufferFrameCount != 0 ) {
        hr = renderClient->GetBuffer( bufferFrameCount, &streamBuffer );
        if ( FAILED( hr ) ) {
          errorText_ = "RtApiWasapi::wasapiThread: Unable to retrieve render buffer.";
          goto Exit;
        }

        // Pull next buffer from outputBuffer
        // Fill render buffer with next buffer
        if ( renderBuffer.pullBuffer( ( char* ) streamBuffer,
                                      bufferFrameCount * stream_.nDeviceChannels[OUTPUT],
                                      stream_.deviceFormat[OUTPUT] ) )
        {
          // Release render buffer
          hr = renderClient->ReleaseBuffer( bufferFrameCount, 0 );
          if ( FAILED( hr ) ) {
            errorText_ = "RtApiWasapi::wasapiThread: Unable to release render buffer.";
            goto Exit;
          }
        }
        else
        {
          // Inform WASAPI that render was unsuccessful
          hr = renderClient->ReleaseBuffer( 0, 0 );
          if ( FAILED( hr ) ) {
            errorText_ = "RtApiWasapi::wasapiThread: Unable to release render buffer.";
            goto Exit;
          }
        }
      }
      else
      {
        // Inform WASAPI that render was unsuccessful
        hr = renderClient->ReleaseBuffer( 0, 0 );
        if ( FAILED( hr ) ) {
          errorText_ = "RtApiWasapi::wasapiThread: Unable to release render buffer.";
          goto Exit;
        }
      }
    }

    // if the callback buffer was pushed renderBuffer reset callbackPulled flag
    if ( callbackPushed ) {
      callbackPulled = false;
      // tick stream time
      RtApi::tickStreamTime();
    }

  }

Exit:
  // clean up
  CoTaskMemFree( captureFormat );
  CoTaskMemFree( renderFormat );

  free ( convBuffer );

  CoUninitialize();

  // update stream state
  stream_.state = STREAM_STOPPED;

  if ( errorText_.empty() )
    return;
  else
    error( errorType );
}

//******************** End of __WINDOWS_WASAPI__ *********************//
#endif
