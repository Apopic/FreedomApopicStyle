// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    // ===================================================================== 判定のずれ (スコアメーター・オフセット自動調整)
    // ずれ = 叩いた時刻 - 音符の時刻 - 判定オフセット (ミリ秒。正 = 遅い、負 = 早い)。演奏中の表示だけに使い、記録はしない。
    // 判定オフセットの自動調整は、設定画面でテスト譜面を叩いて行う (オフセット自動調整)。

    struct HitErrorSummary {
        size_t Count = 0;
        double Mean = 0.0;
        double StdDev = 0.0;
    };
    struct HitErrorStats {
        static constexpr size_t MaxRecent = 48;
        static constexpr double RecentMs = 3000.0;
        std::vector<float> Play;            // この演奏の値 (段位は全曲分。保存はしない)
        struct Recent { float Error; std::chrono::steady_clock::time_point At; };
        std::deque<Recent> RecentHits;      // メーターに残す最近の打撃
        HitErrorSummary LastPlay;           // 直前の演奏 (リザルト表示用)
    } HitStats;

    static std::string FormatSigned(double v, int decimals);
    // 大きく外れた打撃 (可の判定幅の外) を除いて平均とばらつきを求める
    static HitErrorSummary SummarizeHitErrors(const std::vector<float>& errors, double limit);
    double HitErrorLimit() const;

    void RecordHitError(double error);
    void BeginPlayStats();
    void FinishPlayStats();

    // 判定のずれのメーター (中央 = ぴったり、左 = 早い、右 = 遅い)。
    // 色の帯は判定幅 (橙: 良 / 白: 可 / 青: 不可)、白い縦線は最近の打撃、▲ は平均、薄い帯はばらつき (±σ)
    void DrawHitErrorMeter(float cx, float cy, float width, const HitErrorSummary* summary, bool recent, const FontData* font);
    static std::string HitErrorText(const HitErrorSummary& sum);
#endif // FAS_GAME_MEMBER_DECLARATIONS
