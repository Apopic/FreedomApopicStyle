// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    double ScoreRateCalc(double judge, double basis);

    void SetSpeed();

	uint64_t AllNoteCounter(std::vector<std::string_view>& lines);

    void LoadingDraw();
    // 次郎の #MEASURE の値 (sscanf("%lf/%u") と同じ読み方)。読めない・分母 0・大きすぎる時は 1

    // 次郎のギミック譜面らしさを命令の値から判断する。理由 (空なら普通の譜面)
    // 負の #BPMCHANGE・#MEASURE、極端な #SCROLL・#MEASURE、負の #DELAY は、次郎の計算 (切り捨て・32 bit のあふれ) に頼ったギミックでよく使われる
    static double JiroMeasureValue(std::string_view arg);
    static bool RequiresJiroTiming(const std::vector<std::string_view>& lines);
    bool AskTemporaryJiroMode();
    void LoadingProc();
#endif // FAS_GAME_MEMBER_DECLARATIONS
