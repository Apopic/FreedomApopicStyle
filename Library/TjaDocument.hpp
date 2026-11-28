#pragma once
// 譜面制作モード用の TJA 文書モデル。
// 譜面を「ヘッダー + コース (コースのヘッダー + 小節の並び)」に分け、小節の中の音符と命令は位置を分数 (Num / Den) で持つ。
// 書き出す時は小節ごとに分母の最小公倍数で 1 行の音符列にし、命令はその位置で行を分けて挿入する (TJA の書き方そのまま)。
// 文字列は UTF-8 で扱う (読み込みは TextLines が Shift_JIS から変換する。保存時の文字コードは呼び出し側が決める)。

#include <algorithm>
#include <cstdint>
#include <cctype>
#include <numeric>
#include <string>
#include <string_view>
#include <stdexcept>
#include <vector>

namespace tja {

    struct Note {
        int64_t Num = 0;            // 小節の中の位置 Num / Den (0 <= 位置 < 1)
        int64_t Den = 1;
        char Type = '1';            // '1'～'9' (と拡張の文字)。'0' は置かない
    };
    struct Command {
        int64_t Num = 0;            // 位置 Num / Den (0 <= 位置 <= 1。1 は小節の終わり = 音符の後、',' の前)
        int64_t Den = 1;
        std::string Text;           // "#BPMCHANGE 180" など (行そのまま)
    };
    struct Measure {
        int64_t Resolution = 1;             // 元の音符の文字数 (書き出す時もこの細かさ以上で書き、行が不要に変わらないようにする)
        std::vector<Note> Notes;            // 位置順
        std::vector<Command> Commands;      // 位置順 (同じ位置は書いた順)
    };
    struct Course {
        std::vector<std::string> Header;    // COURSE: から #START の前まで (LEVEL: BALLOON: など)
        std::string StartLine = "#START";
        std::vector<Measure> Measures;
        std::vector<std::string> Trailing;  // 最後の ',' の後、#END の前の命令
        std::string EndLine = "#END";
    };

    // 位置の比較 (a/b と c/d)
    inline int ComparePos(int64_t an, int64_t ad, int64_t bn, int64_t bd) {
        const int64_t l = an * bd, r = bn * ad;
        return l < r ? -1 : (l > r ? 1 : 0);
    }
    inline void Reduce(int64_t& num, int64_t& den) {
        if (den <= 0 || num < 0) { num = 0; den = 1; return; }
        const int64_t g = std::gcd(num, den);
        if (g > 1) { num /= g; den /= g; }
        if (num == 0) { den = 1; }
    }
    inline std::string_view Trim(std::string_view s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) { s.remove_prefix(1); }
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) { s.remove_suffix(1); }
        return s;
    }
    inline bool StartsWithKey(std::string_view line, std::string_view key) {
        line = Trim(line);
        if (line.size() < key.size()) { return false; }
        for (size_t i = 0; i < key.size(); ++i) {
            if (std::toupper((unsigned char)line[i]) != std::toupper((unsigned char)key[i])) { return false; }
        }
        return true;
    }

    class Document {
    public:
        std::vector<std::string> Header;    // 最初のコースより前の行 (TITLE: BPM: WAVE: など)
        std::vector<Course> Courses;

        // ---------------------------------------------------------------- 読み込み
        static Document Parse(const std::vector<std::string_view>& lines) {
            Document doc;
            enum class State { Header, CourseHeader, Notes, AfterEnd } state = State::Header;
            std::vector<std::string> pending;       // #END の後、次のコースの前の行
            Course* course = nullptr;
            std::string buffer;                     // 小節の音符 (',' まで)
            std::vector<std::pair<size_t, std::string>> commands;   // 音符の何文字目の前に入る命令か

            auto begin_course = [&](std::vector<std::string> header) {
                doc.Courses.emplace_back();
                course = &doc.Courses.back();
                course->Header = std::move(header);
            };
            auto finish_measure = [&] {
                Measure m;
                const int64_t len = (int64_t)buffer.size();
                m.Resolution = std::max<int64_t>(len, 1);
                for (int64_t i = 0; i < len; ++i) {
                    const char c = buffer[(size_t)i];
                    if (c == '0') { continue; }
                    Note n{ i, len, c };
                    Reduce(n.Num, n.Den);
                    m.Notes.push_back(n);
                }
                for (auto& [index, text] : commands) {
                    Command cmd{ len == 0 ? 0 : (int64_t)index, len == 0 ? 1 : len, std::move(text) };
                    Reduce(cmd.Num, cmd.Den);
                    m.Commands.push_back(std::move(cmd));
                }
                course->Measures.push_back(std::move(m));
                buffer.clear();
                commands.clear();
            };

            for (std::string_view raw : lines) {
                const std::string_view line = Trim(raw);
                const bool is_course = StartsWithKey(line, "COURSE:");
                const bool is_start = StartsWithKey(line, "#START");
                switch (state) {
                case State::Header:
                    if (is_course || is_start) {
                        begin_course({});
                        state = State::CourseHeader;
                        if (is_start) { course->StartLine = std::string(line); state = State::Notes; }
                        else { course->Header.emplace_back(line); }
                    }
                    else {
                        doc.Header.emplace_back(raw.substr(0, raw.size() - (raw.ends_with('\r') ? 1 : 0)));
                    }
                    break;
                case State::AfterEnd:
                    if (is_course || is_start) {
                        begin_course(std::move(pending));
                        pending.clear();
                        if (is_start) { course->StartLine = std::string(line); state = State::Notes; }
                        else { course->Header.emplace_back(line); state = State::CourseHeader; }
                    }
                    else {
                        pending.emplace_back(line);
                    }
                    break;
                case State::CourseHeader:
                    if (is_start) {
                        course->StartLine = std::string(line);
                        state = State::Notes;
                    }
                    else {
                        course->Header.emplace_back(line);
                    }
                    break;
                case State::Notes: {
                    if (StartsWithKey(line, "#END")) {
                        for (auto& [index, text] : commands) { course->Trailing.push_back(std::move(text)); }
                        commands.clear();
                        if (!buffer.empty()) { finish_measure(); }      // ',' の無い最後の小節
                        course->EndLine = std::string(line);
                        state = State::AfterEnd;
                        break;
                    }
                    if (line.empty()) { break; }
                    if (line.front() == '#' || line.starts_with("//") || std::isalpha((unsigned char)line.front())) {
                        commands.emplace_back(buffer.size(), std::string(line));   // 命令・コメント・その他の行はその位置にそのまま残す
                        break;
                    }
                    std::string_view body = line;
                    if (const size_t c = body.find("//"); c != std::string_view::npos) { body = body.substr(0, c); }
                    for (char ch : body) {
                        if (ch == ',') { finish_measure(); }
                        else if (ch != ' ' && ch != '\t') { buffer += ch; }
                    }
                    break;
                }
                }
            }
            if (state == State::Notes && course != nullptr) {
                if (!buffer.empty() || !commands.empty()) { finish_measure(); }
            }
            if (!pending.empty()) {
                // 最後のコースの後ろの行は、ヘッダーとして残す (空行など)
                for (auto& line : pending) {
                    if (!Trim(line).empty()) { doc.Header.push_back(std::move(line)); }
                }
            }
            NormalizeURLHeaders(doc.Header);
            for (auto& c : doc.Courses) { NormalizeURLHeaders(c.Header); }
            return doc;
        }

        // ---------------------------------------------------------------- 書き出し (UTF-8, CRLF)
        std::string Write() const {
            std::string out;
            auto line = [&](std::string_view s) { out.append(s); out.append("\r\n"); };
            auto header = [&](const std::vector<std::string>& lines) {
                size_t begin = 0, end = lines.size();
                while (begin < end && Trim(lines[begin]).empty()) { ++begin; }
                while (end > begin && Trim(lines[end - 1]).empty()) { --end; }
                for (size_t i = begin; i < end; ++i) { line(lines[i]); }
            };
            auto separate = [&] {
                if (!out.empty() && !out.ends_with("\r\n\r\n")) { line(""); }
            };
            header(Header);
            for (const auto& c : Courses) {
                separate();
                header(c.Header);
                separate();
                line(c.StartLine);
                for (const auto& m : c.Measures) { WriteMeasure(m, line); }
                for (const auto& t : c.Trailing) { line(t); }
                line(c.EndLine);
            }
            return out;
        }

        // ---------------------------------------------------------------- ヘッダー
        // 旧名称を読み込み、新名称に統一する。両方ある場合は空でない新名称を優先。
        static void NormalizeURLHeaders(std::vector<std::string>& lines) {
            auto normalize = [&](std::string_view oldKey, std::string_view newKey) {
                const std::string oldPrefix = std::string(oldKey) + ":", newPrefix = std::string(newKey) + ":";
                std::string oldValue, newValue;
                bool found = false;
                for (const auto& line : lines) {
                    const auto trimmed = Trim(line);
                    if (StartsWithKey(trimmed, newPrefix)) {
                        found = true;
                        if (auto v = Trim(trimmed.substr(newPrefix.size())); !v.empty()) newValue = v;
                    } else if (StartsWithKey(trimmed, oldPrefix)) {
                        found = true;
                        if (auto v = Trim(trimmed.substr(oldPrefix.size())); !v.empty()) oldValue = v;
                    }
                }
                if (!found) return;
                bool written = false;
                for (auto it = lines.begin(); it != lines.end();) {
                    if (!StartsWithKey(*it, oldPrefix) && !StartsWithKey(*it, newPrefix)) { ++it; continue; }
                    if (!written) { *it = newPrefix + (newValue.empty() ? oldValue : newValue); written = true; ++it; }
                    else { it = lines.erase(it); }
                }
            };
            normalize("SONGLINK", "SONGURL");
            normalize("MOVIELINK", "MOVIEURL");
        }
        static std::string GetValue(const std::vector<std::string>& lines, std::string_view key) {
            const std::string k = std::string(key) + ":";
            for (const auto& l : lines) {
                if (StartsWithKey(l, k)) { return std::string(Trim(std::string_view(l).substr(Trim(l).data() - l.data() + k.size()))); }
            }
            return {};
        }
        static void SetValue(std::vector<std::string>& lines, std::string_view key, std::string_view value) {
            const std::string k = std::string(key) + ":";
            for (auto& l : lines) {
                if (StartsWithKey(l, k)) { l = k + std::string(value); return; }
            }
            lines.push_back(k + std::string(value));
        }
        std::string CourseName(size_t index) const {
            if (index >= Courses.size()) { return {}; }
            std::string name = GetValue(Courses[index].Header, "COURSE");
            return name.empty() ? std::string("Oni") : name;
        }

        // ---------------------------------------------------------------- 編集
        // 位置 num/den に type の音符を置く ('0' は消す)。同じ位置の音符は置き換える
        static void SetNote(Measure& m, int64_t num, int64_t den, char type) {
            Reduce(num, den);
            auto it = std::ranges::find_if(m.Notes, [&](const Note& n) { return ComparePos(n.Num, n.Den, num, den) >= 0; });
            if (it != m.Notes.end() && ComparePos(it->Num, it->Den, num, den) == 0) {
                if (type == '0') { m.Notes.erase(it); }
                else { it->Type = type; }
                return;
            }
            if (type != '0') { m.Notes.insert(it, Note{ num, den, type }); }
        }
        static const Note* NoteAt(const Measure& m, int64_t num, int64_t den) {
            for (const auto& n : m.Notes) {
                if (ComparePos(n.Num, n.Den, num, den) == 0) { return &n; }
            }
            return nullptr;
        }
        static void AddCommand(Measure& m, int64_t num, int64_t den, std::string text) {
            Reduce(num, den);
            auto it = std::ranges::find_if(m.Commands, [&](const Command& c) { return ComparePos(c.Num, c.Den, num, den) > 0; });
            m.Commands.insert(it, Command{ num, den, std::move(text) });
        }

        // 新しい譜面 (1 コース、空の小節 count 個)
        static Document NewChart(const std::string& title, const std::string& wave, int measures = 16) {
            Document doc;
            doc.Header = { "TITLE:" + title, "SUBTITLE:", "BPM:120", "WAVE:" + wave, "OFFSET:0", "DEMOSTART:0", "SONGVOL:100", "SEVOL:100" };
            Course c;
            c.Header = { "COURSE:Oni", "LEVEL:1", "BALLOON:" };
            c.Measures.resize((size_t)std::max(measures, 1));
            doc.Courses.push_back(std::move(c));
            return doc;
        }

    public:
        // 書き出す音符の 1 行の最大文字数 (',' を含む)。太鼓さん次郎は 1 行 512 文字が限界で、それを超える行で例外を出す。
        // 改行 (CR LF) の分も含めて収まるよう 510 文字までにする
        static constexpr int64_t MaxLineLength = 510;

    private:
        template<class Line>
        static void WriteMeasure(const Measure& m, Line& line) {
            constexpr int64_t maxResolution = 1024 * 1024;
            auto lcm = [](int64_t a, int64_t b) {
                if (a <= 0 || b <= 0 || b > maxResolution || a / std::gcd(a, b) > maxResolution / b) {
                    throw std::length_error("小節の分割数が大きすぎます (上限 1048576)");
                }
                return a / std::gcd(a, b) * b;
            };
            // 細かさ: 音符と小節の途中の命令の位置を表せる最小の分割 (minimal) を、元の細かさ (Resolution) に合わせて細かくする
            // (音符の間隔の見た目を保つ)。ただし 1 行が長くなりすぎる時は、minimal の倍数のうち元の細かさを割り切れて
            // MaxLineLength に収まる一番細かいものにする (音符の位置は変わらない)。それでも収まらない時は行を折り返す
            int64_t minimal = 1;
            for (const auto& n : m.Notes) {
                if (n.Num < 0 || n.Den <= 0 || n.Num >= n.Den) { throw std::invalid_argument("音符の位置が範囲外です"); }
                int64_t a = n.Num, b = n.Den;
                Reduce(a, b);
                minimal = lcm(minimal, b);
            }
            for (const auto& c : m.Commands) {
                if (c.Num < 0 || c.Den <= 0 || c.Num > c.Den) { throw std::invalid_argument("命令の位置が範囲外です"); }
                if (c.Num < c.Den) { int64_t a = c.Num, b = c.Den; Reduce(a, b); minimal = lcm(minimal, b); }
            }
            int64_t den = minimal;
            if (!m.Notes.empty()) {
                const int64_t preferred = lcm(minimal, std::max<int64_t>(m.Resolution, 1));
                if (preferred < MaxLineLength) { den = preferred; }
                else {
                    for (int64_t k = preferred / minimal; k >= 1; --k) {
                        if ((preferred / minimal) % k == 0 && minimal * k < MaxLineLength) { den = minimal * k; break; }
                    }
                }
            }
            if (den > maxResolution) { throw std::length_error("小節の分割数が大きすぎます"); }
            const bool has_notes = !m.Notes.empty();
            std::string notes(has_notes ? (size_t)den : 0, '0');
            for (const auto& n : m.Notes) {
                const int64_t index = n.Num * (den / n.Den);
                if (index >= 0 && index < den) { notes[(size_t)index] = n.Type; }
            }
            // 音符を書いていない小節の途中の命令は、その位置まで '0' で埋める必要がある
            bool mid = false;
            for (const auto& c : m.Commands) { mid = mid || (c.Num > 0 && c.Num < c.Den); }
            if (!has_notes && mid) { notes.assign((size_t)den, '0'); }

            // 音符の行は MaxLineLength 文字 (',' を含む) までで折り返す (太鼓さん次郎は長すぎる行を読めない)
            auto notesLine = [&](std::string_view part, bool last) {
                const size_t limit = (size_t)MaxLineLength - 1;
                while (part.size() > limit) { line(part.substr(0, limit)); part.remove_prefix(limit); }
                if (last) { line(std::string(part) + ","); } else if (!part.empty()) { line(part); }
            };
            size_t written = 0;
            for (const auto& c : m.Commands) {
                const size_t index = c.Num >= c.Den ? notes.size() : (size_t)(c.Num * (den / c.Den));
                if (index > written) {
                    notesLine(std::string_view(notes).substr(written, index - written), false);
                    written = index;
                }
                line(c.Text);
            }
            notesLine(std::string_view(notes).substr(written), true);
        }
    };
}
