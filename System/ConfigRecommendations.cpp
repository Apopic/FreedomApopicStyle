#include "../Game.hpp"

auto Game::DetectDevice() -> DeviceInfo {
    DeviceInfo dev;
    dev.Threads = std::max(1u, std::thread::hardware_concurrency());
#ifdef __ANDROID__
    if (int rate = GetRefreshRate(); rate > 0) { dev.RefreshRate = rate; }
#endif
#ifndef __ANDROID__
    HMONITOR monitor = MonitorFromWindow(((HWND)sys::NativeWindow()), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (GetMonitorInfoW(monitor, reinterpret_cast<MONITORINFO*>(&info)) && EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1) {
        dev.RefreshRate = (int)mode.dmDisplayFrequency;
    }
    // 音声デバイスの問い合わせは COM の状態を変えないよう別スレッドで行う
    std::thread([&dev] {
        const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        IMMDeviceEnumerator* enumerator = nullptr;
        IMMDevice* device = nullptr;
        IAudioClient* client = nullptr;
        WAVEFORMATEX* format = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator))
            && SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))
            && SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client))
            && SUCCEEDED(client->GetMixFormat(&format)) && format != nullptr) {
            REFERENCE_TIME period = 0, minimum = 0;
            dev.MixRate = (int)format->nSamplesPerSec;
            if (SUCCEEDED(client->GetDevicePeriod(&period, &minimum)) && period > 0) {
                dev.PeriodSamples = (int)((int64_t)period * dev.MixRate / 10000000);
            }
            dev.AudioOk = dev.MixRate > 0;
        }
        if (format) { CoTaskMemFree(format); }
        if (client) { client->Release(); }
        if (device) { device->Release(); }
        if (enumerator) { enumerator->Release(); }
        if (SUCCEEDED(init)) { CoUninitialize(); }
        }).join();
#endif
    return dev;
}

auto Game::BuildRecommendations() -> std::vector<RecommendChange> {
    std::vector<RecommendChange> out;
    const DeviceInfo dev = DetectDevice();
    auto Boolean = [&](const char* name, bool& v, bool want, bool restart) {
        if (v != want) { out.push_back({ name, v ? "ON" : "OFF", want ? "ON" : "OFF", restart, [&v, want] { v = want; } }); }
    };
    auto Integer = [&](const char* name, int& v, int want, bool restart, std::function<std::string(int)> text) {
        if (v != want) { out.push_back({ name, text(v), text(want), restart, [&v, want] { v = want; } }); }
    };
    auto Real = [&](const char* name, double& v, double want, int decimals) {
        if (std::abs(v - want) > 1e-9) { out.push_back({ name, FormatNumber(v, decimals), FormatNumber(want, decimals), false, [&v, want] { v = want; } }); }
    };
    auto Plain = [](int v) { return std::to_string(v); };

    // 表示: 垂直同期 + 高速入力 (打鍵は Raw Input の時刻で判定するので、描画は垂直同期に合わせて滑らかにする)。
    // 描画開始の割合は CPU に余裕が無いほど早めにして、垂直同期に間に合わないコマ落ちを防ぐ
#ifdef __ANDROID__
    Boolean("垂直同期", Config.WaitVSync, true, false);
    Boolean("高速入力", Config.FastInput, false, false);
    Boolean("フレーム単位の時間", Config.FrameCountTimer, false, false);
    Real("描画タイミング", Config.FastDrawRate, 0.0, 2);
#else
    Boolean("垂直同期", Config.WaitVSync, true, true);
    Boolean("高速入力", Config.FastInput, true, false);
    Boolean("フレーム単位の時間", Config.FrameCountTimer, true, false);
    Real("描画タイミング", Config.FastDrawRate, dev.Threads <= 4 || dev.RefreshRate >= 200 ? 0.3 : 0.5, 2);
#endif

#ifndef __ANDROID__
    // サウンド: WASAPI 共有モードで、デバイスのサンプリングレートと周期に合わせる (変換と余分なバッファを避ける)
    if (dev.AudioOk) {
        static const int rates[] = { 44100, 48000, 96000, 192000 };
        const int rate = std::ranges::find(rates, dev.MixRate) != std::end(rates) ? dev.MixRate : 48000;
        const int period = dev.PeriodSamples > 0 ? dev.PeriodSamples : rate / 100;
        const int buffer = std::clamp((period + 31) / 32 * 32, 128, 2048);
        Integer("出力方式", Config.SoundDeviceType, 1, true, [](int v) {
            static const char* names[] = { "既定", "WASAPI", "XAudio2", "MME (waveOut)", "ASIO" };
            return std::string(v >= 0 && v < 5 ? names[v] : "?");
            });
        Boolean("排他モード", Config.Exclusive, false, true);
        Integer("サンプリングレート", Config.SampleRate, rate, true, [](int v) { return std::to_string(v) + " Hz"; });
        Integer("バッファサイズ", Config.BufferSize, buffer, true, Plain);
        Real("判定オフセット", Config.JudgeOffset, RecommendedJudgeOffset(dev, 1, false, buffer, rate), 0);
    }
    else {
        Real("判定オフセット", Config.JudgeOffset, RecommendedJudgeOffset(dev, Config.SoundDeviceType, Config.Exclusive, Config.BufferSize, Config.SampleRate), 0);
    }
#endif
    return out;
}
