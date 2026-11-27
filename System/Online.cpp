#include "../Game.hpp"

uint64_t Game::SteadyMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

const fasnet::PlayerInfo* Game::MyPlayer() const {
    return Online.Room.Find(Online.MyId);
}

bool Game::IsHost() const {
    const auto* me = MyPlayer();
    return IsMulti && me != nullptr && me->IsHost;
}

size_t Game::LaneCount() const {
    return IsMulti ? std::max<size_t>(Online.LaneIds.size(), 1) : 1;
}

fasnet::ScoreState Game::ToScore(const JudgeData& judge) {
    fasnet::ScoreState score;
    score.Score = judge.Score;
    score.Good = judge.Good;
    score.Ok = judge.Ok;
    score.Bad = judge.Bad;
    score.Roll = judge.Roll;
    score.Combo = judge.Combo;
    score.MaxCombo = judge.MaxCombo;
    score.HitNote = judge.HitNote;
    score.Accuracy = judge.Accuracy;
    return score;
}

void Game::FromScore(JudgeData& judge, const fasnet::ScoreState& score) {
    judge.Score = score.Score;
    judge.Good = score.Good;
    judge.Ok = score.Ok;
    judge.Bad = score.Bad;
    judge.Roll = score.Roll;
    judge.Combo = score.Combo;
    judge.MaxCombo = score.MaxCombo;
    judge.HitNote = score.HitNote;
    judge.Accuracy = score.Accuracy;
}

std::string Game::HiddenAddress(uint16_t port) { return "***:" + std::to_string(port); }

void Game::OnlineConnect(const std::string& address, uint16_t port) {
    OnlineDisconnect();
    auto task = std::make_shared<OnlineConnectTask>();
    Online.Connecting = task;
    Online.Target = HiddenAddress(port);
    std::thread([task, address, port] {
        bool ok = false;
        try {
            if (auto addr = IPAddress::SolveHostName(address)) ok = task->Sock.Connect(addr->Port(port));
        } catch (...) { ok = false; }
        task->State = ok ? 1 : 2;
        }).detach();
}

void Game::OnlineConnectByAddress() {
    OnlineConnect(Config.ServerAddress, Config.ServerPort);
}

#ifndef __ANDROID__
std::optional<fs::path> Game::FindMultiServer() {
    const fs::path exe = GetExecutablePath();
    for (const fs::path& path : { exe / "MultiServer.exe", exe / "MultiServer" / "MultiServer.exe", exe.parent_path() / "MultiServer" / "MultiServer.exe" }) {
        std::error_code ec;
        if (fs::is_regular_file(path, ec)) { return path.lexically_normal(); }
    }
    return std::nullopt;
}

void Game::OnlineHostLocal() {
    const auto server = FindMultiServer();
    if (!server) {
        OnlineDisconnect();
        Online.Status = "MultiServer.exe が見つかりません (ゲームと同じフォルダに置いてください)";
        return;
    }
    OnlineDisconnect();
    auto task = std::make_shared<OnlineConnectTask>();
    Online.Connecting = task;
    const uint16_t port = Config.ServerPort;
    Online.Target = "この PC:" + std::to_string(port);
    const std::wstring exe = server->wstring();
    const std::wstring dir = server->parent_path().wstring();
    const DWORD pid = GetCurrentProcessId();
    std::thread([task, port, exe, dir, pid] {
        try {
        auto addr = IPAddress::SolveHostName("127.0.0.1");
        if (!addr) { task->State = 2; return; }
        auto tryconnect = [&] { return task->Sock.Connect(addr->Port(port)); };
        if (tryconnect()) { task->State = 1; return; }     // 既に起動している

        std::wstring cmd = L"\"" + exe + L"\" " + std::to_wstring(port) + L" --autostart --minimized --parent " + std::to_wstring(pid);
        STARTUPINFOW si{ sizeof(si) };
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
            task->State = 2;
            return;
        }
        CloseHandle(pi.hThread);
        struct ProcessHandle { HANDLE Value; ~ProcessHandle() { CloseHandle(Value); } } process{pi.hProcess};
        // 待ち受けを始めるまで最大 8 秒待つ (サーバーが起動に失敗して終了したら諦める)
        const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(8);
        bool ok = false;
        while (!ok && std::chrono::steady_clock::now() < limit) {
            if (WaitForSingleObject(pi.hProcess, 200) == WAIT_OBJECT_0) { break; }
            task->Sock = fasnet::NetSocket();
            ok = tryconnect();
        }

        task->State = ok ? 1 : 2;
        } catch(...) { task->State=2; }
        }).detach();
}

#endif
bool Game::OnlineSetTarget(const std::string& text) {
    const auto target = fasnet::SplitHostPort(text, Config.ServerPort);
    if (!target) {
        OnlineDisconnect();
        Online.Status = "接続先が正しくありません (例: 192.168.0.2:8080)";
        return false;
    }
    Config.ServerAddress = target->first;
    Config.ServerPort = target->second;
    Config.Write();
    return true;
}

void Game::OnlineUpdatePreview() {
    // 読み込み済みの演奏用音源を再利用する。配布された譜面・音源のファイル化や再デコードは行わない。
    if (!IsMulti || NowScene != Scene::MultiRoom || Online.Room.Phase != fasnet::RoomPhase::SongSelected
        || Online.LoadedSerial == 0 || Online.LoadedSerial != Online.Room.SongSerial || Chart.SongData.Handle == -1) {
        OnlineStopPreview();
        return;
    }
    if (Online.PreviewSerial == Online.LoadedSerial) { return; }
    OnlineStopPreview();
    Online.PreviewSerial = Online.LoadedSerial;
    const double duration = std::max(0.0, snd::TotalMs(Chart.SongData.Handle) - Chart.SongData.StartMilliseconds);
    const double start = Chart.OriginalData.DemoStart * 1000.0;
    if (duration <= 0.0) { return; }
    Chart.SongData.SetVolume(numeric::Cast<float>(Chart.OriginalData.SongVolume * (Config.SongVolume / 100.0)));
    Chart.SongData.SetCurrent(std::isfinite(start) && start >= 0.0 && start < duration ? start : 0.0);
    Chart.SongData.Play(FALSE);
}

void Game::OnlineStopPreview() {
    if (Online.PreviewSerial == 0) { return; }
    Online.PreviewSerial = 0;
    if (Chart.SongData.Handle != -1) {
        Chart.SongData.Stop();
        Chart.SongData.SetCurrent(0.0);
    }
}

void Game::OnlineWipeSong() {
    OnlineStopPreview();
    if (Online.Song) {
        Online.Song->Wipe();
    }
    Online.Song.reset();
    Online.Incoming.Clear();
}

void Game::OnlineDisconnect() {
    Online.Net.Close();
    Online.Connecting.reset();     // 接続中のスレッドは完了後に自動で破棄される
    Online.MyId = 0;
    Online.Room = fasnet::RoomState();
    OnlineWipeSong();
    Online.LoadedSerial = 0;
    Online.FailedSerial = 0;
    Online.SongRequested = false;
    Online.StartPending = false;
    Online.LaneIds.clear();
    Online.FinishSent = false;
    Online.TransferMode = false;
    Online.Cursor = 0;
    Online.RttMs = 0.0;
    Online.Status.clear();
    Online.Addresses.clear();
    Online.AddressIndex = 0;
    Online.Notice.clear();
    IsMulti = false;
}

void Game::OnlineLost(const std::string& message) {
    OnlineDisconnect();
    Online.Status = message;
    if (NowScene == Scene::Playing || NowScene == Scene::Result || NowScene == Scene::Loading || NowScene == Scene::SongSelect) {
        Chart.Init(true);
        NowScene = Scene::MultiRoom;
    }
}

void Game::OnlineUpdate() {
    if (Online.Connecting) {
        const int state = Online.Connecting->State.load();
        if (state == 1) {
            fasnet::SessionConfig config;
            Online.Net.Start(std::move(Online.Connecting->Sock), config);
            Online.Connecting.reset();
            Online.Net.Send(fasnet::Msg::Hello, fasnet::Hello{ fasnet::ProtocolVersion, Config.PlayerName, Config.TitleName });
        }
        else if (state == 2) {
            Online.Connecting.reset();
            Online.Status = "サーバーに接続できませんでした (" + Online.Target + ")";
        }
    }
    if (!Online.Net.IsStarted()) {
        return;
    }
    // 切断の直前に届いた Reject (参加を断られた理由) を先に処理する
    Online.Net.Poll(OnlineInbox);
    for (const auto& msg : OnlineInbox) {
        HandleOnlineMessage(msg);
        if (!Online.Net.IsStarted()) {
            return;     // Reject などで切断した
        }
    }
    if (!Online.Net.IsAlive()) {
        OnlineLost(!IsMulti
            ? "参加できませんでした (サーバーが応答しないか、バージョンが違います)"
            : "サーバーとの接続が切れました");
        return;
    }

    // 演奏開始の同期に使う往復遅延を定期的に測る
    const auto now = std::chrono::steady_clock::now();
    if (IsMulti && now - Online.LastPing > std::chrono::seconds(2)) {
        Online.LastPing = now;
        Online.Net.Send(fasnet::Msg::Ping, fasnet::U64Value{ SteadyMs() });
    }
}

void Game::HandleOnlineMessage(const fasnet::Connection::Message& msg) {
    using fasnet::Msg;
    switch (msg.Type) {
    case Msg::Welcome: {
        fasnet::Welcome welcome;
        if (fasnet::Decode(msg.Payload, welcome)) {
            Online.MyId = welcome.PlayerId;
            Online.Status.clear();
            // 招待用の接続先。LAN のものを優先して表示する
            Online.Addresses = std::move(welcome.Addresses);
            Online.AddressIndex = 0;
            for (size_t i = 0; i < Online.Addresses.size(); ++i) {
                if (Online.Addresses[i].Label.starts_with("LAN")) { Online.AddressIndex = (int)i; break; }
            }
            IsMulti = true;
        }
        break;
    }
    case Msg::Reject: {
        fasnet::Text reason;
        fasnet::Decode(msg.Payload, reason);
        OnlineDisconnect();
        Online.Status = "参加できませんでした: " + reason.Value;
        break;
    }
    case Msg::RoomState: {
        fasnet::RoomState room;
        if (fasnet::Decode(msg.Payload, room)) {
            const fasnet::RoomPhase prev = Online.Room.Phase;
            Online.Room = std::move(room);
            OnRoomChanged(prev);
        }
        break;
    }
    case Msg::SongBegin: {
        fasnet::SongInfo info;
        if (fasnet::Decode(msg.Payload, info)) {
            OnlineWipeSong();
            Online.Incoming.Begin(info);
            Online.LoadedSerial = 0;
        }
        break;
    }
    case Msg::SongChunk: {
        fasnet::SongChunk chunk;
        if (fasnet::Decode(msg.Payload, chunk)) {
            Online.Incoming.Append(chunk);      // 古い曲の残りなど、合わないチャンクは捨てる
            fasnet::crypto::SecureZero(chunk.Data);
        }
        break;
    }
    case Msg::SongEnd: {
        fasnet::U32Value serial;
        if (fasnet::Decode(msg.Payload, serial) && Online.Incoming.IsActive() && serial.Value == Online.Incoming.Info().Serial) {
            if (auto song = Online.Incoming.Take()) {
                Online.Song = std::move(*song);
            }
            else {
                Online.Incoming.Clear();
            }
        }
        break;
    }
    case Msg::Start: {
        fasnet::StartInfo info;
        if (fasnet::Decode(msg.Payload, info)) {
            OnOnlineStart(info.DelayMs);
        }
        break;
    }
    case Msg::Hit: {
        fasnet::HitEvent hit;
        if (fasnet::Decode(msg.Payload, hit)) {
            ApplyRemoteHit(hit);
        }
        break;
    }
    case Msg::Pong: {
        fasnet::U64Value sent;
        if (fasnet::Decode(msg.Payload, sent)) {
            const double rtt = (double)(SteadyMs() - sent.Value);
            Online.RttMs = Online.RttMs == 0.0 ? rtt : Online.RttMs * 0.8 + rtt * 0.2;
        }
        break;
    }
    default:
        break;
    }
}

void Game::OnRoomChanged(fasnet::RoomPhase prev) {
    const auto& room = Online.Room;
    if (room.Phase != fasnet::RoomPhase::SongSelected || Online.PreviewSerial != room.SongSerial) {
        OnlineStopPreview();
    }

    // ホスト: 送った曲が受理されたら、その通し番号で準備完了を通知する
    if (IsHost() && Online.SongRequested && room.Phase == fasnet::RoomPhase::SongSelected) {
        Online.SongRequested = false;
        Online.LoadedSerial = room.SongSerial;
        Online.Net.Send(fasnet::Msg::Ready, fasnet::U32Value{ room.SongSerial });
    }

    // 全員の演奏が終わったらリザルトへ
    if (room.Phase == fasnet::RoomPhase::Result && NowScene == Scene::Playing && Online.FinishSent) {
        CopyRoomScores();
        NowScene = Scene::Result;
    }
    if (NowScene == Scene::Result) {
        CopyRoomScores();
    }

    // ホストが待機室に戻した (演奏の中断を含む)
    if (room.Phase == fasnet::RoomPhase::Lobby && prev != fasnet::RoomPhase::Lobby) {
        OnlineWipeSong();
        Online.LoadedSerial = 0;
        Online.StartPending = false;
        if (NowScene == Scene::Result || NowScene == Scene::Playing || NowScene == Scene::Loading) {
            Chart.Init(true);
            NowScene = Scene::MultiRoom;
        }
    }

    Online.Cursor = std::clamp(Online.Cursor, 0, std::max((int)room.Players.size() - 1, 0));
}

void Game::OnOnlineStart(uint32_t delayms) {
    if (!IsMulti || Online.LoadedSerial == 0 || Online.LoadedSerial != Online.Room.SongSerial) {
        return;     // 読み込んでいない曲では開始しない
    }
    OnlineStopPreview();
    // サーバーからの到着が片道分遅れているので、往復遅延の半分だけ早めて全員の開始を揃える
    const double wait = std::max(0.0, (double)delayms - Online.RttMs * 0.5);
    Online.StartAt = std::chrono::steady_clock::now() + std::chrono::milliseconds((int64_t)wait);
    Online.StartPending = true;
    Online.FinishSent = false;
    PrevScene = Scene::MultiRoom;
    NowScene = Scene::Playing;
}

void Game::CopyRoomScores() {
    for (size_t lane = 1; lane < Online.LaneIds.size() && lane < Chart.Judge.size(); ++lane) {
        if (const auto* player = Online.Room.Find(Online.LaneIds[lane])) {
            FromScore(Chart.Judge[lane], player->Score);
        }
    }
}
