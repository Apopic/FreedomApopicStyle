// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

    // ===================================================================== 設定画面
    // カテゴリのタブと項目の一覧で構成し、マウス (クリック・ホイール・右クリック) とキーボードの両方で操作できる。
    // 値を変えるたびに config.json へ保存し、スキン・譜面フォルダの変更は画面を出る時に反映する。

    enum class ConfigItemType {
        Bool,       // ON / OFF
        Number,     // ◀ 数値 ▶ (クリックで直接入力)
        Choice,     // ◀ 選択肢 ▶
        Text,       // 文字入力
        Key,        // キー割り当て (可変長。末尾の「+」枠で追加)
        Folder,     // 譜面フォルダ 1 件 (パス / 参照 / 相対・絶対 / 削除)
        FolderAdd,  // 譜面フォルダの追加 (参照して追加 / パスを入力して追加)
        Action,     // ボタン 1 つ (Step を呼ぶ)
    };

    struct ConfigItem {
        std::string Name;
        std::string Help;
        ConfigItemType Type = ConfigItemType::Bool;
        bool Restart = false;                           // 再起動後に反映される
        std::function<std::string()> Get;               // 表示する値
        std::function<void(int)> Step;                  // 値を ±n 段階変える (Bool は反転)
        std::function<bool(const std::string&)> Set;    // 文字入力の確定
        std::function<void()> Reset;                    // 既定値に戻す (戻せない項目は空)
        int Index = -1;                                 // キー: レーン番号 / フォルダ: SongDirectories の添字
#ifndef __ANDROID__
        std::vector<int>* Keys = nullptr;
#endif
    };

    struct ConfigCategory {
        std::string Name;
        std::vector<ConfigItem> Items;
    };

    struct UIRect {
        float L = 0, T = 0, R = 0, B = 0;
        bool Contains(int x, int y) const { return x >= L && x < R && y >= T && y < B; }
    };

    struct PointerState {
        int X = 0, Y = 0;
        bool Left = false, LeftPressed = false, RightPressed = false, MiddlePressed = false, Moved = false;
        int Wheel = 0;
    };
    // 前フレームのボタン状態 (押した瞬間の判定用。画面を切り替えた時は Left = true にして、切り替えに使ったクリックを無視する)
    struct {
        bool Left = true, Right = true, Middle = true;
        int X = -1, Y = -1;
    } PointerPrev;

#ifndef __ANDROID__
    // フォルダ選択ダイアログは別スレッドで開き、ゲームのループは止めない
    struct FolderPickState {
        std::atomic_bool Done = false;
        std::wstring Result;            // 空 = キャンセル (Done を立てる前に書き込む)
    };
    static constexpr int PickerDiscard = -2;
#endif

    static constexpr const char* FolderCategoryName = "フォルダ";

    struct ConfigUIState {
        std::vector<ConfigCategory> Categories;
        bool RecommendOpen = false;     // おすすめ設定の確認画面
        std::vector<RecommendChange> Recommend;
        int Category = 0;
        int Selected = 0;
        int KeySlot = 0;                // キーの枠 / フォルダ行の部品
        int Scroll = 0;                 // 先頭に表示している行
        bool Editing = false;           // 文字入力中
        int EditHandle = 0;
        bool Capturing = false;         // キー割り当ての入力待ち
        bool TaikoEditing = false;      // 面で選択した項目を縁で変更する
        unsigned char CapturePrev[256]{};        // 入力待ちを始めた時のキーの状態 (押しっぱなしのキーを拾わない)
        bool FoldersDirty = false;      // フォルダの数が変わったので行を作り直す
        std::vector<char> FolderExists;
        std::chrono::steady_clock::time_point ResetArmUntil{};     // 「既定値に戻す」をもう一度押すと戻す期限
        std::string ArmedKey;           // 「もう一度押すと実行」を待っている項目
        std::string Message;            // 画面下に一時的に出すお知らせ
        std::chrono::steady_clock::time_point MessageUntil{};
        std::string SkinOnEnter;
        std::vector<std::string> DirsOnEnter;
        std::string NameOnEnter;
        std::string TitleOnEnter;
#ifndef __ANDROID__
        std::shared_ptr<FolderPickState> Picker;
        int PickerTarget = -1;          // -1: 追加, 0～: 置き換え, PickerDiscard: 結果を捨てる
#endif
    } ConfigUI;

    void ShowConfigMessage(std::string message, int seconds = 4);

    static std::string FormatNumber(double value, int decimals);

    // ---- 文字列の折り返し・省略 (描画用。結果はキャッシュする) ----
    mutable std::unordered_map<std::string, std::vector<std::string>> TextWrapCache;
    mutable std::unordered_map<std::string, std::string> TextFitCache;

    static size_t Utf8CharLen(unsigned char c);
    static std::string TextCacheKey(const std::string& str, int handle, float width, char mode);
    // 幅 width に収まるように行を分ける。空白があればそこで、なければ文字の境目で折り返す
    std::vector<std::string> WrapText(const std::string& str, int handle, float width) const;
    // 幅に収まらなければ省略記号を付けて縮める (front: 先頭を省略してパスの末尾を見せる)
    // スキンのフォントで画面に収まるように描く (揃え方に合わせて、画面の端までの幅に収まらない分を … にする)
    void DrawFitScreen(FontData& font, Pos2D<float> pos, const std::string& str) const;
    std::string FitText(const std::string& str, int handle, float width, bool front) const;

    // Config のメンバーを既定値に戻す関数 (Config の外の値は戻せないので空)
    template <class T>
    std::function<void()> MakeConfigReset(T& v) {
        const auto* base = reinterpret_cast<const char*>(&Config);
        const auto* addr = reinterpret_cast<const char*>(&v);
        if (addr < base || addr + sizeof(T) > base + sizeof(_ConfigData)) { return {}; }
        const T* def = reinterpret_cast<const T*>(reinterpret_cast<const char*>(&DefaultConfig) + (addr - base));
        return [&v, def] { v = *def; };
    }
    void ConfigResetItem(ConfigItem& item);

    // 「もう一度押すと実行」: 同じ項目を 4 秒以内にもう一度押したら true
    bool ConfigArmed(const std::string& key) const;
    bool ConfigConfirm(const std::string& key, const std::string& message);
    // カテゴリの項目を既定値に戻す。再起動が必要な項目があれば true
    bool ConfigResetCategory(size_t ci);

    void BuildConfigItems();

    static const char* ConfigLaneKey(int lane);
    static const char* ConfigLaneName(int lane);

    // ---- 譜面フォルダ ----
    void BuildFolderItems(std::vector<ConfigItem>& items);
    static bool SameSongDirectory(const std::string& a, const std::string& b);
    // index: -1 で追加、0～ で置き換え。同じフォルダが既にあれば登録しない
    bool SetSongDirectory(int index, const std::string& value);
    void FolderRemove(int index);
#ifndef __ANDROID__
    static std::wstring PickFolderDialog(HWND owner, const std::wstring& initial);
#endif
    // target: -1 で追加、0～ でそのフォルダを置き換え
    void FolderBrowse(int target);
    void FolderPickFinished(const std::wstring& result, int target);

    // ---- レイアウト (描画と当たり判定で共通) ----
#ifndef __ANDROID__
    static constexpr const char* ConfigControlsText =
        "↑↓: 項目  ←→: 変更  Enter: 決定  Delete・右クリック: 既定値に戻す  Tab: カテゴリ  Esc: 戻る";
#else
    static constexpr const char* ConfigControlsText =
        "カッ (左右): 項目を移動  ドン: ON/OFF・決定・編集を開始/終了 (編集中はカッで値を変更)  上のタブ: カテゴリ  右上: 戻る";
#endif
    static constexpr int FolderParts = 2;       // 0: パス (表示のみ), 1: 削除
    static constexpr int FolderAddParts = 2;    // 0: 参照して追加, 1: パスを入力して追加
    static const char* FolderAddLabel(int part);

    const FontData& ConfigFont() const;
    // 値・ボタン・説明欄用の小さいフォント (スキンの文字のフォントの約 7 割の大きさ。入力欄やボタンに収まるように)
    mutable FontData ConfigSmall;
    mutable int ConfigSmallSource = -2;
    mutable float ConfigSmallSourceSize = 0;
    const FontData& ConfigSmallFont() const;
    float ConfigLineHeight() const;
    float ConfigTextWidth(const char* str) const;
    // 画面下の説明欄: 説明 2 行 + お知らせ 1 行 + 操作方法 (折り返した行数)
    float ConfigFooterHeight() const;
    float ConfigRowHeight() const;
    UIRect ConfigListArea() const;
    int ConfigVisibleRows() const;
    float ConfigRowCenter(int i) const;
    UIRect ConfigRowRect(int i) const;
    // 項目の箱 (スキンの Box 画像) の右端と高さ。値・ボタンはこの中に収める
    float ConfigRowRight() const;
    float ConfigBoxHeight() const;
    UIRect ConfigValueRect(int i) const;
    UIRect ConfigArrowRect(int i, bool right) const;
    int ConfigKeySlotCount(const ConfigItem& item) const;
    UIRect ConfigKeySlotRect(int i, int slot, int count) const;
    int ConfigPartCount(const ConfigItem& item) const;
    UIRect ConfigFolderPartRect(int i, ConfigItemType type, int part) const;
    // 文字入力欄の位置
    UIRect ConfigEditRect(int i) const;
    // タブの幅は名前の長さに合わせる (余った幅は等しく分け、足りない時は全体を縮める)
    UIRect ConfigTabRect(int i) const;
    // 戻るボタンは右上 (画面下は説明欄に使う)
    UIRect ConfigBackRect() const;

#ifdef __ANDROID__
    struct TouchTrack {
        bool Active = false, Dragged = false, Long = false;
        int StartX = 0, StartY = 0, LastY = 0, Accum = 0;
        std::chrono::steady_clock::time_point At{};
    } TouchState;
#endif
    PointerState ReadPointer();

    void UIText(const FontData& font, float x, float y, const std::string& str, unsigned int color, int align = 0);
    static void UIBox(const UIRect& r, unsigned int color, int alpha, bool fill = true);

    std::vector<ConfigItem>& ConfigItems();
    void ConfigEnsureVisible();
    void ConfigSetCategory(int index);
    void ConfigChanged();
    // フォルダの追加・削除で行の数が変わった時は、次のフレームの最初に行を作り直す
    // (操作中の項目への参照を壊さないため、変更した直後には作り直さない)
    void ConfigRebuildFoldersIfNeeded();
    void ConfigStep(ConfigItem& item, int dir);
    void ConfigBeginEdit(ConfigItem& item);
    void ConfigEndEdit(bool apply);
#ifndef __ANDROID__
    // キーを割り当てる。同じレーンで重複していれば移動し、他のレーンにあれば外す (1 つのキーで 2 レーン叩かれないように)
    void ConfigAssignKey(ConfigItem& item, int key);
    void ConfigRemoveKey(ConfigItem& item, int slot);
#endif
    void ConfigFolderPart(ConfigItem& item, int part);
    // 項目の決定操作 (Enter / ドン / クリック)
    void ConfigActivate(ConfigItem& item);
#ifndef __ANDROID__
    // WndProc が受け取ったキー状態から、前回からの間に押されたキーを集める (Raw Input が使えない環境でも動く)
    static std::vector<int> NewlyPressedKeys(byte (&prev)[256]);
#endif
    // ---- オフセット自動調整: 音声の出力遅延の目安から判定オフセットを決める ----
    // 叩く人は聞こえた音に合わせるので、音が遅れて出る分だけ判定を遅らせる (正の判定オフセット)。
    // 遅延 = ゲームの出力バッファ + デバイスの周期 (出力方式ごとの目安)
    // ---- オフセット自動調整: テスト譜面 (拍ごとにクリック音が鳴る) を演奏画面で叩き、ずれの平均だけ判定オフセットを動かす ----
    struct CalibrationState {
        bool Active = false;        // 調整の演奏中
        bool Finished = false;      // 最後まで叩いた
        HitErrorSummary Result;
        Scene ConfigPrev = Scene::ModeSelect;
        bool SavedAutoPlay = false, SavedTraining = false;
        std::string LastResult;     // 前回の調整の結果 (設定画面の値の欄に出す)
    } Calib;
    static constexpr int CalibMeasures = 10;        // 最初の 2 小節は叩かない (クリックだけ)
    // クリック音の WAV (48kHz・16bit・ステレオ)。BPM 120 で拍ごとに鳴らす
    static bool WriteCalibrationWav(const fs::path& path);
    void AutoAdjustOffset();
    // 設定画面に戻った時: 結果を判定オフセットに反映する
    void FinishCalibration();

    // ---- おすすめ設定の確認画面 ----
    void OpenRecommendPanel();
    UIRect RecommendPanelRect() const;
    UIRect RecommendButtonRect(int index) const;
    void ConfigRecommendProc(const PointerState& pt);
    void DrawRecommendPanel(const FontData& font);
    void ConfigMenuInit();
    void PrepareConfigSkin();
    void ConfigMenuEnd();

    void ConfigMenuDraw();

    void ConfigMenuProc();
#endif // FAS_GAME_MEMBER_DECLARATIONS
