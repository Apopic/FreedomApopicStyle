#include "../System.h"
#include <iostream>
int main() {
    ChangeWindowMode(TRUE); SetWindowVisibleFlag(FALSE);
    if (DxLib_Init() == -1) { std::cerr << "Audio initialization unavailable\n"; return 2; }
    for (const char* path : { "build/codec.wav", "build/codec.mp3", "build/codec-decoded.wav", "build/codec48.mp3" }) {
        const auto timing = mp3timing::Read(path);
        const int handle = LoadAlignedSoftSound(path);
        if (handle == -1) { std::cout << path << ": decode failed\n"; continue; }
        int channels = 0, bits = 0, rate = 0, floating = 0;
        GetSoftSoundFormat(handle, &channels, &bits, &rate, &floating);
        const auto frames = GetSoftSoundSampleNum(handle);
        const void* data = GetSoftSoundDataImage(handle);
        std::vector<double> rms((size_t)(frames * 1000 / rate));
        for (size_t i = 0; i < rms.size(); ++i) {
            double energy = 0;
            for (size_t sample = i * rate / 1000; sample < (i + 1) * rate / 1000; ++sample) {
                const double value = bits == 16 ? ((const int16_t*)data)[sample * channels] / 32768.0 : floating ? ((const float*)data)[sample * channels] : 0;
                energy += value * value;
            }
            rms[i] = std::sqrt(energy / std::max(1, rate / 1000));
        }
        const double peak = *std::max_element(rms.begin(), rms.end());
        const auto start = std::find_if(rms.begin(), rms.end(), [&](double value) { return value > peak * 0.1; });
        std::cout << path << ": duration_ms=" << (double)frames * 1000 / rate << ", onset_ms=" << (start - rms.begin()) << ", rate=" << rate << '\n';
        if (std::abs((double)frames / rate - 2.0) > 0.002 || std::abs((start - rms.begin()) - 500) > 1) return 1;
        if (timing.Rate) {
            SoundData sound; sound.Load(path);
            std::cout << "playback compensation_ms=" << sound.StartMilliseconds << '\n';
            if (std::abs(sound.StartMilliseconds - timing.Head(GetSoundTotalSample(sound.Handle), rate) * 1000.0 / rate) > 0.01) return 1;
            sound.Delete();
        }
        DeleteSoftSound(handle);
    }
    DxLib_End();
}
