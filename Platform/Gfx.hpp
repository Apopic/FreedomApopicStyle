#pragma once
// 描画の土台。ウィンドウ・テクスチャ・図形・合成モード・描画範囲をまとめる。
// 実装は Windows が Direct3D 12 (Platform/Win)、Android が SDL3 (Platform/SDL)。関数名はどちらも同じ。
// 毎フレーム何度も呼ばれるので、状態は 1 か所に持ち、呼び出しごとの確保や GPU の状態変更は必要な時だけ行う。
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace gfx {

    // 色は 0xRRGGBB (アルファはブレンドの値で決める)
    using Color = uint32_t;
    constexpr Color Rgb(int r, int g, int b) {
        return ((uint32_t)(r < 0 ? 0 : r > 255 ? 255 : r) << 16) | ((uint32_t)(g < 0 ? 0 : g > 255 ? 255 : g) << 8) | (uint32_t)(b < 0 ? 0 : b > 255 ? 255 : b);
    }
    constexpr int R(Color c) { return (int)((c >> 16) & 0xFF); }
    constexpr int G(Color c) { return (int)((c >> 8) & 0xFF); }
    constexpr int B(Color c) { return (int)(c & 0xFF); }

    // 合成モード。None でも画像のアルファは使う (透過 PNG はそのまま重なる)
    enum class Blend { None, Alpha, Add, Sub, Mul };

    struct RectF { float x = 0, y = 0, w = 0, h = 0; };
    // GPU のテクスチャ (中身は実装ごとに違う)
    struct TextureObject;

    // 画像 (1 枚のテクスチャの一部)。分割画像はテクスチャを共有し、Src だけが違う
    struct Texture {
        std::shared_ptr<TextureObject> Tex;
        RectF Src{};
        explicit operator bool() const { return Tex != nullptr; }
        float Width() const { return Src.w; }
        float Height() const { return Src.h; }
    };

    struct InitOptions {
        std::string Title = "FreedomApopicStyle";
        int Width = 1280, Height = 720;     // 論理解像度 (スキンの解像度)
        bool FullScreen = false;
        bool VSync = true;
        bool UseGpuRenderer = false;      // SDL_GPU backend, with automatic fallback
        bool Resizable = true;
        bool Hidden = false;
        int IconResource = 0;               // Windows: タイトルバー・タスクバーのアイコンのリソース番号 (0 なら既定)
    };

    // CPU 側の画像 (ARGB8888。1 行は W 画素で詰める)
    struct Image {
        int W = 0, H = 0;
        std::vector<uint32_t> Pixels;
        explicit operator bool() const { return W > 0 && H > 0; }
    };

    bool Init(const InitOptions& options);
    void Shutdown();
    bool IsInit();
    std::string LastError();            // 失敗の理由 (Init が false を返した時など)
    const char* BackendName();          // デバッグ表示用 ("D3D12" など)
    int RefreshRate();                  // ウィンドウのあるモニターのリフレッシュレート (分からなければ 0)
    void RaiseWindow();                 // 前面に出して入力を受け付ける

    // 論理解像度 (スキンの解像度)。ウィンドウの大きさに合わせて縦横比を保って拡大する
    void SetLogicalSize(int width, int height);
    int LogicalWidth();
    int LogicalHeight();
    void SetVSync(bool on);
    void SetFullScreen(bool on);
    bool IsFullScreen();
    void SetWindowVisible(bool visible);
    void SetWindowTitle(const char* title);
    // ウィンドウ座標 (マウスなど) を論理座標に直す
    void WindowToLogical(float wx, float wy, float& lx, float& ly);
    void LogicalToWindow(float lx, float ly, float& wx, float& wy);

    // 今の描画先を塗りつぶす (描画範囲は無視)。alpha: 描画先に残すアルファ (描画用テクスチャを透明にする時は 0)
    void Clear(Color color = 0, int alpha = 255);
    void Present();

    // ---- 描画の状態 (DxLib の SetDrawBlendMode / SetDrawArea / SetDrawBright / SetDrawAddColor に当たる) ----
    void SetBlend(Blend mode, int param = 255);
    Blend CurrentBlend();
    int CurrentBlendParam();
    void SetClip(int left, int top, int right, int bottom);
    void ResetClip();
    // 描画範囲を設定しているか (しているなら範囲を返す)
    bool GetClip(int& left, int& top, int& right, int& bottom);
    void SetBright(int r, int g, int b);
    void GetBright(int& r, int& g, int& b);
    // 画像の色に足す (0～255)。DxLib と同じ min(色 × 明るさ + 足す色, 255) × アルファ
    void SetAddColor(int r, int g, int b);

    // ---- 図形 (アンチエイリアスは縁を 1px 薄くして近づける) ----
    void Line(float x1, float y1, float x2, float y2, Color color, float thickness = 1.0f);
    void Box(float left, float top, float right, float bottom, Color color, bool fill, float thickness = 1.0f);
    void Circle(float cx, float cy, float r, int segments, Color color, bool fill, float thickness = 1.0f);
    void Oval(float cx, float cy, float rx, float ry, int segments, Color color, bool fill, float thickness = 1.0f);
    void Triangle(float x1, float y1, float x2, float y2, float x3, float y3, Color color, bool fill);
    void Pixel(float x, float y, Color color);

    // ---- 画像 ----
    Texture LoadTexture(const std::string& path);
    Texture LoadTextureFromMemory(const void* data, size_t size);
    // 横 nx × 縦 ny に分割 (1 コマ cw × ch)。テクスチャは 1 枚を共有する
    std::vector<Texture> LoadDivTexture(const std::string& path, int nx, int ny, float cw, float ch);
    std::vector<Texture> Divide(const Texture& source, int nx, int ny, float cw, float ch);
    bool ImageSize(const std::string& path, int& width, int& height);
    // 画像の画素 (ARGB8888) を読む (音符の見える幅を調べるなど、CPU で画素を見る時)
    std::vector<uint32_t> LoadPixels(const std::string& path, int& width, int& height);
    Texture CreateTexture(int width, int height, bool target);     // target: 描画先にできる
    void UpdateTexture(const Texture& texture, const void* pixels, int pitch);     // ARGB8888
    Texture SubTexture(const Texture& source, float x, float y, float w, float h);

    // 左上 (l, t) ～ 右下 (r, b) に引き伸ばして描く
    void DrawTexture(const Texture& texture, float l, float t, float r, float b);
    // 等倍で (x, y) に、画像の (sx, sy, sw, sh) を描く
    void DrawTextureRect(const Texture& texture, float x, float y, float sx, float sy, float sw, float sh);
    // 画像の (sx, sy, sw, sh) を、左上 (l, t) ～ 右下 (r, b) に引き伸ばして描く
    void DrawTexturePart(const Texture& texture, float l, float t, float r, float b, float sx, float sy, float sw, float sh);
    // 4 隅 (左上・右上・右下・左下) を指定して描く
    void DrawTextureQuad(const Texture& texture, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4);
    // 中心 (cx, cy)・倍率・回転 (ラジアン) で描く
    void DrawTextureRotated(const Texture& texture, float cx, float cy, float scale, float angle, bool flipx = false);

    // 描画先 (nullptr で画面用テクスチャ。描画はすべてスキンの解像度の画面用テクスチャに行い、Present でウィンドウへ拡大する)
    void SetTarget(const Texture* texture);
    // 今の描画先を ARGB8888 で読み出す (録画・スクリーンショット)
    Image ReadPixels(int x, int y, int w, int h);
    // 待たない読み出し (録画用)。ReadPixelsAsync で今の描画先を GPU 上で読み出し用にコピーしておき (失敗すると false)、
    // TakeReadPixels で GPU が終わった分を古い順に受け取る。wait なら終わるまで待つ。受け取れる分が無ければ false
    bool ReadPixelsAsync(int x, int y, int w, int h);
    bool TakeReadPixels(Image& out, bool wait);
    bool SavePng(const std::string& path, int x, int y, int w, int h);
    // 今の画面を GPU 内でコピーする (ウィンドウのバックバッファを直接選んだ時だけ読み戻す)
    bool CaptureScreen(Texture& texture, int w, int h);
}
