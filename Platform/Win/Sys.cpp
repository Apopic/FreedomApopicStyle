// ウィンドウ・イベント・時刻・マウス・タッチ・文字入力 (Win32)
#include "../Sys.hpp"
#include "WinInternal.hpp"
#include <imm.h>
#include <shellapi.h>
#include <timeapi.h>
#include <algorithm>
#include <atomic>
#include <random>
#include <vector>

#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

namespace sys {
    namespace {
        HWND Window = nullptr;
        bool Quit = false;
        int MouseX = 0, MouseY = 0;         // クライアント座標 (物理ピクセル)
        int ButtonsDown = 0;                // クライアント領域で押したボタン
        int Wheel = 0;
        float WheelAccum = 0;
        bool CursorVisible = true;
        struct Finger { uint32_t Id; int X, Y; };   // クライアント座標
        std::vector<Finger> Fingers;
        std::mt19937 Random{ std::random_device{}() };
        bool DropEnabled = true;
        std::vector<std::string> Dropped;
        MessageHandler OnMessage;
        LARGE_INTEGER Frequency{}, Start{};
        HANDLE DelayTimer = nullptr;

        // フルスクリーン (枠なしでモニター全体を覆う) にする前のウィンドウ
        bool FullScreen = false;
        LONG_PTR WindowedStyle = 0;
        RECT WindowedRect{};

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
        wchar_t HighSurrogate = 0;

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
        void Insert(std::string t) {
            if (Input.NumberOnly) { std::erase_if(t, [](char c) { return !(std::isdigit((unsigned char)c) || c == '.' || c == '-' || c == '/' || c == ':'); }); }
            // 改行・タブなどの制御文字は入れない
            std::erase_if(t, [](char c) { return (unsigned char)c < 0x20 || c == 0x7F; });
            if (t.empty() || Input.Text.size() + t.size() > Input.MaxBytes) { return; }
            Input.Text.insert(Input.Cursor, t);
            Input.Cursor += t.size();
        }
        std::string ClipboardText() {
            std::string out;
            if (!OpenClipboard(Window)) { return out; }
            if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
                if (const wchar_t* p = (const wchar_t*)GlobalLock(h)) { out = win::Narrow(p); GlobalUnlock(h); }
            }
            CloseClipboard();
            return out;
        }
        void TextKey(WPARAM vk) {
            if (!Input.Active || Input.State != 0 || !Input.Composition.empty()) { return; }
            switch (vk) {
            case VK_RETURN: Input.State = 1; break;
            case VK_ESCAPE: Input.State = 2; break;
            case VK_BACK:
                if (Input.Cursor > 0) { const size_t p = PrevChar(Input.Text, Input.Cursor); Input.Text.erase(p, Input.Cursor - p); Input.Cursor = p; }
                break;
            case VK_DELETE:
                if (Input.Cursor < Input.Text.size()) { Input.Text.erase(Input.Cursor, NextChar(Input.Text, Input.Cursor) - Input.Cursor); }
                break;
            case VK_LEFT: Input.Cursor = PrevChar(Input.Text, Input.Cursor); break;
            case VK_RIGHT: Input.Cursor = NextChar(Input.Text, Input.Cursor); break;
            case VK_HOME: Input.Cursor = 0; break;
            case VK_END: Input.Cursor = Input.Text.size(); break;
            case 'V':
                if (GetKeyState(VK_CONTROL) & 0x8000) { Insert(ClipboardText()); }
                break;
            default: break;
            }
        }
        // IME は文字入力の間だけ使う (演奏中のキーを IME に取られないように)
        void EnableIme(bool on) {
            if (Window) { ImmAssociateContextEx(Window, nullptr, on ? IACE_DEFAULT : 0); }
        }
        std::string CompositionString(HIMC imc, DWORD kind) {
            const LONG bytes = ImmGetCompositionStringW(imc, kind, nullptr, 0);
            if (bytes <= 0) { return {}; }
            std::wstring w((size_t)bytes / sizeof(wchar_t), L'\0');
            ImmGetCompositionStringW(imc, kind, w.data(), (DWORD)bytes);
            return win::Narrow(w);
        }
        int PhysicalButton(int vk) {
            // GetAsyncKeyState は物理的なボタンを返すので、左右の入れ替えを戻す
            if (GetSystemMetrics(SM_SWAPBUTTON)) { if (vk == VK_LBUTTON) { return VK_RBUTTON; } if (vk == VK_RBUTTON) { return VK_LBUTTON; } }
            return vk;
        }
        void TrackPointer(WPARAM wp, LPARAM lp, bool up) {
            POINTER_INPUT_TYPE type = PT_POINTER;
            const UINT32 id = GET_POINTERID_WPARAM(wp);
            if (!GetPointerType(id, &type) || type != PT_TOUCH) { return; }
            POINT p{ (short)LOWORD(lp), (short)HIWORD(lp) };
            ScreenToClient(Window, &p);
            auto it = std::ranges::find(Fingers, id, &Finger::Id);
            if (up) { if (it != Fingers.end()) { Fingers.erase(it); } return; }
            if (it == Fingers.end()) { Fingers.push_back({ id, p.x, p.y }); }
            else { it->X = p.x; it->Y = p.y; }
        }

        LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
            // マウスの状態は、ゲームの処理より前にすべての通知から読む
            switch (msg) {
            case WM_MOUSEMOVE: MouseX = (short)LOWORD(lp); MouseY = (short)HIWORD(lp); break;
            case WM_LBUTTONDOWN: ButtonsDown |= MouseLeft; MouseX = (short)LOWORD(lp); MouseY = (short)HIWORD(lp); break;
            case WM_RBUTTONDOWN: ButtonsDown |= MouseRight; MouseX = (short)LOWORD(lp); MouseY = (short)HIWORD(lp); break;
            case WM_MBUTTONDOWN: ButtonsDown |= MouseMiddle; MouseX = (short)LOWORD(lp); MouseY = (short)HIWORD(lp); break;
            case WM_LBUTTONUP: ButtonsDown &= ~MouseLeft; break;
            case WM_RBUTTONUP: ButtonsDown &= ~MouseRight; break;
            case WM_MBUTTONUP: ButtonsDown &= ~MouseMiddle; break;
            case WM_MOUSEWHEEL: WheelAccum += GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA; break;
            case WM_KILLFOCUS: ButtonsDown = 0; break;
            case WM_POINTERDOWN: case WM_POINTERUPDATE: TrackPointer(wp, lp, false); break;
            case WM_POINTERUP: case WM_POINTERCAPTURECHANGED: TrackPointer(wp, lp, true); break;
            default: break;
            }
            if (OnMessage) {
                bool handled = false;
                const intptr_t r = OnMessage(hwnd, msg, (uintptr_t)wp, (intptr_t)lp, handled);
                if (handled) { return (LRESULT)r; }
            }
            switch (msg) {
            case WM_CLOSE: Quit = true; return 0;
            case WM_ERASEBKGND: return 1;
            case WM_SIZE: win::NotifyResize(); break;
            case WM_DPICHANGED: {
                if (!FullScreen) {
                    const RECT* r = (const RECT*)lp;
                    SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
                }
                return 0;
            }
            case WM_SETCURSOR:
                if (!CursorVisible && LOWORD(lp) == HTCLIENT) { SetCursor(nullptr); return TRUE; }
                break;
            case WM_KEYDOWN: TextKey(wp); break;
            case WM_CHAR: {
                if (!Input.Active || Input.State != 0) { break; }
                const wchar_t c = (wchar_t)wp;
                if (IS_HIGH_SURROGATE(c)) { HighSurrogate = c; return 0; }
                std::wstring w;
                if (IS_LOW_SURROGATE(c)) { if (HighSurrogate) { w.push_back(HighSurrogate); } w.push_back(c); }
                else { w.push_back(c); }
                HighSurrogate = 0;
                Insert(win::Narrow(w));
                return 0;
            }
            case WM_IME_SETCONTEXT:
                // 変換中の文字は自前で描くので、IME の変換の窓は出さない
                lp &= ~ISC_SHOWUICOMPOSITIONWINDOW;
                return DefWindowProcW(hwnd, msg, wp, lp);
            case WM_IME_STARTCOMPOSITION: return 0;
            case WM_IME_COMPOSITION: {
                if (HIMC imc = ImmGetContext(hwnd)) {
                    if (lp & GCS_RESULTSTR) {
                        const std::string result = CompositionString(imc, GCS_RESULTSTR);
                        Input.Composition.clear();
                        if (Input.Active && Input.State == 0) { Insert(result); }
                    }
                    if (lp & GCS_COMPSTR) { Input.Composition = Input.Active ? CompositionString(imc, GCS_COMPSTR) : std::string(); }
                    ImmReleaseContext(hwnd, imc);
                }
                return 0;
            }
            case WM_IME_ENDCOMPOSITION: Input.Composition.clear(); return 0;
            case WM_DROPFILES: {
                HDROP drop = (HDROP)wp;
                if (DropEnabled) {
                    const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
                    for (UINT i = 0; i < count; ++i) {
                        const UINT len = DragQueryFileW(drop, i, nullptr, 0);
                        std::wstring path(len, L'\0');
                        DragQueryFileW(drop, i, path.data(), len + 1);
                        Dropped.push_back(win::Narrow(path));
                    }
                }
                DragFinish(drop);
                return 0;
            }
            default: break;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
    }

    namespace {
        LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
            try { return HandleMessage(hwnd, msg, wp, lp); }
            catch (...) { return DefWindowProcW(hwnd, msg, wp, lp); }
        }
    }

    bool PumpEvents() {
        Wheel = 0;
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) { Quit = true; continue; }
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        // ホイールは 1 ノッチずつ (高精度のホイール・タッチパッドの端数は持ち越す)
        Wheel = (int)WheelAccum;
        WheelAccum -= (float)Wheel;
        return !Quit;
    }
    bool QuitRequested() { return Quit; }
    void RequestQuit() { Quit = true; }

    uint64_t Ticks() { return (uint64_t)(TicksUs() / 1000); }
    int64_t TicksUs() {
        if (Frequency.QuadPart == 0) { QueryPerformanceFrequency(&Frequency); QueryPerformanceCounter(&Start); }
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        const int64_t d = now.QuadPart - Start.QuadPart;
        return (d / Frequency.QuadPart) * 1000000 + (d % Frequency.QuadPart) * 1000000 / Frequency.QuadPart;
    }
    int Rand(int max) { return max <= 0 ? 0 : std::uniform_int_distribution<int>(0, max)(Random); }
    void Delay(int ms) {
        if (ms <= 0) { Sleep(0); return; }
        // 高精度の待機タイマー (Windows 10 1803 以降) で 1ms 単位の誤差を抑える
        if (!DelayTimer) { DelayTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS); }
        if (DelayTimer) {
            LARGE_INTEGER due; due.QuadPart = -(LONGLONG)ms * 10000;
            if (SetWaitableTimer(DelayTimer, &due, 0, nullptr, nullptr, FALSE)) { WaitForSingleObject(DelayTimer, INFINITE); return; }
        }
        Sleep((DWORD)ms);
    }

    void MousePos(int& x, int& y) {
        float lx = (float)MouseX, ly = (float)MouseY;
        gfx::WindowToLogical((float)MouseX, (float)MouseY, lx, ly);
        x = (int)lx; y = (int)ly;
    }
    int MouseButtons() {
        // ウィンドウの外で離して通知が来なかったボタンは、実際の状態で戻す
        if ((ButtonsDown & MouseLeft) && !(GetAsyncKeyState(PhysicalButton(VK_LBUTTON)) & 0x8000)) { ButtonsDown &= ~MouseLeft; }
        if ((ButtonsDown & MouseRight) && !(GetAsyncKeyState(PhysicalButton(VK_RBUTTON)) & 0x8000)) { ButtonsDown &= ~MouseRight; }
        if ((ButtonsDown & MouseMiddle) && !(GetAsyncKeyState(VK_MBUTTON) & 0x8000)) { ButtonsDown &= ~MouseMiddle; }
        return ButtonsDown;
    }
    int TakeWheel() { const int w = Wheel; Wheel = 0; return w; }
    void ShowCursor(bool show) {
        if (CursorVisible == show) { return; }
        CursorVisible = show;
        POINT p{};
        if (Window && GetCursorPos(&p) && WindowFromPoint(p) == Window) { SetCursor(show ? LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)) : nullptr); }
    }

    int TouchCount() { return (int)Fingers.size(); }
    bool GetTouch(int index, Touch& out) {
        if (index < 0 || index >= (int)Fingers.size()) { return false; }
        float lx = 0, ly = 0;
        gfx::WindowToLogical((float)Fingers[(size_t)index].X, (float)Fingers[(size_t)index].Y, lx, ly);
        out.X = (int)lx;
        out.Y = (int)ly;
        out.Id = (int64_t)Fingers[(size_t)index].Id;
        return true;
    }

    int BeginTextInput(const std::string& initial, size_t maxBytes, bool numberOnly) {
        Input.Active = true;
        Input.Text = initial.substr(0, maxBytes);
        Input.Cursor = Input.Text.size();
        Input.Composition.clear();
        Input.MaxBytes = maxBytes;
        Input.NumberOnly = numberOnly;
        Input.State = 0;
        ++Input.Serial;
        EnableIme(!numberOnly);
        return Input.Serial;
    }
    void EndTextInput(int handle) {
        if (handle != Input.Serial || !Input.Active) { return; }
        Input.Active = false;
        Input.Composition.clear();
        if (Window) {
            if (HIMC imc = ImmGetContext(Window)) { ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CANCEL, 0); ImmReleaseContext(Window, imc); }
        }
        EnableIme(false);
    }
    int TextInputState(int handle) { return handle == Input.Serial ? Input.State : 2; }
    std::string TextInputString(int handle) { return handle == Input.Serial ? Input.Text : std::string(); }
    void SetTextInputString(int handle, const std::string& text) {
        if (handle != Input.Serial) { return; }
        Input.Text = text.substr(0, Input.MaxBytes);
        Input.Cursor = Input.Text.size();
    }
    void SetTextInputFont(int font) { InputFont = font; }
    void DrawTextInput(int handle, float x, float y, gfx::Color color, gfx::Color cursor) {
        if (handle != Input.Serial || !Input.Active || !txt::IsValid(InputFont)) { return; }
        const std::string before = Input.Text.substr(0, Input.Cursor);
        const std::string after = Input.Text.substr(Input.Cursor);
        float cx = x;
        txt::Draw(InputFont, cx, y, before, color);
        cx += (float)txt::Width(InputFont, before);
        const float h = txt::Size(InputFont);
        const float compositionX = cx;
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
        if (Window) {
            if (HIMC imc = ImmGetContext(Window)) {
                float wx = compositionX, wy = y, bx = 0, by = 0;
                gfx::LogicalToWindow(compositionX, y, wx, wy);
                gfx::LogicalToWindow(compositionX, y + h, bx, by);
                COMPOSITIONFORM cf{};
                cf.dwStyle = CFS_POINT;
                cf.ptCurrentPos = { (LONG)wx, (LONG)wy };
                ImmSetCompositionWindow(imc, &cf);
                CANDIDATEFORM cand{};
                cand.dwIndex = 0;
                cand.dwStyle = CFS_EXCLUDE;
                cand.ptCurrentPos = { (LONG)wx, (LONG)by };
                cand.rcArea = { (LONG)wx, (LONG)wy, (LONG)wx + 1, (LONG)by };
                ImmSetCandidateWindow(imc, &cand);
                ImmReleaseContext(Window, imc);
            }
        }
    }

    bool KeyDown(Key key) {
        if (!Window || GetForegroundWindow() != Window) { return false; }
        return (GetKeyState(key == Key::Return ? VK_RETURN : VK_ESCAPE) & 0x8000) != 0;
    }
    void SetDropEnabled(bool enabled) { DropEnabled = enabled; if (!enabled) { Dropped.clear(); } }
    std::vector<std::string> TakeDroppedFiles() { std::vector<std::string> out; out.swap(Dropped); return out; }
    size_t DroppedFileCount() { return Dropped.size(); }
    void SetClipboard(const std::string& text) {
        if (!OpenClipboard(Window)) { return; }
        EmptyClipboard();
        const std::wstring w = win::Widen(text);
        if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t))) {
            if (void* p = GlobalLock(h)) {
                std::memcpy(p, w.c_str(), (w.size() + 1) * sizeof(wchar_t));
                GlobalUnlock(h);
                if (!SetClipboardData(CF_UNICODETEXT, h)) { GlobalFree(h); }
            }
            else { GlobalFree(h); }
        }
        CloseClipboard();
    }

    void* NativeWindow() { return Window; }
    void SetMessageHandler(MessageHandler handler) { OnMessage = std::move(handler); }
}

namespace win {
    HWND CreateMainWindow(const WindowOptions& o) {
        if (sys::Window) { return sys::Window; }
        // 高 DPI の画面ではウィンドウを拡大し、描画は実際の画素数で行う (ぼやけないように)
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        timeBeginPeriod(1);     // Sleep・待機の精度を 1ms にする
        sys::TicksUs();
        HINSTANCE inst = GetModuleHandleW(nullptr);
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = sys::WndProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.lpszClassName = L"FreedomApopicStyleWindow";
        if (o.IconResource) {
            wc.hIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(o.IconResource), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
            wc.hIconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(o.IconResource), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
        }
        RegisterClassExW(&wc);

        const DWORD style = WS_OVERLAPPEDWINDOW & ~(o.Resizable ? 0 : (WS_THICKFRAME | WS_MAXIMIZEBOX));
        // 主モニターの拡大率に合わせた大きさで、画面の中央に置く
        POINT origin{ 0, 0 };
        HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
        UINT dpi = 96;
        {
            using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
            if (HMODULE shcore = LoadLibraryW(L"shcore.dll")) {
                if (auto fn = (GetDpiForMonitorFn)GetProcAddress(shcore, "GetDpiForMonitor")) { UINT dx = 96, dy = 96; if (SUCCEEDED(fn(monitor, 0, &dx, &dy))) { dpi = dx; } }
            }
        }
        RECT r{ 0, 0, MulDiv(o.ClientW, (int)dpi, 96), MulDiv(o.ClientH, (int)dpi, 96) };
        AdjustWindowRectExForDpi(&r, style, FALSE, 0, dpi);
        MONITORINFO mi{ sizeof(mi) };
        GetMonitorInfoW(monitor, &mi);
        const int ww = r.right - r.left, wh = r.bottom - r.top;
        const int x = mi.rcWork.left + std::max(0, ((int)(mi.rcWork.right - mi.rcWork.left) - ww) / 2);
        const int y = mi.rcWork.top + std::max(0, ((int)(mi.rcWork.bottom - mi.rcWork.top) - wh) / 2);
        sys::Window = CreateWindowExW(0, wc.lpszClassName, o.Title.c_str(), style, x, y, ww, wh, nullptr, nullptr, inst, nullptr);
        if (!sys::Window) { return nullptr; }
        DragAcceptFiles(sys::Window, TRUE);
        sys::EnableIme(false);
        if (o.FullScreen) { SetFullScreen(true); }
        if (!o.Hidden) { ShowWindow(sys::Window, SW_SHOW); }
        return sys::Window;
    }
    void DestroyMainWindow() {
        if (sys::Window) { DestroyWindow(sys::Window); sys::Window = nullptr; }
        if (sys::DelayTimer) { CloseHandle(sys::DelayTimer); sys::DelayTimer = nullptr; }
        timeEndPeriod(1);
    }
    HWND MainWindow() { return sys::Window; }
    void SetFullScreen(bool on) {
        HWND w = sys::Window;
        if (!w || on == sys::FullScreen) { return; }
        const bool visible = IsWindowVisible(w) != FALSE;
        if (on) {
            sys::WindowedStyle = GetWindowLongPtrW(w, GWL_STYLE);
            GetWindowRect(w, &sys::WindowedRect);
            MONITORINFO mi{ sizeof(mi) };
            GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &mi);
            SetWindowLongPtrW(w, GWL_STYLE, (sys::WindowedStyle & ~WS_OVERLAPPEDWINDOW) | WS_POPUP | (visible ? WS_VISIBLE : 0));
            SetWindowPos(w, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOOWNERZORDER | (visible ? 0 : SWP_NOACTIVATE));
        }
        else {
            SetWindowLongPtrW(w, GWL_STYLE, sys::WindowedStyle | (visible ? WS_VISIBLE : 0));
            const RECT& r = sys::WindowedRect;
            SetWindowPos(w, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER | (visible ? 0 : SWP_NOACTIVATE));
        }
        sys::FullScreen = on;
        NotifyResize();
    }
    bool IsFullScreen() { return sys::FullScreen; }
    void ClientSize(int& w, int& h) {
        RECT r{};
        if (sys::Window) { GetClientRect(sys::Window, &r); }
        w = r.right - r.left; h = r.bottom - r.top;
    }
}
