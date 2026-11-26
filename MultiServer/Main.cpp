// FreedomApopicStyle MultiServer
// 1 ルーム (最大 4 人) を管理するオンラインプレイ用サーバー (GUI)。
//
// 使い方: MultiServer.exe [ポート番号] [--public <IPv4 アドレス>] [--autostart] [--minimized] [--parent <プロセス ID>]
//   ポート番号   : 待ち受けポート (省略時は前回の設定)
//   --public     : 外部 (インターネット) から参加してもらう時のアドレス (接続先の一覧に表示する)
//   --autostart  : 起動と同時に待ち受けを開始する
//   --minimized  : 最小化して起動する
//   --parent     : 指定したプロセス (ゲーム本体) が終了し、ルームが空になったら自動で終了する
//
// ゲーム本体の「サーバーを起動してホストになる」は --autostart --minimized --parent を付けて起動する。
// 通信はすべて暗号化され (Library/Net.hpp)、配布される譜面・音源はメモリ上だけで扱いディスクには書き込まない。

#include "../Library/Net.hpp"
#include <deque>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace fasnet;
using Clock = std::chrono::steady_clock;

namespace {

    constexpr auto HelloTimeout = std::chrono::seconds(10);
    constexpr auto RejectLinger = std::chrono::milliseconds(500);
    constexpr auto PublishInterval = std::chrono::milliseconds(500);
    constexpr size_t MaxLogs = 200;

    // ------------------------------------------------------------ 文字コード

    std::wstring Widen(const std::string& s) {
        if (s.empty()) { return {}; }
        const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring out(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
        return out;
    }
    std::string Narrow(const std::wstring& s) {
        if (s.empty()) { return {}; }
        const int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
        std::string out(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
        return out;
    }

    // ------------------------------------------------------------ サーバー

    struct Client {
        uint32_t Id = 0;                    // Hello を受理するまでは 0
        std::unique_ptr<Connection> Conn;
        Clock::time_point ConnectedAt = Clock::now();
        std::optional<Clock::time_point> CloseAt;   // Reject 送信後、この時刻に切断する
    };

    struct ServerOptions {
        uint16_t Port = DefaultPort;
        std::optional<std::array<uint8_t, 4>> PublicIp;
    };

    // 画面表示用の状態 (サーバーのスレッドが書き、画面のスレッドが読む)
    struct Snapshot {
        uint64_t Serial = 0;
        bool Running = false;
        uint16_t Port = 0;
        bool HasPublic = false;
        std::chrono::seconds Uptime{};
        std::string Room;
        std::vector<AddressEntry> Addresses;
        std::vector<std::string> Players;
        size_t PlayerCount = 0;
        std::deque<std::string> Logs;
    };

    std::optional<std::array<uint8_t, 4>> ParseIPv4(const std::string& str) {
        std::array<uint8_t, 4> ip{};
        int parts = 0;
        size_t pos = 0;
        while (parts < 4) {
            size_t end = str.find('.', pos);
            const std::string part = str.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
            if (part.empty() || part.size() > 3 || !std::ranges::all_of(part, [](char c) { return c >= '0' && c <= '9'; })) {
                return std::nullopt;
            }
            const int v = std::stoi(part);
            if (v > 255) { return std::nullopt; }
            ip[parts++] = (uint8_t)v;
            if (end == std::string::npos) { break; }
            pos = end + 1;
        }
        if (parts != 4 || str.find('.', pos) != std::string::npos) {
            return std::nullopt;
        }
        return ip;
    }
    std::optional<uint16_t> ParsePort(const std::string& str) {
        try {
            size_t pos = 0;
            const int port = std::stoi(str, &pos);
            if (pos == str.size() && port > 0 && port <= 65535) {
                return static_cast<uint16_t>(port);
            }
        }
        catch (...) {
        }
        return std::nullopt;
    }

    // この PC の IPv4 アドレス (LAN の接続先として表示するため)
    std::vector<std::array<uint8_t, 4>> LocalAddresses() {
        std::vector<std::array<uint8_t, 4>> out;
        char host[256]{};
        if (gethostname(host, sizeof(host)) != 0) {
            return out;
        }
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        if (getaddrinfo(host, nullptr, &hints, &res) != 0) {
            return out;
        }
        for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
            const auto* addr = reinterpret_cast<const sockaddr_in*>(p->ai_addr);
            std::array<uint8_t, 4> ip{};
            std::memcpy(ip.data(), &addr->sin_addr, 4);
            if (ip[0] != 127 && std::ranges::find(out, ip) == out.end()) {
                out.push_back(ip);
            }
        }
        freeaddrinfo(res);
        return out;
    }

    std::string IpText(const std::array<uint8_t, 4>& ip) {
        return std::format("{}.{}.{}.{}", ip[0], ip[1], ip[2], ip[3]);
    }
    // ルーターの内側のアドレス (192.168.x.x / 10.x.x.x / 172.16～31.x.x)
    bool IsPrivateIp(const std::array<uint8_t, 4>& ip) {
        return ip[0] == 10 || (ip[0] == 192 && ip[1] == 168) || (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31);
    }

    class Server {
    public:
        explicit Server(ServerOptions options) : m_Options(std::move(options)) {}

        bool Start(std::string& error) {
            // Listen 内の Bind は失敗を無視するため (使用中のポートだと別ポートで待ち受けてしまう)、先に確認する
            if (!m_Listener.Bind(IPAddress(m_Options.Port)) || !m_Listener.Listen(m_Options.Port)) {
                error = std::format("ポート {} で待ち受けを開始できませんでした (他のプログラムが使用中の可能性があります)", m_Options.Port);
                return false;
            }
            BuildAddresses();
            m_StartedAt = Clock::now();
            Log(std::format("ポート {} でサーバーを開始しました", m_Options.Port));
            return true;
        }

        // Stop() が呼ばれるまで動く (専用スレッドで呼ぶ)
        void Run() {
            std::vector<Connection::Message> messages;
            while (!m_Stop.load()) {
                AcceptClients();

                for (auto& client : m_Clients) {
                    client.Conn->Poll(messages);
                    for (auto& msg : messages) {
                        Handle(client, msg);
                    }
                }

                RemoveClosedClients();

                if (m_Dirty) {
                    BroadcastRoom();
                    m_Dirty = false;
                    m_PublishDirty = true;
                }
                Publish(false);

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            for (auto& client : m_Clients) {
                client.Conn->Close();
            }
            m_Clients.clear();
            Log("サーバーを停止しました");
            Publish(true);
        }
        void Stop() { m_Stop = true; }

        Snapshot GetSnapshot() const {
            std::lock_guard lock(m_SnapshotMutex);
            return m_Snapshot;
        }
        size_t PlayerCount() const {
            std::lock_guard lock(m_SnapshotMutex);
            return m_Snapshot.PlayerCount;
        }

    private:
        Welcome MakeWelcome(uint32_t id) const {
            Welcome welcome;
            welcome.PlayerId = id;
            welcome.Addresses = m_Addresses;
            return welcome;
        }

        void BuildAddresses() {
            m_Addresses.clear();
            auto add = [&](const char* label, const std::array<uint8_t, 4>& ip) {
                m_Addresses.push_back({ label, IpText(ip), m_Options.Port });
            };
            if (m_Options.PublicIp) {
                add("外部", *m_Options.PublicIp);
            }
            for (const auto& ip : LocalAddresses()) {
                add("LAN", ip);
            }
            add("この PC", { 127, 0, 0, 1 });
        }

        // ------------------------------------------------------------ 接続管理

        void AcceptClients() {
            while (auto sock = m_Listener.Accept()) {
                Client client;
                client.Conn = std::make_unique<Connection>();
                SessionConfig config;
                config.IsServer = true;
                client.Conn->Start(NetSocket(std::move(*sock)), config);
                m_Clients.push_back(std::move(client));
            }
        }

        void Reject(Client& client, const std::string& reason) {
            client.Conn->Send(Msg::Reject, Text{ reason });
            client.CloseAt = Clock::now() + RejectLinger;
        }

        void RemoveClosedClients() {
            const auto now = Clock::now();
            for (auto it = m_Clients.begin(); it != m_Clients.end(); ) {
                bool remove = !it->Conn->IsAlive();
                if (it->CloseAt && now >= *it->CloseAt) {
                    remove = true;
                }
                if (it->Id == 0 && !it->CloseAt && now - it->ConnectedAt > HelloTimeout) {
                    remove = true;  // 鍵交換や Hello を済ませない接続
                }
                if (!remove) {
                    ++it;
                    continue;
                }
                it->Conn->Close();
                if (it->Id != 0) {
                    OnLeave(it->Id);
                }
                it = m_Clients.erase(it);
            }
        }

        void OnLeave(uint32_t id) {
            auto pit = std::ranges::find(m_Room.Players, id, &PlayerInfo::Id);
            if (pit == m_Room.Players.end()) {
                return;
            }
            Log(pit->Name + " が退出しました");
            const bool washost = pit->IsHost;
            m_Room.Players.erase(pit);

            if (m_Room.Players.empty()) {
                ResetRoom();
            }
            else {
                if (washost) {
                    m_Room.Players.front().IsHost = true;
                    Log(m_Room.Players.front().Name + " がホストになりました");
                }
                // 配布の途中で送信元が抜けた場合は曲の選択を取り消す
                if (id == m_SongOwner && m_Song.IsActive() && !m_SongComplete) {
                    Log("曲データの送信が中断されました");
                    ReturnToLobby();
                }
                CheckAllFinished();
            }
            m_Dirty = true;
        }

        void ResetRoom() {
            m_Room = RoomState();
            ClearSong();
        }
        void ClearSong() {
            m_Song.Clear();
            m_SongComplete = false;
            m_SongOwner = 0;
        }
        void ReturnToLobby() {
            m_Room.Phase = RoomPhase::Lobby;
            m_Room.SongSerial = 0;
            m_Room.Title.clear();
            m_Room.Subtitle.clear();
            ClearSong();
            for (auto& p : m_Room.Players) {
                p.State = PlayerState::Lobby;
            }
            m_Dirty = true;
        }

        // ------------------------------------------------------------ メッセージ処理

        void Handle(Client& client, const Connection::Message& msg) {
            if (client.CloseAt) {
                return;
            }
            if (client.Id == 0) {
                if (msg.Type == Msg::Hello) {
                    OnHello(client, msg.Payload);
                }
                return;     // 参加前は Hello 以外を受け付けない
            }

            PlayerInfo* player = m_Room.Find(client.Id);
            if (player == nullptr) {
                return;
            }

            switch (msg.Type) {
            case Msg::Ping:
                client.Conn->Send(Msg::Pong, msg.Payload);
                break;
            case Msg::SetName: {
                Text text;
                if (Decode(msg.Payload, text)) {
                    std::string name = SanitizeText(text.Value, MaxNameLength);
                    if (!name.empty() && name != player->Name) {
                        Log(player->Name + " が名前を " + name + " に変更しました");
                        player->Name = std::move(name);
                        m_Dirty = true;
                    }
                }
                break;
            }
            case Msg::SetTitle: {
                Text text;
                if (Decode(msg.Payload, text)) {
                    std::string title = SanitizeText(text.Value, MaxNameLength);
                    if (title != player->Title) {
                        player->Title = std::move(title);
                        m_Dirty = true;
                    }
                }
                break;
            }
            case Msg::SongBegin:
                OnSongBegin(client, *player, msg.Payload);
                break;
            case Msg::SongChunk:
                OnSongChunk(client, msg.Payload);
                break;
            case Msg::SongEnd:
                OnSongEnd(client);
                break;
            case Msg::Ready: {
                // 読み込んだ曲の通し番号を送ってもらい、古い曲の Ready を新しい曲の分と取り違えないようにする
                U32Value serial;
                if (m_Room.Phase == RoomPhase::SongSelected && Decode(msg.Payload, serial)) {
                    player->State = (serial.Value != 0 && serial.Value == m_Room.SongSerial) ? PlayerState::Ready : PlayerState::Loading;
                    m_Dirty = true;
                }
                break;
            }
            case Msg::StartRequest:
                OnStartRequest(*player);
                break;
            case Msg::Hit:
                OnHit(client, *player, msg.Payload);
                break;
            case Msg::Finish: {
                ScoreState score;
                if (m_Room.Phase == RoomPhase::Playing && Decode(msg.Payload, score)) {
                    player->Score = score;
                    player->State = PlayerState::Finished;
                    CheckAllFinished();
                    m_Dirty = true;
                }
                break;
            }
            case Msg::ReturnLobby:
                if (player->IsHost) {
                    ReturnToLobby();
                }
                break;
            case Msg::TransferHost: {
                U32Value target;
                if (player->IsHost && Decode(msg.Payload, target)) {
                    if (PlayerInfo* next = m_Room.Find(target.Value); next != nullptr && next != player) {
                        player->IsHost = false;
                        next->IsHost = true;
                        Log(next->Name + " がホストになりました");
                        m_Dirty = true;
                    }
                }
                break;
            }
            default:
                break;  // 不明なメッセージは無視する
            }
        }

        void OnHello(Client& client, const std::vector<uint8_t>& payload) {
            Hello hello;
            if (!Decode(payload, hello)) {
                Reject(client, "不正な接続要求です");
                return;
            }
            if (hello.Version != ProtocolVersion) {
                Reject(client, std::format("バージョンが一致しません (サーバー: {}, クライアント: {})", ProtocolVersion, hello.Version));
                return;
            }
            if (m_Room.Players.size() >= MaxPlayers) {
                Reject(client, "ルームが満員です");
                return;
            }
            if (m_Room.Phase == RoomPhase::Playing) {
                Reject(client, "演奏中のため参加できません");
                return;
            }

            client.Id = m_NextId++;
            PlayerInfo player;
            player.Id = client.Id;
            player.Name = SanitizeText(hello.Name, MaxNameLength);
            player.Title = SanitizeText(hello.Title, MaxNameLength);
            if (player.Name.empty()) {
                player.Name = std::format("Player{}", client.Id);
            }
            player.IsHost = std::ranges::none_of(m_Room.Players, &PlayerInfo::IsHost);
            player.State = m_Room.Phase == RoomPhase::SongSelected ? PlayerState::Loading : PlayerState::Lobby;
            m_Room.Players.push_back(player);

            // 参加者にも接続先を渡し、ゲームの画面から他の人を招待できるようにする
            client.Conn->Send(Msg::Welcome, MakeWelcome(client.Id));
            // 曲の配布中に参加した場合は、受信済みの分を送り、残りは届き次第転送する
            if (m_Room.Phase == RoomPhase::SongSelected && m_Song.IsActive()) {
                const SongInfo& info = m_Song.Info();
                client.Conn->Send(Msg::SongBegin, Encode(info));
                for (auto& chunk : EncodeSongChunks(info.Serial, m_Song.ChartData(), m_Song.AudioData())) {
                    client.Conn->Send(Msg::SongChunk, std::move(chunk), true);
                }
                if (m_SongComplete) {
                    client.Conn->Send(Msg::SongEnd, Encode(U32Value{ info.Serial }), true);
                }
            }
            Log(player.Name + " が参加しました");
            m_Dirty = true;
        }

        void SendToGuests(uint32_t owner, Msg type, const std::vector<uint8_t>& payload, bool bulk) {
            for (auto& other : m_Clients) {
                if (other.Id != 0 && other.Id != owner && !other.CloseAt) {
                    other.Conn->Send(type, payload, bulk);
                }
            }
        }

        void OnSongBegin(Client& client, PlayerInfo& player, const std::vector<uint8_t>& payload) {
            if (!player.IsHost || m_Room.Phase == RoomPhase::Playing) {
                return;
            }
            SongInfo info;
            if (!Decode(payload, info)) {
                return;
            }
            info.Serial = m_NextSerial++;
            info.Title = SanitizeText(info.Title, MaxTextLength);
            info.Subtitle = SanitizeText(info.Subtitle, MaxTextLength);

            ClearSong();
            m_Song.Begin(info);
            m_SongOwner = client.Id;

            m_Room.Phase = RoomPhase::SongSelected;
            m_Room.SongSerial = info.Serial;
            m_Room.Title = info.Title;
            m_Room.Subtitle = info.Subtitle;
            m_Room.CourseIndex = info.CourseIndex;
            m_Room.Level = info.Level;
            m_Room.SongSpeed = info.SongSpeed;
            for (auto& p : m_Room.Players) {
                p.State = PlayerState::Loading;
                p.Score = ScoreState();
            }

            SendToGuests(client.Id, Msg::SongBegin, Encode(info), false);
            Log(std::format("{} が曲を選択しました: {} ({:.1f} MB)", player.Name, info.Title, (info.ChartSize + (double)info.AudioSize) / (1024.0 * 1024.0)));
            m_Dirty = true;
        }

        void OnSongChunk(Client& client, const std::vector<uint8_t>& payload) {
            if (client.Id != m_SongOwner || !m_Song.IsActive() || m_SongComplete) {
                return;
            }
            SongChunk chunk;
            if (!Decode(payload, chunk)) {
                return;
            }
            chunk.Serial = m_Song.Info().Serial;    // ホストは番号を知らないのでサーバーが付ける
            if (!m_Song.Append(chunk)) {
                Log("曲データが不正なため配布を中止しました");
                ReturnToLobby();
                return;
            }
            SendToGuests(client.Id, Msg::SongChunk, Encode(chunk), true);
            crypto::SecureZero(chunk.Data);
            m_PublishDirty = true;
        }

        void OnSongEnd(Client& client) {
            if (client.Id != m_SongOwner || !m_Song.IsActive() || m_SongComplete) {
                return;
            }
            if (!m_Song.IsComplete()) {
                Log("曲データが不足しているため配布を中止しました");
                ReturnToLobby();
                return;
            }
            m_SongComplete = true;
            SendToGuests(client.Id, Msg::SongEnd, Encode(U32Value{ m_Song.Info().Serial }), true);
            Log("曲データの受信が完了しました");
        }

        void OnStartRequest(const PlayerInfo& player) {
            if (!player.IsHost || m_Room.Phase != RoomPhase::SongSelected || !m_SongComplete) {
                return;
            }
            if (!std::ranges::all_of(m_Room.Players, [](const PlayerInfo& p) { return p.State == PlayerState::Ready; })) {
                return;
            }
            m_Room.Phase = RoomPhase::Playing;
            for (auto& p : m_Room.Players) {
                p.State = PlayerState::Playing;
                p.Score = ScoreState();
            }
            for (auto& client : m_Clients) {
                if (client.Id != 0 && !client.CloseAt) {
                    client.Conn->Send(Msg::Start, StartInfo{ StartDelayMs });
                }
            }
            Log("演奏を開始しました: " + m_Room.Title);
            m_Dirty = true;
        }

        void OnHit(Client& client, PlayerInfo& player, const std::vector<uint8_t>& payload) {
            if (m_Room.Phase != RoomPhase::Playing) {
                return;
            }
            HitEvent hit;
            if (!Decode(payload, hit)) {
                return;
            }
            hit.PlayerId = client.Id;   // 送信元を偽装させない
            player.Score = hit.Score;
            SendToGuests(client.Id, Msg::Hit, Encode(hit), false);
        }

        void CheckAllFinished() {
            if (m_Room.Phase != RoomPhase::Playing || m_Room.Players.empty()) {
                return;
            }
            if (std::ranges::all_of(m_Room.Players, [](const PlayerInfo& p) { return p.State == PlayerState::Finished; })) {
                m_Room.Phase = RoomPhase::Result;
                Log("全員の演奏が終了しました");
            }
        }

        void BroadcastRoom() {
            const auto payload = Encode(m_Room);
            for (auto& client : m_Clients) {
                if (client.Id != 0 && !client.CloseAt) {
                    client.Conn->Send(Msg::RoomState, payload);
                }
            }
        }

        // ------------------------------------------------------------ 画面へ渡す状態

        void Log(const std::string& text) {
            const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
            m_Logs.push_back(std::format("[{:%H:%M:%S}] {}", std::chrono::zoned_time{ std::chrono::current_zone(), now }, text));
            while (m_Logs.size() > MaxLogs) {
                m_Logs.pop_front();
            }
            m_PublishDirty = true;
        }

        static const char* PhaseName(RoomPhase phase) {
            switch (phase) {
            case RoomPhase::Lobby: return "待機中";
            case RoomPhase::SongSelected: return "読み込み待ち";
            case RoomPhase::Playing: return "演奏中";
            case RoomPhase::Result: return "リザルト";
            }
            return "?";
        }
        static const char* StateName(PlayerState state) {
            switch (state) {
            case PlayerState::Lobby: return "待機";
            case PlayerState::Loading: return "読込中";
            case PlayerState::Ready: return "準備完了";
            case PlayerState::Playing: return "演奏中";
            case PlayerState::Finished: return "終了";
            }
            return "?";
        }

        void Publish(bool stopped) {
            const auto now = Clock::now();
            if (!stopped && !m_PublishDirty && now - m_LastPublish < PublishInterval) {
                return;
            }
            m_PublishDirty = false;
            m_LastPublish = now;

            Snapshot snap;
            snap.Running = !stopped;
            snap.Port = m_Options.Port;
            snap.HasPublic = m_Options.PublicIp.has_value();
            snap.Uptime = std::chrono::floor<std::chrono::seconds>(now - m_StartedAt);
            snap.Room = PhaseName(m_Room.Phase);
            if (m_Room.Phase != RoomPhase::Lobby) {
                snap.Room += "  " + m_Room.Title;
            }
            if (m_Song.IsActive() && !m_SongComplete) {
                snap.Room += std::format("  (曲データ受信中 {:.0f}%)", 100.0 * m_Song.Received() / std::max<size_t>(m_Song.Total(), 1));
            }
            snap.Addresses = m_Addresses;
            snap.PlayerCount = m_Room.Players.size();
            for (size_t i = 0; i < m_Room.Players.size(); ++i) {
                const auto& p = m_Room.Players[i];
                snap.Players.push_back(std::format("{}P  {}{}  [{}]{}{}", i + 1, p.IsHost ? "★ " : "", p.Name, StateName(p.State),
                    p.Title.empty() ? "" : "  〈" + p.Title + "〉",
                    m_Room.Phase == RoomPhase::Playing || m_Room.Phase == RoomPhase::Result ? std::format("  スコア {}", p.Score.Score) : ""));
            }
            snap.Logs = m_Logs;

            std::lock_guard lock(m_SnapshotMutex);
            snap.Serial = m_Snapshot.Serial + 1;
            m_Snapshot = std::move(snap);
        }

        ServerOptions m_Options;
        TCPServer m_Listener;
        std::vector<AddressEntry> m_Addresses;
        std::vector<Client> m_Clients;
        RoomState m_Room;
        SongReceiver m_Song;                // 配布中・配布済みの曲 (途中参加者にも送るためメモリに保持する)
        bool m_SongComplete = false;
        uint32_t m_SongOwner = 0;
        uint32_t m_NextId = 1;
        uint32_t m_NextSerial = 1;
        bool m_Dirty = false;
        std::atomic_bool m_Stop = false;

        std::deque<std::string> m_Logs;
        bool m_PublishDirty = true;
        Clock::time_point m_StartedAt = Clock::now();
        Clock::time_point m_LastPublish{};
        mutable std::mutex m_SnapshotMutex;
        Snapshot m_Snapshot;
    };

    // ------------------------------------------------------------ 画面 (Win32)

    enum ControlId : int {
        IdPort = 100, IdPublic, IdStartStop, IdCodes, IdCopy, IdPlayers, IdLogs, IdStatus,
        IdLabelPort, IdLabelPublic, IdLabelCodes, IdLabelPlayers, IdLabelLogs, IdHint,
    };
    constexpr UINT_PTR RefreshTimer = 1;

    struct App {
        HWND Window = nullptr;
        HFONT Font = nullptr;
        UINT Dpi = 96;
        std::unique_ptr<Server> Srv;
        std::thread Thread;
        uint64_t ShownSerial = 0;
        bool WasRunning = false;
        std::wstring LastError;
        Snapshot Shown;
        HANDLE Parent = nullptr;            // --parent のプロセス
        bool ParentGone = false;
        std::wstring IniPath;
    } g;

    HWND Ctrl(int id) { return GetDlgItem(g.Window, id); }
    int Scale(int v) { return MulDiv(v, (int)g.Dpi, 96); }

    void SetText(int id, const std::wstring& text) {
        wchar_t buf[512]{};
        GetWindowTextW(Ctrl(id), buf, 512);
        if (text != buf) { SetWindowTextW(Ctrl(id), text.c_str()); }
    }
    std::wstring GetText(int id) {
        const int n = GetWindowTextLengthW(Ctrl(id));
        std::wstring s(n + 1, L'\0');
        GetWindowTextW(Ctrl(id), s.data(), n + 1);
        s.resize(n);
        return s;
    }

    // 内容が変わった時だけ一覧を作り直す (選択位置を保つ。ログは末尾へスクロール)
    void SetList(int id, const std::vector<std::string>& items, bool scrolltoend) {
        HWND list = Ctrl(id);
        const int sel = (int)SendMessageW(list, LB_GETCURSEL, 0, 0);
        SendMessageW(list, WM_SETREDRAW, FALSE, 0);
        SendMessageW(list, LB_RESETCONTENT, 0, 0);
        for (const auto& item : items) {
            SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)Widen(item).c_str());
        }
        if (scrolltoend && !items.empty()) {
            SendMessageW(list, LB_SETTOPINDEX, items.size() - 1, 0);
        }
        else if (sel >= 0 && sel < (int)items.size()) {
            SendMessageW(list, LB_SETCURSEL, sel, 0);
        }
        SendMessageW(list, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(list, nullptr, TRUE);
    }

    void UpdateFont() {
        if (g.Font) { DeleteObject(g.Font); }
        NONCLIENTMETRICSW ncm{ sizeof(ncm) };
        SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, g.Dpi);
        g.Font = CreateFontIndirectW(&ncm.lfMessageFont);
        for (HWND child = GetWindow(g.Window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            SendMessageW(child, WM_SETFONT, (WPARAM)g.Font, TRUE);
        }
    }

    void Layout() {
        RECT rc{};
        GetClientRect(g.Window, &rc);
        const int m = Scale(10), h = Scale(24), gap = Scale(6);
        const int w = rc.right - m * 2;
        int y = m;
        auto place = [](int id, int x, int y, int w, int h) { MoveWindow(Ctrl(id), x, y, std::max(w, 1), std::max(h, 1), TRUE); };

        int x = m;
        place(IdLabelPort, x, y + Scale(4), Scale(48), h); x += Scale(50);
        place(IdPort, x, y, Scale(70), h); x += Scale(80);
        place(IdLabelPublic, x, y + Scale(4), Scale(80), h); x += Scale(82);
        place(IdPublic, x, y, Scale(130), h); x += Scale(140);
        place(IdStartStop, std::max<int>(x, (int)rc.right - m - Scale(100)), y, Scale(100), h);
        y += h + gap;
        place(IdStatus, m, y, w, h);
        y += h + gap;
        place(IdHint, m, y, w, h);
        y += h + gap;

        place(IdLabelCodes, m, y, w - Scale(110), h);
        place(IdCopy, rc.right - m - Scale(100), y - Scale(2), Scale(100), h);
        y += h;
        const int codes = Scale(80);
        place(IdCodes, m, y, w, codes);
        y += codes + gap;

        place(IdLabelPlayers, m, y, w, h);
        y += h;
        const int players = Scale(90);
        place(IdPlayers, m, y, w, players);
        y += players + gap;

        place(IdLabelLogs, m, y, w, h);
        y += h;
        place(IdLogs, m, y, w, std::max<int>(rc.bottom - m - y, Scale(60)));
    }

    void SaveSettings() {
        WritePrivateProfileStringW(L"Server", L"Port", GetText(IdPort).c_str(), g.IniPath.c_str());
        WritePrivateProfileStringW(L"Server", L"Public", GetText(IdPublic).c_str(), g.IniPath.c_str());
    }

    bool IsRunning() { return g.Srv != nullptr; }

    void StopServer() {
        if (!g.Srv) { return; }
        g.Srv->Stop();
        if (g.Thread.joinable()) { g.Thread.join(); }
        g.Shown = g.Srv->GetSnapshot();
        g.Srv.reset();
    }

    void StartServer() {
        if (IsRunning()) { return; }
        ServerOptions options;
        const auto port = ParsePort(Narrow(GetText(IdPort)));
        if (!port) {
            g.LastError = L"ポート番号が正しくありません (1～65535)";
            return;
        }
        options.Port = *port;
        const std::string pub = Narrow(GetText(IdPublic));
        if (!pub.empty()) {
            options.PublicIp = ParseIPv4(pub);
            if (!options.PublicIp) {
                g.LastError = L"外部アドレスが正しくありません (例: 203.0.113.5)";
                return;
            }
        }
        auto server = std::make_unique<Server>(options);
        std::string error;
        if (!server->Start(error)) {
            g.LastError = Widen(error);
            return;
        }
        g.LastError.clear();
        SaveSettings();
        g.Srv = std::move(server);
        g.Thread = std::thread([srv = g.Srv.get()] { srv->Run(); });
    }

    void Refresh() {
        const bool running = IsRunning();
        if (running) {
            Snapshot snap = g.Srv->GetSnapshot();
            if (snap.Serial != g.ShownSerial || running != g.WasRunning) {
                g.ShownSerial = snap.Serial;
                std::vector<std::string> codes;
                for (const auto& a : snap.Addresses) {
                    codes.push_back(a.Label + "    " + a.Text());
                }
                SetList(IdCodes, codes, false);
                SetList(IdPlayers, snap.Players, false);
                if (snap.Logs != g.Shown.Logs) {
                    SetList(IdLogs, { snap.Logs.begin(), snap.Logs.end() }, true);
                }
                g.Shown = std::move(snap);
            }
        }
        else if (g.WasRunning) {
            SetList(IdCodes, {}, false);
            SetList(IdPlayers, {}, false);
            SetList(IdLogs, { g.Shown.Logs.begin(), g.Shown.Logs.end() }, true);
        }

        std::wstring status;
        if (!g.LastError.empty()) {
            status = L"エラー: " + g.LastError;
        }
        else if (running) {
            const auto& s = g.Shown;
            status = Widen(std::format("待ち受け中  ポート {}  /  稼働 {:%H:%M:%S}  /  ルーム: {}  /  プレイヤー {}/{}",
                s.Port, std::chrono::hh_mm_ss{ s.Uptime }.to_duration(), s.Room, s.PlayerCount, MaxPlayers));
        }
        else {
            status = L"停止中  (「開始」で待ち受けを始めます)";
        }
        SetText(IdStatus, status);
        SetText(IdHint, running && !g.Shown.HasPublic
            ? L"インターネットから参加してもらう時は、ルーターでポートを開放して外部アドレスを入れてください"
            : L"接続先を選んで「コピー」し、参加する人に伝えてください (ゲームのオンライン画面で入力します)");
        SetText(IdStartStop, running ? L"停止" : L"開始");
        for (int id : { IdPort, IdPublic }) {
            EnableWindow(Ctrl(id), !running);
        }
        EnableWindow(Ctrl(IdCopy), running && !g.Shown.Addresses.empty());
        g.WasRunning = running;

        // ゲーム本体から起動された場合: ゲームが終了し、ルームが空になったら自動で終了する
        if (g.Parent != nullptr && !g.ParentGone && WaitForSingleObject(g.Parent, 0) == WAIT_OBJECT_0) {
            g.ParentGone = true;
        }
        if (g.ParentGone && (!running || g.Srv->PlayerCount() == 0)) {
            StopServer();
            DestroyWindow(g.Window);
        }
    }

    void CopySelectedCode() {
        if (g.Shown.Addresses.empty()) { return; }
        int sel = (int)SendMessageW(Ctrl(IdCodes), LB_GETCURSEL, 0, 0);
        if (sel < 0 || sel >= (int)g.Shown.Addresses.size()) { sel = 0; }
        const std::wstring code = Widen(g.Shown.Addresses[sel].Text());
        if (!OpenClipboard(g.Window)) { return; }
        EmptyClipboard();
        const size_t bytes = (code.size() + 1) * sizeof(wchar_t);
        if (HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
            if (void* p = GlobalLock(mem)) {
                std::memcpy(p, code.c_str(), bytes);
                GlobalUnlock(mem);
                if (!SetClipboardData(CF_UNICODETEXT, mem)) { GlobalFree(mem); }
            }
            else {
                GlobalFree(mem);
            }
        }
        CloseClipboard();
        g.LastError.clear();
        SetText(IdHint, L"接続先をコピーしました: " + code);
    }

    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_CREATE: {
            g.Window = hwnd;
            g.Dpi = GetDpiForWindow(hwnd);
            const HINSTANCE inst = ((CREATESTRUCTW*)lp)->hInstance;
            auto make = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int id, DWORD ex = 0) {
                CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)id, inst, nullptr);
            };
            make(L"STATIC", L"ポート", 0, IdLabelPort);
            make(L"EDIT", L"", WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, IdPort, WS_EX_CLIENTEDGE);
            make(L"STATIC", L"外部アドレス", 0, IdLabelPublic);
            make(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, IdPublic, WS_EX_CLIENTEDGE);
            make(L"BUTTON", L"開始", WS_TABSTOP | BS_PUSHBUTTON, IdStartStop);
            make(L"STATIC", L"", SS_ENDELLIPSIS, IdStatus);
            make(L"STATIC", L"", SS_ENDELLIPSIS, IdHint);
            make(L"STATIC", L"接続先 (アドレス:ポート)", 0, IdLabelCodes);
            make(L"BUTTON", L"コピー", WS_TABSTOP | BS_PUSHBUTTON, IdCopy);
            make(L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, IdCodes, WS_EX_CLIENTEDGE);
            make(L"STATIC", L"プレイヤー", 0, IdLabelPlayers);
            make(L"LISTBOX", L"", WS_VSCROLL | LBS_NOSEL | LBS_NOINTEGRALHEIGHT, IdPlayers, WS_EX_CLIENTEDGE);
            make(L"STATIC", L"ログ", 0, IdLabelLogs);
            make(L"LISTBOX", L"", WS_VSCROLL | WS_HSCROLL | LBS_NOSEL | LBS_NOINTEGRALHEIGHT, IdLogs, WS_EX_CLIENTEDGE);
            SendMessageW(Ctrl(IdPort), EM_SETLIMITTEXT, 5, 0);
            SendMessageW(Ctrl(IdLogs), LB_SETHORIZONTALEXTENT, 2000, 0);
            UpdateFont();
            Layout();
            SetTimer(hwnd, RefreshTimer, 250, nullptr);
            return 0;
        }
        case WM_SIZE:
            Layout();
            return 0;
        case WM_DPICHANGED: {
            g.Dpi = HIWORD(wp);
            const RECT* r = (const RECT*)lp;
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            UpdateFont();
            Layout();
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* info = (MINMAXINFO*)lp;
            info->ptMinTrackSize = { Scale(860), Scale(460) };
            return 0;
        }
        case WM_TIMER:
            if (wp == RefreshTimer) { Refresh(); }
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
            case IdStartStop:
                if (IsRunning()) {
                    if (g.Srv->PlayerCount() > 0 && MessageBoxW(hwnd, L"接続中のプレイヤーがいます。サーバーを停止しますか?", L"MultiServer", MB_YESNO | MB_ICONQUESTION) != IDYES) {
                        break;
                    }
                    StopServer();
                }
                else {
                    StartServer();
                }
                Refresh();
                break;
            case IdCopy:
                CopySelectedCode();
                break;
            case IdCodes:
                if (HIWORD(wp) == LBN_DBLCLK) { CopySelectedCode(); }
                break;
            }
            return 0;
        case WM_CLOSE:
            if (IsRunning() && g.Srv->PlayerCount() > 0
                && MessageBoxW(hwnd, L"接続中のプレイヤーがいます。サーバーを終了しますか?", L"MultiServer", MB_YESNO | MB_ICONQUESTION) != IDYES) {
                return 0;
            }
            StopServer();
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, RefreshTimer);
            if (g.Font) { DeleteObject(g.Font); g.Font = nullptr; }
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // 設定ファイル (前回のポートなど) は実行ファイルと同じフォルダ
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    g.IniPath = exe;
    g.IniPath = g.IniPath.substr(0, g.IniPath.find_last_of(L"\\/") + 1) + L"MultiServer.ini";

    wchar_t buf[256]{};
    GetPrivateProfileStringW(L"Server", L"Port", std::to_wstring(DefaultPort).c_str(), buf, 256, g.IniPath.c_str());
    std::wstring port = buf;
    GetPrivateProfileStringW(L"Server", L"Public", L"", buf, 256, g.IniPath.c_str());
    std::wstring publicip = buf;
    bool autostart = false, minimized = false;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv != nullptr && i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--public" && i + 1 < argc) { publicip = argv[++i]; }
        else if (arg == L"--autostart") { autostart = true; }
        else if (arg == L"--minimized") { minimized = true; }
        else if (arg == L"--parent" && i + 1 < argc) {
            const DWORD pid = (DWORD)_wtoi(argv[++i]);
            g.Parent = pid != 0 ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
        }
        else if (ParsePort(Narrow(arg))) { port = arg; }
    }
    if (argv) { LocalFree(argv); }

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"FASMultiServer";
    RegisterClassExW(&wc);

    const std::wstring title = L"FreedomApopicStyle MultiServer (protocol " + std::to_wstring(ProtocolVersion) + L")";
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 620, nullptr, nullptr, inst, nullptr);
    if (hwnd == nullptr) {
        return 1;
    }
    // 作成後に DPI に合わせた大きさにする
    SetWindowPos(hwnd, nullptr, 0, 0, Scale(900), Scale(620), SWP_NOMOVE | SWP_NOZORDER);

    SetWindowTextW(Ctrl(IdPort), port.c_str());
    SetWindowTextW(Ctrl(IdPublic), publicip.c_str());

    ShowWindow(hwnd, minimized ? SW_SHOWMINNOACTIVE : show);
    UpdateWindow(hwnd);
    if (autostart) {
        StartServer();
        if (!g.LastError.empty() && minimized) {
            ShowWindow(hwnd, SW_RESTORE);   // 起動に失敗したら理由が見えるように戻す
        }
    }
    Refresh();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    StopServer();
    if (g.Parent) { CloseHandle(g.Parent); }
    return 0;
}
