#include "../Game.hpp"

void Game::SongSelectInit() {

#ifndef __ANDROID__
    SetDragFileValidFlag(TRUE);
#endif
    Skin.Base->SongSelect.SE.Don.SetVolume((Config.SEVolume));
    Skin.Base->SongSelect.SE.Ka.SetVolume((Config.SEVolume));
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

void Game::SongSelectEnd() {

#ifndef __ANDROID__
    SetDragFileValidFlag(FALSE);
#endif

    DemoSongPlayBlank.Reset();
    ResetDemoSong();
    IsCourseSelect = false;
    SelectSceneLeave();
}

void Game::SongSelectDraw() {

    Skin.Base->SongSelect.Image.BackGround.Draw({});

    if (!IsCourseSelect) {

        for (int i = (BoxDataIndex - 6); i < (BoxDataIndex + 6); ++i) {
            if (i < 0 || i >= BoxDatas.size()) {
                continue;
            }

            Pos2D<float> pos = {
                    0,
                    (i - BoxDataIndex) * Skin.Base->SongSelect.Config.BoxDistance.Y
            };
            pos = {
                    pos.X + Skin.Base->SongSelect.Config.SongBoxListPos.X,
                    pos.Y + Skin.Base->SongSelect.Config.SongBoxListPos.Y
            };

            unsigned int c = 200 * (BoxDataIndex == i);
            SetDrawAddColor(
                (BoxDatas[i]->GenreColor.R + c),
                (BoxDatas[i]->GenreColor.G + c),
                (BoxDatas[i]->GenreColor.B + c)
            );
            DrawSliced(Skin.Base->SongSelect.Image.Box, { 0, pos.Y }, Skin.Base->SongSelect.Config.BoxSlice);
            SetDrawAddColor(0, 0, 0);

            if (BoxDatas[i]->IsGenre()) {
                if (BoxDatas[i]->GetGenre()->IsOpen) {
                    Skin.Base->SongSelect.Font.Title.Draw(
                        pos,
                        GetColor(255, 255, 255),
                        GetColor(0, 0, 0),
                        "Close"
                    );
                }
                else {
                    Skin.Base->SongSelect.Font.Title.Draw(
                        pos,
                        GetColor(255, 255, 255),
                        GetColor(0, 0, 0),
                        FitText(BoxDatas[i]->GetGenre()->Name, Skin.Base->SongSelect.Font.Title.Handle, Skin.Base->SongSelect.Image.Box.Size.Width - 48.0f, false)
                    );
                }
            }
            else {
                Skin.Base->SongSelect.Font.Title.Draw(
                    pos,
                    GetColor(255, 255, 255),
                    GetColor(0, 0, 0),
                    // 長い曲名は箱の幅に収める (お気に入りの印の分も空ける)
                    FitText(BoxDatas[i]->GetChart()->Title, Skin.Base->SongSelect.Font.Title.Handle, Skin.Base->SongSelect.Image.Box.Size.Width - 96.0f, false)
                );
                if (IsFavorite(*BoxDatas[i]->GetChart())) {
                    DrawFavoriteMark(Skin.Base->SongSelect.Image.Box, Skin.Base->SongSelect.Font.Title, pos.Y);
                }
            }
        }
        SetDrawBlendMode(0, 0);

#ifdef __ANDROID__
        Skin.Base->SongSelect.Image.Config.Draw({});
        Skin.Base->SongSelect.Image.Random.Draw({});
        Skin.Base->SongSelect.Image.Search.Draw({});
#endif

    }
    else {
        Skin.Base->SongSelect.Image.TitleBox.Draw({});
        Skin.Base->SongSelect.Font.BoxTitle.Draw(
            Skin.Base->SongSelect.Config.BoxTitlePos,
            GetColor(255, 255, 255),
            GetColor(0, 0, 0),
            FitText(BoxDatas[BoxDataIndex]->GetChart()->Title, Skin.Base->SongSelect.Font.BoxTitle.Handle, Skin.Base->SongSelect.Image.TitleBox.Size.Width - 48.0f, false)
        );
        Skin.Base->SongSelect.Font.BoxSubTitle.Draw(
            Skin.Base->SongSelect.Config.BoxSubTitlePos,
            GetColor(255, 255, 255),
            GetColor(0, 0, 0),
            FitText(BoxDatas[BoxDataIndex]->GetChart()->Subtitle, Skin.Base->SongSelect.Font.BoxSubTitle.Handle, Skin.Base->SongSelect.Image.TitleBox.Size.Width - 48.0f, false)
        );

        for (size_t i = 0; i <= (size_t)CourseType::Edit; ++i) {

            unsigned int c = 100 * (CourseIndex == i);
            float y = Skin.Base->SongSelect.Config.CourseBoxDistance * i;
            SetDrawAddColor(c, c, c);
            Skin.Base->SongSelect.Image.CourseBox.Draw({ 0, y });
            SetDrawAddColor(0, 0, 0);

            if (!BoxDatas[BoxDataIndex]->GetChart()->Courses[i].IsPlayable) {
                continue;
            }

            Skin.Base->SongSelect.Font.Course.Draw({ Skin.Base->SongSelect.Config.CoursePos.X,Skin.Base->SongSelect.Config.CoursePos.Y + y }, GetColor(255, 255, 255), GetColor(0, 0, 0), magic_enum::enum_name((CourseType)i).data());
            Skin.Base->SongSelect.Font.Level.Draw({ Skin.Base->SongSelect.Config.LevelPos.X,Skin.Base->SongSelect.Config.LevelPos.Y + y }, GetColor(255, 255, 255), GetColor(0, 0, 0), "★×" + std::to_string(BoxDatas[BoxDataIndex]->GetChart()->Courses[i].Level));

            ScoreData Score = ScoreDataLoad(BoxDatas[BoxDataIndex]->GetChart()->ChartPath.string(), numeric::Cast<int>(i));

            Skin.Base->SongSelect.Font.HighScore.Draw({ Skin.Base->SongSelect.Config.HighScorePos.X,Skin.Base->SongSelect.Config.HighScorePos.Y + y }, GetColor(255, 255, 255), GetColor(0, 0, 0), std::to_string(Score.Score));
            Skin.Base->SongSelect.Image.Crown.Draw({ 0, y }, Score.Crown);
        }
    }
    if (!IsCourseSelect) {
        DrawSearchStatus(Skin.Base->SongSelect.Font.KeyWord);
        if (FilterUI.Open) {
            DrawFilterPanel(Skin.Base->SongSelect.Font.KeyWord);
        }
    }
#ifdef __ANDROID__
    SetDrawBlendMode(0, 0);
    Skin.Base->SongSelect.Image.Back.Draw({});
#endif
}

void Game::SongSelectProc() {

    if (BoxDatas.empty()) {
        NowScene = !IsMulti ? Scene::ModeSelect : Scene::MultiRoom;
        return;
    }
    if (FilterUI.Open) {
        FilterPanelProc();
        return;
    }
    const PointerState pointer = ReadPointer();

    auto BackInputProc = [&] {
        if (IsCourseSelect) {
            IsCourseSelect = false;
        }
        else {
            NowScene = !IsMulti ? Scene::ModeSelect : Scene::MultiRoom;
        }
        };

#ifndef __ANDROID__
    Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
#else
    Touch.Process(TouchType::Other, BackInputProc, Skin.Base->SongSelect.Image.Back);
#endif

    auto DonInputProc = [&] {
        Skin.Base->SongSelect.SE.Don.Play();
        if (BoxDatas[BoxDataIndex]->IsGenre()) {
            bool& _f = BoxDatas[BoxDataIndex]->GetGenre()->IsOpen;
            _f = !_f;
            BoxDatasUpdate();
        }
        else {
            if (!IsCourseSelect) {
                IsCourseSelect = true;
            }
            else if (BoxDatas[BoxDataIndex]->GetChart()->Courses[CourseIndex].IsPlayable) {
                SongDownload(BoxDatas[BoxDataIndex]->GetChart()->SongURL, BoxDatas[BoxDataIndex]->GetChart()->SongPath);
                MovieDownload(BoxDatas[BoxDataIndex]->GetChart()->MovieURL, BoxDatas[BoxDataIndex]->GetChart()->MoviePath);
                ResetDemoSong();
                PrevScene = Scene::SongSelect;
                NowScene = Scene::Loading;
            }
        }
        };
    auto KaInputProc = [&](bool direction) {
        Skin.Base->SongSelect.SE.Ka.Play();
        if (!direction) {
            if (!IsCourseSelect) {
                BoxDataIndex = BoxDataIndex == 0 ? numeric::Cast<int>(BoxDatas.size() - 1) : BoxDataIndex - 1;
                DemoSongPlayBlank.Reset();
                ResetDemoSong();
            }
            else {
                CourseIndex -= CourseIndex > 0;
            }
        }
        else {
            if (!IsCourseSelect) {
                BoxDataIndex = BoxDataIndex == BoxDatas.size() - 1 ? 0 : BoxDataIndex + 1;
                DemoSongPlayBlank.Reset();
                ResetDemoSong();
            }
            else {
                CourseIndex += CourseIndex < (size_t)CourseType::Edit;
            }
        }
        };
    if (!BoxDatas[BoxDataIndex]->IsGenre()) {
        if (!DemoSongPlayBlank.IsRunning()) {
            ResetDemoSong();
            DemoSongPlayBlank.Start();
        }
        else if (DemoSongPlayBlank.GetElapsed().Second() > DemoSongPlayBlankTime()) { UpdateDemoSong(*BoxDatas[BoxDataIndex]->GetChart()); }
    }
    auto RandomInputProc = [&] {
        Skin.Base->SongSelect.SE.Ka.Play();
        BoxDataIndex = GetRand(numeric::Cast<int>(BoxDatas.size() - 1));
        DemoSongPlayBlank.Stop();
        ResetDemoSong();
        };
    auto SearchInputProc = [&]() {
        Skin.Base->SongSelect.SE.Don.Play();
        OpenFilterPanel();
        };

    auto ConfigInputProc = [&] {
        Skin.Base->SongSelect.SE.Don.Play();
        NowScene = Scene::ConfigMenu;
        PrevScene = Scene::SongSelect;
        };

    // ---- マウス: クリックで選択、選択中をクリックで決定、右クリックで戻る、中クリックでお気に入り ----
    auto SelectBox = [&](int index, bool wheel = false) {
        if (!wheel || WheelSoundReady()) { Skin.Base->SongSelect.SE.Ka.Play(); }
        BoxDataIndex = index;
        DemoSongPlayBlank.Reset();
        ResetDemoSong();
        };
    if (!IsCourseSelect) {
        const int hit = HitTestSelectBox(Skin.Base->SongSelect.Image.Box, Skin.Base->SongSelect.Config.SongBoxListPos.Y,
            Skin.Base->SongSelect.Config.BoxDistance.Y, pointer);
        if (hit >= 0 && pointer.LeftPressed) {
            if (hit == BoxDataIndex) { DonInputProc(); }
            else { SelectBox(hit); }
        }
#ifdef __ANDROID__
        // 長押し: その譜面を譜面制作モードで開く (PC の F7)
        if (hit >= 0 && pointer.RightPressed && !IsMulti && hit < (int)BoxDatas.size() && !BoxDatas[hit]->IsGenre()) {
            if (Editor.Open && Editor.Dirty) { ShowSelectMessage("譜面制作モードに保存していない譜面があります"); }
            else if (EditorOpen(BoxDatas[hit]->GetChart()->ChartPath)) {
                ResetDemoSong();
                EditorReturnScene=Scene::SongSelect; NowScene = Scene::Editor;
                return;
            }
        }
#endif
        if (pointer.Wheel != 0) {
            const int size = (int)BoxDatas.size();
            SelectBox(((BoxDataIndex - pointer.Wheel) % size + size) % size, true);
        }
    }
    else {
        const GraphData& box = Skin.Base->SongSelect.Image.CourseBox;
        for (size_t i = 0; i <= (size_t)CourseType::Edit; ++i) {
            if (!pointer.LeftPressed) { break; }
            const float cy = box.Pos.Y + Skin.Base->SongSelect.Config.CourseBoxDistance * i;
            const UIRect r{ box.Pos.X - box.Size.Width * 0.5f, cy - box.Size.Height * 0.5f, box.Pos.X + box.Size.Width * 0.5f, cy + box.Size.Height * 0.5f };
            if (r.Contains(pointer.X, pointer.Y)) {
                if (i == CourseIndex) { DonInputProc(); }
                else { Skin.Base->SongSelect.SE.Ka.Play(); CourseIndex = i; }
                break;
            }
        }
        if (pointer.Wheel != 0) {
            if (WheelSoundReady()) { Skin.Base->SongSelect.SE.Ka.Play(); }
            CourseIndex = (size_t)std::clamp((int)CourseIndex - pointer.Wheel, 0, (int)CourseType::Edit);
        }
    }
    if (NowScene != Scene::SongSelect) {
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
                Skin.Base->SongSelect.SE.Don.Play();
                EnumChart(Config.SongDirectories);
                });
            Input::GetInstance().HitKeyProcess(VK_F3, KeyState::Down, [&] {
                Skin.Base->SongSelect.SE.Don.Play();
                Config.Load();
                });
            Input::GetInstance().HitKeyProcess(VK_F4, KeyState::Down, [&] {
                Skin.Base->SongSelect.SE.Don.Play();
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
                    EditorReturnScene=Scene::SongSelect; NowScene = Scene::Editor;
                }
                });

            FileImport();
        }

        return;
    }

#else
    if (!IsCourseSelect) {
        Touch.Process(TouchType::Other, ConfigInputProc, Skin.Base->SongSelect.Image.Config);
        Touch.Process(TouchType::Other, SearchInputProc, Skin.Base->SongSelect.Image.Search);
    }
    Touch.Process(TouchType::Don, DonInputProc);
    Touch.Process(TouchType::LeftKa, [&] {KaInputProc(false);});
    Touch.Process(TouchType::RightKa, [&] {KaInputProc(true);});
#endif

}
