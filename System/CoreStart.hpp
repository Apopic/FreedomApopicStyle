// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else


    enum class Scene {
        Null,
        End,
        Title,
        ModeSelect,
        SongSelect,
        DanSelect,
        MultiRoom,
        Loading,
        Playing,
        Result,
        ConfigMenu,
        Editor,
        Count
    };

    Scene NowScene = Scene::Title;
    Scene MemScene = Scene::Null;
    Scene PrevScene = Scene::Null;

#ifdef __ANDROID__
    TouchData Touch;
#endif

    bool IsDanSelect() const;
#endif // FAS_GAME_MEMBER_DECLARATIONS
