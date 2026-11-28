#include "../Game.hpp"
#include "../Library/EditorLayout.hpp"
#include "../Library/PreviewRoll.hpp"
#include "../Library/UniformScroll.hpp"
double Game::EditorCommandValue(const std::string& text, size_t keylength) {
    if (text.size() <= keylength) { return 0.0; }
    return std::atof(std::string(tja::Trim(std::string_view(text).substr(keylength))).c_str());
}

void Game::EditorMessage(std::string text) {
    Editor.Message = std::move(text);
    Editor.MessageUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
}

tja::Course* Game::EditorCourse() {
    return Editor.Open && Editor.Course >= 0 && Editor.Course < (int)Editor.Doc.Courses.size() ? &Editor.Doc.Courses[Editor.Course] : nullptr;
}

int Game::EditorMeasureCount() {
    const auto* c = EditorCourse();
    return c != nullptr ? (int)c->Measures.size() : 0;
}

void Game::EditorChange() {
    ++Editor.Revision;
    EditorStopPreview();
    Editor.Undo.push_back({ Editor.Doc, Editor.Folds, Editor.Course, Editor.OpenFolds });
    if (Editor.Undo.size() > 200) { Editor.Undo.erase(Editor.Undo.begin()); }
    Editor.Redo.clear();
    Editor.Dirty = true;
}

void Game::EditorClamp() {
    const int n = std::max(EditorMeasureCount(), 1);
    Editor.Course = std::clamp(Editor.Course, 0, std::max((int)Editor.Doc.Courses.size() - 1, 0));
    Editor.CursorMeasure = std::clamp(Editor.CursorMeasure, 0, n - 1);
    Editor.CursorSlot = std::clamp(Editor.CursorSlot, 0, EditorDivOf(Editor.CursorMeasure) - 1);
    Editor.Top = std::clamp(Editor.Top, 0, std::max(n - 1, 0));
    EditorNormalizeFolds();
    EditorReveal(Editor.CursorMeasure);     // カーソルの小節は開いて見えるようにする
    EditorShowCursorPage();                 // 横のページもカーソルの位置に合わせる
}

#ifndef __ANDROID__
std::wstring Game::EditorFileDialog(HWND owner, bool save, bool audio, const std::wstring& folder, const std::wstring& name, bool video) {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    struct ComScope { HRESULT Init; ~ComScope(){if(SUCCEEDED(Init))CoUninitialize();} } com{init};
    std::wstring result;
    IFileDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        const auto releaseDialog=[](IFileDialog* p){p->Release();};
        std::unique_ptr<IFileDialog,decltype(releaseDialog)> dialogGuard(dialog,releaseDialog);
        const COMDLG_FILTERSPEC tjafilter[] = { { L"譜面 (*.tja;*.osu)", L"*.tja;*.osu" } };
        const COMDLG_FILTERSPEC tjasavefilter[] = { { L"TJA譜面 (*.tja)", L"*.tja" } };
        const COMDLG_FILTERSPEC audiofilter[] = { { L"音源 (*.ogg;*.wav;*.mp3)", L"*.ogg;*.wav;*.mp3" } };
        const COMDLG_FILTERSPEC videofilter[] = { { L"動画 (*.mp4;*.webm;*.avi;*.wmv)", L"*.mp4;*.webm;*.avi;*.wmv" } };
        dialog->SetFileTypes(1, video ? videofilter : audio ? audiofilter : save ? tjasavefilter : tjafilter);
        if (!audio) { dialog->SetDefaultExtension(L"tja"); }
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | (save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST)
            | (save && !audio && !video ? FOS_STRICTFILETYPES : 0));
        if (!name.empty()) { dialog->SetFileName(name.c_str()); }
        if (!folder.empty()) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(folder.c_str(), nullptr, IID_PPV_ARGS(&item)))) {
                dialog->SetFolder(item);
                item->Release();
            }
        }
        if (SUCCEEDED(dialog->Show(owner))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                const auto releaseItem=[](IShellItem* p){p->Release();};
                std::unique_ptr<IShellItem,decltype(releaseItem)> itemGuard(item,releaseItem);
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr) {
                    const auto freePath=[](wchar_t* p){CoTaskMemFree(p);};
                    std::unique_ptr<wchar_t,decltype(freePath)> pathGuard(path,freePath);
                    result = path;
                }
            }
        }
    }

    return result;
}

void Game::EditorPick(int kind, bool save, bool audio, const fs::path& folder, const std::wstring& name) {
    if (Editor.Picker) { return; }
    if (Config.FullScreen) {
        EditorMessage("フルスクリーンではファイルを選べません (ウィンドウモードにしてください)");
        return;
    }
    auto state = std::make_shared<FolderPickState>();
    Editor.Picker = state;
    Editor.PickerKind = kind;
    const HWND owner = ((HWND)sys::NativeWindow());
    const std::wstring dir = folder.wstring();
    std::thread([state, owner, save, audio, dir, name, kind] {
        try { state->Result = EditorFileDialog(owner, save, audio, dir, name, kind == 5); }
        catch (...) { state->Result.clear(); }
        state->Done = true;
        }).detach();
}

#endif
#ifndef __ANDROID__
#else
void Game::EditorPick(int kind, bool, bool audio, const fs::path& folder, const std::wstring&) {
    if (Editor.PickerPending) { return; }
    const fs::path dest = kind == 1 ? GetExecutablePath() / "Songs" / "Imported" : folder;
    Editor.PickerKind = kind;
    Editor.PickerPending = AndroidStartEditorPicker(kind == 5 ? "video/*" : audio ? "audio/*" : "*/*", PathToU8(dest));
    if (!Editor.PickerPending) { EditorMessage("ファイル選択を開けませんでした"); }
}

#endif
float Game::EditorBrowseRowHeight() const { return EditorLine() * 2.2f; }

int Game::EditorBrowseRows() const { return std::max(1, (int)((EditorResY() - EditorLine() * 5.0f) / EditorBrowseRowHeight())); }

auto Game::EditorBrowseRow(int row) const -> UIRect {
    const float top = EditorLine() * 2.5f, h = EditorBrowseRowHeight();
    return { 24.0f, top + h * row, EditorResX() - 24.0f, top + h * row + h - 6.0f };
}

void Game::EditorBrowseProc(const PointerState& pt) {
    const int n = (int)Editor.BrowseList.size(), rows = EditorBrowseRows();
    Editor.BrowseTop = std::clamp(Editor.BrowseTop - pt.Wheel * 2, 0, std::max(0, n - rows));
    for (int r = 0; r < rows && Editor.BrowseTop + r < n; ++r) {
        if (pt.LeftPressed && EditorBrowseRow(r).Contains(pt.X, pt.Y)) {
            Editor.Browsing = false;
            Skin.Base->Title.SE.Don.Play();
            EditorOpen(Editor.BrowseList[Editor.BrowseTop + r]);
            return;
        }
    }
    const UIRect back{ EditorResX() - 200.0f, 8.0f, EditorResX() - 24.0f, EditorLine() * 2.0f };
    if (pt.LeftPressed && back.Contains(pt.X, pt.Y)) { Editor.Browsing = false; }
#ifndef __ANDROID__
    Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] { Editor.Browsing = false; });
#endif
}

void Game::EditorBrowseDraw(const FontData& sfont) {
    const unsigned int white = gfx::Rgb(255, 255, 255), gray = gfx::Rgb(150, 150, 150);
    UIText(sfont, 24, EditorLine(), "開く譜面を選んでください (上下にドラッグでスクロール)", white);
    const UIRect back{ EditorResX() - 200.0f, 8.0f, EditorResX() - 24.0f, EditorLine() * 2.0f };
    UIBox(back, gfx::Rgb(60, 64, 84), 230);
    UIText(sfont, (back.L + back.R) * 0.5f, (back.T + back.B) * 0.5f, "戻る", white, 1);
    const int n = (int)Editor.BrowseList.size(), rows = EditorBrowseRows();
    for (int r = 0; r < rows && Editor.BrowseTop + r < n; ++r) {
        const UIRect row = EditorBrowseRow(r);
        const fs::path& p = Editor.BrowseList[Editor.BrowseTop + r];
        UIBox(row, gfx::Rgb(44, 48, 64), 230);
        UIText(sfont, row.L + 12, row.T + (row.B - row.T) * 0.33f, FitText(PathToU8(p.stem()), sfont.Handle, row.R - row.L - 24, false), white);
        UIText(sfont, row.L + 12, row.T + (row.B - row.T) * 0.75f, FitText(PathToU8(p.parent_path().filename()), sfont.Handle, row.R - row.L - 24, false), gray);
    }
    UIText(sfont, EditorResX() - 24, EditorResY() - EditorLine(), std::to_string(Editor.BrowseTop + 1) + "-" + std::to_string(std::min(n, Editor.BrowseTop + rows)) + " / " + std::to_string(n), gray, 2);
}

fs::path Game::EditorDefaultFolder() const {
    if (!Editor.Path.empty()) { return Editor.Path.parent_path(); }
    if (!Config.SongDirectories.empty()) { return ResolveSongDirectory(Config.SongDirectories.front()); }
    return GetExecutablePath();
}

bool Game::EditorOpen(const fs::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        EditorMessage("譜面を開けませんでした: " + PathToU8(path.filename()));
        return false;
    }
    std::string raw((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (raw.starts_with("\xEF\xBB\xBF")) { raw.erase(0, 3); }
    TextLines text;
    text.LoadFromMemory(raw);
    ++Editor.Revision;
    if (text.lines.empty()) { EditorMessage("譜面を読み込めませんでした (osu!taiko は Mode:1 に対応)"); return false; }
    Editor.Doc = tja::Document::Parse(text.lines);
    if (Editor.Doc.Courses.empty()) {
        tja::Course course;
        course.Header = { "COURSE:Oni", "LEVEL:1" };
        course.Measures.resize(16);
        Editor.Doc.Courses.push_back(std::move(course));
    }
    // 小節の無いコース (#START の直後に #END) でもカーソルを置けるよう 1 小節は用意する
    for (auto& course : Editor.Doc.Courses) {
        if (course.Measures.empty()) { course.Measures.resize(1); }
    }
    Editor.WasSjis = !raw.empty() && !is_valid_utf8(raw);
    Editor.Path = path;
    if (ToLower(PathToU8(path.extension())) == ".osu") {
        Editor.WasSjis = false;
        Editor.Path += ".tja";
        std::error_code ec;
        for (int suffix = 1; fs::exists(Editor.Path, ec); ++suffix) {
            Editor.Path = path; Editor.Path += "." + std::to_string(suffix) + ".tja";
        }
    }
    Editor.Open = true;
    // #HBSCROLL / #BMSCROLL の譜面はゲームでも次郎形式で遊ぶことが多いので、最初から次郎形式で流す
    Editor.PreviewJiro = (Config.JiroChartMode || std::ranges::any_of(Editor.Doc.Courses, [](const tja::Course& c) {
        return std::ranges::any_of(c.Header, [](const std::string& h) { return tja::StartsWithKey(h, "#HBSCROLL") || tja::StartsWithKey(h, "#BMSCROLL"); });
    })) && tja::Document::GetValue(Editor.Doc.Header, "FASOSUFORMAT").empty();
    Editor.Dirty = false;
    Editor.Undo.clear();
    Editor.Redo.clear();
    Editor.Course = 0;
    // 鬼のコースがあれば最初に開く
    for (int i = 0; i < (int)Editor.Doc.Courses.size(); ++i) {
        if (ToLower(Editor.Doc.CourseName(i)) == "oni" || Editor.Doc.CourseName(i) == "3") { Editor.Course = i; break; }
    }
    Editor.Top = Editor.CursorMeasure = Editor.CursorSlot = 0;
    EditorLoadFolds();      // 前に折りたたんでいた範囲を戻す
    Editor.Panel = EditorState::PanelType::None;
    EditorMessage("開きました: " + PathToU8(path.filename()));
    return true;
}

bool Game::EditorSave() {
    if (!Editor.Open || Editor.Path.empty()) { return false; }
    EditorStoreFolds();
    std::string utf8;
    try { utf8 = Editor.Doc.Write(); }
    catch (const std::exception& e) { EditorMessage(std::string("保存できません: ") + e.what()); return false; }
    std::string bytes;
    bool sjis = false;
#ifndef __ANDROID__
    if (Editor.WasSjis) {
        const int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
        std::wstring wide(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), wide.data(), wlen);
        BOOL lossy = FALSE;
        const int slen = WideCharToMultiByte(932, WC_NO_BEST_FIT_CHARS, wide.data(), wlen, nullptr, 0, nullptr, &lossy);
        if (!lossy && slen > 0) {
            bytes.resize(slen);
            WideCharToMultiByte(932, WC_NO_BEST_FIT_CHARS, wide.data(), wlen, bytes.data(), slen, nullptr, &lossy);
            sjis = !lossy;
        }
    }
#endif
    if (!sjis) { bytes = "\xEF\xBB\xBF" + utf8; }
    std::error_code ec;
    // 初めて上書きする時は元のファイルを .bak として残す
    if (fs::exists(Editor.Path, ec)) {
        fs::path backup = Editor.Path;
        backup += ".bak";
        if (!fs::exists(backup, ec)) { fs::copy_file(Editor.Path, backup, ec); }
    }
    fs::path temp = Editor.Path;
    temp += ".tmp";
    {
        std::ofstream ofs(temp, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            EditorMessage("保存できませんでした (書き込みできない場所です)");
            return false;
        }
        ofs.write(bytes.data(), (std::streamsize)bytes.size());
        if (!ofs) {
            EditorMessage("保存できませんでした");
            return false;
        }
    }
    fs::rename(temp, Editor.Path, ec);
    if (ec) {
        fs::remove(temp, ec);
        EditorMessage("保存できませんでした (ファイルを使用中の可能性があります)");
        return false;
    }
    Editor.Dirty = false;
    Editor.WasSjis = sjis;
    Editor.Rescan = true;
    EditorMessage("保存しました: " + PathToU8(Editor.Path.filename()) + (sjis ? " (Shift_JIS)" : " (UTF-8)"));
    return true;
}

void Game::EditorFetchSong() {
    std::string link(strtrim(tja::Document::GetValue(Editor.Doc.Header, "SONGURL")));
    if (link.empty()) { EditorMessage("SONGURL に音源の URL を入力してください"); return; }
    std::string wave(strtrim(tja::Document::GetValue(Editor.Doc.Header, "WAVE")));
    if (wave.empty()) {
        // WAVE が空なら「譜面の名前.ogg」に保存する
        wave = PathToU8(Editor.Path.stem()) + ".ogg";
        EditorChange();
        tja::Document::SetValue(Editor.Doc.Header, "WAVE", wave);
    }
    const fs::path target = (Editor.Path.parent_path() / U8ToPath(wave)).lexically_normal();
    std::error_code ec;
    if (fs::exists(target, ec)) { EditorMessage("音源は既にあります: " + PathToU8(target.filename()) + " (取り直す時はファイルを消してから)"); return; }
    SongDownload(link, target);
#ifndef __ANDROID__
    Input::GetInstance().ResyncKeys();
#endif
    PointerPrev.Left = PointerPrev.Right = true;
    EditorMessage(fs::exists(target, ec) ? "音源を保存しました: " + PathToU8(target.filename()) : std::string("音源を保存できませんでした"));
}

void Game::EditorFetchMovie() {
#ifndef __ANDROID__
    std::string link(strtrim(tja::Document::GetValue(Editor.Doc.Header, "MOVIEURL")));
    if (link.empty()) { EditorMessage("MOVIEURL に動画の URL を入力してください"); return; }
    std::string movie(strtrim(tja::Document::GetValue(Editor.Doc.Header, "BGMOVIE")));
    if (movie.empty()) {
        movie = PathToU8(Editor.Path.stem()) + ".avi";
        EditorChange();
        tja::Document::SetValue(Editor.Doc.Header, "BGMOVIE", movie);
    }
    const fs::path target = (Editor.Path.parent_path() / U8ToPath(movie)).lexically_normal();
    std::error_code ec;
    if (fs::exists(target, ec)) { EditorMessage("動画は既にあります: " + PathToU8(target.filename()) + " (取り直す時はファイルを消してから)"); return; }
    MovieDownload(link, target);
    Input::GetInstance().ResyncKeys();
    PointerPrev.Left = PointerPrev.Right = true;
    EditorMessage(fs::exists(target, ec) ? "動画を保存しました: " + PathToU8(target.filename()) : std::string("動画を保存できませんでした"));
#endif
}

std::pair<std::vector<float>, int> Game::EditorReadMono() {
    const std::string wave(strtrim(tja::Document::GetValue(Editor.Doc.Header, "WAVE")));
    const fs::path path = (Editor.Path.parent_path() / U8ToPath(wave)).lexically_normal();
    std::error_code ec;
    if (wave.empty() || !fs::is_regular_file(path, ec)) { EditorMessage("WAVE の音源が見つかりません (ヘッダーで音源を指定してください)"); return {}; }
    // 音源をデコードし (mp3 の前後の無音を除く)、解析は別スレッドで行う
    const auto pcm = LoadAlignedPcm(PathToU8(path));
    if (!pcm) { EditorMessage("音源を読み込めませんでした"); return {}; }
    const int channels = pcm->Channels;
    std::vector<float> mono;
    if (pcm->Frames() > 0 && channels > 0) {
        mono = tempo::PrepareMono((size_t)pcm->Frames(), channels, [&](size_t frame, int channel) { return pcm->Samples[frame * channels + channel]; });
    }
    if (mono.empty()) { EditorMessage("この音源の形式は解析できません"); return {}; }
    return { std::move(mono), pcm->Rate };
}

void Game::EditorSetDemoStart() {
    if (!EditorCourse()) { return; }
    const auto& timeline = EditorTimelineCached();
    const double chart = timeline.TimeAt(Editor.CursorMeasure, (double)Editor.CursorSlot / EditorDivOf(Editor.CursorMeasure));
    const double offset = std::atof(tja::Document::GetValue(Editor.Doc.Header, "OFFSET").c_str());
    const double seconds = chart / 1000.0 - offset;
    if (!std::isfinite(seconds) || seconds < 0) { EditorMessage("現在位置は音源開始前のためDEMOSTARTに設定できません"); return; }
    EditorChange();
    tja::Document::SetValue(Editor.Doc.Header, "DEMOSTART", EditorNum(seconds));
    EditorMessage("DEMOSTART: " + FormatNumber(seconds, 3) + " 秒 (" + std::to_string(Editor.CursorMeasure + 1) + " 小節目 / グリッド " + std::to_string(Editor.CursorSlot) + ")");
}

void Game::EditorApplyWaveAlign(const std::vector<audioalign::Onset>& onsets, double toleranceMs) {
    auto* course = EditorCourse();
    if (!course || onsets.empty()) { EditorMessage("補正に使える打音が見つかりませんでした"); return; }
    const auto& timeline = EditorTimelineCached();
    double last = -std::numeric_limits<double>::infinity();
    for (const auto& measure : timeline.Measures) for (const auto& point : measure.Map) {
        if (!std::isfinite(point.second) || point.second < last - 1e-6) { EditorMessage("分岐・逆走・負のDELAYを含むコースの波形補正には対応していません"); return; }
        last = point.second;
    }
    const double offset = std::atof(tja::Document::GetValue(Editor.Doc.Header, "OFFSET").c_str());
    struct Source { int Measure; tja::Note Note; };
    std::vector<Source> sources;
    std::vector<double> times;
    std::vector<bool> movable;
    for (int m = 0; m < (int)course->Measures.size(); ++m) for (const auto& note : course->Measures[m].Notes) {
        const double time = timeline.TimeAt(m, (double)note.Num / note.Den) / 1000.0 - offset;
        sources.push_back({ m, note }); times.push_back(time);
        movable.push_back(time >= 0 && note.Type >= '1' && note.Type <= '4' && (!Editor.Selecting || EditorInSelection(m, note.Num, note.Den)));
    }
    const auto snapped = audioalign::Snap(times, movable, onsets, toleranceMs / 1000.0);
    auto proposed = Editor.Doc;
    auto& target = proposed.Courses[Editor.Course];
    struct Pending { tja::Note Note; double Position; bool Move; size_t Source; };
    std::vector<std::vector<Pending>> pending(target.Measures.size());
    std::vector<bool> touched(target.Measures.size(), false);
    int changed = 0;
    for (size_t i = 0; i < sources.size(); ++i) {
        int measure = sources[i].Measure;
        auto note = sources[i].Note;
        double desired = (double)note.Num / note.Den;
        bool move = false;
        if (movable[i] && std::abs(snapped[i] - times[i]) > 0.001) {
            int destination = 0; double position = 0;
            if (timeline.PosAt((snapped[i] + offset) * 1000.0, destination, position)) {
                if (destination >= 0 && destination < (int)pending.size()) {
                    touched[measure] = touched[destination] = true;
                    measure = destination; desired = position; move = true;
                }
            }
        }
        pending[measure].push_back({note, desired, move, i});
    }
    for (int m = 0; m < (int)target.Measures.size(); ++m) {
        if (!touched[m]) continue;
        auto& measure = target.Measures[m];
        auto& notes = pending[m];
        std::vector<double> positions;
        std::vector<size_t> indices;
        int64_t fixed = 1;
        auto include = [&](int64_t den) {
            constexpr int64_t limit = 1024 * 1024;
            if (den <= 0 || den > limit || fixed / std::gcd(fixed, den) > limit / den) return false;
            fixed = fixed / std::gcd(fixed, den) * den; return true;
        };
        for (const auto& c : measure.Commands) if (c.Num < c.Den && !include(c.Den)) { EditorMessage("命令の分割数が保存上限を超えています"); return; }
        for (size_t j = 0; j < notes.size(); ++j) {
            if (notes[j].Move) { positions.push_back(notes[j].Position); indices.push_back(j); }
            else if (!include(notes[j].Note.Den)) { EditorMessage("音符の分割数が保存上限を超えています"); return; }
        }
        const auto fit = audioalign::FitGrid(positions, fixed,
            [&](double pos) { return timeline.TimeAt(m, pos); },
            [&](size_t i, int64_t num, int64_t den) {
                if (Editor.Selecting && !EditorInSelection(m, num, den)) return false;
                for (size_t j = 0; j < notes.size(); ++j) if (!notes[j].Move) {
                    const auto& n = notes[j].Note;
                    const int cmp = tja::ComparePos(num, den, n.Num, n.Den);
                    if ((j < indices[i] && cmp <= 0) || (j > indices[i] && cmp >= 0)) return false;
                }
                return true;
            });
        if (!fit) { EditorMessage("精度・音符の順序・選択範囲を保てるグリッドがないため適用しませんでした"); return; }
        for (size_t i = 0; i < indices.size(); ++i) {
            auto& entry = notes[indices[i]];
            entry.Note.Num = fit->Slots[i]; entry.Note.Den = fit->Division;
            tja::Reduce(entry.Note.Num, entry.Note.Den);
            const auto& original = sources[entry.Source];
            if (m != original.Measure || tja::ComparePos(entry.Note.Num,entry.Note.Den,original.Note.Num,original.Note.Den) != 0) ++changed;
        }
        measure.Resolution = 1;
        measure.Notes.clear();
        for (const auto& entry : notes) measure.Notes.push_back(entry.Note);
    }
    for (auto& measure : target.Measures) {
        std::stable_sort(measure.Notes.begin(), measure.Notes.end(), [](const auto& a, const auto& b) { return tja::ComparePos(a.Num, a.Den, b.Num, b.Den) < 0; });
        for (size_t i = 1; i < measure.Notes.size(); ++i) {
            const auto& a = measure.Notes[i - 1]; const auto& b = measure.Notes[i];
            if (tja::ComparePos(a.Num, a.Den, b.Num, b.Den) == 0) { EditorMessage("補正先の音符が重なるため適用しませんでした。範囲を狭めて試してください"); return; }
        }
    }
    if (!changed) { EditorMessage("補正範囲内に移動できる音符はありませんでした"); return; }
    try { proposed.Write(); }
    catch (const std::exception&) { EditorMessage("補正後の分割数が保存上限を超えるため適用しませんでした"); return; }
    EditorChange(); Editor.Doc = std::move(proposed); EditorClamp();
    EditorMessage(std::to_string(changed) + " 音を波形の打音へ補正しました (配色・音符数を維持 / Ctrl+Zで戻せます)");
}

void Game::EditorBeginWaveAlign() {
    EditorBeginFields("波形に音符を合わせる (ドン／カッのみ・未選択時はコース全体)",
        { { "補正範囲 (前後ms)", std::to_string(EditorState::DefaultWaveToleranceMs) } }, [this](const auto& values) {
            int tolerance = 0; const auto text = tja::Trim(values.front());
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), tolerance);
            if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size() || tolerance < 10 || tolerance > 1000) { EditorMessage("補正範囲は10～1000msの整数で指定してください"); return; }
            auto [samples, rate] = EditorReadMono();
            if (samples.empty()) { return; }
            EditorStopPreview(); Editor.WaveToleranceMs = tolerance; Editor.WaveAnalyzing = true;
            Editor.WaveTask = std::async(std::launch::async, [samples = std::move(samples), rate] { return audioalign::Detect(samples.data(), samples.size(), rate); });
        });
}

void Game::EditorStartEstimate() {
    if (Editor.Estimating) { return; }
    auto [mono, rate] = EditorReadMono();
    if (mono.empty()) { return; }
    Editor.Estimating = true;
    Editor.EstimateTask = std::async(std::launch::async, [samples = std::move(mono), rate] {
        return tempo::Estimate(samples.data(), samples.size(), rate);
    });
}

void Game::EditorApplyEstimate() {
    const auto& r = Editor.Estimate;
    EditorChange();
    const double bpm = std::abs(r.Bpm - std::round(r.Bpm)) < 1e-6 ? std::round(r.Bpm) : r.Bpm;
    tja::Document::SetValue(Editor.Doc.Header, "BPM", EditorNum(bpm));
    tja::Document::SetValue(Editor.Doc.Header, "OFFSET", EditorNum(-r.FirstDownbeat));
    // 拍子: 4 拍子以外なら各コースの最初の小節に #MEASURE を入れる (前の自動推定の #MEASURE は置き換える)
    for (auto& course : Editor.Doc.Courses) {
        if (course.Measures.empty()) { continue; }
        auto& cmds = course.Measures.front().Commands;
        std::erase_if(cmds, [](const tja::Command& c) { return c.Num == 0 && tja::StartsWithKey(c.Text, "#MEASURE"); });
        if (r.BeatsPerMeasure != 4) {
            tja::Document::AddCommand(course.Measures.front(), 0, 1, "#MEASURE " + std::to_string(r.BeatsPerMeasure) + "/4");
        }
    }
    EditorMessage("BPM " + FormatNumber(bpm, bpm == std::floor(bpm) ? 0 : 2) + " / OFFSET " + FormatNumber(-r.FirstDownbeat, 3) + " / " + std::to_string(r.BeatsPerMeasure) + "/4 にしました (目安なので、テストプレイで確かめてください)");
}

void Game::EditorResizeForAudioLength(double seconds) {
    auto* course = EditorCourse();
    if (!course || course->Measures.empty() || !std::isfinite(seconds) || seconds <= 0) { EditorMessage("音源長を取得できませんでした"); return; }
    const auto& timeline = EditorTimelineCached();
    double target = seconds * 1000 + std::atof(tja::Document::GetValue(Editor.Doc.Header, "OFFSET").c_str()) * 1000;
    int estimated = 0;
    double end = 0;
    for (int i = 0; i < (int)timeline.Measures.size(); ++i) {
        const auto& map = timeline.Measures[i].Map;
        if (map.empty()) { return; }
        for (size_t k = 1; k < map.size(); ++k) {
            if (map[k].second < map[k - 1].second) { EditorMessage("逆走や負の DELAY を含むコースでは小節数を推定できません"); return; }
        }
        end = timeline.TimeAt(i, 1);
        if (!estimated && target <= end + 1e-6) { estimated = i + 1; }
    }
    if (!estimated) {
        const auto tail = EditorTimingAt((int)course->Measures.size());
        const double length = 240000 * tail.Measure.Value() / tail.Bpm;
        if (!std::isfinite(length) || length <= 0) { EditorMessage("正の BPM・拍子で小節数を推定してください"); return; }
        const double count = course->Measures.size() + std::ceil((target - end) / length - 1e-9);
        if (!std::isfinite(count) || count > 100000) { EditorMessage("推定小節数が上限 (100000) を超えています"); return; }
        estimated = std::max(1, (int)count);
    }
    int keep = estimated;
    for (int i = (int)course->Measures.size(); i-- > estimated;) {
        if (!course->Measures[i].Notes.empty() || !course->Measures[i].Commands.empty()) { keep = i + 1; break; }
    }
    const int old = (int)course->Measures.size();
    if (keep != old) {
        EditorChange();
        EditorRemapFolds(std::min(keep, old), std::max(old - keep, 0), std::max(keep - old, 0));
        course->Measures.resize(keep); EditorClamp();
    }
    EditorMessage("音源 " + FormatNumber(seconds, 2) + " 秒: 推定 " + std::to_string(estimated) + " 小節 / 適用 " + std::to_string(keep) + " 小節" + (keep > estimated ? " (内容のある小節を保護)" : ""));
}

void Game::EditorFitAudioLength() {
    const auto wave = tja::Document::GetValue(Editor.Doc.Header, "WAVE");
    if (wave.empty()) { EditorMessage("ヘッダーで音源を指定してください"); return; }
    const auto path = (Editor.Path.parent_path() / U8ToPath(wave)).lexically_normal();
    const auto pcm = LoadAlignedPcm(PathToU8(path));
    if (!pcm) { EditorMessage("音源を読み込めませんでした"); return; }
    EditorResizeForAudioLength(pcm->Rate > 0 ? (double)pcm->Frames() / pcm->Rate : 0);
}

std::string Game::EditorWaveValue(const fs::path& audio, const fs::path& chartdir) {
    std::error_code ec;
    auto inside = [&](const fs::path& file, const fs::path& dir) {
        const fs::path rel = fs::weakly_canonical(file, ec).lexically_relative(fs::weakly_canonical(dir, ec));
        return !rel.empty() && *rel.begin() != "..";
    };
    fs::path source = audio;
    if (!inside(audio, GetExecutablePath())) {
        fs::path dest = chartdir / audio.filename();
        // 同じ名前の別のファイルがあれば名前を変える
        for (int n = 2; fs::exists(dest, ec) && fs::file_size(dest, ec) != fs::file_size(audio, ec); ++n) {
            dest = chartdir / (audio.stem().wstring() + L"_" + std::to_wstring(n) + audio.extension().wstring());
        }
        if (!fs::exists(dest, ec)) { fs::copy_file(audio, dest, ec); }
        if (!ec && fs::exists(dest, ec)) {
            source = dest;
            EditorMessage("音源をゲームのフォルダの外から選んだので、譜面のフォルダに複製しました: " + PathToU8(dest.filename()));
        }
        else {
            EditorMessage("音源を譜面のフォルダに複製できませんでした (元の場所を絶対パスで書きます)");
        }
    }
    const fs::path rel = fs::relative(source, chartdir, ec);
    return PathToU8(ec || rel.empty() ? source : rel);
}

void Game::EditorNewChart(const fs::path& path, const fs::path& audio) {
    if (ToLower(PathToU8(path.extension())) != ".tja") {
        EditorMessage("新規作成は TJA 形式 (*.tja) を指定してください");
        return;
    }
    std::string wave;
    if (!audio.empty()) {
        wave = EditorWaveValue(audio, path.parent_path());
    }
    ++Editor.Revision;
    Editor.Doc = tja::Document::NewChart(PathToU8(path.stem()), wave);
    Editor.Path = path;
    Editor.Open = true;
    Editor.PreviewJiro = Config.JiroChartMode;
    Editor.WasSjis = false;
    Editor.Undo.clear();
    Editor.Redo.clear();
    Editor.Course = 0;
    Editor.Top = Editor.CursorMeasure = Editor.CursorSlot = 0;
    Editor.Folds.clear();
    Editor.OpenFolds.clear();
    if (EditorSave()) { EditorMessage("新しい譜面を作りました: " + PathToU8(path.filename())); }
}

int Game::CourseIndexFromName(std::string name) {
    name = ToLower(std::string(strtrim(name)));
    static const char* names[] = { "easy", "normal", "hard", "oni", "edit", "tower", "dan" };
    for (int i = 0; i < 7; ++i) { if (name == names[i]) { return i; } }
    if (name == "ura") { return 4; }
    int num = -1;
    std::from_chars(name.data(), name.data() + name.size(), num);
    return num >= 0 && num < (int)CourseType::Count ? num : (int)CourseType::Oni;
}

void Game::EditorTestPlay() {
    if (!Editor.Open || Editor.Path.empty()) { return; }
    EditorStopPreview();
    try { Editor.TestText = Editor.Doc.Write(); }
    catch (const std::exception& e) { EditorMessage(std::string("テストプレイできません: ") + e.what()); return; }
    Editor.TestMeasure = Editor.CursorMeasure;
    Editor.Testing = true;
    Editor.SavedTraining = Config.TrainingMode;
    Editor.SavedAutoPlay = Config.AutoPlay;
    Config.TrainingMode = false;     // 開始位置は編集カーソルで固定し、演奏画面では移動させない
    // 次郎形式は疑似再生の「次郎形式再生」と同じにする (時刻の計算をそろえる。編集画面に戻ると元の設定に戻る)
    if (!Config.JiroChartModeSaved) { Config.JiroChartModeSaved = Config.JiroChartMode; }
    Config.JiroChartMode = EditorJiroPreviewEnabled();
    CourseIndex = (size_t)CourseIndexFromName(Editor.Doc.CourseName(Editor.Course));
    Training.Init();
    PrevScene = Scene::Editor;
    NowScene = Scene::Loading;
}

float Game::EditorResX() const { return (float)Skin.Info.Resolution.X; }

float Game::EditorResY() const { return (float)Skin.Info.Resolution.Y; }

const FontData& Game::EditorPanelFont() const {
    const int size = std::clamp((int)std::lround(EditorResY() / 30.0f), 16, 44);
    if (size != EditorPanelFontSize || EditorPanelFontData.Handle == -1) {
        EditorPanelFontData.Delete();
        EditorPanelFontSize = size;
        EditorPanelFontData.Size = (float)size;
        EditorPanelFontData.Handle = txt::OpenFont({ "", "", (float)size, 2, 1, 0 });
    }
    return EditorPanelFontData.Handle != -1 ? EditorPanelFontData : EditorFont();
}

const FontData& Game::EditorFont() const {
    const int size = std::clamp((int)std::lround(EditorResY() / 44.0f), 14, 30);
    if (size != EditorFontSize || EditorFontData.Handle == -1) {
        EditorFontData.Delete();
        EditorFontSize = size;
        EditorFontData.Size = (float)size;
        EditorFontData.Handle = txt::OpenFont({ "", "", (float)size, 2, 1, 0 });
    }
    return EditorFontData.Handle != -1 ? EditorFontData : ConfigSmallFont();
}

float Game::EditorTextWidth(const std::string& str) const {
    const auto& font = EditorFont();
    const float measured = font.Handle != -1 ? (float)GetStrlenCached(str, font.Handle) : 0.0f;
    // Android のフォント準備前・文字幅取得失敗でも UI を画面外へ押し出さない。
    float estimated = 0;
    for (unsigned char c : str) {
        if ((c & 0xc0) == 0x80) { continue; }
        estimated += font.Size * (c < 0x80 ? 0.6f : 1.0f);
    }
    return measured > 0 && measured <= std::max(estimated * 2, font.Size * 2) ? measured : estimated;
}

float Game::EditorLine() const { return EditorFont().Size * 1.4f; }

float Game::EditorLabelWidth() const { return EditorTextWidth("BPM 0000.00") + 20.0f; }

auto Game::EditorGridArea() const -> UIRect {
    const float lh = EditorLine();
    const float top = std::max(EditorButtonsBottom, lh * 2.8f) + lh * 0.6f;
    return { 16.0f, top, EditorResX() - 16.0f, EditorResY() - lh * 2.2f };
}

float Game::EditorRowHeight() const { const UIRect a = EditorGridArea(); return (a.B - a.T) / EditorRows; }

const std::pair<int, int>* Game::EditorFoldAt(int measure) const {
    for (const auto& f : Editor.Folds) { if (f.first <= measure && measure <= f.second) { return &f; } }
    return nullptr;
}

const std::pair<int, int>* Game::EditorOpenFoldAt(int measure) const {
    for (const auto& f : Editor.OpenFolds) { if (f.first <= measure && measure <= f.second) { return &f; } }
    return nullptr;
}

void Game::EditorToggleFold(int measure) {
    if (const auto* closed = EditorFoldAt(measure)) {
        const auto range = *closed;
        std::erase(Editor.Folds, range);
        Editor.OpenFolds.push_back(range);
    }
    else if (const auto* opened = EditorOpenFoldAt(measure)) {
        const auto range = *opened;
        std::erase(Editor.OpenFolds, range);
        Editor.Folds.push_back(range);
        Editor.CursorMeasure = range.first; Editor.CursorSlot = 0;
    }
    EditorNormalizeFolds();
}

void Game::EditorRemoveFoldRegistration(int measure) {
    const auto* registered = EditorFoldAt(measure);
    if (!registered) { registered = EditorOpenFoldAt(measure); }
    if (!registered) { EditorMessage("この小節に折りたたみ登録はありません"); return; }
    const auto range = *registered;
    EditorChange();
    std::erase(Editor.Folds, range);
    std::erase(Editor.OpenFolds, range);
    EditorNormalizeFolds(); EditorClamp();
    EditorMessage("折りたたみ登録を解除しました (小節の内容は残ります)");
}

auto Game::EditorFoldDeleteButton(int row) const -> UIRect {
    const auto lane = EditorLane(row);
    const float size = EditorLine();
    return { lane.L - size * 2 - 8, lane.T - size - 4, lane.L - size - 8, lane.T - 4 };
}

auto Game::EditorFoldButton(int row) const -> UIRect {
    const auto lane = EditorLane(row);
    const float size = EditorLine();
    return { lane.L - size - 4, lane.T - size - 4, lane.L - 4, lane.T - 4 };
}

int Game::EditorNextRowMeasure(int measure) const {
    const auto* f = EditorFoldAt(measure);
    return (f ? f->second : measure) + 1;
}

int Game::EditorRowMeasure(int row) const {
    int m = Editor.Top;
    for (int r = 0; r < row; ++r) { m = EditorNextRowMeasure(m); }
    return m < const_cast<Game*>(this)->EditorMeasureCount() ? m : -1;
}

int Game::EditorMeasureRow(int measure) const {
    int m = Editor.Top;
    for (int r = 0; r < EditorRows; ++r) {
        if (measure >= m && measure < EditorNextRowMeasure(m)) { return r; }
        m = EditorNextRowMeasure(m);
    }
    return -1;
}

void Game::EditorNormalizeFolds() {
    const int n = EditorMeasureCount();
    std::vector<std::pair<int, int>> out;
    auto folds = Editor.Folds;
    std::ranges::sort(folds);
    for (auto [a, b] : folds) {
        a = std::clamp(a, 0, std::max(n - 1, 0)); b = std::clamp(b, a, std::max(n - 1, 0));
        if (b <= a) { continue; }
        if (!out.empty() && a <= out.back().second) { out.back().second = std::max(out.back().second, b); continue; }
        out.push_back({ a, b });
    }
    Editor.Folds = std::move(out);
    for (auto& f : Editor.OpenFolds) {
        f.first = std::clamp(f.first, 0, std::max(n - 1, 0));
        f.second = std::clamp(f.second, f.first, std::max(n - 1, 0));
    }
    std::erase_if(Editor.OpenFolds, [&](const auto& f) {
        return f.second <= f.first || std::ranges::any_of(Editor.Folds, [&](const auto& c) { return f.first <= c.second && c.first <= f.second; });
    });
    std::ranges::sort(Editor.OpenFolds);
    Editor.OpenFolds.erase(std::unique(Editor.OpenFolds.begin(), Editor.OpenFolds.end()), Editor.OpenFolds.end());
    if (const auto* f = EditorFoldAt(Editor.Top)) { Editor.Top = f->first; }
}

void Game::EditorRemapFolds(int at, int removed, int added) {
    auto remap = [&](auto& ranges) {
    std::vector<std::pair<int, int>> folds;
    for (auto [a, b] : ranges) {
        if (removed > 0) {
            const int end = at + removed;
            if (a >= at && b < end) { continue; }
            a = a >= end ? a - removed : (a >= at ? at : a);
            b = b >= end ? b - removed : (b >= at ? at - 1 : b);
        }
        if (a >= at) { a += added; }
        if (b >= at) { b += added; }
        if (b > a) { folds.push_back({ a, b }); }
    }
    ranges = std::move(folds);
    };
    remap(Editor.Folds); remap(Editor.OpenFolds);
}

void Game::EditorScrollRows(int dir) {
    for (; dir > 0; --dir) { const int next = EditorNextRowMeasure(Editor.Top); if (next < EditorMeasureCount()) { Editor.Top = next; } }
    for (; dir < 0; ++dir) { if (Editor.Top <= 0) { break; } Editor.Top = Editor.Top - 1; if (const auto* f = EditorFoldAt(Editor.Top)) { Editor.Top = f->first; } }
}

void Game::EditorFold() {
    const auto* course = EditorCourse();
    if (course == nullptr) { return; }
    if (!Editor.Selecting && (EditorFoldAt(Editor.CursorMeasure) || EditorOpenFoldAt(Editor.CursorMeasure))) { EditorToggleFold(Editor.CursorMeasure); return; }
    if (Editor.Selecting) {
        const auto [a, b] = EditorSelection();
        if (b.Measure <= a.Measure) { EditorMessage("2 小節以上を選ぶと折りたためます"); return; }
        Editor.Folds.push_back({ a.Measure, b.Measure });
        Editor.Selecting = false;
        Editor.CursorMeasure = std::min(b.Measure + 1, EditorMeasureCount() - 1);
        EditorNormalizeFolds();
        EditorMessage(std::to_string(b.Measure - a.Measure + 1) + " 小節を折りたたみました (行をクリックで開く)");
        return;
    }
    int added = 0;
    const int n = (int)course->Measures.size();
    for (int m = 0; m < n;) {
        int e = m;
        while (e < n && course->Measures[e].Notes.empty() && !course->Measures[e].Commands.empty()) { ++e; }
        if (e - m >= 3 && !(m <= Editor.CursorMeasure && Editor.CursorMeasure < e)) { Editor.Folds.push_back({ m, e - 1 }); ++added; m = e; }
        else { m = std::max(e, m + 1); }
    }
    EditorNormalizeFolds();
    EditorMessage(added > 0 ? "ギミックの帯 (音符の無い命令だけの小節が 3 つ以上続く所) を " + std::to_string(added) + " か所折りたたみました" : std::string("折りたためる所がありません (範囲を選ぶと、その小節を折りたためます)"));
}

void Game::EditorReveal(int measure) {
    if (measure < Editor.Top) { Editor.Top = measure; }
    if (const auto* f = EditorFoldAt(Editor.Top)) { Editor.Top = f->first; }
    while (EditorMeasureRow(measure) < 0 && Editor.Top < measure) { EditorScrollRows(1); }
}

double Game::EditorMeasureRatio(int measure) const {
    const double r = const_cast<Game*>(this)->EditorTimingAt(measure).Measure.Value();
    return std::isfinite(r) && r > 0 ? r : 1.0;
}

int Game::EditorDivOf(int measure) const {
    return (int)std::lround(std::clamp(Editor.Division * EditorMeasureRatio(measure), 1.0, (double)Editor.Division * 16));
}

float Game::EditorStandardMeasureWidth() const {
    const auto area = EditorGridArea();
    const float available = std::max(1.0f, area.R - 10 - area.L - EditorLabelWidth());
    return available;
}

void Game::EditorScrollHorizontal(int dir) {
    double longest=1;
    for(int row=0;row<EditorRows;++row) {
        const int m=EditorRowMeasure(row);
        if(m>=0)longest=std::max(longest,std::clamp(EditorMeasureRatio(m),.05,16.0));
    }
    const float width=EditorStandardMeasureWidth();
    EditorHorizontalOffset=std::clamp(EditorHorizontalOffset+dir*width,0.0f,(float)(width*(std::ceil(longest)-1)));
}

void Game::EditorShowCursorPage() {
    if (EditorMeasureCount() <= 0) { return; }
    const float width = EditorStandardMeasureWidth();
    const double ratio = std::clamp(EditorMeasureRatio(Editor.CursorMeasure), 0.05, 16.0);
    const float x = (float)(width * ratio * Editor.CursorSlot / std::max(EditorDivOf(Editor.CursorMeasure), 1));
    // 前・次のページに出た時だけ切り替える (見えている間はそのまま)
    if (x < EditorHorizontalOffset || x >= EditorHorizontalOffset + width) {
        EditorHorizontalOffset = editlayout::PageOffset(x, width, ratio);
    }
}

std::string Game::EditorCommandKey(const std::string& text) {
    const std::string_view t = tja::Trim(text);
    std::string key(t.substr(0, t.find_first_of(" \t")));
    for (auto& c : key) { c = (char)std::toupper((unsigned char)c); }
    return key;
}

void Game::EditorBeginDeleteCommands() {
    auto* course = EditorCourse();
    if (course == nullptr) { return; }
    if (!Editor.Selecting) { EditorMessage("命令を消す範囲を選んでください (Shift+クリック / Shift+矢印)"); return; }
    const auto [a, b] = EditorSelection();
    std::vector<std::pair<std::string, int>> kinds;
    int total = 0;
    for (int m = a.Measure; m <= b.Measure && m < (int)course->Measures.size(); ++m) {
        for (const auto& c : course->Measures[m].Commands) {
            if (!EditorInSelection(m, c.Num, c.Den)) { continue; }
            const std::string key = EditorCommandKey(c.Text);
            auto it = std::ranges::find(kinds, key, &std::pair<std::string, int>::first);
            if (it == kinds.end()) { kinds.push_back({ key, 1 }); } else { ++it->second; }
            ++total;
        }
    }
    if (total == 0) { EditorMessage("選んだ範囲に命令がありません"); return; }
    std::vector<std::string> choices{ "すべて (" + std::to_string(total) + " 個)" };
    for (const auto& [key, n] : kinds) { choices.push_back(key + " (" + std::to_string(n) + " 個)"); }
    EditorBeginFields("範囲の命令を消す (←→ で種類を選ぶ)", { { "消す命令", choices.front(), choices } }, [this, kinds, choices](const auto& values) {
        const auto it = std::ranges::find(choices, values.empty() ? std::string() : values[0]);
        const size_t index = it == choices.end() ? 0 : (size_t)(it - choices.begin());
        EditorDeleteCommandsInSelection(index == 0 ? std::string() : kinds[index - 1].first);
    });
}

void Game::EditorDeleteCommandsInSelection(const std::string& key) {
    auto* course = EditorCourse();
    if (course == nullptr || !Editor.Selecting) { return; }
    const auto [a, b] = EditorSelection();
    auto target = [&](int m, const tja::Command& c) { return EditorInSelection(m, c.Num, c.Den) && (key.empty() || EditorCommandKey(c.Text) == key); };
    int count = 0;
    bool measureCommand = false;
    for (int m = a.Measure; m <= b.Measure && m < (int)course->Measures.size(); ++m) {
        for (const auto& c : course->Measures[m].Commands) {
            if (target(m, c)) { ++count; measureCommand |= EditorCommandKey(c.Text) == "#MEASURE"; }
        }
    }
    if (count == 0) { EditorMessage("消す命令がありません"); return; }
    const std::vector<EditorFrac> before = measureCommand ? EditorMeasureFracs() : std::vector<EditorFrac>{};
    EditorChange();
    for (int m = a.Measure; m <= b.Measure && m < (int)course->Measures.size(); ++m) {
        std::erase_if(course->Measures[m].Commands, [&](const tja::Command& c) { return target(m, c); });
    }
    if (measureCommand) { EditorKeepNotePositions(before); }
    EditorMessage((key.empty() ? std::string("範囲の命令を ") : "範囲の " + key + " を ") + std::to_string(count) + " 個消しました (Ctrl+Z で戻せます)");
}

auto Game::EditorMeasureFracs() -> std::vector<EditorFrac> {
    ++Editor.Revision;      // 小節の長さの記録を作り直させる
    std::vector<EditorFrac> out;
    if (const auto* course = EditorCourse()) {
        for (int m = 0; m < (int)course->Measures.size(); ++m) { out.push_back(EditorTimingAt(m).Measure); }
    }
    return out;
}

void Game::EditorKeepNotePositions(const std::vector<EditorFrac>& before) {
    auto* course = EditorCourse();
    if (course == nullptr) { return; }
    const std::vector<EditorFrac> after = EditorMeasureFracs();
    // 長さを整数の分数にする (小数の #MEASURE も 10 倍ずつして整数に)。負や 0 の長さ (逆走などのギミック) は扱わない
    auto rational = [](const EditorFrac& f, int64_t& n, int64_t& d) {
        double num = f.Num, den = f.Den;
        for (int i = 0; i < 6 && (std::abs(num - std::round(num)) > 1e-9 || std::abs(den - std::round(den)) > 1e-9); ++i) { num *= 10; den *= 10; }
        n = (int64_t)std::llround(num); d = (int64_t)std::llround(den);
        if (d < 0) { n = -n; d = -d; }
        if (n <= 0 || d <= 0) { return false; }
        tja::Reduce(n, d);
        return true;
    };
    // 小節ごとの位置の倍率 (= 前の長さ / 新しい長さ)。変わらない小節・長さが負や 0 の小節は 0
    const int n = (int)std::min(before.size(), after.size());
    std::vector<std::pair<int64_t, int64_t>> factors((size_t)n, { 0, 0 });
    for (int m = 0; m < n; ++m) {
        int64_t bn = 1, bd = 1, an = 1, ad = 1;
        if (!rational(before[(size_t)m], bn, bd) || !rational(after[(size_t)m], an, ad) || bn * ad == an * bd) { continue; }
        factors[(size_t)m] = { bn * ad, bd * an };
    }
    // 音符・命令の小節の中の位置に倍率を掛ける: 拍の位置 (音符の間隔・命令の位置) はそのままで、
    // 伸ばした分は右に空きができ、縮めて小節の終わりからはみ出した音符・命令は消す (#MEASURE は消さずに小節の終わりに置く)
    int removed = 0;
    for (int m = 0; m < n; ++m) {
        const auto [mulN, mulD] = factors[(size_t)m];
        if (mulN == 0) { continue; }
        if (mulN > mulD && !Config.EditorKeepSpacingOnShrink) { continue; }     // 縮める: 「縮めても間隔を保つ」が OFF なら小節に合わせて縮める
        auto& measure = course->Measures[(size_t)m];
        for (auto& x : measure.Notes) { x.Num *= mulN; x.Den *= mulD; tja::Reduce(x.Num, x.Den); }
        for (auto& x : measure.Commands) {
            x.Num *= mulN; x.Den *= mulD; tja::Reduce(x.Num, x.Den);
            if (x.Num > x.Den && tja::StartsWithKey(x.Text, "#MEASURE")) { x.Num = 1; x.Den = 1; }
        }
        removed += (int)std::erase_if(measure.Notes, [](const tja::Note& x) { return x.Num >= x.Den; });
        removed += (int)std::erase_if(measure.Commands, [](const tja::Command& x) { return x.Num > x.Den; });
    }
    ++Editor.Revision;
    if (removed > 0) { EditorMessage("小節を縮めたため、小節の終わりからはみ出した " + std::to_string(removed) + " 個の音符・命令を消しました (Ctrl+Z で戻せます)"); }
}

void Game::EditorBeginBpmAnalyze() {
    if (!EditorCourse()) { return; }
    if (!Editor.Selecting) { EditorMessage("BPM を解析する範囲を選んでください (Shift+クリック / Shift+矢印)"); return; }
    EditorBeginFields("範囲の BPM を解析して #BPMCHANGE を入れる", {
        { "許容するずれ (ms)", "5" },
        { "BPM の丸め", "小数第 2 位", { "整数", "小数第 1 位", "小数第 2 位", "小数第 3 位" } },
        { "入れる位置", "拍ごと (小節の途中も)", { "拍ごと (小節の途中も)", "小節の頭だけ" } },
    }, [this](const auto& values) {
        double tolerance = 0;
        const std::string text = values.empty() ? std::string() : std::string(tja::Trim(values[0]));
        const auto result = fast_float::from_chars(text.data(), text.data() + text.size(), tolerance);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || !std::isfinite(tolerance) || tolerance < 0) { EditorMessage("ずれは 0 以上の数 (ミリ秒) で入れてください"); return; }
        const std::string rounding = values.size() > 1 ? values[1] : std::string();
        const bool perBeat = values.size() <= 2 || values[2] != "小節の頭だけ";
        EditorAnalyzeBpm(std::max(tolerance, 0.5), rounding == "整数" ? 0 : rounding == "小数第 1 位" ? 1 : rounding == "小数第 3 位" ? 3 : 2, perBeat);
    });
}

// 範囲の小節の頭が音源のどこにあるかを 1 小節ずつ求め、譜面の小節の頭が音源から tolerance (ms) 以上ずれる所でだけ #BPMCHANGE を入れる。
// 入れる BPM は、次の小節の頭がちょうど音源の頭に来る値にする (差の小さい変化を見送っても、ずれが積み重ならない)
void Game::EditorAnalyzeBpm(double tolerance, int digits, bool perBeat) {
    auto* course = EditorCourse();
    if (course == nullptr || !Editor.Selecting) { return; }
    const auto [a, b] = EditorSelection();
    const int count = (int)course->Measures.size();
    const int first = std::clamp(a.Measure, 0, count - 1), last = std::clamp(b.Measure, first, count - 1);
    // 範囲の小節の拍数と、範囲の始まりの BPM・時刻
    std::vector<double> beats;
    for (int m = first; m <= last; ++m) { beats.push_back(EditorTimingAt(m).Measure.Value() * 4); }
    const double initial = EditorTimingAt(first).Bpm;
    if (!(initial > 0) || std::ranges::any_of(beats, [](double v) { return !(v > 0); })) { EditorMessage("負の BPM・拍子 (逆走などのギミック) を含む範囲は解析できません"); return; }
    const auto& timeline = EditorTimelineCached();
    if (first >= (int)timeline.Measures.size() || timeline.Measures[(size_t)first].Map.empty()) { EditorMessage("範囲の時刻を求められませんでした"); return; }
    const double offset = std::atof(tja::Document::GetValue(Editor.Doc.Header, "OFFSET").c_str());
    const double start = timeline.Measures[(size_t)first].Map.front().second / 1000.0 - offset;
    auto [mono, rate] = EditorReadMono();
    if (mono.empty()) { return; }
    if (start < 0 || start * rate >= (double)mono.size()) { EditorMessage("範囲の始まりが音源の外にあります"); return; }
    // 小節ごとの拍の頭の時刻 (小節の頭だけの時は、小節の頭と終わりの 2 点)
    auto points = tempo::TrackBeatTimes(mono.data(), mono.size(), rate, start, beats, initial);
    if (points.size() != beats.size()) { EditorMessage("BPM を解析できませんでした"); return; }
    if (!perBeat) { for (auto& p : points) { p = { p.front(), p.back() }; } }
    // 範囲の後ろの小節が今まで使っていた BPM (範囲の BPM を変えても、後ろは元の BPM に戻す)
    const bool hasNext = last + 1 < count;
    const double nextBpm = hasNext ? EditorTimingAt(last + 1).Bpm : 0;
    const bool nextHasOwn = hasNext && std::ranges::any_of(course->Measures[(size_t)(last + 1)].Commands, [](const tja::Command& c) { return c.Num == 0 && tja::StartsWithKey(c.Text, "#BPMCHANGE"); });
    EditorChange();
    for (int m = first; m <= last; ++m) {
        std::erase_if(course->Measures[(size_t)m].Commands, [](const tja::Command& c) { return tja::StartsWithKey(c.Text, "#BPMCHANGE"); });
    }
    ++Editor.Revision;
    double current = EditorTimingAt(first).Bpm;     // 範囲に入る時の BPM (範囲の #BPMCHANGE を消した後)
    const double scale = std::pow(10.0, digits), tol = tolerance / 1000.0;
    double chart = points.front().front(), lowest = 1e9, highest = 0, worst = 0;
    int inserted = 0;
    for (size_t i = 0; i < beats.size(); ++i) {
        const auto& p = points[i];
        int64_t rn = 1, rd = 1;
        if (!EditorToRational(EditorTimingAt(first + (int)i).Measure, rn, rd)) { rn = rd = 1; }
        // 区切り j の拍の位置 (小節の頭だけの時は 0 と小節の終わり)
        auto beatAt = [&](size_t j) { return p.size() == 2 ? (j == 0 ? 0.0 : beats[i]) : std::min((double)j, beats[i]); };
        for (size_t j = 0; j + 1 < p.size(); ++j) {
            const double length = beatAt(j + 1) - beatAt(j);
            double next = chart + length * 60 / current;
            if (std::abs(next - p[j + 1]) > tol && p[j + 1] > chart && length > 0) {
                const double v = std::round(length * 60 / (p[j + 1] - chart) * scale) / scale;
                if (v > 0 && std::abs(v - current) > 1e-9) {
                    // 小節の中の位置 = 拍 / (拍子 × 4) = j × rd / (4 × rn)
                    int64_t num = (int64_t)std::llround(beatAt(j)) * rd, den = 4 * rn;
                    tja::Reduce(num, den);
                    tja::Document::AddCommand(course->Measures[(size_t)first + i], num, den, "#BPMCHANGE " + EditorNum(v));
                    current = v;
                    next = chart + length * 60 / current;
                    ++inserted;
                }
            }
            lowest = std::min(lowest, current); highest = std::max(highest, current);
            worst = std::max(worst, std::abs(next - p[j + 1]));
            chart = next;
        }
    }
    if (hasNext && !nextHasOwn && std::abs(current - nextBpm) > 1e-9) {
        tja::Document::AddCommand(course->Measures[(size_t)last + 1], 0, 1, "#BPMCHANGE " + EditorNum(nextBpm));
    }
    ++Editor.Revision;
    const std::string range = "小節 " + std::to_string(first + 1) + "～" + std::to_string(last + 1) + " (BPM " + EditorNum(lowest) + "～" + EditorNum(highest)
        + "・小節の頭のずれ 最大 " + FormatNumber(worst * 1000, 1) + "ms)";
    EditorMessage(inserted > 0 ? range + ": " + std::to_string(inserted) + " か所に #BPMCHANGE を入れました (目安なので、疑似再生かテストプレイで確かめてください)"
        : range + ": ずれが " + EditorNum(tolerance) + "ms を超える所はありませんでした");
}

auto Game::EditorLane(int row) const -> UIRect {
    const UIRect a = EditorGridArea();
    const float h = EditorRowHeight(), top = a.T + h * row;
    const float left = a.L + EditorLabelWidth() - EditorHorizontalOffset;
    const int measure = EditorRowMeasure(row);
    const float width = measure < 0 || EditorFoldAt(measure) ? std::max(1.0f, a.R - 10 - left)
        : EditorStandardMeasureWidth() * (float)std::clamp(EditorMeasureRatio(measure), 0.05, 16.0);
    return { left, top + h * 0.45f, left + width, top + h * 0.92f };
}

float Game::EditorX(const UIRect& lane, double pos) const { return lane.L + (lane.R - lane.L) * (float)pos; }

float Game::EditorNoteWidthPerHeight(char type) const {
    const auto& img = Skin.Base->Playing.Image.Note;
    const int index = EditorNoteImage(type);
    if (index < 0 || index >= (int)img.Handles.size() || img.Size.Height <= 0) {
        return type == '3' || type == '4' || type == '6' || type == '9' ? 0.84f : 0.6f;
    }
    const void* first = img.Handles.empty() ? nullptr : (const void*)img.Handles.front().Tex.get();
    if (EditorNoteBoundsPath != img.SourcePath || EditorNoteBoundsHandle != first) {
        EditorNoteBoundsPath = img.SourcePath; EditorNoteBoundsHandle = first;
        EditorNoteWidthFractions.assign(img.Handles.size(), 1.0f);
        // 画像の透明な余白を除き、実際に見える音符の幅を一度だけ調べる。
        int width = 0, height = 0;
        const std::vector<uint32_t> pixels = img.SourcePath.empty() ? std::vector<uint32_t>() : gfx::LoadPixels(img.SourcePath, width, height);
        if (!pixels.empty()) {
            const int columns = std::max(1, img.Div.X), rows = std::max(1, img.Div.Y);
            const int w = width / columns, h = height / rows;
            if (w > 0 && h > 0 && (int64_t)width * height <= 16000000) {
                for (int i = 0; i < (int)img.Handles.size() && i < columns * rows; ++i) {
                    float radius = 0;
                    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
                        const int px = (i % columns) * w + x, py = (i / columns) * h + y;
                        if ((pixels[(size_t)py * width + px] >> 24) > 0) {
                            radius = std::max(radius, std::abs(x + 0.5f - w * 0.5f) + 0.5f);
                        }
                    }
                    if (radius > 0) { EditorNoteWidthFractions[i] = std::min(1.0f, 2 * radius / w); }
                }
            }
        }
    }
    const float fraction = index < (int)EditorNoteWidthFractions.size() ? EditorNoteWidthFractions[index] : 1.0f;
    return 1.2f * img.Size.Width / img.Size.Height * fraction;
}

float Game::EditorGridNoteHeight() const {
    // 4/4小節を16等分した1区間を基準に、通常音符の輪郭直径を61.731%へ揃える。
    // ユーザー指定により、従来の64.98%からさらに5%縮小。
    // 透明な画像余白を含むフレーム寸法、BPM、ハイスピードや密度には依存しない。
    constexpr float diameterRatio = 0.61731f;
    const float extent = std::max(EditorNoteWidthPerHeight('1'), EditorNoteWidthPerHeight('2'));
    return EditorStandardMeasureWidth() / 16.0f * diameterRatio / std::max(0.01f, extent);
}

auto Game::EditorButtons() -> std::vector<EditorButton> {
    std::vector<EditorButton> out;
    EditorGroups.clear();
    if (!Editor.Open) { return out; }
    const float lh = EditorLine();
    const float h = lh * 1.7f;              // ボタンの高さ
    const float pad = 6.0f, gap = 10.0f;
    const float left = 12.0f, right = EditorResX() - 12.0f;
    float x = left, y = 8.0f;
    struct Item { std::string Label; std::function<void()> Action; bool Active = false; EditorIcon Icon = EditorIcon::None; char Note = 0; };
    auto itemwidth = [&](const Item& it) {
        if (it.Icon == EditorIcon::SpeedValue) {
            return h + EditorTextWidth(FormatNumber(Editor.PreviewSpeed, 2) + "x") + 12.0f;
        }
        return it.Icon != EditorIcon::None ? h * (it.Icon == EditorIcon::Note ? 1.15f : 1.0f) : EditorTextWidth(it.Label) + 28.0f;
    };
    // まとまりごとに並べる (入りきらなければ次の段へ)。見出しはまとまりの左に小さく書く
    auto group = [&](const std::string& name, std::vector<Item> items) {
        const float labelw = EditorTextWidth(name) + 12.0f;
        float width = pad + labelw;
        for (const auto& it : items) { width += itemwidth(it) + 4.0f; }
        width += pad - 4.0f;
        if (x > left && x + width > right) { x = left; y += h + pad * 2 + 6.0f; }
        EditorGroups.push_back({ { x, y, x + width, y + h + pad * 2 }, name });
        float bx = x + pad + labelw;
        for (auto& it : items) {
            const float w = itemwidth(it);
            out.push_back({ { bx, y + pad, bx + w, y + pad + h }, it.Label, std::move(it.Action), it.Active, it.Icon, it.Note });
            bx += w + 4.0f;
        }
        x += width + gap;
        EditorButtonsBottom = y + h + pad * 2;
    };
    auto newline = [&] { if (x > left) { x = left; y += h + pad * 2 + 6.0f; } };
    auto panel = [this](EditorState::PanelType type) {
        return [this, type] { Editor.Panel = Editor.Panel == type ? EditorState::PanelType::None : type; };
    };
    group("ファイル", {
        { "保存", [this] { if (EditorSave()) { Skin.Base->Title.SE.Don.Play(); } }, false, EditorIcon::Save },
        { "テストプレイ", [this] { EditorTestPlay(); }, false, EditorIcon::Test },
        { "閉じる", [this] { EditorClose(); }, false, EditorIcon::Close },
    });
    group("再生", {
        { Editor.Preview ? "■ 停止" : "▶ 再生", [this] { EditorTogglePreview(); }, Editor.Preview, Editor.Preview ? EditorIcon::Stop : EditorIcon::Play },
        { "◀ 速度", [this] { EditorSetPreviewSpeed(-1); }, false, EditorIcon::SpeedDown },
        { "速度 " + FormatNumber(Editor.PreviewSpeed, 2) + "x", [this] { EditorBeginEdit("再生速度 (0.25～2.0)",FormatNumber(Editor.PreviewSpeed,2),[this](const std::string& text) { double value=0; const auto result=fast_float::from_chars(text.data(),text.data()+text.size(),value); if(result.ec==std::errc{}&&result.ptr==text.data()+text.size())EditorApplyPreviewSpeed(value); else EditorMessage("数値を入力してください"); }); }, false, EditorIcon::SpeedValue },
        { "速度 ▶", [this] { EditorSetPreviewSpeed(1); }, false, EditorIcon::SpeedUp },
        { "次郎形式再生", [this] { EditorToggleJiroPreview(); }, EditorJiroPreviewEnabled(), EditorIcon::JiroPreview },
        { Editor.PreviewTaiko ? "太鼓音 ON" : "太鼓音 OFF", [this] { Editor.PreviewTaiko = !Editor.PreviewTaiko; }, Editor.PreviewTaiko, EditorIcon::TaikoSound },
    });
    group("編集", {
        { "表示 ◀", [this] { EditorScrollHorizontal(-1); }, false, EditorIcon::Left },
        { "表示 ▶", [this] { EditorScrollHorizontal(1); }, false, EditorIcon::Right },
        { "元に戻す", [this] { EditorUndo(false); }, false, EditorIcon::Undo },
        { "やり直し", [this] { EditorUndo(true); }, false, EditorIcon::Redo },
        { "範囲選択", [this] { Editor.SelectMode = !Editor.SelectMode; if (!Editor.SelectMode) { Editor.Selecting = false; } }, Editor.SelectMode, EditorIcon::Select },
        { "コピー", [this] { EditorCopy(false); }, false, EditorIcon::Copy },
        { "切り取り", [this] { EditorCopy(true); }, false, EditorIcon::Cut },
        { "貼り付け", [this] { EditorPaste(); }, false, EditorIcon::Paste },
        { "命令を消す", [this] { EditorBeginDeleteCommands(); }, false, EditorIcon::CommandDelete },
    });
    group("小節", {
        { "挿入", [this] {
            EditorChange();
            auto& ms = EditorCourse()->Measures;
            ms.insert(ms.begin() + std::min(Editor.CursorMeasure + 1, (int)ms.size()), tja::Measure{});
            EditorRemapFolds(Editor.CursorMeasure + 1, 0, 1);
            ++Editor.CursorMeasure;
            EditorClamp();
        }, false, EditorIcon::Insert },
        { "折りたたむ", [this] { EditorFold(); }, false, EditorIcon::Fold },
        { "登録解除", [this] { EditorRemoveFoldRegistration(Editor.CursorMeasure); }, false, EditorIcon::Unfold },
        { "削除", [this] {
            auto& ms = EditorCourse()->Measures;
            // 範囲を選んでいる時は、範囲にかかる小節をまとめて消す
            int from = Editor.CursorMeasure, to = Editor.CursorMeasure;
            if (Editor.Selecting) { const auto [a, b] = EditorSelection(); from = a.Measure; to = b.Measure; }
            from = std::clamp(from, 0, (int)ms.size() - 1);
            to = std::clamp(to, from, (int)ms.size() - 1);
            if (to - from + 1 >= (int)ms.size()) { EditorMessage("すべての小節は削除できません (1 小節は残してください)"); return; }
            EditorChange();
            ms.erase(ms.begin() + from, ms.begin() + to + 1);
            EditorRemapFolds(from, to - from + 1, 0);
            Editor.Selecting = false;
            Editor.CursorMeasure = from;
            EditorClamp();
            if (to > from) { EditorMessage(std::to_string(to - from + 1) + " 小節を削除しました"); }
        }, false, EditorIcon::Delete },
    });
    group("譜面", {
        { "ヘッダー", panel(EditorState::PanelType::Header), Editor.Panel == EditorState::PanelType::Header, EditorIcon::Header },
        { "命令", panel(EditorState::PanelType::Commands), Editor.Panel == EditorState::PanelType::Commands, EditorIcon::Command },
        { "ギミック", panel(EditorState::PanelType::Gimmick), Editor.Panel == EditorState::PanelType::Gimmick, EditorIcon::Gimmick },
        { "自動推定", [this] { EditorStartEstimate(); }, false, EditorIcon::Estimate },
        { "次郎ずれ補正", [this] { EditorFixJiroDrift(); }, false, EditorIcon::JiroFix },
    });
    group("音源", {
        { "音源長→小節数", [this] { EditorFitAudioLength(); }, false, EditorIcon::AudioLength },
        { "現在位置をDEMOSTARTに設定", [this] { EditorSetDemoStart(); }, false, EditorIcon::DemoStart },
        { "波形に音符を合わせる", [this] { EditorBeginWaveAlign(); }, false, EditorIcon::WaveAlign },
        { "範囲のBPMを解析", [this] { EditorBeginBpmAnalyze(); }, false, EditorIcon::BpmAnalyze },
    });
    newline();
    group("コース", {
        { "前のコース", [this] { EditorStoreFolds(); Editor.Course = (Editor.Course + (int)Editor.Doc.Courses.size() - 1) % (int)Editor.Doc.Courses.size(); EditorLoadFolds(); EditorClamp(); }, false, EditorIcon::Left },
        { Editor.Doc.CourseName(Editor.Course), [] {} },
        { "次のコース", [this] { EditorStoreFolds(); Editor.Course = (Editor.Course + 1) % (int)Editor.Doc.Courses.size(); EditorLoadFolds(); EditorClamp(); }, false, EditorIcon::Right },
    });
    group("グリッド", {
        { "細かくしない分割", [this] { EditorSetDivision(-1); }, false, EditorIcon::Left },
        { std::to_string(Editor.Division) + " 分割…", panel(EditorState::PanelType::Grid) },
        { "細かい分割", [this] { EditorSetDivision(1); }, false, EditorIcon::Right },
    });
    static const std::pair<char, const char*> palette[] = {
        { '1', "ドン" }, { '2', "カッ" }, { '3', "大ドン" }, { '4', "大カッ" }, { '5', "連打" },
        { '6', "大連打" }, { '7', "風船" }, { '8', "連打終点" }, { '9', "くすだま" }, { '0', "休符" },
    };
    std::vector<Item> notes;
    for (const auto& [type, label] : palette) {
        notes.push_back({ std::string(1, type) + " " + label, [this, t = type] { Editor.Palette = t; }, Editor.Palette == type, EditorIcon::Note, type });
    }
    group("音符", std::move(notes));
    return out;
}

std::string Game::EditorButtonHelp(const std::string& label) {
    static const std::pair<const char*, const char*> helps[] = {
        { "現在位置をDEMOSTARTに設定", "現在の小節・グリッドを選曲時の試聴開始位置にする。BPM・拍子・OFFSETを反映 (Ctrl+Zで戻せる)" },
        { "波形に音符を合わせる", "配置済みのドン／カッの数と配色を保ち、近くの波形の打音に合わせる。範囲選択中はその範囲だけ (Ctrl+Zで戻せる)" },
        { "音源長→小節数", "音源の長さ・OFFSET・BPM・拍子から総小節数を調整する。内容のある小節は削除せず、Ctrl+Z で戻せる" },
        { "登録解除", "カーソルの小節を含む折りたたみ登録を解除する。小節・音符・命令は残る (Ctrl+Z で戻せる)" },
        { "範囲選択", "ON の間は、小節を 2 回押して範囲を選ぶ (Shift+クリックと同じ)。選んだ範囲はコピー・切り取りできる" },
        { "コピー", "選んだ範囲の音符・命令をコピーする (Ctrl+C)" },
        { "切り取り", "選んだ範囲の音符・命令を切り取る (Ctrl+X)" },
        { "貼り付け", "カーソルの位置に貼り付ける (Ctrl+V)" },
        { "次郎ずれ補正", "太鼓さん次郎で BPM の小数以下の切り捨てにより譜面が早くなる分を、小節の頭の #DELAY で打ち消す。範囲を選んでいればその範囲、無ければコース全体 (Ctrl+Z で戻せる)" },
        { "命令を消す", "選んだ範囲の命令を、すべて・または種類 (#SCROLL など) を選んでまとめて消す (Ctrl+Z で戻せる)" },
        { "範囲のBPMを解析", "選んだ範囲の小節の頭を音源から 1 小節ずつ求め、譜面が音源から指定の ms 以上ずれる小節に #BPMCHANGE を入れる (範囲の #BPMCHANGE は入れ直す。Ctrl+Z で戻せる)" },
        { "前のコース", "編集するコースを切り替える (コースの追加はヘッダーから)" },
        { "次のコース", "編集するコースを切り替える (コースの追加はヘッダーから)" },
        { "細かくしない分割", "グリッドを「よく使う分割」の 1 つ粗いものにする" },
        { "細かい分割", "グリッドを「よく使う分割」の 1 つ細かいものにする" },
        { "保存", "譜面を保存する (Ctrl+S)。初めて上書きする時は元のファイルを .bak として残す" },
        { "テストプレイ", "カーソルの小節から演奏してみる (F5)。F1 でオートプレイの切り替え、Esc で戻る" },
        { "▶ 再生", "編集画面のまま、カーソルの位置から音源と一緒に流す (Space)" },
        { "■ 停止", "疑似再生を止める (Space)" },
        { "次郎形式再生", "Edit の譜面マップ内で次郎形式の時刻・疑似再生を切り替える" },
        { "速度", "疑似再生の速度 (0.25～2 倍)。左右ボタンで変更、数値表示を押すと直接入力。音の高さは変わらない" },
        { "ヘッダー", "曲名・BPM・音源 (WAVE)・OFFSET・コースなどを編集する" },
        { "命令", "#BPMCHANGE などの命令をカーソルの位置に入れる" },
        { "ギミック", "太鼓さん次郎のギミック (逆走・瞬間移動・疑似停止など) の書き方を計算して入れる" },
        { "挿入", "カーソルの小節の後ろに空の小節を入れる" },
        { "折りたたむ", "範囲を選んでいればその小節を、無ければ音符の無い命令だけの小節が 3 つ以上続く所 (ギミックの帯) を 1 行に折りたたむ。折りたたんだ行はクリックで開く" },
        { "削除", "カーソルの小節を消す。範囲を選んでいる時は範囲の小節をまとめて消す (Ctrl+Z で元に戻せる)" },
        { "自動推定", "音源から BPM・開始オフセット・拍子を推定する" },
        { "元に戻す", "直前の変更を取り消す (Ctrl+Z)" },
        { "やり直し", "取り消した変更をやり直す (Ctrl+Y)" },
        { "閉じる", "編集を終える (Esc)" },
        { "Easy", "編集するコース。◀ ▶ で切り替える (コースの追加はヘッダーから)" },
        { "Normal", "編集するコース。◀ ▶ で切り替える (コースの追加はヘッダーから)" },
        { "Hard", "編集するコース。◀ ▶ で切り替える (コースの追加はヘッダーから)" },
        { "Oni", "編集するコース。◀ ▶ で切り替える (コースの追加はヘッダーから)" },
        { "Edit", "編集するコース。◀ ▶ で切り替える (コースの追加はヘッダーから)" },
        { "分割", "グリッドの分割数 (1 小節あたり)。押すと一覧、◀ ▶ はよく使う分割だけを切り替える" },
        { "1 ", "ドン" }, { "2 ", "カッ" }, { "3 ", "大ドン" }, { "4 ", "大カッ" }, { "5 ", "連打 (8 の連打終点まで)" },
        { "6 ", "大連打 (8 の連打終点まで)" }, { "7 ", "風船 (打数はヘッダーの BALLOON)" }, { "8 ", "連打・風船の終わり" },
        { "9 ", "くすだま (打数はヘッダーの BALLOON)" }, { "0 ", "休符: クリックした位置の音符を消す" },
    };
    for (const auto& [key, help] : helps) {
        if (label.starts_with(key) || label.find(key) != std::string::npos && std::string_view(key).size() > 2) { return help; }
    }
    return {};
}

std::vector<int> Game::EditorGridChoices() const {
    std::vector<int> grids(std::begin(EditorDivisions), std::end(EditorDivisions));
    for (const auto& list : { Config.EditorCustomGrids, Config.EditorGrids }) {
        for (int value : list) { if (value >= 1 && value <= EditorMaxDivision) { grids.push_back(value); } }
    }
    std::ranges::sort(grids); grids.erase(std::unique(grids.begin(), grids.end()), grids.end());
    return grids;
}

bool Game::EditorDefaultGrid(int division) {
    return std::ranges::find(EditorDivisions, division) != std::end(EditorDivisions);
}

int Game::EditorGridPageIndex() const {
    return std::clamp(Editor.GridPage, 0, std::max(0, ((int)EditorGridChoices().size() - 1) / EditorGridsPerPage));
}

std::vector<int> Game::EditorGridPanelChoices() const {
    auto choices = EditorGridChoices();
    const int start = EditorGridPageIndex() * EditorGridsPerPage;
    return { choices.begin() + start, choices.begin() + std::min(start + EditorGridsPerPage, (int)choices.size()) };
}

auto Game::EditorGridDeleteButton(int index) const -> UIRect {
    auto r = EditorPanelRow(index, 4);
    r.L = r.R - std::min(EditorLine() * 1.8f, (r.R - r.L) * 0.3f);
    return r;
}

void Game::EditorRemoveGrid(int division) {
    if (EditorDefaultGrid(division)) { EditorMessage("初期の分割数は削除できません"); return; }
    std::erase(Config.EditorCustomGrids, division);
    std::erase(Config.EditorGrids, division);
    if (Config.EditorGrids.empty()) { Config.EditorGrids = { 4, 8, 16 }; }
    if (Editor.Division == division) { EditorSelectDivision(16); }
    Config.Write(); EditorClamp();
    EditorMessage(std::to_string(division) + " 分割の登録を削除しました");
}

void Game::EditorAddGrid(int division) {
    if (division < 1 || division > EditorMaxDivision) { EditorMessage("分割数は 1～4096 の整数で指定してください"); return; }
    if (std::ranges::find(EditorDivisions, division) == std::end(EditorDivisions) &&
        std::ranges::find(Config.EditorCustomGrids, division) == Config.EditorCustomGrids.end()) {
        if (Config.EditorCustomGrids.size() >= 64) { EditorMessage("任意の分割数は 64 個まで登録できます"); return; }
        Config.EditorCustomGrids.push_back(division);
    }
    if (std::ranges::find(Config.EditorGrids, division) == Config.EditorGrids.end()) { Config.EditorGrids.push_back(division); }
    Config.Write(); EditorSelectDivision(division);
    EditorMessage(std::to_string(division) + " 分割を追加しました");
}

std::vector<int> Game::EditorGridList() const {
    std::vector<int> grids;
    for (int g : Config.EditorGrids) {
        if (g >= 1 && g <= EditorMaxDivision && std::ranges::find(grids, g) == grids.end()) { grids.push_back(g); }
    }
    std::ranges::sort(grids);
    if (grids.empty()) { grids = { 4, 8, 16 }; }
    return grids;
}

void Game::EditorSelectDivision(int division) {
    const int old = Editor.Division;
    Editor.Division = std::clamp(division, 1, EditorMaxDivision);
    Editor.CursorSlot = std::clamp((int)std::lround((double)Editor.CursorSlot * Editor.Division / old), 0, EditorDivOf(Editor.CursorMeasure) - 1);
}

void Game::EditorSetDivision(int dir) {
    const auto grids = EditorGridList();
    // 今の分割の次 / 前のよく使う分割 (今の分割が一覧に無くても、大きさの順で移動する)
    int next = Editor.Division;
    if (dir > 0) {
        auto it = std::ranges::upper_bound(grids, Editor.Division);
        next = it != grids.end() ? *it : grids.back();
    }
    else {
        auto it = std::ranges::lower_bound(grids, Editor.Division);
        next = it != grids.begin() ? *std::prev(it) : grids.front();
    }
    EditorSelectDivision(next);
}

auto Game::EditorCursorPos() const -> EditorPos { return { Editor.CursorMeasure, Editor.CursorSlot, EditorDivOf(Editor.CursorMeasure) }; }

int Game::EditorComparePos(const EditorPos& a, const EditorPos& b) {
    if (a.Measure != b.Measure) { return a.Measure < b.Measure ? -1 : 1; }
    return tja::ComparePos(a.Num, a.Den, b.Num, b.Den);
}

void Game::EditorBeginSelection() {
    if (!Editor.Selecting) {
        Editor.Selecting = true;
        Editor.SelMeasure = Editor.CursorMeasure;
        Editor.SelNum = Editor.CursorSlot;
        Editor.SelDen = EditorDivOf(Editor.CursorMeasure);
    }
}

auto Game::EditorSelection() const -> std::pair<EditorPos, EditorPos> {
    EditorPos a{ Editor.SelMeasure, Editor.SelNum, Editor.SelDen }, b = EditorCursorPos();
    if (EditorComparePos(a, b) > 0) { std::swap(a, b); }
    return { a, b };
}

bool Game::EditorInSelection(int measure, int64_t num, int64_t den) const {
    if (!Editor.Selecting) { return false; }
    const auto [a, b] = EditorSelection();
    const EditorPos p{ measure, num, den };
    return EditorComparePos(a, p) <= 0 && EditorComparePos(p, b) <= 0;
}

void Game::EditorRelative(const EditorPos& start, int measure, int64_t num, int64_t den, int64_t& outnum, int64_t& outden) {
    outden = std::lcm(den, start.Den);
    outnum = (int64_t)(measure - start.Measure) * outden + num * (outden / den) - start.Num * (outden / start.Den);
    tja::Reduce(outnum, outden);
}

bool Game::EditorCopy(bool cut) {
    auto* course = EditorCourse();
    if (course == nullptr || !Editor.Selecting) { EditorMessage("Shift+クリックか Shift+矢印で範囲を選んでください"); return false; }
    const auto [a, b] = EditorSelection();
    std::vector<EditorState::ClipItem> clip;
    for (int m = a.Measure; m <= b.Measure && m < (int)course->Measures.size(); ++m) {
        for (const auto& n : course->Measures[m].Notes) {
            if (!EditorInSelection(m, n.Num, n.Den)) { continue; }
            EditorState::ClipItem item;
            EditorRelative(a, m, n.Num, n.Den, item.Num, item.Den);
            item.Type = n.Type;
            clip.push_back(item);
        }
        for (const auto& c : course->Measures[m].Commands) {
            if (!EditorInSelection(m, c.Num, c.Den)) { continue; }
            EditorState::ClipItem item;
            EditorRelative(a, m, c.Num, c.Den, item.Num, item.Den);
            item.Command = c.Text;
            clip.push_back(item);
        }
    }
    if (clip.empty()) { EditorMessage("選択範囲に音符・命令がありません"); return false; }
    Editor.Clipboard = std::move(clip);
    if (cut) {
        EditorChange();
        for (int m = a.Measure; m <= b.Measure && m < (int)course->Measures.size(); ++m) {
            auto& measure = course->Measures[m];
            std::erase_if(measure.Notes, [&](const tja::Note& n) { return EditorInSelection(m, n.Num, n.Den); });
            std::erase_if(measure.Commands, [&](const tja::Command& c) { return EditorInSelection(m, c.Num, c.Den); });
        }
    }
    EditorMessage(std::to_string(Editor.Clipboard.size()) + (cut ? " 個を切り取りました" : " 個をコピーしました"));
    return true;
}

void Game::EditorPaste() {
    auto* course = EditorCourse();
    if (course == nullptr || Editor.Clipboard.empty()) { EditorMessage("コピーした音符がありません"); return; }
    EditorChange();
    const EditorPos base = EditorCursorPos();
    for (const auto& item : Editor.Clipboard) {
        // 貼り付け先 = カーソル + 相対位置 (小節をまたぐ時は足りない小節を足す)
        int64_t den = std::lcm(item.Den, base.Den);
        int64_t num = item.Num * (den / item.Den) + base.Num * (den / base.Den);
        int measure = base.Measure + (int)(num / den);
        num %= den;
        while (measure >= (int)course->Measures.size()) { course->Measures.emplace_back(); }
        if (item.Command.empty()) { tja::Document::SetNote(course->Measures[measure], num, den, item.Type); }
        else { tja::Document::AddCommand(course->Measures[measure], num, den, item.Command); }
    }
    EditorMessage(std::to_string(Editor.Clipboard.size()) + " 個を貼り付けました");
}

void Game::EditorUndo(bool redo) {
    auto& from = redo ? Editor.Redo : Editor.Undo;
    auto& to = redo ? Editor.Undo : Editor.Redo;
    if (from.empty()) { EditorMessage(redo ? "やり直せる操作がありません" : "元に戻せる操作がありません"); return; }
    ++Editor.Revision;
    EditorStopPreview();
    to.push_back({ std::move(Editor.Doc), Editor.Folds, Editor.Course, Editor.OpenFolds });
    Editor.Doc = std::move(from.back().Doc);
    Editor.Folds = std::move(from.back().Folds);
    Editor.OpenFolds = std::move(from.back().OpenFolds);
    Editor.Course = from.back().Course;
    from.pop_back();
    Editor.Dirty = true;
    EditorClamp();
}

void Game::EditorClose() {
    EditorStoreFolds();
    if (Editor.Dirty && std::chrono::steady_clock::now() > Editor.ExitConfirmUntil) {
        Editor.ExitConfirmUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        EditorMessage("保存していない変更があります。もう一度押すと保存せずに閉じます");
        return;
    }
    EditorReleaseSong();
    Editor.Open = false;
    Editor.Dirty = false;
    Editor.Panel = EditorState::PanelType::None;
    Editor.Undo.clear();
    Editor.Redo.clear();
    if(EditorReturnScene==Scene::SongSelect) NowScene=EditorReturnScene;
}

auto Game::EditorCommandRects() -> std::vector<EditorCommandHit> {
    std::vector<EditorCommandHit> out;
    const auto* course = EditorCourse();
    if (course == nullptr) { return out; }
    const FontData& sfont = EditorFont();
    for (int row = 0; row < EditorRows; ++row) {
        const int m = EditorRowMeasure(row);
        if (m < 0 || m >= (int)course->Measures.size()) { break; }
        if (EditorFoldAt(m)) { continue; }      // 折りたたんだ行には命令を出さない
        const UIRect lane = EditorLane(row);
        const float top = lane.T - (sfont.Size + 3.0f) * 3.0f - 2.0f;
        const auto& cmds = course->Measures[m].Commands;
        float lastright = -1e9f;
        int stack = 0;
        for (int i = 0; i < (int)cmds.size(); ++i) {
            const float x = EditorX(lane, std::min(1.0, (double)cmds[i].Num / cmds[i].Den));
            const float w = std::min(EditorTextWidth(cmds[i].Text) + 12.0f, std::max(120.0f, (lane.R - lane.L) * 0.18f));
            stack = x < lastright ? (stack + 1) % 3 : 0;
            const float y = top + stack * (sfont.Size + 3.0f);
            out.push_back({ m, i, { x, y, x + w, y + sfont.Size + 2.0f } });
            lastright = x + w;
        }
    }
    return out;
}

void Game::EditorBeginEdit(std::string label, std::string initial, std::function<void(const std::string&)> done) {
#ifndef __ANDROID__
    if (Editor.Editing) { sys::EndTextInput(Editor.EditHandle); Editor.Editing = false; }     // 入力中にもう 1 つ開いても前のハンドルを残さない
    Editor.EditHandle = sys::BeginTextInput("", (size_t)(1024), false);
    sys::SetTextInputString(Editor.EditHandle, initial.c_str());
    ((void)0);
    sys::SetTextInputFont(EditorFont().Handle);
    Editor.Editing = true;
    Editor.EditLabel = std::move(label);
    Editor.OnEdit = std::move(done);
#else
    bool accepted = false;
    const std::string value = GetInputText(label, initial, &accepted);
    if (accepted) { done(value); }
#endif
}

std::string Game::EditorFieldValue(int index) const {
    const auto& field = Editor.FormFields[index];
#ifndef __ANDROID__
    if (field.Handle != -1) { return sys::TextInputString(field.Handle); }
#endif
    return field.Value;
}

void Game::EditorFinishFields(bool apply, std::vector<std::string> values) {
    if (!Editor.FormOpen) { return; }
    if (apply && values.empty()) { for (int i = 0; i < (int)Editor.FormFields.size(); ++i) { values.push_back(EditorFieldValue(i)); } }
    for (auto& field : Editor.FormFields) {
#ifndef __ANDROID__
        if (field.Handle != -1) { sys::EndTextInput(field.Handle); }
#endif
    }
    auto done = std::move(Editor.FormDone);
    Editor.FormFields.clear(); Editor.FormOpen = false;
#ifndef __ANDROID__
    Input::GetInstance().ResyncKeys();
#endif
    if (apply && done) { done(values); }
}

void Game::EditorFocusField(int index) {
    if (Editor.FormFields.empty()) { return; }
    Editor.FormActive = (index + (int)Editor.FormFields.size()) % (int)Editor.FormFields.size();
#ifndef __ANDROID__
    // キーと IME を選んだ欄に向ける (選択肢の欄は入力欄を持たない)
    if (const int handle = Editor.FormFields[(size_t)Editor.FormActive].Handle; handle != -1) { sys::SetActiveTextInput(handle); }
#endif
}

void Game::EditorCycleField(int index, int direction) {
    auto& field = Editor.FormFields[index];
    if (field.Choices.empty()) { return; }
    const auto it = std::ranges::find(field.Choices, field.Value);
    const int n = (int)field.Choices.size(), current = it == field.Choices.end() ? 0 : (int)(it - field.Choices.begin());
    field.Value = field.Choices[(current + direction + n) % n];
}

void Game::EditorBeginFields(std::string title, std::vector<editform::Field> fields, std::function<void(const std::vector<std::string>&)> done) {
    EditorFinishFields(false);
    EditorStopPreview();
    Editor.FormTitle = std::move(title); Editor.FormFields = std::move(fields); Editor.FormDone = std::move(done); Editor.FormOpen = true;
#ifndef __ANDROID__
    for (auto& field : Editor.FormFields) {
        if (!field.Choices.empty()) { continue; }
        field.Handle = sys::BeginTextInput("", (size_t)(1024), false);
        if (field.Handle == -1) { EditorFinishFields(false); EditorMessage("入力欄を作れませんでした"); return; }
        sys::SetTextInputString(field.Handle, field.Value.c_str());
    }
    sys::SetTextInputFont(EditorFont().Handle);
    EditorFocusField(0); Input::GetInstance().ResyncKeys();
#else
    json schema = json::array();
    for (const auto& field : Editor.FormFields) { schema.push_back({ { "label", field.Label }, { "value", field.Value }, { "choices", field.Choices } }); }
    if (!AndroidCallTwoStrings("StartEditorFields", Editor.FormTitle, schema.dump())) { EditorFinishFields(false); EditorMessage("入力画面を開けませんでした"); }
#endif
}

void Game::EditorBeginGradient() {
    if (!Editor.Selecting) { EditorMessage("グラデーションは範囲を選んでから開いてください"); return; }
    EditorBeginFields("範囲選択のグラデーション", {
        { "命令", "#SCROLL", { "#SCROLL", "#BPMCHANGE", "#DELAY" } }, { "開始値", "1" }, { "終了値", "2" },
        { "曲線", "linear", { "linear", "sine", "quad", "cubic", "quart", "quint", "expo", "circ", "back", "elastic", "bounce" } },
        { "向き", "in", { "in", "out", "inout", "outin" } }, { "適用先", "音符ごと", { "音符ごと", "小節ごと" } }
    }, [this](const auto& v) { EditorInsertCommand(v[0] + " " + v[1] + ">" + v[2] + " " + v[3] + " " + v[4] + (v[5] == "小節ごと" ? " bar" : "")); });
}

void Game::EditorUniformScroll(double visualBpm) {
    auto* course = EditorCourse();
    if (!course || !Editor.Selecting) { EditorMessage("見た目BPMの統一は範囲を選んでから開いてください"); return; }
    const auto [a,b] = EditorSelection();
    try {
        auto result = uniformscroll::Apply(*course, tja::Document::GetValue(Editor.Doc.Header,"BPM"),
            {a.Measure,a.Num,a.Den}, {b.Measure,std::min(b.Num+1,b.Den),b.Den}, visualBpm);
        auto proposed = Editor.Doc;
        proposed.Courses[Editor.Course] = std::move(result);
        proposed.Write();
        EditorChange(); Editor.Doc = std::move(proposed);
        EditorMessage("範囲の見た目BPMを " + EditorNum(visualBpm) + " に統一しました (Ctrl+Zで戻せます)");
    } catch (const std::exception& e) { EditorMessage(e.what()); }
}

void Game::EditorBeginUniformScroll() {
    if (!Editor.Selecting) { EditorMessage("見た目BPMの統一は範囲を選んでから開いてください"); return; }
    const auto [a,b] = EditorSelection();
    std::string initial = EditorCommandBefore(a.Measure,a.Num,a.Den,"#BPMCHANGE");
    double bpm = initial.empty() ? std::atof(tja::Document::GetValue(Editor.Doc.Header,"BPM").c_str()) : EditorCommandValue(initial,10);
    const auto scroll = EditorCommandBefore(a.Measure,a.Num,a.Den,"#SCROLL");
    if (!scroll.empty()) bpm *= EditorCommandValue(scroll,7);
    if (!std::isfinite(bpm) || bpm <= 0) bpm = 120;
    EditorBeginFields("範囲選択の見た目BPMを統一 (#SCROLL)", {{"見た目BPM (BPM × SCROLL)",EditorNum(bpm)}}, [this](const auto& values) {
        try { EditorUniformScroll(uniformscroll::Bpm(values.front())); }
        catch (const std::exception& e) { EditorMessage(e.what()); }
    });
}

void Game::EditorEditCommandValues(const std::string& text, std::function<void(const std::string&)> done) {
    const auto command = editform::SplitCommand(text);
    if (command.Fields.empty() || command.Fields.size() > 12) { EditorBeginEdit("命令を編集", text, std::move(done)); return; }
    if (command.Key == "#MEASURE") {
        // #MEASURE: 小節を縮める時に音符の間隔を保つか (設定に残す)。伸ばす時はいつも保つ
        auto fields = command.Fields;
        fields.push_back({ "縮めても間隔を保つ", Config.EditorKeepSpacingOnShrink ? "ON" : "OFF", { "OFF", "ON" } });
        EditorBeginFields(command.Key + " の値 (縮めても間隔を保つ ON: はみ出した音符は消す)", std::move(fields), [this, command, done = std::move(done)](const auto& values) {
            if (values.empty()) { return; }
            const bool keep = values.back() == "ON";
            if (keep != Config.EditorKeepSpacingOnShrink) { Config.EditorKeepSpacingOnShrink = keep; Config.Write(); }
            done(command.Join(std::vector<std::string>(values.begin(), values.end() - 1)));
        });
        return;
    }
    EditorBeginFields(command.Key + " の値", command.Fields, [command, done = std::move(done)](const auto& values) { done(command.Join(values)); });
}

void Game::EditorEditGimmickField(int index) {
    std::vector<editform::Field> fields;
    std::istringstream names(EditorGimmicks()[index].Params), defaults(EditorGimmicks()[index].Default);
    std::string label, value;
    while (defaults >> value) { if (!(names >> label)) { label = "値"; } fields.push_back({ label, value }); }
    // 慣性移動の BPM は、カーソルの位置の BPM を最初に入れておく
    if ((index == 16 || index == 17) && !fields.empty()) { fields[0].Value = EditorNum(EditorTimingAt(Editor.CursorMeasure).Bpm); }
    EditorBeginFields(EditorGimmicks()[index].Name, std::move(fields), [this, index](const auto& values) {
        std::string params;
        for (const auto& v : values) { if (tja::Trim(v).empty()) { EditorMessage("値を入力してください"); return; } params += (params.empty() ? "" : " ") + v; }
        EditorInsertGimmick(index, params);
    });
}

const std::vector<std::pair<std::string, std::string>>& Game::EditorCommandTemplates() {
    static const std::vector<std::pair<std::string, std::string>> list = {
        { "#BPMCHANGE", "#BPMCHANGE 120" }, { "#SCROLL", "#SCROLL 1" }, { "#MEASURE", "#MEASURE 4/4" }, { "#DELAY", "#DELAY 0.5" },
        { "#GOGOSTART", "#GOGOSTART" }, { "#GOGOEND", "#GOGOEND" }, { "#BARLINEOFF", "#BARLINEOFF" }, { "#BARLINEON", "#BARLINEON" },
        { "#SECTION", "#SECTION" }, { "#BRANCHSTART", "#BRANCHSTART p,80,90" }, { "#N", "#N" }, { "#E", "#E" }, { "#M", "#M" },
        { "#BRANCHEND", "#BRANCHEND" }, { "#LEVELHOLD", "#LEVELHOLD" }, { "自由入力…", "#" },
        { "グラデーション…", "#SCROLL 1>2 linear in" },
        { "見た目BPMを統一…", "uniform-scroll" },
    };
    return list;
}

std::string Game::EditorCommandBefore(int measure, int64_t num, int64_t den, std::string_view key) {
    const auto* course = EditorCourse();
    if (course == nullptr) { return {}; }
    std::string last;
    for (int m = 0; m <= measure && m < (int)course->Measures.size(); ++m) {
        for (const auto& c : course->Measures[m].Commands) {
            if (m == measure && tja::ComparePos(c.Num, c.Den, num, den) > 0) { break; }
            if (tja::StartsWithKey(c.Text, key)) { last = c.Text; }
        }
    }
    return last;
}

void Game::EditorBalloonPlaced(int measure, int64_t num, int64_t den, bool inserted) {
    auto* course=EditorCourse(); if (!course) return;
    int ordinal=0;
    for (int m=0;m<=measure;++m) for (const auto& n:course->Measures[m].Notes)
        if ((n.Type=='7'||n.Type=='9') && (m<measure||tja::ComparePos(n.Num,n.Den,num,den)<0)) ++ordinal;
    std::vector<std::string> counts; std::string value;
    std::istringstream input(tja::Document::GetValue(course->Header,"BALLOON"));
    while(std::getline(input,value,',')) counts.push_back(std::string(tja::Trim(value)));
    while((int)counts.size()<ordinal) counts.push_back("5");
    if(inserted) counts.insert(counts.begin()+ordinal,"5");
    while((int)counts.size()<=ordinal) counts.push_back("5");
    const int c=Editor.Course;
    auto write=[this,c](const auto& list) { std::string joined; for(const auto& v:list){if(!joined.empty())joined+=",";joined+=v;} tja::Document::SetValue(Editor.Doc.Courses[c].Header,"BALLOON",joined); ++Editor.Revision; Editor.Dirty=true; };
    write(counts);
    EditorBeginFields("風船の打数",{{"打数 (1以上)",counts[ordinal]}},[this,c,ordinal,counts,write](const auto& values) mutable {
        int hits=0; const auto text=tja::Trim(values.front()); const auto p=std::from_chars(text.data(),text.data()+text.size(),hits);
        if(p.ec!=std::errc()||p.ptr!=text.data()+text.size()||hits<1||hits>1000000){EditorMessage("打数は1～1000000の整数で指定してください (元の打数を維持)");return;}
        if(c>=(int)Editor.Doc.Courses.size())return;
        counts[ordinal]=std::to_string(hits);write(counts);EditorMessage("風船の打数を設定しました");
    });
}

void Game::EditorInsertCommand(const std::string& text) {
    const std::string t(strtrim(text));
    if (t.empty() || t == "#") { return; }
    auto* course = EditorCourse();
    if (course == nullptr) { return; }
    // #MEASURE で小節の長さを変える時は、音符の拍の位置を変えずに小節の長さだけを変える
    const bool measureCommand = tja::StartsWithKey(t, "#MEASURE");
    const std::vector<EditorFrac> measureBefore = measureCommand ? EditorMeasureFracs() : std::vector<EditorFrac>{};
    // グラデーション: 値を「始まり>終わり」と書くと、範囲の音符ごと (後ろに「小節」と書くと小節線ごと) に、値を少しずつ変えて入れる
    //   例: #SCROLL 1>3 (音符ごと)、#BPMCHANGE 120>240 小節 (小節線ごと)
    if (const size_t gt = t.find('>'); gt != std::string::npos && (tja::StartsWithKey(t, "#SCROLL") || tja::StartsWithKey(t, "#BPMCHANGE") || tja::StartsWithKey(t, "#DELAY"))) {
        if (!Editor.Selecting) { EditorMessage("グラデーションは範囲を選んでから入れてください (Shift+クリック / Shift+矢印)"); return; }
        const size_t sp = t.find_first_of(" \t");
        if (sp == std::string::npos || sp > gt) { EditorMessage("「#SCROLL 1>3」のように 命令 始まり>終わり で書いてください"); return; }
        const std::string key = t.substr(0, sp);
        std::string rest(strtrim(std::string_view(t).substr(sp)));
        bool bars = false;
        ease::Base easingBase = ease::Base::In;      // 書かなければ linear in (等間隔に変える)
        ease::Line easingLine = ease::Line::Linear;
        if (const size_t w = rest.find_first_of(" \t"); w != std::string::npos) {
            const std::string opt(strtrim(std::string_view(rest).substr(w)));
            std::istringstream options(opt);
            std::string token;
            while (options >> token) {
                token = ToLower(token);
                if (token == "小節" || token == "小節線" || token == "bar" || token == "measure") { bars = true; }
                else if (token == "in") { easingBase = ease::Base::In; }
                else if (token == "out") { easingBase = ease::Base::Out; }
                else if (token == "inout") { easingBase = ease::Base::InOut; }
                else if (token == "outin") { easingBase = ease::Base::OutIn; }
                else {
                    static const std::pair<const char*, ease::Line> names[] = {
                        { "linear", ease::Line::Linear }, { "sine", ease::Line::Sine }, { "quad", ease::Line::Quad },
                        { "cubic", ease::Line::Cubic }, { "quart", ease::Line::Quart }, { "quint", ease::Line::Quint },
                        { "expo", ease::Line::Expo }, { "circ", ease::Line::Cric }, { "back", ease::Line::Back },
                        { "elastic", ease::Line::Elastic }, { "bounce", ease::Line::Bounce }
                    };
                    auto found = std::ranges::find_if(names, [&](const auto& n) { return token == n.first; });
                    if (found == std::end(names)) { EditorMessage("不明なイージング: " + token); return; }
                    easingLine = found->second;
                }
            }
            rest = rest.substr(0, w);
        }
        const size_t g2 = rest.find('>');
        char* e1 = nullptr; char* e2 = nullptr;
        const std::string s0 = rest.substr(0, g2), s1 = g2 == std::string::npos ? std::string() : rest.substr(g2 + 1);
        const double v0 = std::strtod(s0.c_str(), &e1), v1 = std::strtod(s1.c_str(), &e2);
        if (s0.empty() || s1.empty() || *e1 != '\0' || *e2 != '\0' || !std::isfinite(v0) || !std::isfinite(v1)) { EditorMessage("値が読めません: " + rest); return; }
        const auto [a, b] = EditorSelection();
        // 入れる位置: 範囲の音符 (連打の終わり 8 は除く) か、範囲の中の小節の頭
        std::vector<std::tuple<int, int64_t, int64_t>> points;
        for (int m = a.Measure; m <= b.Measure && m < (int)course->Measures.size(); ++m) {
            if (bars) { if (EditorInSelection(m, 0, 1)) { points.push_back({ m, 0, 1 }); } continue; }
            for (const auto& n : course->Measures[m].Notes) {
                if (n.Type != '8' && EditorInSelection(m, n.Num, n.Den)) { points.push_back({ m, n.Num, n.Den }); }
            }
        }
        if (points.empty()) { EditorMessage(bars ? "範囲に小節の頭がありません" : "範囲に音符がありません"); return; }
        EditorChange();
        for (size_t i = 0; i < points.size(); ++i) {
            const double v = points.size() == 1 ? v0 : v0 + (v1 - v0) * ease::GetEasingRate((double)i / (double)(points.size() - 1), easingBase, easingLine);
            const auto& [m, num, den] = points[i];
            auto& cmds = course->Measures[m].Commands;
            // 同じ位置の同じ命令は置き換える
            std::erase_if(cmds, [&](const tja::Command& c) { return tja::ComparePos(c.Num, c.Den, num, den) == 0 && tja::StartsWithKey(c.Text, key); });
            tja::Document::AddCommand(course->Measures[m], num, den, key + " " + EditorNum(v));
        }
        EditorMessage(key + " を " + EditorNum(v0) + " から " + EditorNum(v1) + " まで、" + std::to_string(points.size()) + (bars ? " 本の小節線" : " 個の音符") + "に入れました");
        return;
    }
    // 範囲を選んでいる時: 範囲の始まりに入れ、範囲の終わり (最後のグリッドの次) で元に戻す命令も入れる
    //   #GOGOSTART → #GOGOEND、#BARLINEOFF → #BARLINEON、#SCROLL / #BPMCHANGE → 範囲の前の値
    if (Editor.Selecting) {
        const auto [a, b] = EditorSelection();
        int64_t en = b.Num + 1, ed = b.Den;     // 終わり = 選んだ最後の位置の 1 グリッド後 (小節の終わりなら 1)
        int em = b.Measure;
        if (en > ed) { en = ed; }
        std::string restore;
        if (tja::StartsWithKey(t, "#GOGOSTART")) { restore = "#GOGOEND"; }
        else if (tja::StartsWithKey(t, "#BARLINEOFF")) { restore = "#BARLINEON"; }
        else if (tja::StartsWithKey(t, "#SCROLL")) {
            restore = EditorCommandBefore(em, en, ed, "#SCROLL");
            if (restore.empty()) { restore = "#SCROLL 1"; }
        }
        else if (tja::StartsWithKey(t, "#BPMCHANGE")) {
            restore = EditorCommandBefore(em, en, ed, "#BPMCHANGE");
            if (restore.empty()) { const auto v = tja::Document::GetValue(Editor.Doc.Header, "BPM"); restore = "#BPMCHANGE " + (v.empty() ? std::string("120") : v); }
        }
        EditorChange();
        tja::Document::AddCommand(course->Measures[a.Measure], a.Num, a.Den, t);
        if (!restore.empty() && em < (int)course->Measures.size()) { tja::Document::AddCommand(course->Measures[em], en, ed, restore); }
        if (measureCommand) { EditorKeepNotePositions(measureBefore); }
        EditorMessage("範囲に命令を入れました: " + t + (restore.empty() ? "" : " … " + restore));
        return;
    }
    const int division=EditorDivOf(Editor.CursorMeasure);
    EditorChange();
    tja::Document::AddCommand(course->Measures[Editor.CursorMeasure], Editor.CursorSlot, division, t);
    if (measureCommand) { EditorKeepNotePositions(measureBefore); }
    ++Editor.Revision;
    EditorMessage("命令を挿入しました: " + t);
}

auto Game::EditorParseFrac(std::string_view text) -> std::optional<EditorFrac> {
    text = tja::Trim(text);
    if (text.empty()) { return std::nullopt; }
    auto num = [](std::string_view v) -> std::optional<double> {
        v = tja::Trim(v);
        if (v.empty()) { return std::nullopt; }
        char* end = nullptr;
        const std::string str(v);
        const double d = std::strtod(str.c_str(), &end);
        if (end != str.c_str() + str.size() || !std::isfinite(d)) { return std::nullopt; }
        return d;
    };
    if (const size_t slash = text.find('/'); slash != std::string_view::npos) {
        const auto n = num(text.substr(0, slash)), d = num(text.substr(slash + 1));
        if (!n || !d || *d == 0) { return std::nullopt; }
        return EditorFrac{ *n, *d };
    }
    const auto n = num(text);
    if (!n) { return std::nullopt; }
    return EditorFrac{ *n, 1 };
}

std::string Game::EditorNum(double v) {
    if (std::abs(v) < 1e-12) { v = 0; }
    std::string out = std::format("{:.6f}", v);
    while (out.find('.') != std::string::npos && (out.back() == '0' || out.back() == '.')) {
        const bool dot = out.back() == '.';
        out.pop_back();
        if (dot) { break; }
    }
    return out == "-0" ? "0" : out;
}

std::string Game::EditorFracText(double num, double den) { return EditorNum(num) + "/" + EditorNum(den); }

auto Game::EditorTimingAt(int measure) -> EditorTiming {
    const auto* course = EditorCourse();
    if (course == nullptr || measure < 0) { return {}; }
    if (EditorTimingRevision != Editor.Revision || EditorTimingCourse != Editor.Course || EditorTimingCache.size() != course->Measures.size()) {
        EditorTiming t;
        if (const auto v = tja::Document::GetValue(Editor.Doc.Header, "BPM"); !v.empty()) { t.Bpm = std::atof(v.c_str()); }
        auto apply = [&](const tja::Command& c) {
            if (tja::StartsWithKey(c.Text, "#BPMCHANGE")) { t.Bpm = EditorCommandValue(c.Text, 10); }
            else if (tja::StartsWithKey(c.Text, "#MEASURE")) {
                if (auto f = EditorParseFrac(std::string_view(c.Text).substr(8))) { t.Measure = *f; }
            }
            if (t.Bpm == 0 || !std::isfinite(t.Bpm)) { t.Bpm = 120; }
        };
        EditorTimingCache.clear();
        for (const auto& m : course->Measures) {
            for (const auto& c : m.Commands) { if (c.Num == 0) { apply(c); } }
            EditorTimingCache.push_back(t);
            for (const auto& c : m.Commands) { if (c.Num != 0) { apply(c); } }
        }
        EditorTimingAfterLast = t;
        EditorTimingRevision = Editor.Revision;
        EditorTimingCourse = Editor.Course;
    }
    return measure >= (int)EditorTimingCache.size() ? EditorTimingAfterLast : EditorTimingCache[measure];
}

auto Game::EditorGimmicks() -> const std::vector<EditorGimmick>& {
    static const std::vector<EditorGimmick> list = {
        { "BPM変化 (加速・減速)", "倍率 長さ(小節)", "2 1/1", "BPM と #MEASURE に同じ倍率を掛け、その長さだけ速く (遅く) 流す" },
        { "逆走", "長さ(小節) 速さの倍率", "1/2 1", "負の BPM と負の #MEASURE で、音符を逆向きに流す" },
        { "DELAY停止", "止める長さ(小節)", "1/1", "カーソルの位置に #DELAY を入れて譜面を止める (秒数は BPM から計算)" },
        { "疑似停止", "止める長さ(小節)", "1/1", "BPM を 1/1000 にして、その長さだけ止まって見せる (音符は置けない)" },
        { "瞬間移動 (バグ式)", "移動する距離(小節)", "2", "BPM X → -X の 0 秒の小節で、後ろの音符を一瞬で手前へ移す。残像なし" },
        { "瞬間移動 (120000式)", "移動する距離(小節)", "1", "BPM 120000 の小節で移動し、ずれを #DELAY で打ち消す" },
        { "瞬間移動 (BPM計算型)", "長さ(小節) 倍率", "1/4 100", "BPM と #MEASURE に同じ倍率を掛けて、残像つきで素早く動かす" },
        { "途中出現", "出現する位置(小節)", "1/2", "小節の途中から後ろの音符を出現させる (BPM の変化と組み合わせない)" },
        { "痙攣", "長さ(小節) 間隔(分)", "1/4 16", "逆走と順走を細かく繰り返して、音符をその場で震わせる" },
        { "点滅 (逆走式)", "長さ(小節) 間隔(分)", "1/4 16", "瞬間移動で音符を画面の外と行き来させて点滅させる" },
        { "点滅 (120000式)", "長さ(小節) 間隔(分)", "1/4 16", "BPM ±120000 の小節で点滅させる (#DELAY でずれを打ち消す)" },
        { "DELAY±", "長さ(小節)", "1/1", "#DELAY +t と -t を続けて書き、止まったまま離れた音符を判定させる" },
        { "背面停止", "長さ(小節)", "1/1", "疑似停止中に SCROLL 1000 の音符を流し、奥の音符を止めたまま手前を流す" },
        { "背面逆走", "長さ(小節)", "1/1", "逆走中に SCROLL -1 の音符を流す" },
        { "連打背景", "なし (Enter)", "", "1 つ前の小節に黄色連打を重ねる (マイナス #MEASURE 式)" },
        { "ダミーノーツ (逆走式)", "なし (Enter)", "", "カーソルの小節の音符を、譜面の最後に書いたダミーとして同じ場所に重ねて見せる" },
        { "慣性移動", "BPM 時間(小節) 距離(小節) 段数 段ごとの速さの倍率", "200 2/4 4/2 4 1/2", "「BPM の 時間(小節) の長さ」の間に、距離(小節) だけ段数の段に分けて減速しながら進む (例: BPM 200 の 2/4 小節の時間で 4/2 小節ぶん進む)。最後に元の BPM・拍子へ戻す" },
        { "慣性移動 (加速)", "BPM 時間(小節) 距離(小節) 段数 段ごとの速さの倍率", "200 2/4 4/2 4 2", "「BPM の 時間(小節) の長さ」の間に、距離(小節) だけ段数の段に分けて加速しながら進む (例: BPM 200 の 2/4 小節の時間で 4/2 小節ぶん進む)。最後に元の BPM・拍子へ戻す" },
        { "コマ送り (範囲)", "移動距離の倍率", "1", "選んだ範囲を別の小節に分け、音符ごとに「止まる → 次の音符までの距離だけ瞬間移動 (バグ式)」に作り直して、音符が 1 つずつコマ送りで進んで見えるようにする (叩く時刻は変わらない。倍率 1 で元の間隔)" },
        { "痙攣 (範囲)", "振れ幅", "1/2", "選んだ範囲を別の小節に分け、音符の間隔に合わせた区切りごとに「速く前へ進む → 振れ幅 (区切りに対する割合。0 より大きく 1 より小さい) だけ戻る」に作り直して、音符が元の速さで来ながら震えて見えるようにする (区切りは範囲の一番細かい音符の間隔 (4 分より粗い時は 4 分) に自動で合わせる。叩く時刻は変わらない)" },
    };
    return list;
}

std::optional<std::vector<std::string>> Game::EditorBuildGimmick(int index, const std::string& params, int& place, std::string& error) {
    std::vector<std::string> args;
    for (auto part : split(params, ' ')) {
        if (!tja::Trim(part).empty()) { args.emplace_back(tja::Trim(part)); }
    }
    auto frac = [&](size_t i, const char* def) -> std::optional<EditorFrac> {
        return EditorParseFrac(i < args.size() ? std::string_view(args[i]) : std::string_view(def));
    };
    const EditorTiming t = EditorTimingAt(Editor.CursorMeasure);
    const double B = t.Bpm;
    const std::string bpm = EditorNum(B), nbpm = EditorNum(-B);
    const std::string restoreMeasure = "#MEASURE " + EditorFracText(t.Measure.Num, t.Measure.Den);
    std::vector<std::string> out;
    auto restore = [&] { out.push_back("#BPMCHANGE " + bpm); out.push_back(restoreMeasure); };
    auto bad = [&](const char* msg) { error = msg; return std::nullopt; };
    place = 0;
    switch (index) {
    case 0: {   // BPM変化
        const auto x = frac(0, "2"), len = frac(1, "1/1");
        if (!x || !len || x->Value() <= 0 || len->Value() <= 0) { return bad("倍率と長さは正の値にしてください (例: 2 1/1)"); }
        const double k = x->Value();
        if (B * k >= 120000) { return bad("BPM が 120000 以上になります (次郎では落ちます)"); }
        out = { "#BPMCHANGE " + EditorNum(B * k), "#MEASURE " + EditorFracText(len->Num * k, len->Den), "0," };
        restore();
        break;
    }
    case 1: {   // 逆走
        const auto len = frac(0, "1/2"), x = frac(1, "1");
        if (!len || !x || len->Value() <= 0 || x->Value() <= 0) { return bad("長さと倍率は正の値にしてください (例: 1/2 1)"); }
        out = { "#BPMCHANGE " + EditorNum(-B * x->Value()), "#MEASURE " + EditorFracText(-len->Num * x->Value(), len->Den), "0," };
        restore();
        break;
    }
    case 2: {   // DELAY停止
        const auto len = frac(0, "1/1");
        if (!len || len->Value() <= 0) { return bad("長さは正の値にしてください"); }
        place = 1;
        out = { "#DELAY " + EditorNum(std::round(len->Value() * 240.0 / std::abs(B) * 1000.0) / 1000.0) };     // 次郎は小数第 3 位まで読む
        break;
    }
    case 3: {   // 疑似停止
        const auto len = frac(0, "1/1");
        if (!len || len->Value() <= 0) { return bad("長さは正の値にしてください"); }
        out = { "#BPMCHANGE " + EditorNum(B / 1000.0), "#MEASURE " + EditorFracText(len->Num, len->Den * 1000.0), "0," };
        restore();
        break;
    }
    case 4: {   // 瞬間移動 (バグ式)
        const auto d = frac(0, "2");
        if (!d || d->Value() <= 0) { return bad("距離は正の値にしてください"); }
        out = { "#BPMCHANGE " + bpm, "#MEASURE " + EditorFracText(d->Num, d->Den), "0", "#BPMCHANGE " + nbpm, "0," };
        restore();
        break;
    }
    case 5: {   // 瞬間移動 (120000式)
        const auto d = frac(0, "1");
        if (!d || d->Value() <= 0) { return bad("距離は正の値にしてください"); }
        out = { "#DELAY " + EditorNum(-std::round(0.002 * d->Value() * 1000.0) / 1000.0), "#BPMCHANGE 120000", "#MEASURE " + EditorFracText(d->Num, d->Den), "0," };
        restore();
        break;
    }
    case 6: {   // 瞬間移動 (BPM計算型)
        const auto len = frac(0, "1/4"), n = frac(1, "100");
        if (!len || !n || len->Value() <= 0 || n->Value() <= 0) { return bad("長さと倍率は正の値にしてください (例: 1/4 100)"); }
        if (B * n->Value() >= 120000) { return bad("BPM × 倍率 が 120000 以上になります (倍率を下げてください)"); }
        out = { "#BPMCHANGE " + EditorNum(B * n->Value()), "#MEASURE " + EditorFracText(len->Num * n->Value(), len->Den), "#BARLINEOFF", "0," };
        restore();
        out.push_back("#BARLINEON");
        break;
    }
    case 7: {   // 途中出現
        const auto pos = frac(0, "1/2");
        if (!pos || pos->Value() <= 0 || pos->Value() >= t.Measure.Value()) { return bad("出現する位置は 0 より大きく、小節の長さより小さくしてください"); }
        const double rest = t.Measure.Value() - pos->Value();
        out = { "#BPMCHANGE " + bpm, "#MEASURE " + EditorFracText(pos->Num, pos->Den), "0,", "#MEASURE 2/1", "0", "#BPMCHANGE " + nbpm, "0,",
            "#BPMCHANGE " + bpm, "#MEASURE " + EditorFracText(rest * 1000.0, 1000.0), "0," };
        restore();
        break;
    }
    case 8: case 9: case 10: {   // 痙攣・点滅
        const auto len = frac(0, "1/4"), iv = frac(1, "16");
        if (!len || !iv || len->Value() <= 0 || iv->Value() < 1) { return bad("長さと間隔を正しく入れてください (例: 1/4 16)"); }
        const double sets = len->Value() * iv->Value();
        const int count = (int)std::llround(sets);
        if (count < 1 || std::abs(sets - count) > 1e-6) { return bad("長さ × 間隔 が整数になるようにしてください (例: 1/4 と 16 → 4 セット)"); }
        if (count > 256) { return bad("長すぎます (256 セットまで)"); }
        const std::string step = EditorFracText(1, iv->Value() * 2);
        for (int i = 0; i < count; ++i) {
            if (index == 8) {
                out.insert(out.end(), { "#BPMCHANGE " + nbpm, "#MEASURE -" + step, "0,", "#BPMCHANGE " + bpm, "#MEASURE " + step, "0," });
            }
            else if (index == 9) {
                out.insert(out.end(), { "#BPMCHANGE " + nbpm, "#MEASURE -2/1", "0", "#BPMCHANGE " + bpm, "0,", "#BPMCHANGE " + bpm, "#MEASURE " + step, "0,",
                    "#BPMCHANGE " + bpm, "#MEASURE 2/1", "0", "#BPMCHANGE " + nbpm, "0,", "#BPMCHANGE " + bpm, "#MEASURE " + step, "0," });
            }
            else {
                out.insert(out.end(), { "#BPMCHANGE -120000", "#MEASURE -2/1", "0,", "#BPMCHANGE " + bpm, "#MEASURE " + step, "0", "#DELAY -0.004", "0,",
                    "#BPMCHANGE 120000", "#MEASURE 2/1", "0,", "#BPMCHANGE " + bpm, "#MEASURE " + step, "0", "#DELAY -0.004", "0," });
            }
        }
        restore();
        break;
    }
    case 11: {  // DELAY±
        const auto len = frac(0, "1/1");
        if (!len || len->Value() <= 0) { return bad("長さは正の値にしてください"); }
        const std::string sec = EditorNum(std::round(len->Value() * 240.0 / std::abs(B) * 1000.0) / 1000.0);
        place = 1;
        out = { "#DELAY " + sec, "#DELAY -" + sec };
        break;
    }
    case 12: {  // 背面停止
        const auto len = frac(0, "1/1");
        if (!len || len->Value() <= 0) { return bad("長さは正の値にしてください"); }
        out = { "#BPMCHANGE " + EditorNum(B / 1000.0), "#MEASURE " + EditorFracText(len->Num, len->Den * 1000.0), "#SCROLL 1000", "0," };
        restore();
        out.push_back("#SCROLL 1");
        break;
    }
    case 13: {  // 背面逆走
        const auto len = frac(0, "1/1");
        if (!len || len->Value() <= 0) { return bad("長さは正の値にしてください"); }
        out = { "#BPMCHANGE " + nbpm, "#MEASURE " + EditorFracText(-len->Num, len->Den), "#SCROLL -1", "0," };
        restore();
        out.push_back("#SCROLL 1");
        break;
    }
    case 14: {  // 連打背景
        if (Editor.CursorMeasure == 0) { return bad("1 小節目の前には置けません (重ねる小節がありません)"); }
        const EditorTiming prev = EditorTimingAt(Editor.CursorMeasure - 1);
        out = { "#BPMCHANGE " + EditorNum(prev.Bpm), "#MEASURE " + EditorFracText(-prev.Measure.Num, prev.Measure.Den), "0,",
            "#MEASURE " + EditorFracText(prev.Measure.Num, prev.Measure.Den), "5000000000000000000000000000008," };
        restore();
        break;
    }
    case 15: {  // ダミーノーツ (逆走式)
        const auto* course = EditorCourse();
        // カーソルの小節の頭から譜面の終わりまでの距離 (小節単位。#HBSCROLL では BPM によらず拍子の合計)
        double distance = 0;
        EditorFrac measure = t.Measure;
        for (int m = Editor.CursorMeasure; m < (int)course->Measures.size(); ++m) {
            for (const auto& c : course->Measures[m].Commands) {
                if (c.Num == 0 && tja::StartsWithKey(c.Text, "#MEASURE")) {
                    if (auto f = EditorParseFrac(std::string_view(c.Text).substr(8))) { measure = *f; }
                }
            }
            distance += measure.Value();
            for (const auto& c : course->Measures[m].Commands) {
                if (c.Num != 0 && tja::StartsWithKey(c.Text, "#MEASURE")) {
                    if (auto f = EditorParseFrac(std::string_view(c.Text).substr(8))) { measure = *f; }
                }
            }
        }
        const EditorTiming end = EditorTimingAt((int)course->Measures.size());
        const auto& src = course->Measures[Editor.CursorMeasure];
        std::string notes(std::max<int64_t>(src.Resolution, 1), '0');
        for (const auto& n : src.Notes) {
            const int64_t i = n.Num * (int64_t)notes.size() / std::max<int64_t>(n.Den, 1);
            if (i >= 0 && i < (int64_t)notes.size() && n.Num * (int64_t)notes.size() % std::max<int64_t>(n.Den, 1) == 0) { notes[(size_t)i] = n.Type; }
        }
        if (notes.find_first_not_of('0') == std::string::npos) { return bad("カーソルの小節に音符がありません (ダミーにしたい音符を置いた小節を選んでください)"); }
        place = 2;
        out = { "#BPMCHANGE " + EditorNum(-end.Bpm), "#MEASURE " + EditorFracText(-distance * 1000.0, 1000.0), "0,",
            "#BPMCHANGE " + bpm, "#MEASURE " + EditorFracText(t.Measure.Num, t.Measure.Den), notes + "," };
        break;
    }
    case 16:    // 慣性移動 (減速): 倍率 < 1
    case 17: {  // 慣性移動 (加速): 倍率 > 1。どちらも 1 段の長さ (時間) は同じで、流れる速さだけが倍率ずつ変わる
        // 「BPM の 時間(小節) の長さ」を段数で等分し、段 k は 距離 × 倍率^k / Σ倍率^i 小節ぶん進む。
        // #HBSCROLL では、長さ L 小節・BPM b の小節は L 小節ぶん進み、240000 × L / b ミリ秒かかるので、
        // 段の時間をそろえるには BPM_k = BPM × 段数 × L_k / 時間 にする (速さが倍率ずつ変わる)
        const bool accelerate = index == 17;
        const auto base = frac(0, "200"), time = frac(1, "2/4"), distance = frac(2, "4/2"), steps = frac(3, "4"), ratio = frac(4, accelerate ? "2" : "1/2");
        if (!base || !time || !distance || !steps || !ratio || base->Value() <= 0 || time->Value() <= 0 || distance->Value() <= 0
            || steps->Value() < 1 || steps->Value() > 16 || std::floor(steps->Value()) != steps->Value() || ratio->Value() <= 0
            || (accelerate ? ratio->Value() <= 1 : ratio->Value() >= 1)) {
            return bad(accelerate ? "BPM・時間・距離を正の値、段数を 1～16、倍率を 1 より大きくしてください (例: 200 2/4 4/2 4 2)"
                : "BPM・時間・距離を正の値、段数を 1～16、倍率を 0 より大きく 1 より小さくしてください (例: 200 2/4 4/2 4 1/2)");
        }
        const int n = (int)steps->Value();
        const double r = ratio->Value();
        double sum = 0;
        for (int k = 0; k < n; ++k) { sum += std::pow(r, k); }
        for (int k = 0; k < n; ++k) {
            const double length = distance->Value() * std::pow(r, k) / sum;          // 進む距離 = 小節の長さ
            const double speed = base->Value() * n * length / time->Value();         // 段の時間をそろえる BPM
            if (!std::isfinite(speed) || speed < 0.001 || speed >= 120000 || !std::isfinite(length) || length < 0.000001) {
                return bad("BPM が範囲外 (120000 以上か小さすぎる) になります。時間・距離・段数・倍率を調整してください");
            }
            // 拍子は分数で正確に書く: L_k = 距離 × rn^k × rd^(n-1-k) / Σ rn^i × rd^(n-1-i) (大きくなりすぎる時は 1000000 分の 1 の細かさ)
            int64_t num = (int64_t)std::llround(length * 1000000.0), den = 1000000;
            int64_t rn = 0, rd = 0, dn = 0, dd = 0;
            if (EditorToRational(*ratio, rn, rd) && EditorToRational(*distance, dn, dd)) {
                double total = 0, term = 0;
                for (int i = 0; i < n; ++i) {
                    const double t = std::pow((double)rn, i) * std::pow((double)rd, n - 1 - i);
                    total += t;
                    if (i == k) { term = t; }
                }
                if (total * (double)dd < 9e15 && term * (double)dn < 9e15) {
                    num = dn * (int64_t)std::llround(term); den = dd * (int64_t)std::llround(total);
                }
            }
            tja::Reduce(num, den);
            out.insert(out.end(), { "#BPMCHANGE " + EditorNum(std::round(speed * 1000.0) / 1000.0), "#MEASURE " + std::to_string(num) + "/" + std::to_string(den), "," });
        }
        restore();
        break;
    }
    default:
        return bad("不明なギミックです");
    }
    return out;
}

void Game::EditorInsertGimmick(int index, const std::string& params) {
    auto* course = EditorCourse();
    if (course == nullptr) { return; }
    if (index == 18) { EditorInsertFrameStep(params); return; }     // 範囲を作り直すので別の処理
    if (index == 19) { EditorInsertConvulsion(params); return; }
    int place = 0;
    std::string error;
    const auto lines = EditorBuildGimmick(index, params, place, error);
    if (!lines) { EditorMessage("ギミックを挿入できません: " + error); return; }
    EditorChange();
    const std::string name = EditorGimmicks()[index].Name;
    if (place == 1) {
        auto& m = course->Measures[Editor.CursorMeasure];
        for (const auto& line : *lines) { tja::Document::AddCommand(m, Editor.CursorSlot, EditorDivOf(Editor.CursorMeasure), line); }
    }
    else {
        std::vector<std::string_view> views{ "#START" };
        for (const auto& line : *lines) { views.emplace_back(line); }
        views.emplace_back("#END");
        tja::Document block = tja::Document::Parse(views);
        if (block.Courses.empty()) { EditorMessage("ギミックを挿入できません"); return; }
        auto& added = block.Courses.front();
        if (place == 2) {
            // 最後の小節の後ろ (#END の前の命令より前) に足す
            for (auto& m : added.Measures) { course->Measures.push_back(std::move(m)); }
            for (auto& t : added.Trailing) {
                const std::string key = EditorCommandKey(t);
                if (std::ranges::none_of(course->Trailing, [&](const std::string& c) { return EditorCommandKey(c) == key; })) { course->Trailing.insert(course->Trailing.begin(), std::move(t)); }
            }
        }
        else {
            const int at = Editor.CursorMeasure;
            // 元に戻す命令 (最後の ',' の後ろ) は、元のカーソルの小節の頭に入れる
            // 終点 (カーソルの小節の頭) に同じ種類の命令がもうある時は、その命令が効くので足さない
            auto& target = course->Measures[at];
            std::vector<tja::Command> head;
            for (auto& t : added.Trailing) {
                const std::string key = EditorCommandKey(t);
                const bool exists = std::ranges::any_of(target.Commands, [&](const tja::Command& c) { return c.Num == 0 && EditorCommandKey(c.Text) == key; });
                if (!exists) { head.push_back({ 0, 1, std::move(t) }); }
            }
            target.Commands.insert(target.Commands.begin(), head.begin(), head.end());
            course->Measures.insert(course->Measures.begin() + at, added.Measures.begin(), added.Measures.end());
            EditorRemapFolds(at, 0, (int)added.Measures.size());
        }
    }
    // 次郎のギミックは #BMSCROLL / #HBSCROLL が無いと動かない
    const bool hasscroll = std::ranges::any_of(course->Header, [](const std::string& h) { return tja::StartsWithKey(h, "#HBSCROLL") || tja::StartsWithKey(h, "#BMSCROLL"); });
    if (!hasscroll && index != 2) { course->Header.push_back("#HBSCROLL"); }
    EditorClamp();
    EditorMessage("ギミックを挿入しました: " + name + (!hasscroll && index != 2 ? "  (#HBSCROLL を追加)" : "") + (EditorJiroPreviewEnabled() ? "" : "  ※ 次郎形式再生ボタンで次郎形式の時刻表示に切り替えられます"));
}

bool Game::EditorToRational(const EditorFrac& f, int64_t& num, int64_t& den) {
    double n = f.Num, d = f.Den;
    for (int i = 0; i < 6 && (std::abs(n - std::round(n)) > 1e-9 || std::abs(d - std::round(d)) > 1e-9); ++i) { n *= 10; d *= 10; }
    num = (int64_t)std::llround(n); den = (int64_t)std::llround(d);
    if (den < 0) { num = -num; den = -den; }
    if (num <= 0 || den <= 0) { return false; }
    tja::Reduce(num, den);
    return true;
}

auto Game::EditorJiroMeasureMaps() -> std::vector<EditorTimeline::Measure> {
    std::vector<EditorTimeline::Measure> out;
    const auto* course = EditorCourse();
    if (course == nullptr) { return out; }
    double bpm = 120;
    if (const auto v = tja::Document::GetValue(Editor.Doc.Header, "BPM"); !v.empty() && std::atof(v.c_str()) != 0) { bpm = std::atof(v.c_str()); }
    double ratio = 1, pending = 0;
    bool hasPending = false;
    // ゲームの次郎形式の読み込み (Loading.cpp の JiroClock) と同じ計算:
    //   小節の頭 = 直前の #BPMCHANGE の時刻 + (その小節の残りの拍の長さを切り捨て) + (その後の小節の拍の長さの合計を切り捨て) + その後の #DELAY
    //   位置 = 小節の頭 (同じ小節に #BPMCHANGE があればその時刻) + (位置までの長さを切り捨て) + 小節の中の #DELAY
    double evTime = 0, evPos = 0, evRemain = 0, after = 0, totalDelay = 0, measureStart = 0, measureDelay = 0, next = 0;
    bool evHere = true;
    // ゲームと同じく、譜面の始まりの絶対時刻 (曲の始まりからのミリ秒) から計算する: 始まりの 1ms 未満の切り捨てと、
    // 0 より前の時刻を 0 に詰める所をゲームとそろえ、最後に譜面の時刻 (始まり = 0) に直す
    double origin = 0;
    {
        const double offset = std::atof(tja::Document::GetValue(Editor.Doc.Header, "OFFSET").c_str());
        origin = 240.0 / bpm > -offset ? 240000.0 / bpm : offset * -1000.0 - Config.SongOffset;
        origin = std::max(0.0, origin + Config.ChartOffset);
        evTime = next = std::max(0.0, JiroTrunc(origin));
    }
    // 次郎は命令名の後の決まった文字数 (#BPMCHANGE は 8、#MEASURE・#DELAY は 10) を sscanf で読み、読めなければ前に読んだ値のまま
    double scanValue = 0;
    long scanDen = 1;
    auto jiroArg = [](const std::string& text, size_t key, size_t n) {
        return text.size() <= key ? std::string() : std::string(tja::Trim(std::string_view(text).substr(key, n)));
    };
    auto scanDouble = [&](const std::string& arg) {
        char* end = nullptr;
        const double v = std::strtod(arg.c_str(), &end);
        if (end != arg.c_str()) { scanValue = v; }
        return scanValue;
    };
    auto scanMeasure = [&](const std::string& arg) {
        char* end = nullptr;
        const double v = std::strtod(arg.c_str(), &end);
        if (end != arg.c_str()) {
            scanValue = v;
            if (*end == '/') { char* end2 = nullptr; const long d = std::strtol(end + 1, &end2, 10); if (end2 != end + 1) { scanDen = d; } }
        }
        return scanDen != 0 ? scanValue / (double)scanDen : std::numeric_limits<double>::infinity();
    };
    for (const auto& m : course->Measures) {
        if (hasPending) { ratio = pending; hasPending = false; }
        for (const auto& c : m.Commands) {
            if (c.Num == 0 && tja::StartsWithKey(c.Text, "#MEASURE")) {
                if (const double v = scanMeasure(jiroArg(c.Text, 8, 10)); std::isfinite(v)) { ratio = v; }
            }
        }
        measureStart = next;
        measureDelay = 0;
        auto posTime = [&](double p) {
            const double beat = 60000.0 / bpm;
            const double base = evHere ? JiroTrunc(beat * 4.0 * ratio * (p - evPos)) + evTime : JiroTrunc(beat * 4.0 * ratio * p) + measureStart;
            return std::max(0.0, base + measureDelay);
        };
        // 命令と音符の位置を順に (同じ位置では命令が先)
        std::vector<double> positions;
        for (const auto& n : m.Notes) { positions.push_back(std::min(1.0, (double)n.Num / std::max<int64_t>(n.Den, 1))); }
        EditorTimeline::Measure map;
        map.Map.push_back({ 0, posTime(0) });
        size_t ci = 0;
        std::ranges::sort(positions);
        auto runCommands = [&](double upto, bool inclusive) {
            for (; ci < m.Commands.size(); ++ci) {
                const auto& c = m.Commands[ci];
                const double p = std::min(1.0, (double)c.Num / std::max<int64_t>(c.Den, 1));
                if (inclusive ? p > upto + 1e-12 : p >= upto - 1e-12) { break; }
                map.Map.push_back({ p, posTime(p) });
                if (tja::StartsWithKey(c.Text, "#BPMCHANGE")) {
                    double v = scanDouble(jiroArg(c.Text, 10, 8));
                    if (v == 0 || !std::isfinite(v)) { v = bpm; }
                    evTime = posTime(p); bpm = v; evPos = p; evHere = true; after = 0; measureDelay = 0; totalDelay = 0;
                }
                else if (tja::StartsWithKey(c.Text, "#DELAY")) {
                    const double d = JiroTrunc(scanDouble(jiroArg(c.Text, 6, 10)) * 1000.0);
                    if (std::isfinite(d) && std::abs(d) <= 2e9) { measureDelay += d; totalDelay += d; }
                }
                else if (c.Num != 0 && tja::StartsWithKey(c.Text, "#MEASURE")) {
                    if (const double v = scanMeasure(jiroArg(c.Text, 8, 10)); std::isfinite(v)) { pending = v; hasPending = true; }
                }
                map.Map.push_back({ p, posTime(p) });
            }
        };
        for (double p : positions) {
            runCommands(p, true);
            if (map.Map.back().first != p || map.Map.back().second != posTime(p)) { map.Map.push_back({ p, posTime(p) }); }
        }
        runCommands(1.0, true);
        if (evHere) { evRemain = (1.0 - evPos) * 4.0 * ratio; after = 0; evHere = false; }
        else { after += 4.0 * ratio; }
        const double beat = 60000.0 / bpm;
        next = std::max(0.0, evTime + JiroTrunc(beat * evRemain) + JiroTrunc(beat * after) + totalDelay);
        map.Map.push_back({ 1.0, next });
        for (auto& point : map.Map) { point.second -= origin; }
        out.push_back(std::move(map));
    }
    return out;
}

void Game::EditorFixJiroDrift() {
    auto* course = EditorCourse();
    if (course == nullptr || course->Measures.empty()) { return; }
    const bool branched = std::ranges::any_of(course->Measures, [](const tja::Measure& m) {
        return std::ranges::any_of(m.Commands, [](const tja::Command& c) { return tja::StartsWithKey(c.Text, "#BRANCHSTART"); });
    });
    if (branched) { EditorMessage("譜面分岐のあるコースは補正できません"); return; }
    const int count = (int)course->Measures.size();
    int first = 0, last = count - 1;
    if (Editor.Selecting) { const auto [a, b] = EditorSelection(); first = std::clamp(a.Measure, 0, count - 1); last = std::clamp(b.Measure, first, count - 1); }
    // 本来の時刻 (切り捨てない計算) の小節の頭
    const bool savedJiro = Editor.PreviewJiro;
    Editor.PreviewJiro = false;
    ++Editor.Revision;
    std::vector<double> exact;
    {
        const auto tl = EditorBuildTimeline();
        for (const auto& m : tl.Measures) { exact.push_back(m.Map.empty() ? 0 : m.Map.front().second); }
        exact.push_back(tl.Measures.empty() || tl.Measures.back().Map.empty() ? 0 : tl.Measures.back().Map.back().second);
    }
    Editor.PreviewJiro = savedJiro;
    // 小節の頭ごとに、次郎の時刻が本来より 1ms 以上ずれていれば、その分 (ミリ秒の整数) の #DELAY を小節の頭に入れる
    //   drift = 本来 - 次郎。正 = 次郎が早い (正の拍の長さの切り捨て) → 正の #DELAY で遅らせる
    //                        負 = 次郎が遅い (負の BPM・負の #DELAY の 0 方向への切り捨て) → 負の #DELAY で早める
    bool changed = false;
    int inserted = 0;
    double early = 0, late = 0, worst = 0;
    for (int m = std::max(first, 1); m <= last + 1 && m <= count; ++m) {
        const auto maps = EditorJiroMeasureMaps();
        const double jiroHead = m < (int)maps.size() ? maps[(size_t)m].Map.front().second : maps.back().Map.back().second;
        const double drift = exact[(size_t)m] - jiroHead;
        if (std::abs(drift) < 1.0 - 1e-6 || m >= count) { worst = std::max(worst, std::abs(drift)); continue; }
        // 次郎は #DELAY を (int)(秒 × 1000) で 0 方向に切り捨てるので、ミリ秒の整数を 0 方向に丸めて書く
        const double ms = drift > 0 ? std::floor(drift + 1e-6) : -std::floor(-drift + 1e-6);
        if (!changed) { EditorChange(); changed = true; }
        // 小節の頭の #DELAY (次郎は小数第 3 位 = ミリ秒まで読む)
        tja::Document::AddCommand(course->Measures[(size_t)m], 0, 1, "#DELAY " + EditorNum(ms / 1000.0));
        ++Editor.Revision;
        ++inserted;
        (ms > 0 ? early : late) += std::abs(ms);
        worst = std::max(worst, std::abs(drift - ms));
    }
    if (!changed) { EditorMessage("補正するずれはありませんでした (次郎の時刻と本来の時刻の差は 1ms 未満)"); return; }
    ++Editor.Revision;
    std::string detail;
    if (early > 0) { detail += "早くなる分 " + EditorNum(early) + "ms"; }
    if (late > 0) { detail += (detail.empty() ? "" : "・") + std::string("遅くなる分 ") + EditorNum(late) + "ms"; }
    EditorMessage(std::to_string(inserted) + " か所に #DELAY を入れ、次郎の切り捨てでずれる分 (" + detail + ") を打ち消しました"
        + (EditorJiroPreviewEnabled() ? "" : "  ※ 次郎形式再生ボタンで、次郎の時刻で確かめられます"));
}

namespace {
    fs::path EditorFoldsFile() { return GetExecutablePath() / "editorfolds.json"; }
    json ReadEditorFolds() {
        std::ifstream in(EditorFoldsFile());
        json data = in ? json::parse(in, nullptr, false) : json::object();
        return data.is_object() ? data : json::object();
    }
}

void Game::EditorStoreFolds() {
    if (!Editor.Open || Editor.Path.empty() || EditorCourse() == nullptr) { return; }
    try {
        json data = ReadEditorFolds();
        const std::string key = PathToU8(fs::absolute(Editor.Path).lexically_normal()), course = Editor.Doc.CourseName(Editor.Course);
        auto pairs = [](const std::vector<std::pair<int, int>>& list) { json a = json::array(); for (const auto& [f, t] : list) { a.push_back({ f, t }); } return a; };
        if (Editor.Folds.empty() && Editor.OpenFolds.empty()) {
            if (data.contains(key) && data[key].is_object()) { data[key].erase(course); if (data[key].empty()) { data.erase(key); } }
        }
        else { data[key][course] = { { "folds", pairs(Editor.Folds) }, { "open", pairs(Editor.OpenFolds) } }; }
        std::ofstream out(EditorFoldsFile());
        out << data.dump(1);
    }
    catch (...) {}      // 保存できなくても編集は続ける
}

void Game::EditorLoadFolds() {
    Editor.Folds.clear();
    Editor.OpenFolds.clear();
    if (!Editor.Open || Editor.Path.empty() || EditorCourse() == nullptr) { return; }
    try {
        const json data = ReadEditorFolds();
        const std::string key = PathToU8(fs::absolute(Editor.Path).lexically_normal()), course = Editor.Doc.CourseName(Editor.Course);
        if (!data.contains(key) || !data[key].is_object() || !data[key].contains(course)) { return; }
        const json& entry = data[key][course];
        auto read = [&](const char* name, std::vector<std::pair<int, int>>& out) {
            if (!entry.contains(name) || !entry[name].is_array()) { return; }
            for (const auto& p : entry[name]) {
                if (p.is_array() && p.size() == 2 && p[0].is_number_integer() && p[1].is_number_integer()) { out.push_back({ p[0].get<int>(), p[1].get<int>() }); }
            }
        };
        read("folds", Editor.Folds);
        read("open", Editor.OpenFolds);
    }
    catch (...) { Editor.Folds.clear(); Editor.OpenFolds.clear(); }
    EditorNormalizeFolds();     // 譜面が外で書き換えられていても、小節の数を超えない・重ならないようにする
}

namespace {
    // 次郎は #MEASURE の値を命令名の後の 10 文字 (空白を含む) までしか読まないので、空白の後 9 文字に収まる分数にする。
    // 収まらない時は連分数で近い分数を選ぶ (呼ぶ側は、書いた値に合わせて BPM を決め直して時間を保つ)
    std::pair<int64_t, int64_t> JiroFraction(int64_t n, int64_t d) {
        if (d < 0) { n = -n; d = -d; }
        tja::Reduce(n, d);
        auto fits = [](int64_t a, int64_t b) { return (std::to_string(a) + "/" + std::to_string(b)).size() <= 9; };
        if (fits(n, d)) { return { n, d }; }
        const int64_t sign = n < 0 ? -1 : 1;
        int64_t x = n < 0 ? -n : n, y = d, p0 = 0, q0 = 1, p1 = 1, q1 = 0;
        std::pair<int64_t, int64_t> best{ sign * std::max<int64_t>(1, (int64_t)std::llround((double)x / (double)y)), 1 };
        while (y != 0) {
            const int64_t q = x / y, p2 = q * p1 + p0, q2 = q * q1 + q0;
            if (!fits(sign * p2, q2)) { break; }
            if (p2 != 0) { best = { sign * p2, q2 }; }
            p0 = p1; q0 = q1; p1 = p2; q1 = q2;
            const int64_t t = x % y; x = y; y = t;
        }
        return best;
    }
    // 次郎は #BPMCHANGE の値を命令名の後の 8 文字 (空白を含む) までしか読まないので、7 文字に収める
    std::string JiroBpm(double v) {
        for (int precision = 6; precision >= 0; --precision) {
            std::string text = std::format("{:.{}f}", v, precision);
            if (text.find('.') != std::string::npos) { while (text.back() == '0') { text.pop_back(); } if (text.back() == '.') { text.pop_back(); } }
            if (text == "-0") { text = "0"; }
            if (text.size() <= 7) { return text; }
        }
        return std::format("{:.0f}", v);
    }
}

void Game::EditorInsertFrameStep(const std::string& params) { EditorRebuildRange(0, params); }
void Game::EditorInsertConvulsion(const std::string& params) { EditorRebuildRange(1, params); }

// 選んだ範囲を別の小節に分けて作り直す。範囲の前と後ろの部分は、拍子を短くした小節として残す。叩く時刻は変わらない
//   kind 0 (コマ送り): 音符ごとに「止まる (疑似停止) → 次の音符までの距離 × 倍率だけ瞬間移動 (バグ式)」
//   kind 1 (痙攣): 音符の間隔に合わせた区切りごとに「速く前へ進む → 振れ幅だけ後ろへ戻る」(進む距離と時間の合計は元と同じなので、音符は元の速さで来ながら震える)
void Game::EditorRebuildRange(int kind, const std::string& params) {
    auto* course = EditorCourse();
    if (course == nullptr) { return; }
    const char* what = kind == 0 ? "コマ送り" : "痙攣";
    if (!Editor.Selecting) { EditorMessage(std::string(what) + "にする範囲を選んでください (Shift+クリック / Shift+矢印)"); return; }
    std::vector<std::string> args;
    for (auto part : split(params, ' ')) { if (!tja::Trim(part).empty()) { args.emplace_back(tja::Trim(part)); } }
    auto arg = [&](size_t i, const char* def) { return EditorParseFrac(i < args.size() ? std::string_view(args[i]) : std::string_view(def)); };
    int64_t kn = 1, kd = 1, an = 1, ad = 2;
    if (kind == 0) {
        const auto scale = arg(0, "1");
        if (!scale || !EditorToRational(*scale, kn, kd)) { EditorMessage("移動距離の倍率は正の値にしてください (例: 1)"); return; }
    }
    else {
        // 振れ幅だけを指定する (以前の「間隔 振れ幅」の形で書かれていたら最後の値を振れ幅として使う)
        const auto amplitude = arg(args.empty() ? 0 : args.size() - 1, "1/2");
        if (!amplitude || !EditorToRational(*amplitude, an, ad) || an >= ad) {
            EditorMessage("振れ幅は 0 より大きく 1 より小さくしてください (例: 1/2)"); return;
        }
    }
    const auto [a, b] = EditorSelection();
    const int count = (int)course->Measures.size();
    const int first = std::clamp(a.Measure, 0, count - 1), last = std::clamp(b.Measure, first, count - 1);
    for (int m = first; m <= last; ++m) {
        for (const auto& c : course->Measures[(size_t)m].Commands) {
            const bool timing = tja::StartsWithKey(c.Text, "#BPMCHANGE") || tja::StartsWithKey(c.Text, "#MEASURE") || tja::StartsWithKey(c.Text, "#DELAY");
            if (timing && !(m == first && c.Num == 0)) { EditorMessage("範囲の小節に #BPMCHANGE・#MEASURE・#DELAY があると使えません (範囲の始まりの小節の頭は可)"); return; }
            const std::string key = EditorCommandKey(c.Text);
            if (key == "#BRANCHSTART" || key == "#BRANCHEND" || key == "#N" || key == "#E" || key == "#M") { EditorMessage("範囲に譜面分岐があると使えません"); return; }
        }
    }
    const EditorTiming timing = EditorTimingAt(first);
    int64_t rn = 0, rd = 0;
    if (!(timing.Bpm > 0) || !EditorToRational(timing.Measure, rn, rd)) { EditorMessage("負の BPM・拍子の所では使えません"); return; }
    const double B = timing.Bpm;
    // 位置は「小節 first の頭からの長さ (4/4 の小節を 1 とする分数)」で表す
    struct Q { int64_t N = 0, D = 1; };
    auto norm = [](Q q) { tja::Reduce(q.N, q.D); return q; };
    auto absolute = [&](int m, int64_t num, int64_t den) { return norm({ ((int64_t)(m - first) * den + num) * rn, den * rd }); };
    auto less = [](const Q& x, const Q& y) { return x.N * y.D < y.N * x.D; };
    auto sub = [&](const Q& x, const Q& y) { return norm({ x.N * y.D - y.N * x.D, x.D * y.D }); };
    const Q total = absolute(last + 1, 0, 1);
    const Q start = absolute(a.Measure < first ? first : a.Measure, a.Num, std::max<int64_t>(a.Den, 1));
    // 範囲の終わり = 選んだ最後の位置の 1 グリッド後 (小節の終わりを超えない)
    Q end = absolute(b.Measure, std::min<int64_t>(b.Num + 1, b.Den), std::max<int64_t>(b.Den, 1));
    if (less(total, end)) { end = total; }
    if (!less(start, end)) { EditorMessage("範囲が短すぎます"); return; }
    // 範囲の音符・命令を集める (範囲の前・後ろのものは、分けた前後の小節に残す)
    struct Item { Q At; char Type = 0; std::string Text; };
    std::vector<Item> before, inside, after;
    for (int m = first; m <= last; ++m) {
        const auto& measure = course->Measures[(size_t)m];
        auto place = [&](Item it) {
            if (less(it.At, start)) { before.push_back(std::move(it)); }
            else if (less(it.At, end)) { inside.push_back(std::move(it)); }
            else { after.push_back(std::move(it)); }
        };
        for (const auto& n : measure.Notes) { place({ absolute(m, n.Num, n.Den), n.Type, {} }); }
        for (const auto& c : measure.Commands) {
            if (tja::StartsWithKey(c.Text, "#BPMCHANGE") || tja::StartsWithKey(c.Text, "#MEASURE") || tja::StartsWithKey(c.Text, "#DELAY")) { continue; }
            place({ absolute(m, c.Num, c.Den), 0, c.Text });
        }
    }
    // 範囲の前の小節線の表示 (範囲の後で元に戻す)
    bool barline = true;
    for (int m = 0; m < first; ++m) {
        for (const auto& c : course->Measures[(size_t)m].Commands) {
            if (tja::StartsWithKey(c.Text, "#BARLINEOFF")) { barline = false; }
            else if (tja::StartsWithKey(c.Text, "#BARLINEON")) { barline = true; }
        }
    }
    const std::string bpmText = JiroBpm(B), restoreMeasure = "#MEASURE " + EditorFracText(timing.Measure.Num, timing.Measure.Den);
    // 次郎が読める長さの分数 (書いた値を Written に返す)
    auto jiroQ = [&](const Q& q) { const auto [n, d] = JiroFraction(q.N, q.D); return Q{ n, d }; };
    auto qText = [](const Q& q) { return std::to_string(q.N) + "/" + std::to_string(q.D); };
    auto qValue = [](const Q& q) { return (double)q.N / (double)q.D; };
    auto add = [&](const Q& x, const Q& y) { return norm({ x.N * y.D + y.N * x.D, x.D * y.D }); };
    // 区間 [from, to) の音符・命令を、長さ to - from の小節に入れる
    auto fill = [&](tja::Measure& out, const std::vector<Item>& items, const Q& from, const Q& to) {
        const Q length = sub(to, from);
        for (const auto& it : items) {
            if (less(it.At, from) || !less(it.At, to)) { continue; }
            const Q rel = sub(it.At, from);
            int64_t pn = rel.N * length.D, pd = rel.D * length.N;
            tja::Reduce(pn, pd);
            if (it.Text.empty()) { tja::Document::SetNote(out, pn, pd, it.Type); } else { tja::Document::AddCommand(out, pn, pd, it.Text); }
        }
    };
    std::vector<tja::Measure> built;
    // 範囲の前の部分: 元の BPM・拍子のまま、長さだけ短い小節
    if (less(Q{ 0, 1 }, start)) {
        tja::Measure head;
        head.Commands.push_back({ 0, 1, "#BPMCHANGE " + bpmText });
        head.Commands.push_back({ 0, 1, "#MEASURE " + qText(jiroQ(start)) });
        fill(head, before, Q{ 0, 1 }, start);
        built.push_back(std::move(head));
    }
    std::string summary;
    if (kind == 1) {
        // 痙攣: 音符の時刻で範囲を切り、その間を「一番細かい音符の間隔 (4 分より粗ければ 4 分)」に近い長さで等分した区切り。
        // 音符は必ず区切りの頭 (前へ進む小節の頭) に来る。区切りの長さ len、戻る距離 a = len × 振れ幅、
        // 前へ進む小節は 距離 len + a を 時間 len - a で進み (BPM = B × (len + a) / (len - a))、戻る小節は BPM -B・長さ -a (時間 a、距離 -a)
        std::vector<Q> cuts{ start };
        for (const auto& it : inside) { if (it.Text.empty() && less(start, it.At)) { cuts.push_back(it.At); } }
        std::ranges::sort(cuts.begin() + 1, cuts.end(), less);
        cuts.erase(std::unique(cuts.begin(), cuts.end(), [](const Q& x, const Q& y) { return x.N == y.N && x.D == y.D; }), cuts.end());
        cuts.push_back(end);
        // 区切りの基準の長さ: 音符と音符の間隔 (範囲の始まり・終わりとの間は含めない) の一番短いもの。音符が 2 つ未満なら 16 分
        Q unit{ 1, 4 };
        bool found = false;
        for (size_t i = 1; i + 2 < cuts.size(); ++i) {
            const Q gap = sub(cuts[i + 1], cuts[i]);
            if (!found || less(gap, unit)) { unit = gap; found = true; }
        }
        if (!found) { unit = { 1, 16 }; }
        if (less(Q{ 1, 4 }, unit)) { unit = { 1, 4 }; }
        std::vector<std::pair<Q, Q>> slices;
        for (size_t i = 0; i + 1 < cuts.size(); ++i) {
            const Q gap = sub(cuts[i + 1], cuts[i]);
            if (gap.N <= 0) { continue; }
            const int64_t pieces = std::max<int64_t>(1, std::llround(qValue(gap) / qValue(unit)));
            if (pieces > 2048) { EditorMessage("区切りが多すぎます (2048 まで)。範囲を短くしてください"); return; }
            const Q piece = norm({ gap.N, gap.D * pieces });
            Q from = cuts[i];
            for (int64_t k = 0; k < pieces; ++k) {
                const Q to = k + 1 == pieces ? cuts[i + 1] : add(from, piece);
                slices.push_back({ from, to });
                from = to;
            }
        }
        if (slices.size() > 2048) { EditorMessage("区切りが多すぎます (2048 まで)。間隔を大きくするか、範囲を短くしてください"); return; }
        // 戻る間に音符があると、その音符は正しく流せないので使えない
        for (const auto& [from, to] : slices) {
            const Q len = sub(to, from), back = norm({ len.N * an, len.D * ad });
            const Q turn = sub(to, back);
            for (const auto& it : inside) {
                if (it.Text.empty() && !less(it.At, turn) && less(it.At, to)) {
                    EditorMessage("音符が戻る所にかかります。振れ幅を小さくしてください"); return;
                }
            }
        }
        bool firstMeasure = true;
        for (const auto& [from, to] : slices) {
            const Q len = sub(to, from), back = norm({ len.N * an, len.D * ad });
            const Q turn = sub(to, back);                        // 前へ進む小節の終わり (時間で len - a)
            const Q forward = jiroQ(add(len, back));             // 前へ進む距離 (書いた値)
            const Q backward = jiroQ(norm({ -back.N, back.D }));  // 戻る距離 (負)
            tja::Measure go;
            // 時間を len - a に保つ BPM = B × 書いた距離 / (len - a)
            go.Commands.push_back({ 0, 1, "#BPMCHANGE " + JiroBpm(B * qValue(forward) / qValue(sub(turn, from))) });
            go.Commands.push_back({ 0, 1, "#MEASURE " + qText(forward) });
            if (firstMeasure) { go.Commands.push_back({ 0, 1, "#BARLINEOFF" }); firstMeasure = false; }
            fill(go, inside, from, turn);
            built.push_back(std::move(go));
            tja::Measure ret;
            // 時間を a に保つ BPM = -B × 書いた距離 / a
            ret.Commands.push_back({ 0, 1, "#BPMCHANGE " + JiroBpm(B * qValue(backward) / qValue(back)) });
            ret.Commands.push_back({ 0, 1, "#MEASURE " + qText(backward) });
            fill(ret, inside, turn, to);     // 音符以外の命令 (#SCROLL など) だけが入る
            built.push_back(std::move(ret));
        }
        summary = std::to_string(slices.size()) + " 区切り (1 区切り " + qText(unit) + " 小節前後) の痙攣にしました";
    }
    else {
    // 音符ごとの区切り: 範囲の始まり・音符の時刻 (同じ時刻はまとめる)・範囲の終わり
    std::vector<Q> cuts{ start };
    for (const auto& it : inside) {
        if (it.Text.empty() && it.Type != '8' && less(cuts.back(), it.At)) { cuts.push_back(it.At); }
    }
    std::ranges::sort(cuts.begin() + 1, cuts.end(), less);
    cuts.erase(std::unique(cuts.begin(), cuts.end(), [](const Q& x, const Q& y) { return x.N == y.N && x.D == y.D; }), cuts.end());
    cuts.push_back(end);
    if (cuts.size() - 1 > 1024) { EditorMessage("音符が多すぎます (1024 個まで)。範囲を短くしてください"); return; }
    for (size_t i = 0; i + 1 < cuts.size(); ++i) {
        const Q length = sub(cuts[i + 1], cuts[i]);
        if (length.N <= 0) { continue; }
        // 音符は元の BPM のごく短い小節 (1/1000 小節。区間の半分まで) に置く: 1/1000 の BPM の所に置くと、
        // 次郎では音符もその BPM で流れて、判定を過ぎてもレーンに長く残ってしまう
        const bool noteHere = std::ranges::any_of(inside, [&](const Item& it) { return it.Text.empty() && it.Type != '8' && it.At.N == cuts[i].N && it.At.D == cuts[i].D; });
        Q stopFrom = cuts[i];
        if (noteHere) {
            Q eps{ 1, 1000 };
            if (!less(norm({ eps.N * 2, eps.D }), length)) { eps = norm({ length.N, length.D * 2 }); }
            eps = jiroQ(eps);
            stopFrom = add(cuts[i], eps);
            tja::Measure hold;
            hold.Commands.push_back({ 0, 1, "#BPMCHANGE " + bpmText });
            hold.Commands.push_back({ 0, 1, "#MEASURE " + qText(eps) });
            if (i == 0) { hold.Commands.push_back({ 0, 1, "#BARLINEOFF" }); }
            fill(hold, inside, cuts[i], stopFrom);
            built.push_back(std::move(hold));
        }
        // 止まる: 拍子を 1/1000 にして (流れる距離はほぼ 0)、時間を保つ BPM にする。拍子は次郎が読める長さの分数にし、
        // その値に合わせて BPM = B × 書いた拍子 / 長さ にする (長い分数を書くと次郎では途中で切れて、譜面がとても遅くなる)
        const Q stopLength = sub(cuts[i + 1], stopFrom);
        const Q stopMeasure = jiroQ(norm({ stopLength.N, stopLength.D * 1000 }));
        tja::Measure stop;
        stop.Commands.push_back({ 0, 1, "#BPMCHANGE " + JiroBpm(B * qValue(stopMeasure) / qValue(stopLength)) });
        stop.Commands.push_back({ 0, 1, "#MEASURE " + qText(stopMeasure) });
        if (i == 0 && !noteHere) { stop.Commands.push_back({ 0, 1, "#BARLINEOFF" }); }
        fill(stop, inside, stopFrom, cuts[i + 1]);
        built.push_back(std::move(stop));
        // 瞬間移動 (バグ式): BPM X と -X の 0 秒の小節で、次の音符までの距離 × 倍率だけ進める
        Q distance = norm({ length.N * kn, length.D * kd });
        tja::Measure jump;
        jump.Resolution = 2;
        jump.Commands.push_back({ 0, 1, "#BPMCHANGE " + bpmText });
        jump.Commands.push_back({ 0, 1, "#MEASURE " + qText(jiroQ(distance)) });
        jump.Commands.push_back({ 1, 2, "#BPMCHANGE " + JiroBpm(-B) });
        built.push_back(std::move(jump));
    }
    summary = std::to_string(cuts.size() - 1) + " コマのコマ送りにしました";
    }
    // 範囲の後ろの部分: 元の BPM と小節線で、長さだけ短い小節
    if (less(end, total)) {
        tja::Measure tail;
        tail.Commands.push_back({ 0, 1, "#BPMCHANGE " + bpmText });
        tail.Commands.push_back({ 0, 1, "#MEASURE " + qText(jiroQ(sub(total, end))) });
        tail.Commands.push_back({ 0, 1, barline ? "#BARLINEON" : "#BARLINEOFF" });
        fill(tail, after, end, total);
        built.push_back(std::move(tail));
    }
    EditorChange();
    const int removed = last - first + 1;
    course->Measures.erase(course->Measures.begin() + first, course->Measures.begin() + last + 1);
    course->Measures.insert(course->Measures.begin() + first, built.begin(), built.end());
    // その後ろで元の BPM・拍子・小節線に戻す (同じ種類の命令がもうあれば足さない)
    const size_t next = (size_t)first + built.size();
    if (next >= course->Measures.size()) { course->Measures.emplace_back(); }
    auto& restore = course->Measures[next].Commands;
    std::vector<tja::Command> head;
    for (const std::string& t : { "#BPMCHANGE " + bpmText, restoreMeasure, std::string(barline ? "#BARLINEON" : "#BARLINEOFF") }) {
        const std::string key = EditorCommandKey(t);
        if (std::ranges::none_of(restore, [&](const tja::Command& c) { return c.Num == 0 && EditorCommandKey(c.Text) == key; })) { head.push_back({ 0, 1, t }); }
    }
    restore.insert(restore.begin(), head.begin(), head.end());
    EditorRemapFolds(first, removed, (int)built.size());
    const bool hasscroll = std::ranges::any_of(course->Header, [](const std::string& h) { return tja::StartsWithKey(h, "#HBSCROLL") || tja::StartsWithKey(h, "#BMSCROLL"); });
    if (!hasscroll) { course->Header.push_back("#HBSCROLL"); }
    Editor.Selecting = false;
    Editor.CursorMeasure = first;
    Editor.CursorSlot = 0;
    EditorClamp();
    EditorMessage("範囲を分けて、" + summary + (hasscroll ? "" : "  (#HBSCROLL を追加)")
        + (EditorJiroPreviewEnabled() ? "" : "  ※ 次郎形式再生ボタンで次郎の時刻で確かめられます"));
}

auto Game::EditorBuildTimeline() -> EditorTimeline {
    EditorTimeline tl;
    const auto* course = EditorCourse();
    if (course == nullptr) { return tl; }
    double bpm = 120;
    if (const auto v = tja::Document::GetValue(Editor.Doc.Header, "BPM"); !v.empty() && std::atof(v.c_str()) != 0) { bpm = std::atof(v.c_str()); }
    const bool jiro = EditorJiroPreviewEnabled();
    // 分岐: ゲームと同じく #N / #E / #M はどれも分岐の頭の時刻から始め、#BRANCHEND の後は
    // 最後に書かれた分岐の続き (次郎形式は最初の分岐の続き) から進める。効果音は分岐ごとに 1 つ (いちばん後に書かれた分岐) だけ鳴らす
    // #N / #E / #M は命令名が完全に一致する時だけ (#MEASURE などを分岐と取り違えない)
    auto isbranch = [](const std::string& t) {
        std::string_view v = tja::Trim(t);
        v = v.substr(0, v.find_first_of(" \t"));
        return v.size() == 2 && v[0] == '#' && (std::toupper((unsigned char)v[1]) == 'N' || std::toupper((unsigned char)v[1]) == 'E' || std::toupper((unsigned char)v[1]) == 'M');
    };
    const size_t count = course->Measures.size();
    std::vector<int> branchof(count, 0), section(count, -1);
    std::vector<int> lastbranch;    // 分岐の区間ごとの、いちばん後に書かれた分岐の番号
    {
        int cur = 0, sec = -1;
        for (size_t i = 0; i < count; ++i) {
            for (const auto& c : course->Measures[i].Commands) {
                if (c.Num != 0) { continue; }
                if (tja::StartsWithKey(c.Text, "#BRANCHSTART")) { ++sec; lastbranch.push_back(0); cur = 0; }
                else if (tja::StartsWithKey(c.Text, "#BRANCHEND")) { cur = 0; }
                else if (isbranch(c.Text) && sec >= 0) { ++cur; lastbranch[sec] = std::max(lastbranch[sec], cur); }
            }
            branchof[i] = cur;
            section[i] = cur > 0 ? sec : -1;
        }
    }
    // 次郎形式で分岐の無いコースは、次郎と同じ整数ミリ秒の切り捨てで時刻を求める (#DELAY による補正もそのまま見える)
    if (jiro && lastbranch.empty()) { tl.Measures = EditorJiroMeasureMaps(); }
    else {
    struct State { double T, Bpm, Ratio, Pending; bool HasPending; };
    double ratio = 1.0, pending = 0, t = 0;
    bool haspending = false;
    State branchstart{}, firstend{};
    int branchseen = 0;
    for (size_t mi = 0; mi < count; ++mi) {
        const auto& m = course->Measures[mi];
        // 小節の頭の分岐の命令 (時刻を戻す・続きを決める)
        for (const auto& c : m.Commands) {
            if (c.Num != 0) { continue; }
            if (tja::StartsWithKey(c.Text, "#BRANCHSTART")) { branchstart = { t, bpm, ratio, pending, haspending }; branchseen = 0; }
            else if (isbranch(c.Text)) {
                if (branchseen == 1) { firstend = { t, bpm, ratio, pending, haspending }; }
                if (branchseen >= 1) { t = branchstart.T; bpm = branchstart.Bpm; ratio = branchstart.Ratio; pending = branchstart.Pending; haspending = branchstart.HasPending; }
                ++branchseen;
            }
            else if (tja::StartsWithKey(c.Text, "#BRANCHEND")) {
                if (jiro && branchseen >= 2) { t = firstend.T; bpm = firstend.Bpm; ratio = firstend.Ratio; pending = firstend.Pending; haspending = firstend.HasPending; }
                branchseen = 0;
            }
        }
        if (haspending) { ratio = pending; haspending = false; }
        for (const auto& c : m.Commands) {
            if (c.Num == 0 && tja::StartsWithKey(c.Text, "#MEASURE")) {
                if (auto f = EditorParseFrac(std::string_view(c.Text).substr(8)); f && f->Den != 0) { ratio = f->Value(); }
            }
        }
        EditorTimeline::Measure out;
        double pos = 0;
        out.Map.push_back({ 0, t });
        auto advance = [&](double to) {
            if (to > pos) { t += (to - pos) * ratio * 240000.0 / bpm; pos = to; out.Map.push_back({ pos, t }); }
        };
        for (const auto& c : m.Commands) {
            const double cp = std::min(1.0, (double)c.Num / std::max<int64_t>(c.Den, 1));
            advance(cp);
            if (tja::StartsWithKey(c.Text, "#BPMCHANGE")) {
                const double v = EditorCommandValue(c.Text, 10);
                if (v != 0 && std::isfinite(v)) { bpm = v; }
            }
            else if (tja::StartsWithKey(c.Text, "#DELAY")) {
                t += EditorCommandValue(c.Text, 6) * 1000.0;
                out.Map.push_back({ pos, t });
            }
            else if (c.Num != 0 && tja::StartsWithKey(c.Text, "#MEASURE")) {
                // 小節の途中の #MEASURE: 次郎形式は次の小節から、そうでなければその位置から (ゲームと同じ)
                if (auto f = EditorParseFrac(std::string_view(c.Text).substr(8)); f && f->Den != 0) {
                    if (jiro) { pending = f->Value(); haspending = true; }
                    else { ratio = f->Value(); }
                }
            }
        }
        advance(1.0);
        tl.Measures.push_back(std::move(out));
    }
    }
    std::vector<int> balloonCounts;
    std::istringstream balloonInput(tja::Document::GetValue(course->Header,"BALLOON"));
    std::string countText;
    while(std::getline(balloonInput,countText,',')) { int hits=5;const auto value=tja::Trim(countText);const auto parsed=std::from_chars(value.data(),value.data()+value.size(),hits);if(parsed.ec!=std::errc{})hits=5;balloonCounts.push_back(std::max(0,hits)); }
    size_t balloonIndex=0,roll=std::numeric_limits<size_t>::max();
    for (int i=0;i<(int)count;++i) {
        const bool visible=!(branchof[i]>0 && section[i]>=0 && section[i]<(int)lastbranch.size() && branchof[i]!=lastbranch[section[i]]);
        for(const auto& n:course->Measures[i].Notes) {
            int hits=0;
            if(n.Type=='7'||n.Type=='9') {
                hits=balloonIndex<balloonCounts.size()?balloonCounts[balloonIndex]:5;++balloonIndex;
                tl.BalloonCounts[{i,n.Num,n.Den}]=hits;
            }
            if(!visible)continue;
            const double time=tl.TimeAt(i,(double)n.Num/std::max<int64_t>(n.Den,1));
            if(n.Type=='8' && roll<tl.Notes.size()) {tl.Notes[roll].End=time;roll=std::numeric_limits<size_t>::max();}
            tl.Notes.push_back({time,n.Type,time,hits});
            if(n.Type=='5'||n.Type=='6'||n.Type=='7'||n.Type=='9')roll=tl.Notes.size()-1;
        }
    }    std::ranges::stable_sort(tl.Notes, {}, &EditorTimeline::Note::Time);
    return tl;
}

auto Game::EditorTimelineCached() -> const EditorTimeline& {
    static constexpr uint64_t invalid = std::numeric_limits<uint64_t>::max();
    if (EditorTimelineRevision == invalid || EditorTimelineRevision != Editor.Revision ||
        EditorTimelineCourse != Editor.Course || EditorTimelineJiro != EditorJiroPreviewEnabled()) {
        EditorTimelineCache = EditorBuildTimeline();
        EditorTimelineRevision = Editor.Revision;
        EditorTimelineCourse = Editor.Course;
        EditorTimelineJiro = EditorJiroPreviewEnabled();
    }
    return EditorTimelineCache;
}

double Game::EditorPreviewNow() const {
    return Editor.PreviewFrom + std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Editor.PreviewClock).count() * Editor.PreviewSpeed;
}

void Game::EditorSetPreviewSpeed(int dir) {
    const double* begin = std::begin(EditorSpeeds), * end = std::end(EditorSpeeds);
    const double* it = std::find_if(begin, end, [&](double v) { return v >= Editor.PreviewSpeed - 1e-9; });
    int index = (int)(it - begin);
    if (it == end || std::abs(*it - Editor.PreviewSpeed) > 1e-9) { index = 3; }
    index = std::clamp(index + dir, 0, (int)std::size(EditorSpeeds) - 1);
    EditorApplyPreviewSpeed(EditorSpeeds[index]);
}

void Game::EditorApplyPreviewSpeed(double speed) {
    if(!std::isfinite(speed)||speed<.25||speed>2) { EditorMessage("速度は0.25～2.0で入力してください"); return; }
    if (Editor.Preview) {
        const double now = EditorPreviewNow();
        for(auto& [index,next]:Editor.PreviewRollNext) if(next>now) next=now+(next-now)*speed/Editor.PreviewSpeed;
        Editor.PreviewFrom = now;
        Editor.PreviewClock = std::chrono::steady_clock::now();
    }
    EditorStopSong();
    Editor.PreviewSpeed = speed;
    EditorRequestStretch();
    EditorMessage("再生速度: " + FormatNumber(Editor.PreviewSpeed, 2) + " 倍 (音の高さは変わりません)");
}

int Game::EditorActiveSong() const {
    if (std::abs(Editor.PreviewSpeed - 1.0) < 1e-9) { return Editor.SongHandle; }
    return std::abs(Editor.StretchSpeed - Editor.PreviewSpeed) < 1e-9 ? Editor.StretchHandle : -1;
}

void Game::EditorStopSong() {
    for (int h : { Editor.SongHandle, Editor.StretchHandle }) { if (h != -1) { snd::Stop(h); } }
    Editor.SongStarted = false;
}

void Game::EditorLoadSong() {
    const std::string wave = tja::Document::GetValue(Editor.Doc.Header, "WAVE");
    const fs::path path = wave.empty() ? fs::path() : Editor.Path.parent_path() / U8ToPath(wave);
    if (path == Editor.SongPath) { return; }
    EditorFreeSong();
    Editor.SongPath = path;
    std::error_code ec;
    if (!path.empty() && fs::is_regular_file(path, ec)) {
        // 前後の無音を除いた波形を 1 回だけ作り、再生と速度の変更 (WSOLA) の両方で使う
        if (auto pcm = LoadAlignedPcm(PathToU8(path))) {
            Editor.SongHandle = snd::Create(pcm);
            Editor.SongPcm = pcm->Samples;
            Editor.SongPcmChannels = pcm->Channels;
            Editor.SongPcmRate = pcm->Rate;
            Editor.SongPcmTried = true;
        }
    }
}

void Game::EditorRequestStretch() {
    if (std::abs(Editor.PreviewSpeed - 1.0) < 1e-9 || std::abs(Editor.StretchSpeed - Editor.PreviewSpeed) < 1e-9) { return; }
    if (Editor.StretchTask.valid() && std::abs(Editor.StretchTaskSpeed - Editor.PreviewSpeed) < 1e-9) { return; }
    EditorLoadSong();
    if (Editor.SongPath.empty()) { return; }
    if (!Editor.SongPcmTried) {
        // デコードは 1 回だけ行い、速度を変えるたびに使い回す
        Editor.SongPcmTried = true;
        if (auto pcm = LoadAlignedPcm(PathToU8(Editor.SongPath))) {
            Editor.SongPcm = pcm->Samples;
            Editor.SongPcmChannels = pcm->Channels;
            Editor.SongPcmRate = pcm->Rate;
        }
    }
    if (Editor.SongPcm.empty()) { return; }
    if (Editor.StretchCancel) { *Editor.StretchCancel = true; }
    auto cancel = std::make_shared<std::atomic_bool>(false);
    Editor.StretchCancel = cancel;
    Editor.StretchTaskSpeed = Editor.PreviewSpeed;
    Editor.StretchTask = std::async(std::launch::async, [pcm = &Editor.SongPcm, ch = Editor.SongPcmChannels, rate = Editor.SongPcmRate, speed = Editor.PreviewSpeed, cancel] {
        return tstretch::Wsola(*pcm, ch, rate, speed, cancel.get());
    });
}

void Game::EditorPollStretch() {
    if (!Editor.StretchTask.valid() || Editor.StretchTask.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { return; }
    std::vector<float> out;
    try { out = Editor.StretchTask.get(); }
    catch (const std::exception& e) { EditorMessage(std::string("速度変更に失敗しました: ") + e.what()); return; }
    if (out.empty() || Editor.SongPcmChannels <= 0) { return; }
    if (Editor.StretchHandle != -1) { snd::Delete(Editor.StretchHandle); Editor.StretchHandle = -1; }
    const int ch = std::min(Editor.SongPcmChannels, 2);
    const int frames = (int)(out.size() / Editor.SongPcmChannels);
    auto stretched = std::make_shared<snd::Pcm>();
    stretched->Channels = ch;
    stretched->Rate = Editor.SongPcmRate;
    stretched->Samples.resize((size_t)frames * ch);
    for (int f = 0; f < frames; ++f) {
        for (int c = 0; c < ch; ++c) { stretched->Samples[(size_t)f * ch + c] = std::clamp(out[(size_t)f * Editor.SongPcmChannels + c], -1.0f, 1.0f); }
    }
    Editor.StretchHandle = snd::Create(stretched);
    Editor.StretchSpeed = Editor.StretchHandle != -1 ? Editor.StretchTaskSpeed : 0;
}

double Game::EditorSongPosition(double charttime) const {
    // ゲームと同じ: 譜面の時刻 c の音は音源の c - OFFSET の位置 (設定の ChartOffset で譜面が遅れ、SongOffset で音源が遅れる)
    const double offset = std::atof(tja::Document::GetValue(Editor.Doc.Header, "OFFSET").c_str()) * 1000.0;
    return (charttime - offset + Config.ChartOffset - Config.SongOffset) / Editor.PreviewSpeed;
}

void Game::EditorStopPreview() {
    if (!Editor.Preview) { return; }
    Editor.Preview = false;
    EditorStopSong();
}

void Game::EditorReleaseSong() {
    EditorStopPreview();
    EditorFreeSong();
}

void Game::EditorFreeSong() {
    EditorStopSong();
    if (Editor.StretchCancel) { *Editor.StretchCancel = true; }
    if (Editor.StretchTask.valid()) { Editor.StretchTask.wait(); Editor.StretchTask = {}; }
    if (Editor.SongHandle != -1) { snd::Delete(Editor.SongHandle); Editor.SongHandle = -1; }
    if (Editor.StretchHandle != -1) { snd::Delete(Editor.StretchHandle); Editor.StretchHandle = -1; }
    Editor.StretchSpeed = 0;
    Editor.SongPcm.clear();
    Editor.SongPcm.shrink_to_fit();
    Editor.SongPcmTried = false;
    Editor.SongPath.clear();
}

bool Game::EditorJiroPreviewEnabled() const {
    return Editor.PreviewJiro && tja::Document::GetValue(Editor.Doc.Header, "FASOSUFORMAT").empty();
}

void Game::EditorToggleJiroPreview() {
    if (!Editor.Open) { return; }
    if (!tja::Document::GetValue(Editor.Doc.Header, "FASOSUFORMAT").empty()) {
        EditorMessage("osu! 譜面は絶対時刻を維持するため、次郎形式のプレビューには切り替えません");
        return;
    }
    // 再生中なら現在の譜面位置を、新しいタイムラインの時刻へ引き継ぐ。
    const bool wasPlaying = Editor.Preview;
    int measure = Editor.CursorMeasure;
    double pos = (double)Editor.CursorSlot / EditorDivOf(measure);
    if (wasPlaying) {
        int m = measure; double p = pos;
        if (EditorTimelineCached().PosAt(EditorPreviewNow(), m, p)) { measure = m; pos = p; }
        EditorStopPreview();
    }
    Editor.PreviewJiro = !Editor.PreviewJiro;
    if (wasPlaying) {
        EditorTogglePreview();
        if (Editor.Preview) {
            Editor.PreviewFrom = EditorTimelineCached().TimeAt(measure, pos);
            Editor.PreviewLast = Editor.PreviewFrom;
            Editor.PreviewClock = std::chrono::steady_clock::now();
        }
    }
    EditorMessage(Editor.PreviewJiro ? "次郎形式再生 ON (譜面マップ)" : "次郎形式再生 OFF (譜面マップ)");
}

void Game::EditorTogglePreview() {
    if (Editor.Preview) { EditorStopPreview(); return; }
    if (EditorCourse() == nullptr) { return; }
    const auto& tl = EditorTimelineCached();
    Editor.PreviewFrom = tl.TimeAt(Editor.CursorMeasure, (double)Editor.CursorSlot / EditorDivOf(Editor.CursorMeasure));
    Editor.PreviewLast = Editor.PreviewFrom - 1.0;
    Editor.PreviewRollHits.clear();Editor.PreviewRollNext.clear();Editor.PreviewRollRevision=Editor.Revision;
    Editor.Preview = true;
    Editor.SongStarted = false;
    // 音源 (WAVE) を読み込む。流し始めは EditorUpdatePreview が音源の位置に合わせて行う (OFFSET: 譜面の 0 は音源の -OFFSET 秒)
    EditorLoadSong();
    EditorRequestStretch();
    if (Editor.SongHandle == -1) {
        EditorMessage(Editor.SongPath.empty() ? "WAVE が無いので効果音だけで再生します" : "音源を読み込めないので効果音だけで再生します");
    }
    Editor.PreviewClock = std::chrono::steady_clock::now();
}

void Game::EditorUpdatePreview() {
    EditorPollStretch();
    if (!Editor.Preview) { return; }
    const auto& tl = EditorTimelineCached();
    const double now = EditorPreviewNow();
    // 音源: 音源の範囲に入ったら (速度を変えた音源はできあがった時に) 今の位置から流す
    if (const int song = EditorActiveSong(); song != -1 && !Editor.SongStarted) {
        const double pos = EditorSongPosition(now);
        if (pos >= 0) {
            snd::SetVolume(song, (int)(255 * Config.SongVolume / 100));
            snd::SetPositionMs(song, (double)((LONGLONG)pos));
            snd::Play(song, false, 0 == 1);
            Editor.SongStarted = true;
        }
    }
    // 効果音とカーソルは時計で進めるので、流している音源の位置と 20ms 以上離れたら音源に合わせ直す
    // (速度を変えた音源の長さの誤差や、処理が詰まって音源だけが進んだ時に、ずれが残らないように)
    if (const int song = EditorActiveSong(); song != -1 && Editor.SongStarted && snd::IsPlaying(song)) {
        const double offset = std::atof(tja::Document::GetValue(Editor.Doc.Header, "OFFSET").c_str()) * 1000.0;
        const double audioNow = snd::PositionMs(song) * Editor.PreviewSpeed + offset - Config.ChartOffset + Config.SongOffset;
        const double drift = audioNow - now;
        if (std::isfinite(drift) && std::abs(drift) > 20.0 && std::abs(drift) < 2000.0) {
            Editor.PreviewClock -= std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double, std::milli>(drift / Editor.PreviewSpeed));
        }
    }
    if(Editor.PreviewRollRevision!=Editor.Revision) {Editor.PreviewRollHits.clear();Editor.PreviewRollNext.clear();Editor.PreviewRollRevision=Editor.Revision;}
    const double volume=std::clamp(Config.SEVolume,0.0f,100.0f);
    auto& sounds=Skin.Base->Playing.SE;
    const auto play=[&](const SoundData& normal,bool emphasized=false){if(Editor.PreviewTaiko){
        const auto& big=&normal==&sounds.Don?sounds.DonBig:sounds.KaBig;
        const bool dedicated=emphasized&&big.Handle!=-1;
        const auto& sound=dedicated?big:normal;
        sound.SetVolume(numeric::Cast<float>(volume));sound.Play();
        if(emphasized&&!dedicated) {
            const int layer=snd::Duplicate(sound.Handle);
            if(layer!=-1) {
                snd::SetVolume(layer, numeric::Cast<int>(volume*2.55));
                snd::SetDeleteOnFinish(layer, true);
                snd::Play(layer, true);
            }
        }
    }};
    const auto firstNote=std::upper_bound(tl.Notes.begin(),tl.Notes.end(),Editor.PreviewLast,[](double t,const auto& n){return t<n.Time;});
    for(auto it=firstNote;it!=tl.Notes.end()&&it->Time<=now;++it) {
        if(it->Type=='1'||it->Type=='3')play(sounds.Don,it->Type=='3');
        if(it->Type=='2'||it->Type=='4')play(sounds.Ka,it->Type=='4');
    }
    const double rate=std::isfinite(Config.RollSpeed)?std::clamp(Config.RollSpeed,1.0,1000.0):30.0;
    const double interval=1000.0*Editor.PreviewSpeed/rate;
    for(size_t i=0;i<tl.Notes.size();++i) {
        const auto& note=tl.Notes[i];
        if(note.Type!='5'&&note.Type!='6'&&note.Type!='7'&&note.Type!='9')continue;
        if(note.End<=Editor.PreviewFrom||note.Time>now||note.End<=note.Time)continue;
        auto [nextIt,inserted]=Editor.PreviewRollNext.try_emplace(i,std::max(note.Time,Editor.PreviewLast));
        auto& next=nextIt->second;auto& hits=Editor.PreviewRollHits[i];
        const bool balloon=note.Type=='7'||note.Type=='9';
        previewroll::Advance(next,hits,note.Hits,balloon,now,note.End,interval,[&]{play(sounds.Don);},[&]{play(sounds.Balloon);});
    }
    Editor.PreviewLast = now;
    int m = 0;
    double pos = 0;
    if (tl.PosAt(now, m, pos)) {
        if (EditorMeasureRow(m) < 0) { Editor.Top = std::clamp(m, 0, std::max(EditorMeasureCount() - 1, 0)); EditorNormalizeFolds(); }
        const float width=EditorStandardMeasureWidth();
        const float x=(float)(width*std::clamp(EditorMeasureRatio(m),.05,16.0)*pos);
        if(x<EditorHorizontalOffset || x>=EditorHorizontalOffset+width) {
            EditorHorizontalOffset=editlayout::PageOffset(x,width,EditorMeasureRatio(m));
        }
    }
    else if (!tl.Measures.empty() && now > tl.Measures.back().Map.back().second + 1000.0) {
        EditorStopPreview();    // 最後まで流した
    }
}

auto Game::EditorPanelRect() const -> UIRect {
    return { EditorResX() * 0.06f, EditorResY() * 0.1f, EditorResX() * 0.94f, EditorResY() * 0.94f };
}

auto Game::EditorPanelRow(int i, int columns) const -> UIRect {
    const UIRect p = EditorPanelRect();
    // 行の高さは、今のパネルの項目がパネルに入るいちばん大きい高さ (編集画面の行の 3 倍まで)
    int total = 16;
    switch (Editor.Panel) {
    case EditorState::PanelType::Commands: total = (int)EditorCommandTemplates().size(); break;
    case EditorState::PanelType::Gimmick: total = (int)EditorGimmicks().size(); break;
    case EditorState::PanelType::Grid: total = (int)EditorGridPanelChoices().size() + 3; break;
    case EditorState::PanelType::Header: total = (int)EditorHeaderFields().size(); break;
    default: break;
    }
    const int rows = std::max(1, (total + columns - 1) / columns);
    // 分割数のパネルは下に説明が無いので、ボタンをパネルいっぱいまで大きくする
    const bool fill = Editor.Panel == EditorState::PanelType::Grid;
    const float avail = (p.B - p.T) - EditorLine() * 2.0f - EditorLine() * (fill ? 0.6f : 3.2f);
    const float lh = fill ? std::max(avail / rows, EditorLine() * 1.8f) : std::clamp(avail / rows, EditorLine() * 1.8f, EditorLine() * 3.4f);
    const float colw = (p.R - p.L - 32.0f) / columns;
    const int col = i % columns, row = i / columns;
    return { p.L + 16.0f + colw * col, p.T + EditorLine() * 2.0f + lh * row, p.L + 16.0f + colw * (col + 1) - 8.0f, p.T + EditorLine() * 2.0f + lh * row + lh - 6.0f };
}

auto Game::EditorCommandSections() -> const std::vector<EditorPanelSection>& {
    static const std::vector<EditorPanelSection> list = {
        { "テンポ・スクロール", { 0, 1, 3 } }, { "小節", { 2, 6, 7 } }, { "ゴーゴー", { 4, 5 } },
        { "分岐", { 8, 9, 10, 11, 12, 13, 14 } }, { "範囲選択", { 16, 17 } }, { "その他", { 15 } },
    };
    return list;
}

auto Game::EditorGimmickSections() -> const std::vector<EditorPanelSection>& {
    static const std::vector<EditorPanelSection> list = {
        { "速度・逆走", { 0, 1, 8, 16, 17 } }, { "停止", { 2, 3, 11, 12 } }, { "瞬間移動", { 4, 5, 6 } },
        { "出現・点滅", { 7, 9, 10, 18, 19 } }, { "背面・その他", { 13, 14, 15 } },
    };
    return list;
}

auto Game::EditorSectionLayout(const std::vector<EditorPanelSection>& sections, size_t count, int columns) const -> EditorPanelLayout {
    EditorPanelLayout out;
    out.Items.assign(count, UIRect{ -10000, -10000, -10000, -10000 });
    const UIRect p = EditorPanelRect();
    const float line = EditorLine(), headh = line * 1.3f;
    int rows = 0;
    for (const auto& sec : sections) { rows += std::max(1, ((int)sec.Items.size() + columns - 1) / columns); }
    const float avail = (p.B - p.T) - line * 2.0f - line * 3.2f - headh * sections.size();
    const float rowh = std::clamp(avail / std::max(rows, 1), line * 1.8f, line * 3.4f);
    const float colw = (p.R - p.L - 32.0f) / columns;
    float y = p.T + line * 1.9f;
    for (const auto& sec : sections) {
        out.Headers.push_back({ sec.Name, y + headh * 0.5f });
        y += headh;
        for (size_t k = 0; k < sec.Items.size(); ++k) {
            const int col = (int)k % columns, row = (int)k / columns;
            const float x = p.L + 16.0f + colw * col, top = y + rowh * row;
            if (sec.Items[k] >= 0 && sec.Items[k] < (int)count) { out.Items[sec.Items[k]] = { x, top, x + colw - 8.0f, top + rowh - 6.0f }; }
        }
        y += rowh * std::max(1, ((int)sec.Items.size() + columns - 1) / columns);
    }
    return out;
}

auto Game::EditorCommandLayout() const -> EditorPanelLayout { return EditorSectionLayout(EditorCommandSections(), EditorCommandTemplates().size(), 4); }

auto Game::EditorGimmickLayout() const -> EditorPanelLayout { return EditorSectionLayout(EditorGimmickSections(), EditorGimmicks().size(), 3); }

auto Game::EditorHeaderFields() -> const std::vector<EditorHeaderField>& {
    static const std::vector<EditorHeaderField> fields = {
        { "TITLE", false, "曲名" }, { "SUBTITLE", false, "サブタイトル" }, { "BPM", false, "最初の BPM" },
        { "WAVE", false, "音源ファイル (譜面からの相対パス)" }, { "SONGURL", false, "音源の URL" },
        { "BGMOVIE", false, "背景動画 (譜面からの相対パス)" }, { "MOVIEURL", false, "動画の URL" },
        { "MOVIEOFFSET", false, "動画の開始位置のずれ (秒。正で遅く)" },
        { "OFFSET", false, "音源の始まりと譜面のずれ (秒)" }, { "DEMOSTART", false, "選曲画面で流し始める位置 (秒)" },
        { "GENRE", false, "ジャンル" }, { "SONGVOL", false, "曲の音量" }, { "SEVOL", false, "効果音の音量" },
        { "COURSE", true, "コース (Easy / Normal / Hard / Oni / Edit)" }, { "LEVEL", true, "レベル" }, { "BALLOON", true, "風船の打数 (カンマ区切り)" },
        { "SCOREINIT", true, "初項" }, { "SCOREDIFF", true, "公差" },
    };
    return fields;
}

void Game::EditorInit() {
    if (Editor.Testing) {
        // テストプレイから戻った
        Editor.Testing = false;
        Config.TrainingMode = Editor.SavedTraining;
        Config.AutoPlay = Editor.SavedAutoPlay;
        Training.Init();
        Chart.Init(true);
        Editor.TestText.clear();
    }
    PointerPrev.Left = PointerPrev.Right = true;
#ifndef __ANDROID__
    sys::ShowCursor(true);
#endif
}

void Game::EditorEnd() {
    EditorStopPreview();
    EditorFinishFields(false);
#ifndef __ANDROID__
    if (Editor.Editing) {
        sys::EndTextInput(Editor.EditHandle);
        Editor.Editing = false;
    }
    if (!Editor.Testing) {
        sys::ShowCursor(!Config.FullScreen);
    }
#endif
    if (Editor.Rescan && !Editor.Testing) {
        Editor.Rescan = false;
        EnumChart(Config.SongDirectories);
    }
}

void Game::EditorDraw() {
    const FontData& font = ConfigFont();
    const FontData& sfont = EditorFont();
    const unsigned int white = gfx::Rgb(255, 255, 255), black = gfx::Rgb(0, 0, 0), gray = gfx::Rgb(150, 150, 150), accent = gfx::Rgb(255, 200, 40);
    const float resx = EditorResX(), resy = EditorResY(), lh = EditorLine();
    gfx::Box((float)(0), (float)(0), (float)((int)resx), (float)((int)resy), gfx::Rgb(22, 24, 34), true);

    if (!Editor.Open && Editor.Browsing) {
        EditorBrowseDraw(sfont);
    }
    else if (!Editor.Open) {
        UIText(font, resx * 0.5f, resy * 0.25f, "譜面制作", white, 1);
        const char* labels[] = { "新しい譜面を作る…", "譜面を開く…", "戻る" };
        for (int i = 0; i < 3; ++i) {
            const UIRect b = EditorStartButton(i);
            UIBox(b, gfx::Rgb(50, 54, 70), 230);
            UIBox(b, white, 160, false);
            UIText(EditorPanelFont(), (b.L + b.R) * 0.5f, (b.T + b.B) * 0.5f, labels[i], white, 1);
        }
        UIText(sfont, resx * 0.5f, resy * 0.85f, FitText("選曲画面で F7 を押すと、選んでいる譜面を編集できます", sfont.Handle, resx - 32, false), gray, 1);
    }
    else {
        // ボタン
        const auto buttons = EditorButtons();
        for (const auto& g : EditorGroups) {
            UIBox(g.Rect, gfx::Rgb(34, 37, 50), 255);
            UIBox(g.Rect, gfx::Rgb(80, 86, 110), 255, false);
            UIText(sfont, g.Rect.L + 10.0f, (g.Rect.T + g.Rect.B) * 0.5f, g.Label, gfx::Rgb(170, 175, 195));
        }
        const EditorButton* hovered = nullptr;
        for (const auto& b : buttons) {
            const bool over = b.Rect.Contains(PointerPrev.X, PointerPrev.Y) && Editor.Panel == EditorState::PanelType::None;
            if (over) { hovered = &b; }
            UIBox(b.Rect, b.Active ? gfx::Rgb(214, 116, 20) : over ? gfx::Rgb(72, 78, 100) : gfx::Rgb(50, 54, 70), 230);
            UIBox(b.Rect, white, over ? 200 : 120, false);
            if (b.Icon == EditorIcon::None) {
                UIText(sfont, (b.Rect.L + b.Rect.R) * 0.5f, (b.Rect.T + b.Rect.B) * 0.5f, b.Label, white, 1);
            }
            else { EditorDrawIcon(b, white); }
        }
        EditorDrawGrid(sfont);
        // ボタンにカーソルを合わせると名前と下部の概要を表示する。
        if (hovered != nullptr) {
            const float w = EditorTextWidth(hovered->Label) + 16.0f, th = sfont.Size + 10.0f;
            const float x = std::min(hovered->Rect.L, resx - w - 8.0f), y = hovered->Rect.B + 4.0f;
            UIBox({ x, y, x + w, y + th }, gfx::Rgb(44, 48, 66), 255);
            UIText(sfont, x + 8.0f, y + th * 0.5f, hovered->Label, white);
        }
    }

    // 下部: ファイル名・お知らせ・操作方法
    std::string bottom;
    std::string hoverhelp;
    if (Editor.Open && Editor.Panel == EditorState::PanelType::None) {
        for (const auto& b : EditorButtons()) {
            if (b.Rect.Contains(PointerPrev.X, PointerPrev.Y)) { hoverhelp = EditorButtonHelp(b.Label); break; }
        }
        if (hoverhelp.empty()) {
            for (int row = 0; row < EditorRows; ++row) {
                const int m = EditorRowMeasure(row);
                if (m < 0) { break; }
                const auto* registered = EditorFoldAt(m) ? EditorFoldAt(m) : EditorOpenFoldAt(m);
                if (registered && registered->first == m && EditorFoldDeleteButton(row).Contains(PointerPrev.X, PointerPrev.Y)) {
                    hoverhelp = "×: 折りたたみ登録を解除する。小節・音符・命令は残る (Ctrl+Z で戻せる)"; break;
                }
            }
        }
    }
    if (!hoverhelp.empty()) { bottom = hoverhelp; }
    else if (std::chrono::steady_clock::now() < Editor.MessageUntil) { bottom = Editor.Message; }
    else if (Editor.Open) {
#ifndef __ANDROID__
        bottom = PathToU8(Editor.Path.filename()) + (Editor.Dirty ? " *" : "") + "   左クリック: 置く  右クリック: 消す  Shift+クリック: 範囲選択  Ctrl+C/X/V: コピー/切り取り/貼り付け  0-9: 種類  Space: 再生/停止  Ctrl+S: 保存  Ctrl+Z: 元に戻す  F5: テストプレイ  Esc: 閉じる";
#else
        bottom = PathToU8(Editor.Path.filename()) + (Editor.Dirty ? " *" : "") + "   太鼓のカッ (左右): カーソルを前・後ろへ  右のドン: 置いて次へ  左のドン: 消す  上下にドラッグ: 小節のスクロール  上のボタン: 保存・再生・範囲選択・コピーなど";
#endif
    }
    {
        const auto lines = WrapText(bottom, sfont.Handle, resx - 32);
        const size_t n = std::min<size_t>(lines.size(), 2);
        for (size_t i = 0; i < n; ++i) {
            UIText(sfont, 16, resy - lh * (0.75f + (float)(n - 1 - i)), FitText(lines[i], sfont.Handle, resx - 32, false),
                std::chrono::steady_clock::now() < Editor.MessageUntil ? accent : gray);
        }
    }

    // パネル
    if (Editor.Open && Editor.Panel != EditorState::PanelType::None) {
        const UIRect p = EditorPanelRect();
        UIBox({ 0, 0, resx, resy }, black, 110);
        UIBox(p, gfx::Rgb(18, 20, 32), 240);
        UIBox(p, white, 200, false);
        if (Editor.Panel == EditorState::PanelType::Commands) {
            const std::string where = Editor.Selecting
                ? std::string("範囲を選択中: 範囲の始まりに入れ、#GOGOSTART・#BARLINEOFF・#SCROLL・#BPMCHANGE は範囲の終わりで元に戻す。グラデーションは「#SCROLL 1>3」(音符ごと)・「#SCROLL 1>3 小節」(小節線ごと)")
                : "命令を挿入 (カーソルの位置: " + std::to_string(Editor.CursorMeasure + 1) + " 小節目 " + std::to_string(Editor.CursorSlot) + "/" + std::to_string(EditorDivOf(Editor.CursorMeasure)) + ")  /  譜面の命令は クリック: 編集  右クリック: 削除";
            UIText(sfont, p.L + 16, p.T + lh, FitText(where, sfont.Handle, p.R - p.L - 32, false), white);
            const auto& list = EditorCommandTemplates();
            const auto layout = EditorCommandLayout();
            for (const auto& [name, hy] : layout.Headers) { UIText(sfont, p.L + 18, hy, name, accent); }
            for (int i = 0; i < (int)list.size(); ++i) {
                const UIRect r = layout.Items[i];
                UIBox(r, gfx::Rgb(50, 54, 70), 230);
                UIText(EditorPanelFont(), (r.L + r.R) * 0.5f, (r.T + r.B) * 0.5f, FitText(list[i].first, EditorPanelFont().Handle, r.R - r.L - 8, false), white, 1);
            }
        }
        else if (Editor.Panel == EditorState::PanelType::Estimate) {
            const auto& r = Editor.Estimate;
            const double bpm = r.Bpm;
            const std::string lines[] = {
                "音源から推定した値 (目安)",
                "BPM: " + FormatNumber(bpm, std::abs(bpm - std::round(bpm)) < 1e-6 ? 0 : 2) + "   (今: " + tja::Document::GetValue(Editor.Doc.Header, "BPM") + ")",
                "OFFSET: " + FormatNumber(-r.FirstDownbeat, 3) + " 秒   (最初の小節の頭 = 音源の " + FormatNumber(r.FirstDownbeat, 3) + " 秒。今: " + tja::Document::GetValue(Editor.Doc.Header, "OFFSET") + ")",
                "拍子: " + std::to_string(r.BeatsPerMeasure) + "/4" + (r.BeatsPerMeasure != 4 ? "   (各コースの最初の小節に #MEASURE を入れます)" : ""),
                "確からしさ: " + FormatNumber(r.Confidence * 100.0, 0) + "%   (低い時は BPM が半分・倍などになっていることがあります)",
                "音源長: " + FormatNumber(r.DurationSeconds, 2) + " 秒 / 概算 " + std::to_string(tempo::MeasureCount(r.DurationSeconds, r.Bpm, -r.FirstDownbeat, r.BeatsPerMeasure)) + " 小節 (小節数の適用は「音源長→小節数」)",
            };
            float y = p.T + lh;
            for (const auto& line : lines) {
                UIText(sfont, p.L + 16, y, FitText(line, sfont.Handle, p.R - p.L - 32, false), white);
                y += lh * 1.4f;
            }
            const char* labels[] = { "適用 (Enter)", "キャンセル (Esc)" };
            for (int i = 0; i < 2; ++i) {
                const UIRect b = EditorHeaderButton(i);
                UIBox(b, i == 0 ? gfx::Rgb(214, 116, 20) : gfx::Rgb(60, 64, 84), 230);
                UIText(sfont, (b.L + b.R) * 0.5f, (b.T + b.B) * 0.5f, labels[i], white, 1);
            }
        }
        else if (Editor.Panel == EditorState::PanelType::Gimmick) {
            const EditorTiming t = EditorTimingAt(Editor.CursorMeasure);
            UIText(sfont, p.L + 16, p.T + lh, FitText("ギミックを挿入 (" + std::to_string(Editor.CursorMeasure + 1) + " 小節目の前 / BPM " + EditorNum(t.Bpm)
                + " / 拍子 " + EditorFracText(t.Measure.Num, t.Measure.Den) + ")  クリックで数値を入れて挿入", sfont.Handle, p.R - p.L - 32, false), white);
            const auto& list = EditorGimmicks();
            int hover = -1;
            const auto layout = EditorGimmickLayout();
            for (const auto& [name, hy] : layout.Headers) { UIText(sfont, p.L + 18, hy, name, accent); }
            for (int i = 0; i < (int)list.size(); ++i) {
                const UIRect r = layout.Items[i];
                const bool over = r.Contains(PointerPrev.X, PointerPrev.Y);
                if (over) { hover = i; }
                UIBox(r, over ? gfx::Rgb(80, 86, 110) : gfx::Rgb(50, 54, 70), 230);
                UIText(EditorPanelFont(), (r.L + r.R) * 0.5f, (r.T + r.B) * 0.5f, FitText(list[i].Name, EditorPanelFont().Handle, r.R - r.L - 8, false), white, 1);
            }
            const std::string help = hover >= 0 ? std::string(list[hover].Help) : std::string("次郎形式再生ボタンで、次郎形式の時刻・疑似再生に切り替えられます");
            UIText(sfont, p.L + 16, p.B - lh * 1.2f, FitText(help, sfont.Handle, p.R - p.L - 32, false), gray);
        }
        else if (Editor.Panel == EditorState::PanelType::Grid) {
            UIText(sfont, p.L + 16, p.T + lh, FitText("分割数: クリックで選択 / 右クリックで★切替 / ×で登録削除 (初期の分割数は削除不可)", sfont.Handle, p.R - p.L - 32, false), white);
            const auto favorites = EditorGridList();
            const auto grids = EditorGridPanelChoices();
            for (int i = 0; i < (int)grids.size() + 3; ++i) {
                const UIRect r = EditorPanelRow(i, 4);
                if (i > (int)grids.size()) {
                    const bool previous = i == (int)grids.size() + 1;
                    const int page = EditorGridPageIndex(), last = ((int)EditorGridChoices().size() - 1) / EditorGridsPerPage;
                    UIBox(r, gfx::Rgb(50, 54, 70), 230);
                    UIText(sfont, (r.L + r.R) * 0.5f, (r.T + r.B) * 0.5f,
                        previous ? "◀ 前へ" : "次へ ▶  " + std::to_string(page + 1) + "/" + std::to_string(last + 1),
                        previous ? (page > 0 ? white : gray) : (page < last ? white : gray), 1);
                    continue;
                }
                if (i == (int)grids.size()) {
                    UIBox(r, gfx::Rgb(60, 90, 110), 230);
                    UIText(EditorPanelFont(), (r.L + r.R) * 0.5f, (r.T + r.B) * 0.5f, "+ 任意の分割数", white, 1); continue;
                }
                const int g = grids[i];
                const bool fav = std::ranges::find(favorites, g) != favorites.end();
                const bool current = g == Editor.Division;
                UIBox(r, current ? gfx::Rgb(214, 116, 20) : gfx::Rgb(50, 54, 70), 230);
                const auto remove = EditorGridDeleteButton(i);
                const float right = EditorDefaultGrid(g) ? r.R : remove.L;
                UIText(EditorPanelFont(), (r.L + right) * 0.5f, (r.T + r.B) * 0.5f, FitText((fav ? "★ " : "") + std::to_string(g) + " 分割", EditorPanelFont().Handle, right - r.L - 8, false), white, 1);
                if (!EditorDefaultGrid(g)) {
                    UIBox(remove, gfx::Rgb(116, 48, 60), 255);
                    UIText(sfont, (remove.L + remove.R) * 0.5f, (remove.T + remove.B) * 0.5f, "×", white, 1);
                }
            }
        }
        else {
            UIText(sfont, p.L + 16, p.T + lh, FitText("ヘッダー (クリックで編集 / 目: 演奏画面の表示切替 / コース項目は今のコースだけ)", sfont.Handle, p.R - p.L - 32, false), white);
            const auto& fields = EditorHeaderFields();
            // 値の欄は、いちばん長い項目名のすぐ右から (説明が省略されにくいよう、名前の欄を必要な幅だけにする)
            float keyw = 0;
            for (const auto& f : fields) { keyw = std::max(keyw, (float)GetStrlenCached(f.Key, EditorPanelFont().Handle)); }
            for (int i = 0; i < (int)fields.size(); ++i) {
                const UIRect r = EditorPanelRow(i, 2);
                const auto& f = fields[i];
                std::string value = tja::Document::GetValue(f.CourseField ? EditorCourse()->Header : Editor.Doc.Header, f.Key);
                if (value.empty() && (std::string_view(f.Key) == "SONGVOL" || std::string_view(f.Key) == "SEVOL")) { value = "100"; }
                UIBox(r, gfx::Rgb(36, 40, 54), 230);
                const bool button = std::string(f.Key) == "WAVE" || std::string(f.Key) == "BGMOVIE" || std::string(f.Key) == "SONGURL" || std::string(f.Key) == "MOVIEURL";
                const bool visibility = std::string_view(f.Key) == "TITLE" || std::string_view(f.Key) == "SUBTITLE";
                const float vr = visibility ? EditorHeaderVisibilityButton(i).L - 8.0f : button ? r.R - EditorTextWidth("参照…") - 28.0f : r.R - 8.0f;
                const float labelWidth = std::min(keyw, std::max(1.0f, (vr - r.L - 28.0f) * 0.5f));
                UIText(EditorPanelFont(), r.L + 8, (r.T + r.B) * 0.5f,
                    FitText(f.Key, EditorPanelFont().Handle, labelWidth, false), f.CourseField ? gfx::Rgb(160, 210, 255) : white);
                const float vx = r.L + 8.0f + labelWidth + 20.0f;
                UIText(EditorPanelFont(), vx, (r.T + r.B) * 0.5f, FitText(value.empty() ? std::string(f.Help) : value, EditorPanelFont().Handle, std::max(1.0f, vr - vx), false), value.empty() ? gray : white);
                if (button) {
                    const UIRect b = EditorWaveButton(i);
                    UIBox(b, gfx::Rgb(60, 64, 84), 230);
                    UIText(sfont, (b.L + b.R) * 0.5f, (b.T + b.B) * 0.5f, std::string(f.Key) == "WAVE" || std::string(f.Key) == "BGMOVIE" ? "参照…" : "取得", white, 1);
                }
                if (visibility) {
                    const bool hidden = value.starts_with("--");
                    EditorButton icon;
                    icon.Rect = EditorHeaderVisibilityButton(i);
                    icon.Icon = hidden ? EditorIcon::Hidden : EditorIcon::Visible;
                    UIBox(icon.Rect, hidden ? gfx::Rgb(78, 104, 150) : gfx::Rgb(60, 64, 84), 255);
                    EditorDrawIcon(icon, white);
                }
            }
            const char* labels[] = { "コースを追加", "このコースを削除" };
            for (int i = 0; i < 2; ++i) {
                const UIRect b = EditorHeaderButton(i);
                UIBox(b, gfx::Rgb(60, 64, 84), 230);
                UIText(sfont, (b.L + b.R) * 0.5f, (b.T + b.B) * 0.5f, labels[i], white, 1);
            }
        }
    }
#ifndef __ANDROID__
    EditorDrawFields();
    if (Editor.Editing) {
        const UIRect p = EditorEditRect(), box = EditorEditBox();
        UIBox({ 0, 0, resx, resy }, black, 120);
        UIBox(p, gfx::Rgb(18, 20, 32), 245);
        UIBox(p, accent, 255, false);
        UIText(sfont, p.L + 20, p.T + lh * 0.9f, FitText(Editor.EditLabel, sfont.Handle, p.R - p.L - 40, false), white);
        UIBox(box, gfx::Rgb(36, 40, 54), 255);
        UIBox(box, gray, 255, false);
        gfx::SetClip((int)box.L + 8, (int)box.T, (int)box.R - 8, (int)box.B);
        EditorDrawKeyInput((int)box.L + 12, (int)((box.T + box.B) * 0.5f - sfont.Size * 0.5f), Editor.EditHandle);
        gfx::ResetClip();
        const char* labels[] = { "決定 (Enter)", "キャンセル (Esc)" };
        for (int i = 0; i < 2; ++i) {
            const UIRect b = EditorEditButton(i);
            const bool over = b.Contains(PointerPrev.X, PointerPrev.Y);
            UIBox(b, i == 0 ? (over ? gfx::Rgb(255, 220, 90) : accent) : (over ? gfx::Rgb(80, 86, 110) : gfx::Rgb(60, 64, 84)), 240);
            UIText(sfont, (b.L + b.R) * 0.5f, (b.T + b.B) * 0.5f, labels[i], white, 1);
        }
    }
    if (Editor.Picker) {
        UIBox({ 0, 0, resx, resy }, black, 150);
        UIText(sfont, resx * 0.5f, resy * 0.5f, "ファイルを選んでください", white, 1);
    }
    if (Editor.Estimating) {
        UIBox({ 0, 0, resx, resy }, black, 150);
        UIText(sfont, resx * 0.5f, resy * 0.5f, "音源を解析しています...", white, 1);
    }
#endif
    if (Editor.WaveAnalyzing) {
        UIBox({ 0, 0, resx, resy }, black, 150);
        UIText(sfont, resx * 0.5f, resy * 0.5f, "波形の打音を解析しています...", white, 1);
    }
}

// 文字入力欄: 入力中の文字・カーソル・変換中の文字を描く (変換の候補の一覧は OS の IME が出す)
void Game::DrawTextInput(int x,int y,int handle) {
    sys::DrawTextInput(handle, (float)x, (float)y);
}

void Game::EditorDrawKeyInput(int x, int y, int handle) {
    sys::DrawTextInput(handle, (float)x, (float)y, gfx::Rgb(245, 245, 250), gfx::Rgb(255, 210, 70));
}

void Game::EditorDrawFields() {
#ifndef __ANDROID__
    const auto& sfont = EditorFont();
    const float resx = EditorResX(), resy = EditorResY(), lh = EditorLine();
    const auto black = gfx::Rgb(0, 0, 0), white = gfx::Rgb(255, 255, 255), gray = gfx::Rgb(150, 150, 150), accent = gfx::Rgb(255, 200, 40);
    if (Editor.FormOpen) {
        const UIRect p = EditorFieldsRect();
        UIBox({ 0, 0, resx, resy }, black, 140); UIBox(p, gfx::Rgb(18, 20, 32), 250); UIBox(p, accent, 255, false);
        UIText(sfont, p.L + 16, p.T + lh * 0.8f, Editor.FormTitle + "  (Tab: 次の欄)", white);
        for (int i = 0; i < (int)Editor.FormFields.size(); ++i) {
            const auto box = EditorFieldRect(i);
            UIText(sfont, p.L + 16, (box.T + box.B) * 0.5f, FitText(Editor.FormFields[i].Label, sfont.Handle, box.L - p.L - 24, false), white);
            UIBox(box, gfx::Rgb(36, 40, 54), 255); UIBox(box, i == Editor.FormActive ? accent : gray, 255, false);
            if (!Editor.FormFields[i].Choices.empty()) {
                UIText(sfont, box.L + 12, (box.T + box.B) * 0.5f, "◀", accent);
                UIText(sfont, (box.L + box.R) * 0.5f, (box.T + box.B) * 0.5f, EditorFieldValue(i), white, 1);
                UIText(sfont, box.R - 12, (box.T + box.B) * 0.5f, "▶", accent, 2);
            }
            else if (i == Editor.FormActive) {
                gfx::SetClip((int)box.L + 8, (int)box.T, (int)box.R - 8, (int)box.B);
                EditorDrawKeyInput((int)box.L + 10, (int)((box.T + box.B - sfont.Size) * 0.5f), Editor.FormFields[i].Handle);
                gfx::ResetClip();
            }
            else { UIText(sfont, box.L + 10, (box.T + box.B) * 0.5f, FitText(EditorFieldValue(i), sfont.Handle, box.R - box.L - 20, false), white); }
        }
        for (int i = 0; i < 2; ++i) {
            const auto button = EditorFieldsButton(i); UIBox(button, i == 0 ? accent : gfx::Rgb(60, 64, 84), 240);
            UIText(sfont, (button.L + button.R) * 0.5f, (button.T + button.B) * 0.5f, i == 0 ? "適用 (Enter)" : "キャンセル (Esc)", white, 1);
        }
    }
#endif
}

auto Game::EditorFieldsRect() const -> UIRect {
    const float height = std::min(EditorResY() * 0.9f, EditorLine() * (4.4f + 1.8f * Editor.FormFields.size()));
    return { EditorResX() * 0.08f, (EditorResY() - height) * 0.5f, EditorResX() * 0.92f, (EditorResY() + height) * 0.5f };
}

auto Game::EditorFieldRect(int index) const -> UIRect {
    const auto p = EditorFieldsRect();
    const float row = (p.B - p.T - EditorLine() * 4) / std::max<size_t>(1, Editor.FormFields.size());
    const float top = p.T + EditorLine() * 1.8f + index * row;
    return { p.L + (p.R - p.L) * 0.35f, top, p.R - 16, top + row - 4 };
}

auto Game::EditorFieldsButton(int index) const -> UIRect {
    const auto p = EditorFieldsRect();
    const float width = (p.R - p.L - 48) * 0.5f;
    return { p.L + 16 + index * (width + 16), p.B - EditorLine() * 1.8f, p.L + 16 + index * (width + 16) + width, p.B - 8 };
}

auto Game::EditorEditRect() const -> UIRect {
    const float lh = EditorLine();
    return { EditorResX() * 0.1f, EditorResY() * 0.3f, EditorResX() * 0.9f, EditorResY() * 0.3f + lh * 6.6f };
}

auto Game::EditorEditBox() const -> UIRect {
    const UIRect p = EditorEditRect();
    const float lh = EditorLine();
    return { p.L + 20, p.T + lh * 1.8f, p.R - 20, p.T + lh * 3.6f };
}

auto Game::EditorEditButton(int i) const -> UIRect {
    const UIRect p = EditorEditRect();
    const float lh = EditorLine(), w = (p.R - p.L - 60.0f) / 2.0f, h = lh * 1.9f;
    const float x = p.L + 20.0f + (w + 20.0f) * i;
    return { x, p.B - h - 16.0f, x + w, p.B - 16.0f };
}

auto Game::EditorStartButton(int i) const -> UIRect {
    const float w = std::min(EditorResX() * 0.5f, EditorResX() - 64.0f), h = EditorLine() * 2.8f;
    const float cx = EditorResX() * 0.5f, y = EditorResY() * 0.34f + (h + 18.0f) * i;
    return { cx - w * 0.5f, y, cx + w * 0.5f, y + h };
}

auto Game::EditorHeaderVisibilityButton(int row) const -> UIRect {
    const UIRect r = EditorPanelRow(row, 2);
    const float side = std::min(r.B - r.T - 8.0f, EditorPanelFont().Size * 1.35f);
    const float cy = (r.T + r.B) * 0.5f;
    return { r.R - 6.0f - side, cy - side * 0.5f, r.R - 6.0f, cy + side * 0.5f };
}

void Game::EditorToggleHeaderVisibility(const char* key) {
    if (!Editor.Open || (std::string_view(key) != "TITLE" && std::string_view(key) != "SUBTITLE")) { return; }
    std::string value = tja::Document::GetValue(Editor.Doc.Header, key);
    const bool hidden = value.starts_with("--");
    if (hidden) { value.erase(0, 2); }
    else { value.insert(0, "--"); }
    EditorChange();
    tja::Document::SetValue(Editor.Doc.Header, key, value);
    EditorMessage(std::string(key) + (hidden ? ": 演奏画面に表示" : ": 演奏画面では非表示"));
}

auto Game::EditorWaveButton(int row) const -> UIRect {
    const UIRect r = EditorPanelRow(row, 2);
    return { r.R - EditorTextWidth("参照…") - 24.0f, r.T + 2, r.R - 4, r.B - 2 };
}

auto Game::EditorHeaderButton(int i) const -> UIRect {
    const UIRect p = EditorPanelRect();
    const float w = (p.R - p.L - 48.0f) / 2.0f, h = EditorLine() * 1.9f;
    const float x = p.L + 16.0f + (w + 16.0f) * i;
    return { x, p.B - h - 12.0f, x + w, p.B - 12.0f };
}

unsigned int Game::EditorNoteColor(char type) {
    switch (type) {
    case '1': case '3': return gfx::Rgb(235, 72, 52);
    case '2': case '4': return gfx::Rgb(64, 170, 235);
    case '5': case '6': return gfx::Rgb(250, 200, 40);
    case '7': case '9': return gfx::Rgb(250, 130, 40);
    case '8': return gfx::Rgb(200, 200, 200);
    default: return gfx::Rgb(200, 120, 255);
    }
}

int Game::EditorNoteImage(char type) {
    switch (type) {
    case '1': return 1; case '2': return 2; case '3': return 3; case '4': return 4;
    case '5': return 5; case '6': return 8; case '7': return 11; case '8': return 7; case '9': return 13;
    default: return -1;
    }
}

bool Game::EditorDrawSkinNote(float x, float cy, float lane_h, int index, int alpha) {
    const auto& img = Skin.Base->Playing.Image.Note;
    if (index < 0 || index >= (int)img.Handles.size() || img.Size.Height <= 0) { return false; }
    const float h = lane_h * 1.2f, w = h * img.Size.Width / img.Size.Height;
    gfx::SetBlend(gfx::Blend::Alpha, alpha);
    gfx::DrawTexture(img.Handles[index], x - w * 0.5f, cy - h * 0.5f, x + w * 0.5f, cy + h * 0.5f);
    gfx::SetBlend(gfx::Blend::None);
    return true;
}

void Game::EditorDrawNote(float x, float cy, float lane_h, char type, int alpha) {
    if (EditorDrawSkinNote(x, cy, lane_h, EditorNoteImage(type), alpha)) { return; }
    const bool big = type == '3' || type == '4' || type == '6' || type == '9';
    const float r = lane_h * (big ? 0.42f : 0.3f) * (type == '8' ? 0.6f : 1.0f);
    gfx::SetBlend(gfx::Blend::Alpha, alpha);
    gfx::Circle(x, cy, r, 32, EditorNoteColor(type), true);
    gfx::Circle(x, cy, r, 32, gfx::Rgb(255, 255, 255), false, 2.0f);
    gfx::SetBlend(gfx::Blend::None);
}

void Game::EditorDrawIcon(const EditorButton& b, unsigned int color) {
    const UIRect& r = b.Rect;
    const int w = (int)std::ceil(r.R - r.L), h = (int)std::ceil(r.B - r.T);
    // 音符 (スキンの画像と番号の文字) はそのまま描く
    if (b.Icon == EditorIcon::Note || w <= 0 || h <= 0 || w > 512 || h > 512) { EditorDrawIconShape(b, color); return; }
    constexpr int Scale = 4;
    // 速度計は針の向きが倍率で変わるので、倍率もキーに入れる (文字は縮小せずに後で描く)
    const uint64_t speed = b.Icon == EditorIcon::SpeedValue ? (uint64_t)std::clamp((int)std::lround(Editor.PreviewSpeed * 100), 0, 0xFFFF) : 0;
    const uint64_t key = ((uint64_t)b.Icon << 48) | (speed << 32) | ((uint64_t)w << 16) | (uint64_t)h;
    auto it = Editor.IconCache.find(key);
    if (it == Editor.IconCache.end()) {
        // 白い図形を 4 倍の大きさで描き、2 倍 → 等倍と半分ずつ縮小する (線形補間で 2×2 の平均になり、縁が滑らかになる)。
        // 描画先は「白・透明」で消しておくと、重ねた結果が白のままアルファだけが付く
        gfx::Texture big = gfx::CreateTexture(w * Scale, h * Scale, true);
        gfx::Texture mid = gfx::CreateTexture(w * 2, h * 2, true);
        gfx::Texture out = gfx::CreateTexture(w, h, true);
        if (!big || !mid || !out) { EditorDrawIconShape(b, color); return; }
        int cl = 0, ct = 0, cr = 0, cb = 0;
        const bool clipped = gfx::GetClip(cl, ct, cr, cb);
        const gfx::Blend blend = gfx::CurrentBlend();
        const int param = gfx::CurrentBlendParam();
        int br = 255, bg = 255, bb = 255;
        gfx::GetBright(br, bg, bb);
        gfx::ResetClip();
        gfx::SetBright(255, 255, 255);
        gfx::SetAddColor(0, 0, 0);
        gfx::SetBlend(gfx::Blend::Alpha, 255);
        gfx::SetTarget(&big);
        gfx::Clear(gfx::Rgb(255, 255, 255), 0);
        EditorButton scaled = b;
        scaled.Rect = { 0.0f, 0.0f, (float)(w * Scale), (float)(h * Scale) };
        EditorDrawIconShape(scaled, gfx::Rgb(255, 255, 255), false);
        gfx::SetTarget(&mid);
        gfx::Clear(gfx::Rgb(255, 255, 255), 0);
        gfx::DrawTexture(big, 0, 0, (float)(w * 2), (float)(h * 2));
        gfx::SetTarget(&out);
        gfx::Clear(gfx::Rgb(255, 255, 255), 0);
        gfx::DrawTexture(mid, 0, 0, (float)w, (float)h);
        gfx::SetTarget(nullptr);
        gfx::SetBright(br, bg, bb);
        gfx::SetBlend(blend, param);
        if (clipped) { gfx::SetClip(cl, ct, cr, cb); }
        it = Editor.IconCache.emplace(key, std::move(out)).first;
    }
    // 白い画像に色を掛けて描く
    int br = 255, bg = 255, bb = 255;
    gfx::GetBright(br, bg, bb);
    const gfx::Blend blend = gfx::CurrentBlend();
    const int param = gfx::CurrentBlendParam();
    gfx::SetBright(gfx::R(color) * br / 255, gfx::G(color) * bg / 255, gfx::B(color) * bb / 255);
    if (blend == gfx::Blend::None) { gfx::SetBlend(gfx::Blend::Alpha, 255); }
    gfx::DrawTexture(it->second, r.L, r.T, r.L + (float)w, r.T + (float)h);
    gfx::SetBlend(blend, param);
    gfx::SetBright(br, bg, bb);
    if (b.Icon == EditorIcon::SpeedValue) {
        UIText(EditorFont(), (r.L + (r.B - r.T) + r.R) * 0.5f, (r.T + r.B) * 0.5f, FormatNumber(Editor.PreviewSpeed, 2) + "x", color, 1);
    }
}

void Game::EditorDrawIconShape(const EditorButton& b, unsigned int color, bool text) {
    const UIRect& r = b.Rect;
    const float cx = (r.L + r.R) * 0.5f, cy = (r.T + r.B) * 0.5f, s = (r.B - r.T) * 0.30f;
    const float t = std::max(2.0f, s * 0.16f);
    auto line = [&](float x0, float y0, float x1, float y1) { gfx::Line(x0, y0, x1, y1, color, t); };
    auto arrowhead = [&](float x, float y, float dx, float dy) {
        // (x, y) を先端に、(dx, dy) の向きの矢じり
        const float len = s * 0.45f, nx = -dy, ny = dx;
        gfx::Triangle(x, y, x - dx * len + nx * len * 0.6f, y - dy * len + ny * len * 0.6f, x - dx * len - nx * len * 0.6f, y - dy * len - ny * len * 0.6f, color, true);
    };
    switch (b.Icon) {
    case EditorIcon::Visible:
    case EditorIcon::Hidden: {
        for (int i = 0; i < 12; ++i) {
            const float x0 = cx - s + 2 * s * i / 12.0f, x1 = cx - s + 2 * s * (i + 1) / 12.0f;
            const float y0 = s * 0.55f * std::sin(3.14159265f * i / 12.0f), y1 = s * 0.55f * std::sin(3.14159265f * (i + 1) / 12.0f);
            line(x0, cy - y0, x1, cy - y1);
            line(x0, cy + y0, x1, cy + y1);
        }
        gfx::Circle(cx, cy, s * 0.25f, 16, color, true);
        if (b.Icon == EditorIcon::Hidden) { line(cx - s * 0.9f, cy + s * 0.9f, cx + s * 0.9f, cy - s * 0.9f); }
        break;
    }
    case EditorIcon::DemoStart:
        line(cx - s * 0.8f, cy - s, cx - s * 0.8f, cy + s);
        gfx::Triangle(cx - s * 0.4f, cy - s * 0.8f, cx - s * 0.4f, cy + s * 0.8f, cx + s * 0.8f, cy, color, true);
        break;
    case EditorIcon::WaveAlign:
        for (int i = -2; i <= 2; ++i) {
            const float x = cx + i * s * 0.4f, height = (i == 0 ? 1.0f : i % 2 ? 0.4f : 0.7f) * s;
            line(x, cy - height, x, cy + height);
        }
        line(cx - s, cy + s, cx - s * 0.25f, cy + s);
        arrowhead(cx - s * 0.25f, cy + s, 1, 0);
        break;
    case EditorIcon::Unfold:    // 開いた小節枠と、登録を外す ×
        gfx::Box(cx - s, cy - s * 0.8f, cx + s * 0.2f, cy + s * 0.8f, color, false, t);
        line(cx - s * 0.6f, cy - s * 0.4f, cx - s * 0.6f, cy + s * 0.4f);
        line(cx + s * 0.35f, cy - s * 0.4f, cx + s, cy + s * 0.4f);
        line(cx + s * 0.35f, cy + s * 0.4f, cx + s, cy - s * 0.4f);
        break;
    case EditorIcon::AudioLength:    // 時計から小節の枠へ
        gfx::Circle(cx - s * 0.35f, cy - s * 0.25f, s * 0.65f, 24, color, false, t);
        line(cx - s * 0.35f, cy - s * 0.7f, cx - s * 0.35f, cy - s * 0.25f);
        line(cx - s * 0.35f, cy - s * 0.25f, cx, cy - s * 0.05f);
        line(cx + s * 0.25f, cy + s * 0.5f, cx + s, cy + s * 0.5f);
        arrowhead(cx + s, cy + s * 0.5f, 1, 0);
        line(cx + s, cy - s * 0.6f, cx + s, cy + s * 0.1f);
        break;
    case EditorIcon::Save:      // フロッピー
        gfx::Box(cx - s, cy - s, cx + s, cy + s, color, false, t);
        gfx::Box(cx - s * 0.55f, cy - s, cx + s * 0.45f, cy - s * 0.35f, color, true);
        gfx::Box(cx - s * 0.6f, cy + s * 0.15f, cx + s * 0.6f, cy + s, color, false, t);
        break;
    case EditorIcon::Test:      // 太鼓 (丸の中のばち)
        gfx::Circle(cx, cy, s, 32, color, false, t);
        gfx::Circle(cx, cy, s * 0.45f, 32, color, true);
        line(cx + s * 0.6f, cy - s * 1.05f, cx + s * 1.1f, cy - s * 0.4f);
        break;
    case EditorIcon::Close:
        line(cx - s * 0.8f, cy - s * 0.8f, cx + s * 0.8f, cy + s * 0.8f);
        line(cx - s * 0.8f, cy + s * 0.8f, cx + s * 0.8f, cy - s * 0.8f);
        break;
    case EditorIcon::Play:
        gfx::Triangle(cx - s * 0.6f, cy - s, cx - s * 0.6f, cy + s, cx + s, cy, color, true);
        break;
    case EditorIcon::SpeedDown:
    case EditorIcon::SpeedUp: {
        // 速度の増減: 二重三角 (早送り / 巻き戻し)。
        const float dir = b.Icon == EditorIcon::SpeedUp ? 1.0f : -1.0f;
        for (int i = -1; i <= 1; i += 2) {
            const float x = cx + i * s * 0.48f;
            gfx::Triangle(x - dir * s * 0.42f, cy - s * 0.65f, x - dir * s * 0.42f, cy + s * 0.65f, x + dir * s * 0.42f, cy, color, true);
        }
        break;
    }
    case EditorIcon::SpeedValue: {
        // 速度計と現在倍率。数値を押して直接入力できる。
        const float h = r.B - r.T;
        const float gx = r.L + h * 0.48f, gy = cy + s * 0.3f;
        const float radius = s * 0.85f;
        float px = gx - radius, py = gy;
        for (int i = 1; i <= 12; ++i) {
            const float a = 3.14159265f * (1.0f - i / 12.0f);
            const float x = gx + std::cos(a) * radius;
            const float y = gy - std::sin(a) * radius;
            line(px, py, x, y);
            px = x; py = y;
        }
        const float a = 3.14159265f * (1.0f - std::clamp((float)(Editor.PreviewSpeed - 0.25) / 1.75f, 0.0f, 1.0f));
        line(gx, gy, gx + std::cos(a) * radius * 0.7f, gy - std::sin(a) * radius * 0.7f);
        gfx::Circle(gx, gy, t * 0.65f, 12, color, true);
        if (text) { UIText(EditorFont(), (r.L + h + r.R) * 0.5f, cy, FormatNumber(Editor.PreviewSpeed, 2) + "x", color, 1); }
        break;
    }
    case EditorIcon::TaikoSound:
        gfx::Box(cx-s, cy-s*.35f, cx-s*.5f, cy+s*.35f, color, true);
        gfx::Triangle(cx-s*.5f, cy-s*.35f, cx+s*.1f, cy-s*.9f, cx+s*.1f, cy+s*.9f, color, true);
        if(b.Active) {line(cx+s*.4f,cy-s*.5f,cx+s*.8f,cy);line(cx+s*.8f,cy,cx+s*.4f,cy+s*.5f);}
        else {line(cx+s*.35f,cy-s*.45f,cx+s,cy+s*.45f);line(cx+s*.35f,cy+s*.45f,cx+s,cy-s*.45f);}
        break;
    case EditorIcon::JiroPreview:
        gfx::Circle(cx, cy, s, 32, color, false, t);
        gfx::Circle(cx, cy, s*.72f, 32, color, false, t);
        line(cx-s*.7f,cy-s*.8f,cx+s*.65f,cy+s*.7f);
        line(cx+s*.7f,cy-s*.8f,cx-s*.65f,cy+s*.7f);
        break;    case EditorIcon::Stop:
        gfx::Box(cx - s * 0.75f, cy - s * 0.75f, cx + s * 0.75f, cy + s * 0.75f, color, true);
        break;
    case EditorIcon::Undo:
    case EditorIcon::Redo: {    // 半円の矢印
        const float dir = b.Icon == EditorIcon::Undo ? 1.0f : -1.0f;
        const int seg = 10;
        float px = 0, py = 0;
        for (int i = 0; i <= seg; ++i) {
            const float a = 3.14159f * (0.15f + 0.85f * i / seg);
            const float x = cx - dir * std::cos(a) * s * 0.8f, y = cy + s * 0.2f - std::sin(a) * s * 0.8f;
            if (i > 0) { line(px, py, x, y); }
            px = x; py = y;
        }
        arrowhead(cx - dir * s * 0.8f - dir * s * 0.05f, cy + s * 0.55f, 0.0f, 1.0f);
        break;
    }
    case EditorIcon::Fold:      // 折りたたみ: 2 本の線を矢印で寄せる
        line(cx - s * 0.8f, cy - s * 0.8f, cx + s * 0.8f, cy - s * 0.8f);
        line(cx - s * 0.8f, cy + s * 0.8f, cx + s * 0.8f, cy + s * 0.8f);
        line(cx, cy - s * 0.6f, cx, cy - s * 0.1f); line(cx - s * 0.3f, cy - s * 0.4f, cx, cy - s * 0.1f); line(cx + s * 0.3f, cy - s * 0.4f, cx, cy - s * 0.1f);
        line(cx, cy + s * 0.6f, cx, cy + s * 0.1f); line(cx - s * 0.3f, cy + s * 0.4f, cx, cy + s * 0.1f); line(cx + s * 0.3f, cy + s * 0.4f, cx, cy + s * 0.1f);
        break;
    case EditorIcon::Insert:
        line(cx - s * 0.8f, cy, cx + s * 0.8f, cy);
        line(cx, cy - s * 0.8f, cx, cy + s * 0.8f);
        break;
    case EditorIcon::Delete:    // ごみ箱
        gfx::Box(cx - s * 0.6f, cy - s * 0.5f, cx + s * 0.6f, cy + s, color, false, t);
        line(cx - s * 0.85f, cy - s * 0.6f, cx + s * 0.85f, cy - s * 0.6f);
        line(cx - s * 0.25f, cy - s * 0.85f, cx + s * 0.25f, cy - s * 0.85f);
        line(cx - s * 0.2f, cy - s * 0.2f, cx - s * 0.2f, cy + s * 0.7f);
        line(cx + s * 0.2f, cy - s * 0.2f, cx + s * 0.2f, cy + s * 0.7f);
        break;
    case EditorIcon::Header:    // 書類
        gfx::Box(cx - s * 0.7f, cy - s, cx + s * 0.7f, cy + s, color, false, t);
        for (int i = 0; i < 3; ++i) { line(cx - s * 0.4f, cy - s * 0.45f + i * s * 0.45f, cx + s * 0.4f, cy - s * 0.45f + i * s * 0.45f); }
        break;
    case EditorIcon::Command:   // #
        line(cx - s * 0.3f, cy - s, cx - s * 0.5f, cy + s);
        line(cx + s * 0.5f, cy - s, cx + s * 0.3f, cy + s);
        line(cx - s * 0.9f, cy - s * 0.35f, cx + s * 0.9f, cy - s * 0.35f);
        line(cx - s * 0.9f, cy + s * 0.35f, cx + s * 0.9f, cy + s * 0.35f);
        break;
    case EditorIcon::Gimmick: { // 星
        float px[10], py[10];
        for (int i = 0; i < 10; ++i) {
            const float a = -3.14159f / 2 + i * 3.14159f / 5, rr = i % 2 == 0 ? s : s * 0.42f;
            px[i] = cx + std::cos(a) * rr; py[i] = cy + std::sin(a) * rr;
        }
        for (int i = 0; i < 10; ++i) { gfx::Triangle(cx, cy, px[i], py[i], px[(i + 1) % 10], py[(i + 1) % 10], color, true); }
        break;
    }
    case EditorIcon::CommandDelete: {    // 命令 (#) と ×
        const float hx = cx - s * 0.3f;
        line(hx - s * 0.2f, cy - s * 0.8f, hx - s * 0.35f, cy + s * 0.8f);
        line(hx + s * 0.3f, cy - s * 0.8f, hx + s * 0.15f, cy + s * 0.8f);
        line(hx - s * 0.65f, cy - s * 0.3f, hx + s * 0.55f, cy - s * 0.3f);
        line(hx - s * 0.75f, cy + s * 0.3f, hx + s * 0.45f, cy + s * 0.3f);
        line(cx + s * 0.35f, cy + s * 0.15f, cx + s * 0.95f, cy + s * 0.85f);
        line(cx + s * 0.95f, cy + s * 0.15f, cx + s * 0.35f, cy + s * 0.85f);
        break;
    }
    case EditorIcon::JiroFix: {         // 時計と +
        gfx::Circle(cx - s * 0.15f, cy, s * 0.75f, 24, color, false, t);
        line(cx - s * 0.15f, cy, cx - s * 0.15f, cy - s * 0.5f);
        line(cx - s * 0.15f, cy, cx + s * 0.2f, cy);
        line(cx + s * 0.55f, cy + s * 0.65f, cx + s * 1.05f, cy + s * 0.65f);
        line(cx + s * 0.8f, cy + s * 0.4f, cx + s * 0.8f, cy + s * 0.9f);
        break;
    }
    case EditorIcon::BpmAnalyze: {      // メトロノーム
        line(cx - s * 0.75f, cy + s * 0.9f, cx + s * 0.75f, cy + s * 0.9f);
        line(cx - s * 0.75f, cy + s * 0.9f, cx - s * 0.3f, cy - s * 0.9f);
        line(cx + s * 0.75f, cy + s * 0.9f, cx + s * 0.3f, cy - s * 0.9f);
        line(cx - s * 0.3f, cy - s * 0.9f, cx + s * 0.3f, cy - s * 0.9f);
        line(cx, cy + s * 0.55f, cx + s * 0.6f, cy - s * 0.6f);
        gfx::Circle(cx + s * 0.38f, cy - s * 0.18f, t * 0.9f, 12, color, true);
        break;
    }
    case EditorIcon::Estimate: { // 波形
        float px = 0, py = 0;
        for (int i = 0; i <= 16; ++i) {
            const float x = cx - s + 2 * s * i / 16.0f, y = cy - std::sin(i * 3.14159f / 4) * s * (0.3f + 0.7f * (i % 4 == 2));
            if (i > 0) { line(px, py, x, y); }
            px = x; py = y;
        }
        break;
    }
    case EditorIcon::Select:    // 点線の四角
        for (int i = 0; i < 4; ++i) {
            const float a = -s + i * s * 0.5f;
            line(cx + a, cy - s * 0.8f, cx + a + s * 0.3f, cy - s * 0.8f);
            line(cx + a, cy + s * 0.8f, cx + a + s * 0.3f, cy + s * 0.8f);
        }
        line(cx - s, cy - s * 0.8f, cx - s, cy + s * 0.8f);
        line(cx + s, cy - s * 0.8f, cx + s, cy + s * 0.8f);
        break;
    case EditorIcon::Copy:      // 重なった 2 枚
        gfx::Box(cx - s * 0.9f, cy - s * 0.9f, cx + s * 0.3f, cy + s * 0.5f, color, false, t);
        gfx::Box(cx - s * 0.3f, cy - s * 0.4f, cx + s * 0.9f, cy + s, color, false, t);
        break;
    case EditorIcon::Cut:       // はさみ
        gfx::Circle(cx - s * 0.45f, cy + s * 0.55f, s * 0.3f, 16, color, false, t);
        gfx::Circle(cx + s * 0.45f, cy + s * 0.55f, s * 0.3f, 16, color, false, t);
        line(cx - s * 0.3f, cy + s * 0.3f, cx + s * 0.5f, cy - s);
        line(cx + s * 0.3f, cy + s * 0.3f, cx - s * 0.5f, cy - s);
        break;
    case EditorIcon::Paste:     // クリップボード
        gfx::Box(cx - s * 0.75f, cy - s * 0.7f, cx + s * 0.75f, cy + s, color, false, t);
        gfx::Box(cx - s * 0.35f, cy - s, cx + s * 0.35f, cy - s * 0.55f, color, true);
        line(cx - s * 0.4f, cy, cx + s * 0.4f, cy);
        line(cx - s * 0.4f, cy + s * 0.45f, cx + s * 0.4f, cy + s * 0.45f);
        break;
    case EditorIcon::Left:
        gfx::Triangle(cx + s * 0.5f, cy - s * 0.8f, cx + s * 0.5f, cy + s * 0.8f, cx - s * 0.6f, cy, color, true);
        break;
    case EditorIcon::Right:
        gfx::Triangle(cx - s * 0.5f, cy - s * 0.8f, cx - s * 0.5f, cy + s * 0.8f, cx + s * 0.6f, cy, color, true);
        break;
    case EditorIcon::Note: {
        const float lane = (r.B - r.T) * 0.62f;
        if (b.Note == '0') {    // 休符: 消しゴムの印
            line(cx - s * 0.6f, cy - s * 0.6f, cx + s * 0.6f, cy + s * 0.6f);
            line(cx - s * 0.6f, cy + s * 0.6f, cx + s * 0.6f, cy - s * 0.6f);
        }
        else {
            EditorDrawNote(cx, cy, lane, b.Note, 255);
        }
        // 種類の番号 (キーボードの 0～9)
        UIText(EditorFont(), r.L + 4.0f, r.T + EditorFont().Size * 0.45f, std::string(1, b.Note), color);
        break;
    }
    default:
        break;
    }
}

void Game::EditorDrawRollBody(float x0, float x1, float cy, float lane_h, char type) {
    const auto& img = Skin.Base->Playing.Image.Note;
    const int body = type == '6' ? 9 : 6;
    if (x1 > x0 && body < (int)img.Handles.size() && img.Size.Height > 0 && (type == '5' || type == '6')) {
        const float h = lane_h * 1.2f;
        gfx::DrawTexture(img.Handles[body], x0, cy - h * 0.5f, x1, cy + h * 0.5f);
        return;
    }
    UIBox({ x0, cy - lane_h * 0.22f, x1, cy + lane_h * 0.22f }, EditorNoteColor(type), 120);
}

void Game::EditorDrawGrid(const FontData& sfont) {
    const auto* course = EditorCourse();
    if (course == nullptr) { return; }
    const unsigned int white = gfx::Rgb(255, 255, 255), gray = gfx::Rgb(150, 150, 150), accent = gfx::Rgb(255, 200, 40);
    const int count = (int)course->Measures.size();
    const float noteHeight = EditorGridNoteHeight();
    const bool jiroMap = EditorJiroPreviewEnabled();
    const EditorTimeline* mapTiming = jiroMap ? &EditorTimelineCached() : nullptr;

    // 表示より前の小節で始まった連打と、各小節の頭の BPM
    double bpm = 120;
    if (const auto v = tja::Document::GetValue(Editor.Doc.Header, "BPM"); !v.empty()) { bpm = std::atof(v.c_str()); }
    bool rolling = false;
    bool gogo = false;
    char rolltype = '5';
    for (int m = 0; m < std::min(Editor.Top, count); ++m) {
        for (const auto& c : course->Measures[m].Commands) {
            if (tja::StartsWithKey(c.Text, "#BPMCHANGE")) { bpm = EditorCommandValue(c.Text, 10); }
            if (tja::StartsWithKey(c.Text, "#GOGOSTART")) { gogo = true; }
            if (tja::StartsWithKey(c.Text, "#GOGOEND")) { gogo = false; }
        }
        for (const auto& n : course->Measures[m].Notes) {
            if (n.Type == '5' || n.Type == '6' || n.Type == '7' || n.Type == '9') { rolling = true; rolltype = n.Type; }
            else if (n.Type == '8') { rolling = false; }
        }
    }

    // 置く位置のマウスカーソル
    int hm = -1, hs = -1;
    EditorHitLane(PointerPrev.X, PointerPrev.Y, hm, hs);

    for (int row = 0; row < EditorRows; ++row) {
        const int m = EditorRowMeasure(row);
        if (m < 0 || m >= count) { break; }
        const auto& measure = course->Measures[m];
        const UIRect lane = EditorLane(row);
        const float cy = (lane.T + lane.B) * 0.5f, lh = lane.B - lane.T;
        const auto* registration = EditorFoldAt(m) ? EditorFoldAt(m) : EditorOpenFoldAt(m);
        if (registration && registration->first == m) {
            const auto remove = EditorFoldDeleteButton(row);
            UIBox(remove, gfx::Rgb(116, 48, 60), 255);
            UIText(sfont, (remove.L + remove.R) * 0.5f, (remove.T + remove.B) * 0.5f, "×", white, 1);
        }
        // 折りたたんだ範囲: 1 本の帯にまとめる (クリックで開く)
        if (const auto* f = EditorFoldAt(m)) {
            size_t cmds = 0, notes = 0;
            for (int k = f->first; k <= f->second; ++k) {
                cmds += course->Measures[k].Commands.size(); notes += course->Measures[k].Notes.size();
                for (const auto& c : course->Measures[k].Commands) { if (tja::StartsWithKey(c.Text, "#BPMCHANGE")) { bpm = EditorCommandValue(c.Text, 10); } }
            }
            const UIRect band{ lane.L, cy - lh * 0.3f, lane.R, cy + lh * 0.3f };
            UIBox(band, gfx::Rgb(70, 60, 110), 230);
            UIBox(band, gfx::Rgb(150, 130, 220), 200, false);
            UIText(sfont, EditorGridArea().L, cy, std::to_string(f->first + 1) + "-" + std::to_string(f->second + 1), white);
            UIText(sfont, band.L + 12, cy, FitText("▶ " + std::to_string(f->second - f->first + 1) + " 小節を折りたたみ中 (命令 " + std::to_string(cmds) + "・音符 " + std::to_string(notes) + ")  クリックで開く", sfont.Handle, band.R - band.L - 24, false), white);
            rolling = false;
            for (int k = f->first; k <= f->second; ++k) {
                for (const auto& c : course->Measures[k].Commands) {
                    if (tja::StartsWithKey(c.Text, "#GOGOSTART")) { gogo = true; }
                    if (tja::StartsWithKey(c.Text, "#GOGOEND")) { gogo = false; }
                }
            }
            continue;
        }

        // 小節の頭の #BPMCHANGE はこの小節の BPM として表示する
        for (const auto& c : measure.Commands) {
            if (c.Num == 0 && tja::StartsWithKey(c.Text, "#BPMCHANGE")) { bpm = EditorCommandValue(c.Text, 10); }
        }
        const float labelx = EditorGridArea().L;
        std::string measureLabel = std::to_string(m + 1);
        if (mapTiming) { measureLabel += "  " + FormatNumber(mapTiming->TimeAt(m, 0) / 1000.0, 3) + "s"; }
        UIText(sfont, labelx, lane.T + sfont.Size * 0.6f, mapTiming ? FitText(measureLabel, sfont.Handle, EditorLabelWidth() - 8.0f, false) : measureLabel, m == Editor.CursorMeasure ? accent : white);
        if (const auto* registered = EditorOpenFoldAt(m); registered && registered->first == m) {
            const auto button = EditorFoldButton(row);
            UIBox(button, gfx::Rgb(70, 60, 110), 230);
            UIText(sfont, (button.L + button.R) * 0.5f, (button.T + button.B) * 0.5f, "▼", white, 1);
        }
        const float labelRight = lane.L;
        UIText(sfont, labelx, lane.B - sfont.Size * 0.6f, FitText("BPM " + FormatNumber(bpm, bpm == std::floor(bpm) ? 0 : 2), sfont.Handle, labelRight - labelx - 8.0f, false), gray);

        gfx::SetClip((int)(EditorGridArea().L+EditorLabelWidth()-noteHeight*.6f), (int)EditorGridArea().T, (int)EditorGridArea().R, (int)EditorGridArea().B);
        UIBox(lane, gfx::Rgb(50, 50, 58), 255);
        // スキンのレーンの画像を敷く (無ければ単色)
        if (const auto& laneimg = Skin.Base->Playing.Image.Lane; !laneimg.Handles.empty()) {
            gfx::SetBlend(gfx::Blend::Alpha, 170);
            gfx::DrawTexture(laneimg.Handles[0], lane.L, lane.T, lane.R, lane.B);
            gfx::SetBlend(gfx::Blend::None);
        }
        // グリッド (拍の線を明るく)
        double gogoFrom = 0;
        for (const auto& c : measure.Commands) {
            if (!tja::StartsWithKey(c.Text, "#GOGOSTART") && !tja::StartsWithKey(c.Text, "#GOGOEND")) { continue; }
            const double p = std::clamp((double)c.Num / std::max<int64_t>(c.Den, 1), 0.0, 1.0);
            if (gogo) { UIBox({ EditorX(lane, gogoFrom), lane.T, EditorX(lane, p), lane.B }, gfx::Rgb(255, 125, 20), 100); }
            gogo = tja::StartsWithKey(c.Text, "#GOGOSTART");
            gogoFrom = p;
        }
        if (gogo) { UIBox({ EditorX(lane, gogoFrom), lane.T, lane.R, lane.B }, gfx::Rgb(255, 125, 20), 100); }
        const int div = EditorDivOf(m);      // この小節のグリッドの数 (#MEASURE の長さに合わせる)
        const int beat = Editor.Division % 4 == 0 ? Editor.Division / 4 : 0;
        for (int k = 0; k <= div; ++k) {
            const float x = EditorX(lane, (double)k / div);
            const bool strong = k == 0 || k == div;
            const bool beatline = beat > 0 && k % beat == 0;
            gfx::Line(x, lane.T, x, lane.B, strong ? white : beatline ? gfx::Rgb(130, 130, 150) : gfx::Rgb(80, 80, 92), strong ? 2.0f : 1.0f);
        }
        if (Editor.Selecting) {
            const auto [a, b] = EditorSelection();
            if (a.Measure <= m && m <= b.Measure) {
                const double from = m == a.Measure ? (double)a.Num / a.Den : 0.0;
                const double to = m == b.Measure ? (double)b.Num / b.Den + 0.5 / div : 1.0;
                UIBox({ EditorX(lane, from), lane.T, EditorX(lane, std::min(to, 1.0)), lane.B }, gfx::Rgb(90, 150, 255), 90);
            }
        }
        if (m == Editor.CursorMeasure) {
            const float x = EditorX(lane, (double)Editor.CursorSlot / EditorDivOf(Editor.CursorMeasure));
            gfx::Line(x, lane.T - 4, x, lane.B + 4, accent, 3.0f);
        }

        // 連打の帯
        const bool rollingAtStart = rolling;
        const char rolltypeAtStart = rolltype;
        float rollstart = rolling ? lane.L : -1.0f;
        for (const auto& n : measure.Notes) {
            const float x = EditorX(lane, (double)n.Num / n.Den);
            if (n.Type == '5' || n.Type == '6' || n.Type == '7' || n.Type == '9') { rolling = true; rolltype = n.Type; rollstart = x; }
            else if (n.Type == '8' && rolling) {
                EditorDrawRollBody(rollstart, x, cy, noteHeight, rolltype);
                rolling = false;
                rollstart = -1.0f;
            }
        }
        if (rolling && rollstart >= 0) { EditorDrawRollBody(rollstart, lane.R, cy, noteHeight, rolltype); }

        char activeRoll = rollingAtStart ? rolltypeAtStart : '0';
        std::vector<char> rollTypes;
        rollTypes.reserve(measure.Notes.size());
        for (const auto& n : measure.Notes) {
            if (n.Type == '5' || n.Type == '6' || n.Type == '7' || n.Type == '9') { activeRoll = n.Type; }
            rollTypes.push_back(activeRoll);
            if (n.Type == '8') { activeRoll = '0'; }
        }
        // 演奏画面と同じく後の音符から描き、先の音符を手前にする。
        // 連打終端の種類は譜面順に確定しておく。
        for (size_t i=measure.Notes.size(); i-- > 0;) {
            const auto& n=measure.Notes[i];
            const char activeRoll=rollTypes[i];
            const float x = EditorX(lane, (double)n.Num / n.Den);
            if (n.Type == '8') {
                if (activeRoll == '6') {
                    if (!EditorDrawSkinNote(x, cy, noteHeight, 10, 255)) { EditorDrawNote(x, cy, noteHeight, '8', 255); }
                }
                else if (activeRoll != '7' && activeRoll != '9') { EditorDrawNote(x, cy, noteHeight, '8', 255); }
                continue;
            }
            EditorDrawNote(x, cy, noteHeight, n.Type, 255);
            if(n.Type=='7'||n.Type=='9') {
                const auto& counts=EditorTimelineCached().BalloonCounts;
                const auto count=counts.find({m,n.Num,n.Den});
                UIText(sfont,x,cy,std::to_string(count==counts.end()?5:count->second),white,1);
            }
            if (n.Type < '1' || n.Type > '9') { UIText(sfont, x, cy, std::string(1, n.Type), white, 1); }
        }
        if (hm == m && hs >= 0 && !Editor.Editing && Editor.Panel == EditorState::PanelType::None) {
            const float x = EditorX(lane, (double)hs / div);
            if (Editor.Palette != '0') { EditorDrawNote(x, cy, noteHeight, Editor.Palette, 110); }     // 消しゴムは左クリックでは使わないので印を出さない
        }
        for (const auto& c : measure.Commands) {
            if (c.Num != 0 && tja::StartsWithKey(c.Text, "#BPMCHANGE")) { bpm = EditorCommandValue(c.Text, 10); }
        }
        gfx::ResetClip();
    }
    // 疑似再生の位置
    if (Editor.Preview) {
        const auto& tl = EditorTimelineCached();
        int pm = 0;
        double pp = 0;
        if (tl.PosAt(EditorPreviewNow(), pm, pp) && EditorMeasureRow(pm) >= 0 && !EditorFoldAt(pm)) {
            const UIRect lane = EditorLane(EditorMeasureRow(pm));
            const float x = EditorX(lane, pp);
            gfx::Line(x, lane.T - 6, x, lane.B + 6, gfx::Rgb(80, 255, 140), 3.0f);
        }
    }
    // 命令のラベル
    for (const auto& hit : EditorCommandRects()) {
        const auto& cmd = course->Measures[hit.Measure].Commands[hit.Index];
        UIBox(hit.Rect, gfx::Rgb(70, 60, 110), 230);
        UIText(sfont, hit.Rect.L + 6, (hit.Rect.T + hit.Rect.B) * 0.5f, FitText(cmd.Text, sfont.Handle, hit.Rect.R - hit.Rect.L - 10, false), white);
        gfx::Line(hit.Rect.L, hit.Rect.B, hit.Rect.L, EditorLane(EditorMeasureRow(hit.Measure)).T, gfx::Rgb(150, 130, 220), 1.0f);
    }
    // スクロールバー
    const UIRect area = EditorGridArea();
    if (count > EditorRows) {
        const float h = area.B - area.T;
        UIBox({ area.R - 4, area.T + h * Editor.Top / count, area.R, area.T + h * std::min(count, Editor.Top + EditorRows) / count }, white, 160);
    }
}

bool Game::EditorHitLane(int x, int y, int& measure, int& slot) {
    measure = slot = -1;
    for (int row = 0; row < EditorRows; ++row) {
        const UIRect lane = EditorLane(row);
        const UIRect hit{ std::max(lane.L - 8,EditorGridArea().L+EditorLabelWidth()), lane.T - 4, std::min(lane.R + 8,EditorGridArea().R), lane.B + 4 };
        const int m = EditorRowMeasure(row);
        if (!hit.Contains(x, y) || m < 0 || EditorFoldAt(m)) { continue; }
        measure = m;
        slot = std::clamp((int)std::lround((x - lane.L) / (lane.R - lane.L) * EditorDivOf(measure)), 0, EditorDivOf(measure) - 1);
        return true;
    }
    return false;
}

void Game::EditorProc() {
    auto Back = [&] { NowScene = EditorReturnScene; };
    if (Editor.WaveAnalyzing) {
        if (!Editor.WaveTask.valid() || Editor.WaveTask.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { return; }
        Editor.WaveAnalyzing = false;
        try { EditorApplyWaveAlign(Editor.WaveTask.get(), Editor.WaveToleranceMs); }
        catch (const std::exception& e) { EditorMessage(std::string("波形解析に失敗しました: ") + e.what()); }
        return;
    }
    EditorUpdatePreview();
#ifdef __ANDROID__
    if (Editor.PickerPending) {
        const auto picked = AndroidPollEditorPicker();
        if (!picked) { return; }
        Editor.PickerPending = false;
        PointerPrev.Left = PointerPrev.Right = true;
        if (!picked->empty()) {
            const fs::path path = U8ToPath(*picked);
            if (Editor.PickerKind == 1) { EditorOpen(path); }
            else if (EditorCourse() && (Editor.PickerKind == 4 || Editor.PickerKind == 5)) {
                EditorChange();
                tja::Document::SetValue(Editor.Doc.Header, Editor.PickerKind == 4 ? "WAVE" : "BGMOVIE", EditorWaveValue(path, Editor.Path.parent_path()));
            }
        }
        return;
    }
#endif

#ifndef __ANDROID__
    // ---- ファイル選択ダイアログ ----
    if (Editor.Picker) {
        if (!Editor.Picker->Done) { return; }
        const std::wstring result = Editor.Picker->Result;
        const int kind = Editor.PickerKind;
        Editor.Picker.reset();
        PointerPrev.Left = PointerPrev.Right = true;
        Input::GetInstance().ResyncKeys();
        if (kind == 1 && !result.empty()) { EditorOpen(fs::path(result)); }
        else if (kind == 2 && !result.empty()) {
            Editor.NewPath = fs::path(result);
            EditorPick(3, false, true, Editor.NewPath.parent_path(), L"");     // 続けて音源を選ぶ (キャンセルで音源なし)
        }
        else if (kind == 3) { EditorNewChart(Editor.NewPath, result.empty() ? fs::path() : fs::path(result)); }
        else if ((kind == 4 || kind == 5) && !result.empty() && EditorCourse() != nullptr) {
            EditorChange();
            tja::Document::SetValue(Editor.Doc.Header, kind == 4 ? "WAVE" : "BGMOVIE", EditorWaveValue(fs::path(result), Editor.Path.parent_path()));
        }
        return;
    }
#endif
    // ---- 自動推定の解析中 ----
    if (Editor.Estimating) {
        if (Editor.EstimateTask.valid() && Editor.EstimateTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try { Editor.Estimate = Editor.EstimateTask.get(); }
            catch (const std::exception& e) { Editor.Estimating = false; EditorMessage(std::string("推定に失敗しました: ") + e.what()); return; }
            Editor.Estimating = false;
            if (Editor.Estimate.Ok) { Editor.Panel = EditorState::PanelType::Estimate; }
            else { EditorMessage("BPM を推定できませんでした (曲が短いか、拍がはっきりしません)"); }
            PointerPrev.Left = PointerPrev.Right = true;
        }
        return;
    }
    const PointerState pt = ReadPointer();

    if (Editor.FormOpen) {
#ifdef __ANDROID__
        const auto result = AndroidPollString("PollEditorFields");
        if (!result) { return; }
        const auto values = json::parse(*result, nullptr, false);
        if (values.is_array() && values.size() == Editor.FormFields.size() && std::ranges::all_of(values, [](const auto& value) { return value.is_string(); })) {
            EditorFinishFields(true, values.get<std::vector<std::string>>());
        } else { EditorFinishFields(false); }
#else
        int action = 0;
        if (pt.LeftPressed) {
            for (int i = 0; i < (int)Editor.FormFields.size(); ++i) {
                const auto box = EditorFieldRect(i);
                if (!box.Contains(pt.X, pt.Y)) { continue; }
                EditorFocusField(i);
                if (!Editor.FormFields[i].Choices.empty()) { EditorCycleField(i, pt.X < (box.L + box.R) * 0.5f ? -1 : 1); }
            }
            if (EditorFieldsButton(0).Contains(pt.X, pt.Y)) { action = 1; }
            if (EditorFieldsButton(1).Contains(pt.X, pt.Y)) { action = 2; }
        }
        const int handle = Editor.FormFields[Editor.FormActive].Handle;
        if (handle != -1 && sys::TextInputState(handle) != 0) { action = sys::TextInputState(handle); }
        Input::GetInstance().HitKeyProcess(VK_TAB, KeyState::Down, [&] { EditorFocusField(Editor.FormActive + ((__gotkeyboardinput[VK_SHIFT] & 1) ? -1 : 1)); });
        Input::GetInstance().HitKeyProcess(VK_RETURN, KeyState::Down, [&] { action = 1; });
        Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] { action = 2; });
        if (Editor.FormFields[Editor.FormActive].Handle == -1) {
            Input::GetInstance().HitKeyProcess(VK_LEFT, KeyState::Down, [&] { EditorCycleField(Editor.FormActive, -1); });
            Input::GetInstance().HitKeyProcess(VK_RIGHT, KeyState::Down, [&] { EditorCycleField(Editor.FormActive, 1); });
        }
        if (action) { EditorFinishFields(action == 1); }
#endif
        return;
    }
#ifndef __ANDROID__
    // ---- 文字入力中 ----
    if (Editor.Editing) {
        int state = sys::TextInputState(Editor.EditHandle);
        // ボタン: 決定 = Enter、キャンセル = Esc
        if (state == 0 && pt.LeftPressed) {
            if (EditorEditButton(0).Contains(pt.X, pt.Y)) { state = 1; }
            else if (EditorEditButton(1).Contains(pt.X, pt.Y)) { state = 2; }
        }
        if (state != 0) {
            if (state == 1) {
                const std::string buf = sys::TextInputString(Editor.EditHandle);
                auto done = std::move(Editor.OnEdit);
                sys::EndTextInput(Editor.EditHandle);
                Editor.EditHandle = 0;
                Editor.Editing = false;
                Input::GetInstance().ResyncKeys();
                if (done) { done(buf); }
                return;
            }
            sys::EndTextInput(Editor.EditHandle);
            Editor.EditHandle = 0;
            Editor.Editing = false;
            Editor.OnEdit = nullptr;
            Input::GetInstance().ResyncKeys();
        }
        return;
    }
#endif

    // ---- 開始画面 ----
    if (!Editor.Open) {
        auto Start = [&](int i) {
            Skin.Base->Title.SE.Don.Play();
#ifndef __ANDROID__
            if (i == 0) { EditorPick(2, true, false, EditorDefaultFolder(), L"新しい譜面.tja"); }   // 新しい譜面.tja
            else if (i == 1) { EditorPick(1, false, false, EditorDefaultFolder(), L""); }
            else { Back(); }
#else
            if (i == 0) {
                // 新しい譜面: 名前を入れると「最初の譜面フォルダ\名前\名前.tja」を作る (音源はヘッダーの WAVE か SONGURL の取得で)
                std::string name(strtrim(GetInputText("新しい譜面の名前")));
                for (char& c : name) { if (std::string_view("\\/:*?\"<>|").find(c) != std::string_view::npos) { c = '_'; } }
                if (name.empty()) { return; }
                const fs::path dir = EditorDefaultFolder() / U8ToPath(name);
                std::error_code ec;
                fs::create_directories(dir, ec);
                const fs::path path = dir / U8ToPath(name + ".tja");
                if (fs::exists(path, ec)) { EditorOpen(path); }
                else { EditorNewChart(path, fs::path()); }
            }
            else if (i == 1) { EditorPick(1, false, false, EditorDefaultFolder(), L""); }
            else { Back(); }
#endif
        };
        if (Editor.Browsing) { EditorBrowseProc(pt); return; }
        for (int i = 0; i < 3; ++i) {
            if (pt.LeftPressed && EditorStartButton(i).Contains(pt.X, pt.Y)) { Start(i); return; }
        }
#ifndef __ANDROID__
        Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, Back);
#else
        Touch.Process(TouchType::Other, Back, Skin.Base->ConfigMenu.Image.Back);
#endif
        return;
    }

    auto* course = EditorCourse();
    if (course == nullptr) { EditorClose(); return; }
#ifndef __ANDROID__
    const bool ctrl = (__gotkeyboardinput[VK_CONTROL] & 1) != 0;
#else
    const bool ctrl = false;
#endif

    // ---- パネル ----
    if (Editor.Panel != EditorState::PanelType::None) {
        const UIRect p = EditorPanelRect();
        if (pt.LeftPressed && !p.Contains(pt.X, pt.Y)) { Editor.Panel = EditorState::PanelType::None; return; }
        if (Editor.Panel == EditorState::PanelType::Estimate) {
            if (pt.LeftPressed && EditorHeaderButton(0).Contains(pt.X, pt.Y)) {
                EditorApplyEstimate();
                Editor.Panel = EditorState::PanelType::None;
            }
            else if (pt.LeftPressed && EditorHeaderButton(1).Contains(pt.X, pt.Y)) {
                Editor.Panel = EditorState::PanelType::None;
            }
#ifndef __ANDROID__
            Input::GetInstance().HitKeyProcess(VK_RETURN, KeyState::Down, [&] { EditorApplyEstimate(); Editor.Panel = EditorState::PanelType::None; });
            Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] { Editor.Panel = EditorState::PanelType::None; });
#endif
            return;
        }
        if (Editor.Panel == EditorState::PanelType::Grid) {
            const auto choices = EditorGridPanelChoices();
            for (int i = 0; i < (int)choices.size() + 3; ++i) {
                if (!EditorPanelRow(i, 4).Contains(pt.X, pt.Y)) { continue; }
                if (i > (int)choices.size()) {
                    if (pt.LeftPressed) {
                        const int last = ((int)EditorGridChoices().size() - 1) / EditorGridsPerPage;
                        Editor.GridPage = std::clamp(EditorGridPageIndex() + (i == (int)choices.size() + 1 ? -1 : 1), 0, last);
                    }
                    return;
                }
                if (i == (int)choices.size()) {
                    if (pt.LeftPressed) {
                        Editor.Panel = EditorState::PanelType::None;
                        EditorBeginFields("任意のグリッド分割数", { { "分割数 (1～4096)", std::to_string(Editor.Division) } }, [this](const auto& values) {
                            int division = 0; const auto value = tja::Trim(values[0]);
                            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), division);
                            if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size()) { EditorMessage("分割数は整数で入力してください"); return; }
                            EditorAddGrid(division);
                        });
                    }
                    return;
                }
                const int g = choices[i];
                if (pt.LeftPressed && !EditorDefaultGrid(g) && EditorGridDeleteButton(i).Contains(pt.X, pt.Y)) {
                    EditorRemoveGrid(g); return;
                }
                if (pt.LeftPressed) {
                    EditorSelectDivision(g);
                    Editor.Panel = EditorState::PanelType::None;
                    return;
                }
                if (pt.RightPressed) {
                    auto grids = EditorGridList();
                    if (auto it = std::ranges::find(grids, g); it != grids.end()) {
                        if (grids.size() <= 1) { EditorMessage("よく使う分割は 1 つ以上必要です"); return; }
                        grids.erase(it);
                    }
                    else { grids.push_back(g); }
                    std::ranges::sort(grids);
                    Config.EditorGrids = grids;
                    Config.Write();
                    return;
                }
            }
        }
        if (Editor.Panel == EditorState::PanelType::Gimmick) {
            const auto& list = EditorGimmicks();
            for (int i = 0; i < (int)list.size(); ++i) {
                if (pt.LeftPressed && EditorGimmickLayout().Items[i].Contains(pt.X, pt.Y)) {
                    Editor.Panel = EditorState::PanelType::None;
                    if (list[i].Default[0] == '\0') { EditorInsertGimmick(i, ""); }
                    else {
                        EditorEditGimmickField(i);
                    }
                    return;
                }
            }
        }
        else if (Editor.Panel == EditorState::PanelType::Commands) {
            const auto& list = EditorCommandTemplates();
            for (int i = 0; i < (int)list.size(); ++i) {
                if (pt.LeftPressed && EditorCommandLayout().Items[i].Contains(pt.X, pt.Y)) {
                    Editor.Panel = EditorState::PanelType::None;
                    const std::string text = list[i].second;
                    if (text == "uniform-scroll") { EditorBeginUniformScroll(); }
                    else if (text.find('>') != std::string::npos) { EditorBeginGradient(); }
                    else if (text.find(' ') == std::string::npos && text != "#") { EditorInsertCommand(text); }
                    else { EditorEditCommandValues(text, [this](const std::string& s) { EditorInsertCommand(s); }); }
                    return;
                }
            }
        }
        else if (Editor.Panel == EditorState::PanelType::Header) {
            const auto& fields = EditorHeaderFields();
            for (int i = 0; i < (int)fields.size(); ++i) {
                const auto f = fields[i];
                if (pt.LeftPressed && (std::string_view(f.Key) == "TITLE" || std::string_view(f.Key) == "SUBTITLE")
                    && EditorHeaderVisibilityButton(i).Contains(pt.X, pt.Y)) {
                    EditorToggleHeaderVisibility(f.Key);
                    return;
                }
                if (pt.LeftPressed && std::string(f.Key) == "WAVE" && EditorWaveButton(i).Contains(pt.X, pt.Y)) {
                    EditorPick(4, false, true, Editor.Path.parent_path(), L"");
                    return;
                }
                if (pt.LeftPressed && std::string(f.Key) == "BGMOVIE" && EditorWaveButton(i).Contains(pt.X, pt.Y)) {
                    EditorPick(5, false, false, Editor.Path.parent_path(), L"");
                    return;
                }
                if (pt.LeftPressed && std::string(f.Key) == "SONGURL" && EditorWaveButton(i).Contains(pt.X, pt.Y)) {
                    EditorFetchSong();
                    return;
                }
                if (pt.LeftPressed && std::string(f.Key) == "MOVIEURL" && EditorWaveButton(i).Contains(pt.X, pt.Y)) {
                    EditorFetchMovie();
                    return;
                }
                if (pt.LeftPressed && EditorPanelRow(i, 2).Contains(pt.X, pt.Y)) {
                    auto& lines = f.CourseField ? course->Header : Editor.Doc.Header;
                    std::string initial = tja::Document::GetValue(lines, f.Key);
                    if (initial.empty() && (std::string_view(f.Key) == "SONGVOL" || std::string_view(f.Key) == "SEVOL")) { initial = "100"; }
                    EditorBeginEdit(f.Key, initial, [this, f](const std::string& s) {
                        EditorChange();
                        auto& target = f.CourseField ? EditorCourse()->Header : Editor.Doc.Header;
                        std::string value(strtrim(s));
                        // WAVE に絶対パスを書いた時も、譜面からの相対パスにする (ゲームのフォルダの外なら複製する)
                        if (std::string(f.Key) == "WAVE" && !value.empty()) {
                            std::error_code ec;
                            const fs::path p = U8ToPath(value);
                            if (p.is_absolute() && fs::is_regular_file(p, ec)) { value = EditorWaveValue(p, Editor.Path.parent_path()); }
                        }
                        tja::Document::SetValue(target, f.Key, value);
                    });
                    return;
                }
            }
            if (pt.LeftPressed && EditorHeaderButton(0).Contains(pt.X, pt.Y)) {
                // まだ無いコースを追加する
                static const char* names[] = { "Edit", "Oni", "Hard", "Normal", "Easy" };
                for (const char* name : names) {
                    bool used = false;
                    for (int c = 0; c < (int)Editor.Doc.Courses.size(); ++c) {
                        used = used || CourseIndexFromName(Editor.Doc.CourseName(c)) == CourseIndexFromName(name);
                    }
                    if (used) { continue; }
                    EditorChange();
                    tja::Course added;
                    added.Header = { std::string("COURSE:") + name, "LEVEL:1", "BALLOON:" };
                    added.Measures.resize(std::max<size_t>(course->Measures.size(), 1));
                    Editor.Doc.Courses.push_back(std::move(added));
                    Editor.Course = (int)Editor.Doc.Courses.size() - 1;
                    EditorClamp();
                    EditorMessage(std::string("コースを追加しました: ") + name);
                    return;
                }
                EditorMessage("追加できるコースがありません");
                return;
            }
            if (pt.LeftPressed && EditorHeaderButton(1).Contains(pt.X, pt.Y)) {
                if (Editor.Doc.Courses.size() <= 1) { EditorMessage("最後のコースは削除できません"); return; }
                EditorChange();
                Editor.Doc.Courses.erase(Editor.Doc.Courses.begin() + Editor.Course);
                EditorClamp();
                EditorMessage("コースを削除しました (Ctrl+Z で元に戻せます)");
                return;
            }
        }
#ifndef __ANDROID__
        Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] { Editor.Panel = EditorState::PanelType::None; });
#endif
        return;
    }

    // ---- ボタン ----
    if (pt.LeftPressed) {
        for (auto& b : EditorButtons()) {
            if (b.Rect.Contains(pt.X, pt.Y)) {
                if(b.Icon!=EditorIcon::Save)Skin.Base->Title.SE.Ka.Play();
                b.Action();
                return;
            }
        }
    }

    // ---- 命令のラベル: クリックで編集、右クリックで削除 ----
    for (const auto& hit : EditorCommandRects()) {
        if (!hit.Rect.Contains(pt.X, pt.Y)) { continue; }
        if (pt.RightPressed) {
            auto& cmds = course->Measures[hit.Measure].Commands;
            const bool measureCommand = tja::StartsWithKey(cmds[hit.Index].Text, "#MEASURE");
            const std::vector<EditorFrac> before = measureCommand ? EditorMeasureFracs() : std::vector<EditorFrac>{};
            EditorChange();
            EditorMessage("命令を削除しました: " + cmds[hit.Index].Text);
            cmds.erase(cmds.begin() + hit.Index);
            if (measureCommand) { EditorKeepNotePositions(before); }
            return;
        }
        if (pt.LeftPressed) {
            const int m = hit.Measure, i = hit.Index;
            EditorEditCommandValues(course->Measures[m].Commands[i].Text, [this, m, i](const std::string& s) {
                auto& cmds = EditorCourse()->Measures[m].Commands;
                if (i >= (int)cmds.size()) { return; }
                const std::string t(strtrim(s));
                const bool measureCommand = tja::StartsWithKey(cmds[i].Text, "#MEASURE") || tja::StartsWithKey(t, "#MEASURE");
                const std::vector<EditorFrac> before = measureCommand ? EditorMeasureFracs() : std::vector<EditorFrac>{};
                EditorChange();
                if (t.empty()) { cmds.erase(cmds.begin() + i); }
                else { cmds[i].Text = t; }
                if (measureCommand) { EditorKeepNotePositions(before); }
            });
            return;
        }
    }

    // ---- 音符: 左クリックで置く (同じ音符ならもう一度で消す)、右クリックで消す ----
    int hm = -1, hs = -1;
#ifndef __ANDROID__
    const bool shift = (__gotkeyboardinput[VK_SHIFT] & 1) != 0;
#else
    const bool shift = false;
#endif
    if (EditorHitLane(pt.X, pt.Y, hm, hs) && pt.LeftPressed && (shift || Editor.SelectMode)) {
        // 1 回目の Shift+クリックで選択の始まり、2 回目以降で終わりを決める
        if (!Editor.Selecting) {
            Editor.CursorMeasure = hm;
            Editor.CursorSlot = hs;
            EditorBeginSelection();
        }
        Editor.CursorMeasure = hm;
        Editor.CursorSlot = hs;
        return;
    }
    // 折りたたんだ行をクリック: 開く
    if (pt.LeftPressed) {
        for (int row = 0; row < EditorRows; ++row) {
            const int m = EditorRowMeasure(row);
            const auto* f = m >= 0 ? EditorFoldAt(m) : nullptr;
            const UIRect lane = EditorLane(row);
            const auto* opened = m >= 0 ? EditorOpenFoldAt(m) : nullptr;
            if ((f || (opened && opened->first == m)) && EditorFoldDeleteButton(row).Contains(pt.X, pt.Y)) {
                EditorRemoveFoldRegistration(m); return;
            }
            if (f && UIRect{ EditorGridArea().L, lane.T - 4, lane.R + 8, lane.B + 4 }.Contains(pt.X, pt.Y)) {
                const auto range = *f;
                EditorToggleFold(m);
                EditorMessage(std::to_string(range.second - range.first + 1) + " 小節を開きました");
                return;
            }
            if (const auto* registered = EditorOpenFoldAt(m); registered && registered->first == m && EditorFoldButton(row).Contains(pt.X, pt.Y)) {
                EditorToggleFold(m);
                return;
            }
            // 小節番号の列: 横にスクロールしていても (レーンが左にずれていても) 列の幅全体で押せる
            if (m >= 0 && UIRect{ EditorGridArea().L, lane.T, EditorGridArea().L + EditorLabelWidth(), lane.B }.Contains(pt.X, pt.Y)) {
                Editor.CursorMeasure = m;
                Editor.CursorSlot = 0;
                Editor.Selecting = false;
                EditorShowCursorPage();
                if (Editor.Preview) { EditorStopPreview(); EditorTogglePreview(); }
                return;
            }
        }
    }
    if (EditorHitLane(pt.X, pt.Y, hm, hs)) {
        if (pt.LeftPressed || pt.RightPressed) {
            Editor.Selecting = false;
            Editor.CursorMeasure = hm;
            Editor.CursorSlot = hs;
            auto& measure = course->Measures[hm];
            // マウスで消すのは右クリックだけ。グリッドに乗っていない音符 (別の分割数で置いた音符など) も、マウスの近くにあれば消す
            if (pt.RightPressed && tja::Document::NoteAt(measure, hs, EditorDivOf(Editor.CursorMeasure)) == nullptr) {
                const UIRect lane = EditorLane(EditorMeasureRow(hm));
                const float radius = (lane.B - lane.T) * 0.45f;
                auto nearest = measure.Notes.end();
                float best = radius;
                for (auto it = measure.Notes.begin(); it != measure.Notes.end(); ++it) {
                    const float d = std::abs(EditorX(lane, (double)it->Num / std::max<int64_t>(it->Den, 1)) - (float)pt.X);
                    if (d <= best) { best = d; nearest = it; }
                }
                if (nearest != measure.Notes.end()) {
                    EditorChange();
                    measure.Notes.erase(nearest);
                    return;
                }
            }
            const auto* existing = tja::Document::NoteAt(measure, hs, EditorDivOf(Editor.CursorMeasure));
            const bool insertedBalloon=!existing || (existing->Type!='7' && existing->Type!='9');
            // 左クリックは置くだけ (同じ種類の音符があっても消さない。消しゴム (0) を選んでいる時は何もしない)
            const char type = pt.RightPressed ? '0' : Editor.Palette;
#ifndef __ANDROID__
            if (!pt.RightPressed && (type == '0' || (existing != nullptr && existing->Type == type))) { return; }
#endif
            if (!(type == '0' && existing == nullptr)) {
                EditorChange();
                char placed = type;
#ifdef __ANDROID__
                if (existing != nullptr && existing->Type == type) { placed = '0'; }
#endif
                tja::Document::SetNote(measure, hs, EditorDivOf(Editor.CursorMeasure), placed);
                if(placed=='7'||placed=='9') EditorBalloonPlaced(hm,hs,EditorDivOf(hm),insertedBalloon);
                if (placed != '0') { (placed == '2' || placed == '4' ? Skin.Base->Playing.SE.Ka : Skin.Base->Playing.SE.Don).Play(); }
            }
            return;
        }
    }
    if (pt.Wheel != 0) {
        if(shift) { EditorScrollHorizontal(-pt.Wheel); } else { EditorScrollRows(-pt.Wheel); }
#ifdef __ANDROID__
        // スクロールしたらカーソルも見えている小節へ
        if (EditorMeasureRow(Editor.CursorMeasure) < 0) { Editor.CursorMeasure = Editor.Top; Editor.CursorSlot = 0; }
#endif
    }

    // ---- キーボード・タッチ太鼓 ----
    auto PlaceAtCursor = [&](char type) {
        auto& measure = course->Measures[Editor.CursorMeasure];
        const auto* prior=tja::Document::NoteAt(measure,Editor.CursorSlot,EditorDivOf(Editor.CursorMeasure));
        const bool insertedBalloon=!prior || (prior->Type!='7'&&prior->Type!='9');
        if (type == '0' && tja::Document::NoteAt(measure, Editor.CursorSlot, EditorDivOf(Editor.CursorMeasure)) == nullptr) { return; }
        EditorChange();
        tja::Document::SetNote(measure, Editor.CursorSlot, EditorDivOf(Editor.CursorMeasure), type);
        if(type=='7'||type=='9') EditorBalloonPlaced(Editor.CursorMeasure,Editor.CursorSlot,EditorDivOf(Editor.CursorMeasure),insertedBalloon);
    };
    // 折りたたんだ範囲は 1 行として扱う: 入る時は折りたたんだ行 (範囲の始まり) に止まり、次の 1 回で抜ける
    auto StepMeasure = [&](int dir) {
        if (const auto* f = EditorFoldAt(Editor.CursorMeasure)) { Editor.CursorMeasure = dir > 0 ? f->second + 1 : f->first - 1; }
        else { Editor.CursorMeasure += dir; }
        Editor.CursorMeasure = std::clamp(Editor.CursorMeasure, 0, std::max(EditorMeasureCount() - 1, 0));
        if (const auto* g = EditorFoldAt(Editor.CursorMeasure)) { Editor.CursorMeasure = g->first; }
    };
    auto MoveSlot = [&](int dir) {
        if (EditorFoldAt(Editor.CursorMeasure)) {
            const int before = Editor.CursorMeasure;
            StepMeasure(dir);
            Editor.CursorSlot = dir < 0 && !EditorFoldAt(Editor.CursorMeasure) ? EditorDivOf(Editor.CursorMeasure) - 1 : 0;
            if (Editor.CursorMeasure == before) { Editor.CursorSlot = 0; }
            EditorClamp();
            return;
        }
        Editor.CursorSlot += dir;
        if (Editor.CursorSlot < 0) {
            if (Editor.CursorMeasure > 0) { StepMeasure(-1); Editor.CursorSlot = EditorFoldAt(Editor.CursorMeasure) ? 0 : EditorDivOf(Editor.CursorMeasure) - 1; }
            else { Editor.CursorSlot = 0; }
        }
        if (Editor.CursorSlot >= EditorDivOf(Editor.CursorMeasure)) {
            if (Editor.CursorMeasure + 1 < EditorMeasureCount()) { StepMeasure(1); Editor.CursorSlot = 0; }
            else { Editor.CursorSlot = EditorDivOf(Editor.CursorMeasure) - 1; }
        }
        EditorClamp();
    };
#ifdef __ANDROID__
    // レーンのタップを太鼓への打撃として二重処理しない。外部太鼓の操作は引き続き使える。
    TouchData editorInput;
    editorInput.ExternalPressed = Touch.ExternalPressed;
    editorInput.ProcessTaiko(TouchType::LeftKa, [&] { MoveSlot(-1); });
    editorInput.ProcessTaiko(TouchType::RightKa, [&] { MoveSlot(1); });
    editorInput.ProcessTaiko(TouchType::RightDon, [&] {
        if (Editor.SelectMode) { if (!Editor.Selecting) { EditorBeginSelection(); } return; }
        PlaceAtCursor(Editor.Palette);
        if (Editor.Palette != '0') { (Editor.Palette == '2' || Editor.Palette == '4' ? Skin.Base->Playing.SE.Ka : Skin.Base->Playing.SE.Don).Play(); }
        MoveSlot(1);
    });
    editorInput.ProcessTaiko(TouchType::LeftDon, [&] { if (Editor.SelectMode) { Editor.Selecting = false; return; } PlaceAtCursor('0'); });
#else
    if (ctrl) {
        Input::GetInstance().HitKeyProcess('S', KeyState::Down, [&] { EditorSave(); });
        Input::GetInstance().HitKeyProcess('Z', KeyState::Down, [&] { EditorUndo(false); });
        Input::GetInstance().HitKeyProcess('Y', KeyState::Down, [&] { EditorUndo(true); });
        Input::GetInstance().HitKeyProcess('C', KeyState::Down, [&] { EditorCopy(false); });
        Input::GetInstance().HitKeyProcess('X', KeyState::Down, [&] { if (EditorCopy(true)) { Editor.Selecting = false; } });
        Input::GetInstance().HitKeyProcess('V', KeyState::Down, [&] { EditorPaste(); });
        return;
    }
    // Shift+矢印で範囲を選ぶ (矢印だけなら選択を解除して移動)
    auto Select = [&] { if (shift) { EditorBeginSelection(); } else { Editor.Selecting = false; } };
    for (char c = '0'; c <= '9'; ++c) {
        Input::GetInstance().HitKeyProcess(c, KeyState::Down, [&] { Editor.Palette = c; });
        Input::GetInstance().HitKeyProcess(VK_NUMPAD0 + (c - '0'), KeyState::Down, [&] { Editor.Palette = c; });
    }
    Input::GetInstance().HitKeyProcess(VK_UP, KeyState::Down, [&] { Select(); StepMeasure(-1); EditorClamp(); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_DOWN, KeyState::Down, [&] { Select(); StepMeasure(1); EditorClamp(); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_LEFT, KeyState::Down, [&] { Select(); MoveSlot(-1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_RIGHT, KeyState::Down, [&] { Select(); MoveSlot(1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_RETURN, KeyState::Down, [&] { PlaceAtCursor(Editor.Palette); MoveSlot(1); });
    Input::GetInstance().HitKeyProcess(VK_DELETE, KeyState::Down, [&] { PlaceAtCursor('0'); });
    Input::GetInstance().HitKeyProcess(VK_F5, KeyState::Down, [&] { EditorTestPlay(); });
    Input::GetInstance().HitKeyProcess(VK_SPACE, KeyState::Down, [&] { EditorTogglePreview(); });
    Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] { EditorClose(); });
#endif
}
