#include "../Sys.hpp"
#include "SdlInternal.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <map>
#include <random>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace sys {
    namespace {
        bool Quit = false;
        float MouseX = 0, MouseY = 0;
        int Wheel = 0;
        float WheelAccum = 0;
        std::vector<std::pair<SDL_FingerID, SDL_FPoint>> Fingers;
        std::mt19937 Random{ std::random_device{}() };
        bool DropEnabled = true;
        std::vector<std::string> Dropped;
        MessageHandler OnMessage;

        struct TextInput {
            bool Active = false;
            std::string Text;           // 確定した文字
            size_t Cursor = 0;          // バイト位置
            std::string Composition;    // 変換中の文字
            size_t MaxBytes = 256;
            bool NumberOnly = false;
            int State = 0;
            int Serial = 0;
        } Input;
        int InputFont = -1;
        // 入力欄は複数持てる。キーが届くのは Input (選んでいる欄) だけで、ほかの欄はここに置いておく
        std::map<int, TextInput> OtherInputs;
        int NextSerial = 0;
        const TextInput* FindInput(int handle) {
            if (handle == Input.Serial && Input.Active) { return &Input; }
            auto it = OtherInputs.find(handle);
            return it != OtherInputs.end() ? &it->second : nullptr;
        }

#ifdef _WIN32
        WNDPROC SdlProc = nullptr;
        LRESULT CALLBACK SubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
            if (OnMessage) {
                bool handled = false;
                const intptr_t r = OnMessage(hwnd, msg, (uintptr_t)wp, (intptr_t)lp, handled);
                if (handled) { return (LRESULT)r; }
            }
            return CallWindowProcW(SdlProc, hwnd, msg, wp, lp);
        }
#endif
        size_t PrevChar(const std::string& s, size_t pos) {
            if (pos == 0) { return 0; }
            --pos;
            while (pos > 0 && ((unsigned char)s[pos] & 0xC0) == 0x80) { --pos; }
            return pos;
        }
        size_t NextChar(const std::string& s, size_t pos) {
            if (pos >= s.size()) { return s.size(); }
            ++pos;
            while (pos < s.size() && ((unsigned char)s[pos] & 0xC0) == 0x80) { ++pos; }
            return pos;
        }
        void Insert(const char* text) {
            std::string t(text);
            if (Input.NumberOnly) { std::erase_if(t, [](char c) { return !(std::isdigit((unsigned char)c) || c == '.' || c == '-' || c == '/' || c == ':'); }); }
            if (Input.Text.size() + t.size() > Input.MaxBytes) { return; }
            Input.Text.insert(Input.Cursor, t);
            Input.Cursor += t.size();
        }
        void TextKey(const SDL_KeyboardEvent& k) {
            if (!Input.Active || Input.State != 0 || !Input.Composition.empty()) { return; }
            switch (k.key) {
            case SDLK_RETURN: case SDLK_KP_ENTER: Input.State = 1; break;
            case SDLK_ESCAPE: Input.State = 2; break;
            case SDLK_BACKSPACE:
                if (Input.Cursor > 0) { const size_t p = PrevChar(Input.Text, Input.Cursor); Input.Text.erase(p, Input.Cursor - p); Input.Cursor = p; }
                break;
            case SDLK_DELETE:
                if (Input.Cursor < Input.Text.size()) { Input.Text.erase(Input.Cursor, NextChar(Input.Text, Input.Cursor) - Input.Cursor); }
                break;
            case SDLK_LEFT: Input.Cursor = PrevChar(Input.Text, Input.Cursor); break;
            case SDLK_RIGHT: Input.Cursor = NextChar(Input.Text, Input.Cursor); break;
            case SDLK_HOME: Input.Cursor = 0; break;
            case SDLK_END: Input.Cursor = Input.Text.size(); break;
            case SDLK_V:
                if (k.mod & SDL_KMOD_CTRL) { if (char* clip = SDL_GetClipboardText()) { Insert(clip); SDL_free(clip); } }
                break;
            default: break;
            }
        }
    }

    bool PumpEvents() {
#ifdef _WIN32
        // 初めて呼ばれた時に SDL のウィンドウプロシージャの前に自前の処理を挟む
        if (!SdlProc && gfx::sdl::Window()) {
            if (HWND hwnd = (HWND)NativeWindow()) { SdlProc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)&SubclassProc); }
        }
#endif
        Wheel = 0;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_EVENT_QUIT: Quit = true; break;
            case SDL_EVENT_MOUSE_MOTION: gfx::WindowToLogical(e.motion.x, e.motion.y, MouseX, MouseY); break;
            case SDL_EVENT_MOUSE_WHEEL:
                WheelAccum += e.wheel.y;
                break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_MOTION: {
                SDL_FPoint p{ e.tfinger.x * gfx::LogicalWidth(), e.tfinger.y * gfx::LogicalHeight() };
                auto it = std::ranges::find(Fingers, e.tfinger.fingerID, &std::pair<SDL_FingerID, SDL_FPoint>::first);
                if (it == Fingers.end()) { Fingers.push_back({ e.tfinger.fingerID, p }); }
                else { it->second = p; }
                break;
            }
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_CANCELED:
                std::erase_if(Fingers, [&](const auto& f) { return f.first == e.tfinger.fingerID; });
                break;
            case SDL_EVENT_TEXT_INPUT: if (Input.Active && Input.State == 0) { Insert(e.text.text); Input.Composition.clear(); } break;
            case SDL_EVENT_TEXT_EDITING: if (Input.Active) { Input.Composition = e.edit.text ? e.edit.text : ""; } break;
            case SDL_EVENT_KEY_DOWN: TextKey(e.key); break;
            case SDL_EVENT_DROP_FILE: if (DropEnabled && e.drop.data) { Dropped.emplace_back(e.drop.data); } break;
            default: break;
            }
        }
        // ホイールは 1 ノッチずつ (高精度のホイール・タッチパッドの端数は持ち越す)
        Wheel = (int)WheelAccum;
        WheelAccum -= (float)Wheel;
        return !Quit;
    }
    bool QuitRequested() { return Quit; }
    void RequestQuit() { Quit = true; }

    uint64_t Ticks() { return SDL_GetTicks(); }
    int64_t TicksUs() { return (int64_t)(SDL_GetTicksNS() / 1000); }
    int Rand(int max) { return max <= 0 ? 0 : std::uniform_int_distribution<int>(0, max)(Random); }
    void Delay(int ms) { SDL_Delay((Uint32)std::max(ms, 0)); }

    void MousePos(int& x, int& y) { x = (int)MouseX; y = (int)MouseY; }
    int MouseButtons() {
        const SDL_MouseButtonFlags f = SDL_GetMouseState(nullptr, nullptr);
        return ((f & SDL_BUTTON_LMASK) ? MouseLeft : 0) | ((f & SDL_BUTTON_RMASK) ? MouseRight : 0) | ((f & SDL_BUTTON_MMASK) ? MouseMiddle : 0);
    }
    int TakeWheel() { const int w = Wheel; Wheel = 0; return w; }
    void ShowCursor(bool show) { show ? SDL_ShowCursor() : SDL_HideCursor(); }

    int TouchCount() { return (int)Fingers.size(); }
    bool GetTouch(int index, Touch& out) {
        if (index < 0 || index >= (int)Fingers.size()) { return false; }
        out.X = (int)Fingers[(size_t)index].second.x;
        out.Y = (int)Fingers[(size_t)index].second.y;
        out.Id = (int64_t)Fingers[(size_t)index].first;
        return true;
    }

    int BeginTextInput(const std::string& initial, size_t maxBytes, bool numberOnly) {
        if (Input.Active) { Input.Composition.clear(); OtherInputs[Input.Serial] = Input; }
        Input = TextInput{};
        Input.Active = true;
        Input.Text = initial.substr(0, maxBytes);
        Input.Cursor = Input.Text.size();
        Input.Composition.clear();
        Input.MaxBytes = maxBytes;
        Input.NumberOnly = numberOnly;
        Input.State = 0;
        Input.Serial = ++NextSerial;
        if (gfx::sdl::Window()) { SDL_StartTextInput(gfx::sdl::Window()); }
        return Input.Serial;
    }
    void SetActiveTextInput(int handle) {
        if (handle == Input.Serial && Input.Active) { return; }
        auto it = OtherInputs.find(handle);
        if (it == OtherInputs.end()) { return; }
        if (Input.Active) { Input.Composition.clear(); OtherInputs[Input.Serial] = Input; }
        Input = it->second;
        OtherInputs.erase(handle);
    }
    void EndTextInput(int handle) {
        if (OtherInputs.erase(handle)) { return; }
        if (handle != Input.Serial || !Input.Active) { return; }
        Input.Active = false;
        if (gfx::sdl::Window()) { SDL_StopTextInput(gfx::sdl::Window()); }
    }
    int TextInputState(int handle) { const TextInput* in = FindInput(handle); return in ? in->State : 2; }
    std::string TextInputString(int handle) { const TextInput* in = FindInput(handle); return in ? in->Text : std::string(); }
    void SetTextInputString(int handle, const std::string& text) {
        TextInput* in = const_cast<TextInput*>(FindInput(handle));
        if (!in) { return; }
        in->Text = text.substr(0, in->MaxBytes);
        in->Cursor = in->Text.size();
    }
    void SetTextInputFont(int font) { InputFont = font; }
    void DrawTextInput(int handle, float x, float y, gfx::Color color, gfx::Color cursor) {
        if (!txt::IsValid(InputFont)) { return; }
        if (handle != Input.Serial || !Input.Active) {
            if (const TextInput* in = FindInput(handle)) { txt::Draw(InputFont, x, y, in->Text, color); }
            return;
        }
        const std::string before = Input.Text.substr(0, Input.Cursor);
        const std::string after = Input.Text.substr(Input.Cursor);
        float cx = x;
        txt::Draw(InputFont, cx, y, before, color);
        cx += (float)txt::Width(InputFont, before);
        const float h = txt::Size(InputFont);
        if (!Input.Composition.empty()) {
            // 変換中の文字は下線を付けて、カーソルの位置に挟んで表示する
            txt::Draw(InputFont, cx, y, Input.Composition, color);
            const float w = (float)txt::Width(InputFont, Input.Composition);
            gfx::Line(cx, y + h + 1, cx + w, y + h + 1, color, 1.0f);
            cx += w;
        }
        if ((Ticks() / 500) % 2 == 0) { gfx::Line(cx, y, cx, y + h, cursor, 2.0f); }
        txt::Draw(InputFont, cx + 2, y, after, color);
        // IME の候補の窓を入力位置の近くに出す
        if (gfx::sdl::Window()) {
            float wx = cx, wy = y;
            gfx::LogicalToWindow(cx, y, wx, wy);
            const SDL_Rect r{ (int)wx, (int)wy, 1, (int)h };
            SDL_SetTextInputArea(gfx::sdl::Window(), &r, 0);
        }
    }

    bool KeyDown(Key key) {
        const int scancode = key == Key::Return ? SDL_SCANCODE_RETURN : SDL_SCANCODE_ESCAPE;
        int count = 0;
        const bool* keys = SDL_GetKeyboardState(&count);
        return keys && scancode >= 0 && scancode < count && keys[scancode];
    }
    void SetDropEnabled(bool enabled) { DropEnabled = enabled; if (!enabled) { Dropped.clear(); } }
    std::vector<std::string> TakeDroppedFiles() { std::vector<std::string> out; out.swap(Dropped); return out; }
    size_t DroppedFileCount() { return Dropped.size(); }
    void SetClipboard(const std::string& text) { SDL_SetClipboardText(text.c_str()); }

    void* NativeWindow() {
#ifdef _WIN32
        if (!gfx::sdl::Window()) { return nullptr; }
        return SDL_GetPointerProperty(SDL_GetWindowProperties(gfx::sdl::Window()), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
        return nullptr;
#endif
    }
    void SetMessageHandler(MessageHandler handler) { OnMessage = std::move(handler); }
}
