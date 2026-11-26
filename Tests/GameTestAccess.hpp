#pragma once
#include "../Game.hpp"

struct GameTestAccess {
    static _ConfigData& Config(Game& game) { return game.Config; }
    static _Skin& Skin(Game& game) { return game.Skin; }
};
