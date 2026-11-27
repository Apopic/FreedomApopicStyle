// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    // ===================================================================== オンラインプレイ
    // サーバーとの通信は fasnet::Connection の送受信スレッドが行い、
    // メインスレッドは OnlineUpdate で受信済みメッセージを処理するだけにする。

    struct OnlineConnectTask {
        std::atomic<int> State = 0;     // 0: 接続中, 1: 成功, 2: 失敗
        fasnet::NetSocket Sock;
    };

    struct OnlineData {
        fasnet::Connection Net;
        std::shared_ptr<OnlineConnectTask> Connecting;
        uint32_t MyId = 0;
        fasnet::RoomState Room;
        std::string Status;                         // 接続失敗・切断などの表示
        std::optional<fasnet::SongData> Song;       // ホストから配布された曲 (メモリ上のみ。読み込み後に消去する)
        fasnet::SongReceiver Incoming;              // ストリーム受信中の曲
        std::string Target;                         // 接続先 (表示用)
        int JoinMenu = 0;                           // 0: サーバーに接続, 1: 接続先を変更, 2: サーバーを起動してホスト
        std::vector<fasnet::AddressEntry> Addresses;    // このサーバーの接続先 (サーバーから受け取る。招待用)
        int AddressIndex = 0;                       // 表示している接続先
        std::string Notice;                         // 待機室に一時的に出すお知らせ
        std::chrono::steady_clock::time_point NoticeUntil{};
        bool AddressEditing = false;                // 「アドレス:ポート」の入力中
        int AddressHandle = 0;
        uint32_t PreviewSerial = 0;                 // ロビーでプレビューを開始した曲 (演奏とは独立した状態)
        uint32_t LoadedSerial = 0;                  // 読み込み済みの曲の通し番号
        uint32_t FailedSerial = 0;                  // 読み込みに失敗した曲 (再試行しない)
        bool SongRequested = false;                 // ホストが曲を送信し、受理待ち
        double SongSpeed = 1.0;
        bool StartPending = false;
        std::chrono::steady_clock::time_point StartAt{};
        double RttMs = 0.0;
        std::chrono::steady_clock::time_point LastPing{};
        std::vector<uint32_t> LaneIds;              // 演奏中のレーン番号 -> PlayerId (0 番は自分)
        bool FinishSent = false;
        bool TransferMode = false;                  // ホスト移譲の相手を選んでいる
        int Cursor = 0;
    } Online;

    std::vector<fasnet::Connection::Message> OnlineInbox;

    bool IsMulti = false;   // サーバーのルームに参加している

    static uint64_t SteadyMs();
    const fasnet::PlayerInfo* MyPlayer() const;
    bool IsHost() const;
    // 演奏画面に並べるレーン数
    size_t LaneCount() const;
    static fasnet::ScoreState ToScore(const JudgeData& judge);
    static void FromScore(JudgeData& judge, const fasnet::ScoreState& score);

    // 画面に出す接続先: IP アドレス・ホスト名は伏せてポートだけ出す (配信・スクリーンショットで漏れないように。C のコピーでは実際の値)
    static std::string HiddenAddress(uint16_t port);
    void OnlineConnect(const std::string& address, uint16_t port);
    void OnlineConnectByAddress();
#ifndef __ANDROID__
    // サーバー (FreedomApopicStyleServer.exe。古い名前は MultiServer.exe) の場所: ゲームと同じフォルダ、またはビルド出力の MultiServer フォルダ
    static std::optional<fs::path> FindMultiServer();
    // この PC でサーバーを起動 (起動済みならそのまま) して接続し、ホストになる。
    // サーバーはゲームが終了してルームが空になると自動で終了する
    void OnlineHostLocal();
#endif
    // 「アドレス:ポート」(ポート省略時は設定のポート) を設定に保存して接続する
    bool OnlineSetTarget(const std::string& text);
    void OnlineUpdatePreview();
    void OnlineStopPreview();
    void OnlineWipeSong();
    void OnlineDisconnect();
    // 接続が切れた: オンライン関連の画面にいれば待機室へ戻して理由を表示する
    void OnlineLost(const std::string& message);

    void OnlineUpdate();

    void HandleOnlineMessage(const fasnet::Connection::Message& msg);

    void OnRoomChanged(fasnet::RoomPhase prev);

    void OnOnlineStart(uint32_t delayms);

    void CopyRoomScores();

#ifndef __ANDROID__
    static constexpr int JoinMenuCount = 3;
#else
    static constexpr int JoinMenuCount = 2;     // サーバーの起動は Windows のみ
#endif
#endif // FAS_GAME_MEMBER_DECLARATIONS
