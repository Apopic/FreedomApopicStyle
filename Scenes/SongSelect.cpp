#include "../Game.hpp"

void Game::SongSelectInit() {

#ifndef __ANDROID__
    sys::SetDropEnabled(true);
#endif
    Skin.Base->SongSelect.SE.Don.SetVolume((Config.SEVolume));
    Skin.Base->SongSelect.SE.Ka.SetVolume((Config.SEVolume));
    SelectSceneEnter();
    SongSelectMotionIndex = -1;
    SongSelectMotionCount = 0;
    SongSelectOpenAmount = 0;
    SongSelectTransitionStart = -1;
    SongSelectResultStart = ((int)sys::Ticks());

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
    sys::SetDropEnabled(false);
#endif

    DemoSongPlayBlank.Reset();
    ResetDemoSong();
    IsCourseSelect = false;
    SongSelectTransitionStart = -1;
    SelectSceneLeave();
}

float Game::SongSelectBoxExtraHeight() const {
    if (BoxDatas.empty() || BoxDatas[BoxDataIndex]->IsGenre()) { return 0; }
    if (NowScene == Scene::DanSelect) {
        const float line = std::max(24.0f, Skin.Base->DanSelect.Font.SubTitle.Size * 1.3f);
        return std::clamp((DanSelectDetails(*BoxDatas[BoxDataIndex]->GetChart()).size() + 2) * line, 220.0f, 440.0f) * SongSelectOpenAmount;
    }
    const auto& fonts = Skin.Base->SongSelect.Font;
    return std::max(140.0f, fonts.SubTitle.Size * 5.8f) * SongSelectOpenAmount;
}

int Game::SongSelectWrapIndex(int slot) const {
    const int count = static_cast<int>(BoxDatas.size());
    return count > 0 ? (slot % count + count) % count : -1;
}

void Game::UpdateSongSelectAnimation(int now) {
    const int count = static_cast<int>(BoxDatas.size());
    if (count == 0) { SongSelectMotionCount = 0; SongSelectMotionIndex = -1; return; }
    if (SongSelectMotionCount != count || SongSelectMotionIndex < 0) {
        SongSelectScrollPosition = SongSelectScrollFrom = SongSelectScrollTarget = BoxDataIndex;
        SongSelectMotionIndex = BoxDataIndex;
        SongSelectMotionCount = count;
        SongSelectMoveStart = now - 200;
        SongSelectSettledAt = now;
        SongSelectClosingFrom = SongSelectClosingHeight = 0;
    }
    else if (SongSelectMotionIndex != BoxDataIndex) {
        int delta = BoxDataIndex - SongSelectMotionIndex;
        if (delta > count / 2) { delta -= count; }
        if (delta < -count / 2) { delta += count; }
        SongSelectScrollFrom = SongSelectScrollPosition;
        SongSelectScrollTarget += delta;
        SongSelectMoveStart = now;
        SongSelectSettledAt = -1;
        SongSelectMotionIndex = BoxDataIndex;
    }
    const float t = std::clamp((now - SongSelectMoveStart) / 200.0f, 0.0f, 1.0f);
    const float close = std::clamp((now - SongSelectClosingStart) / 120.0f, 0.0f, 1.0f);
    SongSelectClosingHeight = SongSelectClosingFrom * (1 - close * close * (3 - 2 * close));
    const double eased = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
    SongSelectScrollPosition = SongSelectScrollFrom + (SongSelectScrollTarget - SongSelectScrollFrom) * eased;
    if (t < 1) { SongSelectOpenAmount = 0; return; }
    // Stop at an exact coordinate before starting the opening delay.
    SongSelectScrollPosition = SongSelectScrollTarget;
    if (SongSelectSettledAt < 0) { SongSelectSettledAt = SongSelectMoveStart + 200; }
    const int shift = static_cast<int>(SongSelectScrollTarget) - BoxDataIndex;
    SongSelectScrollPosition -= shift;
    SongSelectScrollFrom -= shift;
    SongSelectScrollTarget -= shift;
    SongSelectClosingSlot -= shift;
    const float open = std::clamp((now - SongSelectSettledAt - 450) / 180.0f, 0.0f, 1.0f);
    SongSelectOpenAmount = open * open * (3.0f - 2.0f * open);
}

void Game::SetSongSelectBox(int index, int steps) {
    if (BoxDatas.empty()) { return; }
    const int now = ((int)sys::Ticks());
    UpdateSongSelectAnimation(now);
    index = SongSelectWrapIndex(index);
    if (index == BoxDataIndex && steps == 0) { return; }
    if (const float height = SongSelectBoxExtraHeight(); height > 0) {
        SongSelectClosingSlot = static_cast<int>(SongSelectScrollTarget);
        SongSelectClosingStart = now;
        SongSelectClosingFrom = SongSelectClosingHeight = height;
    }
    int delta = steps != 0 ? steps : index - BoxDataIndex;
    if (steps == 0) {
        const int count = static_cast<int>(BoxDatas.size());
        if (delta > count / 2) { delta -= count; }
        if (delta < -count / 2) { delta += count; }
    }
    SongSelectScrollFrom = SongSelectScrollPosition;
    SongSelectScrollTarget += delta;
    SongSelectMoveStart = now;
    SongSelectSettledAt = -1;
    SongSelectOpenAmount = 0;
    BoxDataIndex = SongSelectMotionIndex = index;
}

bool Game::SelectRandomChartBox() {
    int selected = -1, count = 0;
    for (int i = 0; i < (int)BoxDatas.size(); ++i) {
        if (BoxDatas[i]->IsGenre()) { continue; }
        if (sys::Rand(count++) == 0) { selected = i; }
    }
    if (selected < 0) { return false; }
    SetSongSelectBox(selected);
    return true;
}

void Game::BeginSongSelectTransition(bool courseSelect) {
    if (SongSelectTransitionStart >= 0 || IsCourseSelect == courseSelect) { return; }
    SongSelectTransitionTarget = courseSelect;
    SongSelectTransitionStart = ((int)sys::Ticks());
}

void Game::UpdateSongSelectTransition() {
    if (SongSelectTransitionStart < 0) { return; }
    const int elapsed = ((int)sys::Ticks()) - SongSelectTransitionStart;
    if (elapsed >= 250 && IsCourseSelect != SongSelectTransitionTarget) {
        IsCourseSelect = SongSelectTransitionTarget;
        if (IsCourseSelect) { SongSelectResultStart = ((int)sys::Ticks()); }
    }
    if (elapsed >= 500) { SongSelectTransitionStart = -1; }
}

void Game::DrawCourseHeading(FontData& font, Pos2D<float> position, const std::string& text) {
    if (font.Handle == -1 || text.empty()) { return; }
    const auto& box = Skin.Base->SongSelect.Image.TitleBox;
    font.Draw(position, gfx::Rgb(255, 255, 255), 0,
        FitText(text, font.Handle, (box.Size.Width - 48.0f) / font.WidthScale, false));
}

float Game::SongSelectContentOpacity() const {
    if (SongSelectTransitionStart < 0) { return 1; }
    const float elapsed = (float)((int)sys::Ticks() - SongSelectTransitionStart);
    return std::clamp(std::abs(elapsed - 250.0f) / 250.0f, 0.0f, 1.0f);
}

void Game::DrawCourseBestResult(int course, float y, float contentOpacity) {
    const auto& config = Skin.Base->SongSelect.Config;
    const ScoreData best = ScoreDataLoad(BoxDatas[BoxDataIndex]->GetChart()->ChartPath.string(), course);
    const int elapsed = std::max(0, ((int)sys::Ticks()) - SongSelectResultStart) % 6400;
    const int phase = elapsed % 3200;
    const float opacity = std::clamp(std::min(phase / 240.0f, (3200 - phase) / 240.0f), 0.0f, 1.0f);
    const float width = std::max(1.0f, config.HighScorePos.X - config.LevelPos.X - 100.0f);
    gfx::SetBlend(gfx::Blend::Alpha, (int)(255 * opacity * contentOpacity));
    if (elapsed < 3200) {
        auto& font = Skin.Base->SongSelect.Font.HighScore;
        font.Draw({ config.HighScorePos.X, config.HighScorePos.Y + y }, gfx::Rgb(255, 255, 255), 0,
            FitText(std::to_string(best.Score), font.Handle, width, false));
    }
    else {
        const auto& subtitle = Skin.Base->SongSelect.Font.SubTitle;
        const auto& font = subtitle.Handle == -1 ? Skin.Base->SongSelect.Font.HighScore : subtitle;
        const float x = config.HighScorePos.X, cy = config.HighScorePos.Y + y;
        auto drawRow = [&](const std::array<std::string, 3>& items, float textY) {
            const float column = width / 3.0f;
            const float usable = std::max(1.0f, column - 20.0f);
            for (size_t i = 0; i < items.size(); ++i) {
                const auto& text = items[i];
                const float measured = static_cast<float>(GetStrlenCached(text, font.Handle));
                const float scale = measured > 0 ? std::min(1.0f, usable / measured) : 1.0f;
                const float center = x - width + column * (static_cast<float>(i) + 0.5f);
                txt::Draw(font.Handle, center - measured * scale * 0.5f, textY - font.Size * scale * 0.5f, text.c_str(), gfx::Rgb(255, 255, 255), 0, (float)(scale), (float)(scale));
            }
        };
        if (best.HasResult) {
            char accuracy[32];
            std::snprintf(accuracy, sizeof(accuracy), "%.2f%%", best.Accuracy);
            drawRow({ "良 " + std::to_string(best.Good), "可 " + std::to_string(best.Ok), "不可 " + std::to_string(best.Bad) }, cy - font.Size * 0.62f);
            drawRow({ "連打 " + std::to_string(best.Roll), "最大コンボ " + std::to_string(best.MaxCombo), "精度 " + std::string(accuracy) }, cy + font.Size * 0.62f);
        }
        else {
            drawRow({ "良 -", "可 -", "不可 -" }, cy - font.Size * 0.62f);
            drawRow({ "連打 -", "最大コンボ -", "精度 -" }, cy + font.Size * 0.62f);
        }
    }
    gfx::SetBlend(gfx::Blend::Alpha, (int)(255 * contentOpacity));
    Skin.Base->SongSelect.Image.Crown.Draw({ 0, y }, best.Crown);
}

GraphData Game::SongSelectBoxLayout(int index) const {
    const bool dan = NowScene == Scene::DanSelect;
    const auto distance = dan ? Skin.Base->DanSelect.Config.BoxDistance : Skin.Base->SongSelect.Config.BoxDistance;
    const auto origin = dan ? Skin.Base->DanSelect.Config.SongBoxListPos : Skin.Base->SongSelect.Config.SongBoxListPos;
    GraphData box = dan ? Skin.Base->DanSelect.Image.Box : Skin.Base->SongSelect.Image.Box;
    const float relative = static_cast<float>(index - SongSelectScrollPosition);
    const int targetRelative = index - static_cast<int>(SongSelectScrollTarget);
    const float extra = SongSelectBoxExtraHeight();
    box.Pos.X += relative * distance.X;
    box.Pos.Y += origin.Y + relative * distance.Y;
    if (targetRelative == 0) { box.Size.Height += extra; }
    else { box.Pos.Y += (targetRelative < 0 ? -extra : extra) * 0.5f; }
    if (index == SongSelectClosingSlot) { box.Size.Height += SongSelectClosingHeight; }
    else { box.Pos.Y += (index < SongSelectClosingSlot ? -SongSelectClosingHeight : SongSelectClosingHeight) * 0.5f; }
    return box;
}

std::optional<int> Game::HitTestSongSelectBox(float x, float y) const {
    if (BoxDatas.empty()) { return std::nullopt; }
    std::optional<int> hit;
    float nearest = std::numeric_limits<float>::max();
    const int center = static_cast<int>(std::floor(SongSelectScrollPosition));
    for (int i = center - 6; i <= center + 6; ++i) {
        const GraphData box = SongSelectBoxLayout(i);
        const UIRect rect{ box.Pos.X - box.Size.Width * 0.5f, box.Pos.Y - box.Size.Height * 0.5f,
            box.Pos.X + box.Size.Width * 0.5f, box.Pos.Y + box.Size.Height * 0.5f };
        const float distance = std::abs(y - box.Pos.Y);
        if (x >= rect.L && x < rect.R && y >= rect.T && y < rect.B && distance < nearest) {
            hit = i;
            nearest = distance;
        }
    }
    return hit;
}

void Game::SongSelectDraw() {

    UpdateSongSelectTransition();
    const float contentOpacity = SongSelectContentOpacity();
    const int contentAlpha = (int)(255 * contentOpacity);
    gfx::SetBlend(gfx::Blend::None);
    Skin.Base->SongSelect.Image.BackGround.Draw({});
    auto drawHeading = [&](const FontData& font, Pos2D<float> position, const std::string& text, float width, float scale, int align) {
        if (font.Handle == -1 || text.empty()) { return; }
        const float horizontal = font.WidthScale * scale;
        const std::string fitted = FitText(text, font.Handle, width / horizontal, false);
        const float measured = static_cast<float>(GetStrlenCached(fitted, font.Handle)) * horizontal;
        const float offset = align == 1 ? measured * 0.5f : (align == 2 ? measured : 0.0f);
        txt::Draw(font.Handle, position.X - offset, position.Y - font.Size * scale * 0.5f, fitted.c_str(), gfx::Rgb(255, 255, 255), 0, (float)(horizontal), (float)(scale));
    };

    if (!IsCourseSelect) {

        UpdateSongSelectAnimation(((int)sys::Ticks()));
        const int center = static_cast<int>(std::floor(SongSelectScrollPosition));
        for (int slot = center - 6; slot <= center + 6; ++slot) {
            const int i = SongSelectWrapIndex(slot);
            if (i < 0) { continue; }

            const GraphData box = SongSelectBoxLayout(slot);
            const bool selected = slot == static_cast<int>(SongSelectScrollTarget);
            Pos2D<float> pos = {
                Skin.Base->SongSelect.Config.SongBoxListPos.X + box.Pos.X - Skin.Base->SongSelect.Image.Box.Pos.X,
                box.Pos.Y - (box.Size.Height - Skin.Base->SongSelect.Image.Box.Size.Height) * 0.5f
            };

            unsigned int c = 200 * selected;
            gfx::SetBlend(gfx::Blend::Alpha, contentAlpha);
            gfx::SetAddColor(std::min(255u, BoxDatas[i]->GenreColor.R + c), std::min(255u, BoxDatas[i]->GenreColor.G + c), std::min(255u, BoxDatas[i]->GenreColor.B + c));
            DrawSliced(box, {}, Skin.Base->SongSelect.Config.BoxSlice);
            gfx::SetAddColor(0, 0, 0);

            if (BoxDatas[i]->IsGenre()) {
                if (BoxDatas[i]->GetGenre()->IsOpen) {
                    Skin.Base->SongSelect.Font.Title.Draw(
                        pos,
                        gfx::Rgb(255, 255, 255),
                        gfx::Rgb(0, 0, 0),
                        "Close"
                    );
                }
                else {
                    Skin.Base->SongSelect.Font.Title.Draw(
                        pos,
                        gfx::Rgb(255, 255, 255),
                        gfx::Rgb(0, 0, 0),
                        FitText(BoxDatas[i]->GetGenre()->Name, Skin.Base->SongSelect.Font.Title.Handle, Skin.Base->SongSelect.Image.Box.Size.Width - 48.0f, false)
                    );
                }
            }
            else {
                const auto& titleFont = Skin.Base->SongSelect.Font.Title;
                if (selected) {
                    drawHeading(titleFont, { pos.X, pos.Y + 8.0f * SongSelectOpenAmount }, BoxDatas[i]->GetChart()->Title,
                        box.Size.Width - 96.0f, 1.0f + 0.125f * SongSelectOpenAmount, static_cast<int>(titleFont.Position));
                }
                else { Skin.Base->SongSelect.Font.Title.Draw(
                    pos,
                    gfx::Rgb(255, 255, 255),
                    gfx::Rgb(0, 0, 0),
                    // 長い曲名は箱の幅に収める (お気に入りの印の分も空ける)
                    FitText(BoxDatas[i]->GetChart()->Title, Skin.Base->SongSelect.Font.Title.Handle, Skin.Base->SongSelect.Image.Box.Size.Width - 96.0f, false)
                ); }
                if (IsFavorite(*BoxDatas[i]->GetChart())) {
                    DrawFavoriteMark(box, Skin.Base->SongSelect.Font.Title, pos.Y);
                }
                if (selected && SongSelectOpenAmount > 0.85f) {
                    const auto& chart = *BoxDatas[i]->GetChart();
                    auto font = Skin.Base->SongSelect.Font.SubTitle;
                    if (font.Handle == -1) { font = Skin.Base->SongSelect.Font.Title; }
                    const float line = std::max(24.0f, font.Size * 1.3f);
                    const float left = box.Pos.X - box.Size.Width * 0.5f + 32.0f;
                    const float width = std::max(1.0f, box.Size.Width - 64.0f);
                    gfx::SetBlend(gfx::Blend::Alpha, (int)(255 * contentOpacity * (SongSelectOpenAmount - 0.85f) / 0.15f));
                    drawHeading(font, { box.Pos.X, pos.Y + line + 14.0f }, chart.Subtitle, width, 1.125f, 1);
                    const float courseY = pos.Y + line * 3.0f;
                    const float levelY = courseY + line;
                    const float column = width / ((int)CourseType::Edit + 1);
                    for (int course = 0; course <= (int)CourseType::Edit; ++course) {
                        const auto& data = chart.Courses[course];
                        const float x = left + column * (course + 0.5f);
                        const unsigned int color = data.IsPlayable ? gfx::Rgb(255, 255, 255) : gfx::Rgb(205, 205, 205);
                        const std::string name(magic_enum::enum_name((CourseType)course));
                        UIText(font, x, courseY, FitText(name, font.Handle, column - 8.0f, false), color, 1);
                        UIText(font, x, levelY, data.IsPlayable ? "★×" + std::to_string(data.Level) : "--", color, 1);
                    }
                    gfx::SetBlend(gfx::Blend::None);
                }
            }
        }
        gfx::SetBlend(gfx::Blend::None);

#ifdef __ANDROID__
        Skin.Base->SongSelect.Image.Config.Draw({});
        Skin.Base->SongSelect.Image.Random.Draw({});
        Skin.Base->SongSelect.Image.Search.Draw({});
#endif

    }
    else {
        gfx::SetBlend(gfx::Blend::Alpha, contentAlpha);
        Skin.Base->SongSelect.Image.TitleBox.Draw({});
        DrawCourseHeading(Skin.Base->SongSelect.Font.BoxTitle,
            Skin.Base->SongSelect.Config.BoxTitlePos,
            BoxDatas[BoxDataIndex]->GetChart()->Title
        );
        DrawCourseHeading(Skin.Base->SongSelect.Font.BoxSubTitle,
            Skin.Base->SongSelect.Config.BoxSubTitlePos,
            BoxDatas[BoxDataIndex]->GetChart()->Subtitle
        );

        for (size_t i = 0; i <= (size_t)CourseType::Edit; ++i) {

            unsigned int c = 100 * (CourseIndex == i);
            float y = Skin.Base->SongSelect.Config.CourseBoxDistance * i;
            gfx::SetBlend(gfx::Blend::Alpha, contentAlpha);
            gfx::SetAddColor(c, c, c);
            Skin.Base->SongSelect.Image.CourseBox.Draw({ 0, y });
            gfx::SetAddColor(0, 0, 0);

            if (!BoxDatas[BoxDataIndex]->GetChart()->Courses[i].IsPlayable) {
                continue;
            }

            Skin.Base->SongSelect.Font.Course.Draw({ Skin.Base->SongSelect.Config.CoursePos.X,Skin.Base->SongSelect.Config.CoursePos.Y + y }, gfx::Rgb(255, 255, 255), gfx::Rgb(0, 0, 0), magic_enum::enum_name((CourseType)i).data());
            Skin.Base->SongSelect.Font.Level.Draw({ Skin.Base->SongSelect.Config.LevelPos.X,Skin.Base->SongSelect.Config.LevelPos.Y + y }, gfx::Rgb(255, 255, 255), gfx::Rgb(0, 0, 0), "★×" + std::to_string(BoxDatas[BoxDataIndex]->GetChart()->Courses[i].Level));

            DrawCourseBestResult(numeric::Cast<int>(i), y, contentOpacity);
        }
    }
    gfx::SetBlend(gfx::Blend::None);
    if (!IsCourseSelect) {
        DrawSearchStatus(Skin.Base->SongSelect.Font.KeyWord);
        if (FilterUI.Open) {
            DrawFilterPanel(Skin.Base->SongSelect.Font.KeyWord);
        }
    }
#ifdef __ANDROID__
    gfx::SetBlend(gfx::Blend::None);
    Skin.Base->SongSelect.Image.Back.Draw({});
#endif
}

void Game::SongSelectProc() {

    if (BoxDatas.empty()) {
        NowScene = !IsMulti ? Scene::ModeSelect : Scene::MultiRoom;
        return;
    }
    UpdateSongSelectTransition();
    if (SongSelectTransitionStart >= 0) { return; }
    if (!IsCourseSelect) { UpdateSongSelectAnimation(((int)sys::Ticks())); }
    if (FilterUI.Open) {
        FilterPanelProc();
        return;
    }
    const PointerState pointer = ReadPointer();

    auto BackInputProc = [&] {
        if (IsCourseSelect) {
            BeginSongSelectTransition(false);
        }
        else {
            NowScene = !IsMulti ? Scene::ModeSelect : Scene::MultiRoom;
        }
        };

#ifndef __ANDROID__
    Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
#else
    auto tapped = [&](const GraphData& image) {
        const UIRect rect{ image.Pos.X - image.Size.Width * 0.5f, image.Pos.Y - image.Size.Height * 0.5f,
            image.Pos.X + image.Size.Width * 0.5f, image.Pos.Y + image.Size.Height * 0.5f };
        return pointer.LeftPressed && rect.Contains(pointer.X, pointer.Y);
    };
    if (tapped(Skin.Base->SongSelect.Image.Back)) { BackInputProc(); return; }
#endif
    if (SongSelectTransitionStart >= 0) { return; }

    auto DonInputProc = [&] {
        if (SongSelectTransitionStart >= 0) { return; }
        Skin.Base->SongSelect.SE.Don.Play();
        if (BoxDatas[BoxDataIndex]->IsGenre()) {
            bool& _f = BoxDatas[BoxDataIndex]->GetGenre()->IsOpen;
            _f = !_f;
            BoxDatasUpdate();
        }
        else {
            if (!IsCourseSelect) {
                BeginSongSelectTransition(true);
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
        if (SongSelectTransitionStart >= 0) { return; }
        Skin.Base->SongSelect.SE.Ka.Play();
        if (!direction) {
            if (!IsCourseSelect) {
                SetSongSelectBox(BoxDataIndex - 1, -1);
                DemoSongPlayBlank.Reset();
                ResetDemoSong();
            }
            else {
                CourseIndex -= CourseIndex > 0;
            }
        }
        else {
            if (!IsCourseSelect) {
                SetSongSelectBox(BoxDataIndex + 1, 1);
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
        if (!SelectRandomChartBox()) { return; }
        Skin.Base->SongSelect.SE.Ka.Play();
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
#ifdef __ANDROID__
    if (!IsCourseSelect) {
        if (tapped(Skin.Base->SongSelect.Image.Config)) { ConfigInputProc(); return; }
        if (tapped(Skin.Base->SongSelect.Image.Search)) { SearchInputProc(); return; }
        if (tapped(Skin.Base->SongSelect.Image.Random)) { RandomInputProc(); return; }
    }
#endif

    // ---- マウス: クリックで選択、選択中をクリックで決定、右クリックで戻る、中クリックでお気に入り ----
    auto SelectBox = [&](int index, bool wheel = false, int steps = 0) {
        if (SongSelectTransitionStart >= 0) { return; }
        if (!wheel || WheelSoundReady()) { Skin.Base->SongSelect.SE.Ka.Play(); }
        SetSongSelectBox(index, steps);
        DemoSongPlayBlank.Reset();
        ResetDemoSong();
        };
    if (!IsCourseSelect) {
        const auto hitSlot = HitTestSongSelectBox(static_cast<float>(pointer.X), static_cast<float>(pointer.Y));
        const int hit = hitSlot ? SongSelectWrapIndex(*hitSlot) : -1;
        if (hit >= 0 && pointer.LeftPressed) {
            if (*hitSlot == static_cast<int>(SongSelectScrollTarget)) { DonInputProc(); }
            else { SelectBox(hit, false, *hitSlot - static_cast<int>(SongSelectScrollTarget)); }
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
            SelectBox(((BoxDataIndex - pointer.Wheel) % size + size) % size, true, -pointer.Wheel);
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
    if (NowScene != Scene::SongSelect || SongSelectTransitionStart >= 0) {
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

#endif

}
