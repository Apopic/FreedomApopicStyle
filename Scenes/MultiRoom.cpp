#include "../Game.hpp"

void Game::MultiRoomInit() {
    Skin.Base->MultiRoom.SE.Don.SetVolume(Config.SEVolume);
    Skin.Base->MultiRoom.SE.Ka.SetVolume(Config.SEVolume);
    Online.TransferMode = false;
}

void Game::MultiRoomEnd() {
    OnlineStopPreview();
    Online.TransferMode = false;
#ifndef __ANDROID__
    if (Online.AddressEditing) {
        sys::EndTextInput(Online.AddressHandle);
        Online.AddressHandle = 0;
        Online.AddressEditing = false;
    }
#endif
}

void Game::FitDraw(FontData& font, Pos2D<float> pos, unsigned int color, unsigned int edge, const std::string& text, float maxwidth) {
    font.Draw(pos, color, edge, FitText(text, font.Handle, std::max(maxwidth, 32.0f), false));
}

float Game::PlayerNameLeft() const {
    const auto& box = Skin.Base->MultiRoom.Image.PlayerBox;
    const auto& crown = Skin.Base->MultiRoom.Image.Crown;
    float left = box.Size.Width > 0 ? box.Pos.X - box.Size.Width * 0.5f + 16.0f : 16.0f;
    if (crown.Size.Width > 0) { left = std::max(left, crown.Pos.X + crown.Size.Width * 0.5f + 12.0f); }
    return left;
}

float Game::PlayerNameRight() const {
    const auto& box = Skin.Base->MultiRoom.Image.PlayerBox;
    return box.Size.Width > 0 ? box.Pos.X + box.Size.Width * 0.5f - 16.0f : (float)Skin.Info.Resolution.X - 16.0f;
}

float Game::PlayerLabelX() const {
    const auto& font = Skin.Base->MultiRoom.Font.Player;
    const float x = Skin.Base->MultiRoom.Config.PlayerPos.X;
    // 左揃えのスキンで王冠と重なる位置なら、王冠の右から書く
    return font.Position == FontData::Left ? std::max(x, PlayerNameLeft()) : x;
}

float Game::PlayerLabelWidth() const {
    const auto& font = Skin.Base->MultiRoom.Font.Player;
    const float x = PlayerLabelX(), left = PlayerNameLeft(), right = PlayerNameRight();
    switch (font.Position) {
    case FontData::Center: return 2.0f * std::min(x - left, right - x);
    case FontData::Right: return x - left;
    default: return right - x;
    }
}

void Game::MultiRoomDraw() {

    gfx::SetBlend(gfx::Blend::None);
    Skin.Base->MultiRoom.Image.BackGround.Draw({});

#ifdef __ANDROID__
    Skin.Base->MultiRoom.Image.Back.Draw({});
#endif
    auto& font = Skin.Base->MultiRoom.Font;
    const unsigned int white = gfx::Rgb(255, 255, 255);
    const unsigned int black = gfx::Rgb(0, 0, 0);
    const float bottom = (float)Skin.Info.Resolution.Y - 32.0f;
    const float resx = (float)Skin.Info.Resolution.X;

    if (!IsMulti) {
        if (Online.Connecting || Online.Net.IsStarted()) {
            FitDraw(font.String, { 16, 16 }, white, black, "サーバーに接続しています... (" + Online.Target + ")", resx - (16) - 16.0f);
            FitDraw(font.String, { 16, bottom }, white, black, "Esc: 中止", resx - (16) - 16.0f);
            return;
        }
        if (!Online.Status.empty()) {
            FitDraw(font.String, { 16, 16 }, gfx::Rgb(255, 160, 160), black, Online.Status, resx - (16) - 16.0f);
        }
        const std::string items[3] = {
            "サーバーに接続  (" + HiddenAddress(Config.ServerPort) + "  アドレスは Config の「オンライン」で変更)",
            "接続先 (アドレス:ポート) を入力して接続",
            "この PC でサーバーを起動してホストになる  (ポート " + std::to_string(Config.ServerPort) + ")",
        };
        for (int i = 0; i < JoinMenuCount; ++i) {
            const bool selected = Online.JoinMenu == i;
            FitDraw(font.String, { 48, 96.0f + 56.0f * i }, selected ? gfx::Rgb(255, 220, 0) : white, black, (selected ? "> " : "  ") + items[i], resx - (48) - 16.0f);
        }
#ifndef __ANDROID__
        if (Online.AddressEditing) {
            const int x = 72, y = 96 + 56 * JoinMenuCount + 16;
            gfx::SetBlend(gfx::Blend::Alpha, 200);
            gfx::Box((float)(x - 12), (float)(y - 8), (float)(Skin.Info.Resolution.X - 48), (float)(y + (int)font.String.Size + 16), gfx::Rgb(0, 0, 0), true);
            gfx::SetBlend(gfx::Blend::None);
            gfx::Box((float)(x - 12), (float)(y - 8), (float)(Skin.Info.Resolution.X - 48), (float)(y + (int)font.String.Size + 16), gfx::Rgb(255, 220, 0), false);
            DrawTextInput(x, y, Online.AddressHandle);
            FitDraw(font.String, { 16, bottom }, white, black, "アドレス:ポート を入力して Enter (例: 192.168.0.2:8080 / Esc: キャンセル)", resx - (16) - 16.0f);
            return;
        }
#endif
        FitDraw(font.String, { 16, bottom }, white, black, "↑↓: 選択  /  Enter: 決定  /  Esc: 戻る", resx - (16) - 16.0f);
        return;
    }

    const auto& room = Online.Room;
    // 4人分を基準に、タイトル箱と先頭の箱の隙間をすべての箱に適用する。
    const auto& playerBox = Skin.Base->MultiRoom.Image.PlayerBox;
    const auto& titleBox = Skin.Base->MultiRoom.Image.TitleBox;
    const float configuredDistance = Skin.Base->MultiRoom.Config.BoxDistance;
    const bool hasPlayerBox = HasImage(playerBox);
    const bool hasTitleBox = HasImage(titleBox);
    const float boxHeight = hasPlayerBox ? playerBox.Size.Height : (float)font.Player.Size;
    const float minGap = std::max(1.0f, std::round(Skin.Info.Resolution.Y / 360.0f));
    const float fallbackGap = std::max(minGap, std::round(Skin.Info.Resolution.Y / 36.0f));
    const float topMargin = std::max(minGap, std::round(Skin.Info.Resolution.Y / 180.0f));
    // 接続情報・操作説明はスキンのフォントを70%に縮小し、下端に詰める。
    constexpr float footerScale = 0.7f;
    const float footerHeight = std::max(1.0f, font.String.Size * footerScale);
    const float footerPitch = std::max(12.0f, std::ceil(footerHeight * 1.2f));
    const float footerBottom = (float)Skin.Info.Resolution.Y - std::max(4.0f, minGap) - footerHeight * 0.5f;
    const bool showNotice = std::chrono::steady_clock::now() < Online.NoticeUntil;
    int footerRows = 3;
    if (showNotice) { footerRows = 4; }
#ifndef __ANDROID__
    if (Discord.Request) { footerRows = 5; }
#endif
    const float footerTop = footerBottom - (footerRows - 1) * footerPitch - footerHeight * 0.5f - minGap;
    const auto drawFooter = [&](int row, unsigned int color, const std::string& text) {
        if (font.String.Handle == -1 || text.empty()) { return; }
        const float scaleX = std::max(0.01f, font.String.WidthScale * footerScale);
        const std::string fitted = FitText(text, font.String.Handle, std::max(32.0f, (resx - 32.0f) / scaleX), false);
        txt::Draw(font.String.Handle, 16.0f, footerBottom - row * footerPitch - footerHeight * 0.5f, fitted.c_str(), color, black, (float)(scaleX), (float)(footerScale));
    };
    const float count = (float)fasnet::MaxPlayers;
    float gap = fallbackGap;
    float groupOffset = 0.0f;
    float playerOffset = 0.0f;
    float drawHeight = boxHeight;
    if (hasTitleBox && hasPlayerBox) {
        const float titleTop = titleBox.Pos.Y - titleBox.Size.Height * 0.5f;
        const float titleBottom = titleBox.Pos.Y + titleBox.Size.Height * 0.5f;
        const float originalPlayerTop = playerBox.Pos.Y - boxHeight * 0.5f;
        const float titleGap = originalPlayerTop - titleBottom;
        if (titleGap > 0.0f) { gap = std::max(minGap, std::round(titleGap)); }
        const float firstTop = titleBottom + gap;
        // 間隔を詰める前にタイトル・プレイヤー群を上へ移動する。
        const float overflow = firstTop + count * boxHeight + (count - 1.0f) * gap - footerTop;
        // 余裕があれば元の位置より少し下へ。足りない場合は上端の余白まで上げる。
        const float desiredOffset = std::round(Skin.Info.Resolution.Y / 90.0f);
        groupOffset = std::max(topMargin - titleTop, std::min(desiredOffset, -overflow));
        const float available = footerTop - (titleBottom + groupOffset);
        const auto& crown = Skin.Base->MultiRoom.Image.Crown;
        const float nameHalfHeight = std::abs(Skin.Base->MultiRoom.Config.PlayerPos.Y - playerBox.Pos.Y) + font.Player.Size * 0.5f;
        const float crownHalfHeight = HasImage(crown) ? std::abs(crown.Pos.Y - playerBox.Pos.Y) + crown.Size.Height * 0.5f : 0.0f;
        const float contentHeight = 2.0f * std::max(nameHalfHeight, crownHalfHeight);
        const float minHeight = std::min(boxHeight, std::max(1.0f, contentHeight + 2.0f * minGap));
        // 上に移動しても足りない場合だけ、文字・王冠を収めたまま箱を縦に縮める。
        drawHeight = std::clamp(std::floor((available - count * gap) / count), minHeight, boxHeight);
        if (count * (drawHeight + gap) > available) {
            gap = std::max(minGap, std::min(gap, std::floor(available / count - drawHeight)));
        }
        playerOffset = titleBottom + groupOffset + gap + drawHeight * 0.5f - playerBox.Pos.Y;
    }
    else {
        if (configuredDistance > boxHeight) {
            gap = std::max(minGap, std::min(configuredDistance - boxHeight, fallbackGap));
        }
        if (hasPlayerBox) {
            const float maxGap = std::floor((footerTop - playerBox.Pos.Y - boxHeight * 0.5f) /
                (count - 1.0f) - boxHeight);
            gap = std::max(minGap, std::min(gap, maxGap));
        }
    }
    const float distance = drawHeight + gap;

    Skin.Base->MultiRoom.Image.TitleBox.Draw({ 0, groupOffset });
    const auto titlePos = [groupOffset](Pos2D<float> pos) { pos.Y += groupOffset; return pos; };
    if (room.Phase != fasnet::RoomPhase::Lobby) {
        FitDraw(font.Title, titlePos(Skin.Base->MultiRoom.Config.TitlePos), white, black, room.Title, resx - 32.0f);
        FitDraw(font.SubTitle, titlePos(Skin.Base->MultiRoom.Config.SubTitlePos), white, black, room.Subtitle, resx - 32.0f);
        if (room.CourseIndex >= 0 && room.CourseIndex < (int)CourseType::Count) {
            font.Course.Draw(titlePos(Skin.Base->MultiRoom.Config.CoursePos), white, black, magic_enum::enum_name((CourseType)room.CourseIndex).data());
        }
        font.Level.Draw(titlePos(Skin.Base->MultiRoom.Config.LevelPos), white, black, "★×" + std::to_string(room.Level));
    }

    for (size_t i = 0; i < room.Players.size(); i++) {
        const auto& player = room.Players[i];
        const float y = playerOffset + distance * static_cast<float>(i);

        const bool selected = Online.TransferMode && Online.Cursor == (int)i;
        const unsigned int add = selected ? 120 : (player.Id == Online.MyId ? 40 : 0);
        gfx::SetAddColor(add, add, add);
        if (hasPlayerBox) {
            const float centerY = playerBox.Pos.Y + y;
            gfx::DrawTexture(playerBox.Handles[0], playerBox.Pos.X - playerBox.Size.Width * 0.5f, centerY - drawHeight * 0.5f, playerBox.Pos.X + playerBox.Size.Width * 0.5f, centerY + drawHeight * 0.5f);
        }
        gfx::SetAddColor(0, 0, 0);

        unsigned int color = white;
        const char* state = "";
        switch (player.State) {
        case fasnet::PlayerState::Lobby: state = "待機"; break;
        case fasnet::PlayerState::Loading: state = "読込中"; color = gfx::Rgb(160, 160, 160); break;
        case fasnet::PlayerState::Ready: state = "準備完了"; color = gfx::Rgb(255, 255, 0); break;
        case fasnet::PlayerState::Playing: state = "演奏中"; color = gfx::Rgb(0, 255, 255); break;
        case fasnet::PlayerState::Finished: state = "終了"; color = gfx::Rgb(0, 255, 255); break;
        }
        const std::string label = player.Name + "  [" + state + "]";     // 称号は待機室の箱には出さない (演奏画面・リザルトで表示)
        // 名前は箱の中で、ホストの王冠と重ならない幅に収める (長い名前・称号は省略する)
        const std::string fitted = FitText(label, font.Player.Handle, PlayerLabelWidth(), false);
        font.Player.Draw(
            { PlayerLabelX(), Skin.Base->MultiRoom.Config.PlayerPos.Y + y },
            color, black,
            GetStrlenCached(fitted, font.Player.Handle),
            fitted
        );
        if (player.IsHost) {
            Skin.Base->MultiRoom.Image.Crown.Draw({ 0, y }, 3);
        }
    }

    // 操作説明と通信状況
    std::string help;
    const bool allready = !room.Players.empty() && std::ranges::all_of(room.Players, [](const fasnet::PlayerInfo& p) { return p.State == fasnet::PlayerState::Ready; });
    if (Online.TransferMode) {
        help = "↑↓: 移譲先を選択  /  Enter: 決定  /  Esc: キャンセル";
    }
    else if (IsHost()) {
        help = allready && room.Phase == fasnet::RoomPhase::SongSelected ? "Enter: 演奏開始" : "Enter: 曲を選ぶ";
        help += "  /  Tab: ホスト移譲  /  F1: 設定  /  Esc: 退出";
    }
    else {
        help = "ホストの操作を待っています  /  F1: 設定  /  Esc: 退出";
    }
    drawFooter(0, white, help);

    std::string status = "Ping " + std::to_string((int)Online.RttMs) + "ms";
    status += "  " + Online.Target;
    if (const size_t pending = Online.Net.PendingBulkBytes(); pending > 0) {
        status += "  曲データを送信中 (残り " + FormatNumber(pending / (1024.0 * 1024.0), 1) + "MB)";
    }
    else if (!IsHost() && Online.Incoming.IsActive() && Online.Incoming.Info().Serial == room.SongSerial) {
        const double total = std::max<double>((double)Online.Incoming.Total(), 1.0);
        status += "  曲データを受信中 " + std::to_string((int)(100.0 * Online.Incoming.Received() / total)) + "%";
    }
    drawFooter(1, white, status);

    // 招待用の接続先 (C: コピー / V: 切り替え)
    if (!Online.Addresses.empty()) {
        const auto& a = Online.Addresses[std::clamp(Online.AddressIndex, 0, (int)Online.Addresses.size() - 1)];
        drawFooter(2, gfx::Rgb(255, 230, 120), "接続先 (" + a.Label + "): " + HiddenAddress(a.Port) + (Online.Addresses.size() > 1 ? "   C: コピー  V: 切り替え" : "   C: コピー"));
    }
    if (showNotice) {
        drawFooter(3, gfx::Rgb(160, 255, 160), Online.Notice);
    }
#ifndef __ANDROID__
    if (Discord.Request) {
        drawFooter(4, gfx::Rgb(160, 200, 255), "Discord: " + Discord.Request->UserName + " が参加を希望しています  Y: 許可  N: 断る");
    }
#endif

#ifdef __ANDROID__
    Skin.Base->MultiRoom.Image.Config.Draw({});
    Skin.Base->MultiRoom.Image.Host.Draw({});
#endif
}

void Game::MultiRoomProc() {

    auto BackInputProc = [&] {
        if (Online.TransferMode) {
            Online.TransferMode = false;
            return;
        }
        OnlineDisconnect();
        NowScene = Scene::ModeSelect;
        };

    if (!IsMulti) {
#ifndef __ANDROID__
        // 接続先の入力中
        if (Online.AddressEditing) {
            const int state = sys::TextInputState(Online.AddressHandle);
            if (state != 0) {
                if (state == 1) {
                    const std::string buf = sys::TextInputString(Online.AddressHandle);
                    if (OnlineSetTarget(buf)) { OnlineConnectByAddress(); }
                }
                sys::EndTextInput(Online.AddressHandle);
                Online.AddressHandle = 0;
                Online.AddressEditing = false;
                // 確定に使った Enter が次のフレームで「参加」として誤動作しないようにする
                Input::GetInstance().ResyncKeys();
            }
            return;
        }
#endif
        const bool busy = Online.Connecting || Online.Net.IsStarted();
        auto JoinProc = [&] {
            if (busy) { return; }
            Skin.Base->MultiRoom.SE.Don.Play();
            if (Online.JoinMenu == 0) {
                OnlineConnectByAddress();
                return;
            }
#ifndef __ANDROID__
            if (Online.JoinMenu == 2) {
                OnlineHostLocal();
                return;
            }
            Online.AddressHandle = sys::BeginTextInput("", (size_t)(255), false);
            ((void)0);
            sys::SetTextInputFont(Skin.Base->MultiRoom.Font.String.Handle);
            // 入力欄には前の接続先を入れない (画面に IP アドレスを出さないため)
            Online.AddressEditing = true;
#else
            if (OnlineSetTarget(GetInputText("アドレス:ポート"))) { OnlineConnectByAddress(); }
#endif
            };
        auto MoveProc = [&](int dir) {
            if (busy) { return; }
            Skin.Base->MultiRoom.SE.Ka.Play();
            Online.JoinMenu = std::clamp(Online.JoinMenu + dir, 0, JoinMenuCount - 1);
            };
#ifndef __ANDROID__
        Input::GetInstance().HitKeyProcess(VK_UP, KeyState::Down, [&] { MoveProc(-1); });
        Input::GetInstance().HitKeyProcess(VK_DOWN, KeyState::Down, [&] { MoveProc(1); });
        Input::GetInstance().HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { MoveProc(-1); });
        Input::GetInstance().HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { MoveProc(1); });
        Input::GetInstance().HitKeyProcess(VK_RETURN, KeyState::Down, JoinProc);
        Input::GetInstance().HitKeyesProcess(Config.DonInputLeft, KeyState::Down, JoinProc);
        Input::GetInstance().HitKeyesProcess(Config.DonInputRight, KeyState::Down, JoinProc);
        Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
#else
        Touch.Process(TouchType::LeftKa, [&] { MoveProc(-1); });
        Touch.Process(TouchType::RightKa, [&] { MoveProc(1); });
        Touch.Process(TouchType::Don, JoinProc);
        Touch.Process(TouchType::Other, BackInputProc, Skin.Base->MultiRoom.Image.Back);
#endif
        return;
    }

    const auto& room = Online.Room;

    // 配布された曲をまだ読み込んでいなければ読み込む
    if (!IsHost() && room.Phase == fasnet::RoomPhase::SongSelected && Online.Song
        && Online.Song->Serial == room.SongSerial && Online.LoadedSerial != room.SongSerial && Online.FailedSerial != room.SongSerial) {
        OnlineStopPreview();
        PrevScene = Scene::MultiRoom;
        NowScene = Scene::Loading;
        return;
    }

    OnlineUpdatePreview();

    auto DonInputProc = [&] {
        if (Online.TransferMode) {
            if (Online.Cursor >= 0 && Online.Cursor < (int)room.Players.size() && room.Players[Online.Cursor].Id != Online.MyId) {
                Skin.Base->MultiRoom.SE.Don.Play();
                Online.Net.Send(fasnet::Msg::TransferHost, fasnet::U32Value{ room.Players[Online.Cursor].Id });
            }
            Online.TransferMode = false;
            return;
        }
        if (!IsHost() || room.Phase == fasnet::RoomPhase::Playing) {
            return;
        }
        Skin.Base->MultiRoom.SE.Don.Play();
        const bool allready = std::ranges::all_of(room.Players, [](const fasnet::PlayerInfo& p) { return p.State == fasnet::PlayerState::Ready; });
        if (room.Phase == fasnet::RoomPhase::SongSelected && allready) {
            Online.Net.Send(fasnet::Msg::StartRequest);
        }
        else {
            OnlineStopPreview();
            PrevScene = Scene::MultiRoom;
            NowScene = Scene::SongSelect;
        }
        };

    auto KaInputProc = [&](bool direction) {
        if (!Online.TransferMode) {
            return;
        }
        Skin.Base->MultiRoom.SE.Ka.Play();
        Online.Cursor = std::clamp(Online.Cursor + (direction ? 1 : -1), 0, std::max((int)room.Players.size() - 1, 0));
        };

    auto HostInputProc = [&] {
        if (IsHost() && room.Players.size() >= 2 && room.Phase != fasnet::RoomPhase::Playing) {
            Skin.Base->MultiRoom.SE.Don.Play();
            Online.TransferMode = true;
            Online.Cursor = 0;
        }
        };

    auto ConfigInputProc = [&] {
        if (room.Phase != fasnet::RoomPhase::Playing) {
            Skin.Base->MultiRoom.SE.Don.Play();
            OnlineStopPreview();
            PrevScene = Scene::MultiRoom;
            NowScene = Scene::ConfigMenu;
        }
        };

#ifndef __ANDROID__
    Input::GetInstance().HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { KaInputProc(false); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { KaInputProc(true); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_UP, KeyState::Down, [&] { KaInputProc(false); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_DOWN, KeyState::Down, [&] { KaInputProc(true); }, Config.KeyHoldProcInterval);

    Input::GetInstance().HitKeyesProcess(Config.DonInputLeft, KeyState::Down, DonInputProc);
    Input::GetInstance().HitKeyesProcess(Config.DonInputRight, KeyState::Down, DonInputProc);
    Input::GetInstance().HitKeyProcess(VK_RETURN, KeyState::Down, DonInputProc);

    Input::GetInstance().HitKeyProcess(VK_TAB, KeyState::Down, HostInputProc);
    Input::GetInstance().HitKeyProcess(VK_F1, KeyState::Down, ConfigInputProc);
    Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
    Input::GetInstance().HitKeyProcess('C', KeyState::Down, [&] {
        if (Online.Addresses.empty()) { return; }
        const auto& a = Online.Addresses[std::clamp(Online.AddressIndex, 0, (int)Online.Addresses.size() - 1)];
        if (sys::SetClipboard(a.Text()), true) {
            Skin.Base->MultiRoom.SE.Don.Play();
            Online.Notice = "接続先をコピーしました (" + a.Label + ")";
            Online.NoticeUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        }
        });
    auto IsDrumKey = [&](int key) {
        for (const auto* keys : { &Config.DonInputLeft, &Config.DonInputRight, &Config.KaInputLeft, &Config.KaInputRight }) {
            if (std::ranges::find(*keys, key) != keys->end()) { return true; }
        }
        return false;
    };
    if (Discord.Request && !IsDrumKey('Y') && !IsDrumKey('N')) {
        Input::GetInstance().HitKeyProcess('Y', KeyState::Down, [&] {
            DiscordRpc::GetInstance().RespondJoinRequest(Discord.Request->UserId, true);
            Discord.Request.reset();
            Skin.Base->MultiRoom.SE.Don.Play();
            });
        Input::GetInstance().HitKeyProcess('N', KeyState::Down, [&] {
            if (!Discord.Request) { return; }
            DiscordRpc::GetInstance().RespondJoinRequest(Discord.Request->UserId, false);
            Discord.Request.reset();
            Skin.Base->MultiRoom.SE.Ka.Play();
            });
    }
    Input::GetInstance().HitKeyProcess('V', KeyState::Down, [&] {
        if (Online.Addresses.size() < 2) { return; }
        Skin.Base->MultiRoom.SE.Ka.Play();
        Online.AddressIndex = (Online.AddressIndex + 1) % (int)Online.Addresses.size();
        });
#else
    Touch.Process(TouchType::LeftKa, [&] { KaInputProc(false); });
    Touch.Process(TouchType::RightKa, [&] { KaInputProc(true); });
    Touch.Process(TouchType::Don, DonInputProc);
    Touch.Process(TouchType::Other, HostInputProc, Skin.Base->MultiRoom.Image.Host);
    Touch.Process(TouchType::Other, ConfigInputProc, Skin.Base->MultiRoom.Image.Config);
    Touch.Process(TouchType::Other, BackInputProc, Skin.Base->MultiRoom.Image.Back);
#endif
}
