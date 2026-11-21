#pragma once
// 音の高さを変えずに再生速度を変える (WSOLA: 波形の似た位置を探して重ね合わせる時間伸縮)。
// 譜面制作の疑似再生で、速度を変えた音源を作るのに使う。DxLib には依存しない
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace tstretch {

    // in: インターリーブの float (channels チャンネル)。speed 倍の速さ (長さは 1/speed) にした音を返す。cancel が立ったら途中でやめて空を返す
    inline std::vector<float> Wsola(const std::vector<float>& in, int channels, int rate, double speed, const std::atomic_bool* cancel = nullptr) {
        if (channels <= 0 || rate <= 0 || speed <= 0) { return {}; }
        const size_t frames = in.size() / (size_t)channels;
        if (std::abs(speed - 1.0) < 1e-9) { return in; }
        int n = (int)(rate * 0.046);          // 1 枠 約 46ms
        n += n & 1;
        const int hs = n / 2;                  // 書き込みの間隔 (半分ずつ重ねる)
        const int tol = (int)(rate * 0.008);   // 似た位置を探す範囲 (前後 約 8ms)
        if (frames < (size_t)(n + 2 * tol + 1)) { return {}; }
        std::vector<float> window(n);
        for (int i = 0; i < n; ++i) { window[i] = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * i / n); }
        // 位置探しはモノラルで行う
        std::vector<float> mono(frames);
        for (size_t f = 0; f < frames; ++f) {
            float sum = 0;
            for (int c = 0; c < channels; ++c) { sum += in[f * channels + c]; }
            mono[f] = sum / channels;
        }
        const size_t outframes = (size_t)(frames / speed) + n;
        std::vector<float> out(outframes * channels, 0.0f), weight(outframes, 0.0f);
        long long prev = 0;     // 前の枠を取り出した入力の位置
        for (size_t k = 0;; ++k) {
            if (cancel && cancel->load()) { return {}; }
            const size_t outpos = k * (size_t)hs;
            const long long target = (long long)std::llround((double)k * hs * speed);
            if (target + n + tol >= (long long)frames || outpos + n > outframes) { break; }
            long long best = target;
            if (k > 0) {
                // 前の枠の続き (prev + hs) と最もよく似た位置を target の前後から探す (4 サンプルおきに比べて速くする)
                const long long natural = prev + hs;
                double bestscore = -1e300;
                for (int d = -tol; d <= tol; d += 2) {
                    const long long pos = target + d;
                    if (pos < 0 || pos + n >= (long long)frames || natural + hs >= (long long)frames) { continue; }
                    double score = 0;
                    for (int i = 0; i < hs; i += 4) { score += (double)mono[(size_t)(pos + i)] * mono[(size_t)(natural + i)]; }
                    if (score > bestscore) { bestscore = score; best = pos; }
                }
            }
            for (int i = 0; i < n; ++i) {
                const float w = window[i];
                const size_t src = (size_t)(best + i), dst = outpos + i;
                for (int c = 0; c < channels; ++c) { out[dst * channels + c] += in[src * channels + c] * w; }
                weight[dst] += w;
            }
            prev = best;
        }
        for (size_t f = 0; f < outframes; ++f) {
            if (weight[f] > 1e-3f) { for (int c = 0; c < channels; ++c) { out[f * channels + c] /= weight[f]; } }
        }
        // 最後の書き込み位置で切る
        size_t last = outframes;
        while (last > 0 && weight[last - 1] <= 1e-3f) { --last; }
        out.resize(last * channels);
        return out;
    }
}
