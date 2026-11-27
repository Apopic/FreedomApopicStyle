// 文字の描画 (DirectWrite + Direct2D)。
// 文字列ごとに 2 倍の解像度で白い文字 (と縁取り) の画像を作ってテクスチャにし、色は頂点の色で付ける。
// 作った画像と幅の計測結果はキャッシュして、毎フレームの描画では DirectWrite を呼ばない。
#include "../Text.hpp"
#include "WinInternal.hpp"
#include <d2d1.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <list>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

namespace txt {
    namespace {
        constexpr float RasterScale = 2.0f;
        constexpr size_t MaxTexts = 2048;
        constexpr size_t MaxTextBytes = 64 * 1024 * 1024;
        constexpr size_t MaxWidths = 16384;

        struct FontEntry {
            ComPtr<IDWriteTextFormat> Format;       // RasterScale 倍の大きさ
            ComPtr<IDWriteFontCollection> Collection;
            float Size = 0;
            int Edge = 0, Space = 0;
            float Ascent = 0, Descent = 0, LineGap = 0;    // 1 倍の大きさ (px)
            float OffsetY = 0;
            bool Used = false;
        };
        struct TextEntry {
            gfx::Texture Fill, Outline;
            int BW = 0, BH = 0;             // テクスチャ全体の大きさ (使い回しの区分。Src はその中の文字の範囲)
            float Pad = 0;                  // 画像の周りの余白 (RasterScale 倍の px)
            size_t Bytes = 0;
            std::list<std::string>::iterator Age;
        };
        struct Registered { std::string Path; ComPtr<IDWriteFontCollection> Collection; };

        bool Initialized = false;
        ComPtr<IDWriteFactory> Factory;
        ComPtr<IDWriteFactory5> Factory5;
        ComPtr<ID2D1Factory> D2D;
        ComPtr<IWICImagingFactory> Wic;
        ComPtr<IDWriteFontCollection> System;
        ComPtr<ID2D1StrokeStyle> RoundStroke;
        // 作業用の画像 (足りなくなったら大きくする)
        ComPtr<IWICBitmap> Scratch;
        ComPtr<ID2D1RenderTarget> ScratchTarget;
        ComPtr<ID2D1SolidColorBrush> WhiteBrush;
        UINT ScratchW = 0, ScratchH = 0;

        std::vector<FontEntry> Fonts;
        std::unordered_map<std::string, TextEntry> Texts;     // キー: ハンドル + '\0' + 文字列
        std::list<std::string> Ages;                          // 前ほど最近使った
        std::unordered_map<std::string, int> Widths;
        std::vector<Registered> RegisteredFonts;
        size_t TextBytes = 0;
        // 消した文字のテクスチャを大きさの区分ごとに取っておき、次の文字で使い回す (GPU の確保は重いので)
        std::unordered_map<uint64_t, std::vector<gfx::Texture>> Pool;
        size_t PoolBytes = 0;
        constexpr size_t MaxPoolBytes = 16 * 1024 * 1024;
        uint64_t Bucket(int w, int h) { return ((uint64_t)(uint32_t)w << 32) | (uint32_t)h; }
        gfx::Texture TakeTexture(int bw, int bh) {
            if (auto it = Pool.find(Bucket(bw, bh)); it != Pool.end() && !it->second.empty()) {
                gfx::Texture t = std::move(it->second.back());
                it->second.pop_back();
                PoolBytes -= (size_t)bw * bh * 4;
                return t;
            }
            return gfx::CreateTexture(bw, bh, false);
        }
        void GiveBack(TextEntry& e) {
            for (gfx::Texture* t : { &e.Fill, &e.Outline }) {
                const size_t bytes = (size_t)e.BW * e.BH * 4;
                if (*t && t->Tex.use_count() == 1 && PoolBytes + bytes <= MaxPoolBytes) {
                    Pool[Bucket(e.BW, e.BH)].push_back(gfx::SubTexture(*t, 0, 0, (float)e.BW, (float)e.BH));
                    PoolBytes += bytes;
                }
                *t = {};
            }
        }

        std::string Key(int handle, std::string_view text) {
            std::string k;
            k.reserve(text.size() + 8);
            k.append(reinterpret_cast<const char*>(&handle), sizeof(handle));
            k.push_back('\0');
            k.append(text);
            return k;
        }
        void ForgetFont(int handle) {
            auto mine = [&](const std::string& k) { int h = -1; std::memcpy(&h, k.data(), sizeof(h)); return h == handle; };
            for (auto it = Texts.begin(); it != Texts.end();) {
                if (mine(it->first)) { TextBytes -= it->second.Bytes; Ages.erase(it->second.Age); GiveBack(it->second); it = Texts.erase(it); }
                else { ++it; }
            }
            std::erase_if(Widths, [&](const auto& e) { return mine(e.first); });
        }
        void EvictOldest() {
            auto last = Texts.find(Ages.back());
            if (last != Texts.end()) { TextBytes -= last->second.Bytes; GiveBack(last->second); Texts.erase(last); }
            Ages.pop_back();
        }

        // ---- 文字の形を描く (塗り: DrawGlyphRun / 縁取り: 輪郭の線) ----
        class Renderer final : public IDWriteTextRenderer {
        public:
            ID2D1RenderTarget* Target = nullptr;
            ID2D1Brush* Brush = nullptr;
            float OffsetX = 0, OffsetY = 0;
            float Stroke = 0;           // 0 なら塗り、正なら縁取りの線の太さ
            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
                if (id == __uuidof(IUnknown) || id == __uuidof(IDWritePixelSnapping) || id == __uuidof(IDWriteTextRenderer)) { *out = this; return S_OK; }
                *out = nullptr;
                return E_NOINTERFACE;
            }
            ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
            ULONG STDMETHODCALLTYPE Release() override { return 1; }
            HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override { *disabled = FALSE; return S_OK; }
            HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* m) override { *m = { 1, 0, 0, 1, 0, 0 }; return S_OK; }
            HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* p) override { *p = 1.0f; return S_OK; }
            HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE mode, const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown*) override {
                x += OffsetX; y += OffsetY;
                if (Stroke <= 0) {
                    Target->DrawGlyphRun(D2D1::Point2F(x, y), run, Brush, mode);
                    return S_OK;
                }
                ComPtr<ID2D1PathGeometry> path;
                ComPtr<ID2D1GeometrySink> sink;
                if (FAILED(D2D->CreatePathGeometry(&path)) || FAILED(path->Open(&sink))) { return S_OK; }
                run->fontFace->GetGlyphRunOutline(run->fontEmSize, run->glyphIndices, run->glyphAdvances, run->glyphOffsets, run->glyphCount, run->isSideways, run->bidiLevel % 2, sink.Get());
                sink->Close();
                Target->SetTransform(D2D1::Matrix3x2F::Translation(x, y));
                Target->DrawGeometry(path.Get(), Brush, Stroke, RoundStroke.Get());
                Target->SetTransform(D2D1::Matrix3x2F::Identity());
                return S_OK;
            }
            HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) override { return S_OK; }
            HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) override { return S_OK; }
            HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) override { return S_OK; }
        };

        bool EnsureScratch(UINT w, UINT h) {
            if (Scratch && w <= ScratchW && h <= ScratchH) { return true; }
            const UINT nw = std::max({ w, ScratchW, 1024u }), nh = std::max({ h, ScratchH, 128u });
            ScratchTarget.Reset(); WhiteBrush.Reset(); Scratch.Reset();
            if (FAILED(Wic->CreateBitmap(nw, nh, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &Scratch))) { return false; }
            const D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
            if (FAILED(D2D->CreateWicBitmapRenderTarget(Scratch.Get(), props, &ScratchTarget))) { Scratch.Reset(); return false; }
            ScratchTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            ScratchTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            ScratchTarget->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &WhiteBrush);
            ScratchW = nw; ScratchH = nh;
            return true;
        }
        ComPtr<IDWriteTextLayout> Layout(const FontEntry& f, std::string_view text) {
            const std::wstring w = win::Widen(text);
            ComPtr<IDWriteTextLayout> layout;
            Factory->CreateTextLayout(w.c_str(), (UINT32)w.size(), f.Format.Get(), 1.0e6f, 1.0e6f, &layout);
            return layout;
        }
        // 1 行の文字列を白で描いて、アルファだけの画像にする (テクスチャは bw × bh、文字は左上の w × h)
        gfx::Texture Rasterize(const FontEntry& f, IDWriteTextLayout* layout, UINT w, UINT h, UINT bw, UINT bh, float pad, float stroke) {
            const float baseline = pad + f.Ascent * RasterScale;
            if (w == 0 || h == 0 || !EnsureScratch(bw, bh)) { return {}; }
            DWRITE_LINE_METRICS line{};
            UINT32 lines = 0;
            layout->GetLineMetrics(&line, 1, &lines);
            Renderer r;
            r.Target = ScratchTarget.Get();
            r.Brush = WhiteBrush.Get();
            r.OffsetX = pad;
            r.OffsetY = baseline - line.baseline;
            r.Stroke = stroke;
            ScratchTarget->BeginDraw();
            // 作業用の画像のうち使う範囲だけを消す
            ScratchTarget->PushAxisAlignedClip(D2D1::RectF(0, 0, (float)bw, (float)bh), D2D1_ANTIALIAS_MODE_ALIASED);
            ScratchTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
            layout->Draw(nullptr, &r, 0, 0);
            ScratchTarget->PopAxisAlignedClip();
            if (FAILED(ScratchTarget->EndDraw())) { return {}; }
            WICRect rect{ 0, 0, (INT)bw, (INT)bh };
            thread_local std::vector<uint32_t> px;
            px.resize((size_t)bw * bh);
            if (FAILED(Scratch->CopyPixels(&rect, bw * 4, (UINT)(px.size() * 4), (BYTE*)px.data()))) { return {}; }
            // 白で描いたので、乗算済みの色 = アルファ。色は白、アルファだけを残す
            for (auto& p : px) { p = (p & 0xFF000000u) | 0x00FFFFFFu; }
            gfx::Texture t = TakeTexture((int)bw, (int)bh);
            if (!t) { return {}; }
            gfx::UpdateTexture(t, px.data(), (int)bw * 4);
            return gfx::SubTexture(t, 0, 0, (float)w, (float)h);
        }
        TextEntry* GetText(int handle, std::string_view text) {
            const std::string k = Key(handle, text);
            if (auto it = Texts.find(k); it != Texts.end()) {
                Ages.splice(Ages.begin(), Ages, it->second.Age);
                return &it->second;
            }
            while (!Texts.empty() && (Texts.size() >= MaxTexts || TextBytes >= MaxTextBytes)) { EvictOldest(); }
            const auto& f = Fonts[(size_t)handle];
            ComPtr<IDWriteTextLayout> layout = Layout(f, text);
            if (!layout) { return nullptr; }
            DWRITE_TEXT_METRICS m{};
            layout->GetMetrics(&m);
            TextEntry e;
            e.Pad = std::ceil(f.Edge * RasterScale) + 2.0f;
            const UINT w = (UINT)std::ceil(m.widthIncludingTrailingWhitespace + e.Pad * 2), h = (UINT)std::ceil((f.Ascent + f.Descent) * RasterScale + e.Pad * 2);
            // 使い回しやすいように、テクスチャは幅 64・高さ 16 の倍数にする (余りは透明)
            e.BW = (int)((w + 63) / 64 * 64);
            e.BH = (int)((h + 15) / 16 * 16);
            e.Fill = Rasterize(f, layout.Get(), w, h, (UINT)e.BW, (UINT)e.BH, e.Pad, 0);
            if (f.Edge > 0) { e.Outline = Rasterize(f, layout.Get(), w, h, (UINT)e.BW, (UINT)e.BH, e.Pad, f.Edge * RasterScale * 2.0f); }
            if (e.Fill) { e.Bytes += (size_t)e.BW * (size_t)e.BH * 4 * (e.Outline ? 2 : 1); }
            while (!Texts.empty() && TextBytes + e.Bytes > MaxTextBytes) { EvictOldest(); }
            TextBytes += e.Bytes;
            Ages.push_front(k);
            e.Age = Ages.begin();
            return &Texts.emplace(k, std::move(e)).first->second;
        }

        std::string Lower(std::string s) { for (auto& c : s) { c = (char)std::tolower((unsigned char)c); } return s; }
        // フォントファイルを 1 つだけ含むフォントの集まりを作る
        ComPtr<IDWriteFontCollection> CollectionFromFile(const std::string& path) {
            if (!Factory5) { return nullptr; }
            ComPtr<IDWriteFontSetBuilder> builder0;
            ComPtr<IDWriteFontSetBuilder1> builder;
            ComPtr<IDWriteFontFile> file;
            ComPtr<IDWriteFontSet> set;
            ComPtr<IDWriteFontCollection1> collection;
            if (FAILED(Factory5->CreateFontSetBuilder(&builder0)) || FAILED(builder0.As(&builder))) { return nullptr; }
            if (FAILED(Factory5->CreateFontFileReference(win::Widen(path).c_str(), nullptr, &file))) { return nullptr; }
            if (FAILED(builder->AddFontFile(file.Get())) || FAILED(builder->CreateFontSet(&set))) { return nullptr; }
            if (FAILED(Factory5->CreateFontCollectionFromFontSet(set.Get(), &collection))) { return nullptr; }
            return collection;
        }
        bool HasFamily(IDWriteFontCollection* c, const std::wstring& name) {
            UINT32 index = 0; BOOL exists = FALSE;
            return c && SUCCEEDED(c->FindFamilyName(name.c_str(), &index, &exists)) && exists;
        }
        std::wstring FirstFamily(IDWriteFontCollection* c) {
            ComPtr<IDWriteFontFamily> family;
            ComPtr<IDWriteLocalizedStrings> names;
            if (!c || c->GetFontFamilyCount() == 0 || FAILED(c->GetFontFamily(0, &family)) || FAILED(family->GetFamilyNames(&names))) { return {}; }
            UINT32 len = 0;
            if (FAILED(names->GetStringLength(0, &len))) { return {}; }
            std::wstring s(len, L'\0');
            names->GetString(0, s.data(), len + 1);
            return s;
        }
        // 既定の日本語フォント: DxLib の既定 (ＭＳ ゴシック) に近い等幅の BIZ UDゴシック。無ければ游ゴシック・メイリオ・ＭＳ ゴシック
        std::wstring DefaultFamily() {
            for (const wchar_t* name : { L"BIZ UDGothic", L"Yu Gothic Medium", L"Yu Gothic", L"Meiryo", L"MS Gothic" }) {
                if (HasFamily(System.Get(), name)) { return name; }
            }
            return L"Segoe UI";
        }
    }

    bool Init() {
        if (Initialized) { return true; }
        win::EnsureCom();
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(Factory.GetAddressOf())))) { return false; }
        Factory.As(&Factory5);
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, D2D.GetAddressOf()))) { return false; }
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Wic)))) { return false; }
        Factory->GetSystemFontCollection(&System, FALSE);
        const D2D1_STROKE_STYLE_PROPERTIES stroke = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
        D2D->CreateStrokeStyle(stroke, nullptr, 0, &RoundStroke);
        Initialized = true;
        return true;
    }
    void Shutdown() {
        Texts.clear(); Ages.clear(); Widths.clear(); TextBytes = 0;
        Pool.clear(); PoolBytes = 0;
        Fonts.clear();
        RegisteredFonts.clear();
        ScratchTarget.Reset(); WhiteBrush.Reset(); Scratch.Reset(); ScratchW = ScratchH = 0;
        RoundStroke.Reset(); System.Reset(); Wic.Reset(); D2D.Reset(); Factory5.Reset(); Factory.Reset();
        Initialized = false;
    }

    void RegisterFontFile(const std::string& path) {
        if (path.empty() || std::ranges::find(RegisteredFonts, path, &Registered::Path) != RegisteredFonts.end()) { return; }
        RegisteredFonts.push_back({ path, nullptr });
    }

    int OpenFont(const FontOptions& o) {
        if (!Initialized && !Init()) { return -1; }
        ComPtr<IDWriteFontCollection> collection;
        std::wstring family;
        const std::wstring name = win::Widen(o.Name);
        std::error_code ec;
        // フォントファイルの指定 → 登録したファイル (名前で探す) → Windows のフォント → 既定のフォント
        if (!o.File.empty() && std::filesystem::exists(std::filesystem::path(win::Widen(o.File)), ec)) {
            if ((collection = CollectionFromFile(o.File))) { family = !name.empty() && HasFamily(collection.Get(), name) ? name : FirstFamily(collection.Get()); }
        }
        if (family.empty() && !name.empty()) {
            for (auto& r : RegisteredFonts) {
                if (!r.Collection) { r.Collection = CollectionFromFile(r.Path); }
                if (HasFamily(r.Collection.Get(), name)) { collection = r.Collection; family = name; break; }
            }
            if (family.empty() && HasFamily(System.Get(), name)) { collection = System; family = name; }
        }
        if (family.empty()) { collection = System; family = DefaultFamily(); }

        const DWRITE_FONT_WEIGHT weight = o.Thick >= 8 ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;   // DxLib の太さ (0～9、既定 6) で 8 以上を太字にする
        const float size = std::max(o.Size, 1.0f);
        FontEntry e;
        if (FAILED(Factory->CreateTextFormat(family.c_str(), collection.Get(), weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size * RasterScale, L"ja-jp", &e.Format))) { return -1; }
        e.Format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        e.Collection = collection;
        e.Size = size;
        e.Edge = std::max(o.Edge, 0);
        e.Space = o.Space;
        // 文字の高さ (上端～下端) をフォントの情報から求める
        e.Ascent = size * 0.88f; e.Descent = size * 0.12f;
        {
            ComPtr<IDWriteFontFamily> fam;
            ComPtr<IDWriteFont> font;
            UINT32 index = 0; BOOL exists = FALSE;
            if (collection && SUCCEEDED(collection->FindFamilyName(family.c_str(), &index, &exists)) && exists
                && SUCCEEDED(collection->GetFontFamily(index, &fam)) && SUCCEEDED(fam->GetFirstMatchingFont(weight, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font))) {
                DWRITE_FONT_METRICS m{};
                font->GetMetrics(&m);
                if (m.designUnitsPerEm > 0) {
                    e.Ascent = (float)m.ascent * size / m.designUnitsPerEm;
                    e.Descent = (float)m.descent * size / m.designUnitsPerEm;
                    e.LineGap = (float)m.lineGap * size / m.designUnitsPerEm;
                }
            }
        }
        e.OffsetY = (e.Size - (e.Ascent + e.Descent)) * 0.5f;
        e.Used = true;
        for (int i = 0; i < (int)Fonts.size(); ++i) {
            if (!Fonts[(size_t)i].Used) { Fonts[(size_t)i] = std::move(e); return i; }
        }
        Fonts.push_back(std::move(e));
        return (int)Fonts.size() - 1;
    }
    void CloseFont(int handle) {
        if (!IsValid(handle)) { return; }
        ForgetFont(handle);
        Fonts[(size_t)handle] = FontEntry{};
    }
    bool IsValid(int handle) { return handle >= 0 && handle < (int)Fonts.size() && Fonts[(size_t)handle].Used; }
    float Size(int handle) { return IsValid(handle) ? Fonts[(size_t)handle].Size : 0.0f; }

    int Width(int handle, std::string_view text) {
        if (!IsValid(handle) || text.empty()) { return 0; }
        if (text.find('\n') != std::string_view::npos) {
            int widest = 0;
            for (size_t start = 0; start < text.size();) {
                const auto end = text.find('\n', start);
                auto line = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
                if (line.ends_with('\r')) { line.remove_suffix(1); }
                widest = std::max(widest, Width(handle, line));
                if (end == std::string_view::npos) { break; }
                start = end + 1;
            }
            return widest;
        }
        std::string k = Key(handle, text);
        if (auto it = Widths.find(k); it != Widths.end()) { return it->second; }
        ComPtr<IDWriteTextLayout> layout = Layout(Fonts[(size_t)handle], text);
        int w = 0;
        if (layout) {
            DWRITE_TEXT_METRICS m{};
            layout->GetMetrics(&m);
            w = (int)std::lround(m.widthIncludingTrailingWhitespace / RasterScale);
        }
        if (Widths.size() >= MaxWidths) { Widths.clear(); }
        Widths.emplace(std::move(k), w);
        return w;
    }

    void Draw(int handle, float x, float y, std::string_view text, gfx::Color color, gfx::Color edge, float scalex, float scaley) {
        if (!IsValid(handle) || text.empty()) { return; }
        if (!std::isfinite(scalex) || !std::isfinite(scaley) || scalex <= 0 || scaley <= 0) { return; }
        if (text.find('\n') != std::string_view::npos) {
            const auto& f = Fonts[(size_t)handle];
            const float lineHeight = (f.Ascent + f.Descent + f.LineGap) * scaley;
            for (size_t start = 0; start < text.size();) {
                const auto end = text.find('\n', start);
                auto line = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
                if (line.ends_with('\r')) { line.remove_suffix(1); }
                Draw(handle, x, y, line, color, edge, scalex, scaley);
                y += lineHeight;
                if (end == std::string_view::npos) { break; }
                start = end + 1;
            }
            return;
        }
        TextEntry* e = GetText(handle, text);
        if (!e || !e->Fill) { return; }
        const auto& f = Fonts[(size_t)handle];
        const int alpha = gfx::CurrentBlend() == gfx::Blend::None ? 255 : gfx::CurrentBlendParam();
        const float left = x - e->Pad / RasterScale * scalex, top = y + f.OffsetY * scaley - e->Pad / RasterScale * scaley;
        auto draw = [&](const gfx::Texture& t, gfx::Color tint) {
            if (!t) { return; }
            win::DrawTinted(t, left, top, left + t.Width() / RasterScale * scalex, top + t.Height() / RasterScale * scaley, tint, alpha);
        };
        draw(e->Outline, edge);
        draw(e->Fill, color);
    }

    int DefaultFont() {
        static int font = -1;
        if (!IsValid(font)) { FontOptions o; o.Size = 16; font = OpenFont(o); }
        return font;
    }
    void DrawFormat(float x, float y, gfx::Color color, const char* format, ...) {
        char buf[1024];
        va_list args;
        va_start(args, format);
        std::vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        Draw(DefaultFont(), x, y, buf, color);
    }
}
