#include "../Game.hpp"

const json& Game::GetScoreCache() {
    if (!IsScoreCacheLoaded) {
        IsScoreCacheLoaded = true;
        ScoreCache = json::object();
        std::ifstream ifs(GetExecutablePath() / "scoredata.json", std::ios::binary);
        if (ifs.is_open()) {
            std::vector<uint8_t> bson_vec((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            json data = json::from_bson(bson_vec, true, false);
            if (data.is_object()) {
                ScoreCache = std::move(data);
            }
        }
    }
    return ScoreCache;
}

void Game::ScoreDataSave(uint64_t score, int crown, const JudgeData* result) {
    if (Editor.Testing) { return; }     // 譜面制作のテストプレイは記録しない

    std::string ChartPath = Chart.OriginalData.ChartPath.string();
    std::string CourseName = magic_enum::enum_name((CourseType)CourseIndex).data();
    const ScoreData previous = ScoreDataLoad(ChartPath, numeric::Cast<int>(CourseIndex));

    GetScoreCache();
    // 壊れた記録 (オブジェクトでない値) があっても例外にしないよう、作り直してから書く
    if (!ScoreCache[ChartPath].is_object()) { ScoreCache[ChartPath] = json::object(); }
    if (!ScoreCache[ChartPath][CourseName].is_object()) { ScoreCache[ChartPath][CourseName] = json::object(); }
    ScoreCache[ChartPath][CourseName]["Score"] = score;
    ScoreCache[ChartPath][CourseName]["Crown"] = crown;
    auto& record = ScoreCache[ChartPath][CourseName];
    // 判定数はベストスコアを出した演奏と組にする。王冠だけの更新では置き換えない。
    if (result && result->Score == score && (score > previous.Score || !previous.HasResult)) {
        record["Good"] = result->Good;
        record["Ok"] = result->Ok;
        record["Bad"] = result->Bad;
        record["Roll"] = result->Roll;
        record["MaxCombo"] = result->MaxCombo;
        record["Accuracy"] = result->Accuracy;
        record["HasResult"] = true;
    }
    else if (score != previous.Score) { record["HasResult"] = false; }

    std::vector<uint8_t> v_bson = json::to_bson(ScoreCache);
    std::ofstream ofs(GetExecutablePath() / "scoredata.json", std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(v_bson.data()), v_bson.size());
}

auto Game::ScoreDataLoad(const std::string& chartpath, int courseindex) -> ScoreData {

    ScoreData Score;
    std::string CourseName = magic_enum::enum_name((CourseType)courseindex).data();
    const json& data = GetScoreCache();

    auto chart = data.find(chartpath);
    if (chart == data.end() || !chart->is_object()) {
        return Score;
    }
    auto course = chart->find(CourseName);
    if (course == chart->end() || !course->is_object()) {
        return Score;
    }
    // 値の型が違う (壊れた記録) 時は読まない
    if (auto it = course->find("Score"); it != course->end() && it->is_number()) { Score.Score = it->get<decltype(Score.Score)>(); }
    if (auto it = course->find("Crown"); it != course->end() && it->is_number()) { Score.Crown = it->get<decltype(Score.Crown)>(); }
    auto count = [&](const char* name, uint64_t& value) {
        const auto it = course->find(name);
        if (it == course->end() || !it->is_number_integer() || (!it->is_number_unsigned() && it->get<int64_t>() < 0)) { return false; }
        value = it->get<uint64_t>();
        return true;
    };
    const auto has = course->find("HasResult"), accuracy = course->find("Accuracy");
    Score.HasResult = has != course->end() && has->is_boolean() && has->get<bool>()
        && count("Good", Score.Good) && count("Ok", Score.Ok) && count("Bad", Score.Bad)
        && count("Roll", Score.Roll) && count("MaxCombo", Score.MaxCombo)
        && accuracy != course->end() && accuracy->is_number();
    if (Score.HasResult) {
        Score.Accuracy = accuracy->get<double>();
        Score.HasResult = std::isfinite(Score.Accuracy) && Score.Accuracy >= 0 && Score.Accuracy <= 100;
    }
    return Score;
}

void Game::ResultEnd() {
#ifndef __ANDROID__
    RecorderStop(); // リザルトを離れる直前まで録画する
    if (Recorder.AwaitingDecision) { RecorderDecide(true); }
#endif
    if (IsMulti) {
        ResultIndex = 0;
        Online.FinishSent = false;
        Online.LaneIds.clear();
        Chart.Init(true);
        return;
    }
    // オートプレイ (途中で切り替えた場合も) の結果は記録に残さない
    else if (Chart.AutoUsed || Config.AutoPlay) {}
    else if (const ScoreData saved = ScoreDataLoad(Chart.OriginalData.ChartPath.string(), numeric::Cast<int>(CourseIndex));
        Chart.Judge[0].Score > saved.Score || CrownIndex > saved.Crown || (!saved.HasResult && Chart.Judge[0].Score == saved.Score)) {
        // スコアと王冠はそれぞれ最高記録を残す (王冠だけ更新した場合も保存する)
        ScoreDataSave(std::max(Chart.Judge[0].Score, saved.Score), std::max(CrownIndex, saved.Crown), &Chart.Judge[0]);
    }

    CrownIndex = 0;
    Dan.Reset();
    Chart.Init(true);
}

void Game::ResultDraw() {

    gfx::SetBlend(gfx::Blend::None);
    Skin.Base->Result.Image.BackGround.Draw({});
    Skin.Base->Result.Image.JudgeScore.Draw({});
    Skin.Base->Result.Image.Accuracy.Draw({});
    Skin.Base->Result.Image.Score.Draw({});

    auto ScoreDraw = [&](uint64_t num) {
        int digit = exmath::digit(num);
        float offset = 0;
        int i = 0;
        do {
            Skin.Base->Result.Image.Number.Draw({ offset + Skin.Base->Result.Config.ScorePos.X, Skin.Base->Result.Config.ScorePos.Y }, num % 10);
            num /= 10;
            ++i;
            offset -= Skin.Base->Result.Image.Number.Size.Width;
        } while (i < digit);
        };
    auto AccuracyDraw = [&](double Rate) {

        int iRate = numeric::Cast<int>(Rate * 100);
        int digit = exmath::digit(iRate) + 2;
        if (iRate == 0) {
            digit = 5;
        }

        float offset = 0;
        Skin.Base->Result.Image.Number.Draw({ offset + Skin.Base->Result.Config.AccracyPos.X, Skin.Base->Result.Config.AccracyPos.Y }, 11);
        offset -= Skin.Base->Result.Image.Number.Size.Width;
        --digit;

        int i = 0;
        do {
            if (i == 2) {
                Skin.Base->Result.Image.Number.Draw({ offset + Skin.Base->Result.Config.AccracyPos.X, Skin.Base->Result.Config.AccracyPos.Y }, 12);
            }
            else {
                Skin.Base->Result.Image.Number.Draw({ offset + Skin.Base->Result.Config.AccracyPos.X, Skin.Base->Result.Config.AccracyPos.Y }, iRate % 10);
                iRate /= 10;
            }
            ++i;
            offset -= Skin.Base->Result.Image.Number.Size.Width;
        } while (i < digit);
        };
    auto JudgeDraw = [&](const Pos2D<float>& Pos, uint64_t num) {
        int digit = exmath::digit(num) + 1;
        float offset = Skin.Base->Result.Image.Number.Size.Width * digit;
        Skin.Base->Result.Image.Number.Draw({ offset + Pos.X, Pos.Y }, 10);
        offset -= Skin.Base->Result.Image.Number.Size.Width;
        --digit;
        size_t i = 0;
        do {
            Skin.Base->Result.Image.Number.Draw({ offset + Pos.X, Pos.Y }, num % 10);
            num /= 10;
            ++i;
            offset -= Skin.Base->Result.Image.Number.Size.Width;
        } while (i < digit);
        };

    JudgeData Judge = Chart.Judge[ResultIndex];

    ScoreDraw(Judge.Score);
    AccuracyDraw(Judge.Accuracy);
    JudgeDraw(Skin.Base->Result.Config.GoodPos, Judge.Good);
    JudgeDraw(Skin.Base->Result.Config.OkPos, Judge.Ok);
    JudgeDraw(Skin.Base->Result.Config.BadPos, Judge.Bad);
    JudgeDraw(Skin.Base->Result.Config.RollPos, Judge.Roll);
    JudgeDraw(Skin.Base->Result.Config.MaxComboPos, Judge.MaxCombo);

    CrownIndex = 0;
    if (!Chart.IsDanMode()) {
        if (Judge.Accuracy >= 75) {
            CrownIndex = 1;
        }
        if (Judge.Accuracy >= 90) {
            CrownIndex = 2;
        }
        if (Judge.Accuracy >= 90 && Judge.Bad == 0) {
            CrownIndex = 3;
        }
        if (Judge.Accuracy >= 90 && Judge.Bad == 0 && Judge.Ok == 0) {
            CrownIndex = 4;
        }
    }
    else {
        CrownIndex = DanCrown();

        // 各条件の結果を一覧表示する
        const int lineheight = 16 + 6;
        const int y0 = Skin.Info.Resolution.Y - 24 - lineheight * (int)Dan.Exams.size();
        for (size_t i = 0; i < Dan.Exams.size(); ++i) {
            const auto& e = Dan.Exams[i];
            const char* status = "";
            unsigned int color = gfx::Rgb(255, 255, 255);
            switch (e.Status) {
            case ExamStatus::Gold: status = "金合格"; color = gfx::Rgb(255, 215, 0); break;
            case ExamStatus::Red: status = "合格"; color = gfx::Rgb(255, 110, 110); break;
            default: status = "不合格"; color = gfx::Rgb(160, 160, 160); break;
            }
            const std::string prefix = e.Song >= 0 ? std::to_string(e.Song + 1) + "曲目 " : "全体 ";
            const std::string line = prefix + ExamConditionText(e.Exam) + "  結果: " + ExamValueText(e.Exam.ExamType, e.Value) + "  " + status;
            txt::Draw(txt::DefaultFont(), (float)(24), (float)(y0 + lineheight * (int)i), line.c_str(), color);
        }
    }

    Skin.Base->Result.Image.Crown.Draw({}, CrownIndex);
    DrawFitScreen(Skin.Base->Result.Font.Title, Skin.Base->Result.Config.TitlePos, Chart.OriginalData.Title);
    DrawFitScreen(Skin.Base->Result.Font.SubTitle, Skin.Base->Result.Config.SubTitlePos, Chart.OriginalData.Subtitle);
    if (const std::string& title = LaneTitle(IsMulti ? (size_t)ResultIndex : 0); !title.empty()) {
        Skin.Base->Result.Font.TitleName.Draw(
            Skin.Base->Result.Config.TitleNamePos,
            gfx::Rgb(255, 255, 255),
            gfx::Rgb(0, 0, 0),
            GetStrlenCached(title, Skin.Base->Result.Font.TitleName.Handle),
            title
        );
    }
    if (!IsMulti) {
        Skin.Base->Result.Font.PlayerName.Draw(
            Skin.Base->Result.Config.PlayerNamePos,
            gfx::Rgb(255, 255, 255),
            gfx::Rgb(0, 0, 0),
            GetStrlenCached(Config.PlayerName, Skin.Base->Result.Font.PlayerName.Handle),
            Config.PlayerName
        );
    }
    else if (IsMulti) {
        Skin.Base->Result.Font.PlayerName.Draw(
            Skin.Base->Result.Config.PlayerNamePos,
            gfx::Rgb(255, 255, 255),
            gfx::Rgb(0, 0, 0),
            GetStrlenCached(Names[ResultIndex], Skin.Base->Result.Font.PlayerName.Handle),
            Names[ResultIndex]
        );
    }

    if (Config.ScoreMeter && !IsMulti && HitStats.LastPlay.Count > 0) {
        auto& font = ConfigSmallFont();
        const float resx = (float)Skin.Info.Resolution.X, resy = (float)Skin.Info.Resolution.Y;
        DrawHitErrorMeter(resx * 0.5f, resy - font.Size * 2.0f - 30.0f, 420.0f, &HitStats.LastPlay, false, &font);
    }

#ifdef __ANDROID__
    if (!IsMulti || IsHost()) {
        Skin.Base->Result.Image.Back.Draw({});
    }
#endif
}

void Game::ResultProc() {
    UpdateFullComboVoice(std::chrono::steady_clock::now());

    auto BackInputProc = [&] {
        if (!IsMulti) {
            NowScene = PrevScene;
        }
        else if (IsHost()) {
            // 全員を待機室へ戻す (RoomState を受け取ると各自が待機室へ移動する)
            Online.Net.Send(fasnet::Msg::ReturnLobby);
        }
        };

#ifndef __ANDROID__
    Input::GetInstance().HitKeyProcess(VK_RETURN, KeyState::Down, BackInputProc);
#else
    Touch.Process(TouchType::Other, BackInputProc, Skin.Base->Result.Image.Back);
#endif

    if (IsMulti && LaneCount() >= 2) {

#ifndef __ANDROID__
        Input::GetInstance().HitKeyProcess(VK_LEFT, KeyState::Down, [&] {
            Skin.Base->SongSelect.SE.Ka.Play();
            ResultIndex = std::max(ResultIndex - 1, 0);
            }, Config.KeyHoldProcInterval);
        Input::GetInstance().HitKeyProcess(VK_RIGHT, KeyState::Down, [&] {
            Skin.Base->SongSelect.SE.Ka.Play();
            ResultIndex = std::clamp(ResultIndex + 1, 0, std::max((int)LaneCount() - 1, 0));
            }, Config.KeyHoldProcInterval);
#else
        Touch.Process(TouchType::LeftKa, [&] {
            Skin.Base->SongSelect.SE.Ka.Play();
            ResultIndex = std::max(ResultIndex - 1, 0);
            });
        Touch.Process(TouchType::RightKa, [&] {
            Skin.Base->SongSelect.SE.Ka.Play();
            ResultIndex = std::clamp(ResultIndex + 1, 0, std::max((int)LaneCount() - 1, 0));
            });
#endif
    }
}
