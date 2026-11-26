// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    // ===================================================================== おすすめ設定
    // この PC の画面 (リフレッシュレート)・CPU・既定の音声デバイスに合わせて、表示とサウンドの設定を決める。
    // 演奏オプション・判定・オフセット・名前・キー・フォルダなどは変えない。

    struct DeviceInfo {
        int RefreshRate = 60;
        unsigned Threads = 4;
        bool AudioOk = false;
        int MixRate = 0;                // 共有モードのサンプリングレート
        int PeriodSamples = 0;          // 既定のデバイス周期 (サンプル数)
    };
    static DeviceInfo DetectDevice();

    struct RecommendChange {
        std::string Name, Before, After;
        bool Restart = false;
        std::function<void()> Apply;
    };
    std::vector<RecommendChange> BuildRecommendations();
#endif // FAS_GAME_MEMBER_DECLARATIONS
