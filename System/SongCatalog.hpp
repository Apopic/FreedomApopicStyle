// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    enum class CourseType {
        Null = -1,
        Easy,
        Normal,
        Hard,
        Oni,
        Edit,
        Tower,
        Dan,
        Count
    };

    enum class ExamTypes {
        Null = -1,
        Accuracy,
        Good,
        Ok,
        Bad,
        Score,
        Roll,
        HitNote,
        MaxCombo,
    };

    enum class ExamRange {
        Null = -1,
        More,
        Less,
    };

    struct ExamData {
        ExamTypes ExamType = ExamTypes::Null;
        ExamRange Range = ExamRange::Null;
        double PassVal[2]{ 0.0,0.0 };
    };

    struct CourseData {
        std::vector<uint64_t> Balloons = {};
        size_t Index = 0;
        uint64_t AddScore = 0;
        uint64_t Level = 0;
        bool IsPlayable = false;
        uint32_t NoteCount = 0;     // コンボ数 (音符 1～4 の数。分岐譜面は普通譜面)
    };

    // キャッシュ保存用: パスは UTF-8 文字列として扱う
    static std::string PathToU8(const fs::path& path);
    static fs::path U8ToPath(const std::string& str);

    class ChartData {
    public:

        ChartData() = default;
        explicit ChartData(const fs::path& path) { this->Load(path); }

        struct StrlenData {
            size_t Playing = 0;
            size_t Result = 0;
        };

        std::string Title = "";
        std::string Subtitle = "";
        std::string Genre = "";             // GENRE: ヘッダ
        std::string FolderGenre = "";       // 所属するジャンルフォルダ (genre.ini) の名前 (走査時に設定)
        double MinBPM = 120.0;              // BPM の範囲 (BPM: と #BPMCHANGE から求める)
        double MaxBPM = 120.0;
        bool TitleDisplay = true;
        bool SubtitleDisplay = true;
        StrlenData TitleStrlen;
        StrlenData SubtitleStrlen;
        double BPM = 120.0;
        double SongOffset = 0.0;
        double MovieOffset = 0.0;
        double DemoStart = 0.0;
        float SongVolume = 100.0;
        float SeVolume = 100.0;
        fs::path SongPath = "";
        fs::path MoviePath = "";
        fs::path ChartPath = "";
        std::string SongURL = "";
        std::string MovieURL = "";
        CourseData Courses[(size_t)CourseType::Count];

        std::vector<ExamData> ExamDatas;     // 段位の全体条件 (#START より前、または最初の #NEXTSONG より前の EXAMn)
        bool IsDan = false;

        // 段位の課題曲 (#NEXTSONG タイトル,サブタイトル,ジャンル,音源,SCOREINIT,SCOREDIFF,レベル,コース)
        struct DanSong {
            std::string Title;
            std::string Subtitle;
            std::string Genre;
            fs::path WavePath;
            uint64_t ScoreInit = 0;
            uint64_t ScoreDiff = 0;
            uint64_t Level = 0;
            std::string Course;
            size_t StartLine = 0;           // #NEXTSONG の行番号
            size_t TitleStrlen = 0;         // 演奏画面用 (measure 時のみ)
            size_t SubtitleStrlen = 0;
            std::vector<ExamData> Exams;    // この曲だけの条件 (#NEXTSONG の後に書かれた EXAMn)
        };
        std::vector<DanSong> DanSongs;

        // "種類,赤合格,金合格,範囲(m/l)" を読む
        static bool ParseExam(std::string_view data, ExamData& exam) {
            auto sp = split(data, ',');
            if (sp.size() < 3) { return false; }
            for (auto& v : sp) { v = strtrim(v); }
            static constexpr std::pair<std::string_view, ExamTypes> types[] = {
                { "g", ExamTypes::Accuracy }, { "jp", ExamTypes::Good }, { "jg", ExamTypes::Ok }, { "jb", ExamTypes::Bad },
                { "s", ExamTypes::Score }, { "r", ExamTypes::Roll }, { "h", ExamTypes::HitNote }, { "c", ExamTypes::MaxCombo },
            };
            auto type = std::ranges::find(types, sp[0], &std::pair<std::string_view, ExamTypes>::first);
            if (type == std::end(types)) { return false; }
            exam.ExamType = type->second;
            exam.PassVal[0] = svtov<double>(sp[1]);
            exam.PassVal[1] = svtov<double>(sp[2]);
            exam.Range = (sp.size() >= 4 && sp[3] == "l") ? ExamRange::Less : ExamRange::More;
            return true;
        }

        // preloaded: 読み込み済みのテキスト (LoadingProc で 2 回読まないため)
        // measure : 演奏・リザルト用の文字幅を測る (DxLib のフォントを使うのでメインスレッド専用)
        std::string LoadError;      // Load が false を返した理由
        bool Load(const fs::path& path, const TextLines* preloaded = nullptr, bool measure = false, const _Skin* skin = nullptr) {
            // Font metrics are only needed by the main-thread loading scene.
            if (measure && !skin) { LoadError = "Font metrics unavailable"; return false; }
            TextLines owned;
            const TextLines* text = preloaded;
            LoadError.clear();
            if (text == nullptr) {
                if (!owned.Load(path, !measure)) {
                    LoadError = "ファイルを開けません (見つからない・使用中・アクセスできない)";
                    return false;
                }
                text = &owned;
            }
            const std::vector<std::string_view>& lines = text->lines;
            ChartPath = path;
            const fs::path dir = path.parent_path();

            size_t index = 0;
            uint64_t level = 0;
            uint64_t addscore = 0;
            std::vector<uint64_t> balloon;
            CourseType course = CourseType::Null;

            // 検索用: コンボ数と BPM の範囲
            bool incourse = false;
            size_t countcourse = 0;
            int branch = 0;                 // 0: 分岐外, 1: 普通, 2: 玄人, 3: 達人
            double minbpm = 0.0, maxbpm = 0.0;
            bool bpmchanged = false;

            bool hasSongURL = false, hasMovieURL = false;
            try {
                for (size_t i = 0; i < lines.size(); ++i) {
                    const std::string_view line = lines[i];
                    if (line.empty()) { continue; }

                    // 譜面データ行は検索用にコンボ数 (音符 1～4) だけ数える。分岐譜面は演奏時の総ノーツ数と同じく普通譜面を数える
                    if (incourse && branch <= 1) {
                        std::string_view data = strtrim(line);
                        if (!data.empty() && ((data.front() >= '0' && data.front() <= '9') || data.front() == ',')) {
                            if (auto comment = data.find("//"); comment != std::string_view::npos) {
                                data = data.substr(0, comment);
                            }
                            for (char ch : data) {
                                if (ch >= '1' && ch <= '4') { ++Courses[countcourse].NoteCount; }
                            }
                            continue;
                        }
                    }

                    // ヘッダはすべて行頭一致なので、譜面データ行 (数字・カンマ) やコメント・空白始まりは即スキップ
                    const char c = line.front();
                    if ((c >= '0' && c <= '9') || c == ',' || c == ' ' || c == '\t' || c == '/') { continue; }

                    if (c == '#') {
                        const std::string_view command = line.substr(0, line.find_first_of(" \t"));
                        if (command == "#BPMCHANGE") {
                            const double bpm = std::abs(svtov<double>(strtrim(line.substr(command.size()))));
                            if (bpm > 0) {
                                minbpm = bpmchanged ? std::min(minbpm, bpm) : bpm;
                                maxbpm = bpmchanged ? std::max(maxbpm, bpm) : bpm;
                                bpmchanged = true;
                            }
                        }
                        else if (command == "#BRANCHSTART" || command == "#BRANCHEND") { branch = 0; }
                        else if (command == "#N") { branch = 1; }
                        else if (command == "#E") { branch = 2; }
                        else if (command == "#M") { branch = 3; }
                        else if (command == "#END") { incourse = false; }
                        Exsubstr(line, "#START", [&](std::string_view data) {
                            if (course == CourseType::Null) {
                                course = CourseType::Oni;
                            }
                            IsDan = (course == CourseType::Dan);
                            Courses[(size_t)course].Index = index;
                            Courses[(size_t)course].Level = level;
                            Courses[(size_t)course].AddScore = addscore;
                            Courses[(size_t)course].Balloons = std::move(balloon);
                            Courses[(size_t)course].IsPlayable = true;
                            Courses[(size_t)course].NoteCount = 0;
                            balloon = std::vector<uint64_t>();
                            incourse = true;
                            countcourse = (size_t)course;
                            branch = 0;
                            });
                        if (IsDan) {
                            Exsubstr(line, "#NEXTSONG", [&](std::string_view data) {
                                auto sp = split(data, ',');
                                if (sp.size() < 4) { return; }
                                for (auto& v : sp) { v = strtrim(v); }
                                DanSong song;
                                song.Title = sp[0];
                                song.Subtitle = sp[1];
                                song.Genre = sp[2];
                                song.WavePath = dir / sp[3];
                                if (sp.size() > 4) { song.ScoreInit = svtov<uint64_t>(sp[4]); }
                                if (sp.size() > 5) { song.ScoreDiff = svtov<uint64_t>(sp[5]); }
                                if (sp.size() > 6) { song.Level = svtov<uint64_t>(sp[6]); }
                                if (sp.size() > 7) { song.Course = sp[7]; }
                                song.StartLine = i;
                                if (measure) {
                                    song.TitleStrlen = GetStrlen(song.Title, skin->Base->Playing.Font.Title.Handle);
                                    song.SubtitleStrlen = GetStrlen(song.Subtitle, skin->Base->Playing.Font.SubTitle.Handle);
                                }
                                DanSongs.push_back(std::move(song));
                                });
                        }
                        continue;
                    }

                    Exsubstr(line, "TITLE:", [&](std::string_view data) {
                        if (data.starts_with("--")) {
                            data.remove_prefix(2);
                            TitleDisplay = false;
                        }
                        Title = data;
                        if (measure) {
                            TitleStrlen.Playing = GetStrlen(data, skin->Base->Playing.Font.Title.Handle);
                            TitleStrlen.Result = GetStrlen(data, skin->Base->Result.Font.Title.Handle);
                        }
                        });
                    Exsubstr(line, "GENRE:", [&](std::string_view data) {
                        Genre = data;
                        });
                    Exsubstr(line, "SUBTITLE:", [&](std::string_view data) {
                        if (data.starts_with("--")) {
                            data.remove_prefix(2);
                            SubtitleDisplay = false;
                        }
                        Subtitle = data;
                        if (measure) {
                            SubtitleStrlen.Playing = GetStrlen(data, skin->Base->Playing.Font.SubTitle.Handle);
                            SubtitleStrlen.Result = GetStrlen(data, skin->Base->Result.Font.SubTitle.Handle);
                        }
                        });
                    Exsubstr(line, "BPM:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        const double value = svtov<double>(data);
                        // 0・非数は時刻が無限大になるため無視する (既定の 120 のまま)
                        if (value != 0.0 && std::isfinite(value)) { BPM = value; }
                        });
                    Exsubstr(line, "OFFSET:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        SongOffset = svtov<double>(data);
                        });
                    Exsubstr(line, "MOVIEOFFSET:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        MovieOffset = svtov<double>(data);
                        });
                    Exsubstr(line, "DEMOSTART:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        DemoStart = svtov<double>(data);
                        });
                    Exsubstr(line, "SONGVOL:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        SongVolume = svtov<float>(data);
                        });
                    Exsubstr(line, "SEVOL:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        SeVolume = svtov<float>(data);
                        });
                    Exsubstr(line, "WAVE:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        SongPath = dir / data;
                        });
                    Exsubstr(line, "BGMOVIE:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        MoviePath = dir / data;
                        });
                    Exsubstr(line, "SONGURL:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        SongURL = data; hasSongURL = true;
                        });
                    Exsubstr(line, "MOVIEURL:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        MovieURL = data; hasMovieURL = true;
                        });
                    Exsubstr(line, "SONGLINK:", [&](std::string_view data) {
                        if (!hasSongURL && !data.empty()) { SongURL = data; }
                        });
                    Exsubstr(line, "MOVIELINK:", [&](std::string_view data) {
                        if (!hasMovieURL && !data.empty()) { MovieURL = data; }
                        });
                    Exsubstr(line, "COURSE:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        index = i;
                        course = CourseType::Null;
                        std::string str = ToLower(data);
                        if (str == "easy") {
                            course = CourseType::Easy;
                        }
                        else if (str == "normal") {
                            course = CourseType::Normal;
                        }
                        else if (str == "hard") {
                            course = CourseType::Hard;
                        }
                        else if (str == "oni") {
                            course = CourseType::Oni;
                        }
                        else if (str == "edit") {
                            course = CourseType::Edit;
                        }
                        else if (str == "tower") {
                            course = CourseType::Tower;
                        }
                        else if (str == "dan") {
                            course = CourseType::Dan;
                        }
                        if (course != CourseType::Null) {
                            return;
                        }
                        // 数値指定は範囲外だと Courses[] の範囲外アクセスになるため検証する
                        int num = svtov<int>(data);
                        if (num >= 0 && num < (int)CourseType::Count) {
                            course = (CourseType)num;
                        }
                        });
                    Exsubstr(line, "LEVEL:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        level = svtov<uint64_t>(data);
                        });
                    Exsubstr(line, "SCOREINIT:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        addscore = svtov<uint64_t>(data);
                        });
                    Exsubstr(line, "BALLOON:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        for (auto&& s : split(data, ',')) {
                            balloon.push_back(svtov<uint64_t>(s));
                        }
                        });
                    // EXAMn: (番号は問わない)。#NEXTSONG の後に書かれたものはその曲だけの条件として扱う
                    if (line.starts_with("EXAM")) {
                        const size_t colon = line.find(':');
                        if (colon != std::string_view::npos && colon > 4
                            && std::ranges::all_of(line.substr(4, colon - 4), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
                            ExamData exam;
                            if (ParseExam(strtrim(line.substr(colon + 1)), exam)) {
                                (IsDan && !DanSongs.empty() ? DanSongs.back().Exams : ExamDatas).push_back(exam);
                            }
                        }
                    }
                }
            }
            catch (const std::exception& e) {
                LoadError = "ヘッダーを読めません: " + DescribeException(e);
                return false;
            }
            catch (...) {
                LoadError = "ヘッダーを読めません (不明なエラー)";
                return false;
            }
            const double basebpm = std::abs(BPM);
            MinBPM = bpmchanged ? std::min(basebpm, minbpm) : basebpm;
            MaxBPM = bpmchanged ? std::max(basebpm, maxbpm) : basebpm;
            return true;
        }

        // ---- 譜面キャッシュ用の入出力 (選曲画面で必要な情報のみ。文字幅は演奏開始時に測る) ----
        json ToCache() const {
            json courses = json::array();
            for (const auto& course : Courses) {
                courses.push_back({ course.Balloons, course.Index, course.AddScore, course.Level, course.IsPlayable, course.NoteCount });
            }
            auto ExamsToJson = [](const std::vector<ExamData>& list) {
                json arr = json::array();
                for (const auto& exam : list) {
                    arr.push_back({ (int)exam.ExamType, (int)exam.Range, exam.PassVal[0], exam.PassVal[1] });
                }
                return arr;
            };
            json exams = ExamsToJson(ExamDatas);
            json dansongs = json::array();
            for (const auto& song : DanSongs) {
                dansongs.push_back({
                    { "t", song.Title }, { "st", song.Subtitle }, { "g", song.Genre }, { "w", PathToU8(song.WavePath) },
                    { "si", song.ScoreInit }, { "sd", song.ScoreDiff }, { "l", song.Level }, { "c", song.Course },
                    { "line", song.StartLine }, { "ex", ExamsToJson(song.Exams) },
                });
            }
            return {
                { "t", Title }, { "st", Subtitle }, { "td", TitleDisplay }, { "sd", SubtitleDisplay },
                { "bpm", BPM }, { "of", SongOffset }, { "mo", MovieOffset }, { "ds", DemoStart },
                { "sv", SongVolume }, { "ev", SeVolume },
                { "wave", PathToU8(SongPath) }, { "movie", PathToU8(MoviePath) },
                { "sl", SongURL }, { "ml", MovieURL }, { "dan", IsDan },
                { "genre", Genre }, { "minb", MinBPM }, { "maxb", MaxBPM },
                { "c", std::move(courses) }, { "ex", std::move(exams) },
                { "dans", std::move(dansongs) },
            };
        }
        bool FromCache(const json& j) {
            try {
                Title = j.at("t").get<std::string>();
                Subtitle = j.at("st").get<std::string>();
                TitleDisplay = j.at("td").get<bool>();
                SubtitleDisplay = j.at("sd").get<bool>();
                BPM = j.at("bpm").get<double>();
                SongOffset = j.at("of").get<double>();
                MovieOffset = j.at("mo").get<double>();
                DemoStart = j.at("ds").get<double>();
                SongVolume = j.at("sv").get<float>();
                SeVolume = j.at("ev").get<float>();
                SongPath = U8ToPath(j.at("wave").get<std::string>());
                MoviePath = U8ToPath(j.at("movie").get<std::string>());
                SongURL = j.at("sl").get<std::string>();
                MovieURL = j.at("ml").get<std::string>();
                IsDan = j.at("dan").get<bool>();
                Genre = j.at("genre").get<std::string>();
                MinBPM = j.at("minb").get<double>();
                MaxBPM = j.at("maxb").get<double>();
                const json& courses = j.at("c");
                for (size_t i = 0; i < (size_t)CourseType::Count && i < courses.size(); ++i) {
                    const json& c = courses[i];
                    Courses[i].Balloons = c.at(0).get<std::vector<uint64_t>>();
                    Courses[i].Index = c.at(1).get<size_t>();
                    Courses[i].AddScore = c.at(2).get<uint64_t>();
                    Courses[i].Level = c.at(3).get<uint64_t>();
                    Courses[i].IsPlayable = c.at(4).get<bool>();
                    Courses[i].NoteCount = c.at(5).get<uint32_t>();
                }
                auto ExamsFromJson = [](const json& arr) {
                    std::vector<ExamData> list;
                    for (const json& e : arr) {
                        ExamData exam;
                        exam.ExamType = (ExamTypes)e.at(0).get<int>();
                        exam.Range = (ExamRange)e.at(1).get<int>();
                        exam.PassVal[0] = e.at(2).get<double>();
                        exam.PassVal[1] = e.at(3).get<double>();
                        list.push_back(exam);
                    }
                    return list;
                };
                ExamDatas = ExamsFromJson(j.at("ex"));
                DanSongs.clear();
                for (const json& d : j.at("dans")) {
                    DanSong song;
                    song.Title = d.at("t").get<std::string>();
                    song.Subtitle = d.at("st").get<std::string>();
                    song.Genre = d.at("g").get<std::string>();
                    song.WavePath = U8ToPath(d.at("w").get<std::string>());
                    song.ScoreInit = d.at("si").get<uint64_t>();
                    song.ScoreDiff = d.at("sd").get<uint64_t>();
                    song.Level = d.at("l").get<uint64_t>();
                    song.Course = d.at("c").get<std::string>();
                    song.StartLine = d.at("line").get<size_t>();
                    song.Exams = ExamsFromJson(d.at("ex"));
                    DanSongs.push_back(std::move(song));
                }
                return true;
            }
            catch (const json::exception&) {
                return false;
            }
        }
    };

    struct BoxData;
    struct GenreData {
        GenreData() {}
        GenreData(const fs::path& path) { this->Load(path); }

        bool IsOpen = false;
        std::string FilePath = "";
        std::string Name = "";
        std::string Caption = "";
        Color3<int> GenreColor{};
        Color3<int> FontColor{};
        Color3<int> FontEdgeColor{};

        std::vector<std::unique_ptr<BoxData>> Datas;
        // 通常譜面 [0] / 段位譜面 [1] を含むか (-1: 未計算)。中身は EnumChart で作り直すので、一度計算すれば変わらない
        mutable int8_t HasChartCache[2] = { -1, -1 };

        void Load(const fs::path& path) {

            FilePath = path.string();
            TextLines text;
            text.Load(path);

            auto ColorCodeParse = [&](std::string_view data) -> Color3<int> {
                if (data.size() < 2) { return {}; }
                int color = 0;
                std::from_chars(data.data() + 1, data.data() + data.size(), color, 16);
                int r = 0, g = 0, b = 0;
                GetColor2(color, &r, &g, &b);
                return { (r - 128) * 2, (g - 128) * 2, (b - 128) * 2 };
                };

            for (std::string_view line : text.lines) {
                Exsubstr(line, "GenreName=", [&](std::string_view data) {
                    Name = data;
                    });
                Exsubstr(line, "GenreCaption=", [&](std::string_view data) {
                    Caption = data;
                    });
                Exsubstr(line, "GenreColor=", [&](std::string_view data) {
                    GenreColor = ColorCodeParse(data);
                    });
                Exsubstr(line, "FontColor=", [&](std::string_view data) {
                    FontColor = ColorCodeParse(data);
                    });
                Exsubstr(line, "FontEdgeColor=", [&](std::string_view data) {
                    FontEdgeColor = ColorCodeParse(data);
                    });
            }
        }

    };

    struct BoxData {

        BoxData() = default;
        explicit BoxData(ChartData* ptr) : m_Chart(ptr) {}
        explicit BoxData(GenreData* ptr) : m_Genre(ptr) {}
        ~BoxData() {
            delete m_Chart;
            delete m_Genre;
        }
        // 生ポインタを所有しているため、コピーすると二重解放になる
        BoxData(const BoxData&) = delete;
        BoxData& operator=(const BoxData&) = delete;

        bool IsGenre() const {
            return m_Genre != nullptr;
        };

        Color3<int> GenreColor{};
        Color3<int> FontColor{};
        Color3<int> FontEdgeColor{};

        ChartData* GetChart() const {
            return m_Chart;
        };
        GenreData* GetGenre() const {
            return m_Genre;
        };

        void SetBoxColor() {
            if (m_Genre == nullptr) {
                return;
            }
            GenreColor = m_Genre->GenreColor;
            FontColor = m_Genre->FontColor;
            FontEdgeColor = m_Genre->FontEdgeColor;
        };
        void SetBoxColor(const GenreData& data) {
            GenreColor = data.GenreColor;
            FontColor = data.FontColor;
            FontEdgeColor = data.FontEdgeColor;
        };

    private:

        ChartData* m_Chart = nullptr;
        GenreData* m_Genre = nullptr;
    };

    std::vector<std::unique_ptr<BoxData>> TempBoxDatas;
    std::vector<BoxData*> BoxDatas;

    // ジャンル (サブフォルダを含む) に通常譜面 / 段位譜面があるか
    static bool GenreHasCharts(const GenreData& genre, bool dan);

    int BoxDataIndex = 0;
    libarrier::Timer DemoSongPlayBlank;
    double DemoSongPlayBlankTime() const;

    struct ScoreData {
        uint64_t Score = 0;
        int Crown = 0;
        uint64_t Good = 0, Ok = 0, Bad = 0, Roll = 0, MaxCombo = 0;
        double Accuracy = 0;
        bool HasResult = false;
    };

    SoundData DemoSong = SoundData();
    bool DemoLoadFailed = false;
    void ResetDemoSong();
    void UpdateDemoSong(const ChartData& selected);

    size_t CourseIndex = (size_t)CourseType::Easy;
    bool IsCourseSelect = false;

    // ---- 譜面キャッシュ ----
    // 譜面ファイルのサイズと更新日時が一致すればパースを省略する。形式を変えたら ChartCacheVersion を上げること。
    static constexpr int ChartCacheVersion = 6;

    struct ChartScanItem {
        ChartData* Chart = nullptr;
        std::string FolderGenre;
        uintmax_t Size = 0;
        int64_t WriteTime = 0;
    };

    // ディレクトリを 1 回だけ列挙し、genre.ini があればジャンルとしてまとめる
    void ScanChartDirectory(std::vector<std::unique_ptr<BoxData>>& out, const fs::path& dirpath, const GenreData* genre, std::vector<ChartScanItem>& items);
    void ScanChartEntries(std::vector<std::unique_ptr<BoxData>>& out, const std::vector<fs::directory_entry>& entries, const GenreData* genre, std::vector<ChartScanItem>& items);

    // 譜面フォルダの設定値 (UTF-8) を実際のパスにする。相対パスは実行ファイルのフォルダから
    static fs::path ResolveSongDirectory(const std::string& dir);
    // 実行ファイルのフォルダからの相対パス (別ドライブなど相対にできなければ空)
    static std::string ToRelativeSongDirectory(const fs::path& absolute);

    static std::string CatalogBoxKey(const BoxData* box);
    catalog::Snapshot ChartWatchBaseline;
    std::optional<catalog::Snapshot> ChartWatchCandidate;
    std::future<catalog::Snapshot> ChartWatchTask;
    uint64_t ChartWatchGeneration = 0, ChartWatchTaskGeneration = 0;
    bool ChartReloadPending = false;
    std::chrono::steady_clock::time_point ChartWatchLast{};
    void EnumChart(const std::vector<std::string>& dir);
    void ChartWatchUpdate();
    skincatalog::Watch SkinWatch;
    std::future<catalog::Snapshot> SkinWatchTask;
    fs::path SkinWatchRoot;
    std::chrono::steady_clock::time_point SkinWatchLast{};
    bool SkinReloadSafe() const;
    void ReloadSkinAtSafePoint();
    void FinishSkinReload();
    void SkinWatchUpdate();
    // ===================================================================== 譜面検索・お気に入り
    // 検索は空白区切りの条件をすべて満たす譜面を表示する (AND)。
    //   単語          : タイトル・サブタイトル・ジャンルのどれかに含む
    //   t: s: g:      : タイトル / サブタイトル / ジャンルに含む
    //   c:            : コース (easy normal hard oni edit(ura) tower dan / かんたん ふつう むずかしい おに 裏 / 0～6)
    //   lv:           : レベル (lv:8 でレベル 8 の譜面)
    //   数値は "8"、"8-10"、">=8"、"<=8"、">8"、"<8"、"8+" で指定する。fav で お気に入りのみ
    // c: を指定すると lv: と combo: はそのコースで判定し、指定しなければどれかのコースが満たせばよい。

    struct SearchRange {
        bool Active = false;
        double Min = -std::numeric_limits<double>::infinity();
        double Max = std::numeric_limits<double>::infinity();
        bool Contains(double v) const { return v >= Min && v <= Max; }
    };

    struct SearchQuery {
        std::vector<std::string> Words, Title, Subtitle, Genre;
        int Course = -1;
        SearchRange Level;              // lv:8 (レベルがちょうど 8)
        bool FavoriteOnly = false;
        std::string ExactText;          // タイトルかサブタイトルが完全に一致 (正規化済み)
        std::vector<std::string> GenreFolders;  // このジャンルフォルダ (と中のジャンル) の譜面だけ (小文字)

        bool Empty() const {
            return Words.empty() && Title.empty() && Subtitle.empty() && Genre.empty() && Course < 0
                && !Level.Active && !FavoriteOnly
                && ExactText.empty() && GenreFolders.empty();
        }
    };

    std::unordered_set<std::string> Favorites;
    bool FavoritesLoaded = false;
    std::string SelectMessage;              // 選曲画面に一時的に出すメッセージ
    std::chrono::steady_clock::time_point SelectMessageUntil{};

    void ShowSelectMessage(std::string message);

    // 全角英数字・記号と全角空白を半角にし、英字を小文字にそろえる (IME で入力しても検索できるように)
    static std::string NormalizeSearchText(std::string_view in);

    static std::optional<double> ParseSearchNumber(std::string_view v);
    static int ParseSearchCourse(std::string_view v);

    static SearchQuery ParseSearch(std::string_view text);

    bool MatchChart(const ChartData& chart, const SearchQuery& q) const;

    // ---- お気に入り (favorites.json に保存。ゲームのフォルダ内の譜面は相対パスで記録し、フォルダを移動しても残るようにする) ----
    static std::string FavoriteKey(const fs::path& chartpath);
    void LoadFavorites();
    void SaveFavorites();
    bool IsFavorite(const ChartData& chart) const;
    // ジャンル内の譜面を (サブジャンルも含めて) 集める
    void CollectGenreCharts(const GenreData& genre, std::vector<const ChartData*>& out) const;
    // 譜面ならその譜面を、ジャンルなら中の譜面をまとめて登録・解除する (全部登録済みなら解除、それ以外は全部登録)
    void ToggleFavorite(BoxData* box);
    void ToggleFavoriteSelected();
    void ToggleFavoriteOnly();
    // 一覧を作り直す。条件 (検索・お気に入りのみ) があれば、ジャンルの開閉に関係なく該当する譜面を一覧にする
    void BoxDatasUpdate();

    // 選曲画面の上部: 検索条件・件数・お知らせ / 検索欄を開いている間は書き方の例
    void DrawSearchStatus(const FontData& font);
    void DrawFavoriteMark(const GraphData& box, const FontData& font, float y);

    std::vector<uint8_t> FileToMem(const fs::path& path);
    void MemToFile(const fs::path& path, const std::vector<uint8_t>& datas);
#ifndef __ANDROID__
    void FileImport();
#endif
#ifndef __ANDROID__
    // ---- SONGURL / MOVIEURL のダウンロード ----
    // cmd / PowerShell を経由せず yt-dlp を CreateProcessW で直接起動する。
    // シェルが介在しないため、URL に記号が含まれていてもコマンドとして解釈されることはない。

    // CommandLineToArgvW の規則に従って引数をクォートする
    static std::wstring QuoteArg(const std::wstring& arg);

    // プロセスの終了を待つ間もメッセージ処理と画面更新を続け、ウィンドウが「応答なし」にならないようにする
    bool RunProcess(const std::vector<std::wstring>& args, const fs::path& workdir, const char* status);

    // ---- 外部ツール (ffmpeg / yt-dlp) ----
    // ゲームには同梱せず、必要になった時にプレイヤーの同意を得て配布元から導入する (Library/ToolInstaller.hpp)
    enum class ToolKind { Ffmpeg, YtDlp };
    static const char* ToolName(ToolKind kind);
    // ゲームのフォルダ → PATH の順に探す (見つからなければ空)
    static fs::path FindTool(ToolKind kind);
    // 導入の説明 (配布元とライセンス) を見せて同意を得る
    static bool ConfirmToolInstall(ToolKind kind, const char* reason);
    // 導入を実行する (終わるまで進み具合を表示する。Esc で中止)
    bool InstallTool(ToolKind kind);
    // ツールが無ければ導入を提案する (導入済み・導入できたら true)
    bool EnsureTool(ToolKind kind, const char* reason);

    void MediaDownload(const std::string& link, const fs::path& path, const char* confirm, const char* failed, const wchar_t* ext, const std::vector<std::wstring>& options);
#endif
#ifdef __ANDROID__
    // Android: MainActivity.DownloadMedia (youtubedl-android) を別スレッドで呼び、終わるまで進み具合を表示する
    bool AndroidDownloadMedia(const std::string& link, const fs::path& path, bool audio);
#endif
    void SongDownload(const std::string& link, const fs::path& path);
    void MovieDownload(const std::string& link, const fs::path& path);
#endif // FAS_GAME_MEMBER_DECLARATIONS
