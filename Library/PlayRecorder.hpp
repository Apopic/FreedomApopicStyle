#pragma once
// プレイの録画 (動画ファイルへの書き出し)。
//
// 軽さのための作り:
//   ・映像: DxLib の Direct3D 11 のバックバッファを GPU 上でステージングテクスチャへコピーし、1 つ前にコピーしたものを読む
//     (コピーの完了を待たないので描画が止まらない)。決めたフレームレート (既定 60fps) の時だけコピーする
//   ・エンコード: 読んだ画素を別スレッドで ffmpeg (別プロセス) の標準入力へ流し、H.264 (MP4) にする
//   ・音声: WASAPI のプロセスループバックでこのゲームの音だけを録る (Windows 10 2004 以降。使えなければ PC 全体の音)
//   ・演奏が終わったら、映像と音声を ffmpeg で 1 つの MP4 にまとめる (別スレッド。映像は再エンコードしない)
// ffmpeg.exe はゲームのフォルダか PATH から探す。

#ifndef __ANDROID__

#include <windows.h>
#include <d3d11.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <string_view>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "mmdevapi.lib")
#pragma comment(lib, "ole32.lib")

namespace rec {

    namespace fs = std::filesystem;

    // ffmpeg.exe の場所 (無ければ空)
    inline fs::path FindFfmpeg(const fs::path& exedir) {
        std::error_code ec;
        if (fs::is_regular_file(exedir / "ffmpeg.exe", ec)) { return exedir / "ffmpeg.exe"; }
        wchar_t buf[MAX_PATH]{};
        if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, buf, nullptr) > 0) { return fs::path(buf); }
        return {};
    }
    // CommandLineToArgvW の規則でクォートする (「"」の前と末尾の「\」は 2 倍にする。"C:\dir\" のようなパスも正しく渡る)
    inline std::wstring Quote(const std::wstring& s) {
        if (!s.empty() && s.find_first_of(L" \t\n\v\"") == std::wstring::npos) { return s; }
        std::wstring out = L"\"";
        size_t backslashes = 0;
        for (wchar_t c : s) {
            if (c == L'\\') { ++backslashes; continue; }
            out.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
            backslashes = 0;
            out += c;
        }
        out.append(backslashes * 2, L'\\');
        return out + L"\"";
    }
    // コンソールを出さずに起動する。stdin を渡すと標準入力をパイプにつなぐ
    inline bool Spawn(const std::vector<std::wstring>& args, PROCESS_INFORMATION& pi, HANDLE* stdinwrite) {
        std::wstring cmd;
        for (const auto& a : args) { if (!cmd.empty()) { cmd += L' '; } cmd += Quote(a); }
        STARTUPINFOW si{ sizeof(si) };
        HANDLE readpipe = nullptr, writepipe = nullptr;
        SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
        HANDLE nul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
        if (stdinwrite != nullptr) {
            if (!CreatePipe(&readpipe, &writepipe, &sa, 4 * 1024 * 1024)) { CloseHandle(nul); return false; }
            SetHandleInformation(writepipe, HANDLE_FLAG_INHERIT, 0);
        }
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = readpipe;
        si.hStdOutput = nul;
        si.hStdError = nul;
        const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, nullptr, nullptr, &si, &pi);
        if (readpipe) { CloseHandle(readpipe); }
        CloseHandle(nul);
        if (!ok) {
            if (writepipe) { CloseHandle(writepipe); }
            return false;
        }
        if (stdinwrite != nullptr) { *stdinwrite = writepipe; }
        return true;
    }

    // ---------------------------------------------------------------- 映像: ffmpeg へ生の画素を流す
    class VideoWriter {
    public:
        bool Start(const fs::path& ffmpeg, const fs::path& out, int width, int height, int fps, bool bgra, const std::string& codec, int crf) {
            m_FrameBytes = (size_t)width * height * 4;
            const std::wstring size = std::to_wstring(width) + L"x" + std::to_wstring(height);
            std::vector<std::wstring> args = { ffmpeg.wstring(), L"-y", L"-loglevel", L"error",
                L"-f", L"rawvideo", L"-pix_fmt", bgra ? L"bgra" : L"rgba", L"-s", size, L"-r", std::to_wstring(fps), L"-i", L"-",
                L"-vf", L"scale=trunc(iw/2)*2:trunc(ih/2)*2", L"-pix_fmt", L"yuv420p",
                L"-c:v", std::wstring(codec.begin(), codec.end()) };
            if (codec == "libx264") {
                args.insert(args.end(), { L"-preset", L"veryfast", L"-crf", std::to_wstring(crf), L"-tune", L"animation" });
            }
            else {
                args.insert(args.end(), { L"-b:v", L"12M" });     // ハードウェアエンコーダ (h264_nvenc など)
            }
            args.insert(args.end(), { L"-movflags", L"+faststart", out.wstring() });
            if (!Spawn(args, m_Process, &m_Pipe)) { return false; }
            m_Stop = false;
            m_Thread = std::thread([this] { Loop(); });
            return true;
        }
        // 1 フレーム分の画素 (count 回書く = 取りこぼしたフレームを同じ画で埋める)。詰まっている時は古いものを捨てずに待たない
        bool Push(std::vector<uint8_t>&& frame, int count) {
            std::unique_lock lock(m_Mutex);
            if (m_Queue.size() >= MaxQueue) { ++m_Dropped; return false; }
            m_Queue.push_back({ std::move(frame), count });
            m_Cv.notify_one();
            return true;
        }
        // 書き終わるまで待って ffmpeg を終わらせる。成功なら true
        bool Finish() {
            {
                std::lock_guard lock(m_Mutex);
                m_Stop = true;
                m_Cv.notify_one();
            }
            if (m_Thread.joinable()) { m_Thread.join(); }
            if (m_Pipe) { CloseHandle(m_Pipe); m_Pipe = nullptr; }
            DWORD code = 1;
            if (m_Process.hProcess) {
                WaitForSingleObject(m_Process.hProcess, 60000);
                GetExitCodeProcess(m_Process.hProcess, &code);
                CloseHandle(m_Process.hProcess);
                CloseHandle(m_Process.hThread);
                m_Process = {};
            }
            return code == 0 && !m_Broken;
        }
        size_t Dropped() const { return m_Dropped; }
        size_t FrameBytes() const { return m_FrameBytes; }

    private:
        static constexpr size_t MaxQueue = 12;
        struct Item { std::vector<uint8_t> Data; int Count; };
        void Loop() {
            for (;;) {
                Item item;
                {
                    std::unique_lock lock(m_Mutex);
                    m_Cv.wait(lock, [&] { return m_Stop || !m_Queue.empty(); });
                    if (m_Queue.empty()) { return; }
                    item = std::move(m_Queue.front());
                    m_Queue.pop_front();
                }
                for (int i = 0; i < item.Count && !m_Broken; ++i) {
                    size_t done = 0;
                    while (done < item.Data.size()) {
                        DWORD written = 0;
                        if (!WriteFile(m_Pipe, item.Data.data() + done, (DWORD)std::min<size_t>(item.Data.size() - done, 1 << 20), &written, nullptr) || written == 0) {
                            m_Broken = true;
                            break;
                        }
                        done += written;
                    }
                }
            }
        }
        PROCESS_INFORMATION m_Process{};
        HANDLE m_Pipe = nullptr;
        std::thread m_Thread;
        std::mutex m_Mutex;
        std::condition_variable m_Cv;
        std::deque<Item> m_Queue;
        bool m_Stop = false;
        std::atomic_bool m_Broken = false;
        std::atomic<size_t> m_Dropped = 0;
        size_t m_FrameBytes = 0;
    };

    // ---------------------------------------------------------------- 音声: このプロセスの音を WAV に録る
    class ActivateHandler : public IActivateAudioInterfaceCompletionHandler, public IAgileObject {
    public:
        HANDLE Event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        HRESULT Result = E_FAIL;
        IAudioClient* Client = nullptr;
        ~ActivateHandler() { if (Event) { CloseHandle(Event); } }
        STDMETHODIMP QueryInterface(REFIID riid, void** out) override {
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IActivateAudioInterfaceCompletionHandler)) { *out = static_cast<IActivateAudioInterfaceCompletionHandler*>(this); }
            else if (riid == __uuidof(IAgileObject)) { *out = static_cast<IAgileObject*>(this); }
            else { *out = nullptr; return E_NOINTERFACE; }
            AddRef();
            return S_OK;
        }
        STDMETHODIMP_(ULONG) AddRef() override { return ++m_Ref; }
        STDMETHODIMP_(ULONG) Release() override { const ULONG r = --m_Ref; if (r == 0) { delete this; } return r; }
        STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* op) override {
            HRESULT hr = E_FAIL;
            IUnknown* unk = nullptr;
            if (SUCCEEDED(op->GetActivateResult(&hr, &unk)) && SUCCEEDED(hr) && unk) {
                hr = unk->QueryInterface(__uuidof(IAudioClient), (void**)&Client);
            }
            if (unk) { unk->Release(); }
            Result = hr;
            SetEvent(Event);
            return S_OK;
        }
    private:
        std::atomic<ULONG> m_Ref = 1;
    };

    class AudioCapture {
    public:
        static constexpr int Rate = 48000, Channels = 2;
        bool Start(const fs::path& wav) {
            m_Path = wav;
            m_Stop = false;
            m_Ready = 0;
            m_Thread = std::thread([this] { Loop(); });
            while (m_Ready == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
            return m_Ready == 1;
        }
        // 録音の始まった時刻 (映像との時刻合わせ用)
        std::chrono::steady_clock::time_point StartedAt() const { return m_StartedAt; }
        bool ProcessOnly() const { return m_ProcessOnly; }
        void Stop() {
            m_Stop = true;
            if (m_Thread.joinable()) { m_Thread.join(); }
        }
    private:
        IAudioClient* Activate(WAVEFORMATEX& wf, bool& processonly) {
            // このプロセス (とその子) の音だけ
            AUDIOCLIENT_ACTIVATION_PARAMS params{};
            params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
            params.ProcessLoopbackParams.TargetProcessId = GetCurrentProcessId();
            params.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
            PROPVARIANT pv{};
            pv.vt = VT_BLOB;
            pv.blob.cbSize = sizeof(params);
            pv.blob.pBlobData = (BYTE*)&params;
            auto* handler = new ActivateHandler();
            IActivateAudioInterfaceAsyncOperation* op = nullptr;
            IAudioClient* client = nullptr;
            if (SUCCEEDED(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient), &pv, handler, &op))
                && WaitForSingleObject(handler->Event, 3000) == WAIT_OBJECT_0 && SUCCEEDED(handler->Result) && handler->Client) {
                client = handler->Client;
                if (FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 200 * 10000, 0, &wf, nullptr))) {
                    client->Release();
                    client = nullptr;
                }
                else { processonly = true; }
            }
            if (op) { op->Release(); }
            handler->Release();
            if (client) { return client; }
            // 使えない環境: 既定の出力デバイスの音 (PC 全体の音) を録る
            IMMDeviceEnumerator* en = nullptr;
            IMMDevice* dev = nullptr;
            if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en))
                && SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev))
                && SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client))) {
                WAVEFORMATEX* mix = nullptr;
                // 共有モードのループバックはミックスの形式でしか開けないので、16bit への変換は自分で行う (AUTOCONVERTPCM で依頼)
                if (FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                    200 * 10000, 0, &wf, nullptr))) {
                    client->Release();
                    client = nullptr;
                }
                if (mix) { CoTaskMemFree(mix); }
            }
            if (dev) { dev->Release(); }
            if (en) { en->Release(); }
            return client;
        }
        void Loop() {
            const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            WAVEFORMATEX wf{};
            wf.wFormatTag = WAVE_FORMAT_PCM;
            wf.nChannels = Channels;
            wf.nSamplesPerSec = Rate;
            wf.wBitsPerSample = 16;
            wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
            wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
            bool processonly = false;
            IAudioClient* client = Activate(wf, processonly);
            IAudioCaptureClient* capture = nullptr;
            std::ofstream file(m_Path, std::ios::binary);
            if (!client || FAILED(client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture)) || !file) {
                if (client) { client->Release(); }
                m_Ready = 2;
                if (SUCCEEDED(init)) { CoUninitialize(); }
                return;
            }
            m_ProcessOnly = processonly;
            WriteHeader(file, 0);
            client->Start();
            m_StartedAt = std::chrono::steady_clock::now();
            m_Ready = 1;
            uint64_t frames = 0;
            std::vector<int16_t> silence;
            while (!m_Stop) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                UINT32 packet = 0;
                while (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet > 0) {
                    BYTE* data = nullptr;
                    UINT32 n = 0;
                    DWORD flags = 0;
                    if (FAILED(capture->GetBuffer(&data, &n, &flags, nullptr, nullptr))) { break; }
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        silence.assign((size_t)n * Channels, 0);
                        file.write((const char*)silence.data(), (std::streamsize)silence.size() * 2);
                    }
                    else {
                        file.write((const char*)data, (std::streamsize)n * wf.nBlockAlign);
                    }
                    frames += n;
                    capture->ReleaseBuffer(n);
                }
                // 音が出ていない間はパケットが来ないことがあるので、経過時間に合わせて無音で埋める (映像とずれないように)
                const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_StartedAt).count();
                const uint64_t expected = (uint64_t)(elapsed * Rate);
                if (expected > frames + Rate / 10) {
                    const uint64_t pad = expected - frames - Rate / 20;
                    silence.assign((size_t)pad * Channels, 0);
                    file.write((const char*)silence.data(), (std::streamsize)silence.size() * 2);
                    frames += pad;
                }
            }
            // 止めた時刻まで無音で埋めて、音声を映像と同じ長さにする
            {
                const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_StartedAt).count();
                const uint64_t expected = (uint64_t)(elapsed * Rate);
                if (expected > frames) {
                    silence.assign((size_t)(expected - frames) * Channels, 0);
                    file.write((const char*)silence.data(), (std::streamsize)silence.size() * 2);
                    frames = expected;
                }
            }
            client->Stop();
            capture->Release();
            client->Release();
            file.seekp(0);
            WriteHeader(file, frames * wf.nBlockAlign);
            file.close();
            if (SUCCEEDED(init)) { CoUninitialize(); }
        }
        static void WriteHeader(std::ofstream& f, uint64_t bytes) {
            const uint32_t data = (uint32_t)std::min<uint64_t>(bytes, 0xFFFFFFF0u);
            auto u32 = [&](uint32_t v) { f.write((const char*)&v, 4); };
            auto u16 = [&](uint16_t v) { f.write((const char*)&v, 2); };
            f.write("RIFF", 4); u32(36 + data); f.write("WAVEfmt ", 8); u32(16);
            u16(1); u16(Channels); u32(Rate); u32(Rate * Channels * 2); u16(Channels * 2); u16(16);
            f.write("data", 4); u32(data);
        }
        fs::path m_Path;
        std::thread m_Thread;
        std::atomic_bool m_Stop = false;
        std::atomic<int> m_Ready = 0;       // 1: 録音中, 2: 失敗
        std::atomic_bool m_ProcessOnly = false;
        std::chrono::steady_clock::time_point m_StartedAt{};
    };

    // 映像ファイルと音声ファイルを 1 つの MP4 にまとめる (映像はそのまま。音声は AAC)。audiodelay: 映像より音声が遅れて始まった秒数
    inline bool RunWait(const std::vector<std::wstring>& args);
    inline bool Mux(const fs::path& ffmpeg, const fs::path& video, const fs::path& audio, double audiodelay, const fs::path& out) {
        std::vector<std::wstring> args = { ffmpeg.wstring(), L"-y", L"-loglevel", L"error", L"-i", video.wstring() };
        if (std::abs(audiodelay) > 0.0005) {
            wchar_t buf[32];
            swprintf_s(buf, L"%.3f", audiodelay);
            args.insert(args.end(), { L"-itsoffset", buf });
        }
        args.insert(args.end(), { L"-i", audio.wstring(), L"-map", L"0:v:0", L"-map", L"1:a:0", L"-c:v", L"copy", L"-c:a", L"aac", L"-b:a", L"192k",
            L"-shortest", L"-movflags", L"+faststart", out.wstring() });
        return RunWait(args);
    }

    inline bool RunWait(const std::vector<std::wstring>& args) {
        PROCESS_INFORMATION pi{};
        if (!Spawn(args, pi, nullptr)) { return false; }
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return code == 0;
    }

    // ---- 書き出しの形式 ----
    struct Format { const char* Ext; const char* Label; bool Video; bool Audio; };
    inline constexpr Format Formats[] = {
        { "mp4", "MP4 (H.264 + AAC)", true, true },
        { "mkv", "MKV (H.264 + AAC)", true, true },
        { "mov", "MOV (H.264 + AAC)", true, true },
        { "webm", "WebM (VP9 + Opus)", true, true },
        { "gif", "GIF (音声なし)", true, false },
    };
    inline const Format& FindFormat(std::string_view ext) {
        for (const auto& f : Formats) { if (ext == f.Ext) { return f; } }
        return Formats[0];
    }
    // 録画した映像 (H.264) と音声 (WAV) を指定の形式で書き出す。audio が空なら音声なし
    inline bool Export(const fs::path& ffmpeg, const fs::path& video, const fs::path& audio, double audiodelay, const fs::path& out, std::string_view ext) {
        const Format& f = FindFormat(ext);
        const bool hasaudio = !audio.empty() && f.Audio;
        if (!f.Video && !hasaudio) { return false; }
        std::vector<std::wstring> args = { ffmpeg.wstring(), L"-y", L"-loglevel", L"error" };
        if (f.Video) { args.insert(args.end(), { L"-i", video.wstring() }); }
        if (hasaudio) {
            if (f.Video && std::abs(audiodelay) > 0.0005) {
                wchar_t buf[32];
                swprintf_s(buf, L"%.3f", audiodelay);
                args.insert(args.end(), { L"-itsoffset", buf });
            }
            args.insert(args.end(), { L"-i", audio.wstring() });
        }
        const std::wstring a = f.Video ? L"1:a:0" : L"0:a:0";
        const std::string e(ext);
        if (e == "gif") {
            args.insert(args.end(), { L"-vf", L"fps=30,scale=iw/2:-2:flags=lanczos,split[x][y];[x]palettegen=stats_mode=diff[p];[y][p]paletteuse=dither=bayer", L"-loop", L"0" });
        }
        else if (f.Video) {
            args.insert(args.end(), { L"-map", L"0:v:0" });
            if (hasaudio) { args.insert(args.end(), { L"-map", a }); }
            if (e == "webm") {
                args.insert(args.end(), { L"-c:v", L"libvpx-vp9", L"-b:v", L"0", L"-crf", L"32", L"-deadline", L"realtime", L"-cpu-used", L"8", L"-row-mt", L"1" });
                if (hasaudio) { args.insert(args.end(), { L"-c:a", L"libopus", L"-b:a", L"160k" }); }
            }
            else {
                args.insert(args.end(), { L"-c:v", L"copy" });
                if (hasaudio) { args.insert(args.end(), { L"-c:a", L"aac", L"-b:a", L"192k" }); }
                if (e == "mp4" || e == "mov") { args.insert(args.end(), { L"-movflags", L"+faststart" }); }
            }
            if (hasaudio) { args.push_back(L"-shortest"); }
        }
        args.push_back(out.wstring());
        return RunWait(args);
    }

    // ---- 譜面から音声を作る (録音できない時・合成を選んだ時) ----
    // 曲と効果音を 48kHz ステレオに混ぜる
    struct Pcm {
        std::vector<float> Data;    // インターリーブ
        int Channels = 2;
        int Rate = 48000;
        size_t Frames() const { return Channels > 0 ? Data.size() / (size_t)Channels : 0; }
    };
    inline constexpr int MixRate = 48000;
    // src を out (48kHz ステレオ) の outstart 秒から、src の srcstart 秒の位置から speed 倍で足す (線形補間)
    inline void MixPcm(std::vector<float>& out, const Pcm& src, double outstart, double srcstart, double speed, float gain, double maxseconds = 1e9) {
        if (src.Frames() == 0 || src.Rate <= 0 || speed <= 0) { return; }
        const size_t outframes = out.size() / 2;
        const double step = (double)src.Rate / MixRate * speed;
        double pos = srcstart * src.Rate;
        int64_t o = (int64_t)std::llround(outstart * MixRate);
        if (o < 0) { pos += (double)(-o) * step; o = 0; }
        const int64_t limit = std::min<int64_t>((int64_t)outframes, o + (int64_t)(maxseconds * MixRate));
        const size_t frames = src.Frames();
        for (; o < limit; ++o, pos += step) {
            const size_t i = (size_t)pos;
            if (i + 1 >= frames) { break; }
            const float t = (float)(pos - (double)i);
            for (int c = 0; c < 2; ++c) {
                const int sc = src.Channels == 1 ? 0 : std::min(c, src.Channels - 1);
                const float a = src.Data[i * src.Channels + sc], b = src.Data[(i + 1) * src.Channels + sc];
                out[(size_t)o * 2 + c] += (a + (b - a) * t) * gain;
            }
        }
    }
    inline bool WriteWav(const fs::path& path, const std::vector<float>& stereo) {
        std::ofstream f(path, std::ios::binary);
        if (!f) { return false; }
        const uint32_t frames = (uint32_t)(stereo.size() / 2), bytes = frames * 4;
        auto u32 = [&](uint32_t v) { f.write((const char*)&v, 4); };
        auto u16 = [&](uint16_t v) { f.write((const char*)&v, 2); };
        f.write("RIFF", 4); u32(36 + bytes); f.write("WAVEfmt ", 8); u32(16); u16(1); u16(2); u32(MixRate); u32(MixRate * 4); u16(4); u16(16);
        f.write("data", 4); u32(bytes);
        std::vector<int16_t> pcm(stereo.size());
        for (size_t i = 0; i < stereo.size(); ++i) { pcm[i] = (int16_t)std::lround(std::clamp(stereo[i], -1.0f, 1.0f) * 32767.0f); }
        f.write((const char*)pcm.data(), (std::streamsize)(pcm.size() * 2));
        return (bool)f;
    }
    // WAV (16bit) の最大の振幅 (0～1)。無音の録音を見分けるため
    inline float WavPeak(const fs::path& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) { return 0.0f; }
        f.seekg(44);
        std::vector<int16_t> buf(1 << 16);
        int peak = 0;
        while (f) {
            f.read((char*)buf.data(), (std::streamsize)(buf.size() * 2));
            const size_t n = (size_t)f.gcount() / 2;
            for (size_t i = 0; i < n; ++i) { peak = std::max(peak, std::abs((int)buf[i])); }
        }
        return peak / 32768.0f;
    }
}

#endif
