#pragma once
// 文字の描画 (Windows: DirectWrite / Android: SDL3_ttf)。フォントは番号 (ハンドル) で扱う。
// 文字列を2倍解像度で描いたテクスチャを再利用し、線形補間で拡縮する。キャッシュは件数と容量で制限する。
#include "Gfx.hpp"
#include <string>
#include <string_view>

namespace txt {
    struct FontOptions {
        std::string File;           // フォントファイル (空なら Name から探す)
        std::string Name;           // フォント名 (空ならゲームの既定の日本語フォント)
        float Size = 16;            // 文字の高さ (px)
        int Thick = 0;              // 太さ (0～9。8 以上は太字)
        int Edge = 0;               // 縁取りの幅 (px)
        int Space = 0;              // 旧スキンの字間指定 (現在は描画・幅計測とも未適用)
    };

    bool Init();        // gfx::Init の後
    void Shutdown();

    // 失敗すると -1
    int OpenFont(const FontOptions& options);
    void CloseFont(int handle);
    bool IsValid(int handle);
    float Size(int handle);

    // 文字列の幅 (px。縁取りは含まない)
    int Width(int handle, std::string_view text);
    // (x, y) を左上にして描く。edge は縁取りの色 (縁取りの無いフォントでは使わない)。scalex/scaley で伸ばす
    void Draw(int handle, float x, float y, std::string_view text, gfx::Color color, gfx::Color edge = 0, float scalex = 1.0f, float scaley = 1.0f);

    // 既定のフォント (デバッグ表示など。初めて使う時に開く)
    int DefaultFont();
    // 既定のフォントで printf の書式の文字を描く
    void DrawFormat(float x, float y, gfx::Color color, const char* format, ...);

    // フォントファイルを読める場所に登録する (スキンのフォント。名前で探す時に使う)
    void RegisterFontFile(const std::string& path);
}
