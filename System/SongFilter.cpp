#include "../Game.hpp"

const char* Game::CourseLabel(int course) {
    static const char* names[] = { "かんたん", "ふつう", "むずかしい", "おに", "裏", "タワー", "段位" };
    return course >= 0 && course < (int)std::size(names) ? names[course] : "すべて";
}

void Game::CollectGenreFolders(const std::vector<std::unique_ptr<BoxData>>& datas, const std::string& name, bool inside, std::vector<std::string>& out) const {
    for (const auto& data : datas) {
        if (!data->IsGenre()) { continue; }
        const GenreData& genre = *data->GetGenre();
        const bool match = inside || genre.Name == name;
        if (match && std::ranges::find(out, ToLower(genre.Name)) == out.end()) { out.push_back(ToLower(genre.Name)); }
        CollectGenreFolders(genre.Datas, name, match, out);
    }
}

auto Game::BuildFilterQuery(const FilterConditions& f) const -> SearchQuery {
    SearchQuery q;
    const std::string_view keyword = strtrim(f.Keyword);
    if (f.Exact) {
        if (!keyword.empty()) { q.ExactText = NormalizeSearchText(keyword); }
    }
    else {
        q = ParseSearch(keyword);
    }
    if (!f.Genre.empty()) {
        CollectGenreFolders(TempBoxDatas, f.Genre, false, q.GenreFolders);
        if (q.GenreFolders.empty()) { q.GenreFolders.push_back(ToLower(f.Genre)); }    // 一覧から無くなったジャンル (どの譜面にも当てはまらない)
    }
    if (f.Course >= 0) { q.Course = f.Course; }
    q.FavoriteOnly = q.FavoriteOnly || f.FavoriteOnly;
    return q;
}

size_t Game::CountFilterMatches(const FilterConditions& f) const {
    const SearchQuery q = BuildFilterQuery(f);
    size_t count = 0;
    ForEachChart(TempBoxDatas, [&](const ChartData& chart) { count += MatchChart(chart, q) ? 1 : 0; });
    return count;
}

auto Game::FilterRowType(int row) -> ConfigItemType {
    switch (row) {
    case RowKeyword: return ConfigItemType::Text;
    case RowFavorite: return ConfigItemType::Bool;
    default: return ConfigItemType::Choice;
    }
}

const char* Game::FilterRowLabel(int row) {
    static const char* labels[] = { "キーワード", "一致のしかた", "ジャンル", "コース", "お気に入りのみ" };
    return row >= 0 && row < (int)std::size(labels) ? labels[row] : "";
}

const char* Game::FilterRowHelp(int row) {
    switch (row) {
    case RowKeyword: return "タイトル・サブタイトル・ジャンルで探す";
    case RowExact: return "完全一致: タイトルかサブタイトルが同じ譜面だけ";
    case RowGenre: return "譜面フォルダのジャンルで絞り込む (中のジャンルも含む)";
    case RowCourse: return "そのコースがある譜面だけ";
    case RowFavorite: return "★ を付けた譜面だけ (選曲画面の F5 で ★)";
    default: return "ランダム: 当てはまる譜面から 1 曲選ぶ";
    }
}

std::string Game::FilterRowValue(int row) {
    const auto& e = FilterUI.Edit;
    switch (row) {
    case RowKeyword: return e.Keyword;
    case RowExact: return e.Exact ? "完全一致" : "部分一致";
    case RowGenre: return e.Genre.empty() ? "すべて" : e.Genre;
    case RowCourse: return CourseLabel(e.Course);
    case RowFavorite: return e.FavoriteOnly ? "ON" : "OFF";
    default: return "";
    }
}

void Game::FilterStep(int row, int dir) {
    auto& e = FilterUI.Edit;
    switch (row) {
    case RowExact: e.Exact = !e.Exact; break;
    case RowFavorite: e.FavoriteOnly = !e.FavoriteOnly; break;
    case RowGenre: {
        const int n = (int)FilterUI.Genres.size() + 1;   // 0 = すべて
        auto it = std::ranges::find(FilterUI.Genres, e.Genre);
        int index = e.Genre.empty() || it == FilterUI.Genres.end() ? 0 : (int)(it - FilterUI.Genres.begin()) + 1;
        index = ((index + (dir > 0 ? 1 : -1)) % n + n) % n;
        e.Genre = index == 0 ? std::string() : FilterUI.Genres[index - 1];
        break;
    }
    case RowCourse: {
        const int n = (int)CourseType::Count + 1;          // -1 = すべて
        e.Course = ((e.Course + 1 + (dir > 0 ? 1 : -1)) % n + n) % n - 1;
        break;
    }
    default:
        break;
    }
    FilterUI.PreviewDirty = true;
}

float Game::FilterLineHeight() const { return ConfigLineHeight(); }

auto Game::FilterPanelRect() const -> UIRect {
    const float x = (float)Skin.Info.Resolution.X, y = (float)Skin.Info.Resolution.Y;
    const float lh = FilterLineHeight();
    const float h = std::min(y * 0.9f, lh * (2.2f + 1.8f * static_cast<float>(RowButtons) + 5.6f) + 40.0f);
    return { x * 0.14f, (y - h) * 0.5f, x * 0.86f, (y + h) * 0.5f };
}

float Game::FilterRowHeight() const { return FilterLineHeight() * 1.8f; }

auto Game::FilterRowRect(int row) const -> UIRect {
    const UIRect r = FilterPanelRect();
    const float h = FilterRowHeight();
    const float y = r.T + FilterLineHeight() * 2.2f + 8.0f + h * row;
    return { r.L + 16, y, r.R - 16, y + h };
}

auto Game::FilterValueRect(int row) const -> UIRect {
    const UIRect r = FilterPanelRect();
    const UIRect rr = FilterRowRect(row);
    const float cy = (rr.T + rr.B) * 0.5f, h = (rr.B - rr.T) * 0.36f;
    return { r.L + (r.R - r.L) * 0.34f, cy - h, r.R - 32, cy + h };
}

auto Game::FilterArrowRect(int row, bool right) const -> UIRect {
    const UIRect v = FilterValueRect(row);
    const float w = std::max(36.0f, v.B - v.T);
    return right ? UIRect{ v.R - w, v.T, v.R, v.B } : UIRect{ v.L, v.T, v.L + w, v.B };
}

auto Game::FilterButtonRect(int index) const -> UIRect {
    const UIRect r = FilterPanelRect();
    const float lh = FilterLineHeight();
    const float w = (r.R - r.L - 32 - 16 * (FilterButtonCount - 1)) / (float)FilterButtonCount;
    const float x = r.L + 16 + (w + 16) * index;
    return { x, r.B - 16 - lh * 1.6f, x + w, r.B - 16 };
}

void Game::OpenFilterPanel() {
    FilterUI.Open = true;
    FilterUI.Edit = Filter;
    FilterUI.Selected = RowKeyword;
    FilterUI.Button = 0;
    FilterUI.PreviewDirty = true;
    // ジャンルは譜面フォルダにあるジャンルフォルダ (genre.ini) のうち、このモードの譜面があるものだけ (フォルダの並び順)
    std::vector<std::string> genres;
    auto collect = [&](const std::vector<std::unique_ptr<BoxData>>& datas, auto& self) -> void {
        for (const auto& data : datas) {
            if (!data->IsGenre()) { continue; }
            const GenreData& genre = *data->GetGenre();
            if (GenreHasCharts(genre, IsDanSelect()) && !genre.Name.empty() && std::ranges::find(genres, genre.Name) == genres.end()) {
                genres.push_back(genre.Name);
            }
            self(genre.Datas, self);
        }
    };
    collect(TempBoxDatas, collect);
    FilterUI.Genres = std::move(genres);
    PointerPrev.Left = true;
}

void Game::CloseFilterPanel() {
#ifndef __ANDROID__
    if (FilterUI.Editing) {
        DeleteKeyInput(FilterUI.EditHandle);
    }
#endif
    FilterUI.Editing = false;
    FilterUI.EditHandle = 0;
    FilterUI.Open = false;
    PointerPrev.Left = true;
#ifndef __ANDROID__
    Input::GetInstance().ResyncKeys();
#endif
}

void Game::ApplyFilter(bool random) {
    const FilterConditions previous = Filter;
    Filter = FilterUI.Edit;
    BoxDatasUpdate();
    if (BoxDatas.empty()) {
        Filter = previous;
        BoxDatasUpdate();
        ShowSelectMessage("条件に一致する譜面がありません");
        return;
    }
    BoxDataIndex = random ? GetRand((int)BoxDatas.size() - 1) : 0;
    ShowSelectMessage(Filter.Empty() ? std::string("絞り込みを解除しました") : std::to_string(BoxDatas.size()) + " 件見つかりました");
    DemoSongPlayBlank.Reset();
    ResetDemoSong();
    CloseFilterPanel();
}

void Game::DrawFilterPanel(const FontData& titlefont) {
    if (FilterUI.PreviewDirty) {
        FilterUI.Preview = CountFilterMatches(FilterUI.Edit);
        FilterUI.PreviewDirty = false;
    }
    const FontData& sfont = ConfigSmallFont();
    const unsigned int white = GetColor(255, 255, 255);
    const unsigned int black = GetColor(0, 0, 0);
    const unsigned int gray = GetColor(170, 170, 170);
    const unsigned int accent = GetColor(255, 200, 40);
    const UIRect r = FilterPanelRect();
    const float lh = FilterLineHeight();

    UIBox({ 0, 0, (float)Skin.Info.Resolution.X, (float)Skin.Info.Resolution.Y }, black, 120);
    UIBox(r, GetColor(18, 20, 32), 238);
    UIBox(r, white, 200, false);
    const FontData& title = titlefont.Handle != -1 && titlefont.Size <= lh * 1.6f ? titlefont : sfont;
    UIText(title, r.L + 24, r.T + lh * 1.1f, FitText("譜面の絞り込み", title.Handle, r.R - r.L - 48, false), white);

    const float labelw = FilterValueRect(0).L - (r.L + 32) - 12;
    for (int row = 0; row < RowButtons; ++row) {
        const UIRect rr = FilterRowRect(row);
        const float cy = (rr.T + rr.B) * 0.5f;
        if (FilterUI.Selected == row) {
            UIBox(rr, accent, 60);
        }
        UIText(sfont, rr.L + 16, cy, FitText(FilterRowLabel(row), sfont.Handle, labelw, false), white);

        const UIRect v = FilterValueRect(row);
        const std::string value = FilterRowValue(row);
        switch (FilterRowType(row)) {
        case ConfigItemType::Text:
            UIBox(v, black, 170);
            UIBox(v, white, 120, false);
            if (!(FilterUI.Editing && FilterUI.Selected == row)) {
                UIText(sfont, v.L + 12, cy, value.empty() ? std::string("クリックして入力") : FitText(value, sfont.Handle, v.R - v.L - 24, false), value.empty() ? gray : white);
            }
            break;
        case ConfigItemType::Bool: {
            const bool on = value == "ON";
            const UIRect pill{ v.L, v.T, v.L + std::max(96.0f, sfont.Size * 4.0f), v.B };
            UIBox(pill, on ? GetColor(40, 200, 90) : GetColor(90, 90, 90), 230);
            UIText(sfont, (pill.L + pill.R) * 0.5f, cy, value, white, 1);
            break;
        }
        default: {
            const UIRect la = FilterArrowRect(row, false), ra = FilterArrowRect(row, true);
            UIBox(la, black, 160);
            UIBox(ra, black, 160);
            UIText(sfont, (la.L + la.R) * 0.5f, cy, "◀", white, 1);
            UIText(sfont, (ra.L + ra.R) * 0.5f, cy, "▶", white, 1);
            UIText(sfont, (la.R + ra.L) * 0.5f, cy, FitText(value, sfont.Handle, ra.L - la.R - 16, false), value == "すべて" ? gray : white, 1);
            break;
        }
        }
    }

    static const char* buttons[FilterButtonCount] = { "検索", "リセット", "ランダム", "閉じる" };
    for (int i = 0; i < FilterButtonCount; ++i) {
        const UIRect b = FilterButtonRect(i);
        const bool current = FilterUI.Selected == RowButtons && FilterUI.Button == i;
        UIBox(b, current ? GetColor(214, 116, 20) : GetColor(50, 54, 70), 230);
        UIBox(b, white, 160, false);
        UIText(sfont, (b.L + b.R) * 0.5f, (b.T + b.B) * 0.5f, FitText(buttons[i], sfont.Handle, b.R - b.L - 12, false), white, 1);
    }

    // 下部: 該当数・選んでいる項目の説明・操作方法 (すべてパネルの中で折り返す)
    // ボタンの上から上へ順に置き、行の領域に入りそうなら説明を減らす (パネルが画面の高さで縮んだ時も重ならない)
    const float textw = r.R - r.L - 48;
    const float top = FilterRowRect(RowButtons).T + lh * 0.5f;
    float y = FilterButtonRect(0).T - lh * 0.7f;
#ifndef __ANDROID__
    UIText(sfont, r.L + 24, y, FitText("↑↓: 項目  ←→: 変更  Enter: 決定  Esc: 閉じる", sfont.Handle, textw, false), gray);
    y -= lh;
#endif
    auto help = WrapText(FilterRowHelp(FilterUI.Selected), sfont.Handle, textw);
    help.resize(std::min<size_t>(help.size(), 2));
    while (!help.empty() && y - lh * (float)help.size() < top) { help.pop_back(); }
    for (size_t i = help.size(); i-- > 0;) {
        UIText(sfont, r.L + 24, y, help[i], white);
        y -= lh;
    }
    if (y >= top - lh * 0.5f) {
        UIText(sfont, r.L + 24, y, "該当: " + std::to_string(FilterUI.Preview) + " 譜面", FilterUI.Preview == 0 ? GetColor(255, 120, 120) : GetColor(255, 230, 120));
    }
#ifndef __ANDROID__
    if (FilterUI.Editing) {
        const UIRect v = FilterValueRect(FilterUI.Selected);
        UIBox(v, black, 235);
        UIBox(v, accent, 255, false);
        SetDrawArea((int)v.L + 4, (int)v.T, (int)v.R - 4, (int)v.B);
        DrawTextInput((int)v.L + 12, (int)(((v.T + v.B) * 0.5f) - sfont.Size * 0.5f), FilterUI.EditHandle);
        SetDrawAreaFull();
    }
#endif
}

void Game::FilterPanelProc() {
    auto Se = [&](bool don) {
        if (IsDanSelect()) { (don ? Skin.Base->DanSelect.SE.Don : Skin.Base->DanSelect.SE.Ka).Play(); }
        else { (don ? Skin.Base->SongSelect.SE.Don : Skin.Base->SongSelect.SE.Ka).Play(); }
    };
    const PointerState pt = ReadPointer();

#ifndef __ANDROID__
    if (FilterUI.Editing) {
        const int state = CheckKeyInput(FilterUI.EditHandle);
        const bool clickout = pt.LeftPressed && !FilterValueRect(FilterUI.Selected).Contains(pt.X, pt.Y);
        if (state != 0 || clickout) {
            if (state == 1 || clickout) {
                char buf[1024]{};
                GetKeyInputString(buf, FilterUI.EditHandle);
                FilterUI.Edit.Keyword = std::string(strtrim(buf));
                FilterUI.PreviewDirty = true;
            }
            DeleteKeyInput(FilterUI.EditHandle);
            FilterUI.EditHandle = 0;
            FilterUI.Editing = false;
            Input::GetInstance().ResyncKeys();
        }
        return;
    }
#endif

    auto BeginEdit = [&] {
        Se(true);
#ifndef __ANDROID__
        FilterUI.EditHandle = MakeKeyInput(255, TRUE, FALSE, FALSE);
        SetKeyInputString(FilterUI.Edit.Keyword.c_str(), FilterUI.EditHandle);
        SetActiveKeyInput(FilterUI.EditHandle);
        SetKeyInputStringFont(ConfigSmallFont().Handle);
        FilterUI.Editing = true;
#else
        FilterUI.Edit.Keyword = std::string(strtrim(GetInputText(FilterRowLabel(RowKeyword))));
        FilterUI.PreviewDirty = true;
#endif
    };
    auto PressButton = [&](int button) {
        Se(true);
        switch (button) {
        case 0: ApplyFilter(false); break;
        case 1: FilterUI.Edit = FilterConditions(); FilterUI.PreviewDirty = true; break;
        case 2: ApplyFilter(true); break;
        default: CloseFilterPanel(); break;
        }
    };
    auto Activate = [&](int row) {
        if (row == RowButtons) { PressButton(FilterUI.Button); return; }
        if (FilterRowType(row) == ConfigItemType::Text) { BeginEdit(); return; }
        Se(false);
        FilterStep(row, 1);
    };

    // ---- マウス ----
    for (int i = 0; i < FilterButtonCount; ++i) {
        if (pt.LeftPressed && FilterButtonRect(i).Contains(pt.X, pt.Y)) {
            FilterUI.Selected = RowButtons;
            FilterUI.Button = i;
            PressButton(i);
            return;
        }
    }
    int hovered = -1;
    for (int row = 0; row < RowButtons; ++row) {
        if (FilterRowRect(row).Contains(pt.X, pt.Y)) { hovered = row; break; }
    }
    if (hovered >= 0 && (pt.Moved || pt.LeftPressed)) {
        FilterUI.Selected = hovered;
    }
    if (hovered >= 0 && pt.LeftPressed) {
        const ConfigItemType type = FilterRowType(hovered);
        if (type == ConfigItemType::Bool || type == ConfigItemType::Text) {
            Activate(hovered);
        }
        else if (FilterArrowRect(hovered, false).Contains(pt.X, pt.Y)) { Se(false); FilterStep(hovered, -1); }
        else if (FilterArrowRect(hovered, true).Contains(pt.X, pt.Y)) { Se(false); FilterStep(hovered, 1); }
        else if (FilterValueRect(hovered).Contains(pt.X, pt.Y)) { Activate(hovered); }
    }
    if (!FilterUI.Open || FilterUI.Editing) {
        return;
    }

    // ---- キーボード ----
#ifndef __ANDROID__
    auto Move = [&](int dir) {
        Se(false);
        FilterUI.Selected = std::clamp(FilterUI.Selected + dir, 0, (int)RowButtons);
        };
    auto Change = [&](int dir) {
        if (FilterUI.Selected == RowButtons) {
            Se(false);
            FilterUI.Button = std::clamp(FilterUI.Button + dir, 0, FilterButtonCount - 1);
        }
        else if (FilterRowType(FilterUI.Selected) != ConfigItemType::Text) {
            Se(false);
            FilterStep(FilterUI.Selected, dir);
        }
    };
    Input::GetInstance().HitKeyProcess(VK_UP, KeyState::Down, [&] { Move(-1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_DOWN, KeyState::Down, [&] { Move(1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { Move(-1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { Move(1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_LEFT, KeyState::Down, [&] { Change(-1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_RIGHT, KeyState::Down, [&] { Change(1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_RETURN, KeyState::Down, [&] { Activate(FilterUI.Selected); });
    Input::GetInstance().HitKeyesProcess(Config.DonInputLeft, KeyState::Down, [&] { Activate(FilterUI.Selected); });
    Input::GetInstance().HitKeyesProcess(Config.DonInputRight, KeyState::Down, [&] { Activate(FilterUI.Selected); });
    Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] { Se(false); CloseFilterPanel(); });
#else
    Touch.Process(TouchType::Other, [&] { Se(false); CloseFilterPanel(); },
        IsDanSelect() ? Skin.Base->DanSelect.Image.Back : Skin.Base->SongSelect.Image.Back);
#endif
}

void Game::SelectSceneEnter() {
    FilterUI.Open = false;
    PointerPrev.Left = PointerPrev.Right = PointerPrev.Middle = true;
    // 前の画面 (リザルトなど) の入力を持ち込まない: たまったホイール・押したままの指を離した時のタップで、
    // 入った直後に曲が動いてカッが鳴らないように
#ifndef __ANDROID__
    SetMouseDispFlag(TRUE);
    Input::GetInstance().GetMouseWheel();
#else
    TouchState = TouchTrack{};
    if (GetTouchInputNum() > 0) { TouchState.Active = true; TouchState.Long = true; }
#endif
}

void Game::SelectSceneLeave() {
    if (FilterUI.Open) {
        CloseFilterPanel();
    }
#ifndef __ANDROID__
    SetMouseDispFlag(!Config.FullScreen);
#endif
}

int Game::HitTestSelectBox(const GraphData& box, float listy, float distance, const PointerState& pt) const {
    int hit = -1;
    float best = std::numeric_limits<float>::max();
    for (int i = BoxDataIndex - 6; i < BoxDataIndex + 6; ++i) {
        if (i < 0 || i >= (int)BoxDatas.size()) { continue; }
        const float cy = box.Pos.Y + (i - BoxDataIndex) * distance + listy;
        const UIRect r{ box.Pos.X - box.Size.Width * 0.5f, cy - box.Size.Height * 0.5f, box.Pos.X + box.Size.Width * 0.5f, cy + box.Size.Height * 0.5f };
        if (r.Contains(pt.X, pt.Y) && std::abs(pt.Y - cy) < best) {
            best = std::abs(pt.Y - cy);
            hit = i;
        }
    }
    return hit;
}
