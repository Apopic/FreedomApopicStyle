// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    void MultiRoomInit();
    void MultiRoomEnd();
    // 文字列を最大幅に収めて描く (収まらなければ末尾を省略)
    void FitDraw(FontData& font, Pos2D<float> pos, unsigned int color, unsigned int edge, const std::string& text, float maxwidth);
    // 待機室の名前の位置と幅: 箱 (PlayerBox) の中で、左側の王冠 (Crown) より右に収める
    float PlayerNameLeft() const;
    float PlayerNameRight() const;
    float PlayerLabelX() const;
    float PlayerLabelWidth() const;

    void MultiRoomDraw();
    void MultiRoomProc();
#endif // FAS_GAME_MEMBER_DECLARATIONS
