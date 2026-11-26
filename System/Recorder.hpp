// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    // ===================================================================== 録画 (Library/PlayRecorder.hpp)
#ifndef __ANDROID__
    struct RecorderState {
        bool Active = false;
        rec::VideoWriter Video;
        fs::path Ffmpeg, VideoPath, AudioPath, OutPath;
        int Fps = 60, Width = 0, Height = 0;
        bool Bgra = false;
        ID3D11DeviceContext* Context = nullptr;
        ID3D11Texture2D* BackBuffer = nullptr;
        ID3D11Texture2D* Staging[2]{};
        ID3D11Texture2D* Resolve = nullptr;     // バックバッファがマルチサンプルの時
        int Cur = 0;
        bool Pending = false;                   // 前回コピーしたフレームをまだ読んでいない
        int PendingCount = 0;
        uint64_t Scheduled = 0;                 // 書くと決めたフレーム数
        std::chrono::steady_clock::time_point StartedAt{};
        std::thread Muxer;
        std::atomic_bool Muxing = false;
        std::mutex StatusMutex;
        std::string Status;
        std::chrono::steady_clock::time_point StatusUntil{};
        // 譜面から音声を作るための記録 (録画の開始からの秒)
        struct RecordedHit { double Time; int Kind; float Gain=1; };
        std::vector<RecordedHit> SeLog;      // 0: ドン 1: カッ 2: 風船 3: 大ドン 4: 大カッ
        bool SongStarted = false;
        double SongAt = 0, SongPos = 0, SongSpeed = 1.0, SongUntil = -1;
        float SongGain = 1, SeGain = 1;
        fs::path SongPath, SePath[5];
        double Duration = 0;
        // 保存の確認 (リザルトで聞く)
        std::thread Finisher;                   // 映像のエンコードの終わりを待つ
        std::atomic_bool VideoOk = false;
        bool AwaitingDecision = false;
        bool AbortRequested = false;            // 途中退出・リロード: 止めた時に保存しない
        double AudioDelay = 0;
        size_t Dropped = 0;
    } Recorder;

    double RecorderSeconds() const;
    // 演奏の効果音を鳴らす (録画中は時刻を記録して、譜面から音声を作る時に使う)
    void PlayHitSe(int kind,bool emphasized=false);
    void RecorderNoteSong(double posms);

    void RecorderStatus(std::string text, int seconds = 6);
    void RecorderRelease();
    void RecorderStart();
    void RecorderReadPending();
    // Draw の後、ScreenFlip の前に呼ぶ。録画のフレームレートの時刻を過ぎた時だけ GPU でコピーし、前回のコピーを読む
    void RecorderCapture();
    void RecorderStop();
    // 録画の音声の WAV を譜面から作る (曲 + 叩いた効果音)。作れなければ空
    fs::path RecorderPrepareAudio();
    // 保存する (true) / 破棄する (false)
    void RecorderDecide(bool save);
    // リザルトでの保存の確認 (Y: 保存 / N: 破棄、ボタンのクリックも可)。確認せずにリザルトを出たら保存する
    UIRect RecorderPromptButton(int i) const;
    void RecorderPromptProc();
    void RecorderFinishResult(bool save);
    // ScreenFlip の直前 (録画した後) に描くので、録画には映らない
    void RecorderOverlay();
    void RecorderAbort();
#else
    void RecorderAbort();
    void PlayHitSe(int kind,bool emphasized=false);
#endif
#endif // FAS_GAME_MEMBER_DECLARATIONS
