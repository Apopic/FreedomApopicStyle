// 音声の出力 (WASAPI)。専用のスレッドがイベント駆動で機器のバッファを埋める。
// 排他モード: 機器の最小の周期まで縮められる (Windows のミキサーを通らない)。
// 共有モード: IAudioClient3 で使える最小の周期にする (使えなければ通常の共有モード)。
// 既定の出力機器が変わったり抜かれたりしたら、同じ設定で開き直す。
#include "../Audio.hpp"
#include "../AudioMixer.hpp"
#include "WinInternal.hpp"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "ole32.lib")

using Microsoft::WRL::ComPtr;

namespace snd {
    namespace {
        enum class SampleType { Float32, Int32, Int24In32, Int16 };
        struct OutputFormat { int Rate = 48000, Channels = 2; SampleType Type = SampleType::Float32; int BlockAlign = 8; };

        DeviceOptions Options;
        std::thread Worker;
        std::atomic_bool Running = false, Reopen = false, Opened = false;
        HANDLE ReadyEvent = nullptr;
        std::mutex DescriptionLock;
        std::string Description;
        std::atomic<int> CurrentRate{ 48000 };

        // 既定の出力機器が変わったことを知る
        class DeviceWatcher final : public IMMNotificationClient {
        public:
            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
                if (id == __uuidof(IUnknown) || id == __uuidof(IMMNotificationClient)) { *out = this; return S_OK; }
                *out = nullptr;
                return E_NOINTERFACE;
            }
            ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
            ULONG STDMETHODCALLTYPE Release() override { return 1; }
            HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
                if (flow == eRender && role == eConsole) { Reopen = true; }
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
            HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
            HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return S_OK; }
            HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }
        } Watcher;

        WAVEFORMATEXTENSIBLE MakeFormat(int rate, int channels, SampleType type) {
            WAVEFORMATEXTENSIBLE f{};
            const int container = type == SampleType::Int16 ? 16 : 32;
            f.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            f.Format.nChannels = (WORD)channels;
            f.Format.nSamplesPerSec = (DWORD)rate;
            f.Format.wBitsPerSample = (WORD)container;
            f.Format.nBlockAlign = (WORD)(channels * container / 8);
            f.Format.nAvgBytesPerSec = f.Format.nSamplesPerSec * f.Format.nBlockAlign;
            f.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
            f.Samples.wValidBitsPerSample = (WORD)(type == SampleType::Int24In32 ? 24 : container);
            f.dwChannelMask = channels == 1 ? SPEAKER_FRONT_CENTER : (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
            f.SubFormat = type == SampleType::Float32 ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;
            return f;
        }
        bool Describe(const WAVEFORMATEX* w, OutputFormat& out) {
            out.Rate = (int)w->nSamplesPerSec;
            out.Channels = w->nChannels;
            out.BlockAlign = w->nBlockAlign;
            bool isFloat = w->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
            int valid = w->wBitsPerSample;
            if (w->wFormatTag == WAVE_FORMAT_EXTENSIBLE && w->cbSize >= 22) {
                const auto* e = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(w);
                isFloat = IsEqualGUID(e->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != FALSE;
                if (!isFloat && !IsEqualGUID(e->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) { return false; }
                valid = e->Samples.wValidBitsPerSample ? e->Samples.wValidBitsPerSample : w->wBitsPerSample;
            }
            else if (w->wFormatTag != WAVE_FORMAT_PCM && !isFloat) { return false; }
            if (isFloat && w->wBitsPerSample == 32) { out.Type = SampleType::Float32; }
            else if (!isFloat && w->wBitsPerSample == 32) { out.Type = valid == 24 ? SampleType::Int24In32 : SampleType::Int32; }
            else if (!isFloat && w->wBitsPerSample == 16) { out.Type = SampleType::Int16; }
            else { return false; }
            return out.Channels >= 1;
        }
        const char* TypeName(SampleType t) {
            switch (t) {
            case SampleType::Float32: return "float32";
            case SampleType::Int32: return "int32";
            case SampleType::Int24In32: return "int24";
            default: return "int16";
            }
        }
        // ミキサーの音 (ステレオ float) を機器の形式に直して書く
        void Write(BYTE* dst, const float* src, int frames, const OutputFormat& f) {
            const int ch = f.Channels;
            for (int i = 0; i < frames; ++i) {
                const float l = src[i * 2], r = src[i * 2 + 1];
                for (int c = 0; c < ch; ++c) {
                    const float v = ch == 1 ? (l + r) * 0.5f : c == 0 ? l : c == 1 ? r : 0.0f;
                    switch (f.Type) {
                    case SampleType::Float32: reinterpret_cast<float*>(dst)[c] = v; break;
                    case SampleType::Int32: reinterpret_cast<int32_t*>(dst)[c] = (int32_t)std::lround(std::clamp(v, -1.0f, 1.0f) * 2147483520.0f); break;
                    case SampleType::Int24In32: reinterpret_cast<int32_t*>(dst)[c] = (int32_t)std::lround(std::clamp(v, -1.0f, 1.0f) * 8388607.0f) << 8; break;
                    case SampleType::Int16: reinterpret_cast<int16_t*>(dst)[c] = (int16_t)std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f); break;
                    }
                }
                dst += f.BlockAlign;
            }
        }

        struct Stream {
            ComPtr<IAudioClient> Client;
            ComPtr<IAudioRenderClient> Render;
            HANDLE Event = nullptr;
            OutputFormat Format;
            UINT32 BufferFrames = 0;
            bool Exclusive = false;
            ~Stream() { if (Client) { Client->Stop(); } if (Event) { CloseHandle(Event); } }
        };

        bool OpenExclusive(IMMDevice* device, Stream& s) {
            ComPtr<IAudioClient> client;
            if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client))) { return false; }
            WAVEFORMATEX* mix = nullptr;
            int mixRate = 48000;
            if (SUCCEEDED(client->GetMixFormat(&mix)) && mix) { mixRate = (int)mix->nSamplesPerSec; CoTaskMemFree(mix); }
            // 指定のサンプリングレート → Windows の設定のレートの順に、機器が受け付ける形式を探す
            WAVEFORMATEXTENSIBLE chosen{};
            bool found = false;
            for (int rate : { Options.Rate, mixRate }) {
                for (SampleType t : { SampleType::Float32, SampleType::Int32, SampleType::Int24In32, SampleType::Int16 }) {
                    WAVEFORMATEXTENSIBLE f = MakeFormat(rate, 2, t);
                    if (client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &f.Format, nullptr) == S_OK) { chosen = f; found = true; break; }
                }
                if (found) { break; }
            }
            if (!found) { return false; }
            REFERENCE_TIME def = 0, minimum = 0;
            client->GetDevicePeriod(&def, &minimum);
            const int rate = (int)chosen.Format.nSamplesPerSec;
            REFERENCE_TIME period = Options.BufferFrames > 0 ? (REFERENCE_TIME)std::llround(Options.BufferFrames * 10000000.0 / rate) : minimum;
            period = std::max(period, minimum);
            HRESULT hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period, period, &chosen.Format, nullptr);
            if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
                // 機器の境界に合わせた周期で作り直す
                UINT32 frames = 0;
                client->GetBufferSize(&frames);
                period = (REFERENCE_TIME)std::llround(frames * 10000000.0 / rate);
                client.Reset();
                if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client))) { return false; }
                hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period, period, &chosen.Format, nullptr);
            }
            if (FAILED(hr)) { return false; }
            s.Client = client;
            s.Exclusive = true;
            Describe(&chosen.Format, s.Format);
            return true;
        }
        bool OpenShared(IMMDevice* device, Stream& s) {
            WAVEFORMATEX* mix = nullptr;
            ComPtr<IAudioClient3> client3;
            ComPtr<IAudioClient> client;
            if (SUCCEEDED(device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr, &client3)) && SUCCEEDED(client3->GetMixFormat(&mix)) && mix) {
                UINT32 def = 0, fundamental = 0, minimum = 0, maximum = 0;
                if (SUCCEEDED(client3->GetSharedModeEnginePeriod(mix, &def, &fundamental, &minimum, &maximum)) && fundamental > 0) {
                    // 指定の大きさ (無ければ最小) を、使える範囲と刻みに合わせる
                    UINT32 want = Options.BufferFrames > 0 ? (UINT32)std::llround((double)Options.BufferFrames * mix->nSamplesPerSec / std::max(Options.Rate, 1)) : minimum;
                    want = std::clamp(want, minimum, maximum);
                    want = std::max(minimum, (want + fundamental - 1) / fundamental * fundamental);
                    want = std::min(want, maximum);
                    if (SUCCEEDED(client3->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, want, mix, nullptr))) { client = client3; }
                }
                if (!client) { CoTaskMemFree(mix); mix = nullptr; client3.Reset(); }
            }
            if (!client) {
                if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client)) || FAILED(client->GetMixFormat(&mix)) || !mix) { return false; }
                REFERENCE_TIME def = 0, minimum = 0;
                client->GetDevicePeriod(&def, &minimum);
                if (FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, def, 0, mix, nullptr))) { CoTaskMemFree(mix); return false; }
            }
            const bool ok = Describe(mix, s.Format);
            CoTaskMemFree(mix);
            if (!ok) { return false; }
            s.Client = client;
            s.Exclusive = false;
            return true;
        }
        bool Open(Stream& s) {
            ComPtr<IMMDeviceEnumerator> enumerator;
            ComPtr<IMMDevice> device;
            if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) { return false; }
            if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))) { return false; }
            if (!(Options.Exclusive && OpenExclusive(device.Get(), s)) && !OpenShared(device.Get(), s)) { return false; }
            s.Event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!s.Event || FAILED(s.Client->SetEventHandle(s.Event)) || FAILED(s.Client->GetBufferSize(&s.BufferFrames))
                || FAILED(s.Client->GetService(IID_PPV_ARGS(&s.Render)))) { return false; }
            mixer::SetOutputRate(s.Format.Rate);
            CurrentRate = s.Format.Rate;
            // 最初のバッファを無音以外で埋めてから始める
            BYTE* data = nullptr;
            const UINT32 first = s.Exclusive ? s.BufferFrames : s.BufferFrames;
            if (SUCCEEDED(s.Render->GetBuffer(first, &data))) {
                std::vector<float> buf((size_t)first * 2);
                mixer::Render(buf.data(), (int)first);
                Write(data, buf.data(), (int)first, s.Format);
                s.Render->ReleaseBuffer(first, 0);
            }
            if (FAILED(s.Client->Start())) { return false; }
            char text[160];
            std::snprintf(text, sizeof(text), "WASAPI %s %dHz %s %dch %.1fms", s.Exclusive ? "排他" : "共有", s.Format.Rate, TypeName(s.Format.Type), s.Format.Channels,
                s.BufferFrames * 1000.0 / s.Format.Rate);
            std::lock_guard lock(DescriptionLock);
            Description = text;
            return true;
        }

        void RunDevice() {
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            DWORD taskIndex = 0;
            HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
            if (task) { AvSetMmThreadPriority(task, AVRT_PRIORITY_CRITICAL); }
            else { SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL); }
            ComPtr<IMMDeviceEnumerator> enumerator;
            if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) { enumerator->RegisterEndpointNotificationCallback(&Watcher); }
            std::vector<float> buf;
            bool first = true;
            while (Running) {
                Reopen = false;
                {
                    Stream s;
                    const bool ok = Open(s);
                    Opened = ok;
                    if (first) { first = false; SetEvent(ReadyEvent); }
                    if (ok) {
                        buf.resize((size_t)s.BufferFrames * 2);
                        while (Running && !Reopen) {
                            if (WaitForSingleObject(s.Event, 200) != WAIT_OBJECT_0) { continue; }
                            UINT32 frames = s.BufferFrames;
                            if (!s.Exclusive) {
                                UINT32 padding = 0;
                                if (FAILED(s.Client->GetCurrentPadding(&padding))) { break; }
                                frames = s.BufferFrames - padding;
                            }
                            if (frames == 0) { continue; }
                            BYTE* data = nullptr;
                            if (FAILED(s.Render->GetBuffer(frames, &data))) { break; }    // 機器が無くなった時など: 開き直す
                            mixer::Render(buf.data(), (int)frames);
                            Write(data, buf.data(), (int)frames, s.Format);
                            s.Render->ReleaseBuffer(frames, 0);
                        }
                    }
                }
                // 開けなかった・途切れた時は少し待ってから開き直す
                for (int i = 0; i < 10 && Running && !Reopen; ++i) { Sleep(50); }
            }
            if (enumerator) { enumerator->UnregisterEndpointNotificationCallback(&Watcher); }
            if (task) { AvRevertMmThreadCharacteristics(task); }
            CoUninitialize();
        }
    }

    namespace {
        void Run() {
            try { RunDevice(); }
            catch (...) { Opened = false; }
            if (ReadyEvent) { SetEvent(ReadyEvent); }     // 初めて開く前に止まった時も Init を待たせない
        }
    }

    bool Init(const DeviceOptions& o) {
        if (Running) { return Opened; }
        Options = o;
        if (Options.Rate <= 0) { Options.Rate = 48000; }
        mixer::SetOutputRate(Options.Rate);
        ReadyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Running = true;
        try { Worker = std::thread(Run); }
        catch (...) { Running = false; CloseHandle(ReadyEvent); ReadyEvent = nullptr; return false; }
        // 最初に開けたかどうかが分かるまで待つ (失敗しても後で開き直し続ける)
        WaitForSingleObject(ReadyEvent, 5000);
        return true;
    }
    void Shutdown() {
        if (Running) {
            Running = false;
            if (Worker.joinable()) { Worker.join(); }
        }
        if (ReadyEvent) { CloseHandle(ReadyEvent); ReadyEvent = nullptr; }
        Opened = false;
        DeleteAll();
    }
    bool IsInit() { return Running; }
    int DeviceRate() { return CurrentRate; }
    const char* DeviceDescription() {
        static thread_local std::string copy;
        std::lock_guard lock(DescriptionLock);
        copy = Description;
        return copy.c_str();
    }
}
