module;

#include "RtAudio.h"

export module rt.audio;

export
#ifdef RTAUDIO_USE_NAMESPACE
inline namespace rt {
inline namespace audio {
#else
namespace rt::audio {
#endif
    using rt::audio::RtAudioFormat;
    using rt::audio::RtAudioStreamFlags;
    using rt::audio::RtAudioStreamStatus;
    using rt::audio::RtAudioCallback;
    using rt::audio::RtAudioErrorType;
    using rt::audio::RtAudioErrorCallback;
    using rt::audio::RtApi;
    using rt::audio::RtAudio;
    using rt::audio::ThreadHandle;
    using rt::audio::StreamMutex;
    using rt::audio::CallbackInfo;
    using rt::audio::S24;
}
#ifdef RTAUDIO_USE_NAMESPACE
}
#endif
