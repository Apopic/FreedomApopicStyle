#pragma once
// Discord のリッチプレゼンス (ステータス表示・参加ボタン)。
// Discord クライアントがローカルに開く名前付きパイプ (\\.\pipe\discord-ipc-N) へ、公式 SDK と同じ RPC プロトコルで直接つなぐ。
// SDK の DLL は不要。アプリケーション ID は下の ApplicationId に組み込む。
//
// 通信は専用スレッドで行い、メインスレッドは SetActivity で表示内容を渡し、PollEvents で参加の通知を受け取るだけ。
// Discord が起動していなければ数秒ごとに接続をやり直す。

#ifndef __ANDROID__

#include <windows.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include "nlohmann/json.hpp"

class DiscordRpc {
public:
    // ゲームに組み込む Discord のアプリケーション ID (Developer Portal の Application ID)。
    // プレイヤーは入力しなくてよい。配布する人が自分のアプリケーションの ID に書き換える (空なら Discord 連携をしない)
    static constexpr const char* ApplicationId = "1573337117113974825";

    struct Event {
        enum class Type { Join, JoinRequest } Kind;
        std::string Secret;         // Join: 参加用の秘密 (= サーバーの「アドレス:ポート」)
        std::string UserId;         // JoinRequest: 参加を希望したユーザー
        std::string UserName;
    };

    static DiscordRpc& GetInstance() {
        static DiscordRpc instance;
        return instance;
    }
    DiscordRpc(const DiscordRpc&) = delete;
    DiscordRpc& operator=(const DiscordRpc&) = delete;

    // clientid が空なら停止する。同じ ID で呼んだ場合は何もしない
    void Start(const std::string& clientid) {
        if (clientid == m_ClientId && (m_Thread.joinable() || clientid.empty())) { return; }
        Stop();
        m_ClientId = clientid;
        if (clientid.empty()) { return; }
        if (RegisterProtocol) { RegisterLaunchCommand(clientid); }
        m_Stop = false;
        m_Thread = std::thread([this] { ThreadMain(); });
    }
    void Stop() {
        if (!m_Thread.joinable()) { return; }
        m_Stop = true;
        m_Thread.join();
        m_Connected = false;
    }
    bool IsConnected() const { return m_Connected; }

    // テスト用: 接続するパイプの名前と、起動コマンドを登録するか
    inline static std::wstring PipePrefix = L"discord-ipc-";
    inline static bool RegisterProtocol = true;

    // 表示内容 (Discord の activity オブジェクト)。null で表示を消す。前回と同じ内容なら送らない
    void SetActivity(const nlohmann::json& activity) {
        std::lock_guard lock(m_Mutex);
        if (m_Activity == activity) { return; }
        m_Activity = activity;
        m_ActivityDirty = true;
    }
    void PollEvents(std::vector<Event>& out) {
        out.clear();
        std::lock_guard lock(m_Mutex);
        out.swap(m_Events);
    }
    // 参加の希望 (Ask to Join) に答える
    void RespondJoinRequest(const std::string& userid, bool accept) {
        nlohmann::json cmd = { { "cmd", accept ? "SEND_ACTIVITY_JOIN_INVITE" : "CLOSE_ACTIVITY_REQUEST" }, { "args", { { "user_id", userid } } } };
        std::lock_guard lock(m_Mutex);
        m_Commands.push_back(std::move(cmd));
    }

private:
    DiscordRpc() = default;
    ~DiscordRpc() { Stop(); }

    enum Op : uint32_t { Handshake = 0, Frame = 1, Close = 2, Ping = 3, Pong = 4 };
    // Discord の制限 (20 秒に 5 回) に収まるよう、表示の更新は 4 秒以上あける
    static constexpr auto ActivityInterval = std::chrono::milliseconds(4000);

    // 参加ボタンから Discord がゲームを起動できるように、discord-<ID> のプロトコルを登録する (HKCU)
    static void RegisterLaunchCommand(const std::string& clientid) {
        wchar_t exe[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0) { return; }
        const std::wstring id(clientid.begin(), clientid.end());
        const std::wstring key = L"Software\\Classes\\discord-" + id;
        const std::wstring command = L"\"" + std::wstring(exe) + L"\"";
        const std::wstring name = L"URL:Run game " + id + L" protocol";
        HKEY hkey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hkey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hkey, nullptr, 0, REG_SZ, (const BYTE*)name.c_str(), (DWORD)((name.size() + 1) * sizeof(wchar_t)));
            RegSetValueExW(hkey, L"URL Protocol", 0, REG_SZ, (const BYTE*)L"", sizeof(wchar_t));
            RegCloseKey(hkey);
        }
        if (RegCreateKeyExW(HKEY_CURRENT_USER, (key + L"\\shell\\open\\command").c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hkey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hkey, nullptr, 0, REG_SZ, (const BYTE*)command.c_str(), (DWORD)((command.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(hkey);
        }
    }

    bool Connect() {
        for (int i = 0; i < 10 && !m_Stop; ++i) {
            const std::wstring name = L"\\\\.\\pipe\\" + PipePrefix + std::to_wstring(i);
            HANDLE pipe = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (pipe != INVALID_HANDLE_VALUE) {
                m_Pipe = pipe;
                return Write(Handshake, { { "v", 1 }, { "client_id", m_ClientId } });
            }
        }
        return false;
    }
    void Disconnect() {
        if (m_Pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(m_Pipe);
            m_Pipe = INVALID_HANDLE_VALUE;
        }
        m_Connected = false;
        m_Buffer.clear();
    }
    bool Write(uint32_t op, const nlohmann::json& body) {
        const std::string text = body.dump();
        std::vector<char> frame(8 + text.size());
        const uint32_t head[2] = { op, (uint32_t)text.size() };
        std::memcpy(frame.data(), head, 8);
        std::memcpy(frame.data() + 8, text.data(), text.size());
        DWORD written = 0;
        return WriteFile(m_Pipe, frame.data(), (DWORD)frame.size(), &written, nullptr) && written == frame.size();
    }
    bool Send(nlohmann::json cmd) {
        cmd["nonce"] = std::to_string(++m_Nonce);
        return Write(Frame, cmd);
    }
    // 届いているフレームを読む (ブロックしない)。切断されたら false
    bool ReadFrames() {
        DWORD available = 0;
        if (!PeekNamedPipe(m_Pipe, nullptr, 0, nullptr, &available, nullptr)) { return false; }
        if (available > 0) {
            const size_t old = m_Buffer.size();
            m_Buffer.resize(old + available);
            DWORD read = 0;
            if (!ReadFile(m_Pipe, m_Buffer.data() + old, available, &read, nullptr)) { return false; }
            m_Buffer.resize(old + read);
        }
        while (m_Buffer.size() >= 8) {
            uint32_t head[2];
            std::memcpy(head, m_Buffer.data(), 8);
            if (head[1] > 1024 * 1024) { return false; }
            if (m_Buffer.size() < 8 + (size_t)head[1]) { break; }
            const std::string text(m_Buffer.begin() + 8, m_Buffer.begin() + 8 + head[1]);
            m_Buffer.erase(m_Buffer.begin(), m_Buffer.begin() + 8 + head[1]);
            if (head[0] == Close) { return false; }
            if (head[0] == Ping) { Write(Pong, nlohmann::json::parse(text, nullptr, false)); continue; }
            if (head[0] == Frame) { HandleFrame(nlohmann::json::parse(text, nullptr, false)); }
        }
        return true;
    }
    void HandleFrame(const nlohmann::json& msg) {
        if (!msg.is_object()) { return; }
        const std::string cmd = msg.value("cmd", "");
        const std::string evt = msg.contains("evt") && msg["evt"].is_string() ? msg["evt"].get<std::string>() : "";
        const nlohmann::json data = msg.contains("data") ? msg["data"] : nlohmann::json();
        if (cmd != "DISPATCH") { return; }
        if (evt == "READY") {
            m_Connected = true;
            Send({ { "cmd", "SUBSCRIBE" }, { "evt", "ACTIVITY_JOIN" } });
            Send({ { "cmd", "SUBSCRIBE" }, { "evt", "ACTIVITY_JOIN_REQUEST" } });
            std::lock_guard lock(m_Mutex);
            m_ActivityDirty = true;     // 接続し直したら表示を送り直す
        }
        else if (evt == "ACTIVITY_JOIN" && data.is_object()) {
            std::lock_guard lock(m_Mutex);
            m_Events.push_back({ Event::Type::Join, data.value("secret", ""), "", "" });
        }
        else if (evt == "ACTIVITY_JOIN_REQUEST" && data.is_object() && data.contains("user") && data["user"].is_object()) {
            const auto& user = data["user"];
            std::lock_guard lock(m_Mutex);
            m_Events.push_back({ Event::Type::JoinRequest, "", user.value("id", ""), user.value("username", "") });
        }
    }

    void ThreadMain() {
        using Clock = std::chrono::steady_clock;
        Clock::time_point retry{}, lastactivity{};
        while (!m_Stop) {
            if (m_Pipe == INVALID_HANDLE_VALUE) {
                if (Clock::now() >= retry) {
                    if (!Connect()) { Disconnect(); }
                    retry = Clock::now() + std::chrono::seconds(5);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            if (!ReadFrames()) {
                Disconnect();
                continue;
            }
            if (m_Connected) {
                std::vector<nlohmann::json> commands;
                std::optional<nlohmann::json> activity;
                {
                    std::lock_guard lock(m_Mutex);
                    commands.swap(m_Commands);
                    if (m_ActivityDirty && Clock::now() - lastactivity >= ActivityInterval) {
                        activity = m_Activity;
                        m_ActivityDirty = false;
                    }
                }
                bool ok = true;
                for (auto& cmd : commands) { ok = ok && Send(std::move(cmd)); }
                if (activity) {
                    nlohmann::json args = { { "pid", (int)GetCurrentProcessId() } };
                    if (!activity->is_null()) { args["activity"] = *activity; }
                    ok = ok && Send({ { "cmd", "SET_ACTIVITY" }, { "args", args } });
                    lastactivity = Clock::now();
                }
                if (!ok) { Disconnect(); continue; }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (m_Pipe != INVALID_HANDLE_VALUE && m_Connected) {
            Send({ { "cmd", "SET_ACTIVITY" }, { "args", { { "pid", (int)GetCurrentProcessId() } } } });     // 表示を消す
        }
        Disconnect();
    }

    std::string m_ClientId;
    std::thread m_Thread;
    std::atomic_bool m_Stop = false;
    std::atomic_bool m_Connected = false;
    HANDLE m_Pipe = INVALID_HANDLE_VALUE;   // 通信スレッド専用
    std::vector<char> m_Buffer;
    uint64_t m_Nonce = 0;

    std::mutex m_Mutex;
    nlohmann::json m_Activity;
    bool m_ActivityDirty = false;
    std::vector<nlohmann::json> m_Commands;
    std::vector<Event> m_Events;
};

#endif
