#include "../Game.hpp"

double Game::ScoreRateCalc(double judge, double basis) {
    const double c = 0.9;
    const double b = basis;
    const double m = 10;
    const double d = std::pow(b, std::pow(m, -1 / c));
    return 1 / std::pow(std::log(judge * (b - d) / b + d) / std::log(b), c);
}

void Game::SetSpeed() {

    double SongSpeed = Editor.Testing ? Editor.PreviewSpeed : !IsMulti ? Config.SongSpeed : Online.SongSpeed;

#ifndef __ANDROID__
    if (!Chart.OriginalData.MoviePath.empty() && !Chart.Movie.IsLoaded()) {
        Chart.Movie.Load(Chart.OriginalData.MoviePath.string(), SongSpeed);
    }
#endif

    Chart.SongData.SetVolume(Chart.OriginalData.SongVolume * (Config.SongVolume / 100));
    int freq = std::max(1,numeric::Cast<int>(Chart.SongData.Frequency * SongSpeed));
    Chart.SongData.SetFrequency(freq);
    Chart.SongSpeed = (double)freq / Chart.SongData.Frequency;
}

uint64_t Game::AllNoteCounter(std::vector<std::string_view>& lines) {

    bool StartFlag = false;
    bool BarlineLoading = false;

    size_t BarlineNoteCount = 0;
    uint64_t count = 0;

    for (size_t i = 0; i < lines.size(); ++i) {

        Exsubstr(lines[i], "#START", [&](std::string_view data) {
            StartFlag = true;
            });
        Exsubstr(lines[i], "#END", [&](std::string_view data) {
            StartFlag = false;
            });

        if (lines[i].find("#") != std::string_view::npos) { continue; }
        if (!StartFlag) { continue; }

        if (!BarlineLoading) {
            BarlineLoading = true;
            for (size_t j = i; j < lines.size(); ++j) {
                if (lines[j].find("#") != std::string_view::npos) { continue; }
                for (size_t k = 0, strsize = lines[j].size(); k < strsize; ++k) {
                    if (lines[j][k] == ',') {
                        goto BARLINEREADEND;
                    }
                    else if (lines[j][k] >= '0' && lines[j][k] <= '9') {
                        ++BarlineNoteCount;
                    }
                }
            }
        BARLINEREADEND:;
        }

        for (size_t j = 0, strsize = lines[i].size(); j < strsize; ++j) {
            bool ChartFlag = (lines[i][j] >= '0' && lines[i][j] <= '9');
            bool EndFlag = lines[i][j] == ',';
            bool EmptyFlag = BarlineNoteCount == 0;
            if (ChartFlag || EndFlag || EmptyFlag) {
                if (EndFlag && !EmptyFlag) {
                    BarlineLoading = false;
                    BarlineNoteCount = 0;
                    break;
                }
                if (lines[i][j] >= '1' && lines[i][j] <= '4') {
                    ++count;
                }
            }
        }
	}

	return count;
}

void Game::LoadingDraw() {
    if (PlayingBlackout.HideLoading) {
        gfx::SetBlend(gfx::Blend::None);
        gfx::Box((float)(0), (float)(0), (float)(Skin.Info.Resolution.X), (float)(Skin.Info.Resolution.Y), gfx::Rgb(0, 0, 0), true);
        return;
    }
    txt::DrawFormat((float)(0), (float)(8), gfx::Rgb(255, 255, 255), "ChartLoading...");
}

double Game::JiroMeasureValue(std::string_view arg) {
    const std::string text(arg);
    const char* p = text.c_str();
    char* end = nullptr;
    const double num = std::strtod(p, &end);
    if (end == p || *end != '/') { return 1.0; }
    const char* q = end + 1;
    while (*q == ' ' || *q == '	') { ++q; }
    char* end2 = nullptr;
    const unsigned long den = std::strtoul(q, &end2, 10);      // %u と同じく符号も受け付ける (負は大きな値になる)
    if (end2 == q || (unsigned int)den == 0) { return 1.0; }
    const double ratio = num / (double)(unsigned int)den;
    return std::isfinite(ratio) && std::abs(ratio) <= 1000000.0 ? ratio : 1.0;
}

bool Game::RequiresJiroTiming(const std::vector<std::string_view>& lines) {
    for (auto line : lines) {
        line = strtrim(line.substr(0, line.find("//")));
        if (line == "#HBSCROLL" || line == "#BMSCROLL") { return true; }
    }
    return false;
}

bool Game::AskTemporaryJiroMode() {
#ifndef __ANDROID__
    const bool yes=MessageBox(((HWND)sys::NativeWindow()),"HBSCROLL / BMSCROLL を含む譜面です。\n今回のプレイだけ JiroChartMode を ON にしますか？\n終了後は元の設定へ戻ります。","JiroChartMode の確認",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON1)==IDYES;
    Input::GetInstance().ResyncKeys();return yes;
#else
    bool previous=true;
    while(sys::PumpEvents()) {
        gfx::Clear();
        txt::Draw(txt::DefaultFont(), (float)(40), (float)(80), "HBSCROLL / BMSCROLL: JiroChartMode を今回のプレイだけ ON にしますか？", gfx::Rgb(255, 255, 255));
        gfx::Box((float)(40), (float)(150), (float)(340), (float)(220), gfx::Rgb(60, 100, 150), true);
        gfx::Box((float)(380), (float)(150), (float)(680), (float)(220), gfx::Rgb(70, 70, 70), true);
        txt::Draw(txt::DefaultFont(), (float)(65), (float)(175), "はい (Enter)", gfx::Rgb(255, 255, 255));
        txt::Draw(txt::DefaultFont(), (float)(405), (float)(175), "いいえ (Esc)", gfx::Rgb(255, 255, 255));
        gfx::Present();
        int x=0,y=0;sys::MousePos(x, y);
        bool click=(sys::MouseButtons()&sys::MouseLeft)!=0;
#ifdef __ANDROID__
        if(GetTouchInputNum()>0){int touchId=0,device=0;GetTouchInput(0,&x,&y,&touchId,&device);click=true;}
#endif
        const bool yes=sys::KeyDown(sys::Key::Return)!=0,no=sys::KeyDown(sys::Key::Escape)!=0;
        const bool active=click||yes||no;
        if(active&&!previous){if(yes)return true;if(no)return false;if(click&&y>=150&&y<=220){if(x>=40&&x<=340)return true;if(x>=380&&x<=680)return false;}}
        previous=active;WaitTimer(16);
    }
    EndFlag=true;return false;
#endif
}

void Game::LoadingProc() {

    // 譜面ファイルは 1 回だけ読み込み、ヘッダ解析とノーツ解析で同じテキストを使う
    // (段位道場の 2 曲目以降は、最初に読み込んだテキストとヘッダをそのまま使う)
    ChartData LoadData;
    std::shared_ptr<TextLines> textptr = std::make_shared<TextLines>();
    fs::path chartpath;
    bool loaded = false;
    const bool OnlineGuest = IsMulti && !IsHost();
    const bool DanContinue = Dan.Active;

    if (DanContinue) {
        textptr = Dan.Text;
        LoadData = Dan.Course;
        chartpath = LoadData.ChartPath;
        loaded = textptr != nullptr;
    }
    else if (Editor.Testing) {
        // 譜面制作モードのテストプレイ: 編集中の譜面をメモリから読む (音源は譜面のフォルダから)
        chartpath = Editor.Path;
        textptr->LoadFromMemory(Editor.TestText);
        loaded = LoadData.Load(chartpath, textptr.get(), true, &Skin);
        Online.SongSpeed = Config.SongSpeed;
    }
    else if (!OnlineGuest) {
        chartpath = BoxDatas[BoxDataIndex]->GetChart()->ChartPath;
        loaded = textptr->Load(chartpath) && LoadData.Load(chartpath, textptr.get(), true, &Skin);
        Online.SongSpeed = Config.SongSpeed;
    }
    else {
        // 配布された譜面はファイルに書き出さずメモリから直接読む
        if (!Online.Song) {
            NowScene = Scene::MultiRoom;
            return;
        }
        CourseIndex = (size_t)std::max(Online.Song->CourseIndex, 0);
        Online.SongSpeed = Online.Song->SongSpeed;
        chartpath = GetExecutablePath() / "online.tja";
        textptr->LoadFromMemory(std::string_view(reinterpret_cast<const char*>(Online.Song->Chart.data()), Online.Song->Chart.size()));
        loaded = LoadData.Load(chartpath, textptr.get(), true, &Skin);
    }

    if (!loaded || CourseIndex >= (size_t)CourseType::Count || !LoadData.Courses[CourseIndex].IsPlayable) {
        // 失敗の理由: ファイルが開けない・ヘッダーが読めない・選んだコースが無い
        std::string reason = !loaded ? (LoadData.LoadError.empty() ? std::string("ファイルを開けません (見つからない・使用中・アクセスできない)") : LoadData.LoadError)
            : "選んだコース (" + std::string(CourseIndex < (size_t)CourseType::Count ? magic_enum::enum_name((CourseType)CourseIndex) : "?") + ") がこの譜面にありません (COURSE: と #START を確認してください)";
        ShowErrorDialog(PathToU8(chartpath) + " の読み込みに失敗しました。\n原因: " + reason);
        if (OnlineGuest && Online.Song) {
            Online.FailedSerial = Online.Song->Serial;
            Online.Song->Wipe();
        }
        Dan.Reset();
        NowScene = PrevScene;
        return;
    }

    // 段位道場の開始: 条件を全体条件・個別条件の順に並べる
    if (LoadData.IsDan && !DanContinue) {
        if (LoadData.DanSongs.empty()) {
#ifndef __ANDROID__
            MessageBox(NULL, TEXT("段位譜面に #NEXTSONG がありません。"), TEXT("エラー"), MB_ICONERROR);
#else
            ShowToast("段位譜面に #NEXTSONG がありません。");
#endif
            NowScene = PrevScene;
            return;
        }
        Dan.Reset();
        Dan.Active = true;
        Dan.Course = LoadData;
        Dan.Text = textptr;
        for (const auto& exam : LoadData.ExamDatas) {
            Dan.Exams.push_back({ exam, -1 });
        }
        for (size_t i = 0; i < LoadData.DanSongs.size(); ++i) {
            for (const auto& exam : LoadData.DanSongs[i].Exams) {
                Dan.Exams.push_back({ exam, (int)i });
            }
        }
    }
    const TextLines& text = *textptr;
    // BM/HB譜面はプレイ前に一時的な互換モードを確認する。
    const bool hbScroll = RequiresJiroTiming(text.lines);
    if (hbScroll && !Config.JiroChartMode && AskTemporaryJiroMode()) {
        if (!Config.JiroChartModeSaved) { Config.JiroChartModeSaved = Config.JiroChartMode; }
        Config.JiroChartMode = true;
    }
    // osu の絶対時刻を、次郎の小節ごとの整数切り捨てで累積してずらさない。
    if (!hbScroll && osutaiko::IsConverted(text.lines) && Config.JiroChartMode) {
        if (!Config.JiroChartModeSaved) { Config.JiroChartModeSaved = Config.JiroChartMode; }
        Config.JiroChartMode = false;
    }

    // ノーツ解析では行を書き換えるため、行ビューの配列だけ複製する (文字列本体は text が保持)
    std::vector<std::string_view> lines = text.lines;

    // 段位の 2 曲目以降は判定 (累計スコア) を引き継ぐ
    Chart.Init(!DanContinue);
    Chart.IsOsuChart = osutaiko::IsConverted(text.lines);

    NoteData MainData;
    NoteData MemData;
    Chart.OriginalData = LoadData;
    Chart.NowBPM = LoadData.BPM;
    MainData.BPM = LoadData.BPM;

    double _offset = 0;
    if (240 / LoadData.BPM > -LoadData.SongOffset) {
        _offset = 240000 / LoadData.BPM;
        Chart.SongBlankTime = _offset - (LoadData.SongOffset * -1000) + Config.SongOffset;
        MainData.RelaTime = _offset;
    }
    else {
        _offset = LoadData.SongOffset * -1000 - Config.SongOffset;
        Chart.SongBlankTime = 0;
        MainData.RelaTime = _offset;
    }

    // 譜面オフセット: 音源はそのままで、音符・小節線の時刻だけをずらす。
    // 譜面の開始が 0 より前になる場合は、その分だけ音源も遅らせて開始前の待ち時間を確保する
    if (Config.ChartOffset != 0) {
        _offset += Config.ChartOffset;
        if (_offset < 0) {
            Chart.SongBlankTime += -_offset;
            _offset = 0;
        }
        MainData.RelaTime = _offset;
    }

    Chart.RawNoteDatas.push_back(MainData);
    MainData.AbsTime = _offset;

    for (size_t i = 0; i < (size_t)CourseType::Count; ++i) {
        if (LoadData.Courses[i].IsPlayable && i != CourseIndex) {
            for (size_t j = (size_t)LoadData.Courses[i].Index; j < lines.size(); ++j) {
                if (lines[j].find("#END") != std::string::npos) {
                    lines[j] = "";
                    break;
                }
                lines[j] = "";
            }
        }
    }

    size_t BarlineCounter[2]{};
    size_t BarlineNoteCount = 0;
    size_t BranchCount = 0;

    short StartFlag = 0;
    bool BarlineDisplay = true;
    bool BarlineLoading = false;
    bool AddBarline = false;
    bool NowRollFlag = false;

    bool NowBranchStartFlag = false;
    bool BranchStartFindFlag = false;

    size_t RollStartIndex = 0;
    char RollType = '\0';

    size_t BalloonIndex = 0;
    size_t NoteCount = 0;
    size_t BarlineCount = 0;
    int WrittenMeasure = 0;         // 書かれた順の小節の番号 (#BARLINEOFF の小節も数える)

    double BranchAddTime = 0;

    // 通常: 分岐ごとに #BPMCHANGE / #MEASURE / #DELAY で時刻を計算し、分岐の後は最後に書かれた分岐の続きから進める。
    // 次郎形式: 分岐中の #BPMCHANGE / #MEASURE / #DELAY は最初に書かれた分岐だけが有効で、
    //           2 つ目以降の分岐は最初の分岐の小節ごとの時刻・BPM・拍子を使い、分岐の後は最初の分岐の終わりから続ける
    const bool jiro = Config.JiroChartMode && (hbScroll || !osutaiko::IsConverted(text.lines));
    struct MeasureTiming { double AbsTime; double BPM; double Measure; double PosTime; };
    std::vector<MeasureTiming> BranchTimeline;
    MeasureTiming BranchEnd{};
    size_t BranchMeasureIndex = 0;
    double CurMeasureStart = _offset;   // 直近の小節の開始時刻 (分岐判定の時刻に使う)
    std::optional<double> PendingMeasure;
    // 次郎形式の時刻 (taikojiro の TJA 読み込みと同じ整数ミリ秒の計算):
    //   小節の頭 = 直前の #BPMCHANGE の時刻 + (その小節の残りの拍の長さ を切り捨て) + (その後の小節の拍の長さの合計 を切り捨て) + その後の #DELAY
    //   音符 = 小節の頭 (同じ小節に #BPMCHANGE があればその時刻) + (位置までの長さ を切り捨て) + 小節の中の #DELAY
    //   → 小数のミリ秒は #BPMCHANGE のたびに切り捨てられて、少しずつ早くなる (次郎と同じずれ)
    struct JiroClock {
        bool Started = false;
        double EvTime = 0, EvRemain = 0, After = 0, TotalDelay = 0;
        int EvIndex = 0;
        bool EvHere = false;            // この小節に #BPMCHANGE がある
        bool Pending = false;           // 次の位置で #BPMCHANGE を確定する
        double PendingBpm = 0;
        double PendingTime = 0;         // 後続の DELAY を足す前の命令位置
        double Bpm = 120, Ratio = 1;    // 今の位置の計算に使う BPM と拍子
        double MeasureStart = 0, MeasureDelay = 0, HeadDelay = 0, Next = 0;
        int Index = 0, Count = 1, PrevCount = 1;
        double LastTime = 0;            // 最後に計算した位置の時刻
        double HeadStart = 0;           // 2 つ目以降の分岐: 小節の頭
        double Beat() const { return 60000.0 / Bpm; }
        double PosTime(int k) const {
            const double t = EvHere
                ? JiroTrunc(Beat() * (double)(k - EvIndex) / Count * 4.0 * Ratio) + EvTime
                : JiroTrunc(Beat() * (double)k / Count * 4.0 * Ratio) + MeasureStart;
            return std::max(0.0, t + MeasureDelay);
        }
    } jc;
    // 次郎は命令の値を決まった文字数だけ読む (#BPMCHANGE・#SCROLL は 8 文字、#MEASURE・#DELAY は 10 文字。命令名の直後から)
    auto JiroArg = [&](std::string_view line, size_t keylen, size_t n, std::string_view data) -> std::string_view {
        if (!jiro || line.size() <= keylen) { return data; }
        return strtrim(line.substr(keylen, std::min(n, line.size() - keylen)));
    };
    auto TimingCommandEnabled = [&] { return !jiro || !NowBranchStartFlag || BranchCount < 2; };
    // 次郎は #BPMCHANGE・#SCROLL・#MEASURE・#DELAY の値を共有の変数に sscanf で読み、読めなかった時は前に読んだ値がそのまま残る
    // (範囲の確認もしない)。#MEASURE の分母も前に読んだ分母が残る (例: "#MEASURE -3057.815/1" は 10 文字で切れて、分母は前の値)
    double JiroScanD = 0.0;
    uint32_t JiroScanU = 1;
    auto JiroScanDouble = [&](std::string_view arg) -> double {
        const std::string text(arg);
        char* end = nullptr;
        const double v = std::strtod(text.c_str(), &end);
        if (end != text.c_str()) { JiroScanD = v; }
        return JiroScanD;
    };
    auto JiroScanMeasure = [&](std::string_view arg) -> double {
        const std::string text(arg);
        const char* p = text.c_str();
        char* end = nullptr;
        const double num = std::strtod(p, &end);
        if (end != p) {
            JiroScanD = num;
            if (*end == '/') {
                char* end2 = nullptr;
                const long den = std::strtol(end + 1, &end2, 10);       // %d (負の値は符号なしの大きな値になる)
                if (end2 != end + 1) { JiroScanU = (uint32_t)den; }
            }
        }
        return JiroScanD / (double)JiroScanU;
    };
    // BM/HBSCROLL の拍マップと BPM 表示は最初の分岐の BPM を使う
    auto FirstBranchOrOutside = [&] { return !NowBranchStartFlag || BranchCount < 2; };

    size_t StartIndex = 0;

    if (LoadData.IsDan) {
        if (Dan.SongIndex >= LoadData.DanSongs.size()) {
            Dan.Reset();
            Chart.Init(true);
            NowScene = PrevScene;
            return;
        }
        StartIndex = LoadData.DanSongs[Dan.SongIndex].StartLine;
    }

    for (size_t i = StartIndex; i < lines.size(); ++i) {
        try {
            if (auto pos = lines[i].find("//"); pos != std::string_view::npos) {
                lines[i] = lines[i].substr(0, pos);
            }
            Exsubstr(lines[i], "#START", [&](std::string_view data) {
                StartFlag = 1;
                });
            Exsubstr(lines[i], "#END", [&](std::string_view data) {
                StartFlag = 2;
                });
            if (LoadData.IsDan) {
                Exsubstr(lines[i], "#NEXTSONG", [&](std::string_view data) {
                    ++StartFlag;
                    });
            }
            Exsubstr(lines[i], "#GOGOSTART", [&](std::string_view data) {
                MainData.GoGoStart = true;
                });
            Exsubstr(lines[i], "#GOGOEND", [&](std::string_view data) {
                MainData.GoGoEnd = true;
                });
            Exsubstr(lines[i], "#BARLINEON", [&](std::string_view data) {
                BarlineDisplay = true;
                });
            Exsubstr(lines[i], "#BARLINEOFF", [&](std::string_view data) {
                BarlineDisplay = false;
                });
            if (!StartFlag) {
                Exsubstr(lines[i], "#BMSCROLL", [&](std::string_view data) {
                    Chart.ScrollType = ScrollType::BMSCROLL;
                    });
                Exsubstr(lines[i], "#HBSCROLL", [&](std::string_view data) {
                    Chart.ScrollType = ScrollType::HBSCROLL;
                    });
            }
            Exsubstr(lines[i], "#SECTION", [&](std::string_view data) {
                MainData.Section = true;
                });
            Exsubstr(lines[i], "#LEVELHOLD", [&](std::string_view data) {
                MainData.LevelHold = true;
                });
            Exsubstr(lines[i], "#BRANCHSTART", [&](std::string_view data) {
                // 種類: r = 連打数, s = スコア, それ以外 = 精度 (次郎1 と同じく大文字小文字を区別しない)
                auto sp = split(data, ',');
                if (sp.size() < 3) { throw std::invalid_argument("BRANCHSTART"); }
                BranchData item;
                const char kind = sp[0].empty() ? 'p' : (char)std::tolower((unsigned char)strtrim(sp[0]).front());
                item.Type = kind == 'r' ? IfBranchType::Roll : kind == 's' ? IfBranchType::Score : IfBranchType::Perfect;
                item.ExpertBranch = svtov<double>(strtrim(sp[1]));
                item.MasterBranch = svtov<double>(strtrim(sp[2]));
                // 判定は分岐する小節の 1 つ前の小節の頭で確定する (次郎形式の連打数の分岐は分岐の頭)
                item.AbsTime = jiro && item.Type == IfBranchType::Roll ? MainData.AbsTime : CurMeasureStart;
                BranchTimeline.clear();
                item.StartMs = MainData.AbsTime;
                item.Start = true;
                BranchCount = 0;
                Chart.BranchDatas.push_back(item);
                NowBranchStartFlag = true;
                BranchStartFindFlag = true;
                BranchAddTime = 0;
                });
            if (NowBranchStartFlag) {
                const std::string_view command = strtrim(lines[i]).substr(0, strtrim(lines[i]).find_first_of(" \t"));
                BranchType section = BranchType::Null;
                if (command == "#N") { section = BranchType::Normal; }
                else if (command == "#E") { section = BranchType::Expert; }
                else if (command == "#M") { section = BranchType::Master; }
                if (section != BranchType::Null) {
                    if (BranchCount == 0) {
                        MemData = MainData;
                    }
                    else {
                        if (BranchCount == 1) {
                            BranchEnd = { MainData.AbsTime, MainData.BPM, MainData.Measure, MainData.PosTime };
                        }
                        Chart.RawNoteDatas.back().RelaTime += -BranchAddTime;
                    }
                    MainData = MemData;
                    if (BranchCount == 0) { MainData.BranchStart = true; }
                    MainData.IsBranch = section;
                    BranchAddTime = 0;
                    BranchMeasureIndex = 0;
                    ++BranchCount;
                }
                Exsubstr(lines[i], "#BRANCHEND", [&](std::string_view data) {
                    if (jiro && BranchCount >= 2) {
                        MainData.AbsTime = BranchEnd.AbsTime;
                        MainData.BPM = BranchEnd.BPM;
                        MainData.Measure = BranchEnd.Measure;
                        MainData.PosTime = BranchEnd.PosTime;
                    }
                    if (BranchCount == 0) { MainData.BranchStart = true; }
                    MainData.IsBranch = BranchType::Null;
                    NowBranchStartFlag = false;
                    BranchAddTime = 0;
                    BranchCount = 0;
                    });
            }
            Exsubstr(lines[i], "#SCROLL", [&](std::string_view data) {
                data = JiroArg(lines[i], 7, 8, data);
                if (data.find("i") != std::string_view::npos) {
                    size_t Uindex = data.rfind("+") == std::string_view::npos ? 0 : data.rfind("+");
                    size_t Dindex = data.rfind("-") == std::string_view::npos ? 0 : data.rfind("-");

                    bool Flag = Uindex > Dindex;

                    if (data.rfind("+") != std::string_view::npos && Flag) {
                        std::string_view real = strtrim(data.substr(0, data.rfind("+")));
                        std::string_view imag = strtrim(data.substr(data.rfind("+") + 1, data.rfind("i") - (data.rfind("+") + 1)));
                        MainData.Scroll = real.empty() ? 0 : svtov<double>(real);
                        MainData.Scrolli = imag.empty() ? -1 : svtov<double>(imag) * -1;
                    }
                    if (data.rfind("-") != std::string_view::npos && !Flag) {
                        std::string_view real = strtrim(data.substr(0, data.rfind("-")));
                        std::string_view imag = strtrim(data.substr(data.rfind("-") + 1, data.rfind("i") - (data.rfind("-") + 1)));
                        MainData.Scroll = real == "" ? 0 : svtov<double>(real);
                        MainData.Scrolli = imag == "" ? 1 : svtov<double>(imag);
                    }
                }
                else {
                    MainData.Scroll = svtov<double>(data);
                    MainData.Scrolli = 0;
                    if (jiro) {
                        // 次郎: 読めなければ前に読んだ値 (範囲の確認はしない)。0 は使われない (直前の SCROLL のまま)
                        const double v = JiroScanDouble(data);
                        if (std::isfinite(v) && v != 0.0) { MainData.Scroll = v; }
                        else { MainData.Scroll = Chart.ScrollEvents.empty() ? 1.0 : Chart.ScrollEvents.back().Value; }
                    }
                }
                Chart.ScrollEvents.push_back({ MainData.AbsTime, MainData.Scroll, MainData.IsBranch });
                });
            if (TimingCommandEnabled()) {
                Exsubstr(lines[i], "#BPMCHANGE", [&](std::string_view data) {
                    double bpm = svtov<double>(JiroArg(lines[i], 10, 8, data));
                    if (jiro) {
                        // 次郎: 読めなければ前に読んだ値。0 (区間として使われない) と無限大は直前の BPM のまま
                        bpm = JiroScanDouble(JiroArg(lines[i], 10, 8, data));
                        if (!std::isfinite(bpm) || bpm == 0.0) { bpm = MainData.BPM; }
                    }
                    // BPM 0 は小節の長さが無限大になる (次郎でも止まる) ため、記述の誤りとして扱う
                    else if (bpm == 0.0 || !std::isfinite(bpm)) { throw std::invalid_argument("BPMCHANGE"); }
                    MainData.BPM = bpm;
                    MainData.BpmChangeFlag = true;
                    // BM/HBSCROLL の拍マップと BPM 表示は最初の分岐の BPM を使う
                    if (FirstBranchOrOutside()) {
                        if (jiro) {
                            jc.PendingTime = !jc.Started ? MainData.AbsTime : !AddBarline ? jc.Next + jc.HeadDelay : jc.PosTime(jc.Index);
                            jc.Pending = true; jc.PendingBpm = bpm;
                        }
                        else { Chart.BpmEvents.push_back({ MainData.AbsTime, MainData.BPM }); }
                    }
                    });
                Exsubstr(lines[i], "#MEASURE", [&](std::string_view data) {
                    if (jiro) {
                        // 次郎と同じく、命令名の後の 10 文字を sscanf("%lf/%d") で読む。読めなかった部分は前に読んだ値のまま
                        double measure = JiroScanMeasure(JiroArg(lines[i], 8, 10, data));
                        if (!std::isfinite(measure)) { measure = MainData.Measure; }      // 分母 0 (次郎でも時刻が壊れる)
                        if (AddBarline) { PendingMeasure = measure; }     // 小節の途中の #MEASURE は次の小節から効く
                        else { MainData.Measure = measure; }
                        return;
                    }
                    auto sp = split(JiroArg(lines[i], 8, 10, data), '/');
                    const bool valid = sp.size() >= 2 && svtov<double>(sp[1]) != 0.0 && std::isfinite(svtov<double>(sp[0]) / svtov<double>(sp[1]));
                    // 次郎: 正しくない値 (分母が無い・0 など) の時は直前の拍子のまま
                    if (!valid) {
                        if (jiro) { return; }
                        throw std::invalid_argument("MEASURE");
                    }
                    const double measure = svtov<double>(sp[0]) / svtov<double>(sp[1]);
                    // 次郎形式: 小節の途中の #MEASURE は次の小節から効く
                    if (jiro && AddBarline) { PendingMeasure = measure; }
                    else { MainData.Measure = measure; }
                    });
                Exsubstr(lines[i], "#DELAY", [&](std::string_view data) {
                    double delay = svtov<double>(JiroArg(lines[i], 6, 10, data)) * 1000;
                    if (jiro) {
                        // 次郎: (int)(秒 × 1000) が 0 でなければ使う (1ms 未満は無視、負の値も使う)。読めなければ前に読んだ値
                        delay = JiroScanDouble(JiroArg(lines[i], 6, 10, data)) * 1000.0;
                        if (!std::isfinite(delay) || std::abs(delay) > 2e9) { return; }
                        delay = JiroTrunc(delay);
                        if (delay == 0.0) { return; }
                        const bool head = !AddBarline;
                        // 次郎は命令の位置の時刻 (この #DELAY を足す前) から止め始めを決める
                        const double here = !jc.Started ? MainData.AbsTime : head ? jc.Next + jc.HeadDelay : jc.PosTime(jc.Index);
                        if (head) { jc.HeadDelay += delay; }
                        else { jc.MeasureDelay += delay; }
                        jc.TotalDelay += delay;
                        if (delay > 0 && FirstBranchOrOutside()) {
                            // 次郎と同じく、止め始めは「命令の位置 + 5ms - 1 区切り」
                            const int count = head ? jc.PrevCount : jc.Count;
                            const double sh = JiroTrunc(jc.Ratio * 4.0 / std::max(count, 1) * JiroTrunc(jc.Beat()));
                            Chart.DelayEvents.push_back({ jc.Started ? here + 5.0 - sh : here, delay, MainData.BPM });
                        }
                        MainData.AbsTime = !jc.Started ? MainData.AbsTime + delay : head ? jc.Next + jc.HeadDelay : jc.PosTime(jc.Index);
                        return;
                    }
                    // 正の #DELAY は BM/HBSCROLL で譜面を止める (負の値は時刻をずらすだけ)
                    if (delay > 0 && FirstBranchOrOutside()) {
                        Chart.DelayEvents.push_back({ MainData.AbsTime, delay, MainData.BPM });
                    }
                    Chart.RawNoteDatas.back().RelaTime += delay;
                    MainData.AbsTime += delay;
                    });
            }

            if (lines[i].find("#") != std::string_view::npos) { continue; }
            if (StartFlag != 1) { continue; }

            if (!BarlineLoading) {
                BarlineLoading = true;
                for (size_t j = i; j < lines.size(); ++j) {
                    if (lines[j].find("#") != std::string_view::npos) { continue; }
                    for (size_t k = 0, strsize = lines[j].size(); k < strsize; ++k) {
                        if (lines[j][k] == ',') {
                            goto BARLINEREADEND;
                        }
                        else if (lines[j][k] >= '0' && lines[j][k] <= '9') {
                            ++BarlineNoteCount;
                        }
                    }
                }
            BARLINEREADEND:;
            }
        }
        catch (const std::exception& e) {
            // 何行目のどの記述で、何が読めなかったかを出す
            std::string text(strtrim(lines[i]));
            if (text.size() > 80) { text = text.substr(0, 80) + "…"; }
            ShowErrorDialog(std::to_string(i + 1) + " 行目の記述が不正です。\n" + text + "\n原因: " + DescribeException(e));
            NowScene = PrevScene;
            return;
        }

        // 次郎形式: 小節の終わりで次の小節の頭の時刻を決める
        auto JiroMeasureEnd = [&] {
            if (!jiro || !jc.Started || !TimingCommandEnabled()) { return; }
            if (jc.EvHere) {
                jc.EvRemain = (1.0 - (double)jc.EvIndex / jc.Count) * 4.0 * jc.Ratio;
                jc.After = 0;
                jc.EvHere = false;
            }
            else {
                jc.After += 4.0 * jc.Ratio;
            }
            jc.Next = std::max(0.0, jc.EvTime + JiroTrunc(jc.Beat() * jc.EvRemain) + JiroTrunc(jc.Beat() * jc.After) + jc.TotalDelay);
            jc.PrevCount = jc.Count;
            MainData.AbsTime = jc.Next;
        };
        for (size_t j = 0, strsize = lines[i].size(); j < strsize; ++j) {
            bool ChartFlag = (lines[i][j] >= '0' && lines[i][j] <= '9');
            bool EndFlag = lines[i][j] == ',';
            bool EmptyFlag = BarlineNoteCount == 0;
            if (ChartFlag || EndFlag || EmptyFlag) {

                if (EndFlag && !EmptyFlag) {
                    JiroMeasureEnd();
                    BarlineLoading = false;
                    AddBarline = false;
                    BarlineNoteCount = 0;
                    break;
                }

                MainData.NoteType = lines[i][j];

                // 小節の頭: 次郎形式では 2 つ目以降の分岐は最初の分岐と同じ時刻・BPM・拍子で始める
                if (!AddBarline) {
                    if (PendingMeasure) { MainData.Measure = *PendingMeasure; PendingMeasure.reset(); }
                    if (jiro) {
                        jc.Index = 0;
                        jc.Count = EmptyFlag ? 1 : (int)BarlineNoteCount;
                        if (TimingCommandEnabled()) {
                            if (!jc.Started) {
                                // 譜面の始まり = BPM: の値の区間の始まり
                                jc.Started = true;
                                jc.EvTime = jc.Next = std::max(0.0, JiroTrunc(MainData.AbsTime - jc.HeadDelay));
                                jc.Bpm = MainData.BPM;
                                jc.EvIndex = 0;
                                jc.EvHere = true;
                                jc.TotalDelay = jc.HeadDelay;
                            }
                            jc.MeasureStart = jc.Next + jc.HeadDelay;
                            jc.HeadDelay = 0;
                            jc.MeasureDelay = 0;
                            jc.Ratio = MainData.Measure;
                            MainData.AbsTime = jc.MeasureStart;
                        }
                    }
                    if (jiro && NowBranchStartFlag && BranchCount >= 2 && BranchMeasureIndex < BranchTimeline.size()) {
                        const MeasureTiming& t = BranchTimeline[BranchMeasureIndex];
                        MainData.AbsTime = t.AbsTime;
                        MainData.BPM = t.BPM;
                        MainData.Measure = t.Measure;
                        MainData.PosTime = t.PosTime;
                    }
                    if (NowBranchStartFlag && BranchCount == 1) {
                        BranchTimeline.push_back({ MainData.AbsTime, MainData.BPM, MainData.Measure, MainData.PosTime });
                    }
                    if (NowBranchStartFlag) {
                        ++BranchMeasureIndex;
                    }
                    CurMeasureStart = MainData.AbsTime;
                    jc.HeadStart = MainData.AbsTime;
                }

                // 次郎形式: この位置の時刻を整数ミリ秒で決める (#BPMCHANGE はここで確定する)
                if (jiro && jc.Started) {
                    if (TimingCommandEnabled()) {
                        if (jc.Pending) {
                            const double delayedTime = jc.PosTime(jc.Index);
                            jc.EvTime = jc.PendingTime;
                            jc.Bpm = jc.PendingBpm;
                            jc.EvIndex = jc.Index;
                            jc.EvHere = true;
                            jc.After = 0;
                            // BPMCHANGE の直後に DELAY がある場合も、停止中は新しい BPM の区間。
                            // ノーツの判定時刻を変えず、区間開始からの停止時間を別に保持する。
                            jc.TotalDelay = jc.MeasureDelay = delayedTime - jc.PendingTime;
                            jc.Pending = false;
                            Chart.BpmEvents.push_back({ jc.EvTime, jc.Bpm });
                        }
                        MainData.AbsTime = jc.PosTime(jc.Index);
                    }
                    else {
                        // 2 つ目以降の分岐: 最初の分岐の小節の頭から
                        MainData.AbsTime = std::max(0.0, JiroTrunc(60000.0 / MainData.BPM * (double)jc.Index / jc.Count * 4.0 * MainData.Measure) + jc.HeadStart);
                    }
                    jc.LastTime = MainData.AbsTime;
                }

                double barlinetime = (240000 / MainData.BPM) * MainData.Measure;
                double divtime = barlinetime / (EmptyFlag ? 1 : BarlineNoteCount);

                MainData.RelaTime = divtime;

                MainData.MeasureIndex = !AddBarline ? WrittenMeasure++ : -1;
                if (!AddBarline) {
                    AddBarline = true;
                    if (BarlineDisplay) {
                        MainData.BarlineDisplay = true;
                        ++BarlineCount;
                    }
                    else {
                        MainData.BarlineDisplay = false;
                    }
                }
                else {
                    MainData.BarlineDisplay = false;
                }

#define Matched if (RollType == _ch) {\
break;\
}


#define SetOtherRollEnd Chart.RawNoteDatas[RollStartIndex].RollEndTime = MainData.AbsTime;\
Chart.RawNoteDatas[RollStartIndex].RollEndIndex = Chart.RawNoteDatas.size();\
RollStartIndex = Chart.RawNoteDatas.size();\
RollType = MainData.NoteType

#define Set8RollEnd Chart.RawNoteDatas[RollStartIndex].RollEndTime = MainData.AbsTime;\
Chart.RawNoteDatas[RollStartIndex].RollEndIndex = Chart.RawNoteDatas.size();\
NowRollFlag = false;\
RollType = '\0'

                if (NowRollFlag) {
                    const char _ch = MainData.NoteType;
                    switch (_ch) {
                    case '5':
                        Matched;
                        SetOtherRollEnd;
                        break;
                    case '6':
                        Matched;
                        SetOtherRollEnd;
                        break;
                    case '7':
                        Matched;
                        SetOtherRollEnd;
                        break;
                    case '8':
                        Set8RollEnd;
                        break;
                    case '9':
                        Matched;
                        SetOtherRollEnd;
                        break;
                    }
                }

#undef Matched
#undef SetOtherRollEnd
#undef Set8RollEnd

                if (!NowRollFlag && ((MainData.NoteType >= '5' && MainData.NoteType <= '7') || MainData.NoteType == '9')) {
                    RollStartIndex = Chart.RawNoteDatas.size();
                    NowRollFlag = true;
                    RollType = MainData.NoteType;
                }

                bool removeflag =
                    MainData.NoteType == '0' &&
                    !MainData.GoGoStart &&
                    !MainData.GoGoEnd &&
                    !MainData.BpmChangeFlag &&
                    !MainData.BarlineDisplay &&
                    !MainData.BranchStart &&
                    !MainData.Section &&
                    !MainData.LevelHold &&
                    !(Editor.Testing && MainData.MeasureIndex >= 0);     // テストプレイ: 小節の頭は残す (途中の小節から始める位置に使う)

                if (MainData.NoteType >= '1' && MainData.NoteType <= '4') {
                    NoteCount += MainData.IsBranch == BranchType::Null || MainData.IsBranch == BranchType::Normal;
                }
                if (MainData.NoteType == '0') {
                    MainData.HitFlag = true;
                }
                if (sys::Rand(99) < Config.RandomRate) {
                    switch (MainData.NoteType) {
                    case '1':
                        MainData.NoteType = '2';
                        break;
                    case '2':
                        MainData.NoteType = '1';
                        break;
                    case '3':
                        MainData.NoteType = '4';
                        break;
                    case '4':
                        MainData.NoteType = '3';
                        break;
                    }
                }

                if (!removeflag) {
                    NoteData data(MainData);
                    data.Scroll *= Config.ChartSpeed;
                    data.Scrolli *= Config.ChartSpeed;
                    data.PosTime *= std::abs(std::complex(data.Scroll, data.Scrolli));
                    Chart.RawNoteDatas.push_back(std::move(data));
                }
                else {
                    Chart.RawNoteDatas.back().RelaTime += MainData.RelaTime;
                }

                if (MainData.NoteType == '7' || MainData.NoteType == '9') {
                    auto& balloon = LoadData.Courses[CourseIndex].Balloons;
                    uint64_t ballooncount = 0;
                    if (BalloonIndex + Chart.DanBalloonIndex < balloon.size()) {
                        ballooncount = balloon[BalloonIndex + Chart.DanBalloonIndex];
                    }
                    else {
                        ballooncount = 5;
                    }
                    Chart.RawNoteDatas.back().BalloonCount = ballooncount;
                    ++BalloonIndex;
                }

                MainData.HitFlag = false;
                MainData.GoGoStart = false;
                MainData.GoGoEnd = false;
                MainData.BpmChangeFlag = false;
                MainData.Section = false;
                MainData.LevelHold = false;
                MainData.BranchStart = false;

                MainData.AbsTime += divtime;
                if (jiro && jc.Started) {
                    ++jc.Index;
                    // 命令の時刻 (#SCROLL など) が次の位置の時刻になるように
                    if (TimingCommandEnabled()) { MainData.AbsTime = jc.PosTime(jc.Index); }
                    else { MainData.AbsTime = std::max(0.0, JiroTrunc(60000.0 / MainData.BPM * (double)jc.Index / jc.Count * 4.0 * MainData.Measure) + jc.HeadStart); }
                }
                MainData.PosTime += MainData.RelaTime * (std::signbit(MainData.BPM) || std::signbit(MainData.Measure) ? -1 : 1);

                if (NowBranchStartFlag) {
                    BranchAddTime += MainData.RelaTime * (std::signbit(MainData.BPM) || std::signbit(MainData.Measure) ? -1 : 1);
                }

                if (EndFlag) {
                    JiroMeasureEnd();
                    BarlineLoading = false;
                    AddBarline = false;
                    BarlineNoteCount = 0;
                    break;
                }
            }
        }
    }

    // 次郎形式: 音符の時刻はミリ秒の整数 (切り捨て) で、負の時刻は 0 にする。判定は分岐ごとに書かれた順に行う
    if (Config.JiroChartMode) {
        for (auto& note : Chart.RawNoteDatas) {
            note.AbsTime = std::max(0.0, JiroTrunc(note.AbsTime));
            if (note.RollEndTime != 0) { note.RollEndTime = std::max(0.0, JiroTrunc(note.RollEndTime)); }
        }
        Chart.BuildJiroLanes();
    }

    // BPM 区間・#DELAY・#SCROLL の索引 (表示位置の計算用)
    Chart.BuildTiming(LoadData.BPM, Config.JiroChartMode);
    Chart.BuildScrollLanes(Config.JiroChartMode);

    if (Chart.IsDanMode()) {
        Chart.DanBalloonIndex += BalloonIndex;
        if (Dan.SongIndex == 0) {
            Chart.AllNoteCount = AllNoteCounter(lines);
        }
    }
    else {
		Chart.AllNoteCount = NoteCount;
    }
    Chart.AllBarlineCount = BarlineCount;
    Chart.AddScore = LoadData.Courses[CourseIndex].AddScore;
    // 段位は課題曲ごとの SCOREINIT (#NEXTSONG の 5 番目) を優先する
    if (LoadData.IsDan && LoadData.DanSongs[Dan.SongIndex].ScoreInit > 0) {
        Chart.AddScore = LoadData.DanSongs[Dan.SongIndex].ScoreInit;
    }
    if (Chart.AddScore == 0 && Chart.AllNoteCount != 0) {
        Chart.AddScore = (uint64_t)(1000000 / (double)Chart.AllNoteCount);
    }
    Chart.NowBranchFlag = BranchStartFindFlag ? BranchType::Normal : BranchType::Null;
    if (Config.TrainingMode && !Chart.IsDanMode() && Chart.IsBranchChart()) {
        if (Training.MemBranchFlag > BranchType::Null) {
            Chart.NowBranchFlag = Training.MemBranchFlag;
        }
    }

    // 演奏用の楽曲は全デコードしてメモリに置く (再生中のディスク I/O を無くし、開始位置の指定を正確にするため)
    ((void)0);
    if (!Chart.IsDanMode()) {
        if (!OnlineGuest) {
            Chart.SongData.Load(LoadData.SongPath.string());
        }
        else {
            Chart.SongData.Load(Online.Song->Audio.data(), Online.Song->Audio.size());
        }
    }
    else {
        // 前の曲の演奏中に先読みしていればそれを使う
        if (Dan.NextSongIndex == Dan.SongIndex && Dan.NextSong.Handle != -1) {
            Dan.NextSong.FinishAsync();
            Chart.SongData = Dan.NextSong;
            Dan.NextSong.Handle = -1;
            Dan.NextSongIndex = SIZE_MAX;
        }
        else {
            Chart.SongData.Load(LoadData.DanSongs[Dan.SongIndex].WavePath.string());
        }
    }
    ((void)0);

    SetSpeed();

    Skin.Base->Playing.SE.Don.SetVolume(Chart.OriginalData.SeVolume * (Config.SEVolume / 100));
    Skin.Base->Playing.SE.Ka.SetVolume(Chart.OriginalData.SeVolume * (Config.SEVolume / 100));
    Skin.Base->Playing.SE.DonBig.SetVolume(Chart.OriginalData.SeVolume * (Config.SEVolume / 100));
    Skin.Base->Playing.SE.KaBig.SetVolume(Chart.OriginalData.SeVolume * (Config.SEVolume / 100));
    Skin.Base->Playing.SE.Balloon.SetVolume(Chart.OriginalData.SeVolume * (Config.SEVolume / 100));

    HitNote.resize(IsMulti ? fasnet::MaxPlayers : 1);
    Chart.Judge.resize(IsMulti ? fasnet::MaxPlayers : 1);
    Chart.Judge[0].ScoreRateGood = ScoreRateCalc(Config.JudgeGood, 25.0);
    Chart.Judge[0].ScoreRateOk = ScoreRateCalc(Config.JudgeOk, 75.0);

    if (Dan.Active) {
        Dan.SongStart = Chart.Judge[0];
        Dan.LastCombo = Chart.Judge[0].Combo;
        Dan.SongCombo = 0;
        Dan.SongMaxCombo = 0;
    }

    if (IsMulti) {
        if (IsHost()) {
            // ホスト: 譜面と音源を配布する。受理されると RoomState で通し番号が届き、そこで準備完了を送る
            // (譜面と音源は 256KB ずつに分けて暗号化したストリームで送る。受信側はディスクに書き込まない)
            std::vector<uint8_t> chartbytes = FileToMem(chartpath);
            std::vector<uint8_t> audiobytes = FileToMem(LoadData.SongPath);
            if (chartbytes.empty() || audiobytes.empty() || chartbytes.size() > fasnet::MaxChartSize || audiobytes.size() > fasnet::MaxAudioSize) {
#ifndef __ANDROID__
                MessageBox(NULL, TEXT("譜面または音源を読み込めない (または大きすぎる) ため配布できません。"), TEXT("エラー"), MB_ICONERROR);
#else
                ShowToast("譜面または音源を読み込めない (または大きすぎる) ため配布できません。");
#endif
                NowScene = Scene::MultiRoom;
                return;
            }
            fasnet::SongInfo info;
            info.CourseIndex = (int32_t)CourseIndex;
            info.Level = LoadData.Courses[CourseIndex].Level;
            info.SongSpeed = Config.SongSpeed;
            info.Title = LoadData.Title;
            info.Subtitle = LoadData.Subtitle;
            info.ChartSize = (uint32_t)chartbytes.size();
            info.AudioSize = (uint32_t)audiobytes.size();
            Online.Net.Send(fasnet::Msg::SongBegin, info);
            for (auto& chunk : fasnet::EncodeSongChunks(0, chartbytes, audiobytes)) {
                Online.Net.Send(fasnet::Msg::SongChunk, std::move(chunk), true);
            }
            Online.Net.Send(fasnet::Msg::SongEnd, fasnet::Encode(fasnet::U32Value{ 0 }), true);
            fasnet::crypto::SecureZero(chartbytes);
            fasnet::crypto::SecureZero(audiobytes);
            Online.SongRequested = true;
            Online.LoadedSerial = 0;
        }
        else {
            Online.LoadedSerial = Online.Song->Serial;
            Online.Net.Send(fasnet::Msg::Ready, fasnet::U32Value{ Online.LoadedSerial });
            // 譜面と音源は読み込み済み (音源はデコード済みのデータを DxLib が保持) なので、受信したデータは消去する
            Online.Song->Wipe();
            fasnet::crypto::SecureZero(textptr->data.data(), textptr->data.size());
            textptr->lines.clear();
        }
        NowScene = Scene::MultiRoom;
        return;
    }
    // テストプレイを途中の小節から: トレーニングモードの小節移動と同じく、その小節の頭へ移動して開始を待つ
    if (Editor.Testing && Editor.TestMeasure > 0) {
        // 小節は書かれた順の番号で探す (#BARLINEOFF の小節も数えるので、譜面制作の小節と同じ位置になる)
        uint64_t barline = 0;
        for (size_t i = 0; i < Chart.RawNoteDatas.size(); ++i) {
            if (Chart.RawNoteDatas[i].MeasureIndex < 0) { continue; }
            if (Chart.RawNoteDatas[i].BarlineDisplay) { ++barline; }
            if (Chart.RawNoteDatas[i].MeasureIndex == Editor.TestMeasure) {
                Training.NoteDataIndex = i;
                Training.BarlineIndex = barline > 0 ? barline - 1 : 0;
                Training.Offset = Chart.RawNoteDatas[i].AbsTime;
                for (auto& note : Chart.RawNoteDatas) {
                    if (note.AbsTime < Training.Offset) { note.HitFlag = true; }   // それより前の音符は見逃しにしない
                }
                break;
            }
        }
    }
    if (!Config.TrainingMode || Chart.IsDanMode()) {
        Chart.NowTime.Start();
        if (Editor.Testing && Training.Offset > Chart.SongBlankTime) {
            Chart.SongData.SetCurrent(Training.Offset - Chart.SongBlankTime);
            Chart.SongData.Play(FALSE);
        }
    }
#ifndef __ANDROID__
    LastHitTick = RawKeyboard::Now();
#endif

    NowScene = Scene::Playing;
}
