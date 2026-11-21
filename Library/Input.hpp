#pragma once
#include "DxLib.h"
#include "libarrier/timer.hpp"
#include <vector>
#include <algorithm>
#include <atomic>
#include <future>
#include <mutex>
#include <thread>

#ifndef __ANDROID__

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
inline void FinishCaptionButton(HWND hWnd, WPARAM button) {
    POINT p{};
    if (!GetCursorPos(&p)) { return; }
    const LRESULT hit = SendMessage(hWnd, WM_NCHITTEST, 0, MAKELPARAM((short)p.x, (short)p.y));
    if ((WPARAM)hit != button) { return; }
    switch (button) {
    case HTCLOSE: PostMessage(hWnd, WM_SYSCOMMAND, SC_CLOSE, 0); break;
    case HTMINBUTTON: PostMessage(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0); break;
    case HTMAXBUTTON: PostMessage(hWnd, WM_SYSCOMMAND, IsZoomed(hWnd) ? SC_RESTORE : SC_MAXIMIZE, 0); break;
    default: break;
    }
}

// 押している最中のタイトルバーのボタンを離した: どの通知 (WM_LBUTTONUP / WM_NCLBUTTONUP / 離した後の WM_MOUSEMOVE /
// キャプチャの解除) で気付いても、離した位置が同じボタンの上ならその操作を行う
// (以前は WM_MOUSEMOVE や キャプチャの解除で先に気付くと、操作をせずに取り消していたため、押しても反応しないことがあった)
inline void ReleaseCaptionButton(HWND hWnd) {
    const WPARAM button = __windowdrag.Button;
    if (button == 0) { return; }
    __windowdrag.Button = 0;
    if (GetCapture() == hWnd) { ReleaseCapture(); }
    FinishCaptionButton(hWnd, button);
}

inline LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto Handled = [] {
        SetUseHookWinProcReturnValue(TRUE);     // DxLib・Windows の既定の処理 (モーダルループ) を行わせない
        return (LRESULT)0;
    };
    switch (msg) {
    case WM_NCHITTEST: {
        // 枠のサイズ変更 (モーダルループに入る) を無効にする: 枠の上は普通の枠として扱う
        const LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);
        if (hit >= HTLEFT && hit <= HTBOTTOMRIGHT) {
            SetUseHookWinProcReturnValue(TRUE);
            return HTBORDER;
        }
        break;
    }
    case WM_NCLBUTTONDOWN:
        if (wParam == HTCAPTION && !IsZoomed(hWnd) && GetCursorPos(&__windowdrag.Start) && GetWindowRect(hWnd, &__windowdrag.Rect)) {
            __windowdrag.Active = true;
            if (GetForegroundWindow() != hWnd) { SetForegroundWindow(hWnd); }
            SetCapture(hWnd);
            return Handled();
        }
        if (wParam == HTCLOSE || wParam == HTMINBUTTON || wParam == HTMAXBUTTON) {
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
            if (!IsPrimaryButtonDown()) { ReleaseCaptionButton(hWnd); }
            return Handled();
        }
        break;
    case WM_NCMOUSEMOVE:
        if (__windowdrag.Button != 0 && !IsPrimaryButtonDown()) {
            ReleaseCaptionButton(hWnd);
            return Handled();
        }
        break;
    case WM_NCLBUTTONUP:
    case WM_LBUTTONUP:
        if (__windowdrag.Button != 0) {
            ReleaseCaptionButton(hWnd);
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
            // キャプチャを他に取られても、もう離していればボタンの操作を行う (押したままなら取り消す)
            if (__windowdrag.Button != 0 && !IsPrimaryButtonDown()) { ReleaseCaptionButton(hWnd); }
            __windowdrag.Button = 0;
        }
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
        out.clear();
        std::lock_guard<std::mutex> lock(m_Mutex);
        out.swap(m_Events);
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

        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Events.push_back({ static_cast<uint8_t>(vkey), down, time });
    }

    std::thread m_Thread;
    std::atomic<HWND> m_Window = nullptr;
    HWND m_Foreground = nullptr;
    bool m_Active = false;
    bool m_KeyDown[256]{};          // 入力スレッド専用
    std::mutex m_Mutex;
    std::vector<Event> m_Events;
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
        SetHookWinProc(WndProc);
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

    int GetMouseWheel() { return GetMouseWheelRotVol(); }
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
        int touch_num = GetTouchInputNum();

        for (int i = 0; i < touch_num; ++i) {
            TouchState t{};
            GetTouchInput(i, &t.x, &t.y, &t.id, &t.device);
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

#define Input Input::GetInstance()
