#include "../Game.hpp"

void Game::DanSelectInit() {

#ifndef __ANDROID__
    SetDragFileValidFlag(TRUE);
#endif

    Skin.Base->DanSelect.SE.Don.SetVolume((Config.SEVolume));
    Skin.Base->DanSelect.SE.Ka.SetVolume((Config.SEVolume));
    Dan.Reset();
    CourseIndex = 0;
    SelectSceneEnter();

    BoxDatasUpdate();
    if (BoxDatas.empty() && !Filter.Empty()) {
        // 前回の絞り込み条件に一致する譜面がなくなっていたら解除する
        Filter = FilterConditions();
        BoxDatasUpdate();
    }

    if (BoxDatas.empty()) {
#ifndef __ANDROID__
        MessageBox(NULL, TEXT("譜面がありません。"), TEXT("エラー"), MB_ICONERROR);
#else
        ShowToast("譜面がありません。");
#endif
        NowScene = Scene::ModeSelect;
        return;
    }
    BoxDataIndex = std::clamp<int>(BoxDataIndex, 0, (int)BoxDatas.size() - 1);
}

void Game::DanSelectEnd() {
#ifndef __ANDROID__
    SetDragFileValidFlag(FALSE);
#endif
    SelectSceneLeave();
}

void Game::DanSelectDraw() {

    Skin.Base->DanSelect.Image.BackGround.Draw({});

    for (int i = (BoxDataIndex - 6); i < (BoxDataIndex + 6); ++i) {
        if (i < 0 || i >= BoxDatas.size()) {
            continue;
        }

        Pos2D<float> pos = {
                0,
                (i - BoxDataIndex) * Skin.Base->DanSelect.Config.BoxDistance.Y
        };
        pos = {
                pos.X + Skin.Base->DanSelect.Config.SongBoxListPos.X,
                pos.Y + Skin.Base->DanSelect.Config.SongBoxListPos.Y
        };

        unsigned int c = 200 * (BoxDataIndex == i);
        SetDrawAddColor(
            (BoxDatas[i]->GenreColor.R + c),
            (BoxDatas[i]->GenreColor.G + c),
            (BoxDatas[i]->GenreColor.B + c)
        );
        Skin.Base->DanSelect.Image.Box.Draw({ 0, pos.Y });
        SetDrawAddColor(0, 0, 0);

        if (BoxDatas[i]->IsGenre()) {
            if (BoxDatas[i]->GetGenre()->IsOpen) {
                Skin.Base->DanSelect.Font.Title.Draw(
                    pos,
                    GetColor(255, 255, 255),
                    GetColor(0, 0, 0),
                    "Close"
                );
            }
            else {
                Skin.Base->DanSelect.Font.Title.Draw(
                    pos,
                    GetColor(255, 255, 255),
                    GetColor(0, 0, 0),
                    BoxDatas[i]->GetGenre()->Name
                );
            }
        }
        else {
            Skin.Base->DanSelect.Font.Title.Draw(
                pos,
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                BoxDatas[i]->GetChart()->Title
            );
            if (IsFavorite(*BoxDatas[i]->GetChart())) {
                DrawFavoriteMark(Skin.Base->DanSelect.Image.Box, Skin.Base->DanSelect.Font.Title, pos.Y);
            }
            Skin.Base->DanSelect.Image.Crown.Draw(pos, ScoreDataLoad(BoxDatas[i]->GetChart()->ChartPath.string(), (int)CourseType::Dan).Crown);
        }
    }
    SetDrawBlendMode(0, 0);

    if (!FilterUI.Open && !BoxDatas.empty() && !BoxDatas[BoxDataIndex]->IsGenre()) {
        DrawDanInfo(*BoxDatas[BoxDataIndex]->GetChart());
    }
    DrawSearchStatus(Skin.Base->DanSelect.Font.KeyWord);
    if (FilterUI.Open) {
        DrawFilterPanel(Skin.Base->DanSelect.Font.KeyWord);
    }

#ifdef __ANDROID__
    SetDrawBlendMode(0, 0);
    Skin.Base->DanSelect.Image.Back.Draw({});
    Skin.Base->DanSelect.Image.Config.Draw({});
    Skin.Base->DanSelect.Image.Random.Draw({});
    Skin.Base->DanSelect.Image.Search.Draw({});
#endif
}

void Game::DanSelectProc() {

    if (BoxDatas.empty()) {
        NowScene = Scene::ModeSelect;
        return;
    }
    if (FilterUI.Open) {
        FilterPanelProc();
        return;
    }
    const PointerState pointer = ReadPointer();

    auto BackInputProc = [&] {
        NowScene = Scene::ModeSelect;
        };

#ifndef __ANDROID__
    Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
#else
    Touch.Process(TouchType::Other, BackInputProc, Skin.Base->DanSelect.Image.Back);
#endif

    auto DonInputProc = [&] {
        Skin.Base->DanSelect.SE.Don.Play();
        if (BoxDatas[BoxDataIndex]->IsGenre()) {
            bool& _f = BoxDatas[BoxDataIndex]->GetGenre()->IsOpen;
            _f = !_f;
            BoxDatasUpdate();
        }
        else if (BoxDatas[BoxDataIndex]->GetChart()->Courses[(int)CourseType::Dan].IsPlayable) {
            CourseIndex = (int)CourseType::Dan;
            PrevScene = Scene::DanSelect;
            NowScene = Scene::Loading;
        }
        };
    auto KaInputProc = [&](bool direction) {
        Skin.Base->DanSelect.SE.Ka.Play();
        if (!direction) {
            BoxDataIndex = BoxDataIndex == 0 ? numeric::Cast<int>(BoxDatas.size() - 1) : BoxDataIndex - 1;
        }
        else {
            BoxDataIndex = BoxDataIndex == BoxDatas.size() - 1 ? 0 : BoxDataIndex + 1;
        }
        };
    auto RandomInputProc = [&] {
        Skin.Base->DanSelect.SE.Ka.Play();
        BoxDataIndex = GetRand(numeric::Cast<int>(BoxDatas.size() - 1));
        };
    auto SearchInputProc = [&]() {
        Skin.Base->DanSelect.SE.Don.Play();
        OpenFilterPanel();
        };

    auto ConfigInputProc = [&] {
        Skin.Base->DanSelect.SE.Don.Play();
        NowScene = Scene::ConfigMenu;
        PrevScene = Scene::DanSelect;
        };

    // ---- マウス ----
    {
        const int hit = HitTestSelectBox(Skin.Base->DanSelect.Image.Box, Skin.Base->DanSelect.Config.SongBoxListPos.Y,
            Skin.Base->DanSelect.Config.BoxDistance.Y, pointer);
        if (hit >= 0 && pointer.LeftPressed) {
            if (hit == BoxDataIndex) { DonInputProc(); }
            else { Skin.Base->DanSelect.SE.Ka.Play(); BoxDataIndex = hit; }
        }
        if (pointer.Wheel != 0) {
            if (WheelSoundReady()) { Skin.Base->DanSelect.SE.Ka.Play(); }
            const int size = (int)BoxDatas.size();
            BoxDataIndex = ((BoxDataIndex - pointer.Wheel) % size + size) % size;
        }
    }
    if (NowScene != Scene::DanSelect) {
        return;
    }

#ifndef __ANDROID__
    {

        Input::GetInstance().HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { KaInputProc(false); }, Config.KeyHoldProcInterval);
        Input::GetInstance().HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { KaInputProc(true); }, Config.KeyHoldProcInterval);
        Input::GetInstance().HitKeyesProcess({ VK_UP, VK_LEFT }, KeyState::Down, [&] { KaInputProc(false); }, Config.KeyHoldProcInterval);
        Input::GetInstance().HitKeyesProcess({ VK_DOWN, VK_RIGHT }, KeyState::Down, [&] { KaInputProc(true); }, Config.KeyHoldProcInterval);

        Input::GetInstance().HitKeyesProcess(Config.DonInputLeft, KeyState::Down, DonInputProc);
        Input::GetInstance().HitKeyesProcess(Config.DonInputRight, KeyState::Down, DonInputProc);
        Input::GetInstance().HitKeyProcess(VK_RETURN, KeyState::Down, DonInputProc);

        if (!IsCourseSelect) {

            Input::GetInstance().HitKeyProcess(VK_TAB, KeyState::Down, RandomInputProc);
            Input::GetInstance().HitKeyProcess(VK_SPACE, KeyState::Down, SearchInputProc);

            Input::GetInstance().HitKeyProcess(VK_F1, KeyState::Down, ConfigInputProc);
            Input::GetInstance().HitKeyProcess(VK_F2, KeyState::Down, [&] {
                Skin.Base->DanSelect.SE.Don.Play();
                EnumChart(Config.SongDirectories);
                });
            Input::GetInstance().HitKeyProcess(VK_F3, KeyState::Down, [&] {
                Skin.Base->DanSelect.SE.Don.Play();
                Config.Load();
                });
            Input::GetInstance().HitKeyProcess(VK_F4, KeyState::Down, [&] {
                Skin.Base->DanSelect.SE.Don.Play();
                Skin.Load(Config.SkinName, Config);
                });

            Input::GetInstance().HitKeyProcess(VK_F5, KeyState::Down, [&] {
                (IsDanSelect() ? Skin.Base->DanSelect.SE.Don : Skin.Base->SongSelect.SE.Don).Play();
                ToggleFavoriteSelected();
                });
            Input::GetInstance().HitKeyProcess(VK_F6, KeyState::Down, [&] {
                (IsDanSelect() ? Skin.Base->DanSelect.SE.Ka : Skin.Base->SongSelect.SE.Ka).Play();
                ToggleFavoriteOnly();
                DemoSongPlayBlank.Reset();
                ResetDemoSong();
                });
            Input::GetInstance().HitKeyProcess(VK_F7, KeyState::Down, [&] {
                // 選んでいる譜面を譜面制作モードで開く
                if (IsMulti || BoxDatas.empty() || BoxDatas[BoxDataIndex]->IsGenre()) { return; }
                if (Editor.Open && Editor.Dirty) {
                    ShowSelectMessage("譜面制作モードに保存していない譜面があります");
                    return;
                }
                if (EditorOpen(BoxDatas[BoxDataIndex]->GetChart()->ChartPath)) {
                    ResetDemoSong();
                    NowScene = Scene::Editor;
                }
                });

            FileImport();
        }

        return;
    }
#else
    if (!IsCourseSelect) {
        Touch.Process(TouchType::Other, ConfigInputProc, Skin.Base->DanSelect.Image.Config);
        Touch.Process(TouchType::Other, SearchInputProc, Skin.Base->DanSelect.Image.Search);
    }

    Touch.Process(TouchType::Don, DonInputProc);
    Touch.Process(TouchType::LeftKa, [&] {KaInputProc(false);});
    Touch.Process(TouchType::RightKa, [&] {KaInputProc(true);});
#endif
}
