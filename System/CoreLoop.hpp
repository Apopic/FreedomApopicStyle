// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    std::atomic_bool EndFlag = false;

    libarrier::Timer AverageTimer;
    libarrier::Timer FPSTimer;
    double BackFPS = 60;
    uint64_t NowFPS = 0;

    libarrier::Timer TPSTimer;
    double NowTPS = 10000;
    static constexpr double TpsStallSeconds = 0.1;  // これより長い間隔は止まっていたものとして TPS に含めない

    // 0.5 秒ごとに回数を数えて FPS を求める
    struct RateCounter {
        uint64_t Count = 0;
        double MaxGapMs = 0;        // 区間内で最も長かった間隔
        std::chrono::steady_clock::time_point WindowStart = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point Last = std::chrono::steady_clock::now();
        double Rate = 0;
        double LastMaxGapMs = 0;
        void Tick() {
            const auto now = std::chrono::steady_clock::now();
            MaxGapMs = std::max(MaxGapMs, std::chrono::duration<double, std::milli>(now - Last).count());
            Last = now;
            ++Count;
            const double elapsed = std::chrono::duration<double>(now - WindowStart).count();
            if (elapsed >= 0.5) {
                Rate = Count / elapsed;
                LastMaxGapMs = MaxGapMs;
                Count = 0;
                MaxGapMs = 0;
                WindowStart = now;
            }
        }
    };
    RateCounter FpsCounter;

    libarrier::Timer DrawTimer;
    double BackDrawTime = 1;
    double NowDrawTime = 0;

    struct FadeData {
        libarrier::Timer Timer;
        static constexpr double Time = 0.25;
        std::optional<Scene> PendingScene;
        std::chrono::steady_clock::time_point StartedAt{};
        std::optional<std::chrono::steady_clock::time_point> FullBlackAt;
        gfx::Texture FrameGraph;    // 最後に表示した画面 (場面の切り替えでフェードする)
        void Capture(const _Skin& skin) {
            gfx::CaptureScreen(FrameGraph, skin.Info.Resolution.X, skin.Info.Resolution.Y);
        }
        void DrawPrevious(const _Skin& skin) {
            gfx::SetBlend(gfx::Blend::None);
            gfx::Box(0.0f, 0.0f, (float)skin.Info.Resolution.X, (float)skin.Info.Resolution.Y, gfx::Rgb(0, 0, 0), true);
            if (FrameGraph) { gfx::DrawTexture(FrameGraph, 0.0f, 0.0f, (float)skin.Info.Resolution.X, (float)skin.Info.Resolution.Y); }
        }
        void Draw(const _Skin& skin) {
            const auto now = std::chrono::steady_clock::now();
            if (!PendingScene && (!Timer.IsRunning() || Timer.GetElapsed().Second() > Time)) {
                Timer.Reset();
                return;
            }
            const double opacity = PendingScene
                ? std::clamp(std::chrono::duration<double>(now - StartedAt).count() / Time, 0.0, 1.0)
                : std::clamp(1 - Timer.GetElapsed().Second() / Time, 0.0, 1.0);
            gfx::SetBlend(gfx::Blend::Alpha, (int)(255 * opacity));
            gfx::Box((float)(0), (float)(0), (float)(skin.Info.Resolution.X), (float)(skin.Info.Resolution.Y), gfx::Rgb(0, 0, 0), true);
            gfx::SetBlend(gfx::Blend::None);
            if (PendingScene && opacity >= 1 && !FullBlackAt) { FullBlackAt = now; }
        }
    } Fade;

    void UpdateSceneFade();

    bool Init();

    void Draw();
#ifndef __ANDROID__
    // そのフレームで受け取った Raw Input のキーイベント (Proc の先頭で取り出す)
    std::vector<RawKeyboard::Event> KeyEvents;
    bool RawEventsSeen = false;     // Raw Input のキーイベントを受け取ったことがある
    int64_t LegacyHitTick = 0;      // ウィンドウのキー入力で最後に判定した時刻 (QPC)
    // Raw Input が途中で届かなくなったことの検出 (ウィンドウには押した通知が来たのに Raw Input が来ない)
    unsigned char WndKeysPrev[256]{};
    int64_t RawLastDownTick = 0;
    int64_t PendingWndDown = 0;     // まだ Raw Input で確かめられていない、ウィンドウでの押下時刻
    int64_t LastReregister = 0;
    int64_t LastHitTick = 0;
#endif

    // ウィンドウにはキーを押した通知が来たのに、少し待っても Raw Input のイベントが来ない時は、
    // Raw Input の受け取りを他に奪われた (Config の文字入力やダイアログでのフォーカスの切り替えで DxLib が登録し直すなど) とみなし、
    // ウィンドウのキー入力での判定に戻して登録し直す (届き始めたら Raw Input に戻る)
#ifndef __ANDROID__
    void CheckRawInputAlive();
#endif

#ifdef __ANDROID__
    // ファイル管理アプリから zip をインポートしたら (MainActivity.ImportSerial が増えたら)、譜面の一覧を作り直す
    int ImportSerialSeen = 0;
    int ImportCheckFrame = 0;
    bool ImportPending = false;
    void AndroidCheckImport();
#endif
    void Proc();

    // atomic なので mutex は不要
    std::atomic_bool _waitvsyncLog = false;
    void _LogUpdate(const std::atomic_bool* endflag, const std::atomic_bool* stop);
    bool GetVSyncWaitLog();

    void Main();
#endif // FAS_GAME_MEMBER_DECLARATIONS
