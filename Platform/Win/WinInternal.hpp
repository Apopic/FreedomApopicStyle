#pragma once
// Windows 版の実装どうしで使う (ゲームからは使わない)
#include "../Gfx.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <string_view>

namespace win {
    // UTF-8 と UTF-16 の変換
    std::wstring Widen(std::string_view s);
    std::string Narrow(std::wstring_view s);

    // ---- ウィンドウ (Sys.cpp) ----
    struct WindowOptions {
        std::wstring Title;
        int ClientW = 1280, ClientH = 720;
        bool FullScreen = false, Resizable = true, Hidden = false;
        int IconResource = 0;
    };
    HWND CreateMainWindow(const WindowOptions& o);
    void DestroyMainWindow();
    HWND MainWindow();
    void SetFullScreen(bool on);
    bool IsFullScreen();
    // クライアント領域の大きさ (物理ピクセル)
    void ClientSize(int& w, int& h);

    // ---- 描画 (Gfx.cpp) ----
    // ウィンドウの大きさが変わった (次の Present でバックバッファを作り直す)
    void NotifyResize();
    // 文字用: 色 (0xRRGGBB) とアルファで塗る。明るさ・足す色は使わず、通常の合成で描く
    void DrawTinted(const gfx::Texture& texture, float l, float t, float r, float b, gfx::Color color, int alpha);

    // ---- 画像 (Image.cpp / WIC) ----
    bool DecodeImageFile(const std::string& path, gfx::Image& out);
    bool DecodeImageMemory(const void* data, size_t size, gfx::Image& out);
    bool ImageFileSize(const std::string& path, int& w, int& h);
    bool EncodePng(const std::string& path, const gfx::Image& image);
    // COM をこのスレッドで使えるようにする (何度呼んでもよい)
    void EnsureCom();
}
