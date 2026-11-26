#pragma once
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace mp3timing {
struct Info {
    int Rate = 0, Delay = 0, Padding = 0;
    int64_t Samples = 0;
    bool ConstantBitrateTag = false;
    // Windows' MPEG decoder removes its own 528-sample synthesis delay,
    // but leaves the encoder delay and tail padding. Match the decoded length
    // before compensating; already gapless decoders must remain untouched.
    int Head(int64_t decoded, int rate) const {
        if (Rate != rate || Samples <= Delay + Padding || Delay <= 0 || decoded == Samples - Delay - Padding) return 0;
        if (decoded == Samples - 528) return Delay;
        // The same Windows decoder also exposes the silent Info frame for CBR.
        if (ConstantBitrateTag && decoded == Samples + 1152 - 528) return Delay + 1152;
        return 0;
    }
};
inline Info Parse(const void* image, size_t size) {
    Info result;
    if (!image || size < 4) return result;
    const auto* d = static_cast<const uint8_t*>(image);
    size_t begin = 0;
    if (size >= 10 && std::memcmp(d, "ID3", 3) == 0) {
        for (int i = 6; i < 10; ++i) if (d[i] & 128) return result;
        begin = 10 + ((size_t)d[6] << 21) + ((size_t)d[7] << 14) + ((size_t)d[8] << 7) + d[9];
        if (d[5] & 16) begin += 10;
    }
    for (size_t p = begin; p + 4 <= size && p - begin < 4096; ++p) {
        // MPEG-1 Layer III only. Other decoder formats are deliberately left alone.
        if (d[p] != 255 || (d[p + 1] & 254) != 250 || (d[p + 2] & 12) == 12) continue;
        const int rates[] = { 44100, 48000, 32000, 0 };
        const int rate = rates[(d[p + 2] >> 2) & 3];
        if (!rate) continue;
        size_t x = p + 4 + ((d[p + 1] & 1) ? 0 : 2) + ((d[p + 3] >> 6) == 3 ? 17 : 32);
        if (x + 8 > size || (std::memcmp(d + x, "Xing", 4) && std::memcmp(d + x, "Info", 4))) continue;
        result.ConstantBitrateTag = std::memcmp(d + x, "Info", 4) == 0;
        auto be32 = [&](size_t i) { return (uint32_t)d[i] << 24 | (uint32_t)d[i+1] << 16 | (uint32_t)d[i+2] << 8 | d[i+3]; };
        const uint32_t flags = be32(x + 4);
        x += 8;
        if (!(flags & 1) || (flags & ~15u) || x + 4 > size) return result;
        const uint32_t frames = be32(x); x += 4;
        if (flags & 2) x += 4;
        if (flags & 4) x += 100;
        if (flags & 8) x += 4;
        if (x + 24 > size || (std::memcmp(d + x, "LAME", 4) && std::memcmp(d + x, "Lavc", 4))) return result;
        result.Rate = rate;
        result.Delay = (d[x + 21] << 4) | (d[x + 22] >> 4);
        result.Padding = ((d[x + 22] & 15) << 8) | d[x + 23];
        result.Samples = (int64_t)frames * 1152;
        return result;
    }
    return result;
}
inline Info Read(const std::string& path) {
    std::ifstream file(std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(path.data()), path.size())), std::ios::binary);
    uint8_t tag[10]{};
    if (!file.read(reinterpret_cast<char*>(tag), 10)) return {};
    size_t skip = 0;
    if (std::memcmp(tag, "ID3", 3) == 0) {
        for (int i = 6; i < 10; ++i) if (tag[i] & 128) return {};
        skip = 10 + ((size_t)tag[6] << 21) + ((size_t)tag[7] << 14) + ((size_t)tag[8] << 7) + tag[9];
        if (tag[5] & 16) skip += 10;
    }
    file.seekg(skip);
    std::vector<uint8_t> bytes(8192);
    file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    return Parse(bytes.data(), (size_t)file.gcount());
}
}
