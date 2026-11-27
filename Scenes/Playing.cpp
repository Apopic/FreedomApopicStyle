#include "../Game.hpp"
#include "../Library/KeyMapLayout.hpp"

double Game::ChartNowTime(uint64_t elapsed, bool FrameCounter, double fastdrawrate, double extendrate) const {
    double ret = 0;
    if (FrameCounter) {
        double hz = RefreshRate * extendrate;
        double frame = ((long long)((Chart.NowTime.GetElapsed().Second() * RefreshRate) * hz) + fastdrawrate) / hz;
        ret = (1000.0 * frame) / (double)RefreshRate;
    }
    else {
        ret = Chart.NowTime.GetElapsed().Second() * elapsed;
    }
    return ret * Chart.SongSpeed;
}

const std::string& Game::LaneTitle(size_t lane) const {
    static const std::string empty;
    if (!IsMulti) { return lane == 0 ? Config.TitleName : empty; }
    return lane < Titles.size() ? Titles[lane] : empty;
}

const char* Game::ExamLabel(ExamTypes type) {
    switch (type) {
    case ExamTypes::Accuracy: return "精度";
    case ExamTypes::Good: return "良";
    case ExamTypes::Ok: return "可";
    case ExamTypes::Bad: return "不可";
    case ExamTypes::Score: return "スコア";
    case ExamTypes::Roll: return "連打";
    case ExamTypes::HitNote: return "叩けた数";
    case ExamTypes::MaxCombo: return "最大コンボ";
    default: return "?";
    }
}

std::string Game::ExamValueText(ExamTypes type, double value) {
    if (type == ExamTypes::Accuracy) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f%%", value);
        return buf;
    }
    return std::to_string((int64_t)value);
}

std::string Game::ExamConditionText(const ExamData& exam) {
    return std::string(ExamLabel(exam.ExamType)) + " " + ExamValueText(exam.ExamType, exam.PassVal[0])
        + (exam.Range == ExamRange::Less ? " 未満" : " 以上")
        + " (金 " + ExamValueText(exam.ExamType, exam.PassVal[1]) + ")";
}

auto Game::DanTotalCounters() const -> DanCounters {
    const JudgeData& j = Chart.Judge[0];
    return { j.Accuracy, j.Good, j.Ok, j.Bad, j.Score, j.Roll, j.HitNote, j.MaxCombo };
}

auto Game::DanSongCounters() const -> DanCounters {
    const JudgeData& j = Chart.Judge[0];
    const JudgeData& b = Dan.SongStart;
    DanCounters c;
    c.Good = j.Good - b.Good;
    c.Ok = j.Ok - b.Ok;
    c.Bad = j.Bad - b.Bad;
    c.Score = j.Score - b.Score;
    c.Roll = j.Roll - b.Roll;
    c.HitNote = j.HitNote - b.HitNote;
    c.MaxCombo = Dan.SongMaxCombo;
    c.Accuracy = c.HitNote != 0 ? (c.Good + c.Ok * 0.5) / (double)c.HitNote * 100.0 : 0.0;
    return c;
}

double Game::ExamCounterValue(ExamTypes type, const DanCounters& c) {
    switch (type) {
    case ExamTypes::Accuracy: return c.Accuracy;
    case ExamTypes::Good: return (double)c.Good;
    case ExamTypes::Ok: return (double)c.Ok;
    case ExamTypes::Bad: return (double)c.Bad;
    case ExamTypes::Score: return (double)c.Score;
    case ExamTypes::Roll: return (double)c.Roll;
    case ExamTypes::HitNote: return (double)c.HitNote;
    case ExamTypes::MaxCombo: return (double)c.MaxCombo;
    default: return 0.0;
    }
}

auto Game::EvaluateExam(const ExamData& exam, double value, bool finished) -> ExamStatus {
    if (exam.Range == ExamRange::Less) {
        if (value >= exam.PassVal[0]) { return ExamStatus::Fail; }
        return value < exam.PassVal[1] ? ExamStatus::Gold : ExamStatus::Red;
    }
    if (value >= exam.PassVal[1]) { return ExamStatus::Gold; }
    if (value >= exam.PassVal[0]) { return ExamStatus::Red; }
    return finished ? ExamStatus::Fail : ExamStatus::NotYet;
}

void Game::UpdateDanExams(bool songfinished) {
    if (!Dan.Active || Chart.Judge.empty()) {
        return;
    }
    // 曲内のコンボ (前の曲から続くコンボは数えない)
    const uint64_t combo = Chart.Judge[0].Combo;
    Dan.SongCombo = combo < Dan.LastCombo ? combo : Dan.SongCombo + (combo - Dan.LastCombo);
    Dan.LastCombo = combo;
    Dan.SongMaxCombo = std::max(Dan.SongMaxCombo, Dan.SongCombo);

    const bool lastsong = Dan.SongIndex + 1 >= Dan.Course.DanSongs.size();
    const DanCounters total = DanTotalCounters();
    const DanCounters song = DanSongCounters();

    for (auto& e : Dan.Exams) {
        if (e.Final || (e.Song >= 0 && (size_t)e.Song != Dan.SongIndex)) {
            continue;
        }
        const bool global = e.Song < 0;
        const bool finished = songfinished && (!global || lastsong);
        e.Value = ExamCounterValue(e.Exam.ExamType, global ? total : song);
        e.Status = EvaluateExam(e.Exam, e.Value, finished);
        if (e.Status == ExamStatus::Fail) {
            e.Final = true;
            if (!Dan.Failed) {
                Dan.Failed = true;
                Skin.Base->Playing.SE.DanFall.Play();
            }
        }
        if (finished) {
            e.Final = true;
        }
    }
}

int Game::DanCrown() const {
    if (Dan.Failed) { return 0; }
    bool gold = true;
    for (const auto& e : Dan.Exams) {
        if (e.Status == ExamStatus::Fail || e.Status == ExamStatus::NotYet) { return 0; }
        gold = gold && e.Status == ExamStatus::Gold;
    }
    return gold ? 3 : 2;
}

void Game::SetDrawBranchArea(Pos2D<float> DelayPos) const {
    if (Chart.IsBranchChart()) {
        SetDrawArea(
            numeric::Cast<int>(Skin.Base->Playing.Image.Lane.Pos.X - Skin.Base->Playing.Image.Lane.Size.Width / 2),
            numeric::Cast<int>(Skin.Base->Playing.Image.Lane.Pos.Y - Skin.Base->Playing.Image.Lane.Size.Height / 2 + DelayPos.Y),
            numeric::Cast<int>(Skin.Base->Playing.Image.Lane.Pos.X + Skin.Base->Playing.Image.Lane.Size.Width / 2),
            numeric::Cast<int>(Skin.Base->Playing.Image.Lane.Pos.Y + Skin.Base->Playing.Image.Lane.Size.Height / 2 + DelayPos.Y));
    }
}

bool Game::HasImage(const GraphData& g) { return !g.Handles.empty() && g.Handles[0] != -1 && g.Size.Width > 0 && g.Size.Height > 0; }



void Game::SkinVoiceReset() {
    SkinVoice = SkinVoiceState();
    PlayerCharAnim = {};
    // Keep the judgement targets before hit processing clears NoteType. In Jiro
    // charts, notes written behind a later judgement time are visual dummies.
    double furthest[3] = {-std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    for (auto& note : Chart.RawNoteDatas) {
        note.FullComboNormal = note.NoteType >= '1' && note.NoteType <= '4';
        const bool target = note.FullComboNormal || (note.NoteType >= '5' && note.NoteType <= '7') || note.NoteType == '9';
        note.FullComboTarget = false;
        note.FullComboHit = false;
        if (!target || note.IsBranch == BranchType::NotDisplay) { continue; }
        for (int lane = 0; lane < 3; ++lane) {
            if (note.IsBranch != BranchType::Null && Chart.JiroLaneOf(note.IsBranch) != lane) { continue; }
            if (!Config.JiroChartMode || note.AbsTime + Config.JudgeBad >= furthest[lane]) { note.FullComboTarget = true; }
            furthest[lane] = std::max(furthest[lane], note.AbsTime);
        }
    }
}

void Game::UpdateFullComboVoice(std::chrono::steady_clock::time_point now) {
    if(Chart.Judge.empty())return;
    auto& pl=Skin.Base->Playing;
    if (SkinVoice.FullComboPlayed || !Chart.NowTime.IsRunning()) { return; }
    bool complete = true, hasNormal = false;
    double end = -std::numeric_limits<double>::infinity();
    for (const auto& note : Chart.RawNoteDatas) {
        if (!note.FullComboTarget || note.IsBranch == BranchType::NotDisplay) { continue; }
        if (note.IsBranch != BranchType::Null && note.IsBranch != Chart.NowBranchFlag) { continue; }
        end = std::max(end, std::max(note.AbsTime, note.RollEndTime));
        if (note.FullComboNormal) { hasNormal = true; complete = complete && note.FullComboHit; }
    }
    complete = complete && hasNormal && ChartNowTime(1000) + Training.Offset >= end;
    if (!complete) { SkinVoice.FullComboAt.reset(); return; }
    if (!SkinVoice.FullComboAt) { SkinVoice.FullComboAt = now; }
    if(!SkinVoice.FullComboPlayed && SkinVoice.FullComboAt && std::chrono::duration<double,std::milli>(now-*SkinVoice.FullComboAt).count()>=pl.Config.FullComboVoiceDelay) {
        pl.SE.FullComboVoice.SetVolume(numeric::Cast<float>(Chart.OriginalData.SeVolume*(Config.SEVolume/100.0)));
        pl.SE.FullComboVoice.Play();SkinVoice.FullComboPlayed=true;
    }
}

void Game::SkinVoiceUpdate() {
    if (Chart.Judge.empty()) return;
    const auto combo = Chart.Judge[0].Combo;
    for (const auto& [at,voice] : Skin.Base->Playing.SE.ComboVoice) {
        if (SkinVoice.LastCombo < at && at <= combo && voice) {
            voice->SetVolume(numeric::Cast<float>(Chart.OriginalData.SeVolume*(Config.SEVolume/100.0)));
            voice->Play();
        }
    }
    UpdateFullComboVoice(std::chrono::steady_clock::now());
    SkinVoice.LastCombo = combo;
}

void Game::DrawFrame(const GraphData& g, Pos2D<float> pos, int index) {
    const_cast<GraphData&>(g).Draw(pos, index);
}

float Game::DrawKeyMap() {
    float top=float(Skin.Info.Resolution.Y);
#ifndef __ANDROID__
    auto& box=Skin.Base->Playing.Image.KeyMapBox;
    if(!Config.KeyMapView||!HasImage(box))return top;
    const auto keys=keymapview::Layout({Config.KaInputLeft,Config.DonInputLeft,Config.DonInputRight,Config.KaInputRight},
        float(Skin.Info.Resolution.X),box.Size.Width,box.Size.Height,Skin.Base->Playing.Config.KeyMapDistance,box.Pos.X,box.Pos.Y);
    const auto& font=ConfigSmallFont();
    const bool acceptsInput=!Config.AutoPlay&&GetForegroundWindow()==GetMainWindowHandle();
    for(const auto& key:keys){
        top=std::min(top,key.Y-box.Size.Height*.5f);
        const bool pressed=acceptsInput&&(GetAsyncKeyState(key.Code)&0x8000)!=0;
        box.Draw({key.X-box.Pos.X,key.Y-box.Pos.Y},pressed&&box.Handles.size()>1?1:0);
        const auto color=pressed?GetColor(255,240,120):GetColor(255,255,255);
        const auto names=Config.GetKeyName({key.Code});
        const auto text=names.empty()?std::to_string(key.Code):names.front();
        const float measured=numeric::Cast<float>(GetStrlenCached(text,font.Handle));
        const float scale=measured>0?std::clamp((box.Size.Width-10)/measured,.01f,1.0f):1;
        if(font.Handle!=-1)DrawExtendStringFToHandle(key.X-measured*scale*.5f,key.Y-font.Size*.5f,scale,1,text.c_str(),color,font.Handle,0);
    }
#endif
    return top;
}
void Game::DrawPlayerChar(double now) {
    if(IsMulti||!Skin.Base)return;
    auto& pl=Skin.Base->Playing;auto& image=pl.Image.PlayerChar;
    if(!HasImage(image))return;
    const auto& cf=pl.Config;
    PlayerCharAnim.Update(now,Chart.NowBPM,Chart.Judge.empty()?0:Chart.Judge[0].Combo,cf.PlayerCharJumpBeats,!Chart.NowGoGo);
    const auto* sequence=&cf.PlayerCharFrames;
    const double accuracy=Chart.Judge.empty()||Chart.AllNoteCount==0?0:Chart.Judge[0].Accuracy*Chart.Judge[0].HitNote/Chart.AllNoteCount;
    if(!Chart.Judge.empty()&&Chart.Judge[0].Combo==0&&Chart.Judge[0].Bad>0&&!cf.PlayerCharMissFrames.empty())sequence=&cf.PlayerCharMissFrames;
    else if(Chart.NowGoGo&&!cf.PlayerCharGoGoFrames.empty())sequence=&cf.PlayerCharGoGoFrames;
    else if(accuracy>=75&&!cf.PlayerCharClearFrames.empty())sequence=&cf.PlayerCharClearFrames;
    const auto count=sequence->empty()?image.Handles.size():sequence->size();
    if(!count||!std::isfinite(now))return;
    const double interval=std::isfinite(cf.PlayerCharFrameTime)?std::max(1.0,cf.PlayerCharFrameTime):100;
    struct Tint {
        int R=255,G=255,B=255;
        explicit Tint(uint32_t color){GetDrawBright(&R,&G,&B);SetDrawBright(R*int((color>>16)&255)/255,G*int((color>>8)&255)/255,B*int(color&255)/255);}
        ~Tint(){SetDrawBright(R,G,B);}
    } tint(cf.PlayerCharColor);
    const double jump=PlayerCharAnim.Beats-PlayerCharAnim.JumpAt;
    if(!cf.PlayerCharJumpFrames.empty()&&std::isfinite(jump)&&cf.PlayerCharJumpBeats>0&&jump>=0&&jump<cf.PlayerCharJumpBeats){
        const auto i=std::min(cf.PlayerCharJumpFrames.size()-1,static_cast<size_t>(jump/cf.PlayerCharJumpBeats*cf.PlayerCharJumpFrames.size()));
        const float lift=std::isfinite(cf.PlayerCharJumpHeight)?cf.PlayerCharJumpHeight*float(std::sin(jump/cf.PlayerCharJumpBeats*3.141592653589793)):0;
        const int frame=cf.PlayerCharJumpFrames[i];
        if(frame>=0&&size_t(frame)<image.Handles.size()){
            image.Draw({0,-lift},frame);
        }
        return;
    }
    const bool missed=sequence==&cf.PlayerCharMissFrames;
    const double loop=!missed&&Chart.NowGoGo&&cf.PlayerCharGoGoLoopBeats>0?cf.PlayerCharGoGoLoopBeats:cf.PlayerCharLoopBeats;
    // 不可時も通常のコマ数を基準に進め、1 コマあたりの表示時間を揃える。
    const auto cadence=missed?(cf.PlayerCharFrames.empty()?image.Handles.size():cf.PlayerCharFrames.size()):count;
    const double phase=std::isfinite(loop)&&loop>0?PlayerCharAnim.Beats/loop*cadence:std::max(0.0,now)/interval;
    if(!std::isfinite(phase))return;
    const auto index=static_cast<size_t>(std::fmod(phase,double(count)));
    image.Draw({},sequence->empty()?numeric::Cast<int>(index):(*sequence)[index]);
}

void Game::DrawSliced(const GraphData& g, Pos2D<float> pos, float slice, int index) {
    if (slice <= 0 || index < 0 || index >= (int)g.Handles.size()) { DrawFrame(g, pos, index); return; }
    int sw = 0, sh = 0;
    GetGraphSize(g.Handles[index], &sw, &sh);
    const float l = g.Pos.X + pos.X - g.Size.Width * 0.5f, t = g.Pos.Y + pos.Y - g.Size.Height * 0.5f;
    const float w = g.Size.Width, h = g.Size.Height;
    const float c = std::min({ slice, w * 0.5f, h * 0.5f, sw * 0.5f, sh * 0.5f });
    const int ci = (int)c;
    const float dx[4] = { l, l + c, l + w - c, l + w }, dy[4] = { t, t + c, t + h - c, t + h };
    const int sx[4] = { 0, ci, sw - ci, sw }, sy[4] = { 0, ci, sh - ci, sh };
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            if (sx[x + 1] <= sx[x] || sy[y + 1] <= sy[y]) { continue; }
            DrawRectExtendGraphF(dx[x], dy[y], dx[x + 1], dy[y + 1], sx[x], sy[y], sx[x + 1] - sx[x], sy[y + 1] - sy[y], g.Handles[index], TRUE);
        }
    }
}













void Game::PlayingInit() {
    PlayingPause = {};
    PlayingPauseNextAllowed = {};
    PlayingBlackout = {};
    SkinVoiceReset();
    BeginPlayStats();
#ifndef __ANDROID__
    RecorderStart();
#endif
    if (IsMulti) {
        // 自分を 0 番レーンに、他のプレイヤーをルームの並び順で続ける
        Online.LaneIds.assign(1, Online.MyId);
        for (const auto& player : Online.Room.Players) {
            if (player.Id != Online.MyId && Online.LaneIds.size() < fasnet::MaxPlayers) {
                Online.LaneIds.push_back(player.Id);
            }
        }
        Names.clear();
        Titles.clear();
        for (uint32_t id : Online.LaneIds) {
            const auto* player = Online.Room.Find(id);
            Names.push_back(player != nullptr ? player->Name : Config.PlayerName);
            Titles.push_back(player != nullptr ? player->Title : Config.TitleName);
        }
        Chart.Judge.resize(std::max(Chart.Judge.size(), LaneCount()));
        HitNote.resize(std::max(HitNote.size(), LaneCount()));
        for (size_t lane = 1; lane < Chart.Judge.size(); ++lane) {
            Chart.Judge[lane] = JudgeData();
        }
    }
    for (auto&& taiko : MiniTaikoFlash) {
        taiko.Reset();
    }
}

void Game::PlayingEnd() {
#ifndef __ANDROID__
    if (Recorder.Active && Recorder.SongStarted) { Recorder.SongUntil = RecorderSeconds(); }
    if (NowScene != Scene::Result) { RecorderStop(); }
#endif
    Chart.SongData.Delete();
    Chart.Movie.Delete();
    if (NowScene == Scene::Result) {
        FinishPlayStats();
    }
    PlayingPause = {};
}

const GraphData& Game::PlayingPauseButton(int index) const {
    const auto& images = Skin.Base->Playing.Image;
    return index == 0 ? images.PauseResume : index == 1 ? images.PauseRestart : images.PauseExit;
}

bool Game::EnterPlayingPause() {
    const auto now = std::chrono::steady_clock::now();
    if (!Config.PauseDisplay || IsMulti || PlayingPause.Active || NowScene != Scene::Playing || now < PlayingPauseNextAllowed) { return false; }
    PlayingPause = {};
    PlayingPause.Active = true;
    PlayingPause.StartedAt = now;
    auto stop = [&](libarrier::Timer& timer) {
        if (timer.IsRunning()) { PlayingPause.Timers.push_back(&timer); timer.Stop(); }
    };
    stop(Chart.NowTime); stop(Chart.BranchAnimationTimer); stop(Chart.WaitRollTime); stop(Chart.RollViewEndTimer);
    stop(Training.BarlineMoveTimer);
    for (auto& timer : MiniTaikoFlash) { stop(timer); }
    PlayingPause.SongWasPlaying = Chart.SongData.IsPlay();
    if (PlayingPause.SongWasPlaying) {
        PlayingPause.SongPosition = GetSoundCurrentTime(Chart.SongData.Handle) - Chart.SongData.StartMilliseconds;
        Chart.SongData.Stop();
    }
    return true;
}

void Game::ResumePlayingPause() {
    if (!PlayingPause.Active) { return; }
    const auto now = std::chrono::steady_clock::now();
    const auto duration = now - PlayingPause.StartedAt;
    for (auto* timer : PlayingPause.Timers) { timer->Start(); }
    if (PlayingPause.SongWasPlaying) {
        Chart.SongData.SetCurrent(PlayingPause.SongPosition);
        Chart.SongData.Play(FALSE);
    }
    if (SkinVoice.FullComboAt) { *SkinVoice.FullComboAt += duration; }
#ifndef __ANDROID__
    if (Recorder.Active) { Recorder.StartedAt += duration; }
    LastHitTick = RawKeyboard::Now();
#endif
    PlayingPause = {};
    PlayingPauseNextAllowed = now + std::chrono::seconds(3);
}

void Game::PlayingPauseSelect(int selection) {
    if (!PlayingPause.Active || PlayingBlackout.Kind != PlayingBlackoutKind::None) { return; }
    selection = (selection % 3 + 3) % 3;
    if (selection == PlayingPause.Selection) { return; }
    PlayingPause.Selection = selection;
    Skin.Base->SongSelect.SE.Ka.SetVolume(Config.SEVolume);
    Skin.Base->SongSelect.SE.Ka.Play();
}

void Game::BeginPlayingBlackout(PlayingBlackoutKind kind) {
    if (kind == PlayingBlackoutKind::None || PlayingBlackout.Kind != PlayingBlackoutKind::None || NowScene != Scene::Playing) { return; }
    PlayingBlackout = {};
    PlayingBlackout.Kind = kind;
    PlayingBlackout.StartedAt = std::chrono::steady_clock::now();
    if (Chart.NowTime.IsRunning()) { Chart.NowTime.Stop(); }
    Chart.SongData.Stop();
}

bool Game::UpdatePlayingBlackout() {
    if (PlayingBlackout.Kind == PlayingBlackoutKind::None) { return false; }
    // A completely black frame must actually be drawn before loading begins.
    if (!PlayingBlackout.FullBlackAt || std::chrono::steady_clock::now() - *PlayingBlackout.FullBlackAt < std::chrono::milliseconds(100)) { return true; }
    const auto kind = PlayingBlackout.Kind;
    PlayingBlackout.Kind = PlayingBlackoutKind::None;
    PlayingBlackout.HideLoading = true;
    PlayingPause = {};
    PlayingPauseNextAllowed = {};
    if (kind == PlayingBlackoutKind::Retry) {
        RecorderAbort();
        Training.Init();
        Dan.Reset();
        Chart.Init(true);
    }
    else { ++Dan.SongIndex; }
    NowScene = Scene::Loading;
    return true;
}

void Game::DrawPlayingBlackout() {
    if (PlayingBlackout.Kind == PlayingBlackoutKind::None) { return; }
    const float opacity = std::clamp(std::chrono::duration<float>(std::chrono::steady_clock::now() - PlayingBlackout.StartedAt).count() / 0.25f, 0.0f, 1.0f);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(opacity * 255));
    DrawFillBox(0, 0, Skin.Info.Resolution.X, Skin.Info.Resolution.Y, GetColor(0, 0, 0));
    SetDrawBlendMode(0, 0);
    if (opacity >= 1 && !PlayingBlackout.FullBlackAt) { PlayingBlackout.FullBlackAt = std::chrono::steady_clock::now(); }
}

void Game::PlayingPauseAction(int action) {
    if (!PlayingPause.Active || PlayingBlackout.Kind != PlayingBlackoutKind::None || action < 0 || action > 2) { return; }
    Skin.Base->SongSelect.SE.Don.SetVolume(Config.SEVolume);
    Skin.Base->SongSelect.SE.Don.Play();
    if (action == 0) { ResumePlayingPause(); return; }
    if (action == 1) { BeginPlayingBlackout(PlayingBlackoutKind::Retry); return; }
    RecorderAbort();
    PlayingPause = {};
    PlayingPauseNextAllowed = {};
    Training.Init();
    Dan.Reset();
    Chart.Init(true);
    NowScene = PrevScene;
}

bool Game::PlayingPauseProc() {
    if (IsMulti) {
        if (PlayingPause.Active) { ResumePlayingPause(); }
        return false;
    }
    if (PlayingPause.Active) {
#ifndef __ANDROID__
        auto& input = Input::GetInstance();
        input.HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] { PlayingPauseAction(0); });
        input.HitKeyProcess(VK_TAB, KeyState::Down, [&] { PlayingPauseAction(0); });
        input.HitKeyProcess(VK_UP, KeyState::Down, [&] { PlayingPauseSelect(PlayingPause.Selection - 1); }, 0.15);
        input.HitKeyProcess(VK_DOWN, KeyState::Down, [&] { PlayingPauseSelect(PlayingPause.Selection + 1); }, 0.15);
        input.HitKeyProcess(VK_RETURN, KeyState::Down, [&] { PlayingPauseAction(PlayingPause.Selection); });
        auto consume = [&](const std::vector<int>& keys) {
            for (int key : keys) {
                if (key != VK_ESCAPE && key != VK_TAB && key != VK_UP && key != VK_DOWN && key != VK_RETURN) {
                    input.HitKeyProcess(key, KeyState::Down, [] {});
                }
            }
        };
        consume(Config.DonInputLeft); consume(Config.DonInputRight); consume(Config.KaInputLeft); consume(Config.KaInputRight);
        LastHitTick = RawKeyboard::Now();
#endif
        if (PlayingPause.Active) {
            const auto pointer = ReadPointer();
            for (int i = 0; i < 3; ++i) {
                const auto& image = PlayingPauseButton(i);
                const UIRect r{ image.Pos.X - image.Size.Width * 0.5f, image.Pos.Y - image.Size.Height * 0.5f,
                    image.Pos.X + image.Size.Width * 0.5f, image.Pos.Y + image.Size.Height * 0.5f };
                if (r.Contains(pointer.X, pointer.Y)) {
                    if (pointer.Moved || pointer.LeftPressed) { PlayingPauseSelect(i); }
                    if (pointer.LeftPressed) { PlayingPauseAction(i); break; }
                }
            }
        }
        return true;
    }
    if (!Config.PauseDisplay) { return false; }
#ifndef __ANDROID__
    Input::GetInstance().HitKeyProcess(VK_TAB, KeyState::Down, [&] { EnterPlayingPause(); });
    Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] { EnterPlayingPause(); });
#else
    Touch.Process(TouchType::Other, [&] { EnterPlayingPause(); }, Skin.Base->Playing.Image.Pause);
#endif
    return PlayingPause.Active;
}

void Game::PlayingPauseDraw() {
    if (!PlayingPause.Active || IsMulti) { return; }
    auto& images = Skin.Base->Playing.Image;
    const float opacity = std::clamp(std::chrono::duration<float>(std::chrono::steady_clock::now() - PlayingPause.StartedAt).count() / 0.18f, 0.0f, 1.0f);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(opacity * 255));
    images.PauseShade.Draw({});
    images.PausePanel.Draw({});
    images.PauseResume.Draw({}); images.PauseRestart.Draw({}); images.PauseExit.Draw({});
    const auto& button = PlayingPauseButton(PlayingPause.Selection);
    images.PauseCursor.Draw({button.Pos.X - images.PauseResume.Pos.X, button.Pos.Y - images.PauseResume.Pos.Y});
    SetDrawBlendMode(0, 0);
}

void Game::PlayingDraw() {

    const double NowTime = ChartNowTime(1000, Config.FrameCountTimer, Config.FastDrawRate, Config.FrameExtendRate) + Training.Offset;
    const double ScrollNow = Chart.ScrollType == ScrollType::Normal ? NowTime : Chart.FrozenTime(NowTime);
    Chart.NowBPM = Chart.BpmAt(NowTime);

    Skin.Base->Playing.Image.BackGround.Draw({});
#ifndef __ANDROID__
    if (Chart.Movie.IsLoaded()) {
        // 譜面の進行に再生位置を合わせる (一時停止・巻き戻し・ずれは Sync がシークで補正する)
        // MOVIEOFFSET は従来通り譜面の時刻 (0 = 演奏開始) を基準に、正なら遅らせて負なら先に進める
        Chart.Movie.Sync(NowTime - Chart.OriginalData.MovieOffset * 1000.0, Chart.NowTime.IsRunning());

        if (Config.BGBrightness > 0) {
            Chart.Movie.Draw(0.0f, 0.0f, (float)Skin.Info.Resolution.X, (float)Skin.Info.Resolution.Y);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(255 * (1 - (Config.BGBrightness / 100))));
            DrawFillBox(0, 0, Skin.Info.Resolution.X, Skin.Info.Resolution.Y, GetColor(0, 0, 0));
            SetDrawBlendMode(0, 0);
        }
    }
#endif
    // ローカルプレイヤーのコンボボイスを更新する。
    if (!IsMulti && !PlayingPause.Active) {
        SkinVoiceUpdate();

    }

    const auto& playing = Skin.Base->Playing;
    float laneDistance = playing.Config.MultiPlayLaneDistance;
    float laneOffset = 0.0f;
    if (IsMulti && LaneCount() == 4) {
        // 1P のゲージ等を画面上端からわずかに離し、4レーン全体をできるだけ上へ寄せる。
        const auto& image = playing.Image;
        const float margin = std::max(1.0f, std::round(Skin.Info.Resolution.Y / 240.0f));
        const float bottomMargin = std::max(1.0f, std::round(Skin.Info.Resolution.Y / 360.0f));
        float top = std::numeric_limits<float>::infinity();
        float bottom = -std::numeric_limits<float>::infinity();
        for (const GraphData* g : { &image.ProgressBar, &image.Lane, &image.LaneFrame,
                &image.Base, &image.NamePlate, &image.MiniTaiko, &image.Note }) {
            if (!HasImage(*g)) { continue; }
            top = std::min(top, g->Pos.Y - g->Size.Height * 0.5f);
            bottom = std::max(bottom, g->Pos.Y + g->Size.Height * 0.5f);
        }
        const float minOffset = std::isfinite(top) ? std::ceil(margin - top) : -laneDistance;
        const float bottomLimit = Skin.Info.Resolution.Y - bottomMargin - bottom;
        if (laneDistance > 0.0f && std::isfinite(bottom)) {
            // 下端がはみ出す場合だけ、レーン本体が重ならない範囲で等間隔に詰める。
            float minDistance = 1.0f;
            for (const GraphData* g : { &image.Lane, &image.LaneFrame }) {
                if (HasImage(*g)) { minDistance = std::max(minDistance, g->Size.Height); }
            }
            minDistance = std::min(minDistance, laneDistance);
            const float fitDistance = std::floor((bottomLimit - minOffset) / 3.0f);
            laneDistance = std::clamp(fitDistance, minDistance, laneDistance);
        }
        // 下方向への余分な補正を行わず、1P上端を安全余白に合わせる。
        laneOffset = minOffset;
    }

    int idx = 0;

    do {

        Pos2D<float> DelayPos = { 0.0f, idx * laneDistance + laneOffset };

#ifndef __ANDROID__
        if (Chart.Movie.Handle != -1) {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 225);
        }
        Skin.Base->Playing.Image.LaneFrame.Draw(DelayPos);
        Skin.Base->Playing.Image.Lane.Draw(DelayPos);
        SetDrawBlendMode(0, 0);
#else
        SetDrawBlendMode(0, 0);
        Skin.Base->Playing.Image.LaneFrame.Draw(DelayPos);
        Skin.Base->Playing.Image.Lane.Draw(DelayPos);
#endif

        SetDrawBranchArea(DelayPos);

        switch (Config.JiroChartMode && Chart.NowBranchFlag != BranchType::Null ? Chart.JiroBranchOf(Chart.JiroShown - 1) : Chart.NowBranchFlag) {
        case BranchType::Normal:
            Skin.Base->Playing.Image.NormalLane.Draw(DelayPos);
            break;
        case BranchType::Expert:
            Skin.Base->Playing.Image.ExpertLane.Draw(DelayPos);
            break;
        case BranchType::Master:
            Skin.Base->Playing.Image.MasterLane.Draw(DelayPos);
            break;
        }

        double NoteBranchMotion = 0.0;

        if (Chart.NowBranchAnimation != ABranchType::Null) {
            double _one = Chart.BranchAnimationTimer.GetElapsed().Second() / Chart.BranchAnimationTime;
            float _motiony = numeric::Cast<float>(Skin.Base->Playing.Image.Lane.Size.Height * GetEasingRate(_one, ease::Base::Out, ease::Line::Sine));
            NoteBranchMotion = _motiony;
            if (_one > 1.0) {
                Chart.NowBranchAnimation = ABranchType::Null;
                Chart.BranchAnimationTimer.Reset();
            }
            switch (Chart.NowBranchAnimation) {
            case ABranchType::Normal_Expert:
                Skin.Base->Playing.Image.NormalLane.Draw({ 0, _motiony + DelayPos.Y });
                Skin.Base->Playing.Image.ExpertLane.Draw({ 0, Skin.Base->Playing.Image.Lane.Size.Height + _motiony + DelayPos.Y });
                break;
            case ABranchType::Normal_Master:
                Skin.Base->Playing.Image.NormalLane.Draw({ 0, _motiony + DelayPos.Y });
                Skin.Base->Playing.Image.MasterLane.Draw({ 0, Skin.Base->Playing.Image.Lane.Size.Height + _motiony + DelayPos.Y });
                break;
            case ABranchType::Expert_Normal:
                Skin.Base->Playing.Image.ExpertLane.Draw({ 0, _motiony + DelayPos.Y });
                Skin.Base->Playing.Image.NormalLane.Draw({ 0, Skin.Base->Playing.Image.Lane.Size.Height + _motiony + DelayPos.Y });
                break;
            case ABranchType::Expert_Master:
                Skin.Base->Playing.Image.ExpertLane.Draw({ 0, _motiony + DelayPos.Y });
                Skin.Base->Playing.Image.MasterLane.Draw({ 0, Skin.Base->Playing.Image.Lane.Size.Height + _motiony + DelayPos.Y });
                break;
            case ABranchType::Master_Normal:
                Skin.Base->Playing.Image.MasterLane.Draw({ 0, _motiony + DelayPos.Y });
                Skin.Base->Playing.Image.NormalLane.Draw({ 0, Skin.Base->Playing.Image.Lane.Size.Height + _motiony + DelayPos.Y });
                break;
            case ABranchType::Master_Expert:
                Skin.Base->Playing.Image.MasterLane.Draw({ 0, _motiony + DelayPos.Y });
                Skin.Base->Playing.Image.ExpertLane.Draw({ 0, Skin.Base->Playing.Image.Lane.Size.Height + _motiony + DelayPos.Y });
                break;
            }
        }
        SetDrawAreaFull();
        SetDrawBlendMode(0, 0);

        Skin.Base->Playing.Image.ProgressBar.Draw(DelayPos, 0);
        if (Chart.Judge[idx].HitNote > 0) {

            double Ratio = ((double)Chart.Judge[idx].Good + (double)Chart.Judge[idx].Ok * 0.5) / Chart.AllNoteCount;
            float Width = numeric::Cast<float>(Skin.Base->Playing.Image.ProgressBar.Size.Width * Ratio);
            float MaxWidth = Skin.Base->Playing.Image.ProgressBar.Size.Width;

            Skin.Base->Playing.Image.ProgressBar.RectDraw(
                DelayPos,
                { 0, Skin.Base->Playing.Image.ProgressBar.Size.Height },
                { Width < MaxWidth ? Width : MaxWidth,
                  Skin.Base->Playing.Image.ProgressBar.Size.Height },
                1
            );
        }

        Skin.Base->Playing.Image.Note.Draw(DelayPos); // 判定枠
        if(Chart.NowGoGo) Skin.Base->Playing.Image.GoGoFire.Draw(DelayPos,(size_t)(NowTime/std::max(1.0,Skin.Base->Playing.Config.GoGoFireFrameTime))%std::max(1,Skin.Base->Playing.Image.GoGoFire.Div.X*Skin.Base->Playing.Image.GoGoFire.Div.Y));
        // 上の爆発 (スキンにある時。コマの並びは下の爆発と同じ: 段 = 良・可 × 小・大)
        auto DrawUpperExplosion = [&](const HitNoteData& data) {
            const auto& up = Skin.Base->Playing.Image.JudgeUpperExplosion;
            if (!HasImage(up) || up.Div.Y < 2) { return; }
            size_t ui = numeric::Cast<size_t>(data.MoveElapsedTime / std::max(1.0,Skin.Base->Playing.Config.JudgeUpperExplosionFrameTime));
            if (ui >= (size_t)up.Div.X) { return; }
            ui += (size_t)(up.Div.Y >= 4 ? 2 * up.Div.X * data.JudgeUnderExplosion.Big : 0);
            if (data.JudgeUnderExplosion.Type == JudgeType::Ok) { ui += up.Div.X; }
            const_cast<GraphData&>(up).Draw(DelayPos, (int)ui);
            };
        auto DrawJudgeString = [&](HitNoteData& data) {
            if (!data.JudgeString.IsActive || data.MoveElapsedTime >= data.JudgeString.MoveTime()) { return; }
            const double alpha = 255 * (1 - GetEasingRate(data.MoveElapsedTime / data.JudgeString.MoveTime(), ease::Base::In, ease::Line::Cubic));
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)alpha);
            Skin.Base->Playing.Image.JudgeString.Draw(DelayPos, (size_t)data.JudgeString.Type);
            SetDrawBlendMode(0, 0);
            };

        {

            size_t i = HitNote[idx].Index;
            const double JudgeUnderExplosionTime = Skin.Base->Playing.Config.JudgeUpperExplosionFrameTime * Skin.Base->Playing.Image.JudgeUnderExplosion.Div.X;

            for (size_t c = 0; c < HitNote[idx].Size(); ++c) {
                auto&& data = HitNote[idx].Datas[i];
                if (!data.MoveTimer.IsRunning()) {
                    data.MoveTimer.Start();
                }
                data.MoveElapsedTime = data.MoveTimer.GetElapsed().MilliSecond();
                if (data.JudgeUnderExplosion.IsActive && data.MoveElapsedTime < JudgeUnderExplosionTime) {
                    size_t drawindex = numeric::Cast<size_t>(data.MoveElapsedTime / std::max(1.0,Skin.Base->Playing.Config.JudgeUpperExplosionFrameTime));
                    drawindex += (2 * Skin.Base->Playing.Image.JudgeUnderExplosion.Div.X) * data.JudgeUnderExplosion.Big;
                    if (data.JudgeUnderExplosion.Type == JudgeType::Ok) {
                        drawindex += Skin.Base->Playing.Image.JudgeUnderExplosion.Div.X;
                    }
                    Skin.Base->Playing.Image.JudgeUnderExplosion.Draw(DelayPos, numeric::Cast<int>(drawindex));
                    DrawUpperExplosion(data);
                }
                else {
                    data.JudgeUnderExplosion.IsActive = false;
                }

                ++i;
                if (!(i < HitNote[idx].Size())) {
                    i = 0;
                }
            }
        }

        auto NoteAlpha = [&](double _one, AlphaType Type) {
            int alpha = 255;
            _one = std::clamp(_one, 0.0, 1.0);
            switch (Type) {
            case AlphaType::Hidden:
                alpha = numeric::Cast<int>(255 * _one);
                break;
            case AlphaType::Sudden:
                alpha = numeric::Cast<int>(alpha * (1 - _one));
                break;
            }
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, numeric::Cast<int>(alpha));
            };

        const Pos2D<double>& NoteOrigin = {
                Skin.Base->Playing.Image.Note.Pos.X,
                Skin.Base->Playing.Image.Note.Pos.Y + DelayPos.Y
        };

        // 次郎1 と同じ表示位置:
        //   通常      : (音符の時刻 - 現在時刻) × 音符の BPM × SCROLL
        //   BM/HB     : (音符の拍位置 - 現在の拍位置) × SCROLL。拍は BPM 区間ごとに積分し、#DELAY の間は止まる
        //               BMSCROLL は #SCROLL を無視する (ハイスピードは有効)
        const auto scrollframe = Chart.MakeScrollFrame(ScrollNow);      // 今の位置はフレームに 1 回だけ求める
        auto GetNotePos = [&](NoteData& data)->Pos2D<double> {
            const bool BMScroll = ScrollType::BMSCROLL == Chart.ScrollType;
            double distance = 0;
            double scroll = data.Scroll;
            if (ScrollType::Normal == Chart.ScrollType) {
                if (Config.JiroChartMode) {
                    // 次郎と同じく、音符の時刻で BPM と SCROLL を引く (書いた位置の値ではない。ダミーノーツ・疑似停止の見え方)
                    const BranchType branch = data.IsBranch == BranchType::Null || data.IsBranch == BranchType::NotDisplay ? Chart.NowBranchFlag : data.IsBranch;
                    const auto& cache = Chart.NoteScrollOf(data);
                    distance = (data.AbsTime - NowTime) / (240 / cache.JiroBpm);
                    scroll = cache.JiroScroll[PlayData::JiroLaneOf(branch)] * Config.ChartSpeed;
                }
                else {
                    distance = (data.AbsTime - NowTime) / (240 / data.BPM);
                }
            }
            else {
                distance = Chart.ScrollDistanceFast(data, scrollframe) * 1000.0;     // 通常と同じ単位 (ms × BPM / 240)
                if (Config.JiroChartMode && !BMScroll) {
                    const BranchType branch = data.IsBranch == BranchType::Null || data.IsBranch == BranchType::NotDisplay ? Chart.NowBranchFlag : data.IsBranch;
                    scroll = Chart.NoteScrollOf(data).JiroScroll[PlayData::JiroLaneOf(branch)] * Config.ChartSpeed;
                }
            }

            Pos2D<double> _ret = {
                    distance * Skin.Base->Playing.Config.LaneExtendRate * (BMScroll ? Config.ChartSpeed : scroll),
                    distance * Skin.Base->Playing.Config.LaneExtendRate * (BMScroll ? 0 : data.Scrolli)
            };
            // 次郎形式: 位置は次郎と同じ 32 bit の整数 (区間ごとに切り捨てて足し、あふれたら回り込む)。
            // 次郎の画面 (512 幅) の 1 小節の px (Notedist=2 の 420px。Ver.2.92 で測った値) で計算し、FAS のレーンの長さに伸ばす
            // (あふれた後の位置は 1 小節の px で変わるので、次郎と同じ px で計算しないと同じ見え方にならない)
            if (Config.JiroChartMode && !BMScroll) {
                const double jirounit = PlayData::JiroMeasurePx;
                const double pxscale = 1000.0 * Skin.Base->Playing.Config.LaneExtendRate / jirounit;
                _ret.X = Chart.JiroScrollPx(data, scrollframe, jirounit, scroll, ScrollType::Normal != Chart.ScrollType) * pxscale;
            }
            else if (Config.JiroChartMode) {
                const double unit = 1000.0 * Skin.Base->Playing.Config.LaneExtendRate * Config.ChartSpeed;
                _ret.X = Chart.ScrollPxFast(data, scrollframe, unit);
            }

            Pos2D<float> SkinPos = {
                    Skin.Base->Playing.Image.Lane.Size.Width,
                    Skin.Base->Playing.Image.Lane.Size.Height
            };

            _ret = {
                    _ret.X + NoteOrigin.X,
                    _ret.Y + NoteOrigin.Y
            };

            return _ret;

            };

#define InRange(x, y) (x > Skin.SimulationRect.Left && x < Skin.SimulationRect.Right && y > Skin.SimulationRect.Top && y < Skin.SimulationRect.Bottom)

        std::complex<double> n1{};
        std::complex<double> n2{};
        std::complex<double> n3{};
        std::complex<double> n4{};
        std::complex<double> facing{};
        Pos2D<double> NotePos{};

        const std::complex<double> n0 = { Skin.Base->Playing.Image.Note.Size.Width / 2, Skin.Base->Playing.Image.Note.Size.Height / 2 };
        const double n0r = std::abs(n0);
        const double narr[4] = {
                std::arg(std::complex<double>{ n0.real() * -1, n0.imag() * -1 }),
                std::arg(std::complex<double>{ n0.real() * 1, n0.imag() * -1 }),
                std::arg(std::complex<double>{ n0.real() * 1, n0.imag() * 1 }),
                std::arg(std::complex<double>{ n0.real() * -1, n0.imag() * 1 })
        };

        SetDrawBranchArea(DelayPos);

        // 描画する音符: 通常のスクロールでは、終わった先頭部分 (ScanStart より前) は画面の左へ抜けているので飛ばす
        // (次郎形式・BM/HBSCROLL は逆走などで戻ってくることがあるので全部見る)
        const bool simplescroll = !Config.JiroChartMode && Chart.ScrollType == ScrollType::Normal;
        const std::span<NoteData> drawnotes = simplescroll ? Chart.ActiveNotes() : std::span<NoteData>(Chart.RawNoteDatas);
        // 画面の外の判定に使う余白 (大音符の大きさ + 分岐のレーンの高さ)
        const float cullmargin = Skin.Base->Playing.Image.Note.Size.Width + Skin.Base->Playing.Image.Lane.Size.Height * 2.0f;
        // osu! 譜面では先に全小節線を描く。通常 TJA の重ね順は維持する。
        for (int pass = 0; pass < (Chart.IsOsuChart ? 2 : 1); ++pass)
        for (auto&& data : drawnotes | std::ranges::views::reverse) {
            const bool barlinesOnly = Chart.IsOsuChart && pass == 0;
            if (Chart.IsOsuChart && (barlinesOnly ? !data.BarlineDisplay : data.NoteType == '0')) { continue; }

            // 音符でも小節線でもない (空白・命令だけの位置) は描かない
            if (data.NoteType == '0' && !data.BarlineDisplay) { continue; }

            double BranchDelayPos = 0.0;

            NotePos = GetNotePos(data);
            // 連打以外で画面から大きく外れている音符は、分岐の判定・アニメーションの計算をせずに飛ばす
            if (!(data.NoteType >= '5' && data.NoteType <= '9')
                && (NotePos.X < Skin.SimulationRect.Left - cullmargin || NotePos.X > Skin.SimulationRect.Right + cullmargin
                    || NotePos.Y < Skin.SimulationRect.Top - cullmargin || NotePos.Y > Skin.SimulationRect.Bottom + cullmargin)) {
                continue;
            }
            const double NoteTheta = atan2(data.Scrolli, data.Scroll);

            bool NoteBranch = data.IsBranch == Chart.NowBranchFlag || data.IsBranch == BranchType::Null;
            // 次郎形式: 次の分岐の区間の音符は、決まった分岐 (JiroNext) のものを先に表示する
            if (Config.JiroChartMode && data.IsBranch != BranchType::Null && data.IsBranch != BranchType::NotDisplay) {
                NoteBranch = data.AbsTime >= Chart.JiroUpcomingStart ? data.IsBranch == Chart.JiroNext : data.IsBranch == Chart.NowBranchFlag;
            }

            switch (Chart.NowBranchAnimation) {
            case ABranchType::Normal_Expert:
                if (data.IsBranch == BranchType::Normal) { NoteBranch = true; BranchDelayPos += NoteBranchMotion; }
                if (data.IsBranch == BranchType::Expert) { NoteBranch = true; BranchDelayPos -= Skin.Base->Playing.Image.Lane.Size.Height - NoteBranchMotion; }
                break;
            case ABranchType::Normal_Master:
                if (data.IsBranch == BranchType::Normal) { NoteBranch = true; BranchDelayPos += NoteBranchMotion; }
                if (data.IsBranch == BranchType::Master) { NoteBranch = true; BranchDelayPos -= Skin.Base->Playing.Image.Lane.Size.Height - NoteBranchMotion; }
                break;
            case ABranchType::Expert_Normal:
                if (data.IsBranch == BranchType::Expert) { NoteBranch = true; BranchDelayPos -= NoteBranchMotion; }
                if (data.IsBranch == BranchType::Normal) { NoteBranch = true; BranchDelayPos += Skin.Base->Playing.Image.Lane.Size.Height - NoteBranchMotion; }
                break;
            case ABranchType::Expert_Master:
                if (data.IsBranch == BranchType::Expert) { NoteBranch = true; BranchDelayPos += NoteBranchMotion; }
                if (data.IsBranch == BranchType::Master) { NoteBranch = true; BranchDelayPos -= Skin.Base->Playing.Image.Lane.Size.Height - NoteBranchMotion; }
                break;
            case ABranchType::Master_Normal:
                if (data.IsBranch == BranchType::Master) { NoteBranch = true; BranchDelayPos -= NoteBranchMotion; }
                if (data.IsBranch == BranchType::Normal) { NoteBranch = true; BranchDelayPos += Skin.Base->Playing.Image.Lane.Size.Height - NoteBranchMotion; }
                break;
            case ABranchType::Master_Expert:
                if (data.IsBranch == BranchType::Master) { NoteBranch = true; BranchDelayPos -= NoteBranchMotion; }
                if (data.IsBranch == BranchType::Expert) { NoteBranch = true; BranchDelayPos += Skin.Base->Playing.Image.Lane.Size.Height - NoteBranchMotion; }
                break;
            }

            NotePos.Y += BranchDelayPos;

            if (!NoteBranch) { continue; }
            if (data.BarlineDisplay && (!Chart.IsOsuChart || barlinesOnly)) {
                SetDrawBlendMode(0, 0);
                if (InRange(NotePos.X, NotePos.Y)) {
                    DrawLineAA(
                        numeric::Cast<float>(NotePos.X),
                        numeric::Cast<float>(NotePos.Y - 65),
                        numeric::Cast<float>(NotePos.X),
                        numeric::Cast<float>(NotePos.Y + 65),
                        GetColor(255, 255, 255 * !data.Section)
                    );
                }
            }

            if (barlinesOnly || data.NoteType == '0') {
                continue;
            }


            if (data.NoteType >= '1' &&
                data.NoteType <= '4') {
                if (InRange(NotePos.X, NotePos.Y)) {
                    double hidden = Config.HiddenLevel;
                    double sudden = Config.SuddenLevel;
                    bool hiddenflag = hidden > 0.0;
                    bool suddenflag = sudden > 0.0;
                    bool multiflag = (IsMulti && idx >= 1);
                    if (hiddenflag || suddenflag || multiflag) {
                        double _abs = std::abs(std::complex<double>{ NotePos.X - NoteOrigin.X, NotePos.Y - NoteOrigin.Y })* (data.AbsTime < NowTime ? -1 : 1);
                        double leveling = (Skin.Base->Playing.Image.Lane.Size.Width / DX_PI);
                        double feedrange = (Skin.Base->Playing.Image.Lane.Size.Width / DX_TWO_PI);
                        if (multiflag) {
                            NoteAlpha((_abs - (leveling * 0.025)) / feedrange, AlphaType::Hidden);
                        }
                        if (hiddenflag) {
                            NoteAlpha((_abs - (leveling * hidden)) / feedrange, AlphaType::Hidden);
                        }
                        if (suddenflag) {
                            NoteAlpha(((_abs - (Skin.Base->Playing.Image.Lane.Size.Width)) + (leveling * sudden)) / feedrange, AlphaType::Sudden);
                        }
                    }
                    Skin.Base->Playing.Image.Note.Draw(
                        {
                                (float)(NotePos.X - NoteOrigin.X),
                                (float)(NotePos.Y - NoteOrigin.Y) + DelayPos.Y
                        },
                        data.NoteType - 48
                    );
                    // Note alpha must not leak into the next note or lane UI.
                    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                }
                continue;
            }
            SetDrawBlendMode(0, 0);

            if (data.NoteType >= '5' &&
                data.NoteType <= '6') {
                const Pos2D<double>& cnote = NotePos;
                Pos2D<double> dnote = GetNotePos(Chart.RawNoteDatas[data.RollEndIndex]);
                dnote.Y += BranchDelayPos;

                bool DispFlag =
                    InRange(cnote.X, cnote.Y) ||
                    (data.AbsTime < NowTime && data.RollEndTime > NowTime) ||
                    InRange(dnote.X, dnote.Y);

                if (DispFlag) {

                    bool BigRollFlag = data.NoteType == '6';

                    const std::complex<double>& cdnote = { dnote.X - cnote.X, dnote.Y - cnote.Y };

                    double RollTheta = std::arg(cdnote);
                    facing = std::polar(n0.real() - 3, RollTheta);

                    n1 = std::polar(n0r, narr[0] + RollTheta);
                    n2 = std::polar(n0r, narr[1] + RollTheta);
                    n3 = std::polar(n0r, narr[2] + RollTheta);
                    n4 = std::polar(n0r, narr[3] + RollTheta);

                    DrawModiGraphF(
                        numeric::Cast<float>(cnote.X + facing.real() + n1.real()),
                        numeric::Cast<float>(cnote.Y + facing.imag() + n1.imag()),
                        numeric::Cast<float>(dnote.X - facing.real() + n2.real()),
                        numeric::Cast<float>(dnote.Y - facing.imag() + n2.imag()),
                        numeric::Cast<float>(dnote.X - facing.real() + n3.real()),
                        numeric::Cast<float>(dnote.Y - facing.imag() + n3.imag()),
                        numeric::Cast<float>(cnote.X + facing.real() + n4.real()),
                        numeric::Cast<float>(cnote.Y + facing.imag() + n4.imag()),
                        Skin.Base->Playing.Image.Note.Handles[BigRollFlag ? 9 : 6],
                        TRUE
                    );
                    DrawModiGraphF(
                        numeric::Cast<float>(dnote.X + n1.real()),
                        numeric::Cast<float>(dnote.Y + n1.imag()),
                        numeric::Cast<float>(dnote.X + n2.real()),
                        numeric::Cast<float>(dnote.Y + n2.imag()),
                        numeric::Cast<float>(dnote.X + n3.real()),
                        numeric::Cast<float>(dnote.Y + n3.imag()),
                        numeric::Cast<float>(dnote.X + n4.real()),
                        numeric::Cast<float>(dnote.Y + n4.imag()),
                        Skin.Base->Playing.Image.Note.Handles[BigRollFlag ? 10 : 7],
                        TRUE
                    );
                    DrawModiGraphF(
                        numeric::Cast<float>(cnote.X + n1.real()),
                        numeric::Cast<float>(cnote.Y + n1.imag()),
                        numeric::Cast<float>(cnote.X + n2.real()),
                        numeric::Cast<float>(cnote.Y + n2.imag()),
                        numeric::Cast<float>(cnote.X + n3.real()),
                        numeric::Cast<float>(cnote.Y + n3.imag()),
                        numeric::Cast<float>(cnote.X + n4.real()),
                        numeric::Cast<float>(cnote.Y + n4.imag()),
                        Skin.Base->Playing.Image.Note.Handles[BigRollFlag ? 8 : 5],
                        TRUE
                    );

                }
            }

            if (data.NoteType == '7' ||
                data.NoteType == '9') {

                if (data.BalloonFlag == 1) {
                    NotePos = NoteOrigin;
                }
                if (data.BalloonFlag == 2) {
                    NotePos = GetNotePos(Chart.RawNoteDatas[data.RollEndIndex]);
                }
                // 次郎形式: 風船・くす玉の頭は判定枠より左に来たら、終わり (8) が判定枠より右にある間は判定枠に止めて描く
                // (次郎の clampRollHead。時刻ではなく位置で決まるので、位置があふれた先のくす玉が判定枠に残って見えることもある)
                else if (data.BalloonFlag == 0 && Config.JiroChartMode && data.RollEndIndex < Chart.RawNoteDatas.size()) {
                    const Pos2D<double> endpos = GetNotePos(Chart.RawNoteDatas[data.RollEndIndex]);
                    if (NoteOrigin.X < endpos.X) { NotePos.X = std::max(NotePos.X, (double)NoteOrigin.X); }
                    else { NotePos.X = endpos.X; }
                }

                if (InRange(NotePos.X, NotePos.Y)) {

                    bool KusudamaFlag = data.NoteType == '9';

                    facing = std::polar(n0.real() * 2, NoteTheta);

                    n1 = std::polar(n0r, narr[0] + NoteTheta);
                    n2 = std::polar(n0r, narr[1] + NoteTheta);
                    n3 = std::polar(n0r, narr[2] + NoteTheta);
                    n4 = std::polar(n0r, narr[3] + NoteTheta);

                    DrawModiGraphF(
                        numeric::Cast<float>(NotePos.X + n1.real()),
                        numeric::Cast<float>(NotePos.Y + n1.imag()),
                        numeric::Cast<float>(NotePos.X + n2.real()),
                        numeric::Cast<float>(NotePos.Y + n2.imag()),
                        numeric::Cast<float>(NotePos.X + n3.real()),
                        numeric::Cast<float>(NotePos.Y + n3.imag()),
                        numeric::Cast<float>(NotePos.X + n4.real()),
                        numeric::Cast<float>(NotePos.Y + n4.imag()),
                        Skin.Base->Playing.Image.Note.Handles[KusudamaFlag ? 13 : 11],
                        TRUE
                    );
                    DrawModiGraphF(
                        numeric::Cast<float>(NotePos.X + facing.real() + n1.real()),
                        numeric::Cast<float>(NotePos.Y + facing.imag() + n1.imag()),
                        numeric::Cast<float>(NotePos.X + facing.real() + n2.real()),
                        numeric::Cast<float>(NotePos.Y + facing.imag() + n2.imag()),
                        numeric::Cast<float>(NotePos.X + facing.real() + n3.real()),
                        numeric::Cast<float>(NotePos.Y + facing.imag() + n3.imag()),
                        numeric::Cast<float>(NotePos.X + facing.real() + n4.real()),
                        numeric::Cast<float>(NotePos.Y + facing.imag() + n4.imag()),
                        Skin.Base->Playing.Image.Note.Handles[KusudamaFlag ? 14 : 12],
                        TRUE
                    );
                }
            }
        }
#undef InRange
        SetDrawAreaFull();
        // Always draw the lane UI independently of note fading.
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        Skin.Base->Playing.Image.Base.Draw(DelayPos);
        Skin.Base->Playing.Image.NamePlate.Draw(DelayPos);
        if(idx==0)DrawPlayerChar(NowTime);
        if (const std::string& title = LaneTitle(idx); !title.empty()) {
            Skin.Base->Playing.Font.TitleName.Draw({
                Skin.Base->Playing.Config.TitleNamePos.X,
                Skin.Base->Playing.Config.TitleNamePos.Y + (IsMulti ? DelayPos.Y : 0.0f) },
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                title
            );
        }
        if (!IsMulti) {
            Skin.Base->Playing.Font.PlayerName.Draw(
                Skin.Base->Playing.Config.PlayerNamePos,
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                Config.PlayerName
            );
        }
        else if (IsMulti) {
            Skin.Base->Playing.Font.PlayerName.Draw({
            Skin.Base->Playing.Config.PlayerNamePos.X,
            Skin.Base->Playing.Config.PlayerNamePos.Y + DelayPos.Y },
            GetColor(255, 255, 255),
            GetColor(0, 0, 0),
            Names[idx]
            );
        }

        {

            size_t i = HitNote[idx].Index;

            for (size_t c = 0; c < HitNote[idx].Size(); ++c) {
                auto& data = HitNote[idx].Datas[i];
                if (!data.MoveTimer.IsRunning()) {
                    data.MoveTimer.Start();
                }

                if (data.FlyingNote.IsActive && Config.HitNoteDisp && !IsMulti && data.MoveElapsedTime < data.FlyingNote.MoveTime()) {

                    float _one = numeric::Cast<float>(data.MoveElapsedTime / data.FlyingNote.MoveTime());

                    const auto& note=Skin.Base->Playing.Image.Note;
                    const auto& gauge=Skin.Base->Playing.Image.ProgressBar;
                    const Pos2D<float> end=HasImage(gauge)?Pos2D<float>{gauge.Pos.X+gauge.Size.Width*.5f,gauge.Pos.Y}:
                        Pos2D<float>{Skin.Info.Resolution.X*.98f,Skin.Info.Resolution.Y*.15f};
                    const float dx=end.X-note.Pos.X,dy=end.Y-note.Pos.Y;
                    const float lift=Skin.Info.Resolution.Y*.35f;
                    const float rate=std::clamp(_one,0.0f,1.0f);
                    Pos2D<float> Pos={dx*rate,dy*rate-4*lift*rate*(1-rate)};
                    Skin.Base->Playing.Image.Note.Draw(Pos, data.FlyingNote.Type - 48);
                }
                else {
                    data.FlyingNote.IsActive = false;
                }

                if (data.JudgeString.IsActive && data.MoveElapsedTime < data.JudgeString.MoveTime()) {
                    DrawJudgeString(data);
                }
                else {
                    data.JudgeString.IsActive = false;
                }

                if (!data.FlyingNote.IsActive && !data.JudgeUnderExplosion.IsActive && !data.JudgeString.IsActive) {
                    data = HitNoteData();
                }

                ++i;
                if (!(i < HitNote[idx].Size())) {
                    i = 0;
                }
            }
        }

        // DefaultではBaseより前だと小太鼓が隠れる。発光もこの後に重ねる。
        Skin.Base->Playing.Image.MiniTaiko.Draw(DelayPos);
        auto TaikoAlpha = [&](size_t index) {
            double alpha = 255 * (1 - GetEasingRate(MiniTaikoFlash[index].GetElapsed().MilliSecond() / MiniTaikoFlashTime, ease::Base::In, ease::Line::Cubic));
            if (alpha < 0) { MiniTaikoFlash[index].Reset(); }
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, numeric::Cast<int>(alpha));
            };

        if (MiniTaikoFlash[0 + (4 * idx)].IsRunning()) {
            TaikoAlpha(0 + (4 * idx));
            Skin.Base->Playing.Image.MiniTaiko_Don.Draw({ Skin.Base->Playing.Image.MiniTaiko_Don.Size.Width * -0.5f, DelayPos.Y }, 0);
        }
        if (MiniTaikoFlash[1 + (4 * idx)].IsRunning()) {
            TaikoAlpha(1 + (4 * idx));
            Skin.Base->Playing.Image.MiniTaiko_Ka.Draw({ Skin.Base->Playing.Image.MiniTaiko_Ka.Size.Width * -0.5f, DelayPos.Y }, 0);
        }
        if (MiniTaikoFlash[2 + (4 * idx)].IsRunning()) {
            TaikoAlpha(2 + (4 * idx));
            Skin.Base->Playing.Image.MiniTaiko_Don.Draw({ Skin.Base->Playing.Image.MiniTaiko_Don.Size.Width * 0.5f, DelayPos.Y }, 1);
        }
        if (MiniTaikoFlash[3 + (4 * idx)].IsRunning()) {
            TaikoAlpha(3 + (4 * idx));
            Skin.Base->Playing.Image.MiniTaiko_Ka.Draw({ Skin.Base->Playing.Image.MiniTaiko_Ka.Size.Width * 0.5f, DelayPos.Y }, 1);
        }
        SetDrawBlendMode(0, 0);

        auto ComboDraw = [&](uint64_t num) {
            auto& img = Skin.Base->Playing.Image.ComboNumber;
            int digit = exmath::digit(num);
            const float stride = img.Size.Width;
            float offset = stride * (digit - 1) / 2;
            int i = 0;
            do {
                img.Draw({ offset, DelayPos.Y }, num % 10);
                num /= 10;
                ++i;
                offset -= stride;
            } while (i < digit);
            };

        auto ScoreDraw = [&](uint64_t num) {
            int digit = exmath::digit(num);
            float offset = Skin.Base->Playing.Image.ScoreNumber.Size.Width - (digit - 1) + digit;
            const float stride = Skin.Base->Playing.Image.ScoreNumber.Size.Width;
            int i = 0;
            do {
                Skin.Base->Playing.Image.ScoreNumber.Draw({ offset, DelayPos.Y }, num % 10);
                num /= 10;
                ++i;
                offset -= stride;
            } while (i < digit);
            };
        auto RollDraw = [&](uint64_t num) {
            int digit = exmath::digit(num);
            float offset = Skin.Base->Playing.Image.RollNumber.Size.Width - (digit - 1) + digit;
            int i = 0;
            do {
                Skin.Base->Playing.Image.RollNumber.Draw({ offset, DelayPos.Y }, num % 10);
                num /= 10;
                ++i;
                offset -= Skin.Base->Playing.Image.RollNumber.Size.Width;
            } while (i < digit);
            };

        // 他のプレイヤーの連打の打数は、叩かれなくなって 0.75 秒で消す
        if (idx > 0 && idx < (int)std::size(RemoteRollAt) && std::chrono::steady_clock::now() - RemoteRollAt[idx] > std::chrono::milliseconds(750)) {
            Chart.Judge[idx].Rolls.NowCount = 0;
        }
        if (Chart.Judge[idx].Rolls.NowCount > 0) {
            RollDraw(Chart.Judge[idx].Rolls.NowCount);
        }
        if (Chart.Judge[idx].Combo >= 3) {
            ComboDraw(Chart.Judge[idx].Combo);
        }
        ScoreDraw(Chart.Judge[idx].Score);

        idx++;

    } while (idx < (int)LaneCount() && IsMulti);

    if (LaneCount() < 4) {
        if (!Chart.IsDanMode()) {
            if (Chart.OriginalData.TitleDisplay) {
                DrawFitScreen(Skin.Base->Playing.Font.Title, Skin.Base->Playing.Config.TitlePos, Chart.OriginalData.Title);
            }
            if (Chart.OriginalData.SubtitleDisplay) {
                DrawFitScreen(Skin.Base->Playing.Font.SubTitle, Skin.Base->Playing.Config.SubTitlePos, Chart.OriginalData.Subtitle);
            }
        }
        else if (Dan.Active && Dan.SongIndex < Dan.Course.DanSongs.size()) {
            const auto& song = Dan.Course.DanSongs[Dan.SongIndex];
            const std::string title = "[" + std::to_string(Dan.SongIndex + 1) + "/" + std::to_string(Dan.Course.DanSongs.size()) + "] " + song.Title;
            DrawFitScreen(Skin.Base->Playing.Font.Title, Skin.Base->Playing.Config.TitlePos, title);
            DrawFitScreen(Skin.Base->Playing.Font.SubTitle, Skin.Base->Playing.Config.SubTitlePos, song.Subtitle);
        }
    }

    if (!IsMulti) {
        if (Config.TrainingMode) {
            if (!Chart.NowTime.IsRunning()) {
                DrawFormatStringF(
                    Skin.Base->Playing.Image.Note.Pos.X,
                    Skin.Base->Playing.Image.Note.Pos.Y - 100,
                    GetColor(255, 255, 255),
                    "(%d/%d)",
                    Training.BarlineIndex, Chart.AllBarlineCount
                );
            }
        }
        if (Dan.Active) {

            Skin.Base->Playing.Image.DanBackGround.Draw({});

            // 全体条件と、現在の曲の個別条件を表示する
            size_t row = 0;
            for (const auto& e : Dan.Exams) {
                if (e.Song >= 0 && (size_t)e.Song != Dan.SongIndex) {
                    continue;
                }
                const float y = 120.0f * row++;
                auto& bar = Skin.Base->Playing.Image.ExamProgressBar;
                bar.Draw({ 0, y }, 0);

                const bool less = e.Exam.Range == ExamRange::Less;
                const double red = e.Exam.PassVal[0];
                // 以上: 赤合格に対する達成度 / 未満: 残りの許容量
                const double shown = less ? std::max(0.0, red - e.Value) : e.Value;
                const double ratio = red > 0 ? std::clamp(shown / red, 0.0, 1.0) : 1.0;
                if (e.Status != ExamStatus::Fail) {
                    bar.RectDraw({ 0, y }, { 0, bar.Size.Height }, { (float)(bar.Size.Width * ratio), bar.Size.Height }, 1);
                }

                unsigned int color = GetColor(255, 255, 255);
                switch (e.Status) {
                case ExamStatus::Red: color = GetColor(255, 110, 110); break;
                case ExamStatus::Gold: color = GetColor(255, 215, 0); break;
                case ExamStatus::Fail: color = GetColor(128, 128, 128); break;
                default: break;
                }
                const std::string name = std::string(e.Song >= 0 ? "[曲] " : "") + ExamLabel(e.Exam.ExamType)
                    + " " + ExamValueText(e.Exam.ExamType, red) + (less ? "未満" : "以上");
                const std::string value = ExamValueText(e.Exam.ExamType, shown);

                Skin.Base->Playing.Font.ExamName.Draw(
                    { Skin.Base->Playing.Config.ExamNamePos.X, Skin.Base->Playing.Config.ExamNamePos.Y + y },
                    GetColor(255, 255, 255), GetColor(0, 0, 0),
                    GetStrlenCached(name, Skin.Base->Playing.Font.ExamName.Handle), name);
                Skin.Base->Playing.Font.ExamVal.Draw(
                    { Skin.Base->Playing.Config.ExamValPos.X, Skin.Base->Playing.Config.ExamValPos.Y + y },
                    color, GetColor(0, 0, 0),
                    GetStrlenCached(value, Skin.Base->Playing.Font.ExamVal.Handle), value);
            }
        }
    }

#ifdef __ANDROID__
    if (!IsMulti) {
        Skin.Base->Playing.Image.Back.Draw({});
        if ((Config.TrainingMode && !Chart.IsDanMode()) || Config.PauseDisplay) {
            Skin.Base->Playing.Image.Pause.Draw({});
        }
    }
#endif

    if (IsMulti && Online.StartPending) {
        const auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(Online.StartAt - std::chrono::steady_clock::now()).count();
        const std::string text = "まもなく開始 " + std::to_string(std::max<int64_t>(0, (remain + 999) / 1000));
        Skin.Base->Playing.Font.Title.Draw(
            { Skin.Info.Resolution.X * 0.5f, Skin.Info.Resolution.Y * 0.5f },
            GetColor(255, 255, 255), GetColor(0, 0, 0),
            GetStrlenCached(text, Skin.Base->Playing.Font.Title.Handle) / 2, text);
    }

    const float keyMapTop=DrawKeyMap();
    if (Calib.Active) {
        const FontData& sfont = ConfigSmallFont();
        const float helpY=std::min(Skin.Info.Resolution.Y-sfont.Size*2.2f,keyMapTop-sfont.Size*.5f-8);
        UIText(sfont, 16, helpY, "オフセット調整: クリック音に合わせてドンを叩いてください (Esc: やめる)", GetColor(255, 220, 90));
    }
    else if (Editor.Testing) {
        const FontData& sfont = ConfigSmallFont();
        const float helpY=std::min(Skin.Info.Resolution.Y-sfont.Size*2.2f,keyMapTop-sfont.Size*.5f-8);
        UIText(sfont, 16, helpY, std::string("テストプレイ  オート: ") + (Config.AutoPlay ? "ON" : "OFF") + "  (F1: 切り替え / Esc: 編集に戻る)", GetColor(255, 220, 90));
    }
    // スコアメーター: 画面の中央・最下部
    if (Config.ScoreMeter) {
        const float width = std::min(360.0f, Skin.Info.Resolution.X - 32.0f);
        const HitErrorSummary summary = SummarizeHitErrors(HitStats.Play, HitErrorLimit());
        DrawHitErrorMeter(Skin.Info.Resolution.X * 0.5f, Skin.Info.Resolution.Y - 16.0f, width, &summary, true, nullptr);
    }

    if (Config.ViewDebug) {
        DrawFormatString(0, 0, GetColor(255, 255, 255), "\n\n\nNowTime:%lf\nBPM:%lf", ChartNowTime(1) / Chart.SongSpeed, Chart.NowBPM * Chart.SongSpeed);
    }
    PlayingPauseDraw();
}

void Game::HitAction(HitType type) {
    if (!IsMulti || NowScene != Scene::Playing || Online.Room.Players.size() < 2) {
        return;
    }
    fasnet::HitEvent hit;
    hit.PlayerId = Online.MyId;
    hit.Lane = (int8_t)type;
    hit.Judge = (int8_t)Chart.Judge[0].HitJudge;
    hit.NoteType = Chart.Judge[0].NoteType;
    hit.RollCount = (uint32_t)std::min<uint64_t>(Chart.Judge[0].Rolls.NowCount, UINT32_MAX);
    hit.Score = ToScore(Chart.Judge[0]);
    Online.Net.Send(fasnet::Msg::Hit, hit);
}

void Game::ApplyRemoteHit(const fasnet::HitEvent& hit) {
    if (NowScene != Scene::Playing) {
        return;
    }
    auto it = std::ranges::find(Online.LaneIds, hit.PlayerId);
    if (it == Online.LaneIds.end() || it == Online.LaneIds.begin()) {
        return;
    }
    const size_t lane = (size_t)(it - Online.LaneIds.begin());
    if (lane >= Chart.Judge.size() || lane >= HitNote.size() || lane >= 4) {
        return;
    }
    const JudgeType judge = (JudgeType)hit.Judge;
    switch ((HitType)hit.Lane) {
    case HitType::DonLeft:
    case HitType::KaLeft:
    case HitType::DonRight:
    case HitType::KaRight:
        MiniTaikoFlash[hit.Lane + 4 * lane].Start();
        if (judge != JudgeType::None) {
            HitNote[lane].Add(HitNoteData(hit.NoteType, judge));
        }
        break;
    case HitType::DonBig:
    case HitType::KaBig:
        MiniTaikoFlash[(hit.Lane - 4) + 4 * lane].Start();
        MiniTaikoFlash[(hit.Lane - 2) + 4 * lane].Start();
        if (judge != JudgeType::None) {
            HitNote[lane].Add(HitNoteData(hit.Lane == (int)HitType::DonBig ? '3' : '4', judge));
        }
        break;
    default:
        break;
    }
    FromScore(Chart.Judge[lane], hit.Score);
    // 連打の打数・風船の残り打数 (少しの間叩かれなければ消す)
    Chart.Judge[lane].Rolls.NowCount = hit.RollCount;
    if (lane < std::size(RemoteRollAt)) { RemoteRollAt[lane] = std::chrono::steady_clock::now(); }
}

auto Game::BranchDecide(const BranchData& branchdata) const -> BranchType {
    const JudgeData& judge = Chart.Judge[0];
    double value = 0;
    switch (branchdata.Type) {
    case IfBranchType::Perfect: value = judge.Branch.Accuracy; break;
    case IfBranchType::Roll: value = (double)judge.Branch.Roll; break;
    case IfBranchType::Score: value = (double)judge.Score; break;
    default: return Chart.NowBranchFlag;
    }
    return value < branchdata.MasterBranch ? (value < branchdata.ExpertBranch ? BranchType::Normal : BranchType::Expert) : BranchType::Master;
}

auto Game::BranchAnimationOf(BranchType from, BranchType to) -> ABranchType {
    if (from == to) { return ABranchType::Null; }
    if (from == BranchType::Normal) { return to == BranchType::Expert ? ABranchType::Normal_Expert : ABranchType::Normal_Master; }
    if (from == BranchType::Expert) { return to == BranchType::Normal ? ABranchType::Expert_Normal : ABranchType::Expert_Master; }
    if (from == BranchType::Master) { return to == BranchType::Normal ? ABranchType::Master_Normal : ABranchType::Master_Expert; }
    return ABranchType::Null;
}

void Game::BranchCleanup(double startms) {
    for (auto& note : Chart.RawNoteDatas) {
        if (note.AbsTime >= startms) { continue; }
        if (note.IsBranch != Chart.NowBranchFlag && note.IsBranch != BranchType::Null) {
            note.HitFlag = true;
            note.IsBranch = BranchType::NotDisplay;
        }
        else if (note.IsBranch == Chart.NowBranchFlag) {
            note.IsBranch = BranchType::Null;
        }
    }
}

void Game::JiroBranchUpdate(double now) {
    auto& branches = Chart.BranchDatas;
    if (Chart.NowBranchFlag == BranchType::Null || branches.empty()) { return; }
    size_t index = 0;
    for (size_t k = 1; k < branches.size(); ++k) {
        if (branches[k - 1].StartMs <= now) { index = k; }
    }
    auto& br = branches[index];
    Chart.JiroUpcomingStart = br.Start ? br.StartMs : std::numeric_limits<double>::infinity();
    if (!Chart.LevelHold && !br.Evaluated && br.AbsTime - 200.0 <= now) {
        if (br.AbsTime < now) { br.Evaluated = true; }
        Chart.JiroNext = BranchDecide(br);
        // 分岐が決まって分岐のアニメーションが始まった時から、決まった分岐の音符を判定する
        // (分岐の始まりの直前に早めに叩いても、新しい分岐の最初の音符を判定できるように)
        // 前の区間も分岐していて、今の分岐の音符が分岐の始まりまでに残っている時は、それを判定し終えるまで切り替えない
        if (br.Start && Chart.NowBranchFlag != Chart.JiroNext) {
            const int lane = Chart.JiroLaneOf(Chart.NowBranchFlag);
            bool pending = false;
            for (size_t k = Chart.JiroCursor[lane]; k < Chart.JiroLanes[lane].size(); ++k) {
                const auto& n = Chart.RawNoteDatas[Chart.JiroLanes[lane][k]];
                if (n.AbsTime >= br.StartMs) { break; }
                if (n.IsBranch != BranchType::Null && !n.HitFlag) { pending = true; break; }
            }
            if (!pending) { Chart.NowBranchFlag = Chart.JiroNext; }
        }
    }
    if (br.Start && br.StartMs - 50.0 < now) {
        BranchCleanup(br.StartMs);
        Chart.NowBranchFlag = Chart.JiroNext;
        br.Start = false;
        br.BranchFlag = false;
    }
    const int target = Chart.JiroLaneOf(Chart.JiroNext) + 1;
    if (Chart.JiroShown != target) {
        const auto wall = std::chrono::steady_clock::now();
        const int dir = target > Chart.JiroShown ? 1 : -1;
        if (!Chart.JiroAnimating) {
            Chart.JiroAnimating = true;
            Chart.JiroAnimTick = wall;
            Chart.NowBranchAnimation = BranchAnimationOf(Chart.JiroBranchOf(Chart.JiroShown - 1), Chart.JiroBranchOf(Chart.JiroShown - 1 + dir));
            Chart.BranchAnimationTime = 0.1;
            Chart.BranchAnimationTimer.Start();
        }
        else if (wall - Chart.JiroAnimTick > std::chrono::milliseconds(99)) {
            Chart.JiroAnimating = false;
            Chart.JiroShown += dir;
        }
    }
}

void Game::JiroPassNotes(double now) {
    const double judgenow = now - Config.JudgeOffset;
    const int lane = Chart.JiroLaneOf(Chart.NowBranchFlag);
    for (int other = 0; other < 3; ++other) {
        if (other == lane) { continue; }
        auto& list = Chart.JiroLanes[other];
        size_t& cur = Chart.JiroCursor[other];
        while (cur < list.size() && Chart.RawNoteDatas[list[cur]].AbsTime <= now) {
            auto& n = Chart.RawNoteDatas[list[cur]];
            if (n.IsBranch != BranchType::Null && n.NoteType >= '1' && n.NoteType <= '4') { n.HitFlag = true; }
            ++cur;
        }
    }
    auto& list = Chart.JiroLanes[lane];
    size_t& cur = Chart.JiroCursor[lane];
    if (cur >= list.size()) { return; }
    auto& n = Chart.RawNoteDatas[list[cur]];
    if (n.NoteType >= '5' && n.NoteType <= '9') {
        if (n.HitFlag || (n.RollEndTime != 0 && n.RollEndTime < now)) { ++cur; }     // 連打・風船は終わったら次へ
        return;
    }
    const bool last = cur + 1 >= list.size();
    const double next = last ? 0.0 : Chart.RawNoteDatas[list[cur + 1]].AbsTime;
    const bool passed = n.HitFlag || n.NoteType < '1' || n.NoteType > '4'
        || n.AbsTime + Config.JudgeBad < judgenow
        || (!last && n.AbsTime + Config.JudgeOk <= judgenow && next - Config.JudgeOk < judgenow);
    if (!passed || n.BigNoteTime != 0) { return; }
    if (!n.HitFlag && n.NoteType >= '1' && n.NoteType <= '4') {
        if (!Config.AutoPlay) {
            Chart.Judge[0].Hit(JudgeType::Bad, 0, '\0');
            HitAction(HitType::Empty);
        }
        n.HitFlag = true;       // 種類は残すので、見逃した音符はそのまま流れていく
    }
    ++cur;
}

auto Game::JiroCursorNote() -> NoteData* {
    const int lane = Chart.JiroLaneOf(Chart.NowBranchFlag);
    auto& list = Chart.JiroLanes[lane];
    size_t& cur = Chart.JiroCursor[lane];
    while (cur < list.size() && Chart.RawNoteDatas[list[cur]].HitFlag && Chart.RawNoteDatas[list[cur]].NoteType == '\0') { ++cur; }
    return cur < list.size() ? &Chart.RawNoteDatas[list[cur]] : nullptr;
}

void Game::BranchChange(const BranchData& branchdata) {
    const JudgeData& judge = Chart.Judge[0];
    double value = 0;
    switch (branchdata.Type) {
    case IfBranchType::Perfect: value = judge.Branch.Accuracy; break;
    case IfBranchType::Roll: value = (double)judge.Branch.Roll; break;
    case IfBranchType::Score: value = (double)judge.Score; break;
    default: return;
    }
    const BranchType next = value < branchdata.MasterBranch
        ? (value < branchdata.ExpertBranch ? BranchType::Normal : BranchType::Expert)
        : BranchType::Master;

    auto animation = [](BranchType from, BranchType to) {
        if (from == to) { return ABranchType::Null; }
        if (from == BranchType::Normal) { return to == BranchType::Expert ? ABranchType::Normal_Expert : ABranchType::Normal_Master; }
        if (from == BranchType::Expert) { return to == BranchType::Normal ? ABranchType::Expert_Normal : ABranchType::Expert_Master; }
        if (from == BranchType::Master) { return to == BranchType::Normal ? ABranchType::Master_Normal : ABranchType::Master_Expert; }
        return ABranchType::Null;
    };
    Chart.NowBranchAnimation = animation(Chart.NowBranchFlag, next);
    Chart.NowBranchFlag = next;
}

void Game::PlayingProc() {
    if (UpdatePlayingBlackout()) { return; }
    if (PlayingPauseProc()) { return; }
    if (Config.AutoPlay) { Chart.AutoUsed = true; }

    // オンライン: サーバーが決めた時刻 (往復遅延を補正済み) になったら全員同時に開始する
    if (IsMulti && Online.StartPending && std::chrono::steady_clock::now() >= Online.StartAt) {
        Online.StartPending = false;
        Chart.NowTime.Start();
#ifndef __ANDROID__
        LastHitTick = RawKeyboard::Now();
#endif
    }

    const double NowTime = ChartNowTime(1000) + Training.Offset;
    Chart.UpdateScanStart(NowTime, Config.JudgeBad);
#ifndef __ANDROID__
    const int64_t NowTick = RawKeyboard::Now();
    bool SkipKeyEvents = false;
#endif

    if (Chart.NowTime.IsRunning()) {
        if (Chart.SongBlankTime < NowTime && Chart.SongBlankTime + 128 > NowTime && !Chart.SongData.IsPlay()) {
            if (Training.BarlineIndex < Chart.AllBarlineCount) {
                // 開始タイミングを検出したフレームまでの遅れ (最大 1 フレーム程度) だけ先から再生し、譜面と音を揃える
                Chart.SongData.SetCurrent(NowTime - Chart.SongBlankTime);
                Chart.SongData.Play(FALSE);
#ifndef __ANDROID__
                RecorderNoteSong(NowTime - Chart.SongBlankTime);
#endif
            }
        }
        else if (Chart.SongBlankTime + 5000 < NowTime && !Chart.SongData.IsPlay()) {
            if (Dan.Active) {
                // 曲の終わり: この曲の個別条件 (最終曲なら全体条件も) を確定し、次の曲かリザルトへ
                UpdateDanExams(true);
                if (!Dan.Failed && Dan.SongIndex + 1 < Dan.Course.DanSongs.size()) {
                    BeginPlayingBlackout(PlayingBlackoutKind::DanNext);
                }
                else {
                    NowScene = Scene::Result;
                }
                return;
            }
            if (!IsMulti && Config.TrainingMode) {
                RecorderAbort();
                NowScene = Scene::Loading;
                return;
            }
            if (Calib.Active) {
                // オフセット調整: ずれをまとめて設定画面へ戻る
                Calib.Finished = true;
                Calib.Result = SummarizeHitErrors(HitStats.Play, HitErrorLimit());
                NowScene = Scene::ConfigMenu;
                return;
            }
            if (!IsMulti) {
                NowScene = Scene::Result;
                return;
            }
            // オンライン: 自分の結果を送り、全員が終わるまで待つ (OnRoomChanged でリザルトへ)
            if (!Online.FinishSent) {
                Online.FinishSent = true;
                Online.Net.Send(fasnet::Msg::Finish, ToScore(Chart.Judge[0]));
            }
        }
    }

    if (Config.JiroChartMode) {
        JiroBranchUpdate(NowTime);
    }
    else if (Chart.NowBranchFlag != BranchType::Null && !Chart.LevelHold) {
        for (size_t i = 0, size = Chart.BranchDatas.size(); i < size; ++i) {
            if (Chart.BranchDatas[i].AbsTime < NowTime && Chart.BranchDatas[i].Start) {
                for (size_t j = 0, nsize = Chart.RawNoteDatas.size(); j < nsize; ++j) {
                    if (Chart.RawNoteDatas[j].AbsTime >= Chart.BranchDatas[i].StartMs) { continue; }
                    if (Chart.RawNoteDatas[j].IsBranch != Chart.NowBranchFlag && Chart.RawNoteDatas[j].IsBranch != BranchType::Null) {
                        Chart.RawNoteDatas[j].HitFlag = true;
                        Chart.RawNoteDatas[j].IsBranch = BranchType::NotDisplay;
                    }
                    else if (Chart.RawNoteDatas[j].IsBranch == Chart.NowBranchFlag) {
                        Chart.RawNoteDatas[j].IsBranch = BranchType::Null;
                    }
                }
                BranchChange(Chart.BranchDatas[i]);
                Chart.BranchDatas[i].BranchFlag = false;
                Chart.BranchDatas[i].Start = false;
                Chart.BranchAnimationTimer.Start();
            }
        }
    }

    if (IsMulti) {
        // 送信する判定情報は打撃ごとに作るので、前フレームの値を残さない
        Chart.Judge[0].NoteType = '\0';
        Chart.Judge[0].HitJudge = JudgeType::None;
    }
    else {
        if (Config.TrainingMode && !Chart.IsDanMode()) {
            if (!Chart.NowTime.IsRunning()) {
                if (!Training.BarlineMoveTimer.IsRunning()) {

                    auto MoveInputProc = [&](bool direction) {
                        if (direction) {
                            if (Training.BarlineIndex < Chart.AllBarlineCount) {
                                auto find = std::ranges::find_if(std::next(
                                    Chart.RawNoteDatas.begin() + Training.NoteDataIndex),
                                    Chart.RawNoteDatas.end(),
                                    &NoteData::BarlineDisplay);
                                if (find != Chart.RawNoteDatas.end()) {
                                    Training.NoteDataIndex = std::distance(
                                        Chart.RawNoteDatas.begin(), find);
                                    ++Training.BarlineIndex;
                                    Training.MemNowTime = NowTime;
                                    Training.BarlineMoveTimer.Start();
                                }
                            }
                        }
                        else {
                            if (Training.BarlineIndex > 0) {
                                auto find = std::ranges::find_if(std::reverse_iterator(
                                    Chart.RawNoteDatas.begin() + Training.NoteDataIndex),
                                    Chart.RawNoteDatas.rend(),
                                    &NoteData::BarlineDisplay);
                                if (find != Chart.RawNoteDatas.rend()) {
                                    Training.NoteDataIndex = std::distance(
                                        Chart.RawNoteDatas.begin(), std::prev(find.base()));
                                    --Training.BarlineIndex;
                                    Training.MemNowTime = NowTime;
                                    Training.BarlineMoveTimer.Start();
                                }
                            }
                        }
                        };

                    auto WarpInputProc = [&](bool direction) {
                        if (direction) {
                            auto find = std::ranges::find_if(Chart.RawNoteDatas.begin(),
                                Chart.RawNoteDatas.end(),
                                &NoteData::BarlineDisplay);
                            if (find != Chart.RawNoteDatas.end()) {
                                Training.NoteDataIndex = std::distance(
                                    Chart.RawNoteDatas.begin(), find);
                                Training.BarlineIndex = 0;
                                Training.MemNowTime = NowTime;
                                Training.BarlineMoveTimer.Start();
                            }
                        }
                        else {
                            auto find = std::ranges::find_if(Chart.RawNoteDatas.rbegin(),
                                Chart.RawNoteDatas.rend(),
                                &NoteData::BarlineDisplay);
                            if (find != Chart.RawNoteDatas.rend()) {
                                Training.NoteDataIndex = std::distance(
                                    Chart.RawNoteDatas.begin(), std::prev(find.base()));
                                Training.BarlineIndex = Chart.AllBarlineCount;
                                Training.MemNowTime = NowTime;
                                Training.BarlineMoveTimer.Start();

                            }
                        }
                        };

                    auto StartInputProc = [&] {
#ifndef __ANDROID__
                        // 再開に使った打鍵を演奏の打撃として二重に扱わない
                        SkipKeyEvents = true;
                        LastHitTick = NowTick;
#endif
                        Chart.NowTime.Start();
                        if (Chart.SongBlankTime < NowTime
                            && Training.BarlineIndex > 0
                            && Training.BarlineIndex < Chart.AllBarlineCount) {
                            Chart.SongData.SetCurrent(NowTime - Chart.SongBlankTime);
                            Chart.SongData.Play(FALSE);
                        }
                        };

                    auto BranchChangeProc = [&](bool direction) {
                        if (!Chart.IsBranchChart()) { return; }
                        if (direction) {
                            Chart.NowBranchFlag = (Chart.NowBranchFlag < BranchType::Master)
                                ? (BranchType)((int)Chart.NowBranchFlag + 1)
                                : BranchType::Master;
                        }
                        else {
                            Chart.NowBranchFlag = (Chart.NowBranchFlag > BranchType::Normal)
                                ? (BranchType)((int)Chart.NowBranchFlag - 1)
                                : BranchType::Normal;
                        }
                        Training.MemBranchFlag = Chart.NowBranchFlag;
                        };

#ifndef __ANDROID__
                    Input::GetInstance().HitKeyProcess(VK_NEXT, KeyState::Down, [&] { MoveInputProc(false); }, Config.KeyHoldProcInterval);
                    Input::GetInstance().HitKeyProcess(VK_PRIOR, KeyState::Down, [&] { MoveInputProc(true); }, Config.KeyHoldProcInterval);
                    Input::GetInstance().HitKeyProcess(VK_DOWN, KeyState::Down, [&] { BranchChangeProc(false); });
                    Input::GetInstance().HitKeyProcess(VK_UP, KeyState::Down, [&] { BranchChangeProc(true); });
                    Input::GetInstance().HitKeyProcess(VK_END, KeyState::Down, [&] { WarpInputProc(false); });
                    Input::GetInstance().HitKeyProcess(VK_HOME, KeyState::Down, [&] { WarpInputProc(true); });

                    Input::GetInstance().HitKeyesProcess(Config.DonInputLeft, KeyState::Down, StartInputProc);
                    Input::GetInstance().HitKeyesProcess(Config.DonInputRight, KeyState::Down, StartInputProc);
                    Input::GetInstance().HitKeyProcess(VK_SPACE, KeyState::Down, StartInputProc);
                    Input::GetInstance().HitKeyProcess(VK_RETURN, KeyState::Down, StartInputProc);
#else
                    Touch.Process(TouchType::RightKa, [&] { MoveInputProc(false); });
                    Touch.Process(TouchType::LeftKa, [&] { MoveInputProc(true); });
                    Touch.Process(TouchType::Don, StartInputProc);
#endif
                }

                else {
                    const double MoveTime = Training.BarlineMoveTimer.GetElapsed().Second();
                    double Rate = GetEasingRate(MoveTime / Training.BarlineMoveTime,
                        ease::Base::In, ease::Line::Linear);
                    Training.Offset = std::lerp(Training.MemNowTime,
                        Chart.RawNoteDatas[Training.NoteDataIndex].AbsTime,
                        Rate);
                    if (Rate >= 1.0) {
                        Training.BarlineMoveTimer.Reset();
                    }
                }
            }
        }

        if (Dan.Active) {
            UpdateDanExams(false);
            // 次の曲を演奏中に非同期で先読みし、曲間の待ち時間を減らす
            const size_t next = Dan.SongIndex + 1;
            if (Chart.NowTime.IsRunning() && !Dan.Failed && next < Dan.Course.DanSongs.size() && Dan.NextSongIndex != next) {
                Dan.NextSongIndex = next;
                SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMNOPRESS);
                Dan.NextSong.LoadAsync(Dan.Course.DanSongs[next].WavePath.string());
            }
        }

        auto BackInputProc = [&] {
            RecorderAbort();        // 選曲画面への途中退出: 録画は保存しない
            Training.Init();
            Dan.Reset();
			Chart.Init(true);
            NowScene = PrevScene;
            };
        auto ReLoadInputProc = [&] {
            if (Chart.IsDanMode()) { return; }
            RecorderAbort();        // 譜面のリロード: 録画は保存しない
            NowScene = Scene::Loading;
            };

#ifndef __ANDROID__
        if (!Config.PauseDisplay) {
            Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
            Input::GetInstance().HitKeyProcess(VK_TAB, KeyState::Down, ReLoadInputProc);
        }
        // 譜面制作のテストプレイ: F1 でオートプレイと通常のプレイをその場で切り替える
        if (Editor.Testing && !Calib.Active) {
            Input::GetInstance().HitKeyProcess(VK_F1, KeyState::Down, [&] {
                Config.AutoPlay = !Config.AutoPlay;
                PlayHitSe(1);
                });
        }
#else
        Touch.Process(TouchType::Other, BackInputProc, Skin.Base->Playing.Image.Back);
        if (!Config.PauseDisplay) { Touch.Process(TouchType::Other, ReLoadInputProc, Skin.Base->Playing.Image.Pause); }
#endif
    }

    bool emphasizeHit=false;
    auto JudgeNote = [&](double nowtime, char type, int hitLane=-1) {
        emphasizeHit=false;

        auto& Judge = Chart.Judge[0];
        size_t rollcount = 0;
        size_t ballooncount = 0;
        NoteData* balloondata = nullptr;

        bool NextImage = false;

        for (auto&& data : Chart.ActiveNotes()) {

            bool NoteBranch = data.IsBranch == Chart.NowBranchFlag || data.IsBranch == BranchType::Null;

            if (!NoteBranch) {
                continue;
            }
            if (data.HitFlag) {
                continue;
            }
            // 次郎形式: ドン・カッは、書かれた順で今判定する音符だけを叩ける
            if (Config.JiroChartMode && data.NoteType >= '1' && data.NoteType <= '4' && data.BigNoteTime == 0 && &data != JiroCursorNote()) {
                continue;
            }

            if (data.RollFlag == 1) {
                ++rollcount;
                NextImage = data.NoteType == '6';
            }

            if (data.BalloonFlag == 1) {
                balloondata = &data;
            }

            if (data.BigNoteTime != 0) {
                if (Config.JudgeGood < nowtime - data.BigNoteTime) {
                    data.NoteType -= 2;
                }
                nowtime = data.BigNoteTime;
            }

            const double _HitError = (data.AbsTime - nowtime) + Config.JudgeOffset;
            const bool GoodHit =
                _HitError > -Config.JudgeGood && _HitError < Config.JudgeGood;
            const bool OkHit =
                _HitError > -Config.JudgeOk && _HitError < Config.JudgeOk;
            const bool BadHit =
                _HitError > -Config.JudgeBad && _HitError < Config.JudgeBad;
            bool TypeMatch = type == data.NoteType;

            switch (data.NoteType) {
            case '3':
            case '4':
                TypeMatch = type == data.NoteType - 2;
                break;
            }

            if (!(BadHit && TypeMatch)) { continue; }

            switch (data.NoteType) {
            case '3':
            case '4':
                if(data.BigNoteTime==0) { data.BigNoteTime=nowtime;data.BigNoteLane=hitLane;return; }
                if(hitLane<0 || hitLane==data.BigNoteLane) return;
                emphasizeHit=true;
                break;
            }
            if (GoodHit) {
                HitNote[0].Add(HitNoteData(data.NoteType, JudgeType::Good));
                Judge.Hit(JudgeType::Good, Chart.AddScore, type);
            }
            else if (OkHit) {
                HitNote[0].Add(HitNoteData(data.NoteType, JudgeType::Ok));
                Judge.Hit(JudgeType::Ok, Chart.AddScore, type);
            }
            else if (BadHit) {
                HitNote[0].Add(HitNoteData('\0', JudgeType::Bad));
                Judge.Hit(JudgeType::Bad, 0, type);
            }
            RecordHitError(-_HitError);     // _HitError は「早いほど正」なので符号を逆にして「遅いほど正」にする

            data.HitFlag = true;
            data.FullComboHit = GoodHit || OkHit;
            data.NoteType = '\0';

            return;
        }

        if (rollcount > 0) {
            HitNote[0].Add(HitNoteData(NextImage ? '6' : '5', JudgeType::Roll));
            Judge.Hit(JudgeType::Roll, 100, NextImage ? '6' : '5');
            if (Chart.RollViewEndTimer.IsRunning() && Chart.Judge[0].Rolls.IsEnd) {
                Chart.Judge[0].Rolls.IsEnd = false;
                Chart.Judge[0].Rolls.NowCount = 0;
                Chart.RollViewEndTimer.Reset();
            }
            Chart.Judge[0].Rolls.NowCount++;
        }

        if (type == '1' && balloondata != nullptr) {
            --balloondata->BalloonCount;
            Chart.Judge[0].Rolls.NowCount = balloondata->BalloonCount;
            Chart.RollViewEndTimer.Reset();
            Judge.Hit(JudgeType::Roll, 100, '\0');
            if (balloondata->BalloonCount <= 0) {
                PlayHitSe(2);
                HitNote[0].Add(HitNoteData('3', JudgeType::Roll));
                balloondata->NoteType = '0';
                balloondata->HitFlag = true;
                balloondata->BalloonFlag = 2;
                Judge.NoteType = '3';
            }
        }
        };

    if (Chart.RollViewEndTimer.GetElapsed().Second() > Chart.RollViewEndTime) {
        for (auto&& judge : Chart.Judge) {
            judge.Rolls.NowCount = 0;
            judge.Rolls.IsEnd = false;
        }
        Chart.RollViewEndTimer.Reset();
    }

    // lane は HitType の DonLeft/KaLeft/DonRight/KaRight (= MiniTaikoFlash の添字)
    auto HitLane = [&](int lane, double hittime) {
        const bool don = (lane % 2) == 0;
        Chart.Judge[0].HitJudge = JudgeType::None;
        Chart.Judge[0].NoteType = '\0';

        MiniTaikoFlash[lane].Start();
        JudgeNote(hittime, don ? '1' : '2',lane);
        PlayHitSe(don ? 0 : 1,emphasizeHit);
        HitAction((HitType)lane);
        };

#ifndef __ANDROID__
    // 見逃し判定より先に、押した瞬間の時刻で判定する (描画待ちで遅れた分だけ判定が遅れない)
    if (RawKeyboard::GetInstance().IsActive() && Chart.NowTime.IsRunning() && !Config.AutoPlay && !SkipKeyEvents) {
        const std::vector<int>* lanes[4] = { &Config.DonInputLeft, &Config.KaInputLeft, &Config.DonInputRight, &Config.KaInputRight };
        for (const auto& e : KeyEvents) {
            if (!e.Down) { continue; }
            // 前フレーム以前の打鍵 (ロード中など) は前フレーム時刻に丸める
            const int64_t tick = std::max(e.Time, LastHitTick);
            const double hittime = NowTime - RawKeyboard::TicksToMs(NowTick - tick) * Chart.SongSpeed;
            for (int lane = 0; lane < 4; ++lane) {
                if (std::ranges::find(*lanes[lane], (int)e.VKey) != lanes[lane]->end()) {
                    HitLane(lane, hittime);
                }
            }
        }
    }
    LastHitTick = NowTick;
#endif

    if (Config.JiroChartMode && Chart.NowTime.IsRunning()) {
        JiroPassNotes(NowTime);
    }

    for (auto&& data : Chart.ActiveNotes()) {

        bool NoteBranch = data.IsBranch == Chart.NowBranchFlag || data.IsBranch == BranchType::Null;
        bool HitFlag = data.AbsTime < NowTime;

        if (data.Section && HitFlag) {
            data.Section = false;
            Chart.Judge[0].Branch.Init();
        }

        if (!NoteBranch) { continue; }
        if (data.LevelHold && HitFlag) {
            Chart.LevelHold = true;
        }
        if (data.GoGoStart && HitFlag) {
            Chart.NowGoGo = true;
        }
        if (data.GoGoEnd && HitFlag) {
            Chart.NowGoGo = false;
        }

        if (data.AbsTime - Config.JudgeBad > NowTime) { continue; }
        if (data.HitFlag) { continue; }

        const double _HitError = (data.AbsTime - NowTime) + Config.JudgeOffset;
        const bool BadHit = _HitError > -Config.JudgeBad && _HitError < Config.JudgeBad;

        if (data.BigNoteTime != 0 && Config.JudgeGood < NowTime - data.BigNoteTime) {
            JudgeNote(NowTime, data.NoteType - 2);
        }

        if (!data.HitFlag &&
            !Config.JiroChartMode &&       // 次郎形式の見逃しは JiroPassNotes で書かれた順に判定する
            (data.NoteType >= '1' && data.NoteType <= '4') &&
            data.BigNoteTime == 0 &&
            _HitError < -Config.JudgeBad) {
            Chart.Judge[0].Hit(JudgeType::Bad, 0, '\0');
            HitAction(HitType::Empty);
            data.HitFlag = true;
        }

        if ((data.NoteType >= '5' && data.NoteType <= '6') &&
            HitFlag) {
            data.RollFlag = 1;
            if (data.RollEndTime < NowTime) {
                data.RollFlag = 2;
                data.HitFlag = true;
                Chart.Judge[0].Rolls.IsEnd = true;
                Chart.RollViewEndTimer.Start();
            }
        }

        if ((data.NoteType == '7' || data.NoteType == '9') &&
            HitFlag) {
            data.BalloonFlag = 1;
            if (data.RollEndTime < NowTime) {
                data.BalloonFlag = 2;
                data.HitFlag = true;
                Chart.Judge[0].Rolls.NowCount = 0;
            }
        }
    }

    if (Chart.NowTime.IsRunning()) {
        if (Config.AutoPlay) {

            size_t RollCount = 0;
            NoteData* BalloonData = nullptr;
            bool NextImage = false;

            for (auto&& data : Chart.ActiveNotes()) {

                bool NoteBranch = data.IsBranch == Chart.NowBranchFlag || data.IsBranch == BranchType::Null;
                bool HitFlag = data.AbsTime < NowTime;
                bool IsHitNote = (data.NoteType >= '1' && data.NoteType <= '4');

                if (data.Section && HitFlag) {
                    data.Section = false;
                    Chart.Judge[0].Branch.Init();
                }

                if (!NoteBranch) {
                    continue;
                }

                if (data.RollFlag == 1) {
                    ++RollCount;
                    NextImage = data.NoteType == '6';
                }

                if (data.BalloonFlag == 1) {
                    BalloonData = &data;
                }

                if (HitFlag && !data.HitFlag && IsHitNote) {
                    data.FullComboHit = true;
                    HitNote[0].Add(HitNoteData(data.NoteType, JudgeType::Good));
                    Chart.Judge[0].Hit(JudgeType::Good, 0, data.NoteType);
                    switch (data.NoteType) {
                    case '1':
                        PlayHitSe(0);
                        MiniTaikoFlash[0 + Chart.AutoPlayLR * 2].Start();
                        Chart.AutoPlayLR = !Chart.AutoPlayLR;
                        HitAction((HitType)(0 + Chart.AutoPlayLR * 2));
                        break;
                    case '2':
                        PlayHitSe(1);
                        MiniTaikoFlash[1 + Chart.AutoPlayLR * 2].Start();
                        Chart.AutoPlayLR = !Chart.AutoPlayLR;
                        HitAction((HitType)(1 + Chart.AutoPlayLR * 2));
                        break;
                    case '3':
                        PlayHitSe(0,true);
                        MiniTaikoFlash[0].Start();
                        MiniTaikoFlash[2].Start();
                        HitAction(HitType::DonBig);
                        break;
                    case '4':
                        PlayHitSe(1,true);
                        MiniTaikoFlash[1].Start();
                        MiniTaikoFlash[3].Start();
                        HitAction(HitType::KaBig);
                        break;
                    }
                    data.NoteType = '\0';
                    data.HitFlag = true;
                }
            }

            if (RollCount > 0 && !Chart.WaitRollTime.IsRunning()) {
                PlayHitSe(0);
                if (Chart.RollViewEndTimer.IsRunning() && Chart.Judge[0].Rolls.IsEnd) {
                    for (auto&& judge : Chart.Judge) {
                        judge.Rolls.NowCount = 0;
                        judge.Rolls.IsEnd = false;
                    }
                    Chart.RollViewEndTimer.Reset();
                }
                Chart.AutoPlayLR = !Chart.AutoPlayLR;
                Chart.Judge[0].Roll++;
                Chart.Judge[0].Branch.Roll++;
                Chart.Judge[0].Rolls.NowCount++;
                HitNote[0].Add(HitNoteData(NextImage ? '6' : '5', JudgeType::Roll));
                Chart.WaitRollTime.Start();
                HitAction((HitType)(0 + Chart.AutoPlayLR * 2));
                Chart.Judge[0].NoteType = NextImage ? '6' : '5';
            }
            if (BalloonData != nullptr && !Chart.WaitRollTime.IsRunning()) {
                PlayHitSe(0);
                Chart.AutoPlayLR = !Chart.AutoPlayLR;
                Chart.Judge[0].Roll++;
                Chart.Judge[0].Branch.Roll++;
                --BalloonData->BalloonCount;
                Chart.Judge[0].Rolls.NowCount = BalloonData->BalloonCount;
                Chart.RollViewEndTimer.Reset();
                Chart.WaitRollTime.Start();
                HitAction((HitType)(0 + Chart.AutoPlayLR * 2));
                if (BalloonData->BalloonCount <= 0) {
                    PlayHitSe(2);
                    HitNote[0].Add(HitNoteData('3', JudgeType::Roll));
                    BalloonData->NoteType = '0';
                    BalloonData->HitFlag = true;
                    BalloonData->BalloonFlag = 2;
                    Chart.Judge[0].NoteType = '3';
                }
            }
            if (Chart.WaitRollTime.GetElapsed().Second() > 1.0 / Config.RollSpeed) {
                Chart.WaitRollTime.Reset();
            }
        }
        else {
#ifndef __ANDROID__
            // Raw Input 有効時は判定済みなので、メニュー用のキー状態だけ更新する
            // Raw Input のイベントが一度も届いていない環境 (他のウィンドウに奪われている等) では、ウィンドウのキー入力で判定する
            const bool raw = RawKeyboard::GetInstance().IsActive() && RawEventsSeen;
            auto LegacyHit = [&](int lane) {
                if (raw) { return; }
                LegacyHitTick = RawKeyboard::Now();
                HitLane(lane, NowTime);
            };
            Input::GetInstance().HitKeyesProcess(Config.DonInputLeft, KeyState::Down, [&] { LegacyHit((int)HitType::DonLeft); });
            Input::GetInstance().HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { LegacyHit((int)HitType::KaLeft); });
            Input::GetInstance().HitKeyesProcess(Config.DonInputRight, KeyState::Down, [&] { LegacyHit((int)HitType::DonRight); });
            Input::GetInstance().HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { LegacyHit((int)HitType::KaRight); });
#else
            Touch.Process(TouchType::LeftDon, [&] { HitLane((int)HitType::DonLeft, NowTime); });
            Touch.Process(TouchType::LeftKa, [&] { HitLane((int)HitType::KaLeft, NowTime); });
            Touch.Process(TouchType::RightDon, [&] { HitLane((int)HitType::DonRight, NowTime); });
            Touch.Process(TouchType::RightKa, [&] { HitLane((int)HitType::KaRight, NowTime); });
#endif
        }
    }
}
