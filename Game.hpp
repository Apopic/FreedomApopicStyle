#pragma once
#include "SystemCommon.hpp"
#include "Library/PlayerCharAnimation.hpp"

// Game state and interface: member fragments preserve the original declaration order.
class Game {
private:
    friend struct GameTestAccess;
    // State belongs to the game instance, not to the process.
    _ConfigData Config;
    const _ConfigData DefaultConfig{ _ConfigData::DefaultsTag{} };
    _Skin Skin;

    bool CleanupDone=false;
public:
    ~Game() noexcept;
    void Cleanup() noexcept;
#define FAS_GAME_MEMBER_DECLARATIONS
#include "System/CoreStart.hpp"
#include "Scenes/Title.hpp"
#include "Scenes/ModeSelect.hpp"
#include "System/SongCatalog.hpp"
#include "Scenes/SongSelect.hpp"
#include "Scenes/DanSelect.hpp"
#include "System/Online.hpp"
#include "System/Discord.hpp"
#include "Scenes/MultiRoom.hpp"
#include "Scenes/Loading.hpp"
#include "Scenes/Playing.hpp"
#include "Scenes/Result.hpp"
#include "System/HitStatistics.hpp"
#include "System/ConfigRecommendations.hpp"
#include "Scenes/ConfigMenu.hpp"
#include "Scenes/Editor.hpp"
#include "System/Recorder.hpp"
#include "System/SongFilter.hpp"
#include "System/CoreLoop.hpp"
#undef FAS_GAME_MEMBER_DECLARATIONS
};
