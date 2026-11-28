// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    // ===================================================================== 譜面制作モード
    // 小節を横 1 列に並べ、グリッド (分割数) に合わせてマウスで音符を置く。命令 (#BPMCHANGE など) は小節の中の位置に挿入する。
    // 譜面は tja::Document (Library/TjaDocument.hpp) で持ち、保存時に TJA に書き戻す (元が Shift_JIS ならそのまま Shift_JIS)。
    // テストプレイはメモリ上の譜面で演奏画面を開き、カーソルの小節から始める (トレーニングモードの小節移動を使う)。

    // 編集画面のボタンの絵 (None は文字のボタン)
    Scene EditorReturnScene=Scene::ModeSelect;
    enum class EditorIcon { None, Save, Test, Close, Play, Stop, Undo, Redo, Insert, Delete, Fold, Unfold, AudioLength, DemoStart, WaveAlign, Header, Command, Gimmick, Estimate, Left, Right, Note, CommandDelete, BpmAnalyze, JiroFix, Select, Copy, Cut, Paste, SpeedDown, SpeedUp, SpeedValue, JiroPreview, TaikoSound, Visible, Hidden };
    struct EditorButton {
        UIRect Rect;
        std::string Label;              // 絵のボタンでは名前 (マウスを乗せると出す)
        std::function<void()> Action;
        bool Active = false;            // 選択中 (パレットなど)
        EditorIcon Icon = EditorIcon::None;
        char Note = 0;                  // Icon == Note の時の音符の種類
    };
    struct EditorState {
        // 絵のボタンの画像 (キー: 絵の種類と大きさ)。図形を 4 倍の解像度で描いて縮小し、縁を滑らかにしたもの
        std::map<uint64_t, gfx::Texture> IconCache;
        tja::Document Doc;
        fs::path Path;
        bool Open = false;
        bool Dirty = false;
        uint64_t Revision = 0;
        bool WasSjis = false;           // 元のファイルが Shift_JIS だった
        int Course = 0;
        int Top = 0;                    // 一番上に表示している小節
        std::vector<std::pair<int, int>> Folds;     // 折りたたんでいる小節の範囲 (始まり, 終わり。どちらも含む。editorfolds.json に譜面・コースごとに保存)
        std::vector<std::pair<int, int>> OpenFolds; // 登録を残したまま開いている範囲
        int Division = 16;              // グリッドの分割数 (1 小節あたり)
        int GridPage = 0;
        char Palette = '1';
        int CursorMeasure = 0;
        int CursorSlot = 0;
        struct Snapshot { tja::Document Doc; std::vector<std::pair<int, int>> Folds; int Course = 0; std::vector<std::pair<int, int>> OpenFolds; };
        std::vector<Snapshot> Undo, Redo;
        enum class PanelType { None, Commands, Header, Grid, Estimate, Gimmick } Panel = PanelType::None;
        // 範囲選択 (Shift+クリック / Shift+矢印): 位置は小節番号 + 小節の中の分数
        bool Selecting = false;
        int SelMeasure = 0;
        int64_t SelNum = 0, SelDen = 1;
        struct ClipItem { int64_t Num = 0, Den = 1; char Type = '1'; std::string Command; };    // 選択の始まりからの位置 (小節単位の分数)
        std::vector<ClipItem> Clipboard;
        bool SavedAutoPlay = false;
        bool SelectMode = false;        // 範囲選択のモード (タッチでも範囲を選べるように。Shift+クリックと同じ)
        // 譜面の一覧から開く (Android。ファイルを選ぶ画面の代わり)
        bool Browsing = false;
        std::vector<fs::path> BrowseList;
        int BrowseTop = 0;
        // 自動推定 (BPM・開始オフセット・拍子)
        bool Estimating = false;
        bool WaveAnalyzing = false;
        std::future<std::vector<audioalign::Onset>> WaveTask;
        static constexpr int DefaultWaveToleranceMs = 10;
        double WaveToleranceMs = DefaultWaveToleranceMs;
        std::future<tempo::Result> EstimateTask;
        tempo::Result Estimate;
        bool Editing = false;           // 文字入力中
        int EditHandle = 0;
        std::string EditLabel;
        std::function<void(const std::string&)> OnEdit;
        bool FormOpen = false;
        std::string FormTitle;
        std::vector<editform::Field> FormFields;
        int FormActive = 0;
        std::function<void(const std::vector<std::string>&)> FormDone;
        std::string Message;
        std::chrono::steady_clock::time_point MessageUntil{};
        std::chrono::steady_clock::time_point ExitConfirmUntil{};
#ifndef __ANDROID__
        std::shared_ptr<FolderPickState> Picker;
#else
        bool PickerPending = false;
#endif
        int PickerKind = 0;             // 1: 開く, 2: 新規の保存先, 3: 新規の音源, 4: WAVE の変更, 5: 動画
        fs::path NewPath;
        bool Testing = false;           // テストプレイ中 (演奏画面から戻ったら元に戻す)
        bool SavedTraining = false;
        int TestMeasure = 0;
        std::string TestText;
        bool Rescan = false;            // 保存したので選曲画面の一覧を作り直す
        // 疑似再生 (編集画面のまま、カーソルの小節から音源と一緒に流す)
        bool Preview = false;
        bool PreviewJiro = false;       // Edit の譜面マップ専用 (Config とは独立)
        bool PreviewTaiko = true;
        double PreviewFrom = 0;         // 再生を始めた譜面の時刻 (ms)
        std::unordered_map<size_t,int> PreviewRollHits;
        std::unordered_map<size_t,double> PreviewRollNext;
        uint64_t PreviewRollRevision=std::numeric_limits<uint64_t>::max();
        double PreviewLast = 0;         // 前のフレームの譜面の時刻 (効果音を鳴らす範囲)
        std::chrono::steady_clock::time_point PreviewClock{};
        int SongHandle = -1;            // 読み込んだ音源 (同じ WAVE なら使い回す)
        double PreviewSpeed = 1.0;      // 疑似再生の速度
        // 速度を変えた時の音源 (音の高さは変えずに時間だけ伸び縮みさせたもの。別スレッドで作る)
        std::vector<float> SongPcm;     // デコードした音源 (インターリーブ)
        int SongPcmChannels = 0, SongPcmRate = 0;
        bool SongPcmTried = false;
        int StretchHandle = -1;
        double StretchSpeed = 0;
        std::future<std::vector<float>> StretchTask;
        double StretchTaskSpeed = 0;
        std::shared_ptr<std::atomic_bool> StretchCancel;
        bool SongStarted = false;       // この再生で音源を流し始めた (終わった後に流し直さない)
        fs::path SongPath;
    } Editor;

    static double EditorCommandValue(const std::string& text, size_t keylength);
    // 選べるグリッドの分割数 (1 小節あたり)。◀ ▶ は「よく使う分割」(Config.EditorGrids) だけを順に切り替える
    static constexpr int EditorDivisions[] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256 };
    static constexpr int EditorMaxDivision = 4096;
    static constexpr int EditorRows = 4;

    void EditorMessage(std::string text);
    tja::Course* EditorCourse();
    int EditorMeasureCount();
    // 変更の前に呼ぶ (元に戻す用に今の状態を残す)
    void EditorChange();
    void EditorClamp();

    // ---- ファイル ----
#ifndef __ANDROID__
    static std::wstring EditorFileDialog(HWND owner, bool save, bool audio, const std::wstring& folder, const std::wstring& name, bool video = false);
    void EditorPick(int kind, bool save, bool audio, const fs::path& folder, const std::wstring& name);
#else
    void EditorPick(int kind, bool, bool audio, const fs::path& folder, const std::wstring&);
#endif
    // ---- 譜面の一覧から開く ----

    float EditorBrowseRowHeight() const;
    int EditorBrowseRows() const;
    UIRect EditorBrowseRow(int row) const;
    void EditorBrowseProc(const PointerState& pt);
    void EditorBrowseDraw(const FontData& sfont);
    fs::path EditorDefaultFolder() const;
    bool EditorOpen(const fs::path& path);
    // 元の文字コードで書く (Shift_JIS で表せない文字があれば UTF-8 (BOM 付き))
    bool EditorSave();
    // SONGURL の URL から音源をダウンロードして WAVE の場所に保存する (選曲画面の SONGURL と同じ yt-dlp を使う)
    void EditorFetchSong();
    // MOVIEURL の URL から背景動画をダウンロードして BGMOVIE の場所に保存する (選曲画面の MOVIEURL と同じ)
    void EditorFetchMovie();
    // ---- 自動推定: 音源から BPM・開始オフセット・拍子を推定する (Library/TempoEstimate.hpp) ----
    std::pair<std::vector<float>, int> EditorReadMono();
    void EditorSetDemoStart();
    void EditorApplyWaveAlign(const std::vector<audioalign::Onset>& onsets, double toleranceMs);
    void EditorBeginWaveAlign();
    void EditorStartEstimate();
    void EditorApplyEstimate();
    void EditorResizeForAudioLength(double seconds);
    void EditorFitAudioLength();
    // 選んだ音源から WAVE: の値を決める。ゲームのフォルダの外にある音源は譜面のフォルダへ複製し、譜面からの相対パスで書く
    // (譜面フォルダごと持ち運んだり配布したりしても音源が見つかるように)
    std::string EditorWaveValue(const fs::path& audio, const fs::path& chartdir);
    void EditorNewChart(const fs::path& path, const fs::path& audio);
    static int CourseIndexFromName(std::string name);
    void EditorTestPlay();

    // ---- レイアウト ----
    float EditorResX() const;
    float EditorResY() const;
    // 編集画面の文字 (画面の高さに合わせた小さめの字。ボタン・ラベルが画面の幅に収まるように)
    mutable FontData EditorFontData;
    mutable int EditorFontSize = 0;
    // パネルのボタンの文字 (編集画面の文字よりひと回り大きく)
    mutable FontData EditorPanelFontData;
    mutable int EditorPanelFontSize = 0;
    const FontData& EditorPanelFont() const;
    const FontData& EditorFont() const;
    float EditorTextWidth(const std::string& str) const;
    float EditorLine() const;
    mutable float EditorButtonsBottom = 0;      // ボタンの最後の段の下端 (EditorButtons で更新)
    // 小節番号・BPM を書く列の幅
    float EditorLabelWidth() const;
    UIRect EditorGridArea() const;
    float EditorRowHeight() const;
    // ---- 折りたたみ: 折りたたんだ範囲は 1 行 (始まりの小節の行) にまとめて表示する ----
    const std::pair<int, int>* EditorFoldAt(int measure) const;
    const std::pair<int, int>* EditorOpenFoldAt(int measure) const;
    void EditorToggleFold(int measure);
    void EditorRemoveFoldRegistration(int measure);
    UIRect EditorFoldDeleteButton(int row) const;
    UIRect EditorFoldButton(int row) const;
    // 次の行の小節 (折りたたんだ範囲は飛ばす)
    int EditorNextRowMeasure(int measure) const;
    // 行 (0～EditorRows-1) の小節。範囲の外は -1
    int EditorRowMeasure(int row) const;
    // 小節が表示されている行 (表示されていなければ -1。折りたたんだ範囲の中はその行)
    int EditorMeasureRow(int measure) const;
    // 折りたたみの範囲を正しく保つ (小節の数を超えない・重ならない・一番上は範囲の始まり)
    void EditorNormalizeFolds();
    // 小節の挿入・削除では折りたたみの対象を追跡する。
    void EditorRemapFolds(int at, int removed, int added);
    // 一番上の行を dir 行動かす
    void EditorScrollRows(int dir);
    // 折りたたむ: 範囲を選んでいればその小節を、無ければ音符の無い命令だけの小節が 3 つ以上続く所 (ギミックの帯) をまとめて
    void EditorFold();
    // 小節を表示する (折りたたんでいれば開いて、見える位置までスクロール)
    void EditorReveal(int measure);
    // 小節の長さ (#MEASURE の値。4/4 = 1)
    double EditorMeasureRatio(int measure) const;
    // その小節のグリッドの数: 分割数は 4/4 の 1 小節あたりなので、#MEASURE の長さに合わせて増減する (3/4 で 16 分割なら 12)
    int EditorDivOf(int measure) const;
    float EditorStandardMeasureWidth() const;
    float EditorHorizontalOffset=0;
    void EditorScrollHorizontal(int dir);
    // カーソルの位置が見えるページ (長い小節を横に分けた表示) に切り替える
    void EditorShowCursorPage();
    // 範囲の命令を消す (すべて / 命令の種類ごと)。key が空ならすべて
    void EditorBeginDeleteCommands();
    // whole: 範囲を選んでいない時の譜面全体
    void EditorDeleteCommandsInSelection(const std::string& key, bool whole = false);
    static std::string EditorCommandKey(const std::string& text);
    // 範囲の小節の頭を音源から 1 小節ずつ求め、譜面が音源から tolerance (ms) 以上ずれる小節に #BPMCHANGE を入れる
    void EditorBeginBpmAnalyze();
    void EditorAnalyzeBpm(double tolerance, int digits, bool perBeat);
    // 太鼓さん次郎の BPM の小数以下の切り捨て (#BPMCHANGE ごとに整数ミリ秒へ切り捨てる) で早くなる分を、
    // 小節の頭の #DELAY で打ち消す (範囲を選んでいればその範囲、無ければコース全体)
    void EditorFixJiroDrift();
    // 範囲をコマ送りに見せる: 範囲を interval ごとの段に分け、段の間は疑似停止、段の終わりに distance だけ瞬間移動する
    void EditorInsertFrameStep(const std::string& params);
    // 範囲を痙攣させる: 間隔ごとに「速く前へ進む → 振れ幅だけ戻る」に作り直す (音符は元の時刻・元の速さで来ながら震える)
    void EditorInsertConvulsion(const std::string& params);
    // 範囲を別の小節に分けて作り直す (kind 0: コマ送り / 1: 痙攣)
    void EditorRebuildRange(int kind, const std::string& params);
    // 折りたたみを譜面・コースごとに editorfolds.json へ保存・読み込む (エディタを閉じる・コースを切り替える・保存する時に保存)
    void EditorStoreFolds();
    void EditorLoadFolds();
    // 音符と小節幅を同じ縮尺で描き、演奏画面の16分の間隔と音符径の比率を保つ。
    UIRect EditorLane(int row) const;
    float EditorX(const UIRect& lane, double pos) const;
    mutable std::string EditorNoteBoundsPath;
    mutable const void* EditorNoteBoundsHandle = nullptr;
    mutable std::vector<float> EditorNoteWidthFractions;
    float EditorNoteWidthPerHeight(char type) const;
    float EditorGridNoteHeight() const;

    // ボタンのまとまり (枠と見出しを描く)
    struct EditorGroup { UIRect Rect; std::string Label; };
    std::vector<EditorGroup> EditorGroups;
    std::vector<EditorButton> EditorButtons();
    // ボタンの説明 (マウスを乗せると画面下に出す)
    static std::string EditorButtonHelp(const std::string& label);
    std::vector<int> EditorGridChoices() const;
    static bool EditorDefaultGrid(int division);
    static constexpr int EditorGridsPerPage = 20;
    int EditorGridPageIndex() const;
    std::vector<int> EditorGridPanelChoices() const;
    UIRect EditorGridDeleteButton(int index) const;
    void EditorRemoveGrid(int division);
    void EditorAddGrid(int division);
    std::vector<int> EditorGridList() const;
    void EditorSelectDivision(int division);
    void EditorSetDivision(int dir);
    // ---- 範囲選択・コピー・貼り付け ----
    struct EditorPos { int Measure; int64_t Num, Den; };
    EditorPos EditorCursorPos() const;
    static int EditorComparePos(const EditorPos& a, const EditorPos& b);
    void EditorBeginSelection();
    // 選択範囲 (始まり <= 終わり。終わりの位置の音符も含む)
    std::pair<EditorPos, EditorPos> EditorSelection() const;
    bool EditorInSelection(int measure, int64_t num, int64_t den) const;
    // (measure + num/den) - (start) を小節単位の分数で
    static void EditorRelative(const EditorPos& start, int measure, int64_t num, int64_t den, int64_t& outnum, int64_t& outden);
    bool EditorCopy(bool cut);
    void EditorPaste();
    void EditorUndo(bool redo);
    void EditorClose();

    // ---- 命令・ヘッダー ----
    struct EditorCommandHit { int Measure = -1; int Index = -1; UIRect Rect; };
    std::vector<EditorCommandHit> EditorCommandRects();
    void EditorBeginEdit(std::string label, std::string initial, std::function<void(const std::string&)> done);
    std::string EditorFieldValue(int index) const;
    void EditorFinishFields(bool apply, std::vector<std::string> values = {});
    void EditorFocusField(int index);
    void EditorCycleField(int index, int direction);
    void EditorBeginFields(std::string title, std::vector<editform::Field> fields, std::function<void(const std::vector<std::string>&)> done);
    void EditorBeginGradient();
    void EditorBeginUniformScroll();
    void EditorUniformScroll(double visualBpm);
    void EditorEditCommandValues(const std::string& text, std::function<void(const std::string&)> done);
    void EditorEditGimmickField(int index);
    static const std::vector<std::pair<std::string, std::string>>& EditorCommandTemplates();
    // 位置 (measure, num/den) で効いている命令 (key で始まる最後のもの)。無ければ空
    std::string EditorCommandBefore(int measure, int64_t num, int64_t den, std::string_view key);
    void EditorBalloonPlaced(int measure, int64_t num, int64_t den, bool inserted);
    void EditorInsertCommand(const std::string& text);
    // ---- ギミック (太鼓さん次郎のギミック譜面の定番の書き方を、カーソルの位置の BPM・拍子から計算して挿入する) ----
    // 参考: 「太鼓さん次郎（無印）のギミック解説」(2reflexivity.blogspot.com) の各ギミックの構文。
    // 次郎形式 (JiroChartMode) と #HBSCROLL で、次郎と同じ見え方になる
    struct EditorFrac {
        double Num = 1, Den = 1;
        double Value() const { return Den != 0 ? Num / Den : 0; }
    };
    // "1/4" や "0.25" を分数として読む
    static std::optional<EditorFrac> EditorParseFrac(std::string_view text);
    // 小節ごとの長さ (#MEASURE)。#MEASURE を変えた後に EditorKeepNotePositions(変える前の値) を呼ぶと、
    // 音符・命令の拍の位置を保ったまま小節の長さだけを変える (伸ばした分は右に空きができ、縮めて小節の終わりからはみ出したものは消す)
    std::vector<EditorFrac> EditorMeasureFracs();
    // 分数を整数の分数にする (小数は 10 倍ずつして整数に)。0 や負の値は false
    static bool EditorToRational(const EditorFrac& f, int64_t& num, int64_t& den);
    void EditorKeepNotePositions(const std::vector<EditorFrac>& before);
    // 数値を短く書く (0.128 / 120000 / -0.004)
    static std::string EditorNum(double v);
    static std::string EditorFracText(double num, double den);

    // カーソルの小節の頭の BPM と拍子 (#MEASURE)
    struct EditorTiming { double Bpm = 120; EditorFrac Measure{ 4, 4 }; };
    std::vector<EditorTiming> EditorTimingCache;
    EditorTiming EditorTimingAfterLast;
    uint64_t EditorTimingRevision = std::numeric_limits<uint64_t>::max();
    int EditorTimingCourse = -1;
    EditorTiming EditorTimingAt(int measure);

    struct EditorGimmick {
        const char* Name;
        const char* Params;         // 入力欄の説明
        const char* Default;        // 入力欄の初期値
        const char* Help;
    };
    static const std::vector<EditorGimmick>& EditorGimmicks();
    // ギミックを TJA の行に組み立てる。戻り値: 挿入する行 (#START / #END で包んで tja::Document で読む)
    // place: 0 = カーソルの小節の前に小節として入れる, 1 = カーソルの位置に命令として入れる, 2 = コースの最後に入れる
    std::optional<std::vector<std::string>> EditorBuildGimmick(int index, const std::string& params, int& place, std::string& error);
    void EditorInsertGimmick(int index, const std::string& params);

    // ---- 疑似再生 ----
    // 小節ごとに「小節の中の位置 → 譜面の時刻」の折れ線を作る (#BPMCHANGE は途中でも効き、#DELAY は時刻を飛ばす。#MEASURE は小節の頭で効く)
    struct EditorTimeline {
        std::map<std::tuple<int,int64_t,int64_t>,int> BalloonCounts;
        struct Measure { std::vector<std::pair<double, double>> Map; };    // (位置 0～1, 時刻 ms)
        std::vector<Measure> Measures;
        struct Note { double Time; char Type; double End=0; int Hits=0; };
        std::vector<Note> Notes;        // 時刻順
        double TimeAt(int m, double pos) const {
            if (m < 0 || m >= (int)Measures.size() || Measures[m].Map.empty()) { return 0; }
            const auto& map = Measures[m].Map;
            for (size_t i = map.size(); i-- > 0;) {
                if (map[i].first <= pos) {
                    if (i + 1 < map.size() && map[i + 1].first > map[i].first) {
                        const auto& [p0, t0] = map[i];
                        const auto& [p1, t1] = map[i + 1];
                        return t0 + (t1 - t0) * (pos - p0) / (p1 - p0);
                    }
                    return map[i].second;
                }
            }
            return map.front().second;
        }
        // 時刻 t の位置 (見つからなければ false)。時刻が戻るギミックの小節は、書かれた順で後ろのものを選ぶ
        bool PosAt(double t, int& measure, double& pos) const {
            for (int m = (int)Measures.size(); m-- > 0;) {
                const auto& map = Measures[m].Map;
                for (size_t i = 0; i + 1 < map.size(); ++i) {
                    const auto& [p0, t0] = map[i];
                    const auto& [p1, t1] = map[i + 1];
                    if (p1 <= p0) { continue; }
                    if ((t0 <= t && t < t1) || (t1 < t && t <= t0)) {
                        measure = m;
                        pos = p0 + (p1 - p0) * (t - t0) / (t1 - t0);
                        return true;
                    }
                }
            }
            return false;
        }
    };
    EditorTimeline EditorBuildTimeline();
    const EditorTimeline& EditorTimelineCached();
    // 次郎形式の小節ごとの時刻 (分岐の無いコース用)。taikojiro と同じく #BPMCHANGE ごとに整数ミリ秒へ切り捨てて足す
    // truncate = false で切り捨てない本来の時刻。originOut に譜面の始まりの絶対時刻 (曲の始まりからのミリ秒) を返す
    // commandTimes: 小節ごと・命令ごとの、その命令の位置の時刻 (命令を処理する前。譜面の時刻)
    std::vector<EditorTimeline::Measure> EditorJiroMeasureMaps(bool truncate = true, double* originOut = nullptr, std::vector<std::vector<double>>* commandTimes = nullptr);
    EditorTimeline EditorTimelineCache;
    uint64_t EditorTimelineRevision = std::numeric_limits<uint64_t>::max();
    int EditorTimelineCourse = -1;
    bool EditorTimelineJiro = false;
    double EditorPreviewNow() const;
    static constexpr double EditorSpeeds[] = { 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0 };
    // 疑似再生の速度を変える (再生中なら今の位置から続ける。音源は周波数を変えるので音の高さも変わる)
    void EditorSetPreviewSpeed(int dir);
    void EditorApplyPreviewSpeed(double speed);
    // 今の速度で流す音源 (速度 1 は元の音源、それ以外は時間を伸び縮みさせた音源。まだできていなければ -1)
    int EditorActiveSong() const;
    void EditorStopSong();
    // 音源を読み込む (WAVE が変わった時だけ)
    void EditorLoadSong();
    // 速度を変えた音源を別スレッドで作り始める
    void EditorRequestStretch();
    // 別スレッドで作った音源ができていれば、再生できる形にする
    void EditorPollStretch();
    // 音源の今の位置 (速度を変えた音源ではその時間に直す。ms)
    double EditorSongPosition(double charttime) const;
    void EditorStopPreview();
    void EditorReleaseSong();
    // 読み込んだ音源を消す (疑似再生は止めない)
    void EditorFreeSong();
    void EditorTogglePreview();
    void EditorToggleJiroPreview();
    bool EditorJiroPreviewEnabled() const;
    // 毎フレーム: 音源を流し、通り過ぎた音符の効果音を鳴らし、再生位置の小節が見えるようにスクロールする
    void EditorUpdatePreview();

    UIRect EditorPanelRect() const;
    // パネルの行 (ヘッダー: 項目, 命令: テンプレート) の矩形。上のボタンより少し大きく (タッチでも押しやすく)
    UIRect EditorPanelRow(int i, int columns) const;
    // 命令・ギミックのパネル: ジャンルごとに見出しを付けて並べる
    struct EditorPanelLayout {
        std::vector<UIRect> Items;                          // 項目の番号 (一覧の順) ごとの矩形
        std::vector<std::pair<std::string, float>> Headers; // 見出しと、その文字の高さ
    };
    struct EditorPanelSection { const char* Name; std::vector<int> Items; };
    static const std::vector<EditorPanelSection>& EditorCommandSections();
    static const std::vector<EditorPanelSection>& EditorGimmickSections();
    EditorPanelLayout EditorSectionLayout(const std::vector<EditorPanelSection>& sections, size_t count, int columns) const;
    EditorPanelLayout EditorCommandLayout() const;
    EditorPanelLayout EditorGimmickLayout() const;
    struct EditorHeaderField { const char* Key; bool CourseField; const char* Help; };
    static const std::vector<EditorHeaderField>& EditorHeaderFields();

    // ---- シーン ----
    void EditorInit();
    void EditorEnd();

    void EditorDraw();
    void DrawTextInput(int x,int y,int handle);
    void EditorDrawKeyInput(int x, int y, int handle);
    void EditorDrawFields();
    UIRect EditorFieldsRect() const;
    UIRect EditorFieldRect(int index) const;
    UIRect EditorFieldsButton(int index) const;
    // 文字入力のダイアログ (ヘッダーや自由入力)
    UIRect EditorEditRect() const;
    UIRect EditorEditBox() const;
    UIRect EditorEditButton(int i) const;
    UIRect EditorStartButton(int i) const;
    UIRect EditorWaveButton(int row) const;
    UIRect EditorHeaderVisibilityButton(int row) const;
    void EditorToggleHeaderVisibility(const char* key);
    UIRect EditorHeaderButton(int i) const;

    static unsigned int EditorNoteColor(char type);
    // スキンの音符画像 (Playing.Image.Note の分割) の番号: 1 ドン 2 カッ 3 大ドン 4 大カッ 5 連打 6 連打の帯 7 連打の終わり
    // 8 大連打 9 大連打の帯 10 大連打の終わり 11 風船 13 くすだま
    static int EditorNoteImage(char type);
    // 音符の画像を枠の高さに合わせた大きさで描く (画像の 1 コマは大音符が収まる大きさなので、レーンの高さの 1.2 倍にする)
    bool EditorDrawSkinNote(float x, float cy, float lane_h, int index, int alpha);
    void EditorDrawNote(float x, float cy, float lane_h, char type, int alpha);
    // ボタンの絵 (図形で描く。音符はスキンの画像)
    void EditorDrawIcon(const EditorButton& b, unsigned int color);
    // 絵のボタンの図形を直接描く (EditorDrawIcon が 4 倍の解像度で描いて縮小した画像を作る時に使う)
    void EditorDrawIconShape(const EditorButton& b, unsigned int color, bool text = true);
    // 連打の帯 (スキンの帯の画像を伸ばす。無ければ単色)
    void EditorDrawRollBody(float x0, float x1, float cy, float lane_h, char type);
    void EditorDrawGrid(const FontData& sfont);
    // マウスの下の小節とグリッドの位置 (無ければ false)
    bool EditorHitLane(int x, int y, int& measure, int& slot);

    void EditorProc();
#endif // FAS_GAME_MEMBER_DECLARATIONS
