#pragma once
#include "TjaDocument.hpp"
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <locale>
#include <sstream>

namespace uniformscroll {
struct Position { int Measure; int64_t Num, Den; };
inline int Compare(Position a, Position b) {
    return a.Measure != b.Measure ? (a.Measure < b.Measure ? -1 : 1) : tja::ComparePos(a.Num, a.Den, b.Num, b.Den);
}
inline bool Is(std::string_view text, std::string_view key) {
    text = tja::Trim(text);
    return tja::StartsWithKey(text, key) && (text.size() == key.size() || text[key.size()] == ' ' || text[key.size()] == '\t');
}
inline double Bpm(std::string_view text) {
    if (auto comment = text.find("//"); comment != std::string_view::npos) text = text.substr(0, comment);
    const std::string value(tja::Trim(text));
    char* end = nullptr;
    const double bpm = std::strtod(value.c_str(), &end);
    if (value.empty() || end != value.c_str() + value.size() || !std::isfinite(bpm) || bpm <= 0)
        throw std::invalid_argument("BPMは正の有限の数値で指定してください");
    return bpm;
}
inline std::string Command(double scroll) {
    if (!std::isfinite(scroll) || scroll <= 0) throw std::invalid_argument("SCROLLの計算結果が範囲外です");
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "#SCROLL " << std::setprecision(17) << scroll;
    return out.str();
}
// [begin,end) matches the editor's inclusive selection plus one ending grid.
// Work on a copy so unsupported timing commands cannot leave a partial edit.
inline tja::Course Apply(const tja::Course& source, std::string_view initialBpm,
    Position begin, Position end, double visualBpm) {
    if (!std::isfinite(visualBpm) || visualBpm <= 0 || Compare(begin, end) >= 0 ||
        begin.Measure < 0 || end.Measure >= (int)source.Measures.size() ||
        begin.Den <= 0 || end.Den <= 0 || begin.Num < 0 || begin.Num >= begin.Den || end.Num < 0 || end.Num > end.Den)
        throw std::invalid_argument("範囲と見た目BPMを確認してください");
    double bpm = Bpm(initialBpm.empty() ? "120" : initialBpm);
    std::string restore = "#SCROLL 1";
    bool hasRestore = false;
    struct Point { Position Pos; double Bpm; };
    std::vector<Point> points;
    for (int m = 0; m <= end.Measure; ++m) for (const auto& c : source.Measures[m].Commands) {
        const Position p{m, c.Num, c.Den};
        if (Compare(p, end) > 0) continue;
        if (Is(c.Text, "#BRANCHSTART") || Is(c.Text, "#BMSCROLL") || Is(c.Text, "#HBSCROLL"))
            throw std::invalid_argument("分岐・BMSCROLL・HBSCROLLを含む範囲の見た目BPM統一には対応していません");
        if (Is(c.Text, "#SCROLL")) {
            restore = c.Text;
            if (Compare(p, end) == 0) hasRestore = true;
        }
        if (Is(c.Text, "#BPMCHANGE")) {
            bpm = Bpm(tja::Trim(std::string_view(c.Text)).substr(10));
            if (Compare(p, begin) <= 0) continue;
            if (Compare(p, end) < 0) {
                if (!points.empty() && Compare(points.back().Pos, p) == 0) points.back().Bpm = bpm;
                else points.push_back({p, bpm});
            }
        }
    }
    // Re-read the BPM at the exact beginning, including same-position changes.
    double startBpm = Bpm(initialBpm.empty() ? "120" : initialBpm);
    for (int m = 0; m <= begin.Measure; ++m) for (const auto& c : source.Measures[m].Commands)
        if (Compare({m,c.Num,c.Den}, begin) <= 0 && Is(c.Text, "#BPMCHANGE")) startBpm = Bpm(tja::Trim(std::string_view(c.Text)).substr(10));
    points.insert(points.begin(), {begin, startBpm});
    auto result = source;
    for (int m = begin.Measure; m <= end.Measure; ++m)
        std::erase_if(result.Measures[m].Commands, [&](const tja::Command& c) {
            const Position p{m,c.Num,c.Den};
            return Is(c.Text,"#SCROLL") && Compare(p,begin)>=0 && Compare(p,end)<0;
        });
    for (const auto& p : points) tja::Document::AddCommand(result.Measures[p.Pos.Measure], p.Pos.Num, p.Pos.Den, Command(visualBpm / p.Bpm));
    if (!hasRestore) tja::Document::AddCommand(result.Measures[end.Measure], end.Num, end.Den, restore);
    return result;
}
}
