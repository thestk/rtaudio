module;

#include "RtAudio.h"

export module rt.audio;

export namespace rt::audio {
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

#ifndef RTAUDIO_USE_NAMESPACE
using namespace rt::audio;
#endif
