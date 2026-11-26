#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <cctype>
#include <cstdint>

namespace catalog {
    namespace fs = std::filesystem;
    inline std::string Extension(const fs::path& path) {
        auto ext = path.extension().string();
        for (char& c : ext) { c = (char)std::tolower((unsigned char)c); }
        return ext;
    }
    inline bool IsArchive(const fs::path& path) { const auto ext = Extension(path); return ext == ".zip" || ext == ".osz"; }
    inline bool IsChart(const fs::path& path) { const auto ext = Extension(path); return ext == ".tja" || ext == ".osu"; }
    inline std::string Key(const fs::path& path) {
        const auto key = path.lexically_normal().generic_u8string();
        return std::string(key.begin(), key.end());
    }
    struct Stamp {
        uintmax_t Size = 0;
        int64_t Time = 0;
        bool operator==(const Stamp&) const = default;
    };
    struct Snapshot {
        std::map<std::string, Stamp> Entries;
        bool Complete = true;
    };
    inline Snapshot Scan(const std::vector<fs::path>& roots) {
        Snapshot result;
        for (const auto& root : roots) {
            std::error_code ec;
            const bool exists = fs::exists(root, ec);
            if (ec) { result.Complete = false; continue; }
            if (!exists) { continue; }
            auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
            if (ec) { result.Complete = false; continue; }
            for (; it != fs::recursive_directory_iterator(); it.increment(ec)) {
                if (ec) { result.Complete = false; break; }
                if (!IsChart(it->path()) || !it->is_regular_file(ec)) { continue; }
                const auto size = it->file_size(ec);
                if (ec) { result.Complete = false; ec.clear(); continue; }
                const auto time = it->last_write_time(ec);
                if (ec) { result.Complete = false; ec.clear(); continue; }
                result.Entries[Key(it->path())] = { size, (int64_t)time.time_since_epoch().count() };
            }
            if (ec) { result.Complete = false; }
        }
        return result;
    }
}
