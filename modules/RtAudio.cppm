module;

#ifdef RTAUDIO_USE_NAMESPACE
#define RTAUDIO_NAMESPACE_INLINE inline
#else
#define RTAUDIO_NAMESPACE_INLINE
#endif

#include "RtAudio.h"

export module rtaudio;

export RTAUDIO_NAMESPACE_INLINE namespace rtaudio {
    using rtaudio::RtAudioFormat;
    using rtaudio::RtAudioStreamFlags;
    using rtaudio::RtAudioStreamStatus;
    using rtaudio::RtAudioCallback;
    using rtaudio::RtAudioErrorType;
    using rtaudio::RtAudioErrorCallback;
    using rtaudio::RtApi;
    using rtaudio::RtAudio;
    using rtaudio::ThreadHandle;
    using rtaudio::StreamMutex;
    using rtaudio::CallbackInfo;
    using rtaudio::S24;
}
