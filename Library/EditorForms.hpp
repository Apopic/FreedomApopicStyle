#pragma once
#include "TjaDocument.hpp"
#include <sstream>

namespace editform {
    struct Field { std::string Label, Value; std::vector<std::string> Choices; int Handle = -1; };
    struct Command {
        std::string Key;
        char Separator = ' ';
        std::vector<Field> Fields;
        std::string Join(const std::vector<std::string>& values) const {
            if (std::ranges::all_of(values, [](const auto& v) { return tja::Trim(v).empty(); })) { return {}; }
            std::string text = Key + " ";
            for (size_t i = 0; i < values.size(); ++i) { if (i) { text += Separator; } text += tja::Trim(values[i]); }
            return text;
        }
    };
    inline Command SplitCommand(std::string_view text) {
        text = tja::Trim(text);
        const size_t space = text.find_first_of(" \t");
        Command out{ std::string(text.substr(0, space)) };
        if (space == std::string_view::npos) { return out; }
        std::string rest(tja::Trim(text.substr(space)));
        if (tja::StartsWithKey(out.Key, "#MEASURE")) {
            const auto slash = rest.find('/'); out.Separator = '/';
            out.Fields = { { "分子", rest.substr(0, slash) }, { "分母", slash == std::string::npos ? "1" : rest.substr(slash + 1) } };
        }
        else if (out.Key == "#BRANCHSTART") {
            out.Separator = ',';
            const size_t first = rest.find(','), second = rest.find(',', first == std::string::npos ? rest.size() : first + 1);
            out.Fields = { { "判定方法", rest.substr(0, first), { "p", "r", "s" } },
                { "玄人の閾値", first == std::string::npos ? "80" : rest.substr(first + 1, second == std::string::npos ? second : second - first - 1) },
                { "達人の閾値", second == std::string::npos ? "90" : rest.substr(second + 1) } };
        }
        else {
            std::istringstream input(rest); std::string value;
            while (input >> value) { out.Fields.push_back({ "値 " + std::to_string(out.Fields.size() + 1), value }); }
            if (out.Fields.size() == 1) {
                if (out.Key == "#BPMCHANGE") { out.Fields[0].Label = "BPM"; }
                else if (out.Key == "#SCROLL") { out.Fields[0].Label = "スクロール倍率"; }
                else if (out.Key == "#DELAY") { out.Fields[0].Label = "停止時間 (秒)"; }
            }
        }
        return out;
    }
}
