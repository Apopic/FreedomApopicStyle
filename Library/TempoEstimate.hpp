#pragma once
// 音源から BPM・最初の小節の頭 (譜面の開始オフセット)・拍子を推定する (譜面制作モードの自動推定)。
//
// 手順:
//   1. 10ms ごとに短時間フーリエ変換し、スペクトルの増え方 (spectral flux) から発音の強さの列を作る (全帯域と低音域)
//   2. 発音の強さの自己相関から拍の周期を求め (50～240 BPM。120 BPM 付近を少し優先)、
//      曲全体に拍の格子を当てはめて BPM と拍の位置を細かく合わせる
//   3. 低音域の強さが周期的に強くなる拍から、拍子 (3 拍子 / 4 拍子) を決める
//   4. 最初の音が鳴った位置に一番近い拍を「最初の小節の頭」にする
// 推定なので、結果は目安 (曲の作りによっては半分・倍の BPM になることがある)。

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numeric>
#include <vector>

namespace tempo {

    struct Result {
        bool Ok = false;
        double Bpm = 0.0;
        double FirstDownbeat = 0.0;     // 最初の小節の頭 (秒)。TJA の OFFSET は -FirstDownbeat
        int BeatsPerMeasure = 4;        // 3 または 4
        double Confidence = 0.0;        // 0～1 (拍の周期のはっきりさ)
    };

    namespace detail {
        inline void Fft(std::vector<std::complex<float>>& a) {
            const size_t n = a.size();
            for (size_t i = 1, j = 0; i < n; ++i) {
                size_t bit = n >> 1;
                for (; j & bit; bit >>= 1) { j ^= bit; }
                j ^= bit;
                if (i < j) { std::swap(a[i], a[j]); }
            }
            for (size_t len = 2; len <= n; len <<= 1) {
                const float ang = -2.0f * 3.14159265358979f / (float)len;
                const std::complex<float> wl(std::cos(ang), std::sin(ang));
                for (size_t i = 0; i < n; i += len) {
                    std::complex<float> w(1.0f, 0.0f);
                    for (size_t k = 0; k < len / 2; ++k) {
                        const auto u = a[i + k], v = a[i + k + len / 2] * w;
                        a[i + k] = u + v;
                        a[i + k + len / 2] = u - v;
                        w *= wl;
                    }
                }
            }
        }
        // 0.5 秒の移動平均を引いて負を 0 にし、最大値で割る
        inline void Normalize(std::vector<float>& env, int fps) {
            const int half = std::max(1, fps / 4);
            std::vector<double> prefix(env.size() + 1, 0.0);
            for (size_t i = 0; i < env.size(); ++i) { prefix[i + 1] = prefix[i] + env[i]; }
            std::vector<float> out(env.size());
            for (size_t i = 0; i < env.size(); ++i) {
                const size_t a = i >= (size_t)half ? i - half : 0, b = std::min(env.size(), i + half + 1);
                const double mean = (prefix[b] - prefix[a]) / (double)(b - a);
                out[i] = (float)std::max(0.0, env[i] - mean);
            }
            const float mx = *std::max_element(out.begin(), out.end());
            if (mx > 0) { for (auto& v : out) { v /= mx; } }
            env = std::move(out);
        }
        // 時刻 t (秒) の値 (線形補間)
        inline float At(const std::vector<float>& env, double t, double fps) {
            const double x = t * fps;
            if (x < 0 || x >= (double)env.size() - 1) { return 0.0f; }
            const size_t i = (size_t)x;
            const float f = (float)(x - (double)i);
            return env[i] * (1.0f - f) + env[i + 1] * f;
        }
        // 時刻 t の前後 radius 秒の最大値
        inline float PeakAround(const std::vector<float>& env, double t, double fps, double radius) {
            const long a = (long)std::floor((t - radius) * fps), b = (long)std::ceil((t + radius) * fps);
            float mx = 0.0f;
            for (long i = std::max(0L, a); i <= b && i < (long)env.size(); ++i) { mx = std::max(mx, env[(size_t)i]); }
            return mx;
        }
    }

    // mono: モノラルの音 (-1～1)、rate: サンプリングレート
    inline Result Estimate(const float* mono, size_t count, int rate) {
        using namespace detail;
        Result result;
        if (mono == nullptr || rate <= 0 || count < (size_t)rate * 4) { return result; }    // 4 秒未満は推定しない

        // ---- 1. 発音の強さ (10ms ごと) ----
        const size_t hop = (size_t)std::max(1, rate / 100);
        const double fps = (double)rate / (double)hop;
        size_t window = 1;
        while (window < (size_t)rate / 40) { window <<= 1; }          // 25ms 以上の 2 のべき乗 (44.1kHz なら 2048)
        const size_t bins = window / 2;
        const size_t lowbins = std::max<size_t>(2, (size_t)(200.0 * window / rate));   // 200Hz まで = 低音
        std::vector<float> hann(window);
        for (size_t i = 0; i < window; ++i) { hann[i] = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * (float)i / (float)(window - 1)); }
        const size_t frames = (count - window) / hop + 1;
        std::vector<float> flux(frames, 0.0f), lowflux(frames, 0.0f), prev(bins, 0.0f);
        std::vector<std::complex<float>> buf(window);
        for (size_t f = 0; f < frames; ++f) {
            const float* src = mono + f * hop;
            for (size_t i = 0; i < window; ++i) { buf[i] = { src[i] * hann[i], 0.0f }; }
            Fft(buf);
            float sum = 0.0f, low = 0.0f;
            for (size_t k = 1; k < bins; ++k) {
                const float mag = std::log1p(100.0f * std::abs(buf[k]));
                const float d = mag - prev[k];
                if (d > 0) { sum += d; if (k < lowbins) { low += d; } }
                prev[k] = mag;
            }
            flux[f] = sum;
            lowflux[f] = low;
        }
        if (frames < 8) { return result; }
        flux[0] = lowflux[0] = 0.0f;
        Normalize(flux, (int)fps);
        Normalize(lowflux, (int)fps);
        // 拍は低音 (キック・ベース) を重く見る (ハイハットの裏拍に引っぱられないように)。以降の拍の推定はこの列を使う
        std::vector<float> beatenv(frames);
        for (size_t i = 0; i < frames; ++i) { beatenv[i] = 0.4f * flux[i] + lowflux[i]; }
        Normalize(beatenv, (int)fps);
        // フレームの値は窓の中央の時刻のもの
        const double center = (double)window * 0.5 / (double)rate;

        // ---- 2. 拍の周期 (自己相関) ----
        const size_t minlag = (size_t)std::floor(fps * 60.0 / 240.0), maxlag = (size_t)std::ceil(fps * 60.0 / 50.0);
        if (maxlag * 2 >= frames) { return result; }
        std::vector<double> ac(maxlag * 2 + 2, 0.0);
        for (size_t lag = minlag / 2; lag < ac.size() && lag < frames; ++lag) {
            double s = 0.0;
            for (size_t i = lag; i < frames; ++i) { s += (double)beatenv[i] * beatenv[i - lag]; }
            ac[lag] = s / (double)(frames - lag);
        }
        double energy = 0.0;
        for (size_t i = 0; i < frames; ++i) { energy += (double)beatenv[i] * beatenv[i]; }
        energy /= (double)frames;
        size_t best = minlag;
        double bestscore = -1.0;
        for (size_t lag = minlag; lag <= maxlag; ++lag) {
            const double bpm = 60.0 * fps / (double)lag;
            const double prior = std::exp(-0.5 * std::pow(std::log2(bpm / 140.0) / 1.2, 2.0));    // 140 BPM 付近を少し優先 (太鼓の曲に多い速さ)
            const double s = (ac[lag] + 0.5 * (2 * lag < ac.size() ? ac[2 * lag] : 0.0)) * prior;
            if (s > bestscore) { bestscore = s; best = lag; }
        }
        // 放物線で周期を細かくする
        double period = (double)best;
        if (best > minlag && best < maxlag) {
            const double a = ac[best - 1], b = ac[best], c = ac[best + 1];
            const double den = a - 2 * b + c;
            if (std::abs(den) > 1e-12) { period += 0.5 * (a - c) / den; }
        }
        double bpm = 60.0 * fps / period;
        result.Confidence = energy > 0 ? std::clamp(ac[best] / energy, 0.0, 1.0) : 0.0;

        // 曲全体に拍の格子を当てはめて BPM と位置を合わせる (±1.5%)
        const double duration = (double)frames / fps;
        auto fit = [&](double b, double& phase_out) {
            const double p = 60.0 / b;
            double bestsum = -1.0;
            const int steps = 48;
            for (int s = 0; s < steps; ++s) {
                const double phase = p * s / steps;
                double sum = 0.0;
                for (double t = phase; t < duration; t += p) { sum += At(beatenv, t, fps); }
                if (sum > bestsum) { bestsum = sum; phase_out = phase; }
            }
            return bestsum / (duration / p);
        };
        double phase = 0.0, bestfit = -1.0, bestbpm = bpm;
        for (double b = bpm * 0.985; b <= bpm * 1.015; b += bpm * 0.0005) {
            double ph = 0.0;
            const double score = fit(b, ph);
            if (score > bestfit) { bestfit = score; bestbpm = b; phase = ph; }
        }
        bpm = bestbpm;
        // 整数に近い BPM (譜面の BPM はたいてい整数) は整数にする
        if (std::abs(bpm - std::round(bpm)) < 0.12) {
            bpm = std::round(bpm);
            fit(bpm, phase);
        }
        // 拍の位置を 1ms 単位で詰める
        {
            const double p = 60.0 / bpm;
            double bestsum = -1.0, bestph = phase;
            for (double ph = phase - 0.02; ph <= phase + 0.02; ph += 0.001) {
                double sum = 0.0;
                for (double t = ph; t < duration; t += p) { sum += At(beatenv, t, fps); }
                if (sum > bestsum) { bestsum = sum; bestph = ph; }
            }
            phase = std::fmod(bestph + p * 4, p);
        }

        // ---- 3. 拍子と小節の頭 (低音が強い拍) ----
        const double p = 60.0 / bpm;
        std::vector<float> strength;
        for (double t = phase; t < duration; t += p) { strength.push_back(PeakAround(lowflux, t, fps, 0.03) + 0.3f * PeakAround(flux, t, fps, 0.03)); }
        auto accent = [&](int meter, int& offset) {
            double total = std::accumulate(strength.begin(), strength.end(), 0.0) / std::max<size_t>(strength.size(), 1);
            double bestratio = 0.0;
            for (int k = 0; k < meter; ++k) {
                double s = 0.0;
                int n = 0;
                for (size_t i = k; i < strength.size(); i += meter) { s += strength[i]; ++n; }
                const double ratio = n > 0 && total > 0 ? (s / n) / total : 0.0;
                if (ratio > bestratio) { bestratio = ratio; offset = k; }
            }
            return bestratio;
        };
        int off3 = 0, off4 = 0;
        const double r3 = accent(3, off3), r4 = accent(4, off4);
        const int meter = r3 > r4 * 1.08 ? 3 : 4;
        const int downoffset = meter == 3 ? off3 : off4;

        // ---- 4. 最初の小節の頭: 最初に音が鳴る位置か、その直前の小節の頭 ----
        double first = 0.0;
        for (size_t i = 0; i < frames; ++i) {
            if (beatenv[i] > 0.25f) { first = (double)i / fps; break; }
        }
        // 曲はたいてい小節の頭から始まるので、最初に音が鳴った位置に一番近い拍を最初の小節の頭にする
        // (どの拍が小節の頭かの推定は外れやすいので、拍子の判定だけに使う)
        (void)downoffset;
        double head = phase + std::round((first - phase) / p) * p;
        while (head < 0) { head += p; }
        head += center;     // フレームの時刻を窓の中央に直す
        result.Ok = true;
        result.Bpm = bpm;
        result.FirstDownbeat = head;
        result.BeatsPerMeasure = meter;
        return result;
    }
}
