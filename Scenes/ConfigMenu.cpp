#include "../Game.hpp"

void Game::ShowConfigMessage(std::string message, int seconds) {
    ConfigUI.Message = std::move(message);
    ConfigUI.MessageUntil = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
}

std::string Game::FormatNumber(double value, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    return buf;
}

size_t Game::Utf8CharLen(unsigned char c) {
    return c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
}

std::string Game::TextCacheKey(const std::string& str, int handle, float width, char mode) {
    std::string key;
    key.reserve(str.size() + 16);
    key += mode;
    key.append(reinterpret_cast<const char*>(&handle), sizeof(handle));
    const int w = (int)width;
    key.append(reinterpret_cast<const char*>(&w), sizeof(w));
    key += str;
    return key;
}

std::vector<std::string> Game::WrapText(const std::string& str, int handle, float width) const {
    std::string key = TextCacheKey(str, handle, width, 'w');
    if (auto it = TextWrapCache.find(key); it != TextWrapCache.end()) {
        return it->second;
    }
    if (TextWrapCache.size() > 512) { TextWrapCache.clear(); }

    std::vector<std::string> lines;
    if (handle == -1 || width <= 0) {
        lines.push_back(str);
    }
    else {
        auto fits = [&](const std::string& s) { return (float)GetStrlen(s, handle) <= width; };
        // 折り返してよい位置: 空白・「、。）」・「/」の後。行の 4 割より前なら文字の境目で折る (短すぎる行を作らない)
        auto breakable = [](const std::string& ch) {
            return ch == " " || ch == "/" || ch == ")" || ch == "、" || ch == "。" || ch == "）" || ch == "」" || ch == "・";
        };
        std::string line;
        size_t breakpos = std::string::npos;       // line 内で折り返せる位置 (その文字の直後)
        for (size_t pos = 0; pos < str.size();) {
            const size_t len = std::min(Utf8CharLen((unsigned char)str[pos]), str.size() - pos);
            const std::string ch = str.substr(pos, len);
            pos += len;
            if (ch == "\n") {
                lines.emplace_back(strtrim(line));
                line.clear();
                breakpos = std::string::npos;
                continue;
            }
            if (line.empty() && ch == " ") { continue; }
            if (!line.empty() && !fits(line + ch)) {
                if (breakpos != std::string::npos && breakpos >= line.size() * 2 / 5 && breakpos < line.size()) {
                    std::string rest(strtrim(std::string_view(line).substr(breakpos)));
                    line.resize(breakpos);
                    lines.emplace_back(strtrim(line));
                    line = std::move(rest);
                }
                else {
                    lines.emplace_back(strtrim(line));
                    line.clear();
                }
                breakpos = std::string::npos;
                if (ch == " ") { continue; }
            }
            line += ch;
            if (breakable(ch)) { breakpos = line.size(); }
        }
        if (!line.empty() || lines.empty()) { lines.emplace_back(strtrim(line)); }
    }
    return TextWrapCache.emplace(std::move(key), std::move(lines)).first->second;
}

void Game::DrawFitScreen(FontData& font, Pos2D<float> pos, const std::string& str) const {
    if (font.Handle == -1 || str.empty()) { return; }
    const float resx = (float)Skin.Info.Resolution.X, margin = 12.0f;
    float width = resx - pos.X - margin;
    if (font.Position == FontData::Right) { width = pos.X - margin; }
    else if (font.Position == FontData::Center) { width = 2.0f * std::min(pos.X, resx - pos.X) - margin * 2.0f; }
    const std::string text = FitText(str, font.Handle, std::max(width, 32.0f), false);
    font.Draw(pos, GetColor(255, 255, 255), GetColor(0, 0, 0), (unsigned int)GetStrlenCached(text, font.Handle), text);
}

std::string Game::FitText(const std::string& str, int handle, float width, bool front) const {
    if (handle == -1 || str.empty() || (float)GetStrlenCached(str, handle) <= width) {
        return str;
    }
    std::string key = TextCacheKey(str, handle, width, front ? 'f' : 'e');
    if (auto it = TextFitCache.find(key); it != TextFitCache.end()) {
        return it->second;
    }
    if (TextFitCache.size() > 512) { TextFitCache.clear(); }

    std::vector<size_t> starts;
    for (size_t pos = 0; pos < str.size(); pos += std::min(Utf8CharLen((unsigned char)str[pos]), str.size() - pos)) {
        starts.push_back(pos);
    }
    // 収まる最長の長さを二分探索で求める (幅は文字数に対して単調)
    const std::string ellipsis = "…";
    auto candidate = [&](size_t keep) {
        return front ? ellipsis + str.substr(starts[starts.size() - keep]) : str.substr(0, keep < starts.size() ? starts[keep] : str.size()) + ellipsis;
    };
    size_t lo = 0, hi = starts.size() - 1;
    while (lo < hi) {
        const size_t mid = (lo + hi + 1) / 2;
        if ((float)GetStrlen(candidate(mid), handle) <= width) { lo = mid; }
        else { hi = mid - 1; }
    }
    std::string result = lo == 0 ? ellipsis : candidate(lo);
    return TextFitCache.emplace(std::move(key), std::move(result)).first->second;
}

void Game::ConfigResetItem(ConfigItem& item) {
    if (!item.Reset) { return; }
    item.Reset();
    Skin.Base->Title.SE.Ka.Play();
    ConfigChanged();
    ShowConfigMessage("「" + item.Name + "」を既定値 (" + (item.Get ? item.Get() : std::string()) + ") に戻しました" + (item.Restart ? " (再起動後に反映されます)" : ""));
}

bool Game::ConfigArmed(const std::string& key) const {
    return ConfigUI.ArmedKey == key && std::chrono::steady_clock::now() < ConfigUI.ResetArmUntil;
}

bool Game::ConfigConfirm(const std::string& key, const std::string& message) {
    if (ConfigArmed(key)) { ConfigUI.ArmedKey.clear(); ConfigUI.ResetArmUntil = {}; return true; }
    ConfigUI.ArmedKey = key;
    ConfigUI.ResetArmUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    ShowConfigMessage(message);
    return false;
}

bool Game::ConfigResetCategory(size_t ci) {
    bool restart = false;
    if (ci >= ConfigUI.Categories.size()) { return false; }
    for (auto& item : ConfigUI.Categories[ci].Items) {
        if (item.Reset) { item.Reset(); restart |= item.Restart; }
    }
    return restart;
}

void Game::BuildConfigItems() {
    auto& cats = ConfigUI.Categories;
    cats.clear();

    auto Bool = [this](std::string name, std::string help, bool& v, bool restart = false) {
        ConfigItem it;
        it.Name = std::move(name);
        it.Help = std::move(help);
        it.Type = ConfigItemType::Bool;
        it.Restart = restart;
        it.Get = [&v] { return std::string(v ? "ON" : "OFF"); };
        it.Step = [&v](int) { v = !v; };
        it.Reset = MakeConfigReset(v);
        return it;
    };
    auto Number = [this](std::string name, std::string help, auto& v, double min, double max, double step, int decimals, bool restart = false) {
        using T = std::remove_reference_t<decltype(v)>;
        ConfigItem it;
        it.Name = std::move(name);
        it.Help = std::move(help);
        it.Type = ConfigItemType::Number;
        it.Restart = restart;
        it.Get = [&v, decimals] { return FormatNumber((double)v, decimals); };
        it.Reset = MakeConfigReset(v);
        auto assign = [&v, min, max](double value) {
            value = std::clamp(value, min, max);
            if constexpr (std::is_integral_v<T>) { v = (T)std::llround(value); }
            else { v = (T)value; }
        };
        it.Step = [&v, assign, step](int dir) {
            // 浮動小数の誤差が溜まらないよう段階の倍数に丸める
            assign(std::round(((double)v + step * dir) / step) * step);
        };
        it.Set = [assign](const std::string& str) {
            const std::string_view sv = strtrim(str);
            double value = 0;
            auto result = fast_float::from_chars(sv.data(), sv.data() + sv.size(), value);
            if (sv.empty() || result.ec != std::errc() || result.ptr != sv.data() + sv.size() || !std::isfinite(value)) {
                return false;
            }
            assign(value);
            return true;
        };
        return it;
    };
    // 整数の選択肢 (一覧に無い値が設定されていても、そのまま表示して近い選択肢へ移動できる)
    auto IntChoice = [this](std::string name, std::string help, int& v, std::vector<std::pair<int, std::string>> choices, bool restart = false) {
        ConfigItem it;
        it.Name = std::move(name);
        it.Help = std::move(help);
        it.Type = ConfigItemType::Choice;
        it.Restart = restart;
        it.Reset = MakeConfigReset(v);
        it.Get = [&v, choices] {
            auto found = std::ranges::find(choices, v, &std::pair<int, std::string>::first);
            return found != choices.end() ? found->second : std::to_string(v);
        };
        it.Step = [&v, choices](int dir) {
            const int n = (int)choices.size();
            auto found = std::ranges::find(choices, v, &std::pair<int, std::string>::first);
            int index = found != choices.end() ? (int)(found - choices.begin()) : (dir > 0 ? -1 : n);
            index = ((index + (dir > 0 ? 1 : -1)) % n + n) % n;
            v = choices[index].first;
        };
        return it;
    };
    auto Text = [this](std::string name, std::string help, std::string& v, size_t maxlength, bool allowempty = false) {
        ConfigItem it;
        it.Name = std::move(name);
        it.Help = std::move(help);
        it.Type = ConfigItemType::Text;
        it.Get = [&v] { return v; };
        it.Reset = MakeConfigReset(v);
        it.Set = [&v, maxlength, allowempty](const std::string& str) {
            std::string value(strtrim(str));
            if ((value.empty() && !allowempty) || value.size() > maxlength) { return false; }
            v = std::move(value);
            return true;
        };
        return it;
    };

    // ---- ゲーム ----
    ConfigCategory game{ "ゲーム" };
    game.Items.push_back(Text("PlayerName", "演奏画面やオンラインで表示される名前", Config.PlayerName, fasnet::MaxNameLength));
    game.Items.push_back(Text("TitleName", "名前の上に表示 (空欄で非表示)", Config.TitleName, fasnet::MaxNameLength, true));
    {
        ConfigItem skin;
        skin.Name = "SkinName";
        skin.Help = "使用するスキン (設定画面を閉じた時に読み込み直す)";
        skin.Type = ConfigItemType::Choice;
        skin.Get = [this] { return Config.SkinName; };
        skin.Step = [this](int dir) {
            if (Skin.Infos.empty()) { return; }
            const int n = (int)Skin.Infos.size();
            auto found = std::ranges::find(Skin.Infos, Config.SkinName, &_Skin::SkinInfo::Name);
            int index = found != Skin.Infos.end() ? (int)(found - Skin.Infos.begin()) : 0;
            index = ((index + (dir > 0 ? 1 : -1)) % n + n) % n;
            Config.SkinName = Skin.Infos[index].Name;
        };
        skin.Reset = MakeConfigReset(Config.SkinName);
        game.Items.push_back(std::move(skin));
    }
    auto& play = game;      // 演奏の設定も同じカテゴリ
    play.Items.push_back(Bool("AutoPlay", "譜面を自動で演奏する", Config.AutoPlay));
    play.Items.push_back(Bool("PauseDisplay", "オフライン演奏中に TAB / Esc でポーズ画面を開く (オンラインでは無効)", Config.PauseDisplay));
    play.Items.push_back(Bool("TrainingMode", "小節単位で移動しながら練習する", Config.TrainingMode));
    play.Items.push_back(Number("RandomRate", "音符の色を入れ替える割合 (%)", Config.RandomRate, 0, 100, 5, 0));
    play.Items.push_back(Number("ChartSpeed", "譜面のスクロール速度", Config.ChartSpeed, 0.1, 10.0, 0.1, 1));
    play.Items.push_back(Number("SongSpeed", "曲の再生速度", Config.SongSpeed, 0.5, 2.0, 0.05, 2));
    play.Items.push_back(Number("HiddenLevel", "判定枠の手前で音符を消す強さ", Config.HiddenLevel, 0.0, 10.0, 0.1, 1));
    play.Items.push_back(Number("SuddenLevel", "音符が判定枠の近くまで見えない強さ", Config.SuddenLevel, 0.0, 10.0, 0.1, 1));
    play.Items.push_back(Number("RollSpeed", "オートプレイ時の 1 秒あたりの連打数", Config.RollSpeed, 1, 60, 1, 0));
    play.Items.push_back(Bool("JiroChartMode", "太鼓さん次郎と同じ扱いにする (ダミーノーツ・疑似停止・分岐のタイミングなど)", Config.JiroChartMode));
#ifdef __ANDROID__
    play.Items.push_back(Number("TouchTaikoWidth", "タッチ太鼓の横幅", Config.TouchTaikoWidth, 100, 1500, 10, 0));
    play.Items.push_back(Number("TouchTaikoHeight", "タッチ太鼓の縦幅", Config.TouchTaikoHeight, 50, 1000, 10, 0));
#endif
    cats.push_back(std::move(game));

    // ---- 譜面フォルダ ----
    ConfigCategory folders{ FolderCategoryName };
    BuildFolderItems(folders.Items);
    cats.push_back(std::move(folders));

    // ---- 判定 ----
    ConfigCategory judge{ "判定" };
    judge.Items.push_back(Number("JudgeGood", "良になるずれの大きさ (ミリ秒)", Config.JudgeGood, 1, 200, 1, 0));
    judge.Items.push_back(Number("JudgeOk", "可になるずれの大きさ (ミリ秒)", Config.JudgeOk, 1, 300, 1, 0));
    judge.Items.push_back(Number("JudgeBad", "不可になるずれの大きさ (ミリ秒)", Config.JudgeBad, 1, 400, 1, 0));
    judge.Items.push_back(Number("JudgeOffset", "判定のタイミングをずらす (ミリ秒。正で遅く叩く)", Config.JudgeOffset, -500, 500, 1, 0));
    judge.Items.push_back(Number("SongOffset", "音源の再生タイミングをずらす (ミリ秒)", Config.SongOffset, -2000, 2000, 1, 0));
    judge.Items.push_back(Number("ChartOffset", "音符の時刻をずらす (ミリ秒。正で遅く)", Config.ChartOffset, -2000, 2000, 1, 0));
    {
        ConfigItem calib;
        calib.Name = "AutoOffset";
        calib.Help = "テスト譜面を演奏画面で叩き、クリック音とのずれの平均だけ判定オフセットを動かす";
        calib.Type = ConfigItemType::Action;
        // 調整した後は、その結果 (判定オフセットがどう変わったか) を値の欄に出しておく
        calib.Get = [this] { return Calib.LastResult.empty() ? std::string("テスト譜面で調整…") : Calib.LastResult; };
        calib.Step = [this](int) { AutoAdjustOffset(); };
        judge.Items.push_back(std::move(calib));
    }
    cats.push_back(std::move(judge));

    // ---- 表示 ----
    ConfigCategory view{ "画面と音" };
    {
        ConfigItem recommend;
        recommend.Name = "Recommended";
        recommend.Help = "この端末に合わせて表示とサウンドを設定する";
        recommend.Type = ConfigItemType::Action;
        recommend.Get = [] { return std::string("この端末に合わせて設定…"); };
        recommend.Step = [this](int) { OpenRecommendPanel(); };
        view.Items.push_back(std::move(recommend));
    }
    view.Items.push_back(Bool("ScoreMeter", "判定のずれをメーターで表示する", Config.ScoreMeter));
    view.Items.push_back(Bool("KeyMapView", "演奏中のキーマップを表示する (オート時は押下表示なし)", Config.KeyMapView));
    view.Items.push_back(Number("BGBrightness", "BGMOVIE の明るさ (0 で表示しない)", Config.BGBrightness, 0, 100, 5, 0));
    view.Items.push_back(Bool("HitNoteDisp", "叩いた音符を魂ゲージへ飛ばす演出", Config.HitNoteDisp));
    view.Items.push_back(Number("FrameRate", "目標 FPS (0: 制限なし / 垂直同期設定に従う、1～1000: 上限)。垂直同期 ON 時はモニターの上限にも従う", Config.FrameRate, 0, 1000, 1, 0));
    view.Items.push_back(Bool("FrameCountTimer", "描画フレームに合わせて譜面の時間を進める", Config.FrameCountTimer));
    view.Items.push_back(Number("FrameExtendRate", "フレーム単位の時間の細かさ", Config.FrameExtendRate, 0.1, 8.0, 0.1, 1));
    view.Items.push_back(Bool("FastInput", "描画を待たずに入力を処理する (垂直同期が ON の時のみ)", Config.FastInput));
    view.Items.push_back(Number("FastDrawRate", "高速入力時に垂直同期から描画を始めるまでの割合", Config.FastDrawRate, 0.0, 1.0, 0.05, 2));
#ifndef __ANDROID__
    view.Items.push_back(Bool("WaitVSync", "モニターのリフレッシュレートに合わせて描画する", Config.WaitVSync, true));
    view.Items.push_back(Bool("FullScreen", "全画面で起動する", Config.FullScreen, true));
    view.Items.push_back(Bool("MultiBoot", "複数のウィンドウで同時に起動できるようにする", Config.MultiBoot, true));
#endif
    view.Items.push_back(Bool("ViewDebug", "FPS や内部の時間を表示する", Config.ViewDebug));


    // ---- サウンド ----
    auto& sound = view;
    sound.Items.push_back(Number("SongVolume", "曲の音量 (%)", Config.SongVolume, 0, 100, 1, 0));
    sound.Items.push_back(Number("SEVolume", "ドン・カッなどの効果音の音量 (%)", Config.SEVolume, 0, 100, 1, 0));
#ifndef __ANDROID__
    sound.Items.push_back(IntChoice("SoundDeviceType", "音声の出力方式", Config.SoundDeviceType,
        { { 0, "既定" }, { 1, "WASAPI" }, { 2, "XAudio2" }, { 3, "MME (waveOut)" }, { 4, "ASIO" } }, true));
#endif
    sound.Items.push_back(Bool("Exclusive", "WASAPI を排他モードで使う (遅延が小さくなる)", Config.Exclusive, true));
    sound.Items.push_back(IntChoice("SampleRate", "出力のサンプリングレート", Config.SampleRate,
        { { 44100, "44100 Hz" }, { 48000, "48000 Hz" }, { 96000, "96000 Hz" }, { 192000, "192000 Hz" } }, true));
    sound.Items.push_back(Number("BufferSize", "小さいほど低遅延 (途切れる時は大きく)", Config.BufferSize, _ConfigData::MinBufferSize, 8192, 32, 0, true));
    // 24 を下限の追加段階とし、従来の 32 刻みは維持する。
    auto bufferStep = std::move(sound.Items.back().Step);
    sound.Items.back().Step = [this, bufferStep = std::move(bufferStep)](int dir) {
        if (dir > 0 && Config.BufferSize == _ConfigData::MinBufferSize) { Config.BufferSize = 32; }
        else { bufferStep(dir); }
    };
    cats.push_back(std::move(view));

    // ---- オンライン ----
    ConfigCategory online{ "オンライン" };
    online.Items.push_back(Text("ServerAddress", "MultiServer を起動している PC のアドレス", Config.ServerAddress, 255));
    online.Items.push_back(Number("ServerPort", "MultiServer の待ち受けポート", Config.ServerPort, 1, 65535, 1, 0));
#ifndef __ANDROID__
    online.Items.push_back(Bool("DiscordRPC", "Discord にプレイ中の曲を表示し、フレンドに参加ボタンを出す", Config.DiscordRPC));
#endif
    cats.push_back(std::move(online));

#ifndef __ANDROID__
    // ---- 外部ツール ----
    ConfigCategory toolcat{ "録画とツール" };
    // ---- 録画 ----
    auto& record = toolcat;
    record.Items.push_back(Bool("RecordPlay", "演奏を録画して recordings フォルダに保存する (ffmpeg が必要。リザルトで保存するか選ぶ)", Config.RecordPlay));
    {
        // ON にした時に ffmpeg が無ければ導入を提案する
        auto& item = record.Items.back();
        item.Step = [this, step = item.Step](int n) {
            step(n);
            if (Config.RecordPlay && !EnsureTool(ToolKind::Ffmpeg, "録画には ffmpeg が必要です。")) {
                Config.RecordPlay = false;
                Config.Write();
            }
        };
    }
    record.Items.push_back(IntChoice("RecordFps", "録画のフレームレート", Config.RecordFps, { { 30, "30 fps" }, { 60, "60 fps" }, { 120, "120 fps" } }));
    {
        // 保存する形式 (動画・音声のみ)
        ConfigItem it;
        it.Name = "RecordFormat";
        it.Help = "保存する形式 (MP4 / MKV / MOV / WebM / GIF)";
        it.Type = ConfigItemType::Choice;
        it.Get = [this] { return std::string(rec::FindFormat(Config.RecordFormat).Label); };
        it.Step = [this](int dir) {
            const int n = (int)std::size(rec::Formats);
            int index = (int)(&rec::FindFormat(Config.RecordFormat) - rec::Formats);
            index = ((index + (dir > 0 ? 1 : -1)) % n + n) % n;
            Config.RecordFormat = rec::Formats[index].Ext;
        };
        it.Reset = MakeConfigReset(Config.RecordFormat);
        record.Items.push_back(std::move(it));
    }
    record.Items.push_back(Number("RecordCrf", "録画の画質 (小さいほど高画質。libx264 の時)", Config.RecordCrf, 14, 32, 1, 0));
    record.Items.push_back(Text("RecordCodec", "映像のエンコーダ (libx264 / h264_nvenc / h264_qsv / h264_amf)", Config.RecordCodec, 32));
    for (ToolKind kind : { ToolKind::Ffmpeg, ToolKind::YtDlp }) {
        ConfigItem it;
        it.Name = ToolName(kind);
        it.Help = kind == ToolKind::Ffmpeg
            ? "録画・音源の変換に使う。配布元 (BtbN/FFmpeg-Builds, GPL) からダウンロードして導入・更新する"
            : "SONGURL / MOVIEURL のダウンロードに使う。配布元 (github.com/yt-dlp, Unlicense) からダウンロードして導入・更新する";
        it.Type = ConfigItemType::Action;
        it.Get = [kind] {
            const fs::path path = FindTool(kind);
            if (path.empty()) { return std::string("未導入 (選んで導入)"); }
            std::error_code ec;
            const bool local = fs::equivalent(path.parent_path(), GetExecutablePath(), ec);
            return std::string(local ? "導入済み (選んで更新)" : "PATH にあり (選ぶと導入)");
        };
        it.Step = [this, kind](int) {
            if (ConfirmToolInstall(kind, FindTool(kind).empty() ? "外部ツールを導入します。" : "最新版に更新します。")) {
                InstallTool(kind);
            }
        };
        toolcat.Items.push_back(std::move(it));
    }
    cats.push_back(std::move(toolcat));

    // ---- キー ----
    ConfigCategory keys{ "キー" };
    for (int lane = 0; lane < 4; ++lane) {
        ConfigItem it;
        it.Name = ConfigLaneKey(lane);
        it.Help = "枠を選んでキーを押す / 「+」で追加 / Delete・右クリックで外す";
        it.Type = ConfigItemType::Key;
        it.Index = lane;
        it.Keys = Config.KeyLanes(lane);
        if (it.Keys) { it.Reset = MakeConfigReset(*it.Keys); }
        keys.Items.push_back(std::move(it));
    }
    keys.Items.push_back(Number("KeyHoldProcInterval", "メニューでキーを押し続けた時の移動間隔 (秒)", Config.KeyHoldProcInterval, 0.02, 1.0, 0.01, 2));
    cats.push_back(std::move(keys));
#else
    ConfigCategory keys{ "キー" };
    ConfigItem external;
    external.Name = "外部入力の割り当て";
    external.Help = "おうち太鼓・キーボード・ゲームパッドのボタンや軸を各レーンへ割り当てる";
    external.Type = ConfigItemType::Action;
    external.Get = [] { return std::string("キー設定を開く…"); };
    external.Step = [](int) { AndroidConfigureExternalInput(); };
    keys.Items.push_back(std::move(external));
    cats.push_back(std::move(keys));
#endif
    // ---- リセット (右端のカテゴリ): カテゴリごと・すべての設定を既定値に戻す、キャッシュや記録を消す ----
    // 間違えて押さないよう、同じ項目をもう一度押した時だけ実行する
    ConfigCategory reset{ "リセット" };
    const size_t settingcats = cats.size();
    for (size_t ci = 0; ci < settingcats; ++ci) {
        if (std::ranges::none_of(cats[ci].Items, [](const ConfigItem& item) { return (bool)item.Reset; })) { continue; }
        ConfigItem it;
        it.Name = "Reset: " + cats[ci].Name;
        it.Help = "「" + cats[ci].Name + "」の項目をすべて既定値に戻す (1 つだけ戻す時は、その項目を選んで Delete か右クリック)";
        it.Type = ConfigItemType::Action;
        const std::string key = it.Name;
        it.Get = [this, key] { return std::string(ConfigArmed(key) ? "もう一度押すと戻します" : "既定値に戻す…"); };
        it.Step = [this, ci, key](int) {
            if (!ConfigConfirm(key, "もう一度押すと「" + ConfigUI.Categories[ci].Name + "」の項目をすべて既定値に戻します")) { return; }
            const bool restart = ConfigResetCategory(ci);
            ConfigChanged();
            ShowConfigMessage("「" + ConfigUI.Categories[ci].Name + "」の項目を既定値に戻しました" + (restart ? " (* の項目は再起動後に反映されます)" : ""));
        };
        reset.Items.push_back(std::move(it));
    }
    {
        ConfigItem it;
        it.Name = "Reset: すべて";
        it.Help = "すべてのカテゴリの項目を既定値に戻す (譜面フォルダは変えない)";
        it.Type = ConfigItemType::Action;
        const std::string key = it.Name;
        it.Get = [this, key] { return std::string(ConfigArmed(key) ? "もう一度押すと戻します" : "既定値に戻す…"); };
        it.Step = [this, settingcats, key](int) {
            if (!ConfigConfirm(key, "もう一度押すと、すべての項目を既定値に戻します")) { return; }
            bool restart = false;
            for (size_t ci = 0; ci < settingcats && ci < ConfigUI.Categories.size(); ++ci) { restart |= ConfigResetCategory(ci); }
            ConfigChanged();
            ShowConfigMessage(std::string("すべての項目を既定値に戻しました") + (restart ? " (* の項目は再起動後に反映されます)" : ""));
        };
        reset.Items.push_back(std::move(it));
    }
    // キャッシュ・一時ファイル・記録の削除
    struct ClearTarget { const char* Name; const char* Help; const char* Done; std::vector<const char*> Paths; int Kind; };
    const std::vector<ClearTarget> targets = {
        { "Clear: 譜面一覧", "譜面一覧のキャッシュ (chartcache.bin) を消して、譜面フォルダを読み直す", "譜面一覧のキャッシュを消して読み直しました", { "chartcache.bin" }, 1 },
        { "Clear: 一時ファイル", "ダウンロード・オフセット調整・テストプレイの一時ファイル (download / calibration / temp.tja) を消す", "一時ファイルを消しました", { "download", "calibration", "temp.tja" }, 0 },
        { "Clear: 演奏の記録", "すべての譜面のスコアと王冠の記録 (scoredata.json) を消す (元に戻せません)", "演奏の記録を消しました", { "scoredata.json" }, 2 },
    };
    for (const auto& t : targets) {
        ConfigItem it;
        it.Name = t.Name;
        it.Help = t.Help;
        it.Type = ConfigItemType::Action;
        const std::string key = it.Name;
        it.Get = [this, key] { return std::string(ConfigArmed(key) ? "もう一度押すと消します" : "消す…"); };
        it.Step = [this, key, t](int) {
            if (!ConfigConfirm(key, std::string("もう一度押すと消します: ") + t.Help)) { return; }
            std::error_code ec;
            size_t failed = 0;
            for (const char* name : t.Paths) {
                const fs::path path = GetExecutablePath() / name;
                if (!fs::exists(path, ec)) { continue; }
                fs::remove_all(path, ec);
                if (ec) { ++failed; }
            }
            if (t.Kind == 1) { EnumChart(Config.SongDirectories); }
            if (t.Kind == 2) { ScoreCache = json::object(); IsScoreCacheLoaded = true; }
            ShowConfigMessage(failed == 0 ? std::string(t.Done) : std::string("消せないファイルがありました (使用中の可能性があります)"));
        };
        reset.Items.push_back(std::move(it));
    }
    cats.push_back(std::move(reset));
}

const char* Game::ConfigLaneKey(int lane) {
    static const char* names[] = { "KaInputLeft", "DonInputLeft", "DonInputRight", "KaInputRight" };
    return lane >= 0 && lane < 4 ? names[lane] : "";
}

const char* Game::ConfigLaneName(int lane) {
    static const char* names[] = { "カッ (左)", "ドン (左)", "ドン (右)", "カッ (右)" };
    return lane >= 0 && lane < 4 ? names[lane] : "";
}

void Game::BuildFolderItems(std::vector<ConfigItem>& items) {
    items.clear();
    ConfigUI.FolderExists.clear();
    const auto& dirs = Config.SongDirectories;
    for (size_t i = 0; i < dirs.size(); ++i) {
        std::error_code ec;
        ConfigUI.FolderExists.push_back(fs::is_directory(ResolveSongDirectory(dirs[i]), ec) ? 1 : 0);
        ConfigItem it;
        it.Name = "SongDirectories[" + std::to_string(i) + "]";
        it.Help = "譜面を探すフォルダ (見つからない時は赤く表示)";
        it.Type = ConfigItemType::Folder;
        it.Index = (int)i;
        it.Get = [this, i] { return i < Config.SongDirectories.size() ? Config.SongDirectories[i] : std::string(); };
        items.push_back(std::move(it));
    }
    ConfigItem add;
    add.Name = "AddSongDirectory";
    add.Help = "譜面フォルダを追加する";
    add.Type = ConfigItemType::FolderAdd;
    add.Get = [] { return std::string(); };
    add.Set = [this](const std::string& str) { return SetSongDirectory(-1, str); };
    items.push_back(std::move(add));
}

bool Game::SameSongDirectory(const std::string& a, const std::string& b) {
    const fs::path pa = ResolveSongDirectory(a), pb = ResolveSongDirectory(b);
    std::error_code ec;
    const bool same = fs::equivalent(pa, pb, ec);
    return ec ? pa == pb : same;
}

bool Game::SetSongDirectory(int index, const std::string& value) {
    std::string dir(strtrim(value));
    // エクスプローラーの「パスのコピー」で付く前後の引用符を外す
    if (dir.size() >= 2 && dir.front() == '"' && dir.back() == '"') {
        dir = std::string(strtrim(std::string_view(dir).substr(1, dir.size() - 2)));
    }
    if (dir.empty()) {
        ShowConfigMessage("パスが空です");
        return false;
    }
    auto& dirs = Config.SongDirectories;
    if (index >= (int)dirs.size()) {
        return false;
    }
    for (int i = 0; i < (int)dirs.size(); ++i) {
        if (i != index && SameSongDirectory(dirs[i], dir)) {
            ShowConfigMessage("同じフォルダが既に登録されています");
            return false;
        }
    }
    std::error_code ec;
    if (!fs::is_directory(ResolveSongDirectory(dir), ec)) {
        ShowConfigMessage("注意: フォルダが見つかりません (登録はしました)");
    }
    if (index < 0) { dirs.push_back(std::move(dir)); }
    else { dirs[index] = std::move(dir); }
    ConfigUI.FoldersDirty = true;
    return true;
}

void Game::FolderRemove(int index) {
    auto& dirs = Config.SongDirectories;
    if (index < 0 || index >= (int)dirs.size()) { return; }
    if (dirs.size() <= 1) {
        ShowConfigMessage("譜面フォルダは 1 つ以上必要です (参照で別のフォルダに変更できます)");
        return;
    }
    dirs.erase(dirs.begin() + index);
    ConfigUI.FoldersDirty = true;
    Skin.Base->Title.SE.Ka.Play();
    ConfigChanged();
}

#ifndef __ANDROID__
std::wstring Game::PickFolderDialog(HWND owner, const std::wstring& initial) {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    struct ComScope { HRESULT Init; ~ComScope(){if(SUCCEEDED(Init))CoUninitialize();} } com{init};
    std::wstring result;
    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        const auto releaseDialog=[](IFileOpenDialog* p){p->Release();};
        std::unique_ptr<IFileOpenDialog,decltype(releaseDialog)> dialogGuard(dialog,releaseDialog);
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
        dialog->SetTitle(L"譜面フォルダを選択");   // 譜面フォルダを選択
        if (!initial.empty()) {
            IShellItem* folder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
                dialog->SetFolder(folder);
                folder->Release();
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


#endif
void Game::FolderBrowse(int target) {
#ifndef __ANDROID__
    if (Config.FullScreen) {
        ShowConfigMessage("フルスクリーンではエクスプローラーを開けません。パスを入力してください");
        return;
    }
    if (ConfigUI.Picker) {
        return;
    }
    std::wstring initial;
    {
        const auto& dirs = Config.SongDirectories;
        const fs::path start = target >= 0 && target < (int)dirs.size() ? ResolveSongDirectory(dirs[target]) : GetExecutablePath();
        std::error_code ec;
        if (fs::is_directory(start, ec)) { initial = start.wstring(); }
    }
    auto state = std::make_shared<FolderPickState>();
    ConfigUI.Picker = state;
    ConfigUI.PickerTarget = target;
    const HWND owner = GetMainWindowHandle();
    std::thread([state, owner, initial] {
        try { state->Result = PickFolderDialog(owner, initial); }
        catch (...) { state->Result.clear(); }
        state->Done = true;
        }).detach();
    Skin.Base->Title.SE.Don.Play();
#else
    (void)target;
    ShowConfigMessage("パスを入力してください");
#endif
}

void Game::FolderPickFinished(const std::wstring& result, int target) {
    if (result.empty() || target == -2) {
        return;
    }
    const fs::path picked = fs::path(result).lexically_normal();
    // 実行ファイルのフォルダの中なら相対パス、外なら絶対パスで登録する (行のボタンで切り替えられる)
    std::string value = ToRelativeSongDirectory(picked);
    if (const fs::path rel = U8ToPath(value); value.empty() || (!rel.empty() && *rel.begin() == "..")) {
        value = PathToU8(picked);
    }
    if (SetSongDirectory(target, value)) {
        Skin.Base->Title.SE.Don.Play();
        ConfigChanged();
    }
}

const char* Game::FolderAddLabel(int part) {
    static const char* labels[] = { "参照して追加…", "パスを入力…" };
    return part >= 0 && part < FolderAddParts ? labels[part] : "";
}

const FontData& Game::ConfigFont() const { return Skin.Base->ConfigMenu.Font.String; }

const FontData& Game::ConfigSmallFont() const {
    const FontData& base = ConfigFont();
    if (base.Handle != ConfigSmallSource || base.Size != ConfigSmallSourceSize) {
        ConfigSmall.Delete();
        ConfigSmallSource = base.Handle;
        ConfigSmallSourceSize = base.Size;
        const float size = std::max(12.0f, std::round((base.Size > 1 ? base.Size : 28.0f) * 0.7f));
        ConfigSmall.Size = size;
        ConfigSmall.Handle = CreateFontToHandle(nullptr, (int)size, 3, DX_FONTTYPE_ANTIALIASING_EDGE_8X8, DX_CHARSET_UTF8, 1);
    }
    return ConfigSmall.Handle != -1 ? ConfigSmall : base;
}

float Game::ConfigLineHeight() const {
    const float size = ConfigSmallFont().Size;
    return size > 1 ? size * 1.35f : 24.0f;
}

float Game::ConfigTextWidth(const char* str) const {
    const int handle = ConfigSmallFont().Handle;
    return handle != -1 ? (float)GetStrlenCached(str, handle) : 24.0f * (float)std::strlen(str) / 3.0f;
}

float Game::ConfigFooterHeight() const {
    const size_t controls = WrapText(ConfigControlsText, ConfigSmallFont().Handle, (float)Skin.Info.Resolution.X - 48.0f).size();
    return 12.0f + ConfigLineHeight() * (3.0f + (float)controls);
}

float Game::ConfigRowHeight() const {
    const float d = Skin.Base->ConfigMenu.Config.BoxDistance;
    return d > 8 ? d : 64.0f;
}

auto Game::ConfigListArea() const -> UIRect {
    const float top = 120.0f;
    const float bottom = std::max(top + ConfigRowHeight(), (float)Skin.Info.Resolution.Y - ConfigFooterHeight());
    return { 24.0f, top, (float)Skin.Info.Resolution.X - 24.0f, bottom };
}

int Game::ConfigVisibleRows() const {
    const UIRect area = ConfigListArea();
    return std::max(1, (int)((area.B - area.T) / ConfigRowHeight()));
}

float Game::ConfigRowCenter(int i) const {
    const float h = ConfigRowHeight();
    return ConfigListArea().T + (i - ConfigUI.Scroll) * h + h * 0.5f;
}

auto Game::ConfigRowRect(int i) const -> UIRect {
    const float cy = ConfigRowCenter(i);
    const float h = ConfigRowHeight();
    const UIRect area = ConfigListArea();
    return { area.L, cy - h * 0.5f, area.R, cy + h * 0.5f };
}

float Game::ConfigRowRight() const {
    const auto& box = Skin.Base->ConfigMenu.Image.Box;
    float right = ConfigListArea().R - 24.0f;
    if (box.Size.Width > 0) { right = std::min(right, box.Pos.X + box.Size.Width * 0.5f - 16.0f); }
    return right;
}

float Game::ConfigBoxHeight() const {
    const auto& box = Skin.Base->ConfigMenu.Image.Box;
    return box.Size.Height > 0 ? std::min(ConfigRowHeight(), box.Size.Height) : ConfigRowHeight();
}

auto Game::ConfigValueRect(int i) const -> UIRect {
    const float cy = ConfigRowCenter(i);
    const float h = ConfigBoxHeight() * 0.32f;
    float x = Skin.Base->ConfigMenu.Config.ValPos.X;
    if (x <= 0) { x = Skin.Info.Resolution.X * 0.5f; }
    const float right = ConfigRowRight();
    x = std::min(x, right - 120.0f);
    return { x, cy - h, std::min(x + 420.0f, right), cy + h };
}

auto Game::ConfigArrowRect(int i, bool right) const -> UIRect {
    const UIRect v = ConfigValueRect(i);
    return right ? UIRect{ v.R - 48, v.T, v.R, v.B } : UIRect{ v.L, v.T, v.L + 48, v.B };
}

int Game::ConfigKeySlotCount(const ConfigItem& item) const {
#ifndef __ANDROID__
    if (item.Keys == nullptr) { return 0; }
    const int n = (int)item.Keys->size();
    return n + (n < (int)_ConfigData::MaxKeysPerLane ? 1 : 0);   // 末尾は「+」(追加) の枠
#else
    (void)item;
    return 0;
#endif
}

auto Game::ConfigKeySlotRect(int i, int slot, int count) const -> UIRect {
    const UIRect v = ConfigValueRect(i);
    const float right = ConfigRowRight();
    const float w = std::min(132.0f, (right - v.L) / (float)std::max(count, 1));
    return { v.L + w * slot, v.T, v.L + w * (slot + 1) - 6, v.B };
}

int Game::ConfigPartCount(const ConfigItem& item) const {
    switch (item.Type) {
    case ConfigItemType::Key: return ConfigKeySlotCount(item);
    case ConfigItemType::Folder: return FolderParts;
    case ConfigItemType::FolderAdd: return FolderAddParts;
    default: return 1;
    }
}

auto Game::ConfigFolderPartRect(int i, ConfigItemType type, int part) const -> UIRect {
    const UIRect v = ConfigValueRect(i);
    const float left = v.L;
    const float right = ConfigRowRight();
    if (type == ConfigItemType::FolderAdd) {
        // 2 つのボタンを箱の幅に収まるように並べる
        const float half = (right - left - 8.0f) * 0.5f;
        const float w0 = std::min(ConfigTextWidth(FolderAddLabel(0)) + 40.0f, half);
        const float w1 = std::min(ConfigTextWidth(FolderAddLabel(1)) + 40.0f, right - left - 8.0f - w0);
        if (part == 0) { return { left, v.T, left + w0, v.B }; }
        return { left + w0 + 8.0f, v.T, left + w0 + 8.0f + w1, v.B };
    }
    const float w = ConfigTextWidth("削除") + 32.0f;
    if (part == 1) { return { right - w, v.T, right, v.B }; }
    return { left, v.T, right - w - 8.0f, v.B };   // パス欄
}

auto Game::ConfigEditRect(int i) const -> UIRect {
    if (i < 0 || i >= (int)ConfigUI.Categories[ConfigUI.Category].Items.size()) { return ConfigValueRect(std::max(i, 0)); }
    const ConfigItemType type = ConfigUI.Categories[ConfigUI.Category].Items[i].Type;
    if (type == ConfigItemType::Folder) { return ConfigFolderPartRect(i, type, 0); }
    if (type == ConfigItemType::FolderAdd) {
        const UIRect first = ConfigFolderPartRect(i, type, 0);
        return { first.L, first.T, ConfigRowRight(), first.B };
    }
    return ConfigValueRect(i);
}

auto Game::ConfigTabRect(int i) const -> UIRect {
    const int n = std::max(1, (int)ConfigUI.Categories.size());
    const int handle = ConfigSmallFont().Handle;
    const float avail = Skin.Info.Resolution.X - 48.0f;
    std::vector<float> w(n, avail / n);
    float total = 0;
    for (int k = 0; k < n && k < (int)ConfigUI.Categories.size(); ++k) {
        w[k] = (handle != -1 ? (float)GetStrlenCached(ConfigUI.Categories[k].Name, handle) : 0.0f) + 30.0f;
        total += w[k];
    }
    for (auto& x : w) { x = total < avail ? x + (avail - total) / n : x * avail / total; }
    float l = 24.0f;
    for (int k = 0; k < i && k < n; ++k) { l += w[k]; }
    const float r = l + (i >= 0 && i < n ? w[i] : 0.0f);
    return { l, 52.0f, r - 6.0f, 100.0f };
}

auto Game::ConfigBackRect() const -> UIRect {
    return { (float)Skin.Info.Resolution.X - 184.0f, 6.0f, (float)Skin.Info.Resolution.X - 24.0f, 46.0f };
}

auto Game::ReadPointer() -> PointerState {
    PointerState pt;
    bool right = false, middle = false;
#ifndef __ANDROID__
    GetMousePoint(&pt.X, &pt.Y);
    // タイトルバーをドラッグしてウィンドウを動かしている間は、クリックとして扱わない
    if (!IsWindowDragging()) {
        const int buttons = GetMouseInput();
        pt.Left = (buttons & MOUSE_INPUT_LEFT) != 0;
        right = (buttons & MOUSE_INPUT_RIGHT) != 0;
        middle = (buttons & MOUSE_INPUT_MIDDLE) != 0;
    }
    pt.Wheel = Input::GetInstance().GetMouseWheel();
#else
    // タッチ: 指を離した時に (動かしていなければ) タップ = 左クリック。指を動かすとスクロール (ホイール)、
    // 動かさずに長く押すと右クリック (消す・外す)
    pt.X = PointerPrev.X;
    pt.Y = PointerPrev.Y;
    auto& t = TouchState;
    if (GetTouchInputNum() > 0) {
        GetTouchInput(0, &pt.X, &pt.Y, nullptr, nullptr);
        const auto now = std::chrono::steady_clock::now();
        if (!t.Active) {
            t = TouchTrack{};
            t.Active = true;
            t.StartX = pt.X; t.StartY = pt.Y; t.LastY = pt.Y;
            t.At = now;
        }
        const int slop = std::max(16, Skin.Info.Resolution.Y / 40);
        if (std::abs(pt.X - t.StartX) > slop || std::abs(pt.Y - t.StartY) > slop) { t.Dragged = true; }
        if (t.Dragged && NowScene != Scene::ConfigMenu) {
            const int step = NowScene == Scene::SongSelect || NowScene == Scene::DanSelect
                ? std::max(24, (int)std::lround(std::abs(NowScene == Scene::DanSelect ? Skin.Base->DanSelect.Config.BoxDistance.Y : IsCourseSelect ? Skin.Base->SongSelect.Config.CourseBoxDistance : Skin.Base->SongSelect.Config.BoxDistance.Y)))
                : std::max(24, Skin.Info.Resolution.Y / 18);
            t.Accum += pt.Y - t.LastY;
            t.LastY = pt.Y;
            while (t.Accum >= step) { ++pt.Wheel; t.Accum -= step; }      // 下へ動かす = 上へ戻る
            while (t.Accum <= -step) { --pt.Wheel; t.Accum += step; }
        }
        else if (!t.Long && now - t.At > std::chrono::milliseconds(550)) {
            t.Long = true;
            right = true;
        }
    }
    else if (t.Active) {
        t.Active = false;
        if (!t.Dragged && !t.Long) { pt.Left = true; }
    }
#endif
    pt.LeftPressed = pt.Left && !PointerPrev.Left;
    pt.RightPressed = right && !PointerPrev.Right;
    pt.MiddlePressed = middle && !PointerPrev.Middle;
    pt.Moved = pt.X != PointerPrev.X || pt.Y != PointerPrev.Y;
    PointerPrev.Left = pt.Left;
    PointerPrev.Right = right;
    PointerPrev.Middle = middle;
    PointerPrev.X = pt.X;
    PointerPrev.Y = pt.Y;
    return pt;
}

void Game::UIText(const FontData& font, float x, float y, const std::string& str, unsigned int color, int align) {
    if (font.Handle == -1 || str.empty()) { return; }
    const float w = (float)GetStrlenCached(str, font.Handle);
    const float dx = align == 1 ? w * 0.5f : (align == 2 ? w : 0.0f);
    DrawStringFToHandle(x - dx, y - font.Size * 0.5f, str.c_str(), color, font.Handle, GetColor(0, 0, 0));
}

void Game::UIBox(const UIRect& r, unsigned int color, int alpha, bool fill) {
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
    DrawBoxAA(r.L, r.T, r.R, r.B, color, fill ? TRUE : FALSE, 2.0f);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

auto Game::ConfigItems() -> std::vector<ConfigItem>& {
    return ConfigUI.Categories[ConfigUI.Category].Items;
}

void Game::ConfigEnsureVisible() {
    const int n = (int)ConfigItems().size();
    const int vis = ConfigVisibleRows();
    ConfigUI.Selected = std::clamp(ConfigUI.Selected, 0, std::max(n - 1, 0));
    if (ConfigUI.Selected < ConfigUI.Scroll) { ConfigUI.Scroll = ConfigUI.Selected; }
    if (ConfigUI.Selected >= ConfigUI.Scroll + vis) { ConfigUI.Scroll = ConfigUI.Selected - vis + 1; }
    ConfigUI.Scroll = std::clamp(ConfigUI.Scroll, 0, std::max(n - vis, 0));
}

void Game::ConfigSetCategory(int index) {
    ConfigUI.TaikoEditing = false;
    const int n = (int)ConfigUI.Categories.size();
    ConfigUI.Category = ((index % n) + n) % n;
    ConfigUI.Selected = 0;
    ConfigUI.Scroll = 0;
    ConfigUI.KeySlot = 0;
}

void Game::ConfigChanged() {
    Config.Write();
}

void Game::ConfigRebuildFoldersIfNeeded() {
    if (!ConfigUI.FoldersDirty) { return; }
    ConfigUI.FoldersDirty = false;
    for (auto& cat : ConfigUI.Categories) {
        if (cat.Name == FolderCategoryName) {
            BuildFolderItems(cat.Items);
        }
    }
    ConfigEnsureVisible();
    ConfigUI.KeySlot = std::clamp(ConfigUI.KeySlot, 0, std::max(ConfigPartCount(ConfigItems()[ConfigUI.Selected]) - 1, 0));
}

void Game::ConfigStep(ConfigItem& item, int dir) {
    if (!item.Step) { return; }
    item.Step(dir);
    Skin.Base->Title.SE.Ka.Play();
    ConfigChanged();
}

void Game::ConfigBeginEdit(ConfigItem& item) {
    if (!item.Set) { return; }
    Skin.Base->Title.SE.Don.Play();
#ifndef __ANDROID__
    if (ConfigUI.Editing) { DeleteKeyInput(ConfigUI.EditHandle); ConfigUI.Editing = false; }
    ConfigUI.EditHandle = MakeKeyInput(1024, TRUE, FALSE, FALSE);
    SetKeyInputString(item.Get().c_str(), ConfigUI.EditHandle);
    SetActiveKeyInput(ConfigUI.EditHandle);
    SetKeyInputStringFont(ConfigSmallFont().Handle);
    ConfigUI.Editing = true;
#else
    if (item.Set(GetInputText(item.Name))) {
        ConfigChanged();
    }
#endif
}

void Game::ConfigEndEdit(bool apply) {
#ifndef __ANDROID__
    if (!ConfigUI.Editing) { return; }
    if (apply) {
        std::vector<char> buf(4096, '\0');
        GetKeyInputString(buf.data(), ConfigUI.EditHandle);
        auto& item = ConfigItems()[ConfigUI.Selected];
        const auto before = ConfigUI.MessageUntil;
        if (item.Set && item.Set(buf.data())) {
            ConfigChanged();
        }
        else if (item.Set && ConfigUI.MessageUntil == before) {
            ShowConfigMessage("入力した値は使えません (長すぎる・空欄・数値でないなど)");
        }
    }
    DeleteKeyInput(ConfigUI.EditHandle);
    ConfigUI.EditHandle = 0;
    ConfigUI.Editing = false;
#endif
}

#ifndef __ANDROID__
void Game::ConfigAssignKey(ConfigItem& item, int key) {
    if (item.Keys == nullptr || key <= 0 || key >= 256) { return; }
    auto& keys = *item.Keys;
    int slot = std::clamp(ConfigUI.KeySlot, 0, (int)keys.size());
    if (slot < (int)keys.size() && keys[slot] == key) { return; }

    std::string movedfrom;
    for (int lane = 0; lane < 4; ++lane) {
        auto* other = Config.KeyLanes(lane);
        if (other != item.Keys && std::erase(*other, key) > 0) {
            movedfrom = ConfigLaneName(lane);
        }
    }
    if (auto it = std::ranges::find(keys, key); it != keys.end()) {
        const int from = (int)(it - keys.begin());
        keys.erase(it);
        if (from < slot) { --slot; }
    }
    if (slot < (int)keys.size()) {
        keys[slot] = key;
    }
    else if (keys.size() < _ConfigData::MaxKeysPerLane) {
        keys.push_back(key);
    }
    if (!movedfrom.empty()) {
        const auto names = Config.GetKeyName({ key });
        ShowConfigMessage("「" + (names.empty() ? std::string("?") : names[0]) + "」を" + movedfrom + "から外しました");
    }
    Skin.Base->Title.SE.Don.Play();
    ConfigChanged();
}

void Game::ConfigRemoveKey(ConfigItem& item, int slot) {
    if (item.Keys == nullptr || slot < 0 || slot >= (int)item.Keys->size()) { return; }
    item.Keys->erase(item.Keys->begin() + slot);
    Skin.Base->Title.SE.Ka.Play();
    ConfigChanged();
}

#endif
void Game::ConfigFolderPart(ConfigItem& item, int part) {
    if (item.Type == ConfigItemType::FolderAdd) {
        if (part == 0) { FolderBrowse(-1); }
        else { ConfigBeginEdit(item); }
        return;
    }
    if (part == 1) { FolderRemove(item.Index); }
}

void Game::ConfigActivate(ConfigItem& item) {
    switch (item.Type) {
    case ConfigItemType::Bool:
    case ConfigItemType::Choice:
        ConfigStep(item, 1);
        break;
    case ConfigItemType::Number:
    case ConfigItemType::Text:
        ConfigBeginEdit(item);
        break;
    case ConfigItemType::Key:
        Skin.Base->Title.SE.Don.Play();
        ConfigUI.Capturing = true;
#ifndef __ANDROID__
        std::copy(std::begin(__gotkeyboardinput), std::end(__gotkeyboardinput), ConfigUI.CapturePrev);
#endif
        break;
    case ConfigItemType::Folder:
    case ConfigItemType::FolderAdd:
        ConfigFolderPart(item, ConfigUI.KeySlot);
        break;
    case ConfigItemType::Action:
        if (item.Step) {
            Skin.Base->Title.SE.Don.Play();
            item.Step(1);
        }
        break;
    }
}

#ifndef __ANDROID__
std::vector<int> Game::NewlyPressedKeys(byte (&prev)[256]) {
    std::vector<int> out;
    for (int key = 1; key < 256; ++key) {
        const bool down = __gotkeyboardinput[key] != 0;
        if (down && !prev[key]) { out.push_back(key); }
        prev[key] = down ? 1 : 0;
    }
    return out;
}

#endif
double Game::EstimateOutputLatencyMs(int type, bool exclusive, int buffer, int rate, const DeviceInfo& dev) {
    if (rate <= 0) { rate = 48000; }
    const double buffer_ms = std::max(buffer, 0) * 1000.0 / rate;
    const double period_ms = dev.AudioOk && dev.MixRate > 0 && dev.PeriodSamples > 0 ? dev.PeriodSamples * 1000.0 / dev.MixRate : 10.0;
    switch (type) {
    case 0:                                                           // 既定 (SoundDevice::SetDevice では WASAPI)
    case 1: return exclusive ? buffer_ms : buffer_ms + period_ms;     // WASAPI (共有モードはミキサーの周期が加わる)
    case 4: return buffer_ms;                                         // ASIO
    case 3: return buffer_ms * 2.0;                                   // MME (2 つのバッファを交互に使う)
    default: return period_ms * 2.0;                                  // XAudio2
    }
}

double Game::RecommendedJudgeOffset(const DeviceInfo& dev, int type, bool exclusive, int buffer, int rate) const {
    return std::clamp(std::round(EstimateOutputLatencyMs(type, exclusive, buffer, rate, dev)), 0.0, 500.0);
}

bool Game::WriteCalibrationWav(const fs::path& path) {
    constexpr int rate = 48000;
    const double beatms = 500.0;
    const int beats = CalibMeasures * 4;
    const size_t frames = (size_t)((beats * beatms / 1000.0 + 2.0) * rate);
    std::vector<int16_t> pcm(frames * 2, 0);
    for (int b = 0; b < beats; ++b) {
        const size_t start = (size_t)(b * beatms / 1000.0 * rate);
        const double freq = b % 4 == 0 ? 1600.0 : 1000.0;      // 小節の頭は高い音
        for (size_t i = 0; i < (size_t)(0.04 * rate) && start + i < frames; ++i) {
            const double t = (double)i / rate;
            const double v = std::sin(2.0 * 3.14159265358979 * freq * t) * std::exp(-t * 90.0) * 0.8;
            pcm[(start + i) * 2] = pcm[(start + i) * 2 + 1] = (int16_t)(v * 32767.0);
        }
    }
    std::ofstream out(path, std::ios::binary);
    auto u32 = [&](uint32_t v) { out.write((const char*)&v, 4); };
    auto u16 = [&](uint16_t v) { out.write((const char*)&v, 2); };
    const uint32_t bytes = (uint32_t)(pcm.size() * 2);
    out.write("RIFF", 4); u32(36 + bytes); out.write("WAVE", 4);
    out.write("fmt ", 4); u32(16); u16(1); u16(2); u32(rate); u32(rate * 4); u16(4); u16(16);
    out.write("data", 4); u32(bytes);
    out.write((const char*)pcm.data(), bytes);
    return (bool)out;
}

void Game::AutoAdjustOffset() {
    std::error_code ec;
    const fs::path dir = GetExecutablePath() / "calibration";
    fs::create_directories(dir, ec);
    if (!WriteCalibrationWav(dir / "click.wav")) { ShowConfigMessage("テスト用の音を作れませんでした"); return; }
    std::string tja = "TITLE:オフセット調整\r\nSUBTITLE:--クリック音に合わせて叩いてください\r\nBPM:120\r\nWAVE:click.wav\r\nOFFSET:0\r\nCOURSE:Oni\r\nLEVEL:1\r\n#START\r\n0,\r\n0,\r\n";
    for (int m = 2; m < CalibMeasures; ++m) { tja += "1111,\r\n"; }
    tja += "#END\r\n";
    Calib = CalibrationState{};
    Calib.Active = true;
    Calib.ConfigPrev = PrevScene;
    Calib.SavedAutoPlay = Config.AutoPlay;
    Calib.SavedTraining = Config.TrainingMode;
    Config.AutoPlay = false;
    Config.TrainingMode = false;
    Editor.Testing = true;
    Editor.Path = dir / "calibration.tja";
    Editor.TestText = tja;
    Editor.TestMeasure = 0;
    CourseIndex = (size_t)CourseType::Oni;
    Training.Init();
    Skin.Base->Title.SE.Don.Play();
    PrevScene = Scene::ConfigMenu;
    NowScene = Scene::Loading;
}

void Game::FinishCalibration() {
    if (!Calib.Active) { return; }
    Calib.Active = false;
    Editor.Testing = false;
    Editor.TestText.clear();
    Config.AutoPlay = Calib.SavedAutoPlay;
    Config.TrainingMode = Calib.SavedTraining;
    Training.Init();
    Chart.Init(true);
    PrevScene = Calib.ConfigPrev;
    if (!Calib.Finished) { ShowConfigMessage("オフセット調整をやめました"); return; }
    if (Calib.Result.Count < 12) { ShowConfigMessage("叩いた数が少ないため、判定オフセットを変えませんでした (" + std::to_string(Calib.Result.Count) + " 回)"); return; }
    const double before = Config.JudgeOffset;
    Config.JudgeOffset = std::clamp(std::round(before + Calib.Result.Mean), -500.0, 500.0);
    ConfigChanged();
    Calib.LastResult = "判定 " + FormatSigned(before, 0) + " → " + FormatSigned(Config.JudgeOffset, 0) + "ms";
    // 調整で変わるのは判定オフセットだけ。ほかのオフセットも並べて、今の値が分かるようにする
    ShowConfigMessage("JudgeOffset " + FormatSigned(before, 0) + " → " + FormatSigned(Config.JudgeOffset, 0) + "ms  SongOffset " + FormatSigned(Config.SongOffset, 0)
        + "ms  ChartOffset " + FormatSigned(Config.ChartOffset, 0) + "ms (平均 " + FormatSigned(Calib.Result.Mean, 1) + "ms ±" + FormatNumber(Calib.Result.StdDev, 1)
        + "ms・" + std::to_string(Calib.Result.Count) + " 回)", 15);
}

void Game::OpenRecommendPanel() {
    ConfigUI.Recommend = BuildRecommendations();
    if (ConfigUI.Recommend.empty()) {
        ShowConfigMessage("すでにこの端末へのおすすめの設定になっています");
        return;
    }
    ConfigUI.RecommendOpen = true;
    PointerPrev.Left = true;
}

auto Game::RecommendPanelRect() const -> UIRect {
    const float x = (float)Skin.Info.Resolution.X, y = (float)Skin.Info.Resolution.Y;
    return { x * 0.15f, y * 0.12f, x * 0.85f, y * 0.88f };
}

auto Game::RecommendButtonRect(int index) const -> UIRect {
    const UIRect r = RecommendPanelRect();
    const float w = std::min(200.0f, (r.R-r.L-60.0f)*0.5f);
    const float cx = (r.L + r.R) * 0.5f + (index == 0 ? -w * 0.5f - 12.0f : w * 0.5f + 12.0f);
    return { cx - w * 0.5f, r.B - 66.0f, cx + w * 0.5f, r.B - 18.0f };
}

void Game::ConfigRecommendProc(const PointerState& pt) {
    auto Apply = [&] {
        bool restart = false;
        for (auto& change : ConfigUI.Recommend) {
            change.Apply();
            restart = restart || change.Restart;
        }
        Config.Write();
        SetWaitVSyncFlag(Config.WaitVSync);
        Skin.Base->Title.SE.Don.Play();
        ShowConfigMessage(std::to_string(ConfigUI.Recommend.size()) + " 項目を変更しました" + (restart ? " (* の項目は再起動後に反映されます)" : ""));
        ConfigUI.RecommendOpen = false;
        ConfigUI.Recommend.clear();
    };
    auto Cancel = [&] {
        Skin.Base->Title.SE.Ka.Play();
        ConfigUI.RecommendOpen = false;
        ConfigUI.Recommend.clear();
    };
    if (pt.LeftPressed && RecommendButtonRect(0).Contains(pt.X, pt.Y)) { Apply(); return; }
    if (pt.LeftPressed && RecommendButtonRect(1).Contains(pt.X, pt.Y)) { Cancel(); return; }
#ifndef __ANDROID__
    Input::GetInstance().HitKeyProcess(VK_RETURN, KeyState::Down, Apply);
    if (!ConfigUI.RecommendOpen) { return; }
    Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, Cancel);
#else
    Touch.Process(TouchType::Other, Cancel, Skin.Base->ConfigMenu.Image.Back);
#endif
}

void Game::DrawRecommendPanel(const FontData& font) {
    const unsigned int white = GetColor(255, 255, 255), black = GetColor(0, 0, 0), accent = GetColor(255, 200, 40);
    const UIRect r = RecommendPanelRect();
    UIBox({ 0, 0, (float)Skin.Info.Resolution.X, (float)Skin.Info.Resolution.Y }, black, 130);
    UIBox(r, GetColor(18, 20, 32), 240);
    UIBox(r, white, 200, false);
    const float lh = ConfigLineHeight();
    float y = r.T + 20.0f + lh * 0.5f;
    UIText(font, r.L + 24, y, FitText("この端末へのおすすめ設定 (次の項目を変更します)",font.Handle,r.R-r.L-48,false), white);
    y += lh * 1.4f;
    const float limit = r.B - 90.0f;
    for (const auto& change : ConfigUI.Recommend) {
        if (y > limit) { UIText(font, r.L + 40, y, "…", white); break; }
        UIText(font, r.L + 40, y, FitText(change.Name + (change.Restart ? " *" : "") + ":  " + change.Before + "  →  " + change.After, font.Handle, r.R - r.L - 64, false), white);
        y += lh;
    }
#ifdef __ANDROID__
    const char* labels[] = { "適用", "キャンセル" };
#else
    const char* labels[] = { "適用 (Enter)", "キャンセル (Esc)" };
#endif
    for (int i = 0; i < 2; ++i) {
        const UIRect b = RecommendButtonRect(i);
        UIBox(b, i == 0 ? GetColor(214, 116, 20) : GetColor(50, 54, 70), 230);
        UIBox(b, white, 160, false);
        UIText(font, (b.L + b.R) * 0.5f, (b.T + b.B) * 0.5f, labels[i], white, 1);
    }
}

void Game::ConfigMenuInit() {
    BuildConfigItems();
    ConfigUI.Category = 0;
    ConfigUI.Selected = 0;
    ConfigUI.Scroll = 0;
    ConfigUI.KeySlot = 0;
    ConfigUI.Editing = false;
    ConfigUI.Capturing = false;
    ConfigUI.FoldersDirty = false;
    ConfigUI.RecommendOpen = false;
    ConfigUI.Recommend.clear();
    ConfigUI.Message.clear();
    ConfigUI.SkinOnEnter = Config.SkinName;
    FinishCalibration();
    ConfigUI.DirsOnEnter = Config.SongDirectories;
    ConfigUI.NameOnEnter = Config.PlayerName;
    ConfigUI.TitleOnEnter = Config.TitleName;
    PointerPrev.Left = true;       // 画面を開いたクリックを項目の操作として扱わない
#ifndef __ANDROID__
    SetMouseDispFlag(TRUE);
#endif
}

void Game::PrepareConfigSkin() {
    // 戻る先へ切り替える前に、画像・音声・フォントをすべて確定する。
    Skin.Init(Skin.Root);
    if (std::ranges::find(Skin.Infos,Config.SkinName,&_Skin::SkinInfo::Name)==Skin.Infos.end()) { Config.SkinName="Default"; }
    if (Config.SkinName==ConfigUI.SkinOnEnter && Skin.Info.Name==Config.SkinName) { return; }
    ClearDrawScreen(); DrawString(24,24,"スキンを読み込んでいます...",GetColor(255,255,255)); ScreenFlip();
    Skin.Load(Config.SkinName, Config);
    WaitHandleASyncLoadAll();
    FinishSkinReload();
    ConfigUI.SkinOnEnter=Config.SkinName;
    Config.Write();
#ifndef __ANDROID__
    RawKeyboard::GetInstance().Drain(KeyEvents); KeyEvents.clear();
    Input::GetInstance().ResyncKeys();
#endif
    PointerPrev.Left=PointerPrev.Right=PointerPrev.Middle=true;
    // 読み込み前の説明欄や別解像度の画面を、戻る先の初回描画に残さない。
    ClearDrawScreen();
}

void Game::ConfigMenuEnd() {
    ConfigEndEdit(false);
    ConfigUI.Capturing = false;
#ifndef __ANDROID__
    // 開いたままのフォルダ選択ダイアログの結果は捨てる (ダイアログを閉じるまで次の選択は開かない)
    if (ConfigUI.Picker) { ConfigUI.PickerTarget = PickerDiscard; }
#endif
    PrepareConfigSkin();
    Config.Write();
    if (Config.SongDirectories != ConfigUI.DirsOnEnter) {
        EnumChart(Config.SongDirectories);
    }
    if (IsMulti && Config.PlayerName != ConfigUI.NameOnEnter) {
        Online.Net.Send(fasnet::Msg::SetName, fasnet::Text{ Config.PlayerName });
    }
#ifndef __ANDROID__
    DiscordRpc::GetInstance().Start(DiscordAppId());
#endif
    if (IsMulti && Config.TitleName != ConfigUI.TitleOnEnter) {
        Online.Net.Send(fasnet::Msg::SetTitle, fasnet::Text{ Config.TitleName });
    }
#ifndef __ANDROID__
    SetMouseDispFlag(!Config.FullScreen);
#endif
    ConfigUI.Categories.clear();
}

void Game::ConfigMenuDraw() {
    if (ConfigUI.Categories.empty()) {
        return;
    }
    auto& font = Skin.Base->ConfigMenu.Font.String;
    auto& sfont = ConfigSmallFont();
    const unsigned int white = GetColor(255, 255, 255);
    const unsigned int black = GetColor(0, 0, 0);
    const unsigned int gray = GetColor(150, 150, 150);
    const unsigned int accent = GetColor(255, 200, 40);
    const float resx = (float)Skin.Info.Resolution.X;
    const float resy = (float)Skin.Info.Resolution.Y;

    Skin.Base->ConfigMenu.Image.BackGround.Draw({});
    UIText(font, 24, 26, "設定", white);

    // タブ
    for (int i = 0; i < (int)ConfigUI.Categories.size(); ++i) {
        const UIRect r = ConfigTabRect(i);
        const bool active = i == ConfigUI.Category;
        UIBox(r, active ? GetColor(214, 116, 20) : black, active ? 220 : 140);
        UIBox(r, white, 160, false);
        UIText(sfont, (r.L + r.R) * 0.5f, (r.T + r.B) * 0.5f, FitText(ConfigUI.Categories[i].Name, sfont.Handle, r.R - r.L - 8, false), white, 1);
    }

    // 項目
    auto& items = ConfigItems();
    const UIRect area = ConfigListArea();
    SetDrawArea((int)area.L, (int)area.T, (int)area.R, (int)area.B);
    const int last = std::min((int)items.size(), ConfigUI.Scroll + ConfigVisibleRows() + 1);
    for (int i = ConfigUI.Scroll; i < last; ++i) {
        auto& item = items[i];
        const float cy = ConfigRowCenter(i);
        const bool selected = i == ConfigUI.Selected;

        const unsigned int add = selected ? 100 : 0;
        SetDrawAddColor(add, add, add);
        Skin.Base->ConfigMenu.Image.Box.Draw({ 0, cy - Skin.Base->ConfigMenu.Image.Box.Pos.Y });
        SetDrawAddColor(0, 0, 0);

        float namex = Skin.Base->ConfigMenu.Config.StringPos.X;
        if (namex <= 0) { namex = area.L + 24; }
        UIText(font, namex, cy, FitText(item.Name + (item.Restart ? " *" : ""), font.Handle, ConfigValueRect(i).L - namex - 16.0f, false), white);

        const UIRect v = ConfigValueRect(i);
        switch (item.Type) {
        case ConfigItemType::Bool: {
            const bool on = item.Get() == "ON";
            const UIRect pill{ v.L, v.T, v.L + 140, v.B };
            UIBox(pill, on ? GetColor(40, 200, 90) : GetColor(90, 90, 90), 230);
            UIText(sfont, (pill.L + pill.R) * 0.5f, cy, on ? "ON" : "OFF", white, 1);
            break;
        }
        case ConfigItemType::Number:
        case ConfigItemType::Choice: {
            const UIRect l = ConfigArrowRect(i, false);
            const UIRect r = ConfigArrowRect(i, true);
            UIBox(l, black, 150);
            UIBox(r, black, 150);
            UIText(sfont, (l.L + l.R) * 0.5f, cy, "◀", white, 1);
            UIText(sfont, (r.L + r.R) * 0.5f, cy, "▶", white, 1);
            if (!(ConfigUI.Editing && selected)) {
                UIText(sfont, (l.R + r.L) * 0.5f, cy, FitText(item.Get(), sfont.Handle, r.L - l.R - 8, false), white, 1);
            }
            break;
        }
        case ConfigItemType::Text: {
            UIBox(v, black, 150);
            UIBox(v, white, 120, false);
            if (!(ConfigUI.Editing && selected)) {
                const std::string value = item.Get();
                UIText(sfont, v.L + 12, cy, value.empty() ? std::string("(なし)") : FitText(value, sfont.Handle, v.R - v.L - 24, false), value.empty() ? gray : white);
            }
            break;
        }
        case ConfigItemType::Key: {
#ifndef __ANDROID__
            const std::vector<std::string>* names = Config.KeyNameLanes(item.Index);
            const int count = ConfigKeySlotCount(item);
            for (int slot = 0; slot < count; ++slot) {
                const UIRect r = ConfigKeySlotRect(i, slot, count);
                const bool current = selected && slot == ConfigUI.KeySlot;
                const bool addslot = slot >= (int)item.Keys->size();
                UIBox(r, current ? GetColor(214, 116, 20) : black, current ? 200 : (addslot ? 90 : 150));
                if (addslot) { UIBox(r, white, 110, false); }
                std::string label = addslot ? std::string("+") : (names != nullptr && slot < (int)names->size() ? (*names)[slot] : std::string("?"));
                if (current && ConfigUI.Capturing) { label = "..."; }
                UIText(sfont, (r.L + r.R) * 0.5f, cy, FitText(label, sfont.Handle, r.R - r.L - 8, false), white, 1);
            }
#endif
            break;
        }
        case ConfigItemType::Folder: {
            const UIRect path = ConfigFolderPartRect(i, item.Type, 0);
            const bool exists = item.Index >= 0 && item.Index < (int)ConfigUI.FolderExists.size() && ConfigUI.FolderExists[item.Index];
            UIBox(path, black, 150);
            UIText(sfont, path.L + 12, cy, FitText(item.Get(), sfont.Handle, path.R - path.L - 24, true), exists ? white : GetColor(255, 120, 120));
            const UIRect del = ConfigFolderPartRect(i, item.Type, 1);
            const bool current = selected && ConfigUI.KeySlot == 1;
            UIBox(del, current ? GetColor(214, 116, 20) : GetColor(44, 48, 64), 225);
            UIBox(del, white, 120, false);
            UIText(sfont, (del.L + del.R) * 0.5f, cy, "削除", white, 1);
            break;
        }
        case ConfigItemType::Action: {
            UIBox(v, selected ? GetColor(214, 116, 20) : GetColor(44, 48, 64), 225);
            UIBox(v, white, 120, false);
            UIText(sfont, (v.L + v.R) * 0.5f, cy, FitText(item.Get(), sfont.Handle, v.R - v.L - 16, false), white, 1);
            break;
        }
        case ConfigItemType::FolderAdd: {
            for (int part = 0; part < FolderAddParts; ++part) {
                const UIRect r = ConfigFolderPartRect(i, item.Type, part);
                const bool current = selected && ConfigUI.KeySlot == part;
                UIBox(r, current ? GetColor(214, 116, 20) : GetColor(44, 48, 64), 225);
                UIBox(r, white, 120, false);
                if (!(ConfigUI.Editing && selected)) {
                    UIText(sfont, (r.L + r.R) * 0.5f, cy, FitText(FolderAddLabel(part), sfont.Handle, r.R - r.L - 12, false), white, 1);
                }
            }
            break;
        }
        }
    }
    SetDrawAreaFull();

    // スクロールバー
    const int n = (int)items.size();
    const int vis = ConfigVisibleRows();
    if (n > vis) {
        const float h = area.B - area.T;
        const float top = area.T + h * ConfigUI.Scroll / n;
        const float bottom = area.T + h * std::min(n, ConfigUI.Scroll + vis) / n;
        UIBox({ area.R - 8, area.T, area.R, area.B }, black, 120);
        UIBox({ area.R - 8, top, area.R, bottom }, white, 200);
    }

#ifndef __ANDROID__
    if (ConfigUI.Editing) {
        const UIRect v = ConfigEditRect(ConfigUI.Selected);
        UIBox(v, black, 230);
        UIBox(v, accent, 255, false);
        SetDrawArea((int)v.L + 4, (int)v.T, (int)v.R - 4, (int)v.B);
        DrawTextInput((int)v.L + 12, (int)(((v.T + v.B) * 0.5f) - font.Size * 0.5f), ConfigUI.EditHandle);
        SetDrawAreaFull();
    }
#endif

    // 説明欄 (説明は 2 行まで折り返し、それ以上は省略。操作方法も画面幅で折り返す)
    {
        const float width = resx - 48.0f;
        const float lh = ConfigLineHeight();
        float y = area.B + 12.0f + lh * 0.5f;
        UIBox({ 0, area.B + 4.0f, resx, resy }, black, 110);
        if (!items.empty()) {
            const auto& item = items[ConfigUI.Selected];
            std::vector<std::string> lines = WrapText(item.Help, sfont.Handle, width);
            if (lines.size() > 2) {
                std::string rest;
                for (size_t k = 1; k < lines.size(); ++k) { rest += (k > 1 ? " " : "") + lines[k]; }
                lines.resize(2);
                lines[1] = FitText(rest, sfont.Handle, width, false);
            }
            for (const auto& line : lines) {
                UIText(sfont, 24, y, line, white);
                y += lh;
            }
            y += lh * (2 - (float)lines.size());
            if (std::chrono::steady_clock::now() < ConfigUI.MessageUntil) {
                UIText(sfont, 24, y, FitText(ConfigUI.Message, sfont.Handle, width, false), accent);
            }
            else if (item.Restart) {
                UIText(sfont, 24, y, "* 再起動後に反映されます", accent);
            }
            y += lh;
        }
        else {
            y += lh * 3;
        }
        for (const auto& line : WrapText(ConfigControlsText, sfont.Handle, width)) {
            UIText(sfont, 24, y, line, gray);
            y += lh;
        }
    }

    const UIRect back = ConfigBackRect();
    UIBox(back, black, 180);
    UIBox(back, white, 200, false);
    UIText(sfont, (back.L + back.R) * 0.5f, (back.T + back.B) * 0.5f, "戻る", white, 1);

    if (ConfigUI.RecommendOpen) {
        DrawRecommendPanel(sfont);
    }

    if (ConfigUI.Capturing) {
        UIBox({ 0, 0, resx, resy }, black, 140);
        UIText(sfont, resx * 0.5f, resy * 0.5f, FitText("割り当てるキーを押してください (Esc: キャンセル)", sfont.Handle, resx - 48, false), white, 1);
    }
#ifndef __ANDROID__
    if (ConfigUI.Picker) {
        UIBox({ 0, 0, resx, resy }, black, 160);
        UIText(sfont, resx * 0.5f, resy * 0.5f, FitText("エクスプローラーで譜面フォルダを選んでください", sfont.Handle, resx - 48, false), white, 1);
    }
#endif
}

void Game::ConfigMenuProc() {
    if (ConfigUI.Categories.empty()) {
        ConfigMenuInit();
    }
    ConfigRebuildFoldersIfNeeded();
    auto Leave = [&] {
        Skin.Base->Title.SE.Don.Play();
        PrepareConfigSkin();
        NowScene = PrevScene;
    };

#ifndef __ANDROID__
    // ---- フォルダ選択ダイアログを開いている間は操作を受け付けない ----
    if (ConfigUI.Picker) {
        if (ConfigUI.Picker->Done) {
            const std::shared_ptr<FolderPickState> state = std::move(ConfigUI.Picker);
            ConfigUI.Picker.reset();
            FolderPickFinished(state->Result, ConfigUI.PickerTarget);
            PointerPrev.Left = PointerPrev.Right = PointerPrev.Middle = true;
            Input::GetInstance().ResyncKeys();
        }
        return;
    }
#endif

    const PointerState pt = ReadPointer();

    if (ConfigUI.RecommendOpen) {
        ConfigRecommendProc(pt);
        return;
    }

#ifndef __ANDROID__
    // ---- 文字入力中 ----
    if (ConfigUI.Editing) {
        const int state = CheckKeyInput(ConfigUI.EditHandle);
        const bool clickout = pt.LeftPressed && !ConfigEditRect(ConfigUI.Selected).Contains(pt.X, pt.Y);
        if (state != 0 || clickout) {
            ConfigEndEdit(state == 1 || clickout);
            Input::GetInstance().ResyncKeys();
        }
        return;
    }
    // ---- キー割り当ての入力待ち ----
    if (ConfigUI.Capturing) {
        // Raw Input のイベントと、ウィンドウが受け取ったキー状態の両方で検出する
        // (Raw Input が届かない環境でも入力待ちから抜けられるように)
        int key = 0;
        for (const auto& e : KeyEvents) {
            if (e.Down) { key = e.VKey; break; }
        }
        if (key == 0) {
            const auto pressed = NewlyPressedKeys(ConfigUI.CapturePrev);
            if (!pressed.empty()) { key = pressed.front(); }
        }
        else {
            NewlyPressedKeys(ConfigUI.CapturePrev);
        }
        // 修飾キーの左右は区別して割り当てる (WndProc では VK_SHIFT などにまとまるため)
        if (key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU) {
            const int lr[3][2] = { { VK_LSHIFT, VK_RSHIFT }, { VK_LCONTROL, VK_RCONTROL }, { VK_LMENU, VK_RMENU } };
            const int idx = key == VK_SHIFT ? 0 : key == VK_CONTROL ? 1 : 2;
            key = (GetAsyncKeyState(lr[idx][1]) & 0x8000) ? lr[idx][1] : lr[idx][0];
        }
        if (key == VK_ESCAPE) {
            ConfigUI.Capturing = false;
        }
        else if (key != 0) {
            ConfigAssignKey(ConfigItems()[ConfigUI.Selected], key);
            ConfigUI.Capturing = false;
        }
        if (!ConfigUI.Capturing) {
            Input::GetInstance().ResyncKeys();
        }
        return;
    }
#endif

    auto& items = ConfigItems();
#ifndef __ANDROID__
    const bool shift = (__gotkeyboardinput[VK_SHIFT] & 1) != 0;
#else
    const bool shift = false;
#endif

    // ---- マウス ----
    for (int i = 0; i < (int)ConfigUI.Categories.size(); ++i) {
        if (pt.LeftPressed && ConfigTabRect(i).Contains(pt.X, pt.Y) && i != ConfigUI.Category) {
            Skin.Base->Title.SE.Ka.Play();
            ConfigSetCategory(i);
            return;
        }
    }
    if (pt.LeftPressed && ConfigBackRect().Contains(pt.X, pt.Y)) {
        Leave();
        return;
    }

    const UIRect area = ConfigListArea();
    int hovered = -1;
#ifndef __ANDROID__
    // Android はタッチ太鼓で操作する (項目のタップは使わない。タブと戻るだけタップ)
    if (area.Contains(pt.X, pt.Y)) {
        const int last = std::min((int)items.size(), ConfigUI.Scroll + ConfigVisibleRows());
        for (int i = ConfigUI.Scroll; i < last; ++i) {
            if (ConfigRowRect(i).Contains(pt.X, pt.Y)) { hovered = i; break; }
        }
    }
    if (hovered >= 0 && (pt.Moved || pt.LeftPressed || pt.RightPressed) && hovered != ConfigUI.Selected) {
        ConfigUI.Selected = hovered;
        ConfigUI.KeySlot = 0;
    }
    if (hovered >= 0 && pt.LeftPressed) {
        auto& item = items[hovered];
        switch (item.Type) {
        case ConfigItemType::Bool:
            ConfigStep(item, 1);
            break;
        case ConfigItemType::Number:
        case ConfigItemType::Choice:
            if (ConfigArrowRect(hovered, false).Contains(pt.X, pt.Y)) {
                ConfigStep(item, shift ? -10 : -1);
            }
            else if (ConfigArrowRect(hovered, true).Contains(pt.X, pt.Y)) {
                ConfigStep(item, shift ? 10 : 1);
            }
            else if (ConfigValueRect(hovered).Contains(pt.X, pt.Y)) {
                ConfigActivate(item);
            }
            break;
        case ConfigItemType::Text:
            ConfigBeginEdit(item);
            break;
        case ConfigItemType::Key: {
            const int count = ConfigKeySlotCount(item);
            for (int slot = 0; slot < count; ++slot) {
                if (ConfigKeySlotRect(hovered, slot, count).Contains(pt.X, pt.Y)) {
                    ConfigUI.KeySlot = slot;
                    ConfigActivate(item);
                    break;
                }
            }
            break;
        }
        case ConfigItemType::Action:
            if (ConfigValueRect(hovered).Contains(pt.X, pt.Y)) {
                ConfigActivate(item);
            }
            break;
        case ConfigItemType::Folder:
        case ConfigItemType::FolderAdd:
            for (int part = 0; part < ConfigPartCount(item); ++part) {
                if (ConfigFolderPartRect(hovered, item.Type, part).Contains(pt.X, pt.Y)) {
                    ConfigUI.KeySlot = part;
                    ConfigFolderPart(item, part);
                    break;
                }
            }
            break;
        }
        return;     // 行の数が変わることがあるので、このフレームの処理はここまで
    }
    if (hovered >= 0 && pt.RightPressed && items[hovered].Type != ConfigItemType::Key && items[hovered].Reset) {
        ConfigResetItem(items[hovered]);
        return;
    }
    if (hovered >= 0 && pt.RightPressed && items[hovered].Type == ConfigItemType::Key) {
        const int count = ConfigKeySlotCount(items[hovered]);
        for (int slot = 0; slot < count; ++slot) {
            if (ConfigKeySlotRect(hovered, slot, count).Contains(pt.X, pt.Y)) {
                ConfigRemoveKey(items[hovered], slot);
                break;
            }
        }
        return;
    }
#else
    (void)area; (void)shift;
#endif
    if (pt.Wheel != 0) {
        // ホイールは一覧のスクロールだけ (値は変えない)
        const int n = (int)items.size();
        ConfigUI.Scroll = std::clamp(ConfigUI.Scroll - pt.Wheel, 0, std::max(n - ConfigVisibleRows(), 0));
        ConfigUI.Selected = std::clamp(ConfigUI.Selected, ConfigUI.Scroll, std::max(ConfigUI.Scroll, std::min(n - 1, ConfigUI.Scroll + ConfigVisibleRows() - 1)));
    }

    // ---- キーボード ----
    auto Move = [&](int dir) {
        if (items.empty()) { return; }
        Skin.Base->Title.SE.Ka.Play();
        ConfigUI.Selected = std::clamp(ConfigUI.Selected + dir, 0, (int)items.size() - 1);
        ConfigUI.KeySlot = 0;
            ConfigEnsureVisible();
    };
    auto Change = [&](int dir) {
        if (items.empty()) { return; }
        auto& item = items[ConfigUI.Selected];
        switch (item.Type) {
        case ConfigItemType::Key:
        case ConfigItemType::Folder:
        case ConfigItemType::FolderAdd:
            Skin.Base->Title.SE.Ka.Play();
            ConfigUI.KeySlot = std::clamp(ConfigUI.KeySlot + dir, 0, std::max(ConfigPartCount(item) - 1, 0));
            break;
        case ConfigItemType::Text:
        case ConfigItemType::Action:
        case ConfigItemType::Bool:
            break;
        default:
            ConfigStep(item, dir * (shift ? 10 : 1));
            break;
        }
    };
    auto Activate = [&] {
        if (!items.empty()) { ConfigActivate(items[ConfigUI.Selected]); }
    };

#ifndef __ANDROID__
    Input::GetInstance().HitKeyProcess(VK_UP, KeyState::Down, [&] { Move(-1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_DOWN, KeyState::Down, [&] { Move(1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { Move(-1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { Move(1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_LEFT, KeyState::Down, [&] { Change(-1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_RIGHT, KeyState::Down, [&] { Change(1); }, Config.KeyHoldProcInterval);
    Input::GetInstance().HitKeyProcess(VK_RETURN, KeyState::Down, Activate);
    Input::GetInstance().HitKeyesProcess(Config.DonInputLeft, KeyState::Down, Activate);
    Input::GetInstance().HitKeyesProcess(Config.DonInputRight, KeyState::Down, Activate);
    Input::GetInstance().HitKeyProcess(VK_TAB, KeyState::Down, [&] {
        Skin.Base->Title.SE.Ka.Play();
        ConfigSetCategory(ConfigUI.Category + (shift ? -1 : 1));
        });
    auto ClearKey = [&] {
        if (items.empty()) { return; }
        auto& item = items[ConfigUI.Selected];
        if (item.Type == ConfigItemType::Key) {
            ConfigRemoveKey(item, ConfigUI.KeySlot);
            ConfigUI.KeySlot = std::clamp(ConfigUI.KeySlot, 0, std::max(ConfigKeySlotCount(item) - 1, 0));
        }
        else if (item.Type == ConfigItemType::Folder) {
            FolderRemove(item.Index);
        }
        else { ConfigResetItem(item); }
    };
    Input::GetInstance().HitKeyProcess(VK_DELETE, KeyState::Down, ClearKey);
    Input::GetInstance().HitKeyProcess(VK_ESCAPE, KeyState::Down, Leave);
#else
    // 面で編集を開始・終了し、縁で移動または値を変更する。
    auto TaikoMove = [&](int dir) {
        const int n = (int)items.size();
        if (n == 0 || (dir < 0 && ConfigUI.Selected == 0) || (dir > 0 && ConfigUI.Selected >= n - 1)) {
            Skin.Base->Title.SE.Ka.Play();
            ConfigSetCategory(ConfigUI.Category + dir);
            if (dir < 0) { ConfigUI.Selected = std::max(0, (int)ConfigItems().size() - 1); ConfigEnsureVisible(); }
            return;
        }
        Move(dir);
    };
    auto TaikoDon = [&] {
        if (items.empty()) { return; }
        auto& item = items[ConfigUI.Selected];
        switch (item.Type) {
        case ConfigItemType::Number:
        case ConfigItemType::Choice:
            ConfigUI.TaikoEditing = !ConfigUI.TaikoEditing;
            Skin.Base->Title.SE.Don.Play();
            break;
        case ConfigItemType::Bool:
            ConfigUI.TaikoEditing = false;
            Activate();
            break;
        case ConfigItemType::Folder:
        case ConfigItemType::FolderAdd:
            if (!ConfigUI.TaikoEditing) { ConfigUI.TaikoEditing = true; }
            else { ConfigFolderPart(item, ConfigUI.KeySlot); ConfigUI.TaikoEditing = false; }
            break;
        default:
            Activate();
            break;
        }
    };
    auto topButton = [&](float x, float y) {
        if (ConfigBackRect().Contains(x, y)) { return true; }
        for (int i = 0; i < (int)ConfigUI.Categories.size(); ++i) {
            if (ConfigTabRect(i).Contains(x, y)) { return true; }
        }
        return false;
    };
    Touch.ProcessTaiko(TouchType::LeftKa, [&] { if (ConfigUI.TaikoEditing) { Change(-1); } else { TaikoMove(-1); } }, topButton);
    Touch.ProcessTaiko(TouchType::RightKa, [&] { if (ConfigUI.TaikoEditing) { Change(1); } else { TaikoMove(1); } }, topButton);
    Touch.ProcessTaiko(TouchType::Don, TaikoDon, topButton);
#endif
}
