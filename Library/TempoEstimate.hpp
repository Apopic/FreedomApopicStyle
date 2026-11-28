#pragma once
// 音源から BPM・最初の小節の頭 (譜面の開始オフセット)・拍子を推定する (譜面制作モードの自動推定)。
//
// 手順:
//   1. 10ms ごとに短時間フーリエ変換し、スペクトルの増え方 (spectral flux) から発音の強さの列を作る (全帯域と低音域)
//   2. 発音の強さの自己相関から拍の周期を求め (50～240 BPM。120 BPM 付近を少し優先)、
//      曲全体に拍の格子を当てはめて BPM と拍の位置を細かく合わせる
//   3. 低音域の強さが周期的に強くなる拍から、拍子 (3 拍子 / 4 拍子) を決める
//   4. 1ms のエネルギー変化で打音の立ち上がりを検出し、半分・倍などの候補を比較する
//   5. 打音の時刻に回帰直線を当て、細かい BPM と最初の打音の位置を求め直す
// 推定なので、結果は目安 (曲の作りによっては半分・倍の BPM になることがある)。

#include <algorithm>
#include <cmath>
#include <complex>
#include <functional>
#include <cstddef>
#include <numeric>
#include <vector>
#include <utility>

namespace tempo {

    struct Result {
        bool Ok = false;
        double Bpm = 0.0;
        double FirstDownbeat = 0.0;     // 最初の小節の頭 (秒)。TJA の OFFSET は -FirstDownbeat
        int BeatsPerMeasure = 4;        // 3 または 4
        double Confidence = 0.0;        // 0～1 (拍の周期のはっきりさ)
        double DurationSeconds = 0.0;
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
    inline Result EstimateCoarse(const float* mono, size_t count, int rate) {
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

    // 逆相のステレオを平均して打音を消してしまわないよう、必要なら最も強いチャンネルを使う。
    template<class Sample>
    inline std::vector<float> PrepareMono(size_t frames, int channels, Sample sample) {
        if (!frames || channels <= 0 || channels > 32) { return {}; }
        std::vector<float> mono(frames);
        std::vector<double> energy(channels, 0);
        double mixed = 0;
        for (size_t i = 0; i < frames; ++i) {
            double sum = 0;
            for (int c = 0; c < channels; ++c) { const double v = sample(i, c); if (std::isfinite(v)) { sum += v; energy[c] += v * v; } }
            mono[i] = (float)(sum / channels); mixed += mono[i] * mono[i];
        }
        const auto best = std::max_element(energy.begin(), energy.end());
        if (mixed < *best * 0.1) {
            const int channel = (int)(best - energy.begin());
            for (size_t i = 0; i < frames; ++i) { const float value = sample(i, channel); mono[i] = std::isfinite(value) ? value : 0; }
        }
        return mono;
    }
    inline int MeasureCount(double seconds, double bpm, double offsetSeconds, double beats = 4) {
        if (!std::isfinite(seconds) || !std::isfinite(bpm) || !std::isfinite(offsetSeconds) || !std::isfinite(beats) || seconds <= 0 || bpm <= 0 || beats <= 0) { return 0; }
        const double measures = std::ceil(std::max(0.0, seconds + offsetSeconds) * bpm / (60 * beats) - 1e-9);
        return (int)std::clamp(measures, 1.0, 100000.0);
    }
    inline Result Estimate(const float* input, size_t count, int rate) {
        Result result;
        if (!input || rate < 1000 || rate > 768000 || count < (size_t)rate * 4) { return result; }
        const double duration = (double)count / rate;
        result.DurationSeconds = duration;
        // 解析用に帯域を平均して縮小する。元のサンプルレートに依存する FFT の位相差も抑える。
        std::vector<float> down;
        if (rate > 16000) {
            const double step = rate / 16000.0;
            down.resize((size_t)(count / step));
            for (size_t i = 0; i < down.size(); ++i) {
                const size_t a = (size_t)(i * step), b = std::min(count, (size_t)((i + 1) * step));
                double sum = 0;
                for (size_t j = a; j < b; ++j) { if (std::isfinite(input[j])) { sum += input[j]; } }
                down[i] = (float)(sum / std::max<size_t>(1, b - a));
            }
            input = down.data(); count = down.size(); rate = 16000;
        }
        const auto coarse = EstimateCoarse(input, count, rate);
        if (!coarse.Ok || !std::isfinite(coarse.Bpm) || coarse.Bpm <= 0) { return result; }
        // FFT 窓の中央を発音位置とみなす代わりに、1ms のエネルギーから実際のアタックを探す。
        const size_t frames = (size_t)(1000.0 * count / rate);
        std::vector<double> rms(frames), bass(frames), prefix(frames + 1, 0), novelty(frames, 0);
        double lowpass = 0;
        const double alpha = 1 - std::exp(-2 * 3.14159265358979 * 200 / rate);
        for (size_t i = 0; i < frames; ++i) {
            const size_t a = i * (size_t)rate / 1000, b = std::min(count, (i + 1) * (size_t)rate / 1000);
            double energy = 0, low = 0;
            for (size_t j = a; j < b; ++j) {
                const double value = std::isfinite(input[j]) ? input[j] : 0;
                energy += value * value; lowpass += alpha * (value - lowpass); low += lowpass * lowpass;
            }
            bass[i] = std::sqrt(low / std::max<size_t>(1, b - a));
            rms[i] = std::sqrt(energy / std::max<size_t>(1, b - a)); prefix[i + 1] = prefix[i] + rms[i];
        }
        std::vector<double> smooth(frames), smoothPrefix(frames + 1, 0);
        for (size_t i = 0; i < frames; ++i) {
            const size_t a = i > 4 ? i - 4 : 0, b = std::min(frames, i + 5);
            smooth[i] = (prefix[b] - prefix[a]) / (b - a); smoothPrefix[i + 1] = smoothPrefix[i] + smooth[i];
            const size_t start = i > 30 ? i - 30 : 0;
            novelty[i] = std::max(0.0, smooth[i] - (smoothPrefix[i] - smoothPrefix[start]) / std::max<size_t>(1, i - start));
        }
        auto levels = novelty;
        const size_t percentile = levels.size() * 95 / 100;
        std::nth_element(levels.begin(), levels.begin() + percentile, levels.end());
        const double maximum = *std::max_element(novelty.begin(), novelty.end());
        if (maximum < 1e-7) { return result; }
        const double scale = std::max(levels[percentile], maximum * 0.08);
        struct Onset { double Time, Weight, LowRatio; };
        std::vector<Onset> onsets;
        for (size_t i = 1; i + 8 < frames; ++i) {
            if (novelty[i] < std::max(scale * 0.25, prefix.back() / frames * 0.04) || novelty[i] < novelty[i - 1]) { continue; }
            bool peak = true;
            for (size_t k = i + 1; k <= i + 8; ++k) { if (novelty[k] > novelty[i]) { peak = false; break; } }
            if (!peak) { continue; }
            const size_t start = i > 30 ? i - 30 : 0;
            const auto minimum = std::min_element(rms.begin() + start, rms.begin() + i + 1);
            size_t attack = (size_t)(minimum - rms.begin());
            const double threshold = *minimum + (rms[i] - *minimum) * 0.06;
            while (attack < i && rms[attack] <= threshold) { ++attack; }
            const double time = attack / 1000.0;
            double fullEnergy = 0, lowEnergy = 0;
            for (size_t j = attack; j < std::min(frames, i + 20); ++j) { fullEnergy += rms[j] * rms[j]; lowEnergy += bass[j] * bass[j]; }
            const double lowRatio = fullEnergy > 0 ? std::clamp(std::sqrt(lowEnergy / fullEnergy), 0.0, 1.0) : 0;
            const double weight = std::log1p(std::min(100.0, novelty[i] / scale)) * (0.1 + 0.9 * lowRatio);
            if (!onsets.empty() && time - onsets.back().Time < 0.04) {
                if (weight > onsets.back().Weight) { onsets.back() = { time, weight, lowRatio }; }
            } else { onsets.push_back({ time, weight, lowRatio }); }
            i += 8;
        }
        if (onsets.size() < 4) { return result; }
        auto score = [&](double period, double phase) {
            double sum = 0;
            for (const auto& onset : onsets) {
                const double error = std::remainder(onset.Time - phase, period);
                sum += onset.Weight * std::exp(-0.5 * std::pow(error / 0.012, 2));
            }
            return sum / std::sqrt(std::max(1.0, duration / period));
        };
        double best = -1, period = 60 / coarse.Bpm, phase = coarse.FirstDownbeat;
        auto strongest = onsets;
        std::stable_sort(strongest.begin(), strongest.end(), [](const auto& a, const auto& b) { return a.Weight > b.Weight; });
        std::vector<double> anchors{ coarse.FirstDownbeat };
        for (size_t i = 0; i < std::min<size_t>(4, strongest.size()); ++i) { anchors.push_back(strongest[i].Time); }
        for (double octave : { 0.5, 2.0 / 3.0, 1.0, 1.5, 2.0, 3.0 }) {
            const double seed = coarse.Bpm * octave;
            if (seed < 40 || seed > 360) { continue; }
            for (int delta = -60; delta <= 60; ++delta) {
                const double p = 60 / (seed * (1 + delta * 0.0005));
                for (double anchor : anchors) {
                for (int shift = -3; shift <= 3; ++shift) {
                    const double ph = anchor + shift * 0.002;
                    const double value = score(p, ph);
                    if (value > best) { best = value; period = p; phase = ph; }
                }
                }
            }
        }
        // 拍番号と発音時刻の回帰で、曲が進むほど増える BPM の丸め誤差を取り除く。
        for (int iteration = 0; iteration < 3; ++iteration) {
            double sw = 0, sk = 0, st = 0, skk = 0, skt = 0;
            const double radius = iteration == 0 ? 0.04 : 0.02;
            for (const auto& onset : onsets) {
                const double k = std::round((onset.Time - phase) / period), error = onset.Time - phase - k * period;
                if (std::abs(error) > radius) { continue; }
                const double w = onset.Weight * std::exp(-0.5 * std::pow(error / radius, 2));
                sw += w; sk += w * k; st += w * onset.Time; skk += w * k * k; skt += w * k * onset.Time;
            }
            const double denominator = sw * skk - sk * sk;
            if (sw <= 0 || denominator <= 0) { break; }
            const double fitted = (sw * skt - sk * st) / denominator;
            if (fitted > 60.0 / 400 && fitted < 60.0 / 35) { period = fitted; phase = (st - period * sk) / sw; }
        }
        double weight = 0, matched = 0, first = duration;
        for (const auto& onset : onsets) {
            weight += onset.Weight;
            if (std::abs(std::remainder(onset.Time - phase, period)) < 0.02) { matched += onset.Weight; first = std::min(first, onset.Time); }
        }
        if (first == duration || matched < weight * 0.15) { return result; }
        result.Ok = true; result.Bpm = 60 / period;
        auto accent = [&](int meter) {
            std::vector<double> sums(meter, 0), counts(meter, 0);
            const int firstBeat = (int)std::floor((0 - phase) / period), lastBeat = (int)std::ceil((duration - phase) / period);
            for (int k = firstBeat; k <= lastBeat; ++k) { ++counts[(k % meter + meter) % meter]; }
            for (const auto& onset : onsets) {
                const int k = (int)std::round((onset.Time - phase) / period);
                if (std::abs(onset.Time - phase - k * period) < 0.02) { sums[(k % meter + meter) % meter] += onset.Weight; }
            }
            const double average = matched / std::max(1, lastBeat - firstBeat + 1);
            double best = 0; int residue = 0;
            for (int k = 0; k < meter; ++k) {
                const double score = sums[k] / std::max(1.0, counts[k]) / std::max(1e-9, average);
                if (score > best) { best = score; residue = k; }
            }
            return std::pair<double, int>{ best, residue };
        };
        const auto triple = accent(3), quadruple = accent(4);
        result.BeatsPerMeasure = triple.first > quadruple.first * 1.08 ? 3 : 4;
        const auto chosen = result.BeatsPerMeasure == 3 ? triple : quadruple;
        std::vector<double> beatWeights;
        for (const auto& onset : onsets) {
            if (onset.LowRatio >= 0.35 && std::abs(std::remainder(onset.Time - phase, period)) < 0.02) beatWeights.push_back(onset.Weight);
        }
        const bool hasBass = beatWeights.size() >= 4;
        if (!hasBass) {
            beatWeights.clear();
            for (const auto& onset : onsets) if (std::abs(std::remainder(onset.Time - phase, period)) < 0.02) beatWeights.push_back(onset.Weight);
        }
        std::sort(beatWeights.begin(), beatWeights.end());
        const double threshold = beatWeights.empty() ? 0 : beatWeights[beatWeights.size() / 2] * 0.5;
        auto salient = [&](const Onset& onset) {
            return onset.Weight >= threshold && (!hasBass || onset.LowRatio >= 0.35) && std::abs(std::remainder(onset.Time - phase, period)) < 0.02;
        };
        // 単発の弱起メロディではなく、アクセントの位置から安定して拍が続く小節頭を選ぶ。
        double downbeat = duration;
        for (const auto& onset : onsets) {
            if (!salient(onset)) continue;
            const int k = (int)std::round((onset.Time - phase) / period);
            if (chosen.first >= 1.08 && (k % result.BeatsPerMeasure + result.BeatsPerMeasure) % result.BeatsPerMeasure != chosen.second) continue;
            int following = 0, previousBeat = k - 1;
            for (const auto& next : onsets) {
                if (next.Time < onset.Time - 0.001 || next.Time > onset.Time + period * (result.BeatsPerMeasure - 0.5)) continue;
                const int beat = (int)std::round((next.Time - phase) / period);
                if (salient(next) && beat != previousBeat) { ++following; previousBeat = beat; }
            }
            if (following >= result.BeatsPerMeasure - 1) { downbeat = onset.Time; break; }
        }
        if (downbeat == duration) {
            for (const auto& onset : onsets) if (salient(onset)) { downbeat = onset.Time; break; }
        }
        if (downbeat == duration) downbeat = first;
        result.FirstDownbeat = std::max(0.0, phase + std::round((downbeat - phase) / period) * period);
        result.Confidence = std::clamp(matched / weight, 0.0, 1.0);
        return result;
    }

    // ---- 範囲の BPM の変化を小節ごとに追う (譜面制作モードの「範囲の BPM を解析」) ----
    struct TrackOnset { double Time, Weight; };

    // from～to 秒の発音の時刻と強さ。スペクトルの増え方 (打楽器以外の音の始まりも拾う) で発音を見つけ、
    // 時刻は 1ms ごとのエネルギーの立ち上がりで細かく決める
    inline std::vector<TrackOnset> DetectOnsets(const float* input, size_t count, int rate, double from, double to) {
        std::vector<TrackOnset> onsets;
        if (!input || rate < 4000 || count == 0) { return onsets; }
        const double duration = (double)count / rate;
        from = std::clamp(from, 0.0, duration); to = std::clamp(to, 0.0, duration);
        if (to - from < 0.5) { return onsets; }
        // 16kHz に縮める (区間だけ)
        const int sr = std::min(rate, 16000);
        const double step = (double)rate / sr;
        const size_t base = (size_t)(from * rate), length = (size_t)((to - from) * sr);
        std::vector<float> x(length);
        for (size_t i = 0; i < length; ++i) {
            const size_t a = std::min(count - 1, base + (size_t)(i * step));
            const size_t b = std::min(count, std::max(a + 1, base + (size_t)((i + 1) * step)));
            double sum = 0;
            for (size_t j = a; j < b; ++j) { sum += std::isfinite(input[j]) ? input[j] : 0; }
            x[i] = (float)(sum / (double)(b - a));
        }
        // 1ms ごとのエネルギー
        const size_t ms = (size_t)std::max(1, sr / 1000), frames = length / ms;
        if (frames < 2) { return onsets; }
        std::vector<double> rms(frames);
        for (size_t i = 0; i < frames; ++i) {
            double e = 0;
            for (size_t j = i * ms; j < (i + 1) * ms; ++j) { e += (double)x[j] * x[j]; }
            rms[i] = std::sqrt(e / (double)ms);
        }
        // スペクトルの増え方 (窓 512・4ms ごと、対数の振幅の増えた分の合計)
        constexpr size_t N = 512;
        const size_t hop = (size_t)std::max(1, sr / 250);
        if (length < N + hop) { return onsets; }
        const size_t hops = (length - N) / hop + 1;
        std::vector<float> window(N), prev(N / 2 + 1, 0.0f), flux(hops, 0.0f);
        for (size_t i = 0; i < N; ++i) { window[i] = 0.5f - 0.5f * std::cos(2 * 3.14159265f * (float)i / (N - 1)); }
        std::vector<std::complex<float>> buf(N);
        for (size_t f = 0; f < hops; ++f) {
            for (size_t i = 0; i < N; ++i) { buf[i] = { x[f * hop + i] * window[i], 0.0f }; }
            detail::Fft(buf);
            float sum = 0;
            for (size_t k = 1; k <= N / 2; ++k) {
                const float mag = std::log1p(100.0f * std::abs(buf[k]));
                sum += std::max(0.0f, mag - prev[k]);
                prev[k] = mag;
            }
            flux[f] = f == 0 ? 0.0f : sum;
        }
        // 0.1 秒の移動平均を引いて、はっきり増えた所だけを残す
        std::vector<double> novelty(hops, 0), prefix(hops + 1, 0);
        for (size_t f = 0; f < hops; ++f) { prefix[f + 1] = prefix[f] + flux[f]; }
        for (size_t f = 0; f < hops; ++f) {
            const size_t a = f > 12 ? f - 12 : 0, b = std::min(hops, f + 13);
            novelty[f] = std::max(0.0, flux[f] - (prefix[b] - prefix[a]) / (double)(b - a));
        }
        auto levels = novelty;
        const size_t p90 = levels.size() * 90 / 100;
        std::nth_element(levels.begin(), levels.begin() + p90, levels.end());
        const double maximum = *std::max_element(novelty.begin(), novelty.end());
        if (maximum <= 0) { return onsets; }
        const double threshold = std::max(levels[p90] * 0.3, maximum * 0.03);
        for (size_t f = 1; f + 1 < hops; ++f) {
            if (novelty[f] < threshold) { continue; }
            bool peak = true;
            for (size_t k = f > 5 ? f - 5 : 0; k <= std::min(hops - 1, f + 5); ++k) {
                if (novelty[k] > novelty[f] || (novelty[k] == novelty[f] && k < f)) { peak = false; break; }
            }
            if (!peak) { continue; }
            // 窓の中のエネルギーが谷から上がり始めた所を発音の時刻にする
            const double center = (double)(f * hop + N / 2) / sr;
            const size_t lo = (size_t)std::clamp((center - 0.03) * 1000, 0.0, (double)frames - 1);
            const size_t hi = (size_t)std::clamp((center + 0.012) * 1000, 0.0, (double)frames - 1);
            if (hi <= lo) { continue; }
            const size_t top = (size_t)(std::max_element(rms.begin() + (std::ptrdiff_t)lo, rms.begin() + (std::ptrdiff_t)hi + 1) - rms.begin());
            const size_t bottom = (size_t)(std::min_element(rms.begin() + (std::ptrdiff_t)lo, rms.begin() + (std::ptrdiff_t)top + 1) - rms.begin());
            size_t attack = bottom;
            const double rise = rms[bottom] + (rms[top] - rms[bottom]) * 0.1;
            while (attack < top && rms[attack] <= rise) { ++attack; }
            const double time = from + (rms[top] > rms[bottom] * 1.05 ? (double)attack / 1000.0 : center);
            const double weight = std::log1p(std::min(50.0, novelty[f] / threshold));
            if (!onsets.empty() && std::abs(time - onsets.back().Time) < 0.03) { if (weight > onsets.back().Weight) { onsets.back() = { time, weight }; } }
            else { onsets.push_back({ time, weight }); }
        }
        std::ranges::sort(onsets, {}, &TrackOnset::Time);
        return onsets;
    }

    // start 秒から始まる小節 (それぞれ beats[m] 拍) の頭の時刻を、音源の打音から 1 小節ずつ求める (戻り値は beats.size() + 1 個)。
    // 拍の周期は前の小節の 0.6～1.4 倍で探し、表拍と裏拍 (弱め)、次の小節の始めの 2 拍 (弱め) まで使って合わせる。
    // 打音が少ない小節は前の周期のまま進める
    inline std::vector<double> TrackMeasureHeads(const float* input, size_t count, int rate, double start, const std::vector<double>& beats, double initialBpm) {
        std::vector<double> heads;
        if (!input || rate < 4000 || beats.empty() || !std::isfinite(initialBpm) || initialBpm <= 0 || !std::isfinite(start)) { return heads; }
        double total = 0;
        for (double b : beats) { total += std::max(0.0, b); }
        const auto onsets = DetectOnsets(input, count, rate, start - 0.5, start + total * 60 / initialBpm * 1.6 + 3);
        std::vector<double> times;
        for (const auto& o : onsets) { times.push_back(o.Time); }
        auto bestAt = [&](double t, double radius, double sigma) {
            double value = 0;
            for (auto it = std::lower_bound(times.begin(), times.end(), t - radius); it != times.end() && *it <= t + radius; ++it) {
                const double error = *it - t;
                value = std::max(value, onsets[(size_t)(it - times.begin())].Weight * std::exp(-0.5 * error * error / (sigma * sigma)));
            }
            return value;
        };
        double t = start, period = 60 / initialBpm;
        heads.push_back(t);
        for (size_t m = 0; m < beats.size(); ++m) {
            const double b = beats[m];
            if (!(b > 0)) { heads.push_back(t); continue; }
            const int n = std::max(1, (int)std::floor(b + 1e-9));
            const int ahead = m + 1 < beats.size() ? std::min(2, (int)std::floor(beats[m + 1] + 1e-9)) : 0;
            // 格子の点: (拍の番号, 重み)
            std::vector<std::pair<double, double>> grid;
            for (int j = 0; j <= n; ++j) { grid.push_back({ (double)j, 1.0 }); }
            for (int j = 0; j < n; ++j) { grid.push_back({ j + 0.5, 0.35 }); }
            for (int j = 1; j <= ahead; ++j) { grid.push_back({ (double)(n + j), 0.5 }); grid.push_back({ n + j - 0.5, 0.2 }); }
            double weightSum = 0;
            for (const auto& g : grid) { weightSum += g.second; }
            double bestScore = 0, bestPeriod = period, bestShift = 0;
            for (int k = -200; k <= 200; ++k) {
                const double p = period * (1 + k * 0.002);
                for (int s = -4; s <= 4; ++s) {
                    const double shift = s * 0.002;
                    double score = 0;
                    for (const auto& [j, w] : grid) { score += w * bestAt(t + shift + j * p, 0.04, 0.012); }
                    score = score / weightSum * std::exp(-0.5 * std::pow((p / period - 1) / 0.3, 2)) * std::exp(-0.5 * std::pow(shift / 0.008, 2));
                    if (score > bestScore) { bestScore = score; bestPeriod = p; bestShift = shift; }
                }
            }
            if (bestScore < 0.1) { t += b * period; heads.push_back(t); continue; }
            // 合った打音の時刻と拍の番号に直線を当てはめて、周期と位置を細かく求める (この小節の表拍・裏拍と次の小節の頭)
            double sw = 0, sk = 0, st = 0, skk = 0, skt = 0;
            for (const auto& [j, w] : grid) {
                if (j > n + 0.01) { continue; }
                const double at = t + bestShift + j * bestPeriod;
                for (auto it = std::lower_bound(times.begin(), times.end(), at - 0.025); it != times.end() && *it <= at + 0.025; ++it) {
                    const double error = *it - at;
                    const double v = w * onsets[(size_t)(it - times.begin())].Weight * std::exp(-0.5 * error * error / (0.012 * 0.012));
                    sw += v; sk += v * j; st += v * *it; skk += v * j * j; skt += v * j * *it;
                }
            }
            double fitted = bestPeriod, origin = t + bestShift;
            const double denominator = sw * skk - sk * sk;
            if (sw > 0 && denominator > 1e-9) {
                const double p = (sw * skt - sk * st) / denominator, o = (st - p * sk) / sw;
                if (p > bestPeriod * 0.95 && p < bestPeriod * 1.05 && std::abs(o - t) < 0.02) { fitted = p; origin = o; }
            }
            period = fitted;
            t = origin + b * period;
            heads.push_back(t);
        }
        return heads;
    }

    // 小節ごとの BPM (TrackMeasureHeads の頭の間隔から)
    inline std::vector<double> TrackMeasureBpms(const float* input, size_t count, int rate, double start, const std::vector<double>& beats, double initialBpm) {
        const auto heads = TrackMeasureHeads(input, count, rate, start, beats, initialBpm);
        std::vector<double> out;
        for (size_t m = 0; m + 1 < heads.size(); ++m) { out.push_back(heads[m + 1] > heads[m] ? beats[m] * 60 / (heads[m + 1] - heads[m]) : initialBpm); }
        return out;
    }
}
