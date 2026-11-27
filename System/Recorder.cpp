#include "../Game.hpp"

#ifndef __ANDROID__
double Game::RecorderSeconds() const {
    const auto now = PlayingPause.Active ? PlayingPause.StartedAt : std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - Recorder.StartedAt).count();
}

void Game::PlayHitSe(int kind,bool emphasized) {
    auto& se=Skin.Base->Playing.SE;
    const SoundData* bigSound=kind==0?&se.DonBig:kind==1?&se.KaBig:nullptr;
    const bool dedicated=emphasized&&bigSound&&bigSound->Handle!=-1;
    const int recordedKind=dedicated?(kind==0?3:4):kind;
    const auto& sound=dedicated?*bigSound:kind==0?se.Don:kind==1?se.Ka:se.Balloon;
    sound.Play();
    [[maybe_unused]] float gain=1.0f;
    if(emphasized && kind<2 && !dedicated) {
        const int layer=snd::Duplicate(sound.Handle);
        if(layer!=-1) {
            snd::SetVolume(layer, numeric::Cast<int>(std::clamp(Chart.OriginalData.SeVolume*Config.SEVolume*.0255,0.0,255.0)));
            snd::SetDeleteOnFinish(layer, true);
            snd::Play(layer, true);
            gain=2.0f;
        }
    }
#ifndef __ANDROID__
    if(Recorder.Active) Recorder.SeLog.push_back({RecorderSeconds(),recordedKind,gain});
#endif
}

void Game::RecorderNoteSong(double posms) {
    if (!Recorder.Active || Recorder.SongStarted) { return; }
    Recorder.SongStarted = true;
    Recorder.SongAt = RecorderSeconds();
    Recorder.SongPos = posms / 1000.0;
}

void Game::RecorderStatus(std::string text, int seconds) {
    std::lock_guard lock(Recorder.StatusMutex);
    Recorder.Status = std::move(text);
    Recorder.StatusUntil = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
}

void Game::RecorderRelease() {
    Recorder.PendingCounts.clear();
}

void Game::RecorderStart() {
    if (Recorder.Active || !Config.RecordPlay || Editor.Testing) { return; }
    if (Recorder.AwaitingDecision) { RecorderDecide(true); }     // 前の録画の確認が残っていれば保存する
    if (Recorder.Muxer.joinable()) { Recorder.Muxer.join(); }     // 前の録画の書き出しを待つ
    Recorder.SeLog.clear();
    Recorder.SongStarted = false;
    Recorder.SongUntil = -1;
    Recorder.AbortRequested = false;
    Recorder.SongPath = Chart.OriginalData.SongPath;
    Recorder.SongSpeed = Chart.SongSpeed > 0 ? Chart.SongSpeed : 1.0;
    Recorder.SongGain = rec::VolumeGain(Chart.OriginalData.SongVolume, Config.SongVolume);
    Recorder.SeGain = rec::VolumeGain(Chart.OriginalData.SeVolume, Config.SEVolume);
    Recorder.SePath[0] = U8ToPath(Skin.Base->Playing.SE.Don.FilePath);
    Recorder.SePath[1] = U8ToPath(Skin.Base->Playing.SE.Ka.FilePath);
    Recorder.SePath[2] = U8ToPath(Skin.Base->Playing.SE.Balloon.FilePath);
    Recorder.SePath[3] = U8ToPath(Skin.Base->Playing.SE.DonBig.FilePath);
    Recorder.SePath[4] = U8ToPath(Skin.Base->Playing.SE.KaBig.FilePath);
    Recorder.Ffmpeg = rec::FindFfmpeg(GetExecutablePath());
    if (Recorder.Ffmpeg.empty()) { RecorderStatus("ffmpeg が見つからないため録画できません (Config の「録画とツール」から導入できます)"); return; }
    // 画面用テクスチャ (スキンの解像度) をそのまま録画する。読み出す形式は ARGB8888 (メモリ上は B, G, R, A の順)
    Recorder.Bgra = true;
    Recorder.Width = gfx::LogicalWidth();
    Recorder.Height = gfx::LogicalHeight();

    // 保存先: recordings/日時_曲名.mp4
    std::error_code ec;
    const fs::path dir = GetExecutablePath() / "recordings";
    fs::create_directories(dir, ec);
    char stamp[32];
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm);
    std::string title = Chart.OriginalData.Title;
    for (char& c : title) { if (std::string_view("\\/:*?\"<>|").find(c) != std::string_view::npos || (unsigned char)c < 0x20) { c = '_'; } }
    if (title.size() > 60) { title.resize(60); }
    const std::string name = std::string(stamp) + (title.empty() ? "" : "_" + title);
    Recorder.OutPath = dir / U8ToPath(name + ".mp4");
    Recorder.VideoPath = dir / U8ToPath(name + ".video.mp4");
    Recorder.AudioPath = dir / U8ToPath(name + ".wav");
    Recorder.Fps = std::clamp(Config.RecordFps, 15, 120);
    if (!Recorder.Video.Start(Recorder.Ffmpeg, Recorder.VideoPath, Recorder.Width, Recorder.Height, Recorder.Fps, Recorder.Bgra, Config.RecordCodec, std::clamp(Config.RecordCrf, 0, 51))) {
        RecorderStatus("ffmpeg を起動できませんでした");
        RecorderRelease();
        return;
    }
    Recorder.StartedAt = std::chrono::steady_clock::now();
    Recorder.Scheduled = 0;
    Recorder.PendingCounts.clear();
    for (gfx::Image stale; gfx::TakeReadPixels(stale, true);) {}      // 前の録画を途中で止めた時の読み残しを捨てる
    Recorder.Cur = 0;
    Recorder.Active = true;
}

namespace {
    std::vector<uint8_t> FramePixels(const gfx::Image& frame, int width, int height) {
        const size_t row = (size_t)width * 4;
        std::vector<uint8_t> pixels(row * height);
        for (int y = 0; y < height && y < frame.H; ++y) {
            std::memcpy(pixels.data() + row * y, frame.Pixels.data() + (size_t)frame.W * y, std::min(row, (size_t)frame.W * 4));
        }
        return pixels;
    }
}

// GPU の読み出しが終わったフレームをエンコーダに渡す。wait なら残りをすべて待って渡す (録画の終わり)
void Game::RecorderReadPending(bool wait) {
    gfx::Image frame;
    while (!Recorder.PendingCounts.empty() && gfx::TakeReadPixels(frame, wait)) {
        Recorder.Video.Push(FramePixels(frame, Recorder.Width, Recorder.Height), Recorder.PendingCounts.front());
        Recorder.PendingCounts.pop_front();
    }
    if (wait) { Recorder.PendingCounts.clear(); }
}

void Game::RecorderCapture() {
    if (!Recorder.Active || PlayingPause.Active) { return; }
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - Recorder.StartedAt).count();
    const uint64_t target = (uint64_t)(elapsed * Recorder.Fps) + 1;
    if (target <= Recorder.Scheduled) { return; }
    const int count = (int)std::min<uint64_t>(target - Recorder.Scheduled, (uint64_t)Recorder.Fps);   // 止まっていた分は同じ画で埋める
    Recorder.Scheduled = target;
    // 録画のフレームレートの時だけ画面用テクスチャを GPU 上でコピーし、GPU が終わった前のフレームを受け取る (描画は待たない。エンコードは別スレッド)
    if (gfx::ReadPixelsAsync(0, 0, Recorder.Width, Recorder.Height)) { Recorder.PendingCounts.push_back(count); }
    RecorderReadPending(false);
}

void Game::RecorderStop() {
    if (!Recorder.Active) { return; }
    RecorderReadPending(true);      // 最後にコピーしたフレームまで読む
    Recorder.Active = false;
    RecorderRelease();
    Recorder.Duration = RecorderSeconds();
    Recorder.AudioDelay = 0.0;      // 音声は譜面から作るので、録画の開始が 0 秒
    Recorder.Dropped = Recorder.Video.Dropped();
    // 映像のエンコードの残りは別スレッドで待つ (その間にリザルトで保存するか聞く)
    Recorder.VideoOk = false;
    if (Recorder.Finisher.joinable()) { Recorder.Finisher.join(); }
    Recorder.Finisher = std::thread([this] { try { Recorder.VideoOk = Recorder.Video.Finish(); } catch(...) { Recorder.VideoOk=false; } });
    if (Recorder.AbortRequested) {
        Recorder.AbortRequested = false;
        RecorderDecide(false);      // 途中退出・リロード: 中断して保存しない
    }
    else if (NowScene == Scene::Result) {
        Recorder.AwaitingDecision = true;
    }
    else {
        RecorderDecide(true);       // 段位の次の曲へ進んだ時などは確認せずに保存する
    }
}

fs::path Game::RecorderPrepareAudio() {
    // 譜面から作る: 曲と効果音をデコードし、混ぜて WAV にする
    auto load = [](const fs::path& path) {
        rec::Pcm pcm;
        std::error_code e;
        if (path.empty() || !fs::is_regular_file(path, e)) { return pcm; }
        const auto decoded = LoadAlignedPcm(PathToU8(path));
        if (decoded && decoded->Frames() > 0) {
            pcm.Channels = decoded->Channels;
            pcm.Rate = decoded->Rate;
            pcm.Data = decoded->Samples;
        }

        return pcm;
    };
    std::vector<float> mix((size_t)std::max(1.0, Recorder.Duration * rec::MixRate) * 2, 0.0f);
    bool any = false;
    if (Recorder.SongStarted) {
        const rec::Pcm song = load(Recorder.SongPath);
        if (song.Frames() > 0) {
            rec::MixPcm(mix, song, Recorder.SongAt, Recorder.SongPos, Recorder.SongSpeed, Recorder.SongGain,
                Recorder.SongUntil >= 0 ? std::max(0.0, Recorder.SongUntil-Recorder.SongAt) : 1e9);
            any = true;
        }
    }
    rec::Pcm se[5];
    for (int k = 0; k < 5; ++k) { se[k] = load(Recorder.SePath[k]); }
    // 効果音: ゲームと同じく、同じ音が次に鳴ったら前の音は止める (全部を重ねると大きくなりすぎて曲が埋もれる)
    double next[5] = { 1e18, 1e18, 1e18, 1e18, 1e18 };
    std::vector<double> until(Recorder.SeLog.size(), 2.0);
    for (size_t i = Recorder.SeLog.size(); i-- > 0;) {
        const auto& [t, kind, gain] = Recorder.SeLog[i];
        if (kind < 0 || kind >= 5) { continue; }
        until[i] = std::clamp(next[kind] - t, 0.0, 2.0);
        next[kind] = t;
    }
    for (size_t i = 0; i < Recorder.SeLog.size(); ++i) {
        const auto& [t, kind, gain] = Recorder.SeLog[i];
        if (kind >= 0 && kind < 5 && se[kind].Frames() > 0) { rec::MixPcm(mix, se[kind], t, 0.0, 1.0, Recorder.SeGain*gain, until[i]); any = true; }
    }
    if (!any) { return fs::path(); }
    rec::LimitMixPeak(mix);
    const fs::path path = Recorder.AudioPath;
    if (!rec::WriteWav(path, mix)) { return fs::path(); }
    return path;
}

void Game::RecorderDecide(bool save) {
    Recorder.AwaitingDecision = false;
    if (Recorder.Muxer.joinable()) { Recorder.Muxer.join(); }
    const fs::path audio = save ? RecorderPrepareAudio() : fs::path();
    const std::string ext = rec::FindFormat(Config.RecordFormat).Ext;
    Recorder.OutPath = fs::path(Recorder.OutPath).replace_extension(U8ToPath(std::string(".") + ext));
    Recorder.Muxing = true;
    RecorderStatus(save ? "録画を書き出しています..." : "録画を破棄しています...", 600);
    Recorder.Muxer = std::thread([this, save, audio] {
        try {
        if (Recorder.Finisher.joinable()) { Recorder.Finisher.join(); }
        std::error_code ec;
        bool ok = false;
        if (save && Recorder.VideoOk) {
            ok = rec::Export(Recorder.Ffmpeg, Recorder.VideoPath, audio, Recorder.AudioDelay, Recorder.OutPath, rec::FindFormat(Config.RecordFormat).Ext);
            if (!ok && audio.empty() && rec::FindFormat(Config.RecordFormat).Video) {
                fs::rename(Recorder.VideoPath, fs::path(Recorder.OutPath).replace_extension(L".mp4"), ec);     // 音声なしの映像だけ残す
                ok = !ec;
            }
        }
        fs::remove(Recorder.VideoPath, ec);
        fs::remove(Recorder.AudioPath, ec);
        if (!save) { RecorderStatus("録画を中断しました (保存していません)", 4); }
        else {
            RecorderStatus(ok ? "録画を保存しました: recordings/" + PathToU8(Recorder.OutPath.filename()) + (Recorder.Dropped ? " (" + std::to_string(Recorder.Dropped) + " フレーム欠落)" : "")
                : std::string("録画の書き出しに失敗しました"));
        }
        } catch(...) { try { RecorderStatus("録画の書き出しに失敗しました"); } catch(...) {} }
        Recorder.Muxing = false;
    });
}

auto Game::RecorderPromptButton(int i) const -> UIRect {
    const float cx = Skin.Info.Resolution.X * 0.5f, y = Skin.Info.Resolution.Y * 0.5f + 10.0f;
    const float w = 180.0f, h = 44.0f;
    const float x = i == 0 ? cx - w - 10.0f : cx + 10.0f;
    return { x, y, x + w, y + h };
}

void Game::RecorderPromptProc() {
    if (!Recorder.Active && !Recorder.AwaitingDecision) { return; }
    if (NowScene != Scene::Result) { if (Recorder.AwaitingDecision) { RecorderDecide(true); } return; }
    const bool y = (__gotkeyboardinput['Y'] & 1) != 0, n = (__gotkeyboardinput['N'] & 1) != 0;
    int mx = 0, my = 0;
    sys::MousePos(mx, my);
    const bool click = (sys::MouseButtons() & sys::MouseLeft) != 0;
    static bool prevclick = true;
    const bool pressed = click && !prevclick;
    prevclick = click;
    if (y || (pressed && RecorderPromptButton(0).Contains(mx, my))) { RecorderFinishResult(true); }
    else if (n || (pressed && RecorderPromptButton(1).Contains(mx, my))) { RecorderFinishResult(false); }
}

void Game::RecorderFinishResult(bool save) {
    if (Recorder.Active) { RecorderStop(); }
    if (Recorder.AwaitingDecision) { RecorderDecide(save); }
}

void Game::RecorderOverlay() {
    const FontData& sfont = ConfigSmallFont();
    if ((Recorder.Active || Recorder.AwaitingDecision) && NowScene == Scene::Result) {
        const float cx = Skin.Info.Resolution.X * 0.5f, cy = Skin.Info.Resolution.Y * 0.5f;
        UIBox({ cx - 240.0f, cy - 60.0f, cx + 240.0f, cy + 70.0f }, gfx::Rgb(16, 18, 28), 230);
        UIBox({ cx - 240.0f, cy - 60.0f, cx + 240.0f, cy + 70.0f }, gfx::Rgb(255, 200, 40), 255, false);
        UIText(sfont, cx, cy - 30.0f, "録画を保存しますか? (" + std::string(rec::FindFormat(Config.RecordFormat).Ext) + ")", gfx::Rgb(255, 255, 255), 1);
        const char* labels[] = { "保存する (Y)", "破棄する (N)" };
        for (int i = 0; i < 2; ++i) {
            const UIRect r = RecorderPromptButton(i);
            UIBox(r, i == 0 ? gfx::Rgb(214, 116, 20) : gfx::Rgb(60, 64, 84), 240);
            UIText(sfont, (r.L + r.R) * 0.5f, (r.T + r.B) * 0.5f, labels[i], i == 0 ? gfx::Rgb(0, 0, 0) : gfx::Rgb(255, 255, 255), 1);
        }
    }
    if (Recorder.Active) {
        const float x = (float)Skin.Info.Resolution.X - 16.0f;
        gfx::Circle(x - ConfigTextWidth("REC") - 14.0f, 18.0f, 6.0f, 16, gfx::Rgb(240, 40, 40), true);
        UIText(sfont, x, 18.0f, "REC", gfx::Rgb(255, 255, 255), 2);
    }
    std::lock_guard lock(Recorder.StatusMutex);
    if (!Recorder.Status.empty() && std::chrono::steady_clock::now() < Recorder.StatusUntil && (Recorder.Muxing || !Recorder.Active)) {
        UIText(sfont, 16, (float)Skin.Info.Resolution.Y - sfont.Size * 1.2f, FitText(Recorder.Status, sfont.Handle, (float)Skin.Info.Resolution.X - 32, false), gfx::Rgb(255, 220, 90));
    }
}

void Game::RecorderAbort() {
    if (Recorder.Active) { Recorder.AbortRequested = true; }
}

#endif
#ifndef __ANDROID__
#else
void Game::RecorderAbort() {}

void Game::PlayHitSe(int kind,bool emphasized) {
    auto& se=Skin.Base->Playing.SE;
    const SoundData* bigSound=kind==0?&se.DonBig:kind==1?&se.KaBig:nullptr;
    const bool dedicated=emphasized&&bigSound&&bigSound->Handle!=-1;
    const int recordedKind=dedicated?(kind==0?3:4):kind;
    const auto& sound=dedicated?*bigSound:kind==0?se.Don:kind==1?se.Ka:se.Balloon;
    sound.Play();
    [[maybe_unused]] float gain=1.0f;
    if(emphasized && kind<2 && !dedicated) {
        const int layer=snd::Duplicate(sound.Handle);
        if(layer!=-1) {
            snd::SetVolume(layer, numeric::Cast<int>(std::clamp(Chart.OriginalData.SeVolume*Config.SEVolume*.0255,0.0,255.0)));
            if(snd::SetDeleteOnFinish(layer, true)<0 || snd::Play(layer, true, 0 == 1)<0) snd::Delete(layer);
            else gain=2.0f;
        }
    }
#ifndef __ANDROID__
    if(Recorder.Active) Recorder.SeLog.push_back({RecorderSeconds(),recordedKind,gain});
#endif
}

#endif
