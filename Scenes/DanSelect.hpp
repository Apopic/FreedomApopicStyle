// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    void DanSelectInit();
    void DanSelectEnd();
    void DanSelectDraw();
    void DanSelectProc();
    std::vector<std::string> DanSelectDetails(const ChartData& chart) const;
#endif // FAS_GAME_MEMBER_DECLARATIONS
