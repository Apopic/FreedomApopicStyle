#include "../Game.hpp"

void Game::ModeSelectInit() {
    Chart.Init(true);
    DemoSongPlayBlank.Reset();
    ResetDemoSong();
    BoxDataIndex = 0;
    Skin.Base->ModeSelect.SE.Don.SetVolume(Config.SEVolume);
    Skin.Base->ModeSelect.SE.Ka.SetVolume(Config.SEVolume);
#ifndef __ANDROID__
#endif
}

float Game::ModeBoxDistance() const {
    const auto& box = Skin.Base->ModeSelect.Image.Box;
    float d = Skin.Base->ModeSelect.Config.BoxDistance;
    if (box.Size.Height > 0) { d = std::min(d, box.Size.Height * 1.18f); }
    const float room = (float)Skin.Info.Resolution.Y - box.Size.Height - 32.0f;
    return std::max(1.0f, std::min(d, room / std::max((int)Mode::Count - 1, 1)));
}

float Game::ModeBoxOffset() const {
    const auto& box = Skin.Base->ModeSelect.Image.Box;
    const float resy = (float)Skin.Info.Resolution.Y;
    const float block = ModeBoxDistance() * ((int)Mode::Count - 1);
    const float top = std::max(box.Size.Height * 0.5f + 16.0f, (resy - block) * 0.5f);
    return top - box.Pos.Y;
}

bool Game::WheelSoundReady() {
    const auto now = std::chrono::steady_clock::now();
    if (now - WheelSoundAt < std::chrono::milliseconds(8)) { return false; }
    WheelSoundAt = now;
    return true;
}

void Game::ModeSelectDraw() {
    Skin.Base->ModeSelect.Image.BackGround.Draw({ 0, 0 });
    const float distance = ModeBoxDistance();
    const float offset = ModeBoxOffset();
    for (size_t i = 0; i < (size_t)Mode::Count; ++i) {
        unsigned int c = 100 * ((size_t)ModeSelector == i);
        gfx::SetAddColor(c, c, c);
        Skin.Base->ModeSelect.Image.Box.Draw({ 0, offset + distance * i });
        gfx::SetAddColor(0, 0, 0);
        Skin.Base->ModeSelect.Font.Modes.Draw({
        Skin.Base->ModeSelect.Config.ModesPos.X,
        Skin.Base->ModeSelect.Config.ModesPos.Y + offset + (distance * i) }, gfx::Rgb(255, 255, 255), gfx::Rgb(0, 0, 0), magic_enum::enum_name((Mode)i).data());
    }
}

void Game::ModeSelectProc() {

    auto BackProc = [&] {
        NowScene = Scene::Title;
        };

    auto DonKeyProc = [&] {
        Skin.Base->ModeSelect.SE.Don.Play();
        switch (ModeSelector) {
        case Mode::Single:
            NowScene = Scene::SongSelect;
            break;
        case Mode::Multi:
            NowScene = Scene::MultiRoom;
            break;
        case Mode::Dan:
            NowScene = Scene::DanSelect;
            break;
        case Mode::Edit:
            EditorReturnScene=Scene::ModeSelect; NowScene = Scene::Editor;
            break;
        case Mode::Config:
            PrevScene = Scene::ModeSelect;
            NowScene = Scene::ConfigMenu;
            break;
        case Mode::End:
            NowScene = Scene::Title;
            break;
        }
        };

    auto KaKeyProc = [&](bool direction) {
        Skin.Base->ModeSelect.SE.Ka.Play();
        constexpr int count = (int)Mode::Count;
        ModeSelector = (Mode)(((int)ModeSelector + (direction ? 1 : -1) + count) % count);
        };

#ifndef __ANDROID__
    if (const int wheel = Input::GetInstance().GetMouseWheel(); wheel != 0) {
        KaKeyProc(wheel < 0);
    }

    Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, BackProc);
    Input::GetInstance().HitKeyesProcess(Config.DonInputLeft, KeyState::Down, DonKeyProc);
    Input::GetInstance().HitKeyesProcess(Config.DonInputRight, KeyState::Down, DonKeyProc);
    Input::GetInstance().HitKeyProcess(VK_RETURN, KeyState::Down, DonKeyProc);

    Input::GetInstance().HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { KaKeyProc(false); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { KaKeyProc(true); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyesProcess({ VK_UP,VK_LEFT }, KeyState::Down, [&] { KaKeyProc(false); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyesProcess({ VK_DOWN,VK_RIGHT }, KeyState::Down, [&] { KaKeyProc(true); }, Config.KeyHoldProcInterval);
#else
    Touch.Process(TouchType::Don, DonKeyProc);
    Touch.Process(TouchType::LeftKa, [&] {KaKeyProc(false);});
    Touch.Process(TouchType::RightKa, [&] {KaKeyProc(true);});
#endif
}
