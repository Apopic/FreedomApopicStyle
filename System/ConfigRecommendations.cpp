#include "../Game.hpp"
#ifndef __ANDROID__
#include <dxgi.h>
#include <ksmedia.h>
#pragma comment(lib, "dxgi.lib")
#endif

#ifndef __ANDROID__
namespace {
    // ffmpeg でごく短い映像を実際にエンコードして、ハードウェアエンコーダが使えるか確かめる (ドライバや GPU が対応していないと失敗する)
    bool EncoderWorks(const fs::path& ffmpeg, const std::wstring& codec) {
        const std::vector<std::wstring> args = { ffmpeg.wstring(), L"-hide_banner", L"-loglevel", L"error", L"-f", L"lavfi", L"-i", L"color=c=black:s=320x180:r=30:d=0.2",
            L"-pix_fmt", L"yuv420p", L"-c:v", codec, L"-f", L"null", L"-" };
        PROCESS_INFORMATION pi{};
        if (!rec::Spawn(args, pi, nullptr) || !pi.hProcess) { if (pi.hThread) { CloseHandle(pi.hThread); } return false; }
        DWORD code = 1;
        if (WaitForSingleObject(pi.hProcess, 8000) != WAIT_OBJECT_0) { TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 2000); }
        else { GetExitCodeProcess(pi.hProcess, &code); }
        CloseHandle(pi.hProcess);
        if (pi.hThread) { CloseHandle(pi.hThread); }
        return code == 0;
    }
}
#endif

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
    // 音声デバイスと GPU の問い合わせは COM の状態を変えないよう別スレッドで行う
    std::thread([&dev] {
        const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        IMMDeviceEnumerator* enumerator = nullptr;
        IMMDevice* device = nullptr;
        IAudioClient* client = nullptr;
        IAudioClient3* client3 = nullptr;
        WAVEFORMATEX* format = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator))
            && SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))
            && SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client))
            && SUCCEEDED(client->GetMixFormat(&format)) && format != nullptr) {
            REFERENCE_TIME period = 0, minimum = 0;
            dev.MixRate = (int)format->nSamplesPerSec;
            if (SUCCEEDED(client->GetDevicePeriod(&period, &minimum)) && period > 0) {
                dev.PeriodSamples = (int)((int64_t)period * dev.MixRate / 10000000);
                dev.MinPeriodSamples = (int)((int64_t)minimum * dev.MixRate / 10000000);
            }
            // 共有モードで縮められる周期 (Windows 10 以降の低遅延の共有モード。対応しない機器は既定の周期だけ)
            if (SUCCEEDED(device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr, (void**)&client3))) {
                UINT32 def = 0, fundamental = 0, smallest = 0, largest = 0;
                if (SUCCEEDED(client3->GetSharedModeEnginePeriod(format, &def, &fundamental, &smallest, &largest)) && smallest > 0) {
                    dev.SharedMinPeriodSamples = (int)smallest;
                }
            }
            // 排他モードで Windows の設定と同じサンプリングレートが使えるか (ゲームの WASAPI と同じ順に形式を試す)
            for (const auto [bits, valid, isfloat] : { std::tuple{ 32, 32, true }, std::tuple{ 32, 32, false }, std::tuple{ 32, 24, false }, std::tuple{ 16, 16, false } }) {
                WAVEFORMATEXTENSIBLE f{};
                f.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
                f.Format.nChannels = 2;
                f.Format.nSamplesPerSec = (DWORD)dev.MixRate;
                f.Format.wBitsPerSample = (WORD)bits;
                f.Format.nBlockAlign = (WORD)(2 * bits / 8);
                f.Format.nAvgBytesPerSec = f.Format.nSamplesPerSec * f.Format.nBlockAlign;
                f.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
                f.Samples.wValidBitsPerSample = (WORD)valid;
                f.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
                f.SubFormat = isfloat ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;
                if (client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &f.Format, nullptr) == S_OK) { dev.ExclusiveOk = true; break; }
            }
            dev.AudioOk = dev.MixRate > 0;
        }
        if (format) { CoTaskMemFree(format); }
        if (client3) { client3->Release(); }
        if (client) { client->Release(); }
        if (device) { device->Release(); }
        if (enumerator) { enumerator->Release(); }
        // 録画のエンコーダを選ぶために、使っている GPU の会社を調べる (最初のアダプター = 既定の GPU)
        IDXGIFactory1* factory = nullptr;
        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory))) {
            IDXGIAdapter1* adapter = nullptr;
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(factory->EnumAdapters1(0, &adapter)) && SUCCEEDED(adapter->GetDesc1(&desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                dev.GpuVendor = desc.VendorId;
            }
            if (adapter) { adapter->Release(); }
            factory->Release();
        }
        if (SUCCEEDED(init)) { CoUninitialize(); }
        }).join();
    // ハードウェアエンコーダ: GPU の会社のものを実際に試す (使えなければ libx264 のまま)
    const fs::path ffmpeg = rec::FindFfmpeg(GetExecutablePath());
    if (!ffmpeg.empty()) {
        const wchar_t* candidate = dev.GpuVendor == 0x10DE ? L"h264_nvenc" : dev.GpuVendor == 0x1002 ? L"h264_amf" : dev.GpuVendor == 0x8086 ? L"h264_qsv" : nullptr;
        if (candidate && EncoderWorks(ffmpeg, candidate)) { dev.HwEncoder = PathToU8(fs::path(candidate)); }
    }
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

    // ---- 表示 ----
    // 垂直同期 + 高速入力 (打鍵は Raw Input の時刻で判定するので、描画は垂直同期に合わせて滑らかにする)。FPS の上限は付けない (垂直同期に任せる)。
    // 描画開始の割合は CPU に余裕が無いほど・リフレッシュレートが高いほど早めにして、垂直同期に間に合わないコマ落ちを防ぐ
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
    Integer("目標 FPS", Config.FrameRate, 0, false, [](int v) { return v == 0 ? std::string("制限なし") : std::to_string(v); });
    Real("フレーム補間の倍率", Config.FrameExtendRate, 1.0, 2);

#ifndef __ANDROID__
    // ---- サウンド ----
    // WASAPI で、デバイスのサンプリングレートに合わせる (変換を避ける)。バッファは機器が受け付ける最小の周期にする:
    //   排他モード: デバイスの最小周期 / 共有モード: 低遅延の共有モードの最小周期 (対応しない機器は既定の周期)
    // 排他モードは他のアプリの音が止まるので、使えない機器の時だけ OFF にし、使える時は今の設定のままにする
    if (dev.AudioOk) {
        static const int rates[] = { 44100, 48000, 88200, 96000, 176400, 192000 };
        const int rate = std::ranges::find(rates, dev.MixRate) != std::end(rates) ? dev.MixRate : 48000;
        const bool exclusive = Config.Exclusive && dev.ExclusiveOk;
        int period = exclusive ? dev.MinPeriodSamples : dev.SharedMinPeriodSamples;
        if (period <= 0) { period = dev.PeriodSamples > 0 ? dev.PeriodSamples : rate / 100; }
        period = (int)std::lround((double)period * rate / std::max(dev.MixRate, 1));
        const int buffer = std::clamp(period, Config.MinBufferSize, 4096);
        Integer("出力方式", Config.SoundDeviceType, 1, true, [](int v) {
            static const char* names[] = { "既定", "WASAPI", "XAudio2", "MME (waveOut)", "ASIO" };
            return std::string(v >= 0 && v < 5 ? names[v] : "?");
            });
        Boolean("排他モード", Config.Exclusive, exclusive, true);
        Integer("サンプリングレート", Config.SampleRate, rate, true, [](int v) { return std::to_string(v) + " Hz"; });
        Integer("バッファサイズ", Config.BufferSize, buffer, true, [rate](int v) { return std::to_string(v) + " (" + FormatNumber(v * 1000.0 / rate, 1) + "ms)"; });
    }

    // ---- 録画 ----
    // GPU のハードウェアエンコーダが使えるなら CPU を使わないそれにする (演奏中のコマ落ちを防ぐ)。
    // フレームレートは、ハードウェアエンコーダか 8 スレッド以上の CPU ならリフレッシュレートまで (最大 120)、それ以外は 60
    if (!dev.HwEncoder.empty()) {
        if (Config.RecordCodec != dev.HwEncoder) {
            const std::string want = dev.HwEncoder;
            out.push_back({ "録画のエンコーダ", Config.RecordCodec, want, false, [this, want] { Config.RecordCodec = want; } });
        }
    }
    else if (Config.RecordCodec != "libx264" && !rec::FindFfmpeg(GetExecutablePath()).empty() && !EncoderWorks(rec::FindFfmpeg(GetExecutablePath()), std::wstring(Config.RecordCodec.begin(), Config.RecordCodec.end()))) {
        out.push_back({ "録画のエンコーダ", Config.RecordCodec, "libx264", false, [this] { Config.RecordCodec = "libx264"; } });
    }
    {
        const bool fast = !dev.HwEncoder.empty() || dev.Threads >= 8;
        const int fps = fast ? (dev.RefreshRate >= 120 ? 120 : 60) : (dev.Threads <= 4 ? 30 : 60);
        Integer("録画のフレームレート", Config.RecordFps, fps, false, [](int v) { return std::to_string(v) + " fps"; });
    }
    Boolean("SDL GPU 描画 (SDL3 版のみ)", Config.UseGpuRenderer, false, true);
#endif

    // ---- 判定オフセット ----
    // 音・映像・キーの遅れと叩く癖は人と機器で違い、バッファの長さからは決まらない。実際の演奏で測ったずれ (オート以外の演奏とオフセット自動調整) の
    // 平均がある時だけ、それを中心にする値にする。測った数が少ない時は変えない (設定画面の「オフセット自動調整」で測れる)
    if (Config.PersonalOffsetCount >= MinPersonalOffsetHits) {
        Real("判定オフセット", Config.JudgeOffset, std::clamp(std::round(Config.PersonalOffset), -500.0, 500.0), 0);
    }
    return out;
}

void Game::AddPersonalOffsetSample(double offset, size_t count) {
    // 新しい演奏ほど重く (古い分は最大 MaxPersonalOffsetHits 打ぶんの重みに抑える)。機器や叩き方が変わっても追いつく
    if (count == 0 || !std::isfinite(offset)) { return; }
    const double old = (double)std::min(Config.PersonalOffsetCount, MaxPersonalOffsetHits);
    Config.PersonalOffset = (Config.PersonalOffset * old + offset * (double)count) / (old + (double)count);
    Config.PersonalOffsetCount = std::min(Config.PersonalOffsetCount + (int)std::min<size_t>(count, (size_t)MaxPersonalOffsetHits), MaxPersonalOffsetHits);
    Config.Write();
}
