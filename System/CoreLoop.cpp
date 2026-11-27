#include "../Game.hpp"

bool Game::Init() {

    {
        std::error_code ec;
        fs::remove(GetExecutablePath() / "temp.tja", ec);
    }

#ifndef __ANDROID__
    // 初めての起動: この PC に合わせたおすすめ設定にしておく (音声デバイスはこの後で作るので、ここで決める)
    if (Config.FirstRun) {
        for (auto& change : BuildRecommendations()) {
            change.Apply();
        }
        Config.FirstRun = false;
        Config.Write();
    }
    // 複数の起動を許さない時は、名前付きミューテックスで 2 つ目の起動を止める
    if (!Config.MultiBoot) {
        static HANDLE single = CreateMutexA(nullptr, FALSE, "FreedomApopicStyle.SingleInstance");
        if (single && GetLastError() == ERROR_ALREADY_EXISTS) { return false; }
    }
#endif
    // ウィンドウはスキンを読むまで隠しておく (論理解像度はスキンの解像度。ウィンドウの大きさは最初は 1280×720)
    gfx::InitOptions window;
    window.Title = "FreedomApopicStyle";
    window.FullScreen = Config.FullScreen;
    window.VSync = Config.WaitVSync;
    window.UseGpuRenderer = Config.UseGpuRenderer;
    window.Hidden = true;
    window.IconResource = 101;      // タイトルバー・タスクバーのアイコン (FreedomApopicStyle.rc の IDI_ICON1)
    if (!gfx::Init(window) || !txt::Init()) {
        ShowErrorDialog(std::string("画面を初期化できませんでした。\n原因: ") + gfx::LastError());
        return false;
    }
    Input::GetInstance();       // ウィンドウの通知の受け取りを登録する
#ifndef __ANDROID__
    SoundDevice::GetInstance().SetDevice(
        Config.SoundDeviceType,
        Config.Exclusive,
        Config.BufferSize,
        Config.SampleRate
    );
    ApplyDarkWindowMode(((HWND)sys::NativeWindow()));
    // 背景動画は ffmpeg にデコードさせる (無ければ背景動画は表示しない)
    gfx::SetMoviePlayerPath(PathToU8(rec::FindFfmpeg(GetExecutablePath())));
#else
    snd::Init({ 48000, 0 });
#endif

    if (const int rate = gfx::RefreshRate(); rate > 0) { RefreshRate = rate; }
    BackFPS = RefreshRate;

#ifdef __ANDROID__

    JNIEnv* env;
    const ANativeActivity* NativeActivity;
    int AssetCopyState = 0;

    NativeActivity = GetNativeActivity();

    while (sys::PumpEvents() && AssetCopyState == 0) {
        gfx::Clear();

        if (NativeActivity->vm->AttachCurrentThreadAsDaemon(&env, NULL) != JNI_OK) {
            return false;
        }

        jclass jclass_MainActivity = env->GetObjectClass(NativeActivity->clazz);
        jfieldID jfieldID_mAssetCopyState = env->GetFieldID(jclass_MainActivity, "mAssetCopyState", "I");
        AssetCopyState = env->GetIntField(NativeActivity->clazz, jfieldID_mAssetCopyState);
        env->DeleteLocalRef(jclass_MainActivity);
        NativeActivity->vm->DetachCurrentThread();

        txt::Draw(txt::DefaultFont(), (float)(0), (float)(0), "スキンアセットの読み込み中", gfx::Rgb(255, 255, 255));
        gfx::Present();
    }
#endif
    Skin.Init();
    Skin.Load(Config.SkinName, Config);
    EnumChart(Config.SongDirectories);

#ifndef __ANDROID__
    gfx::SetWindowVisible(true);
    gfx::RaiseWindow();
    // 失敗した場合は従来のウィンドウメッセージによる入力判定で動作する
    RawKeyboard::GetInstance().Start(((HWND)sys::NativeWindow()));
    DiscordRpc::GetInstance().Start(DiscordAppId());
#else
    Touch.Geometry.Load(Skin, Config);
#endif
    return true;
}

void Game::UpdateSceneFade() {
    const auto now = std::chrono::steady_clock::now();
    if (Fade.PendingScene) {
        NowScene = MemScene;
        if (Fade.FullBlackAt && now - *Fade.FullBlackAt >= std::chrono::milliseconds(100)) {
            NowScene = *Fade.PendingScene;
            Fade.PendingScene.reset();
            Fade.FullBlackAt.reset();
        }
    }
    else if (NowScene != MemScene && MemScene != Scene::Null && MemScene != Scene::Loading
        && !(NowScene == Scene::Loading && PlayingBlackout.HideLoading)) {
        Fade.PendingScene = NowScene;
        Fade.StartedAt = now;
        Fade.FullBlackAt.reset();
        Fade.Timer.Reset();
        NowScene = MemScene;
        if (MemScene == Scene::Playing) { Chart.NowTime.Stop(); Chart.SongData.Stop(); }
    }
}

void Game::Draw() {

    FpsCounter.Tick();
    BackFPS = FpsCounter.Rate;
    FPSTimer.Start();
    DrawTimer.Start();
    struct DrawMeasurement {
        double& Average;
        libarrier::Timer& Timer;
        ~DrawMeasurement() { Average = (Average + Timer.GetElapsed().Second()) / 2; }
    } measurement{BackDrawTime, DrawTimer};

    UpdateSceneFade();
    if (Fade.PendingScene) {
        // Scene actions may already have cleared chart/editor data. Fade the
        // last presented frame instead of drawing an invalid previous scene.
        Fade.DrawPrevious(Skin);
        Fade.Draw(Skin);
        return;
    }
    if (NowScene != MemScene) {
        if (MemScene == Scene::Loading && NowScene != Scene::Playing) { PlayingBlackout = {}; }

        if (NowScene == Scene::Loading) { Fade.Timer.Reset(); }
        else { Fade.Timer.Start(); }

        switch (MemScene) {
        case Scene::SongSelect:
            SongSelectEnd();
            break;
        case Scene::DanSelect:
            DanSelectEnd();
            break;
        case Scene::MultiRoom:
            MultiRoomEnd();
            break;
        case Scene::ConfigMenu:
            ConfigMenuEnd();
            break;
        case Scene::Editor:
            EditorEnd();
            break;
        case Scene::Playing:
            PlayingEnd();
            break;
        case Scene::Result:
            ResultEnd();
            break;
        }

        MemScene = NowScene;
        // 一時的に有効にした次郎形式は、演奏・リザルトから離れたら元に戻す
        if (Config.JiroChartModeSaved && NowScene != Scene::Loading && NowScene != Scene::Playing && NowScene != Scene::Result) {
            Config.JiroChartMode = *Config.JiroChartModeSaved;
            Config.JiroChartModeSaved.reset();
        }

        switch (NowScene) {
        case Scene::Title:
            TitleInit();
            break;
        case Scene::ModeSelect:
            ModeSelectInit();
            break;
        case Scene::SongSelect:
            SongSelectInit();
            break;
        case Scene::DanSelect:
            DanSelectInit();
            break;
        case Scene::MultiRoom:
            MultiRoomInit();
            break;
        case Scene::Playing:
            PlayingInit();
            break;
        case Scene::ConfigMenu:
            ConfigMenuInit();
            break;
        case Scene::Editor:
            EditorInit();
            break;
        }
    }

    switch (NowScene) {
    case Scene::Title:
        TitleDraw();
        break;
    case Scene::ModeSelect:
        ModeSelectDraw();
        break;
    case Scene::SongSelect:
        SongSelectDraw();
        break;
    case Scene::DanSelect:
        DanSelectDraw();
        break;
    case Scene::MultiRoom:
        MultiRoomDraw();
        break;
    case Scene::Loading:
        LoadingDraw();
        break;
    case Scene::Playing:
        PlayingDraw();
        break;
    case Scene::Result:
        ResultDraw();
        break;
    case Scene::ConfigMenu:
        ConfigMenuDraw();
        break;
    case Scene::Editor:
        EditorDraw();
        break;
    }

#ifdef __ANDROID__
    if (NowScene != Scene::Editor && NowScene != Scene::SongSelect && NowScene != Scene::DanSelect) { Touch.Geometry.Draw(); }
#endif



    if (Config.ViewDebug) {
        if (!AverageTimer.IsRunning() || AverageTimer.GetElapsed().Second() > 0.5) {
            AverageTimer.Start();
            NowFPS = numeric::Cast<uint64_t>(BackFPS);
            NowDrawTime = BackDrawTime;
        }
        std::string renderer = gfx::BackendName();
        std::ranges::transform(renderer, renderer.begin(), [](unsigned char ch) { return (char)std::toupper(ch); });
        if (NowScene == Scene::Playing) {
            txt::DrawFormat(0, 0, gfx::Rgb(255, 255, 255), "FPS:%llu (max %.1fms)\nTPS:%.0f\nDrawTime:%.12lf\nRenderer:%s\nAudio:%s\nNowTime:%lf\nBPM:%lf", NowFPS, FpsCounter.LastMaxGapMs, NowTPS, NowDrawTime, renderer.c_str(), snd::DeviceDescription(), ChartNowTime(1) / Chart.SongSpeed, Chart.NowBPM * Chart.SongSpeed);
        }
        else {
            txt::DrawFormat(0, 0, gfx::Rgb(255, 255, 255), "FPS:%llu (max %.1fms)\nTPS:%.0f\nDrawTime:%.12lf\nRenderer:%s\nAudio:%s", NowFPS, FpsCounter.LastMaxGapMs, NowTPS, NowDrawTime, renderer.c_str(), snd::DeviceDescription());
        }
    }
    if (NowScene == Scene::Playing) { DrawPlayingBlackout(); }
    else if (NowScene == Scene::Loading) {
        gfx::SetBlend(gfx::Blend::None);
        gfx::Box((float)(0), (float)(0), (float)(Skin.Info.Resolution.X), (float)(Skin.Info.Resolution.Y), gfx::Rgb(0, 0, 0), true);
    }
    Fade.Draw(Skin);
    Fade.Capture(Skin);
}

#ifndef __ANDROID__
void Game::CheckRawInputAlive() {
    auto& raw = RawKeyboard::GetInstance();
    const int64_t now = RawKeyboard::Now();
    for (const auto& e : KeyEvents) {
        if (e.Down) { RawLastDownTick = std::max(RawLastDownTick, e.Time); }
    }
    const bool pressed = ConsumeNewKeyboardPress(__gotkeyboardinput, WndKeysPrev);
    if (!raw.IsActive() || !RawEventsSeen) {
        PendingWndDown = 0;
        return;
    }
    if (pressed && PendingWndDown == 0) {
        PendingWndDown = now;
    }
    if (PendingWndDown != 0) {
        if (RawLastDownTick >= PendingWndDown - RawKeyboard::MsToTicks(100)) {
            PendingWndDown = 0;     // Raw Input も届いている
        }
        else if (now - PendingWndDown > RawKeyboard::MsToTicks(150)) {
            PendingWndDown = 0;
            RawEventsSeen = false;  // ウィンドウのキー入力で判定する
            if (now - LastReregister > RawKeyboard::MsToTicks(2000)) {
                LastReregister = now;
                raw.Reregister();
            }
        }
    }
}

#endif
#ifdef __ANDROID__
void Game::AndroidCheckImport() {
    if (++ImportCheckFrame >= 30) {
        ImportCheckFrame = 0;
        const ANativeActivity* activity = GetNativeActivity();
        JNIEnv* env = nullptr;
        if (activity->vm->AttachCurrentThreadAsDaemon(&env, nullptr) == JNI_OK) {
            jclass cls = env->GetObjectClass(activity->clazz);
            if (jfieldID field = env->GetFieldID(cls, "ImportSerial", "I")) {
                const int serial = env->GetIntField(activity->clazz, field);
                if (serial != ImportSerialSeen) { ImportSerialSeen = serial; ImportPending = true; }
            }
            if (env->ExceptionCheck()) { env->ExceptionClear(); }
            env->DeleteLocalRef(cls);
        }
    }
    // 選曲中に一覧を作り直すと選んでいる箱が無くなるので、メニューの画面で行う
    if (ImportPending) {
        ImportPending = false;
        ChartReloadPending = true;
    }
}

#endif
void Game::Proc() {

#ifndef __ANDROID__
    RawKeyboard::GetInstance().Drain(KeyEvents);
    if (!RawEventsSeen && !KeyEvents.empty()) {
        // Raw Input が届いた (初めて、または止まった後に再開した): ウィンドウのキー入力で判定済みの打鍵を、もう一度判定しないよう捨てる
        RawEventsSeen = true;
        std::erase_if(KeyEvents, [&](const RawKeyboard::Event& e) { return e.Time <= LegacyHitTick; });
    }
    CheckRawInputAlive();
#endif
    OnlineUpdate();
    DiscordUpdate();
#ifdef __ANDROID__
    AndroidCheckImport();
#endif
    ChartWatchUpdate();
    SkinWatchUpdate();

    // TPS は元の計算方法 (前回の値と今回の間隔から求めた値の平均)。
    // ただし読み込みや画面の切り替え・ウィンドウ操作などで 1 回だけ長く止まった間隔は、処理速度ではないので数えない
    if (TPSTimer.IsRunning()) {
        const double dt = TPSTimer.GetElapsed().Second();
        if (dt > 0.0 && dt < TpsStallSeconds) {
            NowTPS = (NowTPS + 1.0 / dt) / 2.0;
        }
    }
    TPSTimer.Start();

#ifdef __ANDROID__
    Touch.ExternalPressed = AndroidPollExternalInput();
    Touch.Num = GetTouchInputDownLog(Touch.List, TOUCHINPUTPOINT_MAX);
#endif

    // A requested scene is committed only after a fully black frame and its hold.
    if (Fade.PendingScene || NowScene != MemScene) { return; }
    switch (NowScene) {
    case Scene::Title:
        TitleProc();
        break;
    case Scene::ModeSelect:
        ModeSelectProc();
        break;
    case Scene::SongSelect:
        SongSelectProc();
        break;
    case Scene::DanSelect:
        DanSelectProc();
        break;
    case Scene::MultiRoom:
        MultiRoomProc();
        break;
    case Scene::Loading:
        LoadingProc();
        break;
    case Scene::Playing:
        PlayingProc();
        break;
    case Scene::Result:
        ResultProc();
        break;
    case Scene::ConfigMenu:
        ConfigMenuProc();
        break;
    case Scene::Editor:
        EditorProc();
        break;
    }
}

void Game::_LogUpdate(const std::atomic_bool* endflag, const std::atomic_bool* stop) {
    // DxLib の WaitVSync は描画デバイスを参照する。別スレッドから呼ぶと
    // スキン切り替えの SetGraphMode によるデバイス再作成と競合する。
    // このスレッドは DxLib に触れず、Windows の合成タイミングだけを待つ。
    using Clock = std::chrono::steady_clock;
    const auto period = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / std::clamp(RefreshRate, 1, 1000)));
    while (!*endflag && !*stop) {
        const auto before = Clock::now();
#ifndef __ANDROID__
        const bool synchronized = SUCCEEDED(DwmFlush());
        // 合成が無効、または待機せず戻る環境でも空回りさせない。
        if (!synchronized || Clock::now() - before < period / 4)
#endif
        { std::this_thread::sleep_until(before + period); }
        if (*endflag || *stop) { break; }
        _waitvsyncLog = true;
    }
}

bool Game::GetVSyncWaitLog() {
    return _waitvsyncLog.exchange(false);
}

void Game::Main() {
    struct CleanupGuard { Game& GameRef; ~CleanupGuard() { GameRef.Cleanup(); } } cleanup{*this};

    if (!Init()) {
        return;
    }

    libarrier::Timer WaitTimer;
    WaitTimer.Start();
    uint64_t waittime = 0;
    bool drawflag = false;
    using FrameClock = std::chrono::steady_clock;
    auto nextFrame = FrameClock::now();
    int previousTarget = -1;
    auto frameReady = [&] {
        // 垂直同期の周期以下には既存の同期処理を優先する。
        const int target = Config.WaitVSync && Config.FrameRate >= RefreshRate ? 0 : Config.FrameRate;
        if (previousTarget != target) {
            previousTarget = target;
            nextFrame = FrameClock::now();
        }
        return previousTarget == 0 || FrameClock::now() >= nextFrame;
    };
    auto beginFrame = [&] {
        if (previousTarget <= 0) return;
        const auto period = std::chrono::duration_cast<FrameClock::duration>(std::chrono::duration<double>(1.0 / previousTarget));
        const auto now = FrameClock::now();
        // 遅れたフレームを一度に取り戻さず、微小な待機誤差だけを吸収する。
        const auto tolerance = std::min(period / 20, std::chrono::duration_cast<FrameClock::duration>(std::chrono::microseconds(500)));
        nextFrame = std::max(nextFrame + period, now + period - tolerance);
    };

    // Android NDK の libc++ でも使える、例外時に停止・join する監視スレッド。
    struct VSyncThread {
        std::atomic_bool Stop = false;
        std::thread Worker;
        VSyncThread(Game& game, const std::atomic_bool* end) : Worker([this, &game, end] { game._LogUpdate(end, &Stop); }) {}
        ~VSyncThread() { Stop = true; join(); }
        void join() { if (Worker.joinable()) { Worker.join(); } }
    } thd(*this, &EndFlag);

    while (true) {

        if (!sys::PumpEvents() || NowScene == Scene::End) {
            EndFlag = true;
        }
        if (EndFlag) {
            break;
        }
        if (Config.WaitVSync && Config.FastInput) {
            if (GetVSyncWaitLog() && drawflag) {
                WaitTimer.Start();
                waittime = (uint64_t)(1000000000 * Config.FastDrawRate / RefreshRate);
                drawflag = false;
            }
            Proc();
            if ((uint64_t)WaitTimer.GetElapsed().NanoSecond() > waittime && frameReady()) {
                beginFrame();
                Draw();
#ifndef __ANDROID__
                RecorderCapture();
                RecorderOverlay();
                RecorderPromptProc();
#endif
                gfx::Present();
                gfx::Clear();
                WaitTimer.Reset();
                drawflag = true;
            }
        }
        else {
            if (!frameReady()) {
                // 通常モードでは Proc と Draw の従来の順序・頻度を維持する。
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                continue;
            }
            beginFrame();
            Draw();
#ifndef __ANDROID__
            RecorderCapture();
            RecorderOverlay();
            RecorderPromptProc();
#endif
            Proc();
            gfx::Present();
            gfx::Clear();
        }
    }

    thd.join();
}

Game::~Game() noexcept { Cleanup(); }

void Game::Cleanup() noexcept {
    if(CleanupDone) return;
    CleanupDone=true;
    EndFlag=true;
    const auto attempt=[](auto&& action) noexcept { try { action(); } catch(...) {} };
    attempt([&]{ Fade.FrameGraph = {}; });
    attempt([&]{ OnlineDisconnect(); });
    // Workers must finish while all referenced game state still exists.
    if(Editor.StretchCancel) *Editor.StretchCancel=true;
    attempt([&]{ if(Editor.StretchTask.valid()) Editor.StretchTask.wait(); });
    attempt([&]{ if(Editor.WaveTask.valid()) Editor.WaveTask.wait(); });
    attempt([&]{ if(Editor.EstimateTask.valid()) Editor.EstimateTask.wait(); });
    attempt([&]{ if(ChartWatchTask.valid()) ChartWatchTask.wait(); });
    attempt([&]{ if(SkinWatchTask.valid()) SkinWatchTask.wait(); });
#ifndef __ANDROID__
    attempt([&]{ RecorderStop(); });
    attempt([&]{ if(Recorder.AwaitingDecision) RecorderDecide(true); });
    // Muxer may itself join Finisher: never join both concurrently.
    attempt([&]{ if(Recorder.Muxer.joinable()) Recorder.Muxer.join(); });
    attempt([&]{ if(Recorder.Finisher.joinable()) Recorder.Finisher.join(); });
    attempt([&]{ Recorder.Video.Finish(); });
    attempt([&]{ RecorderRelease(); });
    attempt([&]{ RawKeyboard::GetInstance().Stop(); });
    attempt([&]{ DiscordRpc::GetInstance().Stop(); });
#endif
    if(gfx::IsInit()) {
        attempt([&]{ EditorReleaseSong(); });
        attempt([&]{ Editor.IconCache.clear(); });
        attempt([&]{ EditorFontData.Delete(); });
        attempt([&]{ EditorPanelFontData.Delete(); });
        attempt([&]{ DemoSong.Delete(); });
        attempt([&]{ ConfigSmall.Delete(); });
        attempt([&]{ Skin.Dispose(); });
        attempt([&]{ snd::Shutdown(); });
        attempt([&]{ txt::Shutdown(); });
        attempt([&]{ gfx::Shutdown(); });
    } else {
        attempt([&]{ Skin.Dispose(); });
    }
}
