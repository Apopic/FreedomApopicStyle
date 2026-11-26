// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    // ===================================================================== 譜面の絞り込み画面
    // 選曲・段位選択画面で Space (Android は検索ボタン) を押すと開く。マウス・キーボードの両方で操作できる。
    // キーワード欄では、これまでの検索式 (lv>=8 c:oni など) もそのまま使える。

    struct FilterConditions {
        std::string Keyword;
        bool Exact = false;             // キーワードがタイトル・サブタイトルと完全に一致
        std::string Genre;              // 空 = すべて
        int Course = -1;                // -1 = すべて
        bool FavoriteOnly = false;

        bool Empty() const {
            return strtrim(Keyword).empty() && Genre.empty() && Course < 0 && !FavoriteOnly;
        }
        std::string Summary() const {
            std::vector<std::string> parts;
            if (!strtrim(Keyword).empty()) { parts.push_back("「" + std::string(strtrim(Keyword)) + "」" + (Exact ? "(完全一致)" : "")); }
            if (!Genre.empty()) { parts.push_back(Genre); }
            if (Course >= 0) { parts.push_back(CourseLabel(Course)); }
            if (FavoriteOnly) { parts.push_back("★お気に入り"); }
            std::string out;
            for (size_t i = 0; i < parts.size(); ++i) { out += (i ? " / " : "") + parts[i]; }
            return out;
        }
    } Filter;

    static const char* CourseLabel(int course);

    // 絞り込み条件を検索条件に変換する (画面で指定した条件は、キーワード欄の同じ種類の条件より優先する)
    // 名前が name のジャンルフォルダと、その中のジャンルフォルダの名前 (小文字)
    void CollectGenreFolders(const std::vector<std::unique_ptr<BoxData>>& datas, const std::string& name, bool inside, std::vector<std::string>& out) const;
    SearchQuery BuildFilterQuery(const FilterConditions& f) const;

    template<class F>
    void ForEachChart(const std::vector<std::unique_ptr<BoxData>>& datas, F&& f) const {
        for (const auto& data : datas) {
            if (data->IsGenre()) { ForEachChart(data->GetGenre()->Datas, f); }
            else if (data->GetChart()->IsDan == IsDanSelect()) { f(*data->GetChart()); }
        }
    }
    size_t CountFilterMatches(const FilterConditions& f) const;

    enum FilterRow { RowKeyword, RowExact, RowGenre, RowCourse, RowFavorite, RowButtons, FilterRowCount };
    static constexpr int FilterButtonCount = 4;
    struct FilterPanelState {
        bool Open = false;
        FilterConditions Edit;
        int Selected = 0;
        int Button = 0;                 // 0: 検索, 1: リセット, 2: ランダム, 3: 閉じる
        bool Editing = false;
        int EditHandle = 0;
        std::vector<std::string> Genres;
        size_t Preview = 0;
        bool PreviewDirty = true;
    } FilterUI;

    static ConfigItemType FilterRowType(int row);
    static const char* FilterRowLabel(int row);
    static const char* FilterRowHelp(int row);
    std::string FilterRowValue(int row);
    void FilterStep(int row, int dir);

    // ---- レイアウト (文字は設定画面と同じ小さいフォントで、行の高さもその大きさから決める) ----
    float FilterLineHeight() const;
    UIRect FilterPanelRect() const;
    float FilterRowHeight() const;
    UIRect FilterRowRect(int row) const;
    UIRect FilterValueRect(int row) const;
    UIRect FilterArrowRect(int row, bool right) const;
    UIRect FilterButtonRect(int index) const;

    // ---- 開閉・適用 ----
    void OpenFilterPanel();
    void CloseFilterPanel();
    void ApplyFilter(bool random);

    void DrawFilterPanel(const FontData& titlefont);

    void FilterPanelProc();

    // ---- 選曲・段位選択画面の共通処理 ----
    void SelectSceneEnter();
    void SelectSceneLeave();
    // 一覧の箱のうち、ポインタの下にあるもの (重なっている場合は中心が近いほう)。なければ -1
    int HitTestSelectBox(const GraphData& box, float listy, float distance, const PointerState& pt) const;
#endif // FAS_GAME_MEMBER_DECLARATIONS
