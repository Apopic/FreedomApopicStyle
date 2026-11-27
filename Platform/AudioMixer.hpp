#pragma once
// 音声の出力 (WASAPI / SDL) からミキサーを呼ぶ (ゲームからは使わない)
namespace snd::mixer {
    void SetOutputRate(int rate);
    int OutputRate();
    // frames 分の音を、ステレオの float (-1～1) で out に書く (出力のスレッドから呼ぶ)
    void Render(float* out, int frames);
}
