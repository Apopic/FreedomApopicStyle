#include "../Game.hpp"

void Game::DanSelectInit() {

#ifndef __ANDROID__
    sys::SetDropEnabled(true);
#endif

    Skin.Base->DanSelect.SE.Don.SetVolume((Config.SEVolume));
    Skin.Base->DanSelect.SE.Ka.SetVolume((Config.SEVolume));
    Dan.Reset();
    CourseIndex = 0;
    SelectSceneEnter();
    IsCourseSelect = false;
    SongSelectMotionIndex = -1;
    SongSelectMotionCount = 0;
    SongSelectOpenAmount = 0;
    SongSelectClosingFrom = SongSelectClosingHeight = 0;

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
    sys::SetDropEnabled(false);
#endif
    SelectSceneLeave();
}

std::vector<std::string> Game::DanSelectDetails(const ChartData& chart) const {
    std::vector<std::string> lines;
    if (!chart.Subtitle.empty()) { lines.push_back(chart.Subtitle); }
    for (size_t i = 0; i < chart.DanSongs.size(); ++i) {
        const auto& song = chart.DanSongs[i];
        lines.push_back(std::to_string(i + 1) + ". " + song.Title + (song.Level ? "  ★×" + std::to_string(song.Level) : ""));
        for (const auto& exam : song.Exams) { lines.push_back("  " + ExamConditionText(exam)); }
    }
    if (!chart.ExamDatas.empty()) { lines.push_back("合格条件"); }
    for (const auto& exam : chart.ExamDatas) { lines.push_back(ExamConditionText(exam)); }
    return lines;
}

void Game::DanSelectDraw() {
    auto& skin = Skin.Base->DanSelect;
    gfx::SetBlend(gfx::Blend::None);
    skin.Image.BackGround.Draw({});
    UpdateSongSelectAnimation(((int)sys::Ticks()));
    const int center = (int)std::floor(SongSelectScrollPosition);
    auto heading = [&](const FontData& font, float x, float y, const std::string& text, float width, float scale, bool centered) {
        if (font.Handle == -1 || text.empty()) { return; }
        const float sx = scale * font.WidthScale;
        const std::string fitted = FitText(text, font.Handle, width / sx, false);
        const float measured = GetStrlenCached(fitted, font.Handle) * sx;
        txt::Draw(font.Handle, x - (centered ? measured * .5f : 0), y - font.Size * scale * .5f, fitted.c_str(), gfx::Rgb(255, 255, 255), 0, (float)(sx), (float)(scale));
    };
    for (int slot = center - 6; slot <= center + 6; ++slot) {
        const int index = SongSelectWrapIndex(slot);
        if (index < 0) { continue; }
        const auto& data = *BoxDatas[index];
        const GraphData box = SongSelectBoxLayout(slot);
        const bool selected = slot == (int)SongSelectScrollTarget;
        const unsigned int boost = selected ? 200 : 0;
        gfx::SetAddColor(std::min(255u,data.GenreColor.R+boost), std::min(255u,data.GenreColor.G+boost), std::min(255u,data.GenreColor.B+boost));
        DrawSliced(box, {}, skin.Config.BoxSlice);
        gfx::SetAddColor(0, 0, 0);
        const bool expandedTitle = selected && !data.IsGenre();
        const float titleY = box.Pos.Y - (box.Size.Height-skin.Image.Box.Size.Height)*.5f + (expandedTitle ? 8*SongSelectOpenAmount : 0);
        const std::string title = data.IsGenre() ? (data.GetGenre()->IsOpen ? "Close" : data.GetGenre()->Name) : data.GetChart()->Title;
        heading(skin.Font.Title, box.Pos.X, titleY, title, box.Size.Width-96, 1+(expandedTitle ? .125f*SongSelectOpenAmount : 0), true);
        if (data.IsGenre()) { continue; }
        if (IsFavorite(*data.GetChart())) { DrawFavoriteMark(box, skin.Font.Title, titleY); }
        GraphData crown = skin.Image.Crown;
        crown.Pos.X += box.Pos.X-skin.Image.Box.Pos.X;
        crown.Pos.Y = titleY;
        crown.Draw({},ScoreDataLoad(data.GetChart()->ChartPath.string(),(int)CourseType::Dan).Crown);
        if (!selected || SongSelectOpenAmount <= .85f) { continue; }
        const auto lines = DanSelectDetails(*data.GetChart());
        const auto& font = skin.Font.SubTitle.Handle == -1 ? skin.Font.Title : skin.Font.SubTitle;
        const float top = titleY + std::max(30.0f,font.Size*1.6f);
        const float bottom = box.Pos.Y + box.Size.Height*.5f-20;
        const float line = std::min(std::max(24.0f,font.Size*1.3f), (bottom-top)/std::max(1.0f,(float)lines.size()));
        const float scale = std::min(1.0f,line/std::max(1.0f,font.Size*1.3f));
        gfx::SetBlend(gfx::Blend::Alpha, (int)(255*(SongSelectOpenAmount-.85f)/.15f));
        for (size_t i=0;i<lines.size();++i) { heading(font,box.Pos.X-box.Size.Width*.5f+32,top+line*i,lines[i],box.Size.Width-64,scale,false); }
        gfx::SetBlend(gfx::Blend::None);
    }
    gfx::SetBlend(gfx::Blend::None);
    DrawSearchStatus(skin.Font.KeyWord);
    if (FilterUI.Open) { DrawFilterPanel(skin.Font.KeyWord); }
#ifdef __ANDROID__
    gfx::SetBlend(gfx::Blend::None);
    skin.Image.Back.Draw({}); skin.Image.Config.Draw({}); skin.Image.Random.Draw({}); skin.Image.Search.Draw({});
#endif
}

void Game::DanSelectProc() {

    if (BoxDatas.empty()) {
        NowScene = Scene::ModeSelect;
        return;
    }
    UpdateSongSelectAnimation(((int)sys::Ticks()));
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
    auto tapped = [&](const GraphData& image) {
        const UIRect rect{image.Pos.X-image.Size.Width*.5f,image.Pos.Y-image.Size.Height*.5f,
            image.Pos.X+image.Size.Width*.5f,image.Pos.Y+image.Size.Height*.5f};
        return pointer.LeftPressed && rect.Contains(pointer.X,pointer.Y);
    };
    if (tapped(Skin.Base->DanSelect.Image.Back)) { BackInputProc(); return; }
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
            SetSongSelectBox(BoxDataIndex - 1, -1);
        }
        else {
            SetSongSelectBox(BoxDataIndex + 1, 1);
        }
        };
    auto RandomInputProc = [&] {
        if (!SelectRandomChartBox()) { return; }
        Skin.Base->DanSelect.SE.Ka.Play();
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

#ifdef __ANDROID__
    if (tapped(Skin.Base->DanSelect.Image.Config)) { ConfigInputProc(); return; }
    if (tapped(Skin.Base->DanSelect.Image.Search)) { SearchInputProc(); return; }
    if (tapped(Skin.Base->DanSelect.Image.Random)) { RandomInputProc(); return; }
#endif
    // マウスと Android のタップ・スワイプ。
    {
        const auto slot = HitTestSongSelectBox((float)pointer.X, (float)pointer.Y);
        const int hit = slot ? SongSelectWrapIndex(*slot) : -1;
        if (hit >= 0 && pointer.LeftPressed) {
            if (*slot == (int)SongSelectScrollTarget) { DonInputProc(); }
            else { Skin.Base->DanSelect.SE.Ka.Play(); SetSongSelectBox(hit, *slot-(int)SongSelectScrollTarget); }
        }
        if (pointer.Wheel != 0) {
            if (WheelSoundReady()) { Skin.Base->DanSelect.SE.Ka.Play(); }
            const int size = (int)BoxDatas.size();
            SetSongSelectBox(((BoxDataIndex - pointer.Wheel) % size + size) % size, -pointer.Wheel);
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
#endif
}
