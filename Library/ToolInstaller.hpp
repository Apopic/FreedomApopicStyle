#pragma once
// 外部ツール (ffmpeg / yt-dlp) の導入。
//
// ■ ライセンスについて
//   ゲームには ffmpeg・yt-dlp を同梱しない。プレイヤーが Config で導入を選んだ時に、配布元の公式な配布場所から
//   プレイヤー自身の PC にダウンロードする (再配布ではない)。ゲームは別プロセスとして起動するだけでリンクしない。
//   ・yt-dlp : Unlicense (パブリックドメイン相当)。https://github.com/yt-dlp/yt-dlp
//   ・ffmpeg : FFmpeg 公式サイトが案内している BtbN 氏の Windows ビルド (GPL 版。libx264 を含む)。
//              https://github.com/BtbN/FFmpeg-Builds  ライセンス文は licenses/ffmpeg-LICENSE.txt に保存する
//
// ■ 安全性
//   ・HTTPS (WinHTTP。証明書は Windows が検証する) でのみ取得し、配布元が公開している SHA-256 と照合する
//   ・zip の展開は Windows 標準の tar.exe (System32 のもの。".." を含むパスは展開しない) を使う
//   ・ダウンロード中のファイルは .part に書き、検証が通ってから置き換える
//
// DxLib には依存しない (テストからも使うため)。

#include "NetCrypto.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

namespace tools {
    namespace fs = std::filesystem;

    inline constexpr const wchar_t* YtDlpUrl = L"https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe";
    inline constexpr const wchar_t* YtDlpSumsUrl = L"https://github.com/yt-dlp/yt-dlp/releases/latest/download/SHA2-256SUMS";
    inline constexpr const char* YtDlpAsset = "yt-dlp.exe";
    inline constexpr const wchar_t* FfmpegUrl = L"https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip";
    inline constexpr const wchar_t* FfmpegSumsUrl = L"https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/checksums.sha256";
    inline constexpr const char* FfmpegAsset = "ffmpeg-master-latest-win64-gpl.zip";
    inline constexpr uint64_t MaxDownload = 512ull * 1024 * 1024;

    // 導入の進み具合 (導入のスレッドが書き、画面が読む)
    struct Progress {
        enum class Phase { Idle, Running, Done, Failed };
        std::atomic<Phase> State = Phase::Idle;
        std::atomic<uint64_t> Received = 0;
        std::atomic<uint64_t> Total = 0;
        std::atomic<bool> Cancel = false;

        void SetText(std::string text) { std::lock_guard lock(m_Mutex); m_Text = std::move(text); }
        std::string Text() const { std::lock_guard lock(m_Mutex); return m_Text; }
        // 「ダウンロード中 45%」など
        std::string Describe() const {
            std::string text = Text();
            const uint64_t total = Total.load(), got = Received.load();
            if (State == Phase::Running && total > 0) {
                text += " " + std::to_string((int)(100.0 * (double)got / (double)total)) + "%";
            }
            return text;
        }

    private:
        mutable std::mutex m_Mutex;
        std::string m_Text;
    };

    // HTTPS の GET。to が空なら out に、空でなければファイルに書く (リダイレクトは WinHTTP が追う)
    inline bool HttpGet(const std::wstring& url, const fs::path& to, std::string* out, Progress& progress, std::string& error) {
        URL_COMPONENTS uc{ sizeof(uc) };
        wchar_t host[256]{}, path[2048]{};
        uc.lpszHostName = host; uc.dwHostNameLength = 256;
        uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS) {
            error = "URL が正しくありません";
            return false;
        }
        struct Handle {
            HINTERNET h = nullptr;
            ~Handle() { if (h) { WinHttpCloseHandle(h); } }
        } session, connect, request;
        session.h = WinHttpOpen(L"FreedomApopicStyle-ToolInstaller/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session.h) { error = "通信を開始できませんでした"; return false; }
        DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | 0x00002000 /* TLS1.3 */;
        if (!WinHttpSetOption(session.h, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) {
            protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
            WinHttpSetOption(session.h, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
        }
        WinHttpSetTimeouts(session.h, 15000, 15000, 30000, 30000);
        connect.h = WinHttpConnect(session.h, host, uc.nPort, 0);
        if (!connect.h) { error = "サーバーに接続できませんでした"; return false; }
        request.h = WinHttpOpenRequest(connect.h, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!request.h) { error = "通信を開始できませんでした"; return false; }
        // HTTPS から HTTP へのリダイレクトは追わない (既定の方針)
        if (!WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.h, nullptr)) {
            error = "ダウンロードできませんでした (インターネットに接続されているか確認してください)";
            return false;
        }
        DWORD status = 0, size = sizeof(status);
        WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        if (status != 200) {
            error = "ダウンロードできませんでした (HTTP " + std::to_string(status) + ")";
            return false;
        }
        wchar_t lenbuf[32]{};
        size = sizeof(lenbuf);
        progress.Received = 0;
        progress.Total = WinHttpQueryHeaders(request.h, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, lenbuf, &size, WINHTTP_NO_HEADER_INDEX) ? _wcstoui64(lenbuf, nullptr, 10) : 0;
        if (progress.Total > MaxDownload) { error = "ファイルが大きすぎます"; return false; }

        std::ofstream file;
        if (!to.empty()) {
            file.open(to, std::ios::binary | std::ios::trunc);
            if (!file) { error = "ファイルを書き込めませんでした"; return false; }
        }
        std::vector<char> buf(64 * 1024);
        while (true) {
            if (progress.Cancel) { error = "中止しました"; return false; }
            DWORD got = 0;
            if (!WinHttpReadData(request.h, buf.data(), (DWORD)buf.size(), &got)) {
                error = "ダウンロードが途中で切れました";
                return false;
            }
            if (got == 0) { break; }
            progress.Received += got;
            if (progress.Received > MaxDownload) { error = "ファイルが大きすぎます"; return false; }
            if (out) { out->append(buf.data(), got); }
            else if (!file.write(buf.data(), got)) { error = "ファイルを書き込めませんでした (空き容量を確認してください)"; return false; }
        }
        if (progress.Total > 0 && progress.Received != progress.Total) { error = "ダウンロードが途中で切れました"; return false; }
        return true;
    }

    inline std::string Hex(std::span<const uint8_t> bytes) {
        static constexpr char digits[] = "0123456789abcdef";
        std::string out;
        for (uint8_t b : bytes) { out += digits[b >> 4]; out += digits[b & 15]; }
        return out;
    }
    inline std::string Sha256File(const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) { return {}; }
        fasnet::crypto::Sha256 sha;
        std::vector<char> buf(256 * 1024);
        while (file) {
            file.read(buf.data(), (std::streamsize)buf.size());
            const auto n = file.gcount();
            if (n > 0) { sha.Update(reinterpret_cast<const uint8_t*>(buf.data()), (size_t)n); }
        }
        const auto digest = sha.Final();
        return Hex(digest);
    }
    // "ハッシュ  ファイル名" 形式 (sha256sum) の一覧から名前の一致する行のハッシュを取り出す
    inline std::string FindSum(const std::string& sums, std::string_view name) {
        size_t pos = 0;
        while (pos < sums.size()) {
            size_t end = sums.find('\n', pos);
            if (end == std::string::npos) { end = sums.size(); }
            std::string_view line(sums.data() + pos, end - pos);
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) { line.remove_suffix(1); }
            const size_t sp = line.find(' ');
            if (sp == 64) {
                std::string_view file = line.substr(sp);
                while (!file.empty() && (file.front() == ' ' || file.front() == '*')) { file.remove_prefix(1); }
                if (file == name) {
                    std::string hash(line.substr(0, 64));
                    for (auto& c : hash) { c = (char)std::tolower((unsigned char)c); }
                    if (std::ranges::all_of(hash, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); })) { return hash; }
                }
            }
            pos = end + 1;
        }
        return {};
    }

    // ダウンロードして SHA-256 を確かめ、.part から置き換える
    inline bool DownloadVerified(const std::wstring& url, const std::wstring& sumsurl, std::string_view asset, const fs::path& dest, Progress& progress, std::string& error) {
        progress.SetText("確認用のハッシュを取得しています");
        std::string sums;
        if (!HttpGet(sumsurl, {}, &sums, progress, error)) { return false; }
        const std::string expected = FindSum(sums, asset);
        if (expected.empty()) { error = "配布元のハッシュ一覧に " + std::string(asset) + " がありません"; return false; }

        progress.SetText("ダウンロード中");
        std::error_code ec;
        fs::create_directories(dest.parent_path(), ec);
        const fs::path part = fs::path(dest).concat(L".part");
        const bool ok = HttpGet(url, part, nullptr, progress, error);
        progress.Total = 0;
        if (!ok) { fs::remove(part, ec); return false; }
        progress.SetText("ファイルを確認しています");
        if (Sha256File(part) != expected) {
            fs::remove(part, ec);
            error = "ダウンロードしたファイルが配布元のハッシュと一致しません (壊れているか、改ざんされています)";
            return false;
        }
        fs::rename(part, dest, ec);
        if (ec) {
            fs::remove(dest, ec);
            fs::rename(part, dest, ec);
        }
        if (ec) { fs::remove(part, ec); error = "ファイルを置き換えられませんでした (使用中の可能性があります)"; return false; }
        return true;
    }

    // コンソールを出さずに起動して終了を待つ (戻り値: 終了コード。起動失敗は -1)
    inline int RunHidden(const std::wstring& exe, std::wstring cmdline, const fs::path& workdir, DWORD timeoutms) {
        STARTUPINFOW si{ sizeof(si) };
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(exe.c_str(), cmdline.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, workdir.c_str(), &si, &pi)) {
            return -1;
        }
        CloseHandle(pi.hThread);
        if (WaitForSingleObject(pi.hProcess, timeoutms) != WAIT_OBJECT_0) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 5000);
        }
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        return (int)code;
    }

    inline bool InstallYtDlp(const fs::path& exedir, Progress& progress, std::string& error) {
        return DownloadVerified(YtDlpUrl, YtDlpSumsUrl, YtDlpAsset, exedir / "yt-dlp.exe", progress, error);
    }

    inline bool InstallFfmpeg(const fs::path& exedir, Progress& progress, std::string& error) {
        std::error_code ec;
        const fs::path work = exedir / "download" / "ffmpeg-install";
        fs::remove_all(work, ec);
        fs::create_directories(work, ec);
        const fs::path zip = work / FfmpegAsset;
        if (!DownloadVerified(FfmpegUrl, FfmpegSumsUrl, FfmpegAsset, zip, progress, error)) {
            fs::remove_all(work, ec);
            return false;
        }
        progress.SetText("展開しています");
        // PATH に頼らず System32 の tar.exe を使う (Windows 10 1803 以降に標準で入っている)
        wchar_t sysdir[MAX_PATH]{};
        GetSystemDirectoryW(sysdir, MAX_PATH);
        const std::wstring tar = std::wstring(sysdir) + L"\\tar.exe";
        if (!fs::is_regular_file(tar, ec)) {
            fs::remove_all(work, ec);
            error = "tar.exe が見つかりません (Windows 10 1803 以降が必要です)";
            return false;
        }
        const fs::path outdir = work / "x";
        fs::create_directories(outdir, ec);
        std::wstring cmd = L"\"" + tar + L"\" -x -f \"" + zip.wstring() + L"\" -C \"" + outdir.wstring() + L"\"";
        if (RunHidden(tar, cmd, work, 5 * 60 * 1000) != 0) {
            fs::remove_all(work, ec);
            error = "展開に失敗しました";
            return false;
        }
        // 展開したフォルダから bin/ffmpeg.exe などを探して、ゲームのフォルダへコピーする
        fs::path bin, license;
        for (auto it = fs::recursive_directory_iterator(outdir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (!it->is_regular_file(ec)) { continue; }
            const auto name = it->path().filename().wstring();
            if (_wcsicmp(name.c_str(), L"ffmpeg.exe") == 0) { bin = it->path().parent_path(); }
            else if (_wcsicmp(name.c_str(), L"LICENSE.txt") == 0 && license.empty()) { license = it->path(); }
        }
        if (bin.empty()) {
            fs::remove_all(work, ec);
            error = "展開したファイルに ffmpeg.exe がありません";
            return false;
        }
        for (const wchar_t* name : { L"ffmpeg.exe", L"ffprobe.exe" }) {
            if (!fs::exists(bin / name, ec)) { continue; }
            fs::copy_file(bin / name, exedir / name, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                fs::remove_all(work, ec);
                error = "ffmpeg.exe をコピーできませんでした (使用中の可能性があります)";
                return false;
            }
        }
        if (!license.empty()) {
            fs::create_directories(exedir / "licenses", ec);
            fs::copy_file(license, exedir / "licenses" / "ffmpeg-LICENSE.txt", fs::copy_options::overwrite_existing, ec);
        }
        fs::remove_all(work, ec);
        return true;
    }

    // 別スレッドで導入する。終わると progress.State が Done / Failed になる
    class Installer {
    public:
        ~Installer() { Join(); }
        bool Busy() const { return m_Progress.State == Progress::Phase::Running; }
        const Progress& State() const { return m_Progress; }
        void Start(std::function<bool(Progress&, std::string&)> job, std::string donetext) {
            if (Busy()) { return; }
            Join();
            m_Progress.Cancel = false;
            m_Progress.Received = 0;
            m_Progress.Total = 0;
            m_Progress.SetText("準備しています");
            m_Progress.State = Progress::Phase::Running;
            m_Thread = std::thread([this, job = std::move(job), donetext = std::move(donetext)] {
                std::string error;
                const bool ok = job(m_Progress, error);
                m_Progress.SetText(ok ? donetext : error);
                m_Progress.State = ok ? Progress::Phase::Done : Progress::Phase::Failed;
            });
        }
        void Cancel() { m_Progress.Cancel = true; }
        void Join() {
            if (m_Thread.joinable()) {
                m_Progress.Cancel = Busy() ? true : m_Progress.Cancel.load();
                m_Thread.join();
            }
        }

    private:
        Progress m_Progress;
        std::thread m_Thread;
    };
}
