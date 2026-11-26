#pragma once
// osu!taiko の Mode:1 を共通の TJA モデルへ変換する。
// https://osu.ppy.sh/wiki/en/Client/File_formats/osu_(file_format)
#include "TjaDocument.hpp"
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <locale>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace osutaiko {
    inline int FormatVersion(std::string_view source) {
        if (source.starts_with("\xEF\xBB\xBF")) { source.remove_prefix(3); }
        if (!source.starts_with("osu file format v")) { return 0; }
        const auto version = tja::Trim(source.substr(17, source.find('\n') - 17));
        int value = 0;
        for (char c : version) { if (c < '0' || c > '9') { return 0; } value = value * 10 + c - '0'; if (value > 1000) { return 0; } }
        return value;
    }
    inline std::string AudioFilename(std::string value) {
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') { value = value.substr(1, value.size() - 2); }
        std::replace(value.begin(), value.end(), '\\', '/');
        return value;
    }
    inline bool IsConverted(const std::vector<std::string_view>& lines) {
        return std::ranges::any_of(lines, [](auto line) { return line.starts_with("FASOSUFORMAT:"); });
    }
    inline std::vector<std::string_view> Split(std::string_view text, char delimiter) {
        std::vector<std::string_view> fields;
        do {
            const size_t pos = text.find(delimiter);
            fields.push_back(tja::Trim(text.substr(0, pos)));
            if (pos == std::string_view::npos) { break; }
            text.remove_prefix(pos + 1);
        } while (true);
        return fields;
    }
    inline double Number(std::string_view text) {
        std::string copy(text);
        char* end = nullptr;
        const double value = std::strtod(copy.c_str(), &end);
        if (copy.empty() || end != copy.c_str() + copy.size() || !std::isfinite(value)) { throw std::invalid_argument("osu numeric value"); }
        return value;
    }
    inline std::string NumberText(double value) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(14) << value;
        return out.str();
    }
    // 選曲では音符列の展開も TJA モデルの生成も行わず、表示・検索に必要な情報だけ読む。
    inline std::optional<std::string> Metadata(std::string_view source) {
        const int version = FormatVersion(source);
        if (!version) { return std::nullopt; }
        try {
            std::unordered_map<std::string, std::string> properties;
            std::vector<double> bpms;
            std::string section;
            size_t circles = 0;
            double first = std::numeric_limits<double>::infinity();
            for (auto line : Split(source, '\n')) {
                if (line.empty() || line.starts_with("//")) { continue; }
                if (line.front() == '[' && line.back() == ']') { section = line; continue; }
                if (section == "[TimingPoints]") {
                    const auto fields = Split(line, ',');
                    if (fields.size() < 2) { return std::nullopt; }
                    first = std::min(first, Number(fields[0]));
                    if (fields.size() < 7 || fields[6] != "0") { const double beat = Number(fields[1]); if (beat <= 0) { return std::nullopt; } bpms.push_back(60000 / beat); }
                }
                else if (section == "[HitObjects]") {
                    const auto fields = Split(line, ',');
                    if (fields.size() < 5) { return std::nullopt; }
                    first = std::min(first, Number(fields[2]));
                    const double type = Number(fields[3]);
                    if (type < 0 || type > 255) { return std::nullopt; }
                    if ((int)type & 1) { ++circles; }
                    if (circles > 200000) { return std::nullopt; }
                }
                else if (section == "[General]" || section == "[Metadata]" || section == "[Difficulty]") {
                    const size_t colon = line.find(':');
                    if (colon != std::string_view::npos) { properties[std::string(tja::Trim(line.substr(0, colon)))] = tja::Trim(line.substr(colon + 1)); }
                }
            }
            if (properties["Mode"] != "1" || bpms.empty() || !std::isfinite(first)) { return std::nullopt; }
            const double legacy = version < 5 ? 24 : 0;
            const double od = properties["OverallDifficulty"].empty() ? 5 : std::clamp(Number(properties["OverallDifficulty"]), 0.0, 10.0);
            const double previewRaw = properties["PreviewTime"].empty() ? -1 : Number(properties["PreviewTime"]);
            const double preview = previewRaw < 0 ? 0 : previewRaw + legacy;
            std::string out = "TITLE:" + (properties["TitleUnicode"].empty() ? properties["Title"] : properties["TitleUnicode"]) + "\nSUBTITLE:--" + properties["Version"] +
                "\nGENRE:osu!taiko\nWAVE:" + AudioFilename(properties["AudioFilename"]) + "\nBPM:" + NumberText(bpms.front()) +
                "\nOFFSET:" + NumberText(-(first + legacy) / 1000) + "\nDEMOSTART:" + NumberText(preview / 1000) +
                "\nFASOSUFORMAT:" + std::to_string(version) + "\nCOURSE:Oni\nLEVEL:" + std::to_string(std::max(1, (int)std::ceil(od))) + "\n#START\n";
            for (double bpm : bpms) { out += "#BPMCHANGE " + NumberText(bpm) + "\n"; }
            out += std::string(circles, '1') + ",\n#END\n";
            return out;
        } catch (const std::exception&) { return std::nullopt; }
    }
    inline std::optional<tja::Document> Parse(std::string_view source) {
        if (source.starts_with("\xEF\xBB\xBF")) { source.remove_prefix(3); }
        if (!source.starts_with("osu file format v")) { return std::nullopt; }
        try {
            const int version = FormatVersion(source);
            if (!version) { return std::nullopt; }
            const double legacyOffset = version < 5 ? 24.0 : 0.0;
            struct Timing { double Time, Beat, Speed; int Meter; bool Red, Kiai; };
            struct Event { double Time; char Type = 0; std::string Command; };
            std::unordered_map<std::string, std::string> properties;
            std::vector<std::vector<std::string_view>> rawTiming, objects;
            std::string section;
            for (auto line : Split(source, '\n')) {
                if (line.empty() || line.starts_with("//")) { continue; }
                if (line.front() == '[' && line.back() == ']') { section = line; continue; }
                if (section == "[TimingPoints]") { rawTiming.push_back(Split(line, ',')); }
                else if (section == "[HitObjects]") { objects.push_back(Split(line, ',')); }
                else if (section == "[General]" || section == "[Metadata]" || section == "[Difficulty]") {
                    const size_t colon = line.find(':');
                    if (colon != std::string_view::npos) { properties[std::string(tja::Trim(line.substr(0, colon)))] = tja::Trim(line.substr(colon + 1)); }
                }
            }
            if (properties["Mode"] != "1" || rawTiming.empty() || objects.empty() || objects.size() > 200000 || rawTiming.size() > 200000) { return std::nullopt; }
            auto property = [&](const char* key, double fallback) {
                const auto it = properties.find(key);
                return it == properties.end() ? fallback : Number(it->second);
            };
            const double multiplier = property("SliderMultiplier", 1.4), od = property("OverallDifficulty", 5);
            if (multiplier <= 0 || multiplier > 100 || od < 0 || od > 10) { return std::nullopt; }
            std::stable_sort(rawTiming.begin(), rawTiming.end(), [](const auto& a, const auto& b) { return Number(a.at(0)) < Number(b.at(0)); });
            std::vector<Timing> timings;
            double beat = 500, speed = 1;
            int meter = 4;
            for (const auto& fields : rawTiming) {
                if (fields.size() < 2) { return std::nullopt; }
                const bool red = fields.size() < 7 || Number(fields[6]) != 0;
                const bool noTicks = !red && (fields[1] == "NaN" || fields[1] == "nan");
                const double length = noTicks ? 0 : Number(fields[1]), time = Number(fields[0]) + legacyOffset;
                if (red) {
                    if (length < 1 || length > 60000) { return std::nullopt; }
                    beat = length; speed = 1;
                    const double beats = fields.size() > 2 ? Number(fields[2]) : 4;
                    if (beats < 1 || beats > 32) { return std::nullopt; }
                    meter = (int)beats;
                }
                else {
                    speed = length < 0 ? std::clamp(-100.0 / length, 0.1, 10.0) : 1;
                }
                if (std::abs(time) > 86400000) { return std::nullopt; }
                const double effects = fields.size() > 7 ? Number(fields[7]) : 0;
                if (effects < 0 || effects > 255) { return std::nullopt; }
                timings.push_back({ time, beat, speed, meter, red, ((int)effects & 1) != 0 });
            }
            auto timingAt = [&](double time) -> const Timing& {
                auto it = std::upper_bound(timings.begin(), timings.end(), time, [](double t, const Timing& p) { return t < p.Time; });
                return it == timings.begin() ? timings.front() : *std::prev(it);
            };
            std::vector<Event> events;
            bool kiai = false;
            for (const auto& timing : timings) {
                if (timing.Red) { events.push_back({ timing.Time, 0, "#BPMCHANGE " + NumberText(60000 / timing.Beat) }); }
                events.push_back({ timing.Time, 0, "#SCROLL " + NumberText(timing.Speed) });
                if (timing.Kiai != kiai) { events.push_back({ timing.Time, 0, timing.Kiai ? "#GOGOSTART" : "#GOGOEND" }); kiai = timing.Kiai; }
            }
            std::string balloons;
            for (const auto& object : objects) {
                if (object.size() < 5) { return std::nullopt; }
                const double time = Number(object[2]) + legacyOffset;
                const double typeValue = Number(object[3]), soundValue = Number(object[4]);
                if (time < -86400000 || time > 86400000 || typeValue < 0 || typeValue > 255 || soundValue < 0 || soundValue > 255) { return std::nullopt; }
                const int type = (int)typeValue, sound = (int)soundValue;
                if (type & 1) { events.push_back({ time, (char)((sound & 10) ? ((sound & 4) ? '4' : '2') : ((sound & 4) ? '3' : '1')), {} }); }
                else if (type & 2) {
                    if (object.size() < 8) { return std::nullopt; }
                    const auto& timing = timingAt(time);
                    const double repeats = Number(object[6]), length = Number(object[7]);
                    if (repeats < 1 || repeats > 10000 || length <= 0) { return std::nullopt; }
                    const double end = time + length * repeats / (multiplier * 100 * timing.Speed) * timing.Beat;
                    if (!std::isfinite(end) || end > 86400000) { return std::nullopt; }
                    events.push_back({ time, (sound & 4) ? '6' : '5', {} });
                    events.push_back({ end, '8', {} });
                }
                else if (type & 8) {
                    if (object.size() < 6) { return std::nullopt; }
                    const double end = Number(object[5]) + legacyOffset;
                    if (end <= time || end > 86400000) { return std::nullopt; }
                    events.push_back({ time, '9', {} }); events.push_back({ end, '8', {} });
                    // TJA には交互入力の denden が無いため、打数を時間と OD から見積もるくす玉として扱う。
                    balloons += (balloons.empty() ? "" : ",") + std::to_string(std::max(1, (int)std::ceil((end - time) / 1000 * (5 + od * 0.5))));
                }
                else { return std::nullopt; }
            }
            std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) { return a.Time < b.Time; });
            const double start = events.front().Time, finish = events.back().Time;
            if (finish - start > 86400000) { return std::nullopt; }
            std::string title = properties["TitleUnicode"].empty() ? properties["Title"] : properties["TitleUnicode"];
            auto doc = tja::Document::NewChart(title, AudioFilename(properties["AudioFilename"]), 1);
            tja::Document::SetValue(doc.Header, "FASOSUFORMAT", std::to_string(version));
            tja::Document::SetValue(doc.Header, "SUBTITLE", "--" + properties["Version"]);
            tja::Document::SetValue(doc.Header, "GENRE", "osu!taiko");
            tja::Document::SetValue(doc.Header, "BPM", NumberText(60000 / timingAt(start).Beat));
            tja::Document::SetValue(doc.Header, "OFFSET", NumberText(-start / 1000));
            const double previewRaw = property("PreviewTime", -1);
            tja::Document::SetValue(doc.Header, "DEMOSTART", NumberText(previewRaw < 0 ? 0 : (previewRaw + legacyOffset) / 1000));
            auto& course = doc.Courses.front();
            tja::Document::SetValue(course.Header, "LEVEL", std::to_string(std::max(1, (int)std::ceil(od))));
            tja::Document::SetValue(course.Header, "BALLOON", balloons);
            course.Measures.clear();
            size_t ei = 0;
            double from = start;
            while (from <= finish) {
                if (course.Measures.size() >= 100000) { return std::nullopt; }
                const auto& timing = timingAt(from);
                double to = from + timing.Beat * timing.Meter;
                auto next = std::upper_bound(timings.begin(), timings.end(), from, [](double t, const Timing& p) { return t < p.Time; });
                for (; next != timings.end() && next->Time < to; ++next) { if (next->Red) { to = next->Time; break; } }
                if (to <= from) { return std::nullopt; }
                tja::Measure measure;
                tja::Document::AddCommand(measure, 0, 1, "#MEASURE " + NumberText((to - from) / (4 * timing.Beat)) + "/1");
                constexpr int64_t grid = 19200; // 標準 BPM で 1ms 未満の丸め誤差
                while (ei < events.size() && events[ei].Time < to) {
                    const auto& event = events[ei++];
                    int64_t position = std::clamp<int64_t>(std::llround((event.Time - from) / (to - from) * grid), 0, grid - 1);
                    // 小節頭の丸め誤差を吸収する。通常音符だけを最大 0.5ms 早める。
                    // 命令をまたぐ場合や、既に音符がある場合は動かさない。
                    if (event.Type >= '1' && event.Type <= '4' && event.Time - from <= 0.5 &&
                        measure.Notes.empty() && measure.Commands.back().Num == 0) { position = 0; }
                    if (event.Type) { tja::Document::SetNote(measure, position, grid, event.Type); }
                    else { tja::Document::AddCommand(measure, position, grid, event.Command); }
                }
                course.Measures.push_back(std::move(measure));
                from = to;
            }
            return doc;
        } catch (const std::exception&) { return std::nullopt; }
    }
}
