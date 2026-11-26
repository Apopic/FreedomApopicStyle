#pragma once
#include "DxLib.h"

class SoundDevice final {
public:

    SoundDevice() = default;
    ~SoundDevice() = default;

    SoundDevice(const SoundDevice&) = delete;
    SoundDevice& operator=(const SoundDevice&) = delete;
    SoundDevice(SoundDevice&&) = delete;
    SoundDevice& operator=(SoundDevice&&) = delete;

    static SoundDevice& GetInstance() {
        static SoundDevice instance;
        return instance;
    }

    enum SoundDeviceType : int {
        None = 0,
        WASAPI,
        XAudio,
        MMEwaveOut,
        ASIO
    };

#ifndef __ANDROID__
    void SetDevice(int type, bool exclusive, int bufsize, int samplerate) {

        if (samplerate <= 0) { samplerate = 48000; }
        if (bufsize <= 0) { bufsize = 480; }
        int ns = static_cast<int>(10000000LL * bufsize / samplerate);
        switch (type) {
        case SoundDeviceType::WASAPI:
            SetEnableWASAPIFlag(TRUE, exclusive, ns, samplerate);
            break;
        case SoundDeviceType::XAudio:
            SetEnableXAudioFlag(TRUE);
            break;
        case SoundDeviceType::MMEwaveOut:
            SetEnableMMEwaveOutFlag(TRUE, bufsize, samplerate);
            break;
        case SoundDeviceType::ASIO:
            SetEnableASIOFlag(TRUE, bufsize, samplerate);
            break;
        default:
            SetDevice(SoundDeviceType::WASAPI, exclusive, bufsize, samplerate);
            break;
        }
    }
#endif
};
