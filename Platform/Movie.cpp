#include "Movie.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <regex>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace gfx {
    namespace {
        std::string Ffmpeg;
        constexpr int MaxWidth = 1280;      // 背景なので大きい動画は縮めて受け取る (読み込みの量を減らす)

#ifdef _WIN32
        std::wstring Wide(const std::string& s) {
            const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
            std::wstring w(n > 0 ? n - 1 : 0, L'\0');
            if (n > 0) { MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n); }
            return w;
        }
        // 子プロセスを起動して標準出力 (またはエラー出力) を読むパイプを返す
        struct Process {
            HANDLE Proc = nullptr, Read = nullptr;
            bool Start(const std::wstring& cmd, bool stderrOut) {
                SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
                HANDLE write = nullptr;
                if (!CreatePipe(&Read, &write, &sa, 1 << 20)) { return false; }
                SetHandleInformation(Read, HANDLE_FLAG_INHERIT, 0);
                STARTUPINFOW si{ sizeof(si) };
                si.dwFlags = STARTF_USESTDHANDLES;
                si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
                HANDLE nul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
                si.hStdOutput = stderrOut ? nul : write;
                si.hStdError = stderrOut ? write : nul;
                PROCESS_INFORMATION pi{};
                std::wstring line = cmd;
                const BOOL ok = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
                CloseHandle(write);
                if (nul != INVALID_HANDLE_VALUE) { CloseHandle(nul); }
                if (!ok) { CloseHandle(Read); Read = nullptr; return false; }
                CloseHandle(pi.hThread);
                Proc = pi.hProcess;
                return true;
            }
            bool ReadAll(void* buf, size_t size) {
                auto* p = static_cast<char*>(buf);
                while (size > 0) {
                    DWORD got = 0;
                    if (!ReadFile(Read, p, (DWORD)std::min<size_t>(size, 1 << 20), &got, nullptr) || got == 0) { return false; }
                    p += got; size -= got;
                }
                return true;
            }
            void Stop() {
                if (Proc) { TerminateProcess(Proc, 0); WaitForSingleObject(Proc, 1000); CloseHandle(Proc); Proc = nullptr; }
                if (Read) { CloseHandle(Read); Read = nullptr; }
            }
            ~Process() { Stop(); }
        };
#endif
    }

    void SetMoviePlayerPath(const std::string& ffmpeg) { Ffmpeg = ffmpeg; }

    struct Movie::Impl {
        std::string Path;
        int SrcW = 0, SrcH = 0, W = 0, H = 0;
        double Fps = 30;
        Texture Tex;
        // 受け取り側のスレッド
        std::thread Worker;
        std::atomic_bool StopFlag{ false };
        std::mutex Lock;
        std::vector<uint8_t> Latest;        // 最後に受け取ったフレーム
        double LatestMs = -1;               // そのフレームの時刻
        bool Fresh = false;                 // テクスチャへまだ送っていない
        std::atomic_bool End{ false };
        double StartMs = 0;                 // ffmpeg を起動した位置
        double ShownMs = -1;
        std::atomic<double> WantMs{ 0 };    // 今表示したい時刻 (受け取り側が先へ進みすぎないように)
        bool Running = false;
#ifdef _WIN32
        Process Proc;
#endif
        void StopWorker() {
            StopFlag = true;
#ifdef _WIN32
            Proc.Stop();
#endif
            if (Worker.joinable()) { Worker.join(); }
            StopFlag = false;
        }
        void StartAt(double ms) {
            StopWorker();
            StartMs = std::max(ms, 0.0);
            End = false;
#ifdef _WIN32
            wchar_t ss[64];
            swprintf(ss, 64, L"%.3f", StartMs / 1000.0);
            const std::wstring cmd = L"\"" + Wide(Ffmpeg) + L"\" -v error -ss " + ss + L" -i \"" + Wide(Path) + L"\" -an -sn -vf scale=" + std::to_wstring(W) + L":" + std::to_wstring(H)
                + L" -pix_fmt bgra -f rawvideo -";
            if (!Proc.Start(cmd, false)) { End = true; return; }
            Worker = std::thread([this] { try {
                std::vector<uint8_t> frame((size_t)W * H * 4);
                for (int64_t index = 0; !StopFlag; ++index) {
                    if (!Proc.ReadAll(frame.data(), frame.size())) { End = true; break; }
                    // 表示の時刻になるまで待ってから渡す (その間 ffmpeg はパイプがいっぱいになって止まる)
                    const double at = StartMs + index * 1000.0 / Fps;
                    while (!StopFlag && LatestMs >= 0 && at > WantMs.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
                    if (StopFlag) { break; }
                    std::lock_guard lock(Lock);
                    Latest.swap(frame);
                    frame.resize((size_t)W * H * 4);
                    LatestMs = at;
                    Fresh = true;
                }
            } catch (...) { End = true; } });
#else
            End = true;
#endif
        }
    };

    Movie::Movie() : P(std::make_unique<Impl>()) {}
    Movie::~Movie() { Close(); }

    bool Movie::Open(const std::string& path) {
        Close();
#ifdef _WIN32
        if (Ffmpeg.empty()) { return false; }
        // 大きさとフレームレートを調べる (ffmpeg -i のエラー出力の "Video: ..., 1920x1080 ..., 29.97 fps")
        Process probe;
        if (!probe.Start(L"\"" + Wide(Ffmpeg) + L"\" -hide_banner -i \"" + Wide(path) + L"\"", true)) { return false; }
        std::string info;
        char buf[4096];
        DWORD got = 0;
        while (ReadFile(probe.Read, buf, sizeof(buf), &got, nullptr) && got > 0) { info.append(buf, got); }
        probe.Stop();
        std::smatch m;
        if (!std::regex_search(info, m, std::regex(R"(Video:.*?(\d{2,5})x(\d{2,5}))"))) { return false; }
        P->SrcW = std::stoi(m[1]); P->SrcH = std::stoi(m[2]);
        if (std::regex_search(info, m, std::regex(R"(([\d.]+) fps)"))) { P->Fps = std::max(1.0, std::stod(m[1])); }
        const double scale = P->SrcW > MaxWidth ? (double)MaxWidth / P->SrcW : 1.0;
        P->W = std::max(2, (int)std::lround(P->SrcW * scale) & ~1);
        P->H = std::max(2, (int)std::lround(P->SrcH * scale) & ~1);
        P->Path = path;
        P->Tex = CreateTexture(P->W, P->H, false);
        if (!P->Tex) { return false; }
        std::vector<uint8_t> black((size_t)P->W * P->H * 4, 0);
        UpdateTexture(P->Tex, black.data(), P->W * 4);
        return true;
#else
        (void)path;
        return false;
#endif
    }
    void Movie::Close() {
        P->StopWorker();
        P->Tex = {};
        P->Path.clear();
        P->Latest.clear();
        P->LatestMs = P->ShownMs = -1;
        P->Running = false;
    }
    bool Movie::IsOpen() const { return (bool)P->Tex; }
    float Movie::Width() const { return (float)P->SrcW; }
    float Movie::Height() const { return (float)P->SrcH; }
    bool Movie::Ended() const { return P->End; }
    const Texture& Movie::Frame() const { return P->Tex; }

    void Movie::Sync(double targetMs, bool running) {
        if (!IsOpen()) { return; }
        P->WantMs = targetMs;
        if (!running || targetMs < 0) {
            if (targetMs < 0 && P->ShownMs != 0 && P->Running) { P->StopWorker(); P->Running = false; P->ShownMs = -1; }
            return;
        }
        // 始まっていない・大きく戻った・先へ飛んだ時は、その位置から読み直す
        const bool behind = P->LatestMs >= 0 && targetMs + 200.0 < P->LatestMs;
        const bool ahead = P->Running && P->LatestMs >= 0 && targetMs > P->LatestMs + 1500.0 && !P->End;
        if (!P->Running || behind || ahead) {
            {
                std::lock_guard lock(P->Lock);
                P->LatestMs = -1;
            }
            P->StartAt(targetMs);
            P->Running = true;
        }
        std::lock_guard lock(P->Lock);
        if (P->Fresh && !P->Latest.empty()) {
            UpdateTexture(P->Tex, P->Latest.data(), P->W * 4);
            P->ShownMs = P->LatestMs;
            P->Fresh = false;
        }
    }
}
