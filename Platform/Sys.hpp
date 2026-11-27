#pragma once
// イベント処理・時刻・乱数・マウス・タッチ・文字入力 (Windows: Win32 / Android: SDL3)。
// マウスの座標は論理解像度 (スキンの解像度) の座標で返す。
#include "Text.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace sys {
    // イベントを処理する。終了の要求があれば false (DxLib の ProcessMessage が -1 を返す時に当たる)
    bool PumpEvents();
    bool QuitRequested();
    void RequestQuit();

    uint64_t Ticks();           // ミリ秒 (起動から)
    int64_t TicksUs();          // マイクロ秒 (起動から)
    int Rand(int max);          // 0～max (max を含む)
    void Delay(int ms);

    // ---- マウス (ボタンは 1: 左, 2: 右, 4: 中) ----
    enum MouseButton : int { MouseLeft = 1, MouseRight = 2, MouseMiddle = 4 };
    void MousePos(int& x, int& y);
    int MouseButtons();
    int TakeWheel();            // 前に呼んでから回した量 (奥が正)
    void ShowCursor(bool show);

    // ---- タッチ (論理座標) ----
    struct Touch { int X = 0, Y = 0; int64_t Id = 0; };
    int TouchCount();
    bool GetTouch(int index, Touch& out);

    // ---- 文字入力 (IME を含む)。一度に 1 つだけ ----
    // 番号を返す (-1 は失敗)。maxBytes: UTF-8 の最大バイト数
    int BeginTextInput(const std::string& initial, size_t maxBytes, bool numberOnly = false);
    void EndTextInput(int handle);
    // 複数の入力欄がある時に、キーと IME を受け取る欄を選ぶ (BeginTextInput で作った欄は作った時に選ばれる)
    void SetActiveTextInput(int handle);
    // 0: 入力中, 1: 決定 (Enter), 2: キャンセル (Esc)
    int TextInputState(int handle);
    std::string TextInputString(int handle);
    void SetTextInputString(int handle, const std::string& text);
    void SetTextInputFont(int font);
    // 入力欄の文字・カーソル・変換中の文字を (x, y) から描く
    void DrawTextInput(int handle, float x, float y, gfx::Color color = 0xFFFFFF, gfx::Color cursor = 0xFFFFFF);

    // ---- キー (ゲームのキー設定は Windows の仮想キーコードで、Library/Input.hpp が扱う。ここは画面の操作用) ----
    enum class Key { Return, Escape };
    bool KeyDown(Key key);

    // ---- ファイルのドロップ ----
    void SetDropEnabled(bool enabled);
    std::vector<std::string> TakeDroppedFiles();
    size_t DroppedFileCount();

    void SetClipboard(const std::string& text);

    // ネイティブのウィンドウ (Windows は HWND。Android は nullptr)
    void* NativeWindow();
    // Windows のウィンドウのメッセージを先に受け取る (handled を true にすると既定の処理をしない)
    using MessageHandler = std::function<intptr_t(void* hwnd, unsigned msg, uintptr_t wparam, intptr_t lparam, bool& handled)>;
    void SetMessageHandler(MessageHandler handler);
}
