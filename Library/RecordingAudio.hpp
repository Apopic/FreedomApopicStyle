#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace rec {
    // 設定の百分率をそのまま振幅へ適用する。小さい設定を平方根で持ち上げない。
    inline float VolumeGain(double chartPercent, double configPercent) {
        if (!std::isfinite(chartPercent) || !std::isfinite(configPercent)) { return 0; }
        return (float)(std::clamp(chartPercent, 0.0, 100.0) * std::clamp(configPercent, 0.0, 100.0) / 10000.0);
    }
    inline void LimitMixPeak(std::vector<float>& mix, float ceiling = 0.98f) {
        float peak = 0;
        for (float& sample : mix) {
            if (!std::isfinite(sample)) { sample = 0; }
            peak = std::max(peak, std::abs(sample));
        }
        // 下げるだけにし、低音量の設定や無音を正規化で持ち上げない。
        if (peak > ceiling) { for (float& sample : mix) { sample *= ceiling / peak; } }
    }
}
