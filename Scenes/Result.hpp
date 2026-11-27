// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    int ResultIndex = 0;
    int CrownIndex = 0;

    json ScoreCache;
    bool IsScoreCacheLoaded = false;

    const json& GetScoreCache();
    void ScoreDataSave(uint64_t score, int crown, const JudgeData* result = nullptr);
    ScoreData ScoreDataLoad(const std::string& chartpath, int courseindex);
    void ResultEnd();
    void ResultDraw();
    void ResultProc();
#endif // FAS_GAME_MEMBER_DECLARATIONS
