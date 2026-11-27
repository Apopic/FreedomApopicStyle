#pragma once
#include "../Platform/Sys.hpp"
#include "RawInputQueue.hpp"
#include <cstdint>
#ifndef __ANDROID__
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "libarrier/timer.hpp"
#include <vector>
#include <algorithm>
#include <atomic>
#include <future>
#include <mutex>
#include <thread>

#ifndef __ANDROID__
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

inline bool ApplyDarkWindowMode(HWND window) {
    const BOOL dark = TRUE;
    HRESULT result = DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    if (FAILED(result)) { result = DwmSetWindowAttribute(window, 19, &dark, sizeof(dark)); }
    return SUCCEEDED(result);
}
inline LRESULT CaptionHitTest(HWND window, POINT point) {
    // DWM may report only the glyph area. Use the native button rectangles for
    // the entire visible hit area, including their padding and DPI scaling.
    TITLEBARINFOEX titlebar{};
    titlebar.cbSize = sizeof(titlebar);
    SendMessageW(window, WM_GETTITLEBARINFOEX, 0, (LPARAM)&titlebar);
    for (const auto& button : {std::pair{2, HTMINBUTTON}, std::pair{3, HTMAXBUTTON}, std::pair{5, HTCLOSE}}) {
        if (!(titlebar.rgstate[button.first] & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_UNAVAILABLE | STATE_SYSTEM_OFFSCREEN))
            && PtInRect(&titlebar.rgrect[button.first], point)) { return button.second; }
    }
    LRESULT hit = 0;
    const LPARAM position = MAKELPARAM((short)point.x, (short)point.y);
    if (DwmDefWindowProc(window, WM_NCHITTEST, 0, position, &hit)) { return hit; }
    return DefWindowProc(window, WM_NCHITTEST, 0, position);
}

inline byte __gotkeyboardinput[256]{};

// ウィンドウの操作で Windows がモーダルループ (操作が終わるまで戻ってこない処理) に入ると、
// ゲームのループ・描画・入力・オンラインの通信処理が止まってしまう。次の操作は自前で処理してループを止めない:
//   タイトルバーのドラッグ (ウィンドウの移動) / 閉じる・最小化・最大化ボタン /
//   システムメニュー (タイトルバーの右クリック・アイコンのクリック・Alt+Space) / Alt・F10 キーのメニュー
struct WindowDragState {
    bool Active = false;    // タイトルバーをドラッグ中
    POINT Start{};          // ドラッグ開始時のカーソル位置 (スクリーン座標)
    RECT Rect{};            // ドラッグ開始時のウィンドウ位置
    WPARAM Button = 0;      // 押している最中のタイトルバーのボタン (HTCLOSE など。0 = なし)
};
inline WindowDragState __windowdrag;
inline bool IsWindowDragging() { return __windowdrag.Active || __windowdrag.Button != 0; }

inline void EndWindowDrag(HWND hWnd) {
    if (!__windowdrag.Active && __windowdrag.Button == 0) { return; }
    __windowdrag.Active = false;
    __windowdrag.Button = 0;
    if (GetCapture() == hWnd) { ReleaseCapture(); }
}
inline bool IsPrimaryButtonDown() {
    return (GetAsyncKeyState(GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON) & 0x8000) != 0;
}
// 離した位置がまだ同じボタンの上なら、そのボタンの操作を行う (既定の処理と同じ動き)
inline void FinishCaptionButton(HWND hWnd, WPARAM button, POINT released) {
    const LRESULT hit = CaptionHitTest(hWnd, released);
    if ((WPARAM)hit != button) { return; }
    switch (button) {
    case HTCLOSE: PostMessage(hWnd, WM_SYSCOMMAND, SC_CLOSE, 0); break;
    case HTMINBUTTON: PostMessage(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0); break;
    case HTMAXBUTTON: PostMessage(hWnd, WM_SYSCOMMAND, IsZoomed(hWnd) ? SC_RESTORE : SC_MAXIMIZE, 0); break;
    default: break;
    }
}

// ボタンを離した通知の座標で確定する。移動やキャプチャの解除はクリックとみなさない。
inline void ReleaseCaptionButton(HWND hWnd, POINT released) {
    const WPARAM button = __windowdrag.Button;
    if (button == 0) { return; }
    __windowdrag.Button = 0;
    if (GetCapture() == hWnd) { ReleaseCapture(); }
    FinishCaptionButton(hWnd, button, released);
}

// WndProc が「処理済み」(SDL・Windows の既定の処理を行わせない) にした通知か
inline bool __wndprochandled = false;
inline void SetUseHookWinProcReturnValue(int handled) { __wndprochandled = handled != 0; }

inline LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 前の通知で設定した「処理済み」を次の通知へ持ち越さない。
    SetUseHookWinProcReturnValue(FALSE);
    auto Handled = [] {
        SetUseHookWinProcReturnValue(TRUE);     // DxLib・Windows の既定の処理 (モーダルループ) を行わせない
        return (LRESULT)0;
    };
    switch (msg) {
    case WM_MOUSEACTIVATE:
        if (LOWORD(lParam) == HTCLOSE || LOWORD(lParam) == HTMINBUTTON || LOWORD(lParam) == HTMAXBUTTON || LOWORD(lParam) == HTCAPTION) {
            SetUseHookWinProcReturnValue(TRUE);
            return MA_ACTIVATE;    // 非アクティブなウィンドウの最初の押下を捨てない。
        }
        break;
    case WM_NCHITTEST: {
        // 枠のサイズ変更 (モーダルループに入る) を無効にする: 枠の上は普通の枠として扱う
        const POINT point{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        const LRESULT hit = CaptionHitTest(hWnd, point);
        if (hit >= HTLEFT && hit <= HTBOTTOMRIGHT) {
            SetUseHookWinProcReturnValue(TRUE);
            return HTBORDER;
        }
        SetUseHookWinProcReturnValue(TRUE);
        return hit;
    }
    case WM_NCLBUTTONDBLCLK:    // 連続クリックの2回目もボタンの押下として受け取る。
    case WM_NCLBUTTONDOWN:
        if (msg == WM_NCLBUTTONDOWN && wParam == HTCAPTION && !IsZoomed(hWnd) && GetCursorPos(&__windowdrag.Start) && GetWindowRect(hWnd, &__windowdrag.Rect)) {
            __windowdrag.Active = true;
            if (GetForegroundWindow() != hWnd) { SetForegroundWindow(hWnd); }
            SetCapture(hWnd);
            return Handled();
        }
        if (wParam == HTCLOSE || wParam == HTMINBUTTON || wParam == HTMAXBUTTON) {
            if (GetForegroundWindow() != hWnd) { SetForegroundWindow(hWnd); }
            __windowdrag.Button = wParam;           // 離した時に処理する
            SetCapture(hWnd);
            return Handled();
        }
        if (wParam == HTSYSMENU) {
            return Handled();                       // アイコンのクリックでシステムメニューを開かない
        }
        break;
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
        if (wParam == HTCAPTION || wParam == HTSYSMENU) {
            return Handled();                       // タイトルバーの右クリックでシステムメニューを開かない
        }
        break;
    case WM_SYSCOMMAND:
        switch (wParam & 0xFFF0) {
        case SC_KEYMENU:        // Alt・F10・Alt+Space のメニュー
        case SC_MOUSEMENU:
        case SC_MOVE:           // キーボードでの移動・サイズ変更 (どちらもモーダルループに入る)
        case SC_SIZE:
            return Handled();
        default:
            break;
        }
        break;
    case WM_MOUSEMOVE:
        if (__windowdrag.Active) {
            POINT p{};
            if (!IsPrimaryButtonDown()) {
                EndWindowDrag(hWnd);    // ボタンを離した通知を取りこぼした場合
            }
            else if (GetCursorPos(&p)) {
                SetWindowPos(hWnd, nullptr,
                    __windowdrag.Rect.left + (p.x - __windowdrag.Start.x),
                    __windowdrag.Rect.top + (p.y - __windowdrag.Start.y),
                    0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return Handled();
        }
        if (__windowdrag.Button != 0) {
            return Handled();
        }
        break;
    case WM_NCMOUSEMOVE:
    case WM_NCMOUSELEAVE: {
        LRESULT result = 0;
        DwmDefWindowProc(hWnd, msg, wParam, lParam, &result);
        break;
    }
    case WM_NCLBUTTONUP:
    case WM_LBUTTONUP:
        if (__windowdrag.Button != 0) {
            POINT released{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            if (msg == WM_LBUTTONUP) { ClientToScreen(hWnd, &released); }
            ReleaseCaptionButton(hWnd, released);
            return Handled();
        }
        if (__windowdrag.Active) {
            EndWindowDrag(hWnd);
            return Handled();                       // ドラッグの終わりをゲーム内のクリックとして扱わない
        }
        break;
    case WM_CAPTURECHANGED:
        if ((HWND)lParam != hWnd) {
            __windowdrag.Active = false;
            __windowdrag.Button = 0;
        }
        break;
    case WM_CANCELMODE:
        EndWindowDrag(hWnd);
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        __gotkeyboardinput[wParam & 0xFF] = 1;
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        __gotkeyboardinput[wParam & 0xFF] = 0;
        break;
    case WM_KILLFOCUS:
        // フォーカスを失うと WM_KEYUP が届かず押しっぱなしになるため全解除
        std::fill(std::begin(__gotkeyboardinput), std::end(__gotkeyboardinput), byte{ 0 });
        EndWindowDrag(hWnd);
        break;
    }

    return 0;
}

// 演奏判定用の低遅延キー入力。
// 専用スレッドで Raw Input を受け取り、押下時刻を QueryPerformanceCounter で記録する。
// メインループが描画や ScreenFlip の VSync 待ちで止まっていても、押した瞬間の時刻で判定できる。
class RawKeyboard {
public:
    struct Event {
        uint8_t VKey;
        bool Down;
        int64_t Time;   // QueryPerformanceCounter の値
    };

    static RawKeyboard& GetInstance() {
        static RawKeyboard instance;
        return instance;
    }

    RawKeyboard(const RawKeyboard&) = delete;
    RawKeyboard& operator=(const RawKeyboard&) = delete;

    // ForegroundWindow: このウィンドウがアクティブな時だけ入力を受け付ける
    bool Start(HWND ForegroundWindow) {
        if (m_Thread.joinable()) {
            return m_Active;
        }
        m_Foreground = ForegroundWindow;
        std::promise<bool> ready;
        auto result = ready.get_future();
        m_Thread = std::thread([this, p = std::move(ready)]() mutable { ThreadMain(std::move(p)); });
        m_Active = result.get();
        if (!m_Active && m_Thread.joinable()) {
            m_Thread.join();
        }
        return m_Active;
    }
    void Stop() {
        if (!m_Thread.joinable()) {
            return;
        }
        if (HWND hwnd = m_Window.load()) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        m_Thread.join();
        m_Active = false;
    }
    bool IsActive() const { return m_Active; }

    // 前回呼び出し以降のイベントを取り出す (out は上書き)
    void Drain(std::vector<Event>& out) {
        m_Events.Drain(out);
    }

    static int64_t Now() {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        return t.QuadPart;
    }
    static int64_t MsToTicks(double ms) {
        static const double freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return (double)f.QuadPart; }();
        return (int64_t)(ms * freq / 1000.0);
    }
    // Raw Input の受け取りを登録し直す (同じプロセスの別のウィンドウ (DxLib の DirectInput など) に登録を奪われた時)
    void Reregister() {
        if (HWND hwnd = m_Window.load()) { PostMessage(hwnd, ReregisterMessage, 0, 0); }
    }
    static double TicksToMs(int64_t ticks) {
        static const double freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return (double)f.QuadPart; }();
        return ticks * 1000.0 / freq;
    }

private:
    RawKeyboard() = default;
    ~RawKeyboard() { Stop(); }

    void ThreadMain(std::promise<bool> ready) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

        WNDCLASSEXA wc{ sizeof(WNDCLASSEXA) };
        wc.lpfnWndProc = &RawKeyboard::WndProcStatic;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "FASRawKeyboard";
        RegisterClassExA(&wc);

        HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, this);
        if (hwnd == nullptr) {
            ready.set_value(false);
            return;
        }
        if (!Register(hwnd)) {
            DestroyWindow(hwnd);
            ready.set_value(false);
            return;
        }
        m_Window = hwnd;
        ready.set_value(true);

        MSG msg;
        while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
            DispatchMessageA(&msg);
        }
        m_Window = nullptr;
    }

    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTA*>(lParam);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        }
        auto self = reinterpret_cast<RawKeyboard*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
        switch (msg) {
        case ReregisterMessage:
            Register(hwnd);
            return 0;
        case WM_INPUT:
            if (self) { self->OnInput(reinterpret_cast<HRAWINPUT>(lParam)); }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    static constexpr UINT ReregisterMessage = WM_APP + 1;
    static bool Register(HWND hwnd) {
        RAWINPUTDEVICE rid{ 0x01, 0x06, RIDEV_INPUTSINK, hwnd };   // Generic Desktop / Keyboard
        return RegisterRawInputDevices(&rid, 1, sizeof(rid)) != FALSE;
    }

    void OnInput(HRAWINPUT handle) {
        const int64_t time = Now();

        RAWINPUT raw{};
        UINT size = sizeof(raw);
        if (GetRawInputData(handle, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1) {
            return;
        }
        if (raw.header.dwType != RIM_TYPEKEYBOARD) {
            return;
        }
        const USHORT vkey = raw.data.keyboard.VKey;
        if (vkey == 0 || vkey >= 255) {
            return;
        }

        // 非アクティブ時の入力は無視し、押下状態もリセットする
        if (m_Foreground != nullptr && GetForegroundWindow() != m_Foreground) {
            std::fill(std::begin(m_KeyDown), std::end(m_KeyDown), false);
            return;
        }

        const bool down = (raw.data.keyboard.Flags & RI_KEY_BREAK) == 0;
        if (m_KeyDown[vkey] == down) {
            return;     // キーリピートは除外
        }
        m_KeyDown[vkey] = down;

        m_Events.Push({ static_cast<uint8_t>(vkey), down, time });
    }

    std::thread m_Thread;
    std::atomic<HWND> m_Window = nullptr;
    HWND m_Foreground = nullptr;
    std::atomic_bool m_Active = false;
    bool m_KeyDown[256]{};          // 入力スレッド専用
    RawInputQueue<Event> m_Events;
};

enum class KeyState {
    Null,
    Down,
    Hold,
    Up,
    Release
};

class Input {

    byte* __KeyBoardInput = __gotkeyboardinput;
    KeyState __KeyBoardState[256]{};
    libarrier::Timer HoldProcIntervalTimer;
    bool IsFirstInput = false;

    Input() {
        // SDL のウィンドウの通知を先に受け取る (処理済みにした通知は SDL に渡さない)
        sys::SetMessageHandler([](void* hwnd, unsigned msg, uintptr_t wp, intptr_t lp, bool& handled) -> intptr_t {
            const LRESULT result = WndProc((HWND)hwnd, (UINT)msg, (WPARAM)wp, (LPARAM)lp);
            handled = __wndprochandled;
            return (intptr_t)result;
        });
    }
    ~Input() = default;


public:

    static Input& GetInstance() {
        static Input Instance;
        return Instance;
    }


    template<typename Func>
    void HitKeyProcess(int keycode, KeyState state, const Func& func, double holdprocinterval = -1) {
        if (keycode < 0 || keycode >= 256) {
            return;
        }

        KeyState& keystate = __KeyBoardState[keycode];

        if (__KeyBoardInput[keycode]) {
            if (keystate == KeyState::Down) {
                keystate = KeyState::Hold;
                IsFirstInput = true;
            }
            else if (keystate != KeyState::Hold) {
                keystate = KeyState::Down;
            }
        }
        else {
            switch (keystate) {
            case KeyState::Down:
                keystate = KeyState::Up;
                break;
            case KeyState::Hold:
                keystate = KeyState::Up;
                HoldProcIntervalTimer.Reset();
                break;
            case KeyState::Up:
                keystate = KeyState::Release;
                break;
            default:
                break;
            }
        }

        if (keystate == KeyState::Hold && holdprocinterval != -1) {
            if (HoldProcIntervalTimer.GetElapsed().Second() < (IsFirstInput ? 0.5 : holdprocinterval)) {
                if (!HoldProcIntervalTimer.IsRunning()) {
                    HoldProcIntervalTimer.Start();
                }
                return;
            }
            HoldProcIntervalTimer.Reset();
            IsFirstInput = false;
        }
        else if (keystate != state) {
            return;
        }

        func();
    }

    // 文字入力などでキー処理を止めていた後に呼ぶ。今押されているキーは「押し続け」扱いにして、
    // 確定に使った Enter などが次のフレームで「押した瞬間」として誤動作しないようにする
    void ResyncKeys() {
        for (int key = 0; key < 256; ++key) {
            __KeyBoardState[key] = __KeyBoardInput[key] ? KeyState::Hold : KeyState::Release;
        }
        HoldProcIntervalTimer.Reset();
        IsFirstInput = true;
    }

    template<typename Func>
    void HitKeyesProcess(const std::vector<int>& keycodearray, KeyState state, const Func& func, double holdprocinterval = -1) {
        for (int keycode : keycodearray) {
            HitKeyProcess(keycode, state, func, holdprocinterval);
        }
    }

    int GetMouseWheel() { return sys::TakeWheel(); }
};

#else

class Input {

    struct TouchState {
        int id;
        int device;
        int x, y;
        bool is_pressed;
    };

    std::vector<TouchState> GetCurrentTouches() {
        std::vector<TouchState> current_touches;
        const int touch_num = sys::TouchCount();

        for (int i = 0; i < touch_num; ++i) {
            TouchState t{};
            sys::Touch touch;
            if (!sys::GetTouch(i, touch)) { continue; }
            t.x = touch.X; t.y = touch.Y; t.id = (int)touch.Id; t.device = 0;
            t.is_pressed = false;
            current_touches.push_back(t);
        }
        return current_touches;
    }

    std::vector<TouchState> prev_touches;

    Input() = default;

public:

    static Input& GetInstance() {
        static Input Instance;
        return Instance;
    }

    template<typename Func>
    void GetTouchDatas(const Func& func) {

        std::vector<TouchState> current_touches = GetCurrentTouches();

        for (auto& cur : current_touches) {
            auto it = std::find_if(prev_touches.begin(), prev_touches.end(),
                [&](const TouchState& prev) { return prev.id == cur.id; });
            if (it == prev_touches.end()) {
                cur.is_pressed = true;
            }
        }

        func();
        prev_touches = std::move(current_touches);
    }

};

#endif
