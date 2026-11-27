#include "Audio.hpp"
#include "AudioMixer.hpp"
#include "../Library/PcmMixer.hpp"
#include <array>
#include <limits>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <thread>

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>
#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>
#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#undef STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

namespace snd {
    namespace {
        using Voice = pcmmixer::Voice;
        struct Sound {
            bool Used = false;
            std::shared_ptr<const Pcm> Data;
            std::vector<Voice> Voices;
            int Next = 0;                   // 次に使う重ね鳴らしの枠
            float Gain = 1.0f;
            int VolumeValue = 255;
            bool VolumeSet = false;
            int Hz = 0;                     // 0: 元の周波数
            double Resume = 0;              // fromTop=false の時の開始位置 (フレーム)
            bool Loop = false;
            bool DeleteOnFinish = false;
            std::shared_ptr<std::atomic_bool> Ready;    // 非同期の展開が終わったか
            std::thread Loader;
        };
        std::mutex Lock;                        // Sounds を守る (コールバックとメインスレッド)
        std::vector<Sound> Sounds;
        std::atomic<int> Rate{ 48000 };
        constexpr int MixFrames = 512;

        bool Valid(int h) { return h >= 0 && h < (int)Sounds.size() && Sounds[(size_t)h].Used; }
        int Allocate(std::shared_ptr<const Pcm>& retired) {
            for (int i = 0; i < (int)Sounds.size(); ++i) {
                auto& s = Sounds[(size_t)i];
                if (!s.Used && !s.Loader.joinable()) { retired = std::move(s.Data); s = Sound{}; return i; }
            }
            Sounds.emplace_back();
            return (int)Sounds.size() - 1;
        }

        std::shared_ptr<Pcm> FromInterleaved(float* data, uint64_t frames, int channels, int rate) {
            if (!data || frames == 0 || channels <= 0) { return nullptr; }
            auto p = std::make_shared<Pcm>();
            p->Rate = rate;
            p->Channels = std::min(channels, 2);
            p->Samples.resize((size_t)frames * p->Channels);
            for (uint64_t f = 0; f < frames; ++f) {
                for (int c = 0; c < p->Channels; ++c) { p->Samples[(size_t)(f * p->Channels + c)] = data[f * channels + c]; }
            }
            return p;
        }
        std::shared_ptr<Pcm> DecodeBytes(const unsigned char* bytes, size_t size) {
            if (!bytes || size < 4) { return nullptr; }
            std::shared_ptr<Pcm> out;
            if (std::memcmp(bytes, "OggS", 4) == 0) {
                int channels = 0, rate = 0;
                short* shorts = nullptr;
                const int frames = stb_vorbis_decode_memory(bytes, (int)size, &channels, &rate, &shorts);
                if (frames > 0 && shorts) {
                    out = std::make_shared<Pcm>();
                    out->Rate = rate;
                    out->Channels = std::min(channels, 2);
                    out->Samples.resize((size_t)frames * out->Channels);
                    for (int f = 0; f < frames; ++f) {
                        for (int c = 0; c < out->Channels; ++c) { out->Samples[(size_t)f * out->Channels + c] = shorts[(size_t)f * channels + c] / 32768.0f; }
                    }
                }
                free(shorts);
                return out;
            }
            if (std::memcmp(bytes, "fLaC", 4) == 0) {
                unsigned int ch = 0, rate = 0; drflac_uint64 frames = 0;
                float* data = drflac_open_memory_and_read_pcm_frames_f32(bytes, size, &ch, &rate, &frames, nullptr);
                out = FromInterleaved(data, frames, (int)ch, (int)rate);
                drflac_free(data, nullptr);
                return out;
            }
            if (std::memcmp(bytes, "RIFF", 4) == 0 || std::memcmp(bytes, "RF64", 4) == 0) {
                unsigned int ch = 0, rate = 0; drwav_uint64 frames = 0;
                float* data = drwav_open_memory_and_read_pcm_frames_f32(bytes, size, &ch, &rate, &frames, nullptr);
                out = FromInterleaved(data, frames, (int)ch, (int)rate);
                drwav_free(data, nullptr);
                return out;
            }
            drmp3_config cfg{}; drmp3_uint64 frames = 0;
            float* data = drmp3_open_memory_and_read_pcm_frames_f32(bytes, size, &cfg, &frames, nullptr);
            out = FromInterleaved(data, frames, (int)cfg.channels, (int)cfg.sampleRate);
            drmp3_free(data, nullptr);
            return out;
        }
        std::vector<unsigned char> ReadFile(const std::string& path) {
            std::ifstream in(std::filesystem::path(std::u8string(path.begin(), path.end())), std::ios::binary | std::ios::ate);
            if (!in) { return {}; }
            const auto size = in.tellg();
            if (size <= 0 || (uintmax_t)size > std::numeric_limits<size_t>::max() || (uintmax_t)size > (uintmax_t)std::numeric_limits<std::streamsize>::max()) { return {}; }
            std::vector<unsigned char> bytes((size_t)size);
            in.seekg(0);
            if (!in.read((char*)bytes.data(), (std::streamsize)bytes.size())) { return {}; }
            return bytes;
        }
    }

    namespace mixer {
        void SetOutputRate(int rate) { Rate = rate > 0 ? rate : 48000; }
        int OutputRate() { return Rate; }
        void Render(float* out, int frames) {
            std::fill_n(out, (size_t)frames * 2, 0.0f);
            const int rate = Rate;
            for (int done = 0; done < frames;) {
                const int n = std::min(frames - done, MixFrames);
                float* dst = out + (size_t)done * 2;
                {
                    // ロックは短い区切りごとに離す (その間にメインスレッドが再生・停止・移動できる)
                    std::lock_guard lock(Lock);
                    for (auto& s : Sounds) {
                        if (!s.Used || !s.Data || (s.Ready && !s.Ready->load())) { continue; }
                        const auto& pcm = *s.Data;
                        const double step = (double)(s.Hz > 0 ? s.Hz : pcm.Rate) / rate;
                        bool any = false;
                        for (auto& v : s.Voices) {
                            if (!v.Playing) { continue; }
                            any = true;
                            pcmmixer::Mix(std::span<float>(dst, (size_t)n * 2), pcm.Samples, pcm.Channels, step, s.Gain, s.Loop, v);
                        }
                        if (any && s.DeleteOnFinish && std::none_of(s.Voices.begin(), s.Voices.end(), [](const Voice& v) { return v.Playing; })) {
                            s.Used = false;     // 波形はメインスレッドで消す・使い回す時に解放する
                        }
                    }
                }
                for (int i = 0; i < n * 2; ++i) { dst[i] = std::clamp(dst[i], -1.0f, 1.0f); }
                done += n;
            }
        }
    }

    std::shared_ptr<Pcm> Decode(const std::string& path) {
        const auto bytes = ReadFile(path);
        return DecodeBytes(bytes.data(), bytes.size());
    }
    std::shared_ptr<Pcm> DecodeMemory(const void* data, size_t size) { return DecodeBytes(static_cast<const unsigned char*>(data), size); }

    int Create(std::shared_ptr<const Pcm> pcm, int voices) {
        if (!pcm || pcm->Rate <= 0 || pcm->Channels < 1 || pcm->Channels > 2 || pcm->Samples.size() % pcm->Channels || pcm->Frames() <= 0) { return -1; }
        std::shared_ptr<const Pcm> retired;
        std::lock_guard lock(Lock);
        const int h = Allocate(retired);
        auto& s = Sounds[(size_t)h];
        s.Used = true;
        s.Data = std::move(pcm);
        s.Voices.assign((size_t)std::clamp(voices, 1, 64), Voice{});
        return h;
    }
    int Load(const std::string& path, int voices) { return Create(Decode(path), voices); }
    int LoadMemory(const void* data, size_t size, int voices) { return Create(DecodeMemory(data, size), voices); }
    int LoadAsync(const std::string& path, int voices) {
        std::shared_ptr<const Pcm> retired;
        std::lock_guard lock(Lock);
        const int h = Allocate(retired);
        auto& s = Sounds[(size_t)h];
        s.Used = true;
        s.Voices.assign((size_t)std::clamp(voices, 1, 64), Voice{});
        s.Ready = std::make_shared<std::atomic_bool>(false);
        auto ready = s.Ready;
        s.Loader = std::thread([h, path, ready] {
            std::shared_ptr<Pcm> pcm;
            try { pcm = Decode(path); } catch (...) { /* Publish a failed load instead of terminating the worker. */ }
            std::lock_guard lock(Lock);
            if (Valid(h) && Sounds[(size_t)h].Ready == ready) {
                auto& sound=Sounds[(size_t)h];
                sound.Data=std::move(pcm);
                if (!sound.Data) { for (auto& voice:sound.Voices) { voice.Playing=false; } }
            }
            ready->store(true);
        });
        return h;
    }
    int Duplicate(int handle) {
        std::shared_ptr<const Pcm> data;
        int voices = 1, volume = 255;
        {
            std::lock_guard lock(Lock);
            if (!Valid(handle)) { return -1; }
            data = Sounds[(size_t)handle].Data;
            voices = (int)Sounds[(size_t)handle].Voices.size();
            volume = Sounds[(size_t)handle].VolumeValue;
        }
        const int h = Create(data, voices);
        SetVolume(h, volume);
        return h;
    }
    void Delete(int handle) {
        std::thread loader;
        std::shared_ptr<const Pcm> retired;
        {
            std::lock_guard lock(Lock);
            if (handle < 0 || handle >= (int)Sounds.size()) { return; }
            auto& s = Sounds[(size_t)handle];
            s.Used = false;
            retired = std::move(s.Data);
            s.Ready.reset();
            if (s.Loader.joinable()) { loader = std::move(s.Loader); }
        }
        if (loader.joinable()) { loader.join(); }
    }
    void DeleteAll() { for (int i = 0; i < (int)Sounds.size(); ++i) { Delete(i); } }
    bool IsValid(int handle) { std::lock_guard lock(Lock); return Valid(handle); }
    bool IsReady(int handle) {
        std::lock_guard lock(Lock);
        return Valid(handle) && (!Sounds[(size_t)handle].Ready || Sounds[(size_t)handle].Ready->load());
    }
    void Wait(int handle) {
        std::thread loader;
        {
            std::lock_guard lock(Lock);
            if (!Valid(handle) || !Sounds[(size_t)handle].Loader.joinable()) { return; }
            loader = std::move(Sounds[(size_t)handle].Loader);
        }
        loader.join();
    }
    std::shared_ptr<const Pcm> Data(int handle) { std::lock_guard lock(Lock); return Valid(handle) ? Sounds[(size_t)handle].Data : nullptr; }

    void Play(int handle, bool fromTop, bool loop) {
        std::lock_guard lock(Lock);
        if (!Valid(handle)) { return; }
        auto& s = Sounds[(size_t)handle];
        if (!s.Data && (!s.Ready || s.Ready->load())) { return; }
        s.Loop = loop;
        auto& v = s.Voices[(size_t)s.Next];
        s.Next = (s.Next + 1) % (int)s.Voices.size();
        v.Pos = fromTop ? 0.0 : s.Resume;
        v.Playing = true;
    }
    void Stop(int handle) {
        std::lock_guard lock(Lock);
        if (!Valid(handle)) { return; }
        auto& s = Sounds[(size_t)handle];
        for (auto& v : s.Voices) {
            if (v.Playing) { s.Resume = v.Pos; }
            v.Playing = false;
        }
    }
    bool IsPlaying(int handle) {
        std::lock_guard lock(Lock);
        return Valid(handle) && std::any_of(Sounds[(size_t)handle].Voices.begin(), Sounds[(size_t)handle].Voices.end(), [](const Voice& v) { return v.Playing; });
    }
    void SetDeleteOnFinish(int handle, bool on) { std::lock_guard lock(Lock); if (Valid(handle)) { Sounds[(size_t)handle].DeleteOnFinish = on; } }

    void SetVolume(int handle, int volume) {
        std::lock_guard lock(Lock);
        if (!Valid(handle)) { return; }
        auto& s = Sounds[(size_t)handle];
        volume = std::clamp(volume, 0, 255);
        if (s.VolumeSet && s.VolumeValue == volume) { return; }
        s.VolumeValue = volume;
        s.VolumeSet = true;
        // DxLib と同じ大きさ: 出力の音を測って求めた 0.5 × (音量 / 255)^2.5
        // (DxLib は全体を -6dB で出し、音量は 0～255 を 2.5 乗の目盛りで換算する。音量 100%・62%・30% で一致を確かめた)
        s.Gain = (float)(0.5 * std::pow(s.VolumeValue / 255.0, 2.5));
    }
    int Volume(int handle) { std::lock_guard lock(Lock); return Valid(handle) ? Sounds[(size_t)handle].VolumeValue : 0; }
    void SetFrequency(int handle, int hz) { std::lock_guard lock(Lock); if (Valid(handle)) { Sounds[(size_t)handle].Hz = std::max(hz, 0); } }
    int Frequency(int handle) {
        std::lock_guard lock(Lock);
        if (!Valid(handle)) { return 0; }
        const auto& s = Sounds[(size_t)handle];
        return s.Hz > 0 ? s.Hz : (s.Data ? s.Data->Rate : 0);
    }
    int OriginalFrequency(int handle) { std::lock_guard lock(Lock); return Valid(handle) && Sounds[(size_t)handle].Data ? Sounds[(size_t)handle].Data->Rate : 0; }
    void SetPositionMs(int handle, double ms) {
        std::lock_guard lock(Lock);
        if (!Valid(handle) || !Sounds[(size_t)handle].Data) { return; }
        auto& s = Sounds[(size_t)handle];
        const double frame = std::max(ms, 0.0) * s.Data->Rate / 1000.0;
        s.Resume = frame;
        for (auto& v : s.Voices) { if (v.Playing) { v.Pos = frame; } }
    }
    double PositionMs(int handle) {
        std::lock_guard lock(Lock);
        if (!Valid(handle) || !Sounds[(size_t)handle].Data) { return 0; }
        const auto& s = Sounds[(size_t)handle];
        double pos = s.Resume;
        for (const auto& v : s.Voices) { if (v.Playing) { pos = std::max(pos, v.Pos); } }
        return pos * 1000.0 / s.Data->Rate;
    }
    double TotalMs(int handle) {
        std::lock_guard lock(Lock);
        return Valid(handle) && Sounds[(size_t)handle].Data && Sounds[(size_t)handle].Data->Rate > 0 ? Sounds[(size_t)handle].Data->Frames() * 1000.0 / Sounds[(size_t)handle].Data->Rate : 0.0;
    }
    int64_t TotalFrames(int handle) { std::lock_guard lock(Lock); return Valid(handle) && Sounds[(size_t)handle].Data ? Sounds[(size_t)handle].Data->Frames() : 0; }
}
