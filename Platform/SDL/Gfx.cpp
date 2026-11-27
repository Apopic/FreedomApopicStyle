#include "SdlInternal.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <unordered_map>

namespace gfx {
    using sdl::Raw;
    namespace {
        struct State {
            SDL_Window* Window = nullptr;
            SDL_Renderer* Renderer = nullptr;
            int LogicalW = 1280, LogicalH = 720;
            Blend Mode = Blend::None;
            int Param = 255;
            int BrightR = 255, BrightG = 255, BrightB = 255;
            int AddR = 0, AddG = 0, AddB = 0;
            SDL_BlendMode SubMode = SDL_BLENDMODE_INVALID;
            Texture Canvas;                         // 描画先 (スキンの解像度)。Present でウィンドウに拡大して表示する
            const Texture* Target = nullptr;        // SetTarget で選んだ描画先 (nullptr なら Canvas)
            std::vector<SDL_Vertex> Vertices;      // 図形の頂点 (使い回して確保を減らす)
            std::vector<int> Indices;
        } S;

        SDL_BlendMode ToSdl(Blend mode) {
            switch (mode) {
            case Blend::Add: return SDL_BLENDMODE_ADD;
            case Blend::Mul: return SDL_BLENDMODE_MUL;
            case Blend::Sub:
                if (S.SubMode == SDL_BLENDMODE_INVALID) {
                    // 描画先 - 描く色 × アルファ
                    S.SubMode = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_REV_SUBTRACT,
                        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
                }
                return S.SubMode;
            default: return SDL_BLENDMODE_BLEND;
            }
        }
        float Alpha() { return (S.Mode == Blend::None ? 255 : std::clamp(S.Param, 0, 255)) / 255.0f; }
        SDL_FColor ToFColor(Color c, float alpha) {
            return { R(c) / 255.0f * S.BrightR / 255.0f, G(c) / 255.0f * S.BrightG / 255.0f, B(c) / 255.0f * S.BrightB / 255.0f, alpha };
        }
        // ウィンドウの中で画面用テクスチャを表示する位置 (縦横比を保って中央に)
        SDL_FRect PresentRect() {
            int ow = 0, oh = 0;
            SDL_GetRenderOutputSize(S.Renderer, &ow, &oh);
            const float scale = std::min((float)ow / S.LogicalW, (float)oh / S.LogicalH);
            const float w = S.LogicalW * scale, h = S.LogicalH * scale;
            return { (ow - w) * 0.5f, (oh - h) * 0.5f, w, h };
        }
        void BindTarget() {
            SDL_Texture* t = S.Target ? Raw(*S.Target) : Raw(S.Canvas);
            SDL_SetRenderTarget(S.Renderer, t);
        }
        // 画像の形 (アルファ) を明るさにした画像 (RGB = アルファ)。DxLib の SetDrawAddColor (画像の色に足す) を、
        // これに足す色を掛けて加算で重ねて再現する (結果 = 描画先 + 足す色 × アルファ。画像の色 + 足す色が 255 を超えない時は DxLib と同じ値)。
        // 画像ごとに初めて使う時に 1 回だけ作る
        // White: RGB = アルファ。Headroom: RGB = (1 - 色) × アルファ (あとどれだけ明るくできるか)
        struct Silhouette { std::weak_ptr<TextureObject> Source; std::shared_ptr<SDL_Texture> White, Headroom; };
        Texture AddScratch;     // 色を足す時の作業用 (画面用テクスチャと同じ大きさ)
        std::unordered_map<SDL_Texture*, Silhouette> Silhouettes;
        SDL_Texture* WhiteOf(const std::shared_ptr<TextureObject>& tex) {
            if (auto it = Silhouettes.find(tex->T); it != Silhouettes.end()) {
                if (!it->second.Source.expired() && it->second.Source.lock() == tex) { return it->second.White.get(); }
                Silhouettes.erase(it);
            }
            if (Silhouettes.size() > 256) { std::erase_if(Silhouettes, [](const auto& e) { return e.second.Source.expired(); }); }
            float w = 0, h = 0;
            SDL_GetTextureSize(tex->T, &w, &h);
            SDL_Texture* white = SDL_CreateTexture(S.Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, (int)w, (int)h);
            if (!white) { return nullptr; }
            SDL_Texture* previous = SDL_GetRenderTarget(S.Renderer);
            SDL_Rect clip{}; const bool clipped = SDL_RenderClipEnabled(S.Renderer); if (clipped) { SDL_GetRenderClipRect(S.Renderer, &clip); }
            SDL_SetRenderTarget(S.Renderer, white);
            SDL_SetRenderClipRect(S.Renderer, nullptr);
            SDL_SetRenderDrawColor(S.Renderer, 255, 255, 255, 0);
            SDL_RenderClear(S.Renderer);
            // 色は描画先 (白) のまま、アルファだけを画像から写す
            const SDL_BlendMode copyAlpha = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD,
                SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ZERO, SDL_BLENDOPERATION_ADD);
            SDL_SetTextureBlendMode(tex->T, copyAlpha);
            SDL_SetTextureAlphaMod(tex->T, 255);
            SDL_SetTextureColorMod(tex->T, 255, 255, 255);
            SDL_RenderTexture(S.Renderer, tex->T, nullptr, nullptr);
            // 白 (アルファ付き) を黒い画像に重ねて、RGB = アルファ の画像にする
            SDL_Texture* mask = SDL_CreateTexture(S.Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, (int)w, (int)h);
            if (mask) {
                SDL_SetRenderTarget(S.Renderer, mask);
                SDL_SetRenderDrawColor(S.Renderer, 0, 0, 0, 255);
                SDL_RenderClear(S.Renderer);
                SDL_SetTextureBlendMode(white, SDL_BLENDMODE_BLEND);
                SDL_RenderTexture(S.Renderer, white, nullptr, nullptr);
                SDL_DestroyTexture(white);
                white = mask;
            }
            // Headroom = White - 色 × アルファ = (1 - 色) × アルファ
            SDL_Texture* head = SDL_CreateTexture(S.Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, (int)w, (int)h);
            if (head) {
                SDL_SetRenderTarget(S.Renderer, head);
                SDL_SetTextureBlendMode(white, SDL_BLENDMODE_NONE);
                SDL_RenderTexture(S.Renderer, white, nullptr, nullptr);
                SDL_SetTextureBlendMode(tex->T, SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_REV_SUBTRACT,
                    SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD));
                SDL_RenderTexture(S.Renderer, tex->T, nullptr, nullptr);
                SDL_SetTextureScaleMode(head, SDL_SCALEMODE_LINEAR);
            }
            SDL_SetRenderTarget(S.Renderer, previous);
            if (clipped) { SDL_SetRenderClipRect(S.Renderer, &clip); }

            SDL_SetTextureScaleMode(white, SDL_SCALEMODE_LINEAR);
            auto& e = Silhouettes[tex->T];
            e.Source = tex;
            e.White = std::shared_ptr<SDL_Texture>(white, SDL_DestroyTexture);
            e.Headroom = head ? std::shared_ptr<SDL_Texture>(head, SDL_DestroyTexture) : nullptr;
            return white;
        }
        SDL_Texture* HeadroomOf(const std::shared_ptr<TextureObject>& tex) {
            if (!WhiteOf(tex)) { return nullptr; }
            auto it = Silhouettes.find(tex->T);
            return it != Silhouettes.end() ? it->second.Headroom.get() : nullptr;
        }
        void ApplyTexture(SDL_Texture* tex, int r, int g, int b, Uint8 alpha, SDL_BlendMode mode) {
            SDL_SetTextureBlendMode(tex, mode);
            SDL_SetTextureAlphaMod(tex, alpha);
            SDL_SetTextureColorMod(tex, (Uint8)r, (Uint8)g, (Uint8)b);
        }
        void Flush(SDL_Texture* tex = nullptr) {
            if (S.Vertices.empty()) { return; }
            if (!tex) { SDL_SetRenderDrawBlendMode(S.Renderer, ToSdl(S.Mode)); }
            SDL_RenderGeometry(S.Renderer, tex, S.Vertices.data(), (int)S.Vertices.size(), S.Indices.empty() ? nullptr : S.Indices.data(), (int)S.Indices.size());
            S.Vertices.clear();
            S.Indices.clear();
        }
        int AddVertex(float x, float y, SDL_FColor c, float u = 0, float v = 0) {
            S.Vertices.push_back({ { x, y }, c, { u, v } });
            return (int)S.Vertices.size() - 1;
        }
        void AddTri(int a, int b, int c) { S.Indices.push_back(a); S.Indices.push_back(b); S.Indices.push_back(c); }
        // 線分の帯 (幅 thickness) と、外側 1px の薄い縁
        void Strip(float x1, float y1, float x2, float y2, float thickness, SDL_FColor c) {
            const float dx = x2 - x1, dy = y2 - y1, len = std::sqrt(dx * dx + dy * dy);
            if (len <= 0.0001f) { return; }
            const float nx = -dy / len, ny = dx / len, h = std::max(thickness, 1.0f) * 0.5f, f = h + 1.0f;
            SDL_FColor edge = c; edge.a = 0;
            const int o0 = AddVertex(x1 + nx * f, y1 + ny * f, edge), i0 = AddVertex(x1 + nx * h, y1 + ny * h, c);
            const int i1 = AddVertex(x1 - nx * h, y1 - ny * h, c), o1 = AddVertex(x1 - nx * f, y1 - ny * f, edge);
            const int o2 = AddVertex(x2 + nx * f, y2 + ny * f, edge), i2 = AddVertex(x2 + nx * h, y2 + ny * h, c);
            const int i3 = AddVertex(x2 - nx * h, y2 - ny * h, c), o3 = AddVertex(x2 - nx * f, y2 - ny * f, edge);
            AddTri(o0, i0, o2); AddTri(i0, i2, o2);
            AddTri(i0, i1, i2); AddTri(i1, i3, i2);
            AddTri(i1, o1, i3); AddTri(o1, o3, i3);
        }
        void DrawTextureVerts(const Texture& t, const float* xy) {
            if (!t) { return; }
            float tw = 0, th = 0;
            SDL_GetTextureSize(Raw(t), &tw, &th);
            if (tw <= 0 || th <= 0) { return; }
            const float u0 = t.Src.x / tw, v0 = t.Src.y / th, u1 = (t.Src.x + t.Src.w) / tw, v1 = (t.Src.y + t.Src.h) / th;
            const Uint8 alpha = (Uint8)std::lround(Alpha() * 255.0f);
            auto submit = [&](SDL_Texture* tex, int r, int g, int b, Uint8 a, SDL_BlendMode mode) {
                // SDL_RenderGeometry はテクスチャの色・アルファの倍率を使わず頂点の色を掛けるので、倍率は頂点の色に入れる
                ApplyTexture(tex, 255, 255, 255, 255, mode);
                const SDL_FColor c{ r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
                const int a0 = AddVertex(xy[0], xy[1], c, u0, v0), a1 = AddVertex(xy[2], xy[3], c, u1, v0);
                const int a2 = AddVertex(xy[4], xy[5], c, u1, v1), a3 = AddVertex(xy[6], xy[7], c, u0, v1);
                AddTri(a0, a1, a2); AddTri(a0, a2, a3);
                Flush(tex);
            };
            submit(Raw(t), S.BrightR, S.BrightG, S.BrightB, alpha, ToSdl(S.Mode));
            if (S.AddR || S.AddG || S.AddB) {
                SDL_Texture* w = WhiteOf(t.Tex);
                SDL_Texture* head = HeadroomOf(t.Tex);
                if (w && head && S.Canvas) {
                    // DxLib と同じ min(色 + 足す色, 1) × アルファ にする。足す量 = min(足す色 × アルファ, (1 - 色) × アルファ) を
                    // 作業用の画像で求めてから (最小値の合成)、描画先に加える。半透明で描く時は両方に同じ割合を掛ける
                    if (!AddScratch || (int)AddScratch.Width() != S.LogicalW || (int)AddScratch.Height() != S.LogicalH) {
                        AddScratch = CreateTexture(S.LogicalW, S.LogicalH, true);
                    }
                    if (AddScratch) {
                        SDL_Texture* target = SDL_GetRenderTarget(S.Renderer);
                        SDL_Rect clip{}; const bool clipped = SDL_RenderClipEnabled(S.Renderer); if (clipped) { SDL_GetRenderClipRect(S.Renderer, &clip); }
                        SDL_SetRenderTarget(S.Renderer, Raw(AddScratch));
                        // 描画範囲は描画先ごとに残るので、毎回そろえる (前に使った範囲が残っていると一部だけ明るくなる)
                        SDL_SetRenderClipRect(S.Renderer, clipped ? &clip : nullptr);
                        SDL_SetRenderDrawColor(S.Renderer, 0, 0, 0, 0);
                        SDL_RenderClear(S.Renderer);
                        submit(head, alpha, alpha, alpha, 255, SDL_BLENDMODE_NONE);
                        submit(w, S.AddR * alpha / 255, S.AddG * alpha / 255, S.AddB * alpha / 255, 255,
                            SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_MINIMUM, SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_MINIMUM));
                        SDL_SetRenderTarget(S.Renderer, target);
                        if (clipped) { SDL_SetRenderClipRect(S.Renderer, &clip); }
                        SDL_SetTextureBlendMode(Raw(AddScratch), SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD,
                            SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD));
                        SDL_SetTextureColorMod(Raw(AddScratch), 255, 255, 255);
                        SDL_SetTextureAlphaMod(Raw(AddScratch), 255);
                        SDL_RenderTexture(S.Renderer, Raw(AddScratch), nullptr, nullptr);
                    }
                }
            }
        }
    }

    bool Init(const InitOptions& o) {
        if (S.Renderer) { return true; }
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) { return false; }
        SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
        if (o.Resizable) { flags |= SDL_WINDOW_RESIZABLE; }
        if (o.FullScreen) { flags |= SDL_WINDOW_FULLSCREEN; }
        if (o.Hidden) { flags |= SDL_WINDOW_HIDDEN; }
        S.Window = SDL_CreateWindow(o.Title.c_str(), o.Width, o.Height, flags);
        if (!S.Window) { return false; }
        if (o.UseGpuRenderer) { S.Renderer = SDL_CreateRenderer(S.Window, SDL_GPU_RENDERER); }
        if (!S.Renderer) { S.Renderer = SDL_CreateRenderer(S.Window, nullptr); }
        if (!S.Renderer) { SDL_DestroyWindow(S.Window); S.Window = nullptr; return false; }
        SetVSync(o.VSync);
        SetLogicalSize(o.Width, o.Height);
        S.Vertices.reserve(4096);
        S.Indices.reserve(8192);
        return true;
    }
    void Shutdown() {
        Silhouettes.clear();
        AddScratch = {};
        if (S.Renderer) { SDL_DestroyRenderer(S.Renderer); S.Renderer = nullptr; }
        if (S.Window) { SDL_DestroyWindow(S.Window); S.Window = nullptr; }
    }
    bool IsInit() { return S.Renderer != nullptr; }
    SDL_Window* sdl::Window() { return S.Window; }
    SDL_Renderer* sdl::Renderer() { return S.Renderer; }
    std::string LastError() { return SDL_GetError(); }
    const char* BackendName() { const char* n = S.Renderer ? SDL_GetRendererName(S.Renderer) : nullptr; return n ? n : "unknown"; }
    int RefreshRate() {
        const SDL_DisplayMode* mode = S.Window ? SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(S.Window)) : nullptr;
        return mode && mode->refresh_rate > 0 ? (int)std::lround(mode->refresh_rate) : 0;
    }
    void RaiseWindow() { if (S.Window) { SDL_RaiseWindow(S.Window); } }

    void SetLogicalSize(int width, int height) {
        width = std::max(width, 1);
        height = std::max(height, 1);
        if (S.Canvas && width == S.LogicalW && height == S.LogicalH) { return; }
        S.LogicalW = width;
        S.LogicalH = height;
        if (!S.Renderer) { return; }
        SDL_SetRenderTarget(S.Renderer, nullptr);
        S.Canvas = CreateTexture(width, height, true);
        if (S.Canvas) { SDL_SetTextureBlendMode(Raw(S.Canvas), SDL_BLENDMODE_NONE); }
        BindTarget();
    }
    int LogicalWidth() { return S.LogicalW; }
    int LogicalHeight() { return S.LogicalH; }
    void SetVSync(bool on) { if (S.Renderer) { SDL_SetRenderVSync(S.Renderer, on ? 1 : SDL_RENDERER_VSYNC_DISABLED); } }
    void SetFullScreen(bool on) { if (S.Window) { SDL_SetWindowFullscreen(S.Window, on); } }
    bool IsFullScreen() { return S.Window && (SDL_GetWindowFlags(S.Window) & SDL_WINDOW_FULLSCREEN) != 0; }
    void SetWindowVisible(bool visible) { if (S.Window) { visible ? SDL_ShowWindow(S.Window) : SDL_HideWindow(S.Window); } }
    void SetWindowTitle(const char* title) { if (S.Window) { SDL_SetWindowTitle(S.Window, title); } }
    void WindowToLogical(float wx, float wy, float& lx, float& ly) {
        lx = wx; ly = wy;
        if (!S.Renderer || !S.Window) { return; }
        int ww = 0, wh = 0, pw = 0, ph = 0;
        SDL_GetWindowSize(S.Window, &ww, &wh);
        SDL_GetWindowSizeInPixels(S.Window, &pw, &ph);
        const float px = ww > 0 ? wx * pw / ww : wx, py = wh > 0 ? wy * ph / wh : wy;
        const SDL_FRect r = PresentRect();
        lx = r.w > 0 ? (px - r.x) * S.LogicalW / r.w : px;
        ly = r.h > 0 ? (py - r.y) * S.LogicalH / r.h : py;
    }
    void LogicalToWindow(float lx, float ly, float& wx, float& wy) {
        wx = lx; wy = ly;
        if (!S.Renderer || !S.Window) { return; }
        int ww = 0, wh = 0, pw = 0, ph = 0;
        SDL_GetWindowSize(S.Window, &ww, &wh);
        SDL_GetWindowSizeInPixels(S.Window, &pw, &ph);
        const SDL_FRect r = PresentRect();
        const float px = r.x + lx * r.w / S.LogicalW, py = r.y + ly * r.h / S.LogicalH;
        wx = pw > 0 ? px * ww / pw : px;
        wy = ph > 0 ? py * wh / ph : py;
    }

    void Clear(Color color, int alpha) {
        SDL_SetRenderDrawColor(S.Renderer, (Uint8)R(color), (Uint8)G(color), (Uint8)B(color), (Uint8)std::clamp(alpha, 0, 255));
        SDL_RenderClear(S.Renderer);
    }
    void Present() {
        // 画面用テクスチャをウィンドウに縦横比を保って拡大して表示する (余白は黒)
        SDL_SetRenderTarget(S.Renderer, nullptr);
        SDL_SetRenderClipRect(S.Renderer, nullptr);
        SDL_SetRenderDrawColor(S.Renderer, 0, 0, 0, 255);
        SDL_RenderClear(S.Renderer);
        if (S.Canvas) {
            const SDL_FRect dst = PresentRect();
            SDL_RenderTexture(S.Renderer, Raw(S.Canvas), nullptr, &dst);
        }
        SDL_RenderPresent(S.Renderer);
        BindTarget();
    }

    void SetBlend(Blend mode, int param) { S.Mode = mode; S.Param = std::clamp(param, 0, 255); }
    Blend CurrentBlend() { return S.Mode; }
    int CurrentBlendParam() { return S.Param; }
    void SetClip(int left, int top, int right, int bottom) {
        const SDL_Rect r{ left, top, std::max(right - left, 0), std::max(bottom - top, 0) };
        SDL_SetRenderClipRect(S.Renderer, &r);
    }
    void ResetClip() { SDL_SetRenderClipRect(S.Renderer, nullptr); }
    bool GetClip(int& left, int& top, int& right, int& bottom) {
        SDL_Rect r{};
        const bool on = S.Renderer && SDL_RenderClipEnabled(S.Renderer);
        if (on) { SDL_GetRenderClipRect(S.Renderer, &r); }
        left = r.x; top = r.y; right = r.x + r.w; bottom = r.y + r.h;
        return on;
    }
    void SetBright(int r, int g, int b) { S.BrightR = std::clamp(r, 0, 255); S.BrightG = std::clamp(g, 0, 255); S.BrightB = std::clamp(b, 0, 255); }
    void GetBright(int& r, int& g, int& b) { r = S.BrightR; g = S.BrightG; b = S.BrightB; }
    void SetAddColor(int r, int g, int b) { S.AddR = std::clamp(r, 0, 255); S.AddG = std::clamp(g, 0, 255); S.AddB = std::clamp(b, 0, 255); }

    void Line(float x1, float y1, float x2, float y2, Color color, float thickness) {
        Strip(x1, y1, x2, y2, thickness, ToFColor(color, Alpha()));
        Flush();
    }
    void Box(float l, float t, float r, float b, Color color, bool fill, float thickness) {
        if (r < l) { std::swap(l, r); }
        if (b < t) { std::swap(t, b); }
        const SDL_FColor c = ToFColor(color, Alpha());
        if (fill) {
            const int a = AddVertex(l, t, c), b1 = AddVertex(r, t, c), c1 = AddVertex(r, b, c), d = AddVertex(l, b, c);
            AddTri(a, b1, c1); AddTri(a, c1, d);
        }
        else {
            const float h = thickness * 0.5f;
            Strip(l - h, t, r + h, t, thickness, c); Strip(r, t, r, b, thickness, c);
            Strip(r + h, b, l - h, b, thickness, c); Strip(l, b, l, t, thickness, c);
        }
        Flush();
    }
    void Oval(float cx, float cy, float rx, float ry, int segments, Color color, bool fill, float thickness) {
        segments = std::clamp(segments, 8, 256);
        const SDL_FColor c = ToFColor(color, Alpha());
        SDL_FColor edge = c; edge.a = 0;
        constexpr float pi2 = 6.28318530718f;
        if (fill) {
            const int center = AddVertex(cx, cy, c);
            int prevIn = -1, prevOut = -1, firstIn = -1, firstOut = -1;
            for (int i = 0; i <= segments; ++i) {
                const float a = pi2 * i / segments, ca = std::cos(a), sa = std::sin(a);
                const int in = i == segments ? firstIn : AddVertex(cx + ca * rx, cy + sa * ry, c);
                const int out = i == segments ? firstOut : AddVertex(cx + ca * (rx + 1), cy + sa * (ry + 1), edge);
                if (i == 0) { firstIn = in; firstOut = out; }
                else { AddTri(center, prevIn, in); AddTri(prevIn, prevOut, out); AddTri(prevIn, out, in); }
                prevIn = in; prevOut = out;
            }
        }
        else {
            for (int i = 0; i < segments; ++i) {
                const float a0 = pi2 * i / segments, a1 = pi2 * (i + 1) / segments;
                Strip(cx + std::cos(a0) * rx, cy + std::sin(a0) * ry, cx + std::cos(a1) * rx, cy + std::sin(a1) * ry, thickness, c);
            }
        }
        Flush();
    }
    void Circle(float cx, float cy, float r, int segments, Color color, bool fill, float thickness) { Oval(cx, cy, r, r, segments, color, fill, thickness); }
    void Triangle(float x1, float y1, float x2, float y2, float x3, float y3, Color color, bool fill) {
        const SDL_FColor c = ToFColor(color, Alpha());
        if (fill) { AddTri(AddVertex(x1, y1, c), AddVertex(x2, y2, c), AddVertex(x3, y3, c)); }
        else { Strip(x1, y1, x2, y2, 1, c); Strip(x2, y2, x3, y3, 1, c); Strip(x3, y3, x1, y1, 1, c); }
        Flush();
    }
    void Pixel(float x, float y, Color color) { Box(x, y, x + 1, y + 1, color, true); }

    Texture sdl::FromSurface(SDL_Surface* surface) {
        Texture t;
        if (!surface || !S.Renderer) { return t; }
        SDL_Texture* tex = SDL_CreateTextureFromSurface(S.Renderer, surface);
        if (!tex) { return t; }
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
        t.Tex = std::make_shared<TextureObject>(tex);
        t.Src = { 0, 0, (float)surface->w, (float)surface->h };
        return t;
    }
    Texture LoadTexture(const std::string& path) {
        SDL_Surface* s = IMG_Load(path.c_str());
        Texture t = sdl::FromSurface(s);
        if (s) { SDL_DestroySurface(s); }
        return t;
    }
    Texture LoadTextureFromMemory(const void* data, size_t size) {
        SDL_IOStream* io = SDL_IOFromConstMem(data, size);
        SDL_Surface* s = io ? IMG_Load_IO(io, true) : nullptr;
        Texture t = sdl::FromSurface(s);
        if (s) { SDL_DestroySurface(s); }
        return t;
    }
    std::vector<Texture> Divide(const Texture& source, int nx, int ny, float cw, float ch) {
        std::vector<Texture> out;
        if (!source || nx <= 0 || ny <= 0) { return out; }
        out.reserve((size_t)nx * ny);
        for (int y = 0; y < ny; ++y) {
            for (int x = 0; x < nx; ++x) { out.push_back(SubTexture(source, source.Src.x + cw * x, source.Src.y + ch * y, cw, ch)); }
        }
        return out;
    }
    std::vector<Texture> LoadDivTexture(const std::string& path, int nx, int ny, float cw, float ch) { return Divide(LoadTexture(path), nx, ny, cw, ch); }
    Texture SubTexture(const Texture& source, float x, float y, float w, float h) {
        Texture t = source;
        t.Src = { x, y, w, h };
        return t;
    }
    bool ImageSize(const std::string& path, int& width, int& height) {
        width = height = 0;
        SDL_Surface* s = IMG_Load(path.c_str());
        if (!s) { return false; }
        width = s->w; height = s->h;
        SDL_DestroySurface(s);
        return width > 0 && height > 0;
    }
    std::vector<uint32_t> LoadPixels(const std::string& path, int& width, int& height) {
        width = height = 0;
        std::vector<uint32_t> out;
        SDL_Surface* s = IMG_Load(path.c_str());
        if (!s) { return out; }
        SDL_Surface* c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(s);
        if (!c) { return out; }
        width = c->w; height = c->h;
        out.resize((size_t)width * height);
        for (int y = 0; y < height; ++y) { std::memcpy(out.data() + (size_t)y * width, (const uint8_t*)c->pixels + (size_t)c->pitch * y, (size_t)width * 4); }
        SDL_DestroySurface(c);
        return out;
    }
    Texture CreateTexture(int width, int height, bool target) {
        Texture t;
        SDL_Texture* tex = SDL_CreateTexture(S.Renderer, SDL_PIXELFORMAT_ARGB8888, target ? SDL_TEXTUREACCESS_TARGET : SDL_TEXTUREACCESS_STREAMING, width, height);
        if (!tex) { return t; }
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        t.Tex = std::make_shared<TextureObject>(tex);
        t.Src = { 0, 0, (float)width, (float)height };
        return t;
    }
    void UpdateTexture(const Texture& texture, const void* pixels, int pitch) { if (texture) { SDL_UpdateTexture(Raw(texture), nullptr, pixels, pitch); } }

    void DrawTexture(const Texture& t, float l, float tp, float r, float b) {
        const float xy[8] = { l, tp, r, tp, r, b, l, b };
        DrawTextureVerts(t, xy);
    }
    void DrawTextureRect(const Texture& t, float x, float y, float sx, float sy, float sw, float sh) {
        if (!t) { return; }
        DrawTexture(SubTexture(t, t.Src.x + sx, t.Src.y + sy, sw, sh), x, y, x + sw, y + sh);
    }
    void DrawTexturePart(const Texture& t, float l, float tp, float r, float b, float sx, float sy, float sw, float sh) {
        if (!t) { return; }
        DrawTexture(SubTexture(t, t.Src.x + sx, t.Src.y + sy, sw, sh), l, tp, r, b);
    }
    void DrawTextureQuad(const Texture& t, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4) {
        const float xy[8] = { x1, y1, x2, y2, x3, y3, x4, y4 };
        DrawTextureVerts(t, xy);
    }
    void DrawTextureRotated(const Texture& t, float cx, float cy, float scale, float angle, bool flipx) {
        if (!t) { return; }
        const float hw = t.Src.w * scale * 0.5f * (flipx ? -1.0f : 1.0f), hh = t.Src.h * scale * 0.5f;
        const float c = std::cos(angle), s = std::sin(angle);
        auto px = [&](float x, float y) { return cx + x * c - y * s; };
        auto py = [&](float x, float y) { return cy + x * s + y * c; };
        DrawTextureQuad(t, px(-hw, -hh), py(-hw, -hh), px(hw, -hh), py(hw, -hh), px(hw, hh), py(hw, hh), px(-hw, hh), py(-hw, hh));
    }

    void SetTarget(const Texture* texture) { S.Target = texture; BindTarget(); }
    namespace {
        SDL_Surface* ReadSurface(int x, int y, int w, int h) {
            const SDL_Rect r{ x, y, w, h };
            SDL_Surface* s = SDL_RenderReadPixels(S.Renderer, &r);
            if (!s) { return nullptr; }
            if (s->format != SDL_PIXELFORMAT_ARGB8888) {
                SDL_Surface* c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888);
                SDL_DestroySurface(s);
                s = c;
            }
            return s;
        }
    }
    Image ReadPixels(int x, int y, int w, int h) {
        Image out;
        SDL_Surface* s = ReadSurface(x, y, w, h);
        if (!s) { return out; }
        out.W = s->w; out.H = s->h;
        out.Pixels.resize((size_t)out.W * out.H);
        for (int row = 0; row < out.H; ++row) { std::memcpy(out.Pixels.data() + (size_t)row * out.W, (const uint8_t*)s->pixels + (size_t)s->pitch * row, (size_t)out.W * 4); }
        SDL_DestroySurface(s);
        return out;
    }
    namespace { std::deque<Image> ReadQueue; }
    // SDL 版はその場で読む (受け取りは古い順)
    bool ReadPixelsAsync(int x, int y, int w, int h) {
        Image img = ReadPixels(x, y, w, h);
        if (!img) { return false; }
        ReadQueue.push_back(std::move(img));
        return true;
    }
    bool TakeReadPixels(Image& out, bool) {
        if (ReadQueue.empty()) { return false; }
        out = std::move(ReadQueue.front());
        ReadQueue.pop_front();
        return true;
    }
    bool SavePng(const std::string& path, int x, int y, int w, int h) {
        SDL_Surface* s = ReadSurface(x, y, w, h);
        if (!s) { return false; }
        const bool ok = IMG_SavePNG(s, path.c_str());
        SDL_DestroySurface(s);
        return ok;
    }
    bool CaptureScreen(Texture& texture, int w, int h) {
        if (!S.Renderer || w <= 0 || h <= 0) { return false; }
        if (SDL_Texture* source = SDL_GetRenderTarget(S.Renderer)) {
            if (Raw(texture) == source) { return true; }
            if (!texture || (int)texture.Width() != w || (int)texture.Height() != h
                || SDL_GetNumberProperty(SDL_GetTextureProperties(Raw(texture)), SDL_PROP_TEXTURE_ACCESS_NUMBER, -1) != SDL_TEXTUREACCESS_TARGET) {
                texture = CreateTexture(w, h, true);
            }
            if (!texture) { return false; }
            SDL_Rect viewport{}, clip{};
            const bool viewportSet = SDL_RenderViewportSet(S.Renderer);
            const bool clipped = SDL_RenderClipEnabled(S.Renderer);
            SDL_GetRenderViewport(S.Renderer, &viewport);
            SDL_GetRenderClipRect(S.Renderer, &clip);
            float scaleX = 1, scaleY = 1;
            SDL_GetRenderScale(S.Renderer, &scaleX, &scaleY);
            Uint8 red = 255, green = 255, blue = 255, alpha = 255;
            SDL_BlendMode blend = SDL_BLENDMODE_BLEND;
            SDL_GetTextureColorMod(source, &red, &green, &blue);
            SDL_GetTextureAlphaMod(source, &alpha);
            SDL_GetTextureBlendMode(source, &blend);
            if (!SDL_SetRenderTarget(S.Renderer, Raw(texture))) { return false; }
            SDL_SetRenderViewport(S.Renderer, nullptr);
            SDL_SetRenderClipRect(S.Renderer, nullptr);
            SDL_SetRenderScale(S.Renderer, 1, 1);
            SDL_SetTextureColorMod(source, 255, 255, 255);
            SDL_SetTextureAlphaMod(source, 255);
            SDL_SetTextureBlendMode(source, SDL_BLENDMODE_NONE);
            const SDL_FRect area{0, 0, (float)w, (float)h};
            const bool copied = SDL_RenderTexture(S.Renderer, source, &area, &area);
            SDL_SetTextureColorMod(source, red, green, blue);
            SDL_SetTextureAlphaMod(source, alpha);
            SDL_SetTextureBlendMode(source, blend);
            SDL_SetRenderTarget(S.Renderer, source);
            SDL_SetRenderScale(S.Renderer, scaleX, scaleY);
            SDL_SetRenderViewport(S.Renderer, viewportSet ? &viewport : nullptr);
            SDL_SetRenderClipRect(S.Renderer, clipped ? &clip : nullptr);
            return copied;
        }
        // A window backbuffer cannot be sampled as a texture. Keep readback for
        // that uncommon caller; normal game frames use the GPU canvas above.
        SDL_Surface* s = ReadSurface(0, 0, w, h);
        if (!s) { return false; }
        if (!texture || (int)texture.Width() != s->w || (int)texture.Height() != s->h) { texture = CreateTexture(s->w, s->h, false); }
        if (texture) { UpdateTexture(texture, s->pixels, s->pitch); }
        SDL_DestroySurface(s);
        return (bool)texture;
    }
}
