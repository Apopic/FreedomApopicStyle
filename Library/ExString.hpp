#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <ranges>
#include <algorithm>
#include <clocale>
#include <cuchar>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include "OsuTaiko.hpp"
#include "../Platform/Text.hpp"
#include "fast_float.h"

template<typename T>
std::vector<T> split(T str, char del) {
    std::vector<T> result;
    for (auto&& s : str | std::views::split(del)) {
        result.emplace_back(s.begin(), s.end());
    }
    return result;
}

inline std::string_view strtrim(std::string_view str) {
    const size_t s = str.find_first_not_of(" \t");
    if (s == std::string_view::npos) {
        return {};
    }
    const size_t e = str.find_last_not_of(" \t");
    return str.substr(s, e - s + 1);
}

inline std::string ToLower(std::string_view str) {
    std::string ret(str);
    std::ranges::transform(str, ret.begin(), [](unsigned char c) { return std::tolower(c); });
    return ret;
}

inline size_t GetStrlen(std::string_view data, int handle) {
    const int width = txt::Width(handle, data);
    return width > 0 ? (size_t)width : 0;
}

// 毎フレーム同じ文字列の幅を測る描画処理向けのキャッシュ (メインスレッド専用)
inline std::unordered_map<std::string, size_t>& StrlenCache() {
    static std::unordered_map<std::string, size_t> cache;
    return cache;
}
inline void ClearStrlenCache() {
    StrlenCache().clear();
}
inline size_t GetStrlenCached(std::string_view data, int handle) {
    auto& cache = StrlenCache();
    if (cache.size() > 4096) {
        cache.clear();
    }
    std::string key;
    key.reserve(data.size() + sizeof(int));
    key.append(reinterpret_cast<const char*>(&handle), sizeof(int));
    key.append(data);
    if (auto it = cache.find(key); it != cache.end()) {
        return it->second;
    }
    const auto width = GetStrlen(data, handle);
    if (width == 0) { return 0; } // フォントの準備前の失敗はキャッシュしない
    return cache.emplace(std::move(key), width).first->second;
}

template<typename func>
void Exsubstr(std::string_view raw, std::string_view name, func f) {
    if (!raw.starts_with(name)) {
        return;
    }
    raw.remove_prefix(name.size());
    std::string_view data = strtrim(raw);
    f(data);
}

template <typename T>
T svtov(std::string_view sv) {
    T value{};
    fast_float::from_chars(sv.data(), sv.data() + sv.size(), value);
    return value;
}

inline bool is_valid_utf8(std::string_view sv) {
    auto it = sv.begin();
    auto end = sv.end();

    while (it != end) {
        uint8_t b1 = static_cast<uint8_t>(*it++);

        if (b1 <= 0x7F) {
            continue;
        }

        if (b1 >= 0xC2 && b1 <= 0xDF) {
            if (it == end) return false;
            uint8_t b2 = static_cast<uint8_t>(*it++);
            if (b2 < 0x80 || b2 > 0xBF) return false;
            continue;
        }

        if (b1 >= 0xE0 && b1 <= 0xEF) {
            if (it == end) return false;
            uint8_t b2 = static_cast<uint8_t>(*it++);

            if (b2 < 0x80 || b2 > 0xBF) return false;

            if (b1 == 0xE0 && b2 < 0xA0) return false;
            if (b1 == 0xED && b2 > 0x9F) return false;

            if (it == end) return false;
            uint8_t b3 = static_cast<uint8_t>(*it++);
            if (b3 < 0x80 || b3 > 0xBF) return false;
            continue;
        }

        if (b1 >= 0xF0 && b1 <= 0xF4) {
            if (it == end) return false;
            uint8_t b2 = static_cast<uint8_t>(*it++);

            if (b2 < 0x80 || b2 > 0xBF) return false;

            if (b1 == 0xF0 && b2 < 0x90) return false;
            if (b1 == 0xF4 && b2 > 0x8F) return false;

            for (int i = 0; i < 2; ++i) {
                if (it == end) return false;
                uint8_t bx = static_cast<uint8_t>(*it++);
                if (bx < 0x80 || bx > 0xBF) return false;
            }
            continue;
        }

        return false;
    }

    return true;
}

#ifndef __ANDROID__

inline std::wstring StrToWStr(const std::string& str) {
    if (str.empty()) return {};
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    if (size_needed == 0) throw std::system_error(GetLastError(), std::system_category());
    std::wstring w; w.resize(size_needed);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &w[0], size_needed);
    return w;
}

inline std::string SjisToUtf8(const std::string& str) {
    if (str.empty()) return "";
    if (is_valid_utf8(str)) return str;

    int wlen = MultiByteToWideChar(932, 0, str.c_str(), -1, NULL, 0);
    std::vector<wchar_t> wstr(wlen);
    MultiByteToWideChar(932, 0, str.c_str(), -1, wstr.data(), wlen);

    int ulen = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, NULL, 0, NULL, NULL);
    std::vector<char> ustr(ulen);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, ustr.data(), ulen, NULL, NULL);

    return std::string(ustr.data());
}
#else

#include <iconv.h>

inline std::string SjisToUtf8(const std::string& str) {
    if (str.empty()) return "";
    if (is_valid_utf8(str)) return str;

    iconv_t cd = iconv_open("UTF-8", "CP932");
    if (cd == (iconv_t)-1) return str;

    std::vector<char> outBuf(str.size() * 3);
    char* inPtr = const_cast<char*>(str.data()), * outPtr = outBuf.data();
    size_t inBytes = str.size(), outBytes = outBuf.size();

    size_t res = iconv(cd, &inPtr, &inBytes, &outPtr, &outBytes);
    iconv_close(cd);

    if (res == (size_t)-1) return str;
    return std::string(outBuf.data(), outBuf.size() - outBytes);
}

#endif

// テキストファイルを一括で読み込み、文字コード変換 (Shift-JIS -> UTF-8) もファイル単位で 1 回だけ行う。
// 行は data を指す string_view なので、TextLines より長く保持しないこと。
struct TextLines {
    std::string data;
    std::vector<std::string_view> lines;

    bool Load(const std::filesystem::path& path, bool osuMetadataOnly = false) {
        data.clear();
        lines.clear();

        std::ifstream ifs(path, std::ios::binary | std::ios::ate);
        if (!ifs.is_open()) {
            return false;
        }
        const auto size = ifs.tellg();
        if (size < 0 || size > 64 * 1024 * 1024) {
            return false;
        }
        data.resize(static_cast<size_t>(size));
        ifs.seekg(0, std::ios::beg);
        if (!ifs.read(data.data(), size)) { data.clear(); return false; }
        ifs.close();

        Parse(osuMetadataOnly);
        return !lines.empty();
    }

    // ネットワーク経由で受け取った譜面など、ファイルを介さずに読み込む
    void LoadFromMemory(std::string_view bytes) {
        data.assign(bytes);
        lines.clear();
        Parse();
    }

private:

    void Parse(bool osuMetadataOnly = false) {
        // UTF-8 BOM があると 1 行目 (TITLE: など) が一致しなくなるため除去する
        if (data.starts_with("\xEF\xBB\xBF")) {
            data.erase(0, 3);
        }
#ifndef __ANDROID__
        data = SjisToUtf8(data);
#endif
        if (data.starts_with("osu file format v")) {
            if (osuMetadataOnly) {
                const auto metadata = osutaiko::Metadata(data);
                if (!metadata) { data.clear(); return; }
                data = *metadata;
            }
            else {
                const auto converted = osutaiko::Parse(data);
                if (!converted) { data.clear(); return; }
                data = converted->Write();
            }
        }

        lines.reserve(std::count(data.begin(), data.end(), '\n') + 1);
        std::string_view view(data);
        while (!view.empty()) {
            size_t pos = view.find('\n');
            std::string_view line = view.substr(0, pos);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            lines.push_back(line);
            if (pos == std::string_view::npos) {
                break;
            }
            view.remove_prefix(pos + 1);
        }
    }
};
