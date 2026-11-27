// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    // ===================================================================== おすすめ設定
    // この PC の画面 (リフレッシュレート)・CPU・GPU・既定の音声デバイスを調べて、表示・サウンド・録画の設定を決める。
    // 判定オフセットは、実際の演奏で測ったずれ (PersonalOffset) がたまっている時だけ決める。
    // 判定幅・演奏速度・譜面の速さ・演奏オプション・名前・キー・フォルダ・音量・スキンなど、好みで決めるものは変えない。

    struct DeviceInfo {
        int RefreshRate = 60;
        unsigned Threads = 4;
        bool AudioOk = false;
        int MixRate = 0;                // 共有モードのサンプリングレート
        int PeriodSamples = 0;          // 既定のデバイス周期 (サンプル数)
        int MinPeriodSamples = 0;       // 排他モードの最小周期 (サンプル数)
        int SharedMinPeriodSamples = 0; // 低遅延の共有モードの最小周期 (0: 対応しない)
        bool ExclusiveOk = false;       // 排他モードで Windows の設定と同じサンプリングレートを使える
        unsigned GpuVendor = 0;         // 既定の GPU の会社 (0x10DE: NVIDIA / 0x1002: AMD / 0x8086: Intel)
        std::string HwEncoder;          // 実際に使えたハードウェアエンコーダ (h264_nvenc など。無ければ空)
    };
    static DeviceInfo DetectDevice();

    struct RecommendChange {
        std::string Name, Before, After;
        bool Restart = false;
        std::function<void()> Apply;
    };
    std::vector<RecommendChange> BuildRecommendations();
    // 判定のずれの記録 (おすすめの判定オフセットに使う)。offset: そのずれを打ち消す判定オフセット、count: 打撃の数
    static constexpr int MinPersonalOffsetHits = 150;
    static constexpr int MaxPersonalOffsetHits = 3000;
    void AddPersonalOffsetSample(double offset, size_t count);
#endif // FAS_GAME_MEMBER_DECLARATIONS
