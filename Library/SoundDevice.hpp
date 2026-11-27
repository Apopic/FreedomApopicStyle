#pragma once
#include "../Platform/Audio.hpp"

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

    // 音声の出力を開く。Windows は WASAPI (exclusive なら排他モード、使えなければ共有モード) で、既定の出力機器に出す。
    // 出力方式の種類 (XAudio・MME・ASIO) は WASAPI にまとめ、バッファの大きさ (遅延) とサンプリングレートを使う
    bool SetDevice(int type, bool exclusive, int bufsize, int samplerate) {
        (void)type;
        if (samplerate <= 0) { samplerate = 48000; }
        if (bufsize <= 0) { bufsize = 480; }
        return snd::Init({ samplerate, bufsize, exclusive });
    }
};
