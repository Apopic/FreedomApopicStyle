// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    enum class Mode {
        Single,
        Multi,
        Dan,
        Edit,
        Config,
        End,
        Count
    } ModeSelector = Mode::Single;

    void ModeSelectInit();

    // モードの箱の間隔 (箱の高さの 1.25 倍まで詰め、画面に収まらなければさらに詰める)
    float ModeBoxDistance() const;
    // 箱の並び全体を画面の中央より少し上に置くための縦のずらし (スキンの位置からの差)
    float ModeBoxOffset() const;
    // ホイールで素早く動かした時に移動の音が重ならないよう、鳴らす間隔を空ける
    std::chrono::steady_clock::time_point WheelSoundAt{};
    bool WheelSoundReady();
    void ModeSelectDraw();

    void ModeSelectProc();
#endif // FAS_GAME_MEMBER_DECLARATIONS
