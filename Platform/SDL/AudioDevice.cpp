#include "../Audio.hpp"
#include "../AudioMixer.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <string>

// 音声の出力 (SDL3 の AudioStream)。Android で使う
namespace snd {
    namespace {
        SDL_AudioStream* Stream = nullptr;
        std::string Description;
        void Callback(void*, SDL_AudioStream* stream, int additional, int) {
            std::array<float, 1024 * 2> buf;
            int remaining = additional / (int)(sizeof(float) * 2);
            while (remaining > 0) {
                const int frames = std::min(remaining, 1024);
                mixer::Render(buf.data(), frames);
                if (!SDL_PutAudioStreamData(stream, buf.data(), frames * 2 * (int)sizeof(float))) { break; }
                remaining -= frames;
            }
        }
    }
    bool Init(const DeviceOptions& o) {
        if (Stream) { return true; }
        if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) { return false; }
        if (o.BufferFrames > 0) { SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, std::to_string(o.BufferFrames).c_str()); }
        const int rate = o.Rate > 0 ? o.Rate : 48000;
        mixer::SetOutputRate(rate);
        const SDL_AudioSpec spec{ SDL_AUDIO_F32, 2, rate };
        Stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Callback, nullptr);
        if (!Stream) { return false; }
        if (!SDL_ResumeAudioStreamDevice(Stream)) { SDL_DestroyAudioStream(Stream); Stream = nullptr; return false; }
        Description = "SDL " + std::to_string(rate) + "Hz";
        return true;
    }
    void Shutdown() {
        if (Stream) { SDL_DestroyAudioStream(Stream); Stream = nullptr; }
        DeleteAll();
    }
    bool IsInit() { return Stream != nullptr; }
    int DeviceRate() { return mixer::OutputRate(); }
    const char* DeviceDescription() { return Description.c_str(); }
}
