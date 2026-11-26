#include "../Game.hpp"

std::string Game::DiscordAppId() const {
#ifndef __ANDROID__
    return Config.DiscordRPC ? std::string(DiscordRpc::ApplicationId) : std::string();
#else
    return {};      // Android には Discord のローカル接続が無い
#endif
}

std::string Game::DiscordText(std::string text) {
    if (text.size() > 128) {
        size_t n = 125;
        while (n > 0 && ((unsigned char)text[n] & 0xC0) == 0x80) { --n; }
        text = text.substr(0, n) + "...";
    }
    while (text.size() < 2) { text += ' '; }
    return text;
}

const fasnet::AddressEntry* Game::DiscordJoinAddress() const {
    for (const char* prefix : { "外部", "LAN" }) {
        for (const auto& a : Online.Addresses) {
            if (a.Label.starts_with(prefix)) { return &a; }
        }
    }
    return nullptr;
}

void Game::DiscordUpdate() {
#ifndef __ANDROID__
    auto& rpc = DiscordRpc::GetInstance();
    if (DiscordAppId().empty()) { return; }

    rpc.PollEvents(Discord.Events);
    for (const auto& e : Discord.Events) {
        if (e.Kind == DiscordRpc::Event::Type::Join) {
            // 演奏中・読み込み中は参加しない (オンラインのルームにいる時も)
            if (!IsMulti && !e.Secret.empty() && NowScene != Scene::Playing && NowScene != Scene::Loading) {
                NowScene = Scene::MultiRoom;
                // 招待の接続先は設定に保存しない (自分のいつものサーバーを上書きしない)
                if (const auto target = fasnet::SplitHostPort(e.Secret)) {
                    OnlineConnect(target->first, target->second);
                }
                else {
                    Online.Status = "Discord から受け取った接続先が正しくありません";
                }
            }
        }
        else if (IsMulti) {
            Discord.Request = e;
            Discord.RequestUntil = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        }
        else {
            rpc.RespondJoinRequest(e.UserId, false);    // ルームにいないので参加できない
        }
    }
    if (Discord.Request && std::chrono::steady_clock::now() > Discord.RequestUntil) {
        Discord.Request.reset();
    }

    if (NowScene == Scene::Playing && Discord.LastScene != Scene::Playing) {
        Discord.PlayStart = (int64_t)std::time(nullptr);
    }
    Discord.LastScene = NowScene;
    const auto now = std::chrono::steady_clock::now();
    if (now < Discord.NextUpdate) { return; }
    Discord.NextUpdate = now + std::chrono::milliseconds(500);

    std::string details, state;
    const auto& chart = Chart.OriginalData;
    auto Song = [&] { return Dan.Active ? "段位道場: " + Dan.Course.Title : chart.Title; };
    auto Course = [&] {
        if (Dan.Active) { return std::to_string(Dan.SongIndex + 1) + " 曲目"; }
        std::string text = CourseLabel((int)CourseIndex);
        if (CourseIndex < (size_t)CourseType::Count) { text += "  ★" + std::to_string(chart.Courses[CourseIndex].Level); }
        return text;
    };
    switch (NowScene) {
    case Scene::SongSelect: details = IsMulti ? "オンライン: 曲を選んでいます" : "曲を選んでいます"; break;
    case Scene::DanSelect: details = "段位を選んでいます"; break;
    case Scene::Loading:
    case Scene::Playing: details = Song(); state = Course() + (IsMulti ? "  (オンライン)" : ""); break;
    case Scene::Result: details = "リザルト: " + Song(); state = "スコア " + std::to_string(Chart.Judge.empty() ? 0 : Chart.Judge[0].Score); break;
    case Scene::MultiRoom: details = IsMulti ? "オンライン: 待機室" : "オンライン: 参加先を選んでいます"; break;
    case Scene::ConfigMenu: details = "設定中"; break;
    case Scene::Editor: details = "譜面を作っています"; break;
    default: details = "メニュー"; break;
    }

    nlohmann::json activity = { { "details", DiscordText(details) }, { "assets", { { "large_image", "logo" }, { "large_text", "FreedomApopicStyle" } } }, { "instance", false } };
    if (!state.empty()) { activity["state"] = DiscordText(state); }
    if (NowScene == Scene::Playing && Discord.PlayStart > 0) { activity["timestamps"] = { { "start", Discord.PlayStart } }; }
    if (IsMulti) {
        const size_t players = Online.Room.Players.size();
        if (!activity.contains("state")) { activity["state"] = DiscordText("ルーム " + std::to_string(players) + " / " + std::to_string(fasnet::MaxPlayers) + " 人"); }
        // 参加ボタン: 演奏中でなく、空きがあり、他の人が使える接続先がある時だけ
        const auto* address = DiscordJoinAddress();
        if (address != nullptr && Online.Room.Phase != fasnet::RoomPhase::Playing && players < fasnet::MaxPlayers) {
            char id[32];
            std::snprintf(id, sizeof(id), "fas-%016llx", (unsigned long long)std::hash<std::string>{}(address->Text()));
            activity["party"] = { { "id", id }, { "size", { (int)std::max<size_t>(players, 1), (int)fasnet::MaxPlayers } } };
            activity["secrets"] = { { "join", address->Text() } };
        }
    }
    rpc.SetActivity(activity);
#endif
}
