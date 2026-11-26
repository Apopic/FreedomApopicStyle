#include "../Game.hpp"

void Game::TitleInit() {
    Skin.Base->Title.SE.Don.SetVolume(Config.SEVolume);
    Skin.Base->Title.SE.Ka.SetVolume(Config.SEVolume);
}

void Game::TitleDraw() {
    Skin.Base->Title.Image.BackGround.Draw({});
    Skin.Base->Title.Image.Logo.Draw({});
}

void Game::TitleProc() {

    auto DonKeyProc = [&] {
        Skin.Base->Title.SE.Don.Play();
        NowScene = Scene::ModeSelect;
        };

#ifndef __ANDROID__
    Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] {
        EndFlag = true;
        });
    Input::GetInstance().HitKeyesProcess(Config.DonInputLeft, KeyState::Down, DonKeyProc);
    Input::GetInstance().HitKeyesProcess(Config.DonInputRight, KeyState::Down, DonKeyProc);
    Input::GetInstance().HitKeyProcess(VK_RETURN, KeyState::Down, DonKeyProc);
#else
    Touch.Process(TouchType::Don, DonKeyProc);
#endif
}
