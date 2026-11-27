#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

namespace pcmmixer {
struct Voice { double Pos = 0; bool Playing = false; };

inline void Mix(std::span<float> output, std::span<const float> source, int channels,
    double step, float gain, bool loop, Voice& voice) {
    if (!voice.Playing) { return; }
    if ((channels != 1 && channels != 2) || source.empty() || source.size() % channels
        || !std::isfinite(step) || step <= 0 || !std::isfinite(voice.Pos) || voice.Pos < 0 || !std::isfinite(gain)) {
        voice.Playing = false; return;
    }
    const size_t total = source.size()/channels, frames = output.size()/2;
    double pos = voice.Pos;
    if (step == 1 && pos == std::floor(pos)) {
        size_t written = 0;
        while (written < frames) {
            if (pos >= (double)total) {
                if (!loop) { voice.Playing = false; break; }
                pos = std::fmod(pos, (double)total);
            }
            const size_t start = (size_t)pos, count = std::min(frames-written, total-start);
            if (channels == 2) {
                const float* in = source.data()+start*2;
                float* out = output.data()+written*2;
                for (size_t i=0;i<count*2;++i) { out[i] += in[i]*gain; }
            }
            else {
                for (size_t i=0;i<count;++i) {
                    const float sample = source[start+i]*gain;
                    output[(written+i)*2] += sample;
                    output[(written+i)*2+1] += sample;
                }
            }
            pos += count; written += count;
        }
    }
    else {
        for (size_t i=0;i<frames;++i) {
            if (pos >= (double)total) {
                if (!loop) { voice.Playing = false; break; }
                pos = std::fmod(pos, (double)total);
            }
            const size_t first=(size_t)pos, next=first+1<total?first+1:(loop?0:first);
            const float fraction=(float)(pos-first);
            const float* a=source.data()+first*channels;
            const float* b=source.data()+next*channels;
            output[i*2] += (a[0]+(b[0]-a[0])*fraction)*gain;
            output[i*2+1] += (channels==2?a[1]+(b[1]-a[1])*fraction:a[0]+(b[0]-a[0])*fraction)*gain;
            pos += step;
        }
    }
    if (pos >= (double)total) {
        if (loop) { pos=std::fmod(pos,(double)total); }
        else { voice.Playing=false; }
    }
    voice.Pos=pos;
}
}
