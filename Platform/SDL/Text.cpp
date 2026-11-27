#include "../Text.hpp"
#include "SdlInternal.hpp"
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <list>
#include <unordered_map>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace txt {
    namespace {
        struct FontEntry {
            TTF_Font* Fill = nullptr;
            TTF_Font* Outline = nullptr;    // 縁取り用 (縁取りが無ければ nullptr)
            float Size = 0;
            int Edge = 0, Space = 0;
            float OffsetY = 0;              // 文字の高さを Size に合わせるためのずれ
            bool Used = false;
        };
        struct TextEntry {
            SDL_Texture* Fill = nullptr;
            SDL_Texture* Outline = nullptr;
            int FillW = 0, FillH = 0, OutlineW = 0, OutlineH = 0;
            size_t Bytes = 0;
            std::list<std::string>::iterator Age;
        };
        bool Initialized = false;
        std::vector<FontEntry> Fonts;
        std::unordered_map<std::string, TextEntry> Texts;     // キー: ハンドル + '\0' + 文字列
        std::list<std::string> Ages;                          // 前ほど最近使った
        std::vector<std::string> Registered;
        constexpr size_t MaxTexts = 2048;
        constexpr size_t MaxTextBytes = 64 * 1024 * 1024;
        constexpr float RasterScale = 2.0f;
        size_t TextBytes = 0;

        std::string Key(int handle, std::string_view text) {
            std::string k;
            k.reserve(text.size() + 8);
            k.append(reinterpret_cast<const char*>(&handle), sizeof(handle));
            k.push_back('\0');
            k.append(text);
            return k;
        }
        void DestroyText(TextEntry& e) {
            if (e.Fill) { SDL_DestroyTexture(e.Fill); }
            if (e.Outline) { SDL_DestroyTexture(e.Outline); }
            e.Fill = e.Outline = nullptr;
            TextBytes -= e.Bytes; e.Bytes = 0;
        }
        void ForgetFont(int handle) {
            for (auto it = Texts.begin(); it != Texts.end();) {
                int h = -1;
                std::memcpy(&h, it->first.data(), sizeof(h));
                if (h == handle) { DestroyText(it->second); Ages.erase(it->second.Age); it = Texts.erase(it); }
                else { ++it; }
            }
        }
        TextEntry* GetText(int handle, std::string_view text) {
            const std::string k = Key(handle, text);
            if (auto it = Texts.find(k); it != Texts.end()) {
                Ages.splice(Ages.begin(), Ages, it->second.Age);
                return &it->second;
            }
            while (!Texts.empty() && (Texts.size() >= MaxTexts || TextBytes >= MaxTextBytes)) {
                auto last = Texts.find(Ages.back());
                if (last != Texts.end()) { DestroyText(last->second); Texts.erase(last); }
                Ages.pop_back();
            }
            const auto& f = Fonts[(size_t)handle];
            TextEntry e;
            auto rasterize = [&](TTF_Font* font, SDL_Texture*& texture, int& width, int& height) {
                SDL_Surface* surface = TTF_RenderText_Blended(font, text.data(), text.size(), SDL_Color{255,255,255,255});
                if (!surface) { return; }
                width = surface->w; height = surface->h;
                texture = SDL_CreateTextureFromSurface(gfx::sdl::Renderer(), surface);
                SDL_DestroySurface(surface);
                if (texture) {
                    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
                    e.Bytes += (size_t)width * height * 4;
                }
            };
            rasterize(f.Fill, e.Fill, e.FillW, e.FillH);
            if (f.Outline) { rasterize(f.Outline, e.Outline, e.OutlineW, e.OutlineH); }
            while (!Texts.empty() && TextBytes + e.Bytes > MaxTextBytes) {
                auto last = Texts.find(Ages.back());
                if (last != Texts.end()) { DestroyText(last->second); Texts.erase(last); }
                Ages.pop_back();
            }
            TextBytes += e.Bytes;
            Ages.push_front(k);
            e.Age = Ages.begin();
            return &Texts.emplace(k, e).first->second;
        }

        std::string Lower(std::string s) { for (auto& c : s) { c = (char)std::tolower((unsigned char)c); } return s; }
        bool SameName(TTF_Font* font, const std::string& name) {
            const char* family = TTF_GetFontFamilyName(font);
            return family && Lower(family) == Lower(name);
        }
        // 名前からフォントファイルを探す: 登録したファイル → Windows のフォント
        std::string FindFontFile(const std::string& name) {
            for (const auto& path : Registered) {
                if (TTF_Font* f = TTF_OpenFont(path.c_str(), 12)) {
                    const bool same = SameName(f, name);
                    TTF_CloseFont(f);
                    if (same) { return path; }
                }
            }
#ifdef _WIN32
            // HKLM / HKCU の Fonts の値の名前 ("Meiryo & Meiryo Italic & ... (TrueType)") にフォント名が含まれるもの
            for (HKEY root : { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER }) {
                HKEY key = nullptr;
                if (RegOpenKeyExW(root, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ, &key) != ERROR_SUCCESS) { continue; }
                const int wlen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
                std::wstring wname(wlen > 0 ? wlen - 1 : 0, L'\0');
                if (wlen > 0) { MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wname.data(), wlen); }
                std::string found;
                for (DWORD i = 0;; ++i) {
                    wchar_t value[512]; DWORD vlen = 512; BYTE data[1024]; DWORD dlen = sizeof(data) - 2; DWORD type = 0;
                    if (RegEnumValueW(key, i, value, &vlen, nullptr, &type, data, &dlen) != ERROR_SUCCESS) { break; }
                    if (type != REG_SZ || wname.empty() || std::wstring_view(value, vlen).find(wname) == std::wstring_view::npos) { continue; }
                    data[dlen] = 0; data[dlen + 1] = 0;
                    std::filesystem::path file(reinterpret_cast<const wchar_t*>(data));
                    if (file.is_relative()) {
                        wchar_t windir[MAX_PATH]; GetWindowsDirectoryW(windir, MAX_PATH);
                        file = std::filesystem::path(windir) / L"Fonts" / file;
                    }
                    found = file.u8string().c_str() ? std::string(reinterpret_cast<const char*>(file.u8string().c_str())) : std::string();
                    break;
                }
                RegCloseKey(key);
                if (!found.empty()) { return found; }
            }
#endif
            return {};
        }
        // 既定の日本語フォント: DxLib の既定 (ＭＳ ゴシック) に近い等幅の BIZ UDゴシック。
        // ＭＳ ゴシックは小さい大きさでビットマップの文字になり、縁取りが付かずずれて重なるので最後にする
        std::string DefaultFontFile() {
#ifdef _WIN32
            wchar_t windir[MAX_PATH]; GetWindowsDirectoryW(windir, MAX_PATH);
            const std::filesystem::path dir = std::filesystem::path(windir) / L"Fonts";
            for (const wchar_t* f : { L"BIZ-UDGothicR.ttc", L"YuGothM.ttc", L"meiryo.ttc", L"msgothic.ttc" }) {
                std::error_code ec;
                if (std::filesystem::exists(dir / f, ec)) { return reinterpret_cast<const char*>((dir / f).u8string().c_str()); }
            }
#else
            for (const char* f : { "/system/fonts/NotoSansCJK-Regular.ttc", "/system/fonts/NotoSansJP-Regular.otf", "/system/fonts/DroidSansFallback.ttf" }) {
                std::error_code ec;
                if (std::filesystem::exists(f, ec)) { return f; }
            }
#endif
            return {};
        }
    }

    bool Init() {
        if (Initialized) { return true; }
        if (!TTF_Init()) { return false; }
        Initialized = true;
        return true;
    }
    void Shutdown() {
        for (auto& [k, e] : Texts) { DestroyText(e); }
        Texts.clear(); Ages.clear();
        for (int i = 0; i < (int)Fonts.size(); ++i) { CloseFont(i); }
        Fonts.clear();
        Initialized = false;
        TTF_Quit();
    }

    void RegisterFontFile(const std::string& path) {
        if (!path.empty() && std::ranges::find(Registered, path) == Registered.end()) { Registered.push_back(path); }
    }

    int OpenFont(const FontOptions& o) {
        if (!Initialized && !Init()) { return -1; }
        std::string file = o.File;
        std::error_code ec;
        if (file.empty() || !std::filesystem::exists(std::filesystem::path(std::u8string(file.begin(), file.end())), ec)) { file = o.Name.empty() ? std::string() : FindFontFile(o.Name); }
        if (file.empty()) { file = DefaultFontFile(); }
        if (file.empty()) { return -1; }
        const float px = std::max(o.Size, 1.0f) * RasterScale;
        TTF_Font* fill = TTF_OpenFont(file.c_str(), px);
        if (!fill) { return -1; }
        // フォントファイルの中に名前の違うフォントが複数ある (.ttc) 時は、名前の合うものを選ぶ
        if (!o.Name.empty() && !SameName(fill, o.Name)) {
            for (int index = 1; index < 16; ++index) {
                SDL_PropertiesID props = SDL_CreateProperties();
                SDL_SetStringProperty(props, TTF_PROP_FONT_CREATE_FILENAME_STRING, file.c_str());
                SDL_SetFloatProperty(props, TTF_PROP_FONT_CREATE_SIZE_FLOAT, px);
                SDL_SetNumberProperty(props, TTF_PROP_FONT_CREATE_FACE_NUMBER, index);
                TTF_Font* other = TTF_OpenFontWithProperties(props);
                SDL_DestroyProperties(props);
                if (!other) { break; }
                if (SameName(other, o.Name)) { TTF_CloseFont(fill); fill = other; break; }
                TTF_CloseFont(other);
            }
        }
        if (o.Thick >= 8) { TTF_SetFontStyle(fill, TTF_STYLE_BOLD); }     // DxLib の太さ (0～9、既定 6) で 8 以上を太字にする
        FontEntry e;
        e.Fill = fill;
        e.Size = px / RasterScale;
        e.Edge = std::max(o.Edge, 0);
        e.Space = o.Space;
        TTF_SetFontHinting(fill, TTF_HINTING_LIGHT);
        e.OffsetY = (e.Size - TTF_GetFontHeight(fill) / RasterScale) * 0.5f;
        if (e.Edge > 0) {
            e.Outline = TTF_CopyFont(fill);
            if (e.Outline) { TTF_SetFontOutline(e.Outline, (int)(e.Edge * RasterScale)); }
        }
        e.Used = true;
        for (int i = 0; i < (int)Fonts.size(); ++i) {
            if (!Fonts[(size_t)i].Used) { Fonts[(size_t)i] = e; return i; }
        }
        Fonts.push_back(e);
        return (int)Fonts.size() - 1;
    }
    void CloseFont(int handle) {
        if (!IsValid(handle)) { return; }
        ForgetFont(handle);
        auto& f = Fonts[(size_t)handle];
        if (f.Outline) { TTF_CloseFont(f.Outline); }
        if (f.Fill) { TTF_CloseFont(f.Fill); }
        f = FontEntry{};
    }
    bool IsValid(int handle) { return handle >= 0 && handle < (int)Fonts.size() && Fonts[(size_t)handle].Used; }
    float Size(int handle) { return IsValid(handle) ? Fonts[(size_t)handle].Size : 0.0f; }

    int Width(int handle, std::string_view text) {
        if (!IsValid(handle) || text.empty()) { return 0; }
        if (text.find('\n') != std::string_view::npos) {
            int widest = 0;
            for (size_t start = 0; start < text.size();) {
                const auto end = text.find('\n', start);
                auto line = text.substr(start, end == std::string_view::npos ? text.size()-start : end-start);
                if (line.ends_with('\r')) { line.remove_suffix(1); }
                widest = std::max(widest, Width(handle, line));
                if (end == std::string_view::npos) { break; }
                start = end + 1;
            }
            return widest;
        }
        int w = 0, h = 0;
        const auto& f = Fonts[(size_t)handle];
        TTF_GetStringSize(f.Fill, text.data(), text.size(), &w, &h);
        w = (int)std::lround(w / RasterScale);
        // Match the rendered text exactly. SDL_ttf does not apply our legacy
        // letter spacing value; adding it only here made right aligned text overflow.
        return w;
    }

    void Draw(int handle, float x, float y, std::string_view text, gfx::Color color, gfx::Color edge, float scalex, float scaley) {
        if (!IsValid(handle) || text.empty()) { return; }
        if (!std::isfinite(scalex) || !std::isfinite(scaley) || scalex <= 0 || scaley <= 0) { return; }
        if (text.find('\n') != std::string_view::npos) {
            const float lineHeight = TTF_GetFontLineSkip(Fonts[(size_t)handle].Fill) / RasterScale * scaley;
            for (size_t start = 0; start < text.size();) {
                const auto end = text.find('\n', start);
                auto line = text.substr(start, end == std::string_view::npos ? text.size()-start : end-start);
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
        const Uint8 alpha = (Uint8)(gfx::CurrentBlend() == gfx::Blend::None ? 255 : gfx::CurrentBlendParam());
        const float ty = y + f.OffsetY * scaley;
        auto draw = [&](SDL_Texture* texture, int width, int height, float left, float top, gfx::Color tint) {
            if (!texture) { return; }
            SDL_SetTextureColorMod(texture, (Uint8)gfx::R(tint), (Uint8)gfx::G(tint), (Uint8)gfx::B(tint));
            SDL_SetTextureAlphaMod(texture, alpha);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            const SDL_FRect dst{left, top, width / RasterScale * scalex, height / RasterScale * scaley};
            SDL_RenderTexture(gfx::sdl::Renderer(), texture, nullptr, &dst);
        };
        draw(e->Outline, e->OutlineW, e->OutlineH, x-f.Edge*scalex, ty-f.Edge*scaley, edge);
        draw(e->Fill, e->FillW, e->FillH, x, ty, color);
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
