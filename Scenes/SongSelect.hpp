// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    void SongSelectInit();
    void SongSelectEnd();
    void SongSelectDraw();
    void SongSelectProc();
    GraphData SongSelectBoxLayout(int index) const;
    std::optional<int> HitTestSongSelectBox(float x, float y) const;
    float SongSelectBoxExtraHeight() const;
    int SongSelectMotionIndex = -1;
    int SongSelectMotionCount = 0;
    int SongSelectMoveStart = 0;
    int SongSelectSettledAt = -1;
    double SongSelectScrollPosition = 0;
    double SongSelectScrollFrom = 0;
    double SongSelectScrollTarget = 0;
    int SongSelectClosingSlot = 0;
    int SongSelectClosingStart = 0;
    float SongSelectClosingFrom = 0;
    float SongSelectClosingHeight = 0;
    float SongSelectOpenAmount = 0;
    int SongSelectWrapIndex(int slot) const;
    void UpdateSongSelectAnimation(int now);
    void SetSongSelectBox(int index, int steps = 0);
    int SongSelectTransitionStart = -1;
    bool SongSelectTransitionTarget = false;
    int SongSelectResultStart = 0;
    void BeginSongSelectTransition(bool courseSelect);
    void UpdateSongSelectTransition();
    float SongSelectContentOpacity() const;
    void DrawCourseBestResult(int course, float y, float contentOpacity);
    void DrawCourseHeading(FontData& font, Pos2D<float> position, const std::string& text);
#endif // FAS_GAME_MEMBER_DECLARATIONS
