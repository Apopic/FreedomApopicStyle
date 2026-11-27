#pragma once
// SDL 版の実装どうしで使う (ゲームからは使わない)
#include "../Gfx.hpp"
#include <SDL3/SDL.h>

namespace gfx {
    struct TextureObject {
        SDL_Texture* T = nullptr;
        explicit TextureObject(SDL_Texture* t) : T(t) {}
        ~TextureObject() { if (T && gfx::IsInit()) { SDL_DestroyTexture(T); } }     // レンダラーを壊すとテクスチャも消えるので、その後は解放しない
        TextureObject(const TextureObject&) = delete;
        TextureObject& operator=(const TextureObject&) = delete;
    };
    namespace sdl {
        SDL_Window* Window();
        SDL_Renderer* Renderer();
        inline SDL_Texture* Raw(const Texture& t) { return t.Tex ? t.Tex->T : nullptr; }
        Texture FromSurface(SDL_Surface* surface);
    }
}
