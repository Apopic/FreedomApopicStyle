#pragma once
#include "../Library/TempoEstimate.hpp"

static void TempoTests() {
    for (int meter : { 3, 4 }) {
        const int rate = 16000;
        const double first = 2.137, bpm = 137.5, period = 60 / bpm;
        std::vector<float> samples(rate * 24, 0);
        auto add = [&](double start, double frequency, double gain) {
            const size_t at = (size_t)std::llround(start * rate);
            for (size_t i = 0; i < (size_t)(rate * 0.045) && at + i < samples.size(); ++i) {
                const double t = (double)i / rate;
                samples[at + i] += (float)(gain * std::sin(2 * 3.14159265359 * frequency * t) * std::exp(-t * 150));
            }
        };
        for (int k = -4; k < 0; ++k) add(first + k * period, 660, 0.05);
        add(first - period * 0.5, 880, 0.08);
        for (int k = 0; first + k * period < 23.9; ++k) add(first + k * period, 95, k % meter == 0 ? 0.8 : 0.3);
        const auto r = tempo::Estimate(samples.data(), samples.size(), rate);
        std::cout << "pickup tempo " << r.Bpm << ", downbeat " << r.FirstDownbeat << '\n';
        Check(r.Ok && r.BeatsPerMeasure == meter && std::abs(r.Bpm-bpm)<0.08 && std::abs(r.FirstDownbeat-first)<0.008, "pickup melody excluded from first downbeat");
    }
    for (const double bpm : { 120.0, 137.5, 89.75, 180.0, 240.0, 300.0 }) {
        const int meter = bpm == 180 ? 3 : 4;
        const int rate = 22050;
        const double first = 0.137, period = 60 / bpm, duration = 24;
        std::vector<float> samples((size_t)(rate * duration), 0);
        for (int beat = 0; first + beat * period < duration - 0.05; ++beat) {
            const size_t start = (size_t)std::llround((first + beat * period) * rate);
            for (size_t i = 0; i < (size_t)(rate * 0.04) && start + i < samples.size(); ++i) {
                const double time = (double)i / rate;
                samples[start + i] += (float)((beat % meter == 0 ? 0.8 : 0.3) * std::sin(2 * 3.14159265 * 95 * time) * std::exp(-time * 150));
            }
            const size_t hat = (size_t)std::llround((first + (beat + 0.5) * period) * rate);
            for (size_t i = 0; i < (size_t)(rate * 0.008) && hat + i < samples.size(); ++i) {
                const double time = (double)i / rate;
                samples[hat + i] += (float)(0.035 * std::sin(2 * 3.14159265 * 2500 * time) * std::exp(-time * 400));
            }
        }
        const auto estimate = tempo::Estimate(samples.data(), samples.size(), rate);
        std::cout << "tempo " << bpm << " -> " << estimate.Bpm << ", onset " << estimate.FirstDownbeat << '\n';
        Check(estimate.Ok && std::abs(estimate.Bpm - bpm) < 0.08, "fractional BPM regression accuracy");
        Check(std::abs(estimate.FirstDownbeat - first) < 0.006, "offset attack accuracy");
        Check(std::abs(estimate.DurationSeconds - duration) < 1e-6, "audio duration retained");
        Check(estimate.BeatsPerMeasure == meter, "three and four beat meter");
    }
    std::vector<float> silence(8000 * 8, 0);
    Check(!tempo::Estimate(silence.data(), silence.size(), 8000).Ok, "silence is not a confident tempo");
    for (size_t i = 0; i < silence.size(); ++i) { silence[i] = (float)(0.2 * std::sin(i * 2 * 3.14159265358979 * 220 / 8000)); }
    Check(!tempo::Estimate(silence.data(), silence.size(), 8000).Ok, "sustained tone is not a rhythm");
    const auto mono = tempo::PrepareMono(100, 2, [](size_t i, int channel) { return (float)std::sin(i * 0.2) * (channel == 0 ? 1 : -1); });
    Check(*std::max_element(mono.begin(), mono.end()) > 0.9f, "antiphase stereo does not cancel");
    Check(tempo::MeasureCount(120, 120, 0) == 60 && tempo::MeasureCount(120, 120, -2) == 59, "measure count respects duration and OFFSET");
    Check(tempo::MeasureCount(90, 120, 0, 3) == 60, "measure count respects meter");
}
