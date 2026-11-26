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
}
