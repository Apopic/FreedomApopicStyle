#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <cstdint>
#include <vector>

namespace audioalign {
struct GridFit { int64_t Division; std::vector<int64_t> Slots; };
// Minimize the serialized common grid, including stationary notes and commands.
// timeAt is in milliseconds: timing accuracy must not depend on BPM or MEASURE.
template<class TimeAt, class Allowed>
std::optional<GridFit> FitGrid(const std::vector<double>& positions, int64_t fixedDivision,
    TimeAt timeAt, Allowed allowed) {
    constexpr int64_t limit = 1024 * 1024, fine = 1920;
    if (fixedDivision < 1 || fixedDivision > limit) return {};
    std::vector<int64_t> grids;
    for (int64_t d = 1; d <= fine; ++d) {
        const auto factor = fixedDivision / std::gcd(fixedDivision, d);
        if (factor <= limit / d) grids.push_back(factor * d);
    }
    std::sort(grids.begin(), grids.end());
    grids.erase(std::unique(grids.begin(), grids.end()), grids.end());
    std::vector<double> targets, errors;
    for (double p : positions) {
        if (!std::isfinite(p) || p < 0 || p >= 1) return {};
        const double target = timeAt(p);
        const double legacy = timeAt(std::clamp(std::round(p * fine), 0.0, double(fine - 1)) / fine);
        if (!std::isfinite(target) || !std::isfinite(legacy)) return {};
        targets.push_back(target);
        errors.push_back(std::max(10.0, std::abs(legacy - target)));
    }
    for (int64_t division : grids) {
        GridFit fit{division, {}};
        bool valid = true;
        for (size_t i = 0; i < positions.size(); ++i) {
            const auto slot = std::clamp<int64_t>((int64_t)std::llround(positions[i] * double(division)), 0, division - 1);
            const double time = timeAt(double(slot) / double(division));
            if (!std::isfinite(time) || std::abs(time - targets[i]) > errors[i] + 1e-7 ||
                (!fit.Slots.empty() && slot <= fit.Slots.back()) || !allowed(i, slot, division)) { valid = false; break; }
            fit.Slots.push_back(slot);
        }
        if (valid) return fit;
    }
    return {};
}
struct Onset { double Time = 0, Strength = 0; }; // seconds, full-band attack strength
inline std::vector<Onset> Detect(const float* samples, size_t count, int rate) {
    if (!samples || rate < 1000 || count < (size_t)rate / 10) return {};
    const size_t frames = count * 1000 / rate;
    std::vector<double> rms(frames), novelty(frames);
    for (size_t i = 0; i < frames; ++i) {
        const size_t begin = i * (size_t)rate / 1000, end = std::min(count, (i + 1) * (size_t)rate / 1000);
        double energy = 0;
        for (size_t j = begin; j < end; ++j) { const double x = std::isfinite(samples[j]) ? samples[j] : 0; energy += x * x; }
        rms[i] = std::sqrt(energy / std::max<size_t>(1, end - begin));
    }
    double baseline = 0;
    for (size_t i = 0; i < frames; ++i) {
        novelty[i] = std::max(0.0, rms[i] - baseline);
        baseline += (rms[i] - baseline) * 0.12;
    }
    const double peak = *std::max_element(novelty.begin(), novelty.end());
    if (peak < 1e-6) return {};
    std::vector<Onset> attacks;
    for (size_t i = 1; i + 8 < frames; ++i) {
        if (novelty[i] < peak * 0.025 || novelty[i] < novelty[i - 1]) continue;
        bool maximum = true;
        for (size_t j = i + 1; j <= i + 8; ++j) if (novelty[j] > novelty[i]) { maximum = false; break; }
        if (!maximum) continue;
        const size_t begin = i > 25 ? i - 25 : 0;
        const auto low = std::min_element(rms.begin() + begin, rms.begin() + i + 1);
        size_t attack = (size_t)(low - rms.begin());
        const double threshold = *low + (rms[i] - *low) * 0.06;
        while (attack < i && rms[attack] <= threshold) ++attack;
        // Attack detection identifies a transient; align to its local waveform
        // apex rather than the preceding threshold crossing. Limit the search
        // to 30ms so the next drum hit cannot pull this event forward.
        size_t apex=std::min(count-1,attack*(size_t)rate/1000);
        double amplitude=0;
        const size_t end=std::min(count,(i+30)*(size_t)rate/1000);
        for(size_t j=apex;j<end;++j) {
            const double value=std::isfinite(samples[j])?std::abs(samples[j]):0;
            if(value>amplitude){amplitude=value;apex=j;}
        }
        const Onset onset{ (double)apex/rate, novelty[i] / peak };
        if (!attacks.empty() && onset.Time - attacks.back().Time < 0.025) {
            if (onset.Strength > attacks.back().Strength) attacks.back() = onset;
        } else attacks.push_back(onset);
        i += 8;
    }
    return attacks;
}
// Neighbour midpoints form disjoint search regions. No two notes consume one
// attack, and colours/order/count are preserved even with a wide tolerance.
inline std::vector<double> Snap(const std::vector<double>& times, const std::vector<bool>& movable,
    const std::vector<Onset>& onsets, double tolerance) {
    auto result = times;
    if (!std::isfinite(tolerance) || tolerance <= 0 || times.size() != movable.size() || !std::is_sorted(times.begin(), times.end())) return result;
    for (size_t i = 0; i < times.size(); ++i) {
        if (!movable[i] || !std::isfinite(times[i])) continue;
        const double left = std::max(times[i] - tolerance, i ? (times[i - 1] + times[i]) * 0.5 + 1e-6 : 0.0);
        const double right = std::min(times[i] + tolerance, i + 1 < times.size() ? (times[i] + times[i + 1]) * 0.5 - 1e-6 : std::numeric_limits<double>::max());
        auto first = std::lower_bound(onsets.begin(), onsets.end(), left, [](const Onset& a, double t) { return a.Time < t; });
        double localPeak = 0;
        for (auto it = first; it != onsets.end() && it->Time <= right; ++it) localPeak = std::max(localPeak, it->Strength);
        double best = std::numeric_limits<double>::max();
        for (auto it = first; it != onsets.end() && it->Time <= right; ++it) {
            if (it->Strength < std::max(1e-6, localPeak * 0.2)) continue;
            const double distance = std::abs(it->Time - times[i]) / tolerance;
            const double score = 0.45 * distance * distance + 0.55 * (1 - it->Strength / localPeak);
            if (score < best) { best = score; result[i] = it->Time; }
        }
    }
    return result;
}
}
