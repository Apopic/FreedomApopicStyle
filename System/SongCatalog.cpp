#include "../Game.hpp"
#ifndef __ANDROID__
#include "cppunzip.hpp"
#endif

std::string Game::PathToU8(const fs::path& path) {
    auto u8 = path.u8string();
    return std::string(u8.begin(), u8.end());
}

fs::path Game::U8ToPath(const std::string& str) {
    return fs::path(std::u8string(str.begin(), str.end()));
}

bool Game::GenreHasCharts(const GenreData& genre, bool dan) {
    int8_t& cached = genre.HasChartCache[dan ? 1 : 0];
    if (cached < 0) {
        cached = 0;
        for (const auto& data : genre.Datas) {
            if (data->IsGenre() ? GenreHasCharts(*data->GetGenre(), dan) : data->GetChart()->IsDan == dan) {
                cached = 1;
                break;
            }
        }
    }
    return cached != 0;
}

double Game::DemoSongPlayBlankTime() const {
    return 0.25;
}

void Game::ResetDemoSong() {
    // 読み込み中の DeleteSoundMem は待機することがあるので、選曲操作では解放しない。
    if (DemoSong.Handle != -1 && CheckHandleASyncLoad(DemoSong.Handle) == FALSE) { DemoSong.Stop(); }
    DemoLoadFailed = false;
}

void Game::UpdateDemoSong(const ChartData& selected) {
    const std::string path = selected.SongPath.string();
    const int loading = DemoSong.Handle == -1 ? 0 : CheckHandleASyncLoad(DemoSong.Handle);
    if (loading > 0) { return; }
    if (loading < 0) { DemoSong.Handle = -1; DemoLoadFailed = true; }
    if (DemoSong.FilePath != path || (DemoSong.Handle == -1 && !DemoLoadFailed)) {
        DemoSong.Delete();
        const int previous = GetCreateSoundDataType();
        SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMNOPRESS);
        DemoSong.LoadAsync(path);
        SetCreateSoundDataType(previous);
        DemoLoadFailed = DemoSong.Handle == -1;
        return;
    }
    if (DemoSong.Handle != -1 && !DemoSong.IsPlay()) {
        DemoSong.FinishAsync();
        const double duration = (double)GetSoundTotalTime(DemoSong.Handle), preview = selected.DemoStart * 1000;
        DemoSong.SetCurrent(std::isfinite(preview) && preview >= 0 && preview < duration ? preview : 0);
        DemoSong.SetVolume(selected.SongVolume * Config.SongVolume / 100);
        DemoSong.Play(FALSE);
    }
}

void Game::ScanChartDirectory(std::vector<std::unique_ptr<BoxData>>& out, const fs::path& dirpath, const GenreData* genre, std::vector<ChartScanItem>& items) {
    std::vector<fs::directory_entry> entries;
    std::error_code ec;
    for (fs::directory_iterator it(dirpath, ec), end; !ec && it != end; it.increment(ec)) {
        entries.push_back(*it);
    }

    auto genreit = std::ranges::find_if(entries, [](const fs::directory_entry& e) { return e.path().filename() == "genre.ini"; });
    if (genreit != entries.end()) {
        auto ownedGenre = std::make_unique<GenreData>(genreit->path());
        auto box = std::make_unique<BoxData>(ownedGenre.get());
        ownedGenre.release();
        box->SetBoxColor();
        GenreData* newgenre = box->GetGenre();
        out.push_back(std::move(box));
        ScanChartEntries(newgenre->Datas, entries, newgenre, items);
        return;
    }
    ScanChartEntries(out, entries, genre, items);
}

void Game::ScanChartEntries(std::vector<std::unique_ptr<BoxData>>& out, const std::vector<fs::directory_entry>& entries, const GenreData* genre, std::vector<ChartScanItem>& items) {
    for (const auto& entry : entries) {
        std::error_code ec;
        const auto extension = ToLower(PathToU8(entry.path().extension()));
        if (extension == ".tja" || extension == ".osu") {
            ChartScanItem item;
            auto ownedChart = std::make_unique<ChartData>();
            item.Chart = ownedChart.get();
            item.Chart->ChartPath = entry.path();
            item.FolderGenre = genre != nullptr ? genre->Name : std::string();
            // directory_entry は列挙時の属性をキャッシュしているので追加のファイルアクセスは発生しない
            item.Size = entry.file_size(ec);
            item.WriteTime = entry.last_write_time(ec).time_since_epoch().count();
            auto box = std::make_unique<BoxData>(item.Chart);
            ownedChart.release();
            if (genre != nullptr) {
                box->SetBoxColor(*genre);
            }
            out.push_back(std::move(box));
            items.push_back(item);
        }
        if (entry.is_directory(ec)) {
            ScanChartDirectory(out, entry.path(), genre, items);
        }
    }
}

fs::path Game::ResolveSongDirectory(const std::string& dir) {
    const fs::path path = U8ToPath(dir);
    fs::path resolved = (path.is_absolute() ? path : GetExecutablePath() / path).lexically_normal();
    if (!resolved.has_filename() && resolved.has_relative_path()) {
        resolved = resolved.parent_path();      // 末尾の区切り文字を取る ("Songs\" と "Songs" を同じにする)
    }
    return resolved;
}

std::string Game::ToRelativeSongDirectory(const fs::path& absolute) {
    std::error_code ec;
    const fs::path rel = fs::relative(absolute, GetExecutablePath(), ec);
    if (ec || rel.empty() || rel.is_absolute()) { return {}; }
    const std::string str = PathToU8(rel);
    return str == "." ? std::string(".") : str;
}

std::string Game::CatalogBoxKey(const BoxData* box) {
    if (!box) { return {}; }
    return box->IsGenre() ? "g:" + box->GetGenre()->FilePath : "c:" + catalog::Key(box->GetChart()->ChartPath);
}

void Game::EnumChart(const std::vector<std::string>& dir) {
    const auto selected = BoxDataIndex < BoxDatas.size() ? CatalogBoxKey(BoxDatas[BoxDataIndex]) : std::string();
    std::unordered_set<std::string> open;
    auto remember = [&](const auto& boxes, auto&& self) -> void {
        for (const auto& box : boxes) { if (box->IsGenre()) { const auto* g = box->GetGenre(); if (g->IsOpen) { open.insert(g->FilePath); } self(g->Datas, self); } }
    };
    remember(TempBoxDatas, remember);
    ResetDemoSong();
    DemoSongPlayBlank.Reset();
    BoxDatas.clear();
    TempBoxDatas.clear();

    std::vector<ChartScanItem> items;
    for (const auto& d : dir) {
        const fs::path dirpath = ResolveSongDirectory(d);
        std::error_code ec;
        if (!fs::is_directory(dirpath, ec)) { continue; }
        ScanChartDirectory(TempBoxDatas, dirpath, nullptr, items);
    }
    ++ChartWatchGeneration;
    ChartWatchBaseline.Entries.clear();
    for (const auto& item : items) { ChartWatchBaseline.Entries[catalog::Key(item.Chart->ChartPath)] = { item.Size, item.WriteTime }; }
    ChartWatchCandidate.reset();
    ChartReloadPending = false;

    // キャッシュ読み込み
    const fs::path cachepath = GetExecutablePath() / "chartcache.bin";
    json cache;
    {
        std::ifstream ifs(cachepath, std::ios::binary);
        if (ifs.is_open()) {
            std::vector<uint8_t> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            cache = json::from_msgpack(buf, true, false);
        }
        if (!cache.is_object() || cache.value("version", 0) != ChartCacheVersion || !cache.contains("charts") || !cache["charts"].is_object()) {
            cache = { { "version", ChartCacheVersion }, { "charts", json::object() } };
        }
    }
    const json& cached = cache["charts"];

    // State: 0 = キャッシュ利用, 1 = パース成功, 2 = パース失敗
    std::vector<std::string> keys(items.size());
    std::vector<char> state(items.size(), 0);
    std::vector<size_t> needparse;
    for (size_t i = 0; i < items.size(); ++i) {
        keys[i] = PathToU8(items[i].Chart->ChartPath);
        auto it = cached.find(keys[i]);
        if (it != cached.end() && it->is_object()
            && it->value("size", (uintmax_t)0) == items[i].Size
            && it->value("time", (int64_t)0) == items[i].WriteTime
            && it->contains("data") && items[i].Chart->FromCache((*it)["data"])) {
            continue;
        }
        needparse.push_back(i);
    }

    // ワーカーの例外をメインスレッドへ戻す (並列アルゴリズムから直接投げるとterminateする)。
    std::mutex parseErrorMutex;
    std::exception_ptr parseError;
#ifndef __ANDROID__
    std::for_each(std::execution::par, needparse.begin(), needparse.end(), [&](size_t i) {
#else
    std::for_each(needparse.begin(), needparse.end(), [&](size_t i) {      // NDK の libc++ には並列アルゴリズムが無い
#endif
        try {
        ChartData* chart = items[i].Chart;
        fs::path path = chart->ChartPath;
        *chart = ChartData();
        if (chart->Load(path)) {
            state[i] = 1;
        }
        else {
            chart->ChartPath = path;
            state[i] = 2;
        }
        } catch(...) { std::lock_guard lock(parseErrorMutex); if(!parseError) parseError=std::current_exception(); }
        });
    if(parseError) std::rethrow_exception(parseError);
    for (auto& item : items) {
        item.Chart->FolderGenre = item.FolderGenre;
    }

    // 今回見つかった譜面だけでキャッシュを作り直す (削除された譜面のエントリは消える)
    if (!needparse.empty() || cached.size() != items.size()) {
        json newcharts = json::object();
        for (size_t i = 0; i < items.size(); ++i) {
            if (state[i] == 0) {
                newcharts[keys[i]] = cached.at(keys[i]);
            }
            else if (state[i] == 1) {
                newcharts[keys[i]] = { { "size", items[i].Size }, { "time", items[i].WriteTime }, { "data", items[i].Chart->ToCache() } };
            }
            // 読み込みに失敗した譜面はキャッシュせず、次回も読み直す
        }
        cache["charts"] = std::move(newcharts);
        std::vector<uint8_t> buf = json::to_msgpack(cache);
        std::ofstream ofs(cachepath, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    }

    if (std::ranges::count(state, 2) > 0) {
        std::string message = "次の譜面の読み込みに失敗しました\n";
        size_t shown = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            if (state[i] != 2) { continue; }
            if (++shown > 10) { message += "..."; break; }
            message += items[i].Chart->ChartPath.string() + "\n";
        }
#ifndef __ANDROID__
        MessageBox(NULL, message.c_str(), TEXT("エラー"), MB_ICONERROR);
#else
        ShowToast(message);
#endif
    }

    auto restore = [&](auto& boxes, auto&& self) -> void {
        for (auto& box : boxes) { if (box->IsGenre()) { auto* g = box->GetGenre(); g->IsOpen = open.contains(g->FilePath); self(g->Datas, self); } }
    };
    restore(TempBoxDatas, restore);
    BoxDatasUpdate();
    BoxDataIndex = BoxDatas.empty() ? 0 : std::clamp<int>(BoxDataIndex, 0, (int)BoxDatas.size() - 1);
    const auto found = std::ranges::find_if(BoxDatas, [&](const auto* box) { return CatalogBoxKey(box) == selected; });
    if (found != BoxDatas.end()) { BoxDataIndex = (int)(found - BoxDatas.begin()); }
    else { IsCourseSelect = false; }
    if (IsCourseSelect && !BoxDatas.empty() && !BoxDatas[BoxDataIndex]->IsGenre() &&
        (CourseIndex >= (size_t)CourseType::Count || !BoxDatas[BoxDataIndex]->GetChart()->Courses[CourseIndex].IsPlayable)) { IsCourseSelect = false; }
}

void Game::ChartWatchUpdate() {
    const bool safe = NowScene == Scene::SongSelect || NowScene == Scene::DanSelect || NowScene == Scene::Title || NowScene == Scene::ModeSelect;
    if (!safe) { return; }
    if (ChartWatchTask.valid() && ChartWatchTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto snapshot = ChartWatchTask.get();
            if (ChartWatchTaskGeneration == ChartWatchGeneration && snapshot.Complete) {
                if (snapshot.Entries == ChartWatchBaseline.Entries) { ChartWatchCandidate.reset(); }
                else if (ChartWatchCandidate && ChartWatchCandidate->Entries == snapshot.Entries) { ChartReloadPending = true; }
                else { ChartWatchCandidate = std::move(snapshot); }
            }
        } catch (const std::exception&) { /* 一時的な読み込み失敗は次回の走査で再試行する */ }
    }
    if (ChartReloadPending && !FilterUI.Open) {
        EnumChart(Config.SongDirectories);
        ShowSelectMessage("譜面の変更を反映しました");
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (ChartWatchTask.valid() || now - ChartWatchLast < std::chrono::milliseconds(750)) { return; }
    ChartWatchLast = now;
    std::vector<fs::path> roots;
    for (const auto& directory : Config.SongDirectories) { roots.push_back(ResolveSongDirectory(directory)); }
    ChartWatchTaskGeneration = ChartWatchGeneration;
    ChartWatchTask = std::async(std::launch::async, [roots = std::move(roots)] { return catalog::Scan(roots); });
}

bool Game::SkinReloadSafe() const {
    return !Skin.IsLoading && NowScene == MemScene && !FilterUI.Open &&
        (NowScene == Scene::Title || NowScene == Scene::ModeSelect || NowScene == Scene::SongSelect || NowScene == Scene::DanSelect ||
            (NowScene == Scene::ConfigMenu && !ConfigUI.Editing && !ConfigUI.Capturing && !ConfigUI.RecommendOpen
#ifndef __ANDROID__
                && !ConfigUI.Picker
#endif
            ));
}

void Game::ReloadSkinAtSafePoint() {
    if (!SkinReloadSafe()) { return; }
    const std::string selected=Config.SkinName;
    Skin.Init(Skin.Root);
    if (std::ranges::find(Skin.Infos,selected,&_Skin::SkinInfo::Name)==Skin.Infos.end()) {
        Config.SkinName="Default";
        Config.Write(); // 削除された名前を次回起動に持ち越さない
    }
    if (!Skin.TryLoad(Config.SkinName,nullptr,Config.SkinName==selected)) { return; } // 新たな欠落・破損では現行を保持。Defaultへの退避は起動時と同じ読み方。
    FinishSkinReload();
    if (NowScene==Scene::ConfigMenu && ConfigUI.SkinOnEnter==selected) { ConfigUI.SkinOnEnter=Config.SkinName; }
    if (Config.SkinName!=selected) { ShowConfigMessage("指定スキンがなくなったためDefaultへ戻しました"); ShowSelectMessage("指定スキンがなくなったためDefaultへ戻しました"); }
}

void Game::FinishSkinReload() {
    ConfigSmall.Delete(); ConfigSmallSource=-2; ConfigSmallSourceSize=0;
    EditorFontData.Delete(); EditorFontSize=0; EditorPanelFontData.Delete(); EditorPanelFontSize=0;
    EditorNoteWidthFractions.clear();
    EditorNoteBoundsHandle = -2; EditorNoteBoundsPath.clear();
    ClearStrlenCache();
    SkinVoice={};
    PlayerCharAnim={};
    Skin.Base->Title.SE.Don.SetVolume(Config.SEVolume); Skin.Base->Title.SE.Ka.SetVolume(Config.SEVolume);
    Skin.Base->ModeSelect.SE.Don.SetVolume(Config.SEVolume); Skin.Base->ModeSelect.SE.Ka.SetVolume(Config.SEVolume);
    Skin.Base->SongSelect.SE.Don.SetVolume(Config.SEVolume); Skin.Base->SongSelect.SE.Ka.SetVolume(Config.SEVolume);
    Skin.Base->DanSelect.SE.Don.SetVolume(Config.SEVolume); Skin.Base->DanSelect.SE.Ka.SetVolume(Config.SEVolume);
}

void Game::SkinWatchUpdate() {
    if (!SkinReloadSafe()) { return; }
    if (SkinWatchTask.valid() && SkinWatchTask.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto snapshot=SkinWatchTask.get();
            if (SkinWatchRoot==Skin.Root) {
                const bool changed = SkinWatch.Accept(std::move(snapshot));
                // 初回の走査より前に指定スキンが消えていた場合も退避する。
                const auto selected = std::ranges::find(Skin.Infos, Config.SkinName, &_Skin::SkinInfo::Name);
                std::error_code ec;
                const bool exists = selected != Skin.Infos.end() && fs::is_regular_file(GetExecutablePath()/selected->Path, ec);
                const bool removed = selected==Skin.Infos.end() ? Config.SkinName!="Default" :
                    !exists && (!ec || ec==std::errc::no_such_file_or_directory || ec==std::errc::not_a_directory);
                if (changed || removed) { ReloadSkinAtSafePoint(); }
            }
        } catch (const std::exception&) { SkinWatch.Candidate.reset(); }
    }
    const auto now=std::chrono::steady_clock::now();
    if (SkinWatchTask.valid() || now-SkinWatchLast<std::chrono::milliseconds(750)) { return; }
    if (SkinWatchRoot!=Skin.Root) { SkinWatch={}; SkinWatchRoot=Skin.Root; }
    SkinWatchLast=now;
    try { SkinWatchTask=std::async(std::launch::async,[root=Skin.Root]{return skincatalog::Scan(root);}); }
    catch (const std::exception&) { /* ワーカーを作れない時は次の間隔で再試行する */ }
}

void Game::ShowSelectMessage(std::string message) {
    SelectMessage = std::move(message);
    SelectMessageUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
}

std::string Game::NormalizeSearchText(std::string_view in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ) {
        const unsigned char c = in[i];
        if (c == 0xE3 && i + 2 < in.size() && (unsigned char)in[i + 1] == 0x80 && (unsigned char)in[i + 2] == 0x80) {
            out += ' ';
            i += 3;
            continue;
        }
        if (c == 0xEF && i + 2 < in.size()) {
            const uint32_t cp = ((c & 0x0Fu) << 12) | (((unsigned char)in[i + 1] & 0x3Fu) << 6) | ((unsigned char)in[i + 2] & 0x3Fu);
            if (cp >= 0xFF01 && cp <= 0xFF5E) {
                out += (char)(cp - 0xFEE0);
                i += 3;
                continue;
            }
        }
        out += (char)c;
        ++i;
    }
    return ToLower(out);
}

std::optional<double> Game::ParseSearchNumber(std::string_view v) {
    double value = 0;
    auto result = fast_float::from_chars(v.data(), v.data() + v.size(), value);
    if (v.empty() || result.ec != std::errc() || result.ptr != v.data() + v.size()) {
        return std::nullopt;
    }
    return value;
}

int Game::ParseSearchCourse(std::string_view v) {
    static const std::pair<std::string_view, CourseType> names[] = {
        { "easy", CourseType::Easy }, { "e", CourseType::Easy }, { "かんたん", CourseType::Easy },
        { "normal", CourseType::Normal }, { "n", CourseType::Normal }, { "ふつう", CourseType::Normal },
        { "hard", CourseType::Hard }, { "h", CourseType::Hard }, { "むずかしい", CourseType::Hard },
        { "oni", CourseType::Oni }, { "o", CourseType::Oni }, { "おに", CourseType::Oni }, { "鬼", CourseType::Oni },
        { "edit", CourseType::Edit }, { "ura", CourseType::Edit }, { "u", CourseType::Edit }, { "裏", CourseType::Edit },
        { "tower", CourseType::Tower }, { "dan", CourseType::Dan }, { "段位", CourseType::Dan },
    };
    for (const auto& [name, course] : names) {
        if (v == name) { return (int)course; }
    }
    if (v.size() == 1 && v[0] >= '0' && v[0] < '0' + (int)CourseType::Count) {
        return v[0] - '0';
    }
    return -1;
}

auto Game::ParseSearch(std::string_view text) -> SearchQuery {
    SearchQuery q;
    const std::string normalized = NormalizeSearchText(text);

    // 空白区切り ("..." で空白を含む語)
    std::vector<std::string> tokens;
    std::string current;
    bool quoted = false;
    for (char ch : normalized) {
        if (ch == '"') { quoted = !quoted; continue; }
        if (ch == ' ' && !quoted) {
            if (!current.empty()) { tokens.push_back(std::move(current)); current.clear(); }
            continue;
        }
        current += ch;
    }
    if (!current.empty()) { tokens.push_back(std::move(current)); }

    for (const std::string& token : tokens) {
        if (token == "fav" || token == "favorite" || token == "★" || token == "お気に入り") {
            q.FavoriteOnly = true;
            continue;
        }
        // キー + 演算子 + 値
        size_t k = 0;
        while (k < token.size() && token[k] >= 'a' && token[k] <= 'z') { ++k; }
        std::string_view key(token.data(), k);
        std::string_view rest = std::string_view(token).substr(k);
        std::string_view op;
        if (rest.starts_with(">=") || rest.starts_with("<=")) { op = rest.substr(0, 2); }
        else if (!rest.empty() && (rest[0] == ':' || rest[0] == '=' || rest[0] == '>' || rest[0] == '<')) { op = rest.substr(0, 1); }
        const std::string_view value = rest.substr(op.size());
        const bool text_op = op == ":" || op == "=";

        bool handled = false;
        if (k > 0 && !op.empty() && !value.empty()) {
            if (text_op && (key == "t" || key == "title")) { q.Title.emplace_back(value); handled = true; }
            else if (text_op && (key == "s" || key == "sub" || key == "subtitle")) { q.Subtitle.emplace_back(value); handled = true; }
            else if (text_op && (key == "g" || key == "genre")) { q.Genre.emplace_back(value); handled = true; }
            else if (text_op && (key == "c" || key == "course" || key == "d" || key == "diff")) {
                q.Course = ParseSearchCourse(value);
                handled = q.Course >= 0;
            }
            else if (text_op && (key == "l" || key == "lv" || key == "level")) {
                if (const auto n = ParseSearchNumber(value)) {
                    q.Level.Active = true;
                    q.Level.Min = q.Level.Max = *n;
                    handled = true;
                }
            }
        }
        if (!handled) {
            q.Words.push_back(token);   // 条件として読めなければ単語として扱う
        }
    }
    return q;
}

bool Game::MatchChart(const ChartData& chart, const SearchQuery& q) const {
    if (q.FavoriteOnly && !IsFavorite(chart)) {
        return false;
    }
    const std::string title = ToLower(chart.Title);
    const std::string subtitle = ToLower(chart.Subtitle);
    const std::string genre = ToLower(chart.Genre) + "\n" + ToLower(chart.FolderGenre);
    auto has = [](const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; };

    if (!q.ExactText.empty() && NormalizeSearchText(chart.Title) != q.ExactText && NormalizeSearchText(chart.Subtitle) != q.ExactText) {
        return false;
    }
    if (!q.GenreFolders.empty() && std::ranges::find(q.GenreFolders, ToLower(chart.FolderGenre)) == q.GenreFolders.end()) {
        return false;
    }
    for (const auto& w : q.Title) { if (!has(title, w)) { return false; } }
    for (const auto& w : q.Subtitle) { if (!has(subtitle, w)) { return false; } }
    for (const auto& w : q.Genre) { if (!has(genre, w)) { return false; } }
    for (const auto& w : q.Words) {
        if (!has(title, w) && !has(subtitle, w) && !has(genre, w)) { return false; }
    }

    if (q.Course >= 0 || q.Level.Active) {
        bool found = false;
        for (int i = 0; i < (int)CourseType::Count && !found; ++i) {
            const CourseData& course = chart.Courses[i];
            if (!course.IsPlayable || (q.Course >= 0 && i != q.Course)) { continue; }
            if (q.Level.Active && !q.Level.Contains((double)course.Level)) { continue; }
            found = true;
        }
        if (!found) { return false; }
    }
    return true;
}

std::string Game::FavoriteKey(const fs::path& chartpath) {
    const fs::path rel = chartpath.lexically_relative(GetExecutablePath());
    const fs::path& key = (!rel.empty() && *rel.begin() != "..") ? rel : chartpath;
    const auto u8 = key.generic_u8string();
    return std::string(u8.begin(), u8.end());
}

void Game::LoadFavorites() {
    FavoritesLoaded = true;
    Favorites.clear();
    std::ifstream ifs(GetExecutablePath() / "favorites.json");
    if (!ifs.is_open()) {
        return;
    }
    const json data = json::parse(ifs, nullptr, false);
    if (data.is_object() && data.contains("charts") && data["charts"].is_array()) {
        for (const auto& item : data["charts"]) {
            if (item.is_string()) { Favorites.insert(item.get<std::string>()); }
        }
    }
}

void Game::SaveFavorites() {
    std::vector<std::string> list(Favorites.begin(), Favorites.end());
    std::ranges::sort(list);
    const json data = { { "version", 1 }, { "charts", list } };
    std::ofstream ofs(GetExecutablePath() / "favorites.json");
    ofs << data.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) << "\n";
}

bool Game::IsFavorite(const ChartData& chart) const {
    return Favorites.contains(FavoriteKey(chart.ChartPath));
}

void Game::CollectGenreCharts(const GenreData& genre, std::vector<const ChartData*>& out) const {
    for (const auto& data : genre.Datas) {
        if (data->IsGenre()) {
            CollectGenreCharts(*data->GetGenre(), out);
        }
        else if (data->GetChart()->IsDan == IsDanSelect()) {
            out.push_back(data->GetChart());
        }
    }
}

void Game::ToggleFavorite(BoxData* box) {
    if (box == nullptr) {
        return;
    }
    if (!FavoritesLoaded) { LoadFavorites(); }
    std::vector<const ChartData*> charts;
    std::string name;
    if (box->IsGenre()) {
        CollectGenreCharts(*box->GetGenre(), charts);
        name = "ジャンル「" + box->GetGenre()->Name + "」の " + std::to_string(charts.size()) + " 譜面";
    }
    else {
        charts.push_back(box->GetChart());
        name = box->GetChart()->Title;
    }
    if (charts.empty()) {
        ShowSelectMessage("登録できる譜面がありません");
        return;
    }
    const bool all = std::ranges::all_of(charts, [&](const ChartData* c) { return IsFavorite(*c); });
    for (const ChartData* chart : charts) {
        const std::string key = FavoriteKey(chart->ChartPath);
        if (all) { Favorites.erase(key); }
        else { Favorites.insert(key); }
    }
    SaveFavorites();
    ShowSelectMessage(all ? "お気に入りから外しました: " + name : "★ お気に入りに追加しました: " + name);
    // お気に入りのみ表示中に外した場合は一覧から消える
    if (all && Filter.FavoriteOnly) {
        BoxDatasUpdate();
        if (BoxDatas.empty()) {
            Filter.FavoriteOnly = false;
            BoxDatasUpdate();
        }
        BoxDataIndex = BoxDatas.empty() ? 0 : std::clamp<int>(BoxDataIndex, 0, (int)BoxDatas.size() - 1);
    }
}

void Game::ToggleFavoriteSelected() {
    if (!BoxDatas.empty()) {
        ToggleFavorite(BoxDatas[BoxDataIndex]);
    }
}

void Game::ToggleFavoriteOnly() {
    if (!FavoritesLoaded) { LoadFavorites(); }
    Filter.FavoriteOnly = !Filter.FavoriteOnly;
    BoxDatasUpdate();
    if (BoxDatas.empty()) {
        Filter.FavoriteOnly = false;
        BoxDatasUpdate();
        ShowSelectMessage("お気に入りの譜面がありません");
    }
    else {
        ShowSelectMessage(Filter.FavoriteOnly ? "★ お気に入りのみ表示" : "お気に入り以外も表示");
    }
    BoxDataIndex = 0;
}

void Game::BoxDatasUpdate() {
    // TempBoxDatas が空の時に古いポインタが残ると解放済みメモリを参照するため、先にクリアする
    BoxDatas.clear();
    if (TempBoxDatas.empty()) {
        return;
    }
    if (!FavoritesLoaded) { LoadFavorites(); }

    const SearchQuery query = BuildFilterQuery(Filter);
    const bool filtering = !query.Empty();

    auto recursive = [&](const std::vector<std::unique_ptr<BoxData>>& datas, auto& self) -> void {
        if (!filtering) {
            // 通常の一覧: ジャンル (開いていれば中身) → このフォルダの譜面
            for (const auto& data : datas) {
                if (!data->IsGenre() || !GenreHasCharts(*data->GetGenre(), IsDanSelect())) { continue; }
                BoxDatas.push_back(data.get());
                if (data->GetGenre()->IsOpen) {
                    self(data->GetGenre()->Datas, self);
                }
            }
        }
        for (const auto& data : datas) {
            if (data->IsGenre()) {
                if (filtering) { self(data->GetGenre()->Datas, self); }
                continue;
            }
            const ChartData& chart = *data->GetChart();
            if (chart.IsDan != IsDanSelect()) { continue; }
            if (filtering && !MatchChart(chart, query)) { continue; }
            BoxDatas.push_back(data.get());
        }
        };
    recursive(TempBoxDatas, recursive);
}

void Game::DrawSearchStatus(const FontData& font) {
    const float resx = (float)Skin.Info.Resolution.X, resy = (float)Skin.Info.Resolution.Y;
    const FontData& sfont = ConfigSmallFont();
    std::string status;
    if (!Filter.Empty()) {
        status = "絞り込み中: " + Filter.Summary() + "  (" + std::to_string(BoxDatas.size()) + "件)";
    }
    if (std::chrono::steady_clock::now() < SelectMessageUntil) {
        status += (status.empty() ? "" : "   ") + SelectMessage;
    }
    if (!status.empty()) {
        const FontData& f = font.Handle != -1 ? font : sfont;
        UIText(f, 16, 24, FitText(status, f.Handle, resx - 32, false), GetColor(255, 230, 120));
    }
#ifndef __ANDROID__
    if (!FilterUI.Open) {
        // 画面下の操作方法 (画面幅で折り返し、背景を付けて読みやすくする)
#ifndef __ANDROID__
        const auto lines = WrapText("Space: 絞り込み  F5: お気に入り  F6: お気に入りのみ表示  F7: 譜面制作で開く", sfont.Handle, resx - 32);
#else
        const auto lines = WrapText("タップ: 選ぶ  選んでいる曲をタップ: 決定  上下にドラッグ: 移動  検索ボタン: 絞り込み", sfont.Handle, resx - 32);
#endif
        const float lh = ConfigLineHeight();
        const float top = resy - 8 - lh * (float)lines.size();
        UIBox({ 0, top - 4, resx, resy }, GetColor(0, 0, 0), 210);     // 後ろの箱の文字が透けて読みにくくならないよう濃くする
        float y = top + lh * 0.5f;
        for (const auto& line : lines) {
            UIText(sfont, 16, y, line, GetColor(225, 225, 225));
            y += lh;
        }
    }
#endif
}

void Game::DrawFavoriteMark(const GraphData& box, const FontData& font, float y) {
    const float x = box.Pos.X - box.Size.Width * 0.5f + font.Size;
    UIText(font, x, y, "★", GetColor(255, 215, 0), 1);
}

std::vector<uint8_t> Game::FileToMem(const fs::path& path) {
    std::vector<std::uint8_t> buffer;
    std::error_code ec;
    if (fs::exists(path, ec)) {
        const auto size = fs::file_size(path, ec);
        if (ec) { return buffer; }
        buffer.resize((size_t)size);
        std::ifstream ifs(path, std::ios::binary);
        ifs.read(reinterpret_cast<char*>(buffer.data()), size);
    }
    return buffer;
}

void Game::MemToFile(const fs::path& path, const std::vector<uint8_t>& datas) {
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(datas.data()), datas.size());
}

#ifndef __ANDROID__
void Game::FileImport() {

    std::vector<std::string> DropFiles;

    if (GetDragFileNum() <= 0) {
        return;
    }

    char path[MAX_PATH];
    while (GetDragFilePath(path) == 0) {
        DropFiles.push_back(path);
    }
    DragFileInfoClear();

    fs::path ImportFolderPath;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    // DxLib 側で別モードで初期化済みの場合 RPC_E_CHANGED_MODE になるが、COM 自体は使える
    const bool NeedUninitialize = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) { return; }

    IFileOpenDialog* pFileOpen = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pFileOpen));

    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions))) {
            pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
        }

        IShellItem* pItem = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(GetExecutablePath().parent_path().wstring().c_str(), NULL, IID_PPV_ARGS(&pItem)))) {
            pFileOpen->SetFolder(pItem);
            pItem->Release();
        }

        if (SUCCEEDED(pFileOpen->Show(NULL))) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileOpen->GetResult(&pItem))) {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                    ImportFolderPath = pszPath;
                    CoTaskMemFree(pszPath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }
    if (NeedUninitialize) {
        CoUninitialize();
    }

    // ダイアログをキャンセルした場合は何もしない (空パスだとカレントディレクトリへ書き込んでしまう)
    if (ImportFolderPath.empty()) {
        return;
    }

    const fs::path ImportRoot = fs::weakly_canonical(ImportFolderPath);

    for (auto&& file : DropFiles) {
        try {
            fs::path path(StrToWStr(file));

            if (!catalog::IsArchive(path)) {
                fs::path dir = ImportFolderPath / path.filename();
                if (fs::is_directory(path)) {
                    fs::copy(path, dir, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                }
                else {
                    fs::copy(path, dir, fs::copy_options::overwrite_existing);
                }
                continue;
            }

            const fs::path archiveFolder = catalog::Extension(path) == ".osz" ? ImportFolderPath / path.stem() : ImportFolderPath;
            fs::create_directories(archiveFolder);
            const fs::path archiveRoot = fs::weakly_canonical(archiveFolder);
            std::ifstream ifs(path, std::ios::binary);
            cppunzip::IStreamFile istream(ifs);
            cppunzip::UnZipper unzip(istream);

            for (auto&& entry : unzip.listFiles()) {
                std::string name = entry.fileName();
                if (name.empty()) { continue; }
                if (name.back() == '/') { name.pop_back(); }

                // "../" などでインポート先の外へ書き込むエントリ (Zip Slip) を拒否する
                const fs::path dest = fs::weakly_canonical(archiveFolder / StrToWStr(name));
                auto [rootEnd, destIt] = std::mismatch(archiveRoot.begin(), archiveRoot.end(), dest.begin(), dest.end());
                if (rootEnd != archiveRoot.end()) { continue; }

                if (entry.isDir()) {
                    fs::create_directories(dest);
                    continue;
                }
                fs::create_directories(dest.parent_path());
                MemToFile(dest, entry.readContent());
            }
        }
        catch (const std::exception& e) {
            ShowErrorDialog(file + " のインポートに失敗しました。\n原因: " + DescribeException(e));
        }
    }

    const bool watched = std::ranges::any_of(Config.SongDirectories, [&](const auto& directory) {
        const auto root = fs::weakly_canonical(ResolveSongDirectory(directory));
        const auto relative = ImportRoot.lexically_relative(root);
        return !relative.empty() && *relative.begin() != "..";
    });
    if (!watched) { Config.SongDirectories.push_back(PathToU8(ImportRoot)); Config.Write(); }
    EnumChart(Config.SongDirectories);
}

std::wstring Game::QuoteArg(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring out = L"\"";
    for (auto it = arg.begin(); ; ++it) {
        size_t backslashes = 0;
        while (it != arg.end() && *it == L'\\') {
            ++it;
            ++backslashes;
        }
        if (it == arg.end()) {
            out.append(backslashes * 2, L'\\');
            break;
        }
        if (*it == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
        }
        else {
            out.append(backslashes, L'\\');
        }
        out.push_back(*it);
    }
    out.push_back(L'"');
    return out;
}

bool Game::RunProcess(const std::vector<std::wstring>& args, const fs::path& workdir, const char* status) {
    std::wstring cmdline;
    for (const auto& arg : args) {
        if (!cmdline.empty()) { cmdline += L' '; }
        cmdline += QuoteArg(arg);
    }

    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    // 進捗が見えるよう yt-dlp 用のコンソールを開く
    if (!CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, workdir.c_str(), &si, &pi)) {
        return false;
    }
    CloseHandle(pi.hThread);

    while (WaitForSingleObject(pi.hProcess, 16) == WAIT_TIMEOUT) {
        if (ProcessMessage() == -1) {
            TerminateProcess(pi.hProcess, 1);
            EndFlag = true;
            break;
        }
        ClearDrawScreen();
        DrawString(16, 16, status, GetColor(255, 255, 255));
        ScreenFlip();
    }

    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    return code == 0;
}

const char* Game::ToolName(ToolKind kind) { return kind == ToolKind::Ffmpeg ? "ffmpeg" : "yt-dlp"; }

fs::path Game::FindTool(ToolKind kind) {
    if (kind == ToolKind::Ffmpeg) { return rec::FindFfmpeg(GetExecutablePath()); }
    std::error_code ec;
    const fs::path local = GetExecutablePath() / "yt-dlp.exe";
    if (fs::is_regular_file(local, ec)) { return local; }
    wchar_t buf[MAX_PATH]{};
    if (SearchPathW(nullptr, L"yt-dlp.exe", nullptr, MAX_PATH, buf, nullptr) > 0) { return fs::path(buf); }
    return {};
}

bool Game::ConfirmToolInstall(ToolKind kind, const char* reason) {
    std::string text = std::string(reason) + "\n\n";
    if (kind == ToolKind::Ffmpeg) {
        text += "ffmpeg を配布元からダウンロードして、ゲームのフォルダに置きます (約 200MB)。\n"
            "・配布元: BtbN/FFmpeg-Builds (FFmpeg 公式サイトが案内している Windows 版)\n"
            "・ライセンス: GPL v3 (ライセンス文は licenses フォルダに保存します)\n"
            "・ゲームには同梱していません。ゲームは別のプログラムとして起動するだけです。\n";
    }
    else {
        text += "yt-dlp を配布元からダウンロードして、ゲームのフォルダに置きます (約 20MB)。\n"
            "・配布元: github.com/yt-dlp/yt-dlp (公式のリリース)\n"
            "・ライセンス: Unlicense\n"
            "・ダウンロードする動画・音源の利用は、各サイトの規約と著作権に従ってください。\n";
    }
    text += "\nダウンロードしたファイルは配布元が公開している SHA-256 と照合します。導入しますか?";
    return MessageBox(GetMainWindowHandle(), text.c_str(), "外部ツールの導入", MB_YESNO | MB_ICONQUESTION) == IDYES;
}

bool Game::InstallTool(ToolKind kind) {
    tools::Installer job;
    const fs::path exedir = GetExecutablePath();
    job.Start([kind, exedir](tools::Progress& p, std::string& e) {
        return kind == ToolKind::Ffmpeg ? tools::InstallFfmpeg(exedir, p, e) : tools::InstallYtDlp(exedir, p, e);
        }, std::string(ToolName(kind)) + " を導入しました");
    while (job.Busy()) {
        if (ProcessMessage() == -1) { job.Cancel(); EndFlag = true; break; }
        if (CheckHitKey(KEY_INPUT_ESCAPE)) { job.Cancel(); }
        ClearDrawScreen();
        DrawString(16, 16, (std::string(ToolName(kind)) + ": " + job.State().Describe()).c_str(), GetColor(255, 255, 255));
        DrawString(16, 48, "Esc: 中止", GetColor(200, 200, 200));
        ScreenFlip();
    }
    job.Join();
    const bool ok = job.State().State == tools::Progress::Phase::Done;
    if (!EndFlag) {
        MessageBox(GetMainWindowHandle(), job.State().Text().c_str(), ToolName(kind), ok ? MB_ICONINFORMATION : MB_ICONERROR);
    }
    Input::GetInstance().ResyncKeys();
    return ok;
}

bool Game::EnsureTool(ToolKind kind, const char* reason) {
    if (!FindTool(kind).empty()) { return true; }
    return ConfirmToolInstall(kind, reason) && InstallTool(kind) && !FindTool(kind).empty();
}

void Game::MediaDownload(const std::string& link, const fs::path& path, const char* confirm, const char* failed, const wchar_t* ext, const std::vector<std::wstring>& options) {
    std::error_code fec;
    if (link.empty() || fs::exists(path, fec)) {
        return;
    }
    if (MessageBox(NULL, confirm, "", MB_YESNO) != IDYES) {
        return;
    }
    // URL 以外 (オプションとして解釈される "-" 始まりなど) は受け付けない
    if (!link.starts_with("https://") && !link.starts_with("http://")) {
        MessageBox(NULL, TEXT("リンクは http:// または https:// で始まる必要があります"), TEXT("エラー"), MB_ICONERROR);
        return;
    }

    std::error_code ec;
    const fs::path workdir = GetExecutablePath() / "download";
    fs::create_directories(workdir, ec);
    const fs::path output = workdir / (std::wstring(L"media.") + ext);
    fs::remove(output, ec);

    // ゲームのフォルダの yt-dlp.exe を優先し、無ければ PATH から探す。どちらにも無ければ導入を提案する
    // (音源の変換・動画の再エンコードには ffmpeg も使う)
    if (!EnsureTool(ToolKind::YtDlp, "ダウンロードには yt-dlp が必要です。") || !EnsureTool(ToolKind::Ffmpeg, "音源・動画の変換には ffmpeg が必要です。")) {
        return;
    }
    const std::wstring exe = FindTool(ToolKind::YtDlp).wstring();

    // 自己更新は pip / winget 版では失敗するため、失敗してもダウンロードは続行する
    RunProcess({ exe, L"-U" }, workdir, "yt-dlp を更新しています...");
    if (EndFlag) { return; }

    std::vector<std::wstring> args{ exe };
    args.insert(args.end(), { L"--ffmpeg-location", FindTool(ToolKind::Ffmpeg).parent_path().wstring() });
    args.insert(args.end(), options.begin(), options.end());
    args.insert(args.end(), { L"-o", (workdir / L"media.%(ext)s").wstring(), L"--", StrToWStr(link) });

    if (!RunProcess(args, workdir, "ダウンロードしています...") || !fs::exists(output, ec)) {
        if (!EndFlag) {
            MessageBox(NULL, failed, TEXT("エラー"), MB_ICONERROR);
        }
        return;
    }

    fs::rename(output, path, ec);
    if (ec) {
        // 別ドライブへの移動は rename できないためコピーで代替する
        fs::copy_file(output, path, fs::copy_options::overwrite_existing, ec);
        fs::remove(output, ec);
    }
}

#endif
#ifdef __ANDROID__
bool Game::AndroidDownloadMedia(const std::string& link, const fs::path& path, bool audio) {
    if (!link.starts_with("https://") && !link.starts_with("http://")) { ShowToast("リンクは http:// または https:// で始まる必要があります"); return false; }
    const fs::path workdir = GetExecutablePath() / "download";
    std::error_code ec;
    fs::create_directories(workdir, ec);
    struct Task { std::atomic_bool Done = false; std::string Result, Error; };
    auto task = std::make_shared<Task>();
    std::thread([task, link, workdir, audio] {
        struct Completion { std::shared_ptr<Task> State; ~Completion(){State->Done=true;} } completion{task};
        try {
        const ANativeActivity* activity = GetNativeActivity();
        JNIEnv* env = nullptr;
        if (!activity || !activity->vm || activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) { task->Error = "JNI"; return; }
        struct Attachment {
            JavaVM* Vm; JNIEnv* Env;
            ~Attachment(){if(Env->ExceptionCheck())Env->ExceptionClear();Vm->DetachCurrentThread();}
        } attachment{activity->vm,env};
        const auto readString=[&](jstring value,std::string& out){
            if(!value||env->ExceptionCheck())return;
            const char* chars=env->GetStringUTFChars(value,nullptr);
            if(!chars)return;
            struct UtfChars { JNIEnv* Env; jstring Value; const char* Chars; ~UtfChars(){Env->ReleaseStringUTFChars(Value,Chars);} } release{env,value,chars};
            out=chars;
        };
        jclass cls = env->GetObjectClass(activity->clazz);
        if(!cls||env->ExceptionCheck())return;
        jmethodID method = env->GetMethodID(cls, "DownloadMedia", "(Ljava/lang/String;Ljava/lang/String;Z)Ljava/lang/String;");
        if (method != nullptr && !env->ExceptionCheck()) {
            jstring jurl = env->NewStringUTF(link.c_str());
            if(!jurl||env->ExceptionCheck())return;
            jstring jdir = env->NewStringUTF(workdir.string().c_str());
            if(!jdir||env->ExceptionCheck())return;
            auto jres = (jstring)env->CallObjectMethod(activity->clazz, method, jurl, jdir, (jboolean)audio);
            if(env->ExceptionCheck())return;
            if (jres != nullptr) {
                readString(jres,task->Result);
                env->DeleteLocalRef(jres);
            }
            env->DeleteLocalRef(jurl);
            env->DeleteLocalRef(jdir);
            if (task->Result.empty() && !env->ExceptionCheck()) {
                jfieldID field = env->GetFieldID(cls, "DownloadError", "Ljava/lang/String;");
                if(env->ExceptionCheck())return;
                if (auto jerr = field ? (jstring)env->GetObjectField(activity->clazz, field) : nullptr) {
                    readString(jerr,task->Error);
                    env->DeleteLocalRef(jerr);
                }
            }
        }
        // 残ったローカル参照は専用スレッドの DetachCurrentThread で解放する。
        } catch(...) { task->Result.clear(); }
        }).detach();
    while (!task->Done) {
        if (ProcessMessage() == -1) { EndFlag = true; return false; }
        ClearDrawScreen();
        DrawString(16, 16, "ダウンロードしています... (初回は yt-dlp の準備に時間がかかります)", GetColor(255, 255, 255));
        ScreenFlip();
    }
    if (task->Result.empty()) { ShowToast("ダウンロードに失敗しました: " + task->Error); return false; }
    fs::rename(task->Result, path, ec);
    if (ec) {
        fs::copy_file(task->Result, path, fs::copy_options::overwrite_existing, ec);
        if(ec)return false;
        std::error_code cleanupError;
        fs::remove(task->Result, cleanupError);
    }
    return !ec && fs::exists(path, ec);
}

#endif
void Game::SongDownload(const std::string& link, const fs::path& path) {
#ifndef __ANDROID__
    MediaDownload(link, path, "音源ファイルがありません。ダウンロードしますか？", "音源のダウンロードに失敗しました", L"ogg", { L"-x", L"--audio-format", L"vorbis" });
#else
    std::error_code ec;
    if (link.empty() || fs::exists(path, ec)) { return; }
    AndroidDownloadMedia(link, path, true);
#endif
}

void Game::MovieDownload(const std::string& link, const fs::path& path) {
#ifndef __ANDROID__
    MediaDownload(link, path, "動画ファイルがありません。ダウンロードしますか？", "動画のダウンロードに失敗しました", L"avi", { L"--recode-video", L"avi" });
#endif
}
