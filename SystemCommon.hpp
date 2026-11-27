#pragma once
#include "GameDependencies.hpp"
#ifndef __ANDROID__
#include "resource.h"
#endif

const fs::path& GetExecutablePath();
#ifdef __ANDROID__
void ShowToast(const std::string& text);
std::string GetInputText(std::string theme = "", std::string initial = "", bool* accepted = nullptr);
bool AskYesNo(const std::string& title, const std::string& message);
#endif
void ShowErrorDialog(const std::string& text);
std::string DescribeException(const std::exception& e);

class _ConfigData {
public:

    _ConfigData() {
        Load();
    }
    // 既定値だけの設定 (ファイルを読まない。設定画面の「既定値に戻す」に使う)
    struct DefaultsTag {};
    explicit _ConfigData(DefaultsTag) {}

    static constexpr int MinBufferSize = 24;

    void Load() {

        std::ifstream ifs(GetExecutablePath() / "config.json");

        if (!ifs.is_open()) {
            ifs.close();
            FirstRun = true;
            Write();
            ifs = std::ifstream(GetExecutablePath() / "config.json");
        }

        json data = json::parse(ifs, nullptr, false);
        ifs.close();

        if (data.is_discarded() || !data.is_object()) {
            BufferSize = std::max(BufferSize, MinBufferSize);
            FrameRate = std::clamp(FrameRate, 0, 1000);
#ifndef __ANDROID__
            UpdateKeyNames();
#endif
            return;
        }

#define JSONDATA(name) name = data.value(#name, name)

        try {
        JSONDATA(PlayerName);
        JSONDATA(TitleName);
        JSONDATA(AutoPlay);
        JSONDATA(PauseDisplay);
        JSONDATA(ServerAddress);
        JSONDATA(ServerPort);
        JSONDATA(DiscordRPC);
        JSONDATA(RandomRate);
        JSONDATA(HiddenLevel);
        JSONDATA(SuddenLevel);
        JSONDATA(JudgeGood);
        JSONDATA(JudgeOk);
        JSONDATA(JudgeBad);
        JSONDATA(SongOffset);
        JSONDATA(JudgeOffset);
        JSONDATA(ChartOffset);
        JSONDATA(JiroChartMode);
        JSONDATA(EditorGrids);
        JSONDATA(EditorCustomGrids);
        JSONDATA(RecordPlay);
        JSONDATA(RecordFps);
        JSONDATA(RecordCrf);
        JSONDATA(RecordCodec);
        JSONDATA(RecordFormat);
        JSONDATA(ScoreMeter);
        JSONDATA(KeyMapView);
        JSONDATA(ChartSpeed);
        JSONDATA(SongSpeed);
        JSONDATA(TrainingMode);
        JSONDATA(BGBrightness);
        JSONDATA(SkinName);
        JSONDATA(SongDirectories);
        JSONDATA(SongVolume);
        JSONDATA(SEVolume);
        JSONDATA(HitNoteDisp);
        JSONDATA(RollSpeed);
        JSONDATA(FastInput);
        JSONDATA(FrameRate);
        JSONDATA(FastDrawRate);
        JSONDATA(FrameCountTimer);
        JSONDATA(FrameExtendRate);
        JSONDATA(Exclusive);
        JSONDATA(SampleRate);
        JSONDATA(BufferSize);
        JSONDATA(ViewDebug);
#ifndef __ANDROID__
        JSONDATA(KeyHoldProcInterval);
        JSONDATA(SoundDeviceType);
        JSONDATA(WaitVSync);
        JSONDATA(FullScreen);
        JSONDATA(MultiBoot);
        JSONDATA(KaInputLeft);
        JSONDATA(DonInputLeft);
        JSONDATA(DonInputRight);
        JSONDATA(KaInputRight);
#else
        JSONDATA(TouchTaikoWidth);
        JSONDATA(TouchTaikoHeight);
#endif
        }
        catch (const json::exception&) {
            // 型が不正な項目があっても、読み込めた分と既定値で起動を続ける
        }

#undef JSONDATA

        BufferSize = std::max(BufferSize, MinBufferSize);
        FrameRate = std::clamp(FrameRate, 0, 1000);
#ifndef __ANDROID__
        UpdateKeyNames();
#endif
    }

    void Write() {
        BufferSize = std::max(BufferSize, MinBufferSize);
        FrameRate = std::clamp(FrameRate, 0, 1000);
        // 一時的に有効にした次郎形式は保存しない (保存するのは元の値)
        const bool jirocurrent = JiroChartMode;
        if (JiroChartModeSaved) { JiroChartMode = *JiroChartModeSaved; }
        struct RestoreJiro { bool& v; bool cur; ~RestoreJiro() { v = cur; } } restorejiro{ JiroChartMode, jirocurrent };

#define JSONDATA(name) {#name, name}
        nlohmann::ordered_json data = {
                JSONDATA(PlayerName),
                JSONDATA(TitleName),
                JSONDATA(AutoPlay),
                JSONDATA(PauseDisplay),
                JSONDATA(ServerAddress),
                JSONDATA(ServerPort),
                JSONDATA(DiscordRPC),
                JSONDATA(RandomRate),
                JSONDATA(HiddenLevel),
                JSONDATA(SuddenLevel),
                JSONDATA(JudgeGood),
                JSONDATA(JudgeOk),
                JSONDATA(JudgeBad),
                JSONDATA(SongOffset),
                JSONDATA(JudgeOffset),
                JSONDATA(ChartOffset),
                JSONDATA(JiroChartMode),
                JSONDATA(EditorGrids),
                JSONDATA(EditorCustomGrids),
                JSONDATA(RecordPlay),
                JSONDATA(RecordFps),
                JSONDATA(RecordCrf),
                JSONDATA(RecordCodec),
                JSONDATA(RecordFormat),
                JSONDATA(ScoreMeter),
                JSONDATA(KeyMapView),
                JSONDATA(ChartSpeed),
                JSONDATA(SongSpeed),
                JSONDATA(TrainingMode),
                JSONDATA(BGBrightness),
                JSONDATA(SkinName),
                JSONDATA(SongDirectories),
                JSONDATA(SongVolume),
                JSONDATA(SEVolume),
                JSONDATA(HitNoteDisp),
                JSONDATA(RollSpeed),
                JSONDATA(FastInput),
                JSONDATA(FrameRate),
                JSONDATA(FastDrawRate),
                JSONDATA(FrameCountTimer),
                JSONDATA(FrameExtendRate),
                JSONDATA(Exclusive),
                JSONDATA(SampleRate),
                JSONDATA(BufferSize),
                JSONDATA(ViewDebug),
#ifndef __ANDROID__
                JSONDATA(KeyHoldProcInterval),
                JSONDATA(SoundDeviceType),
                JSONDATA(FullScreen),
                JSONDATA(WaitVSync),
                JSONDATA(MultiBoot),
                JSONDATA(KaInputLeft),
                JSONDATA(DonInputLeft),
                JSONDATA(DonInputRight),
                JSONDATA(KaInputRight),
#else
                JSONDATA(TouchTaikoWidth),
                JSONDATA(TouchTaikoHeight),
#endif
        };
#undef JSONDATA

#ifndef __ANDROID__
        UpdateKeyNames();
#endif
        std::ofstream ofs(GetExecutablePath() / "config.json");
        ofs << data.dump(4, ' ', false, nlohmann::json::error_handler_t::replace) << "\n";   // 正しくない UTF-8 があっても例外にしない
    }

#ifndef __ANDROID__
    static constexpr size_t MaxKeysPerLane = 16;

    std::vector<int>* KeyLanes(int lane) {
        std::vector<int>* lanes[] = { &KaInputLeft, &DonInputLeft, &DonInputRight, &KaInputRight };
        return lane >= 0 && lane < 4 ? lanes[lane] : nullptr;
    }
    std::vector<std::string>* KeyNameLanes(int lane) {
        std::vector<std::string>* lanes[] = { &KeyNameKaLeft, &KeyNameDonLeft, &KeyNameDonRight, &KeyNameKaRight };
        return lane >= 0 && lane < 4 ? lanes[lane] : nullptr;
    }

    // キーの割り当ては可変長。未割り当て (0) や範囲外・重複を取り除き、1 レーンの上限に収める
    // (以前の 4 枠固定の設定ファイルにある 0 の枠もここで消える)
    static void NormalizeKeys(std::vector<int>& keys) {
        std::vector<int> out;
        out.reserve(keys.size());
        for (int key : keys) {
            if (key > 0 && key < 256 && std::ranges::find(out, key) == out.end() && out.size() < MaxKeysPerLane) {
                out.push_back(key);
            }
        }
        keys = std::move(out);
    }

    void UpdateKeyNames() {
        for (int lane = 0; lane < 4; ++lane) {
            NormalizeKeys(*KeyLanes(lane));
        }
        KeyNameKaLeft = GetKeyName(KaInputLeft);
        KeyNameDonLeft = GetKeyName(DonInputLeft);
        KeyNameDonRight = GetKeyName(DonInputRight);
        KeyNameKaRight = GetKeyName(KaInputRight);
    }

    std::vector<std::string> GetKeyName(const std::vector<int>& keys) {
        std::vector<std::string> strvec;
        for (auto&& key : keys) {
            UINT ScanCode = MapVirtualKey(key, MAPVK_VK_TO_VSC);
            char KeyName[128];
            if (GetKeyNameText(ScanCode << 16, KeyName, sizeof(KeyName)) != 0) {
                strvec.push_back(KeyName);
                continue;
            }
            strvec.push_back("*");
        }
        return strvec;
    }
#endif

    bool FirstRun = false;              // config.json が無かった (初めての起動)
    std::string PlayerName = "NoName";
    std::string TitleName = "";         // 称号 (名札の名前の上に表示。空なら表示しない)

    std::string ServerAddress = "localhost";
    uint16_t ServerPort = 8080;
    bool DiscordRPC = true;             // Discord にステータスを表示する (アプリケーション ID はゲームに組み込み)

    bool AutoPlay = false;
    bool PauseDisplay = false;
    bool TrainingMode = false;
    bool HitNoteDisp = true;
    int FrameRate = 0;                 // 0: 従来の描画間隔、1～1000: FPS 上限
    bool WaitVSync = true;
    bool FastInput = true;
    bool FrameCountTimer = true;
    bool Exclusive = false;
    bool FullScreen = false;
    bool ViewDebug = false;
    bool MultiBoot = false;

    int RandomRate = 0;
    int SoundDeviceType = 0; // None = 0, WASAPI = 1, XAudio = 2, MMEwaveOut = 3, ASIO = 4
    int SampleRate = 192000;
    int BufferSize = 480;

    double HiddenLevel = 0.0;
    double SuddenLevel = 0.0;
    double JudgeGood = 25;
    double JudgeOk = 75;
    double JudgeBad = 100;
    double JudgeOffset = 0.0;
    double ChartOffset = 0.0;           // 譜面 (音符・小節線) の時刻をずらす (ミリ秒。正で遅く)
    bool JiroChartMode = false;         // 太鼓さん次郎と同じ時刻・判定順・表示位置・分岐のタイミング
    std::optional<bool> JiroChartModeSaved;     // 譜面を読む時に一時的に次郎形式にした時の元の値 (保存しない)
    std::vector<int> EditorGrids{ 4, 8, 12, 16, 24, 32, 48 };    // 譜面制作でよく使うグリッドの分割数
    std::vector<int> EditorCustomGrids;
    bool RecordPlay = false;            // 演奏を録画して recordings フォルダに MP4 で保存する
    int RecordFps = 60;
    int RecordCrf = 20;                 // 画質 (libx264 の CRF。小さいほど高画質・大きいファイル)
    std::string RecordCodec = "libx264";    // 映像のエンコーダ (libx264 / h264_nvenc / h264_qsv / h264_amf)
    std::string RecordFormat = "mp4";       // 保存する形式 (mp4 / mkv / mov / webm / gif / m4a / mp3 / wav / flac)
    bool ScoreMeter = false;            // 演奏中に判定のずれをメーターで表示する
    bool KeyMapView = true;
    double SongOffset = 0;
    double ChartSpeed = 1;
    double SongSpeed = 1;
    double BGBrightness = 100;
    double RollSpeed = 30;
    double KeyHoldProcInterval = 0.15;
    double FastDrawRate = 0.5;
    double FrameExtendRate = 1.0;

    float SongVolume = 62.0f;
    float SEVolume = 70.0f;
    float TouchTaikoWidth = 560.0f;
    float TouchTaikoHeight = 280.0f;

    std::string SkinName = "Default";
    std::vector<std::string> SongDirectories{ "Songs" };

#ifndef __ANDROID__
    std::vector<int> KaInputLeft{ 'D','S' };
    std::vector<int> DonInputLeft{ 'F','G' };
    std::vector<int> DonInputRight{ 'J','H' };
    std::vector<int> KaInputRight{ 'K','L' };

    std::vector<std::string> KeyNameKaLeft{ "D","S" };
    std::vector<std::string> KeyNameDonLeft{ "F","G" };
    std::vector<std::string> KeyNameDonRight{ "J","H" };
    std::vector<std::string> KeyNameKaRight{ "K","L" };
#endif

};

class _Skin {
public:
    _Skin() = default;
    _Skin(const _Skin&) = delete;
    _Skin& operator=(const _Skin&) = delete;
    ~_Skin() { Dispose(); }

    fs::path Root;
    void Init(fs::path root = GetExecutablePath() / "Skins") {
        Root = std::move(root);
        std::vector<SkinInfo> found;
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(Root, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (it->path().extension() != ".json") {
                continue;
            }
            std::ifstream ifs(it->path());
            json data = json::parse(ifs, nullptr, false);
            if (data.is_discarded() || !data.is_object() || !data.contains("Info") || !data["Info"].is_object()) {
                continue;
            }
            if (!data["Info"].contains("Name") || !data["Info"]["Name"].is_string()) { continue; }
            found.push_back(
                SkinInfo{
                        data["Info"].value("Name", std::string()),
                        it->path().string()
                }
            );
        }
        if (!ec) { Infos = std::move(found); }
        else if (!fs::exists(Root, ec) && !ec) { Infos.clear(); }
    }

    // const json の operator[] は存在しないキーで未定義動作になるため、安全に子要素を取り出す
    static const json& JsonChild(const json& j, const char* key) {
        static const json null_json;
        if (j.is_object()) {
            if (auto it = j.find(key); it != j.end()) {
                return *it;
            }
        }
        return null_json;
    }

    bool IsLoading = false;
    bool TryLoad(const std::string& name, std::string* error = nullptr, bool strict = false) {
#ifndef __ANDROID__
        // 論理解像度の切り替えと、ユーザーが選んだウィンドウの大きさを分離する。
        struct WindowGeometry {
            HWND Window=nullptr;
            WINDOWPLACEMENT Placement{sizeof(WINDOWPLACEMENT)};
            int Width=0,Height=0;
            explicit WindowGeometry(bool preserve) {
                if(!preserve||!DxLib_IsInit()||!GetWindowModeFlag())return;
                Window=GetMainWindowHandle();
                if(!GetWindowPlacement(Window,&Placement)||GetWindowSize(&Width,&Height)<0){Window=nullptr;return;}
                if(Width>0&&Height>0)SetWindowSize(Width,Height);
            }
            ~WindowGeometry(){if(Window){if(Width>0&&Height>0)SetWindowSize(Width,Height);SetWindowPlacement(Window,&Placement);}}
        } geometry(Base!=nullptr);
#endif
        _Skin replacement;
        replacement.Infos = Infos;
        replacement.RequireFiles = strict;
        replacement.AllowedMissingAssets = MissingAssets;
        IsLoading = true;
        try {
            replacement.LoadImpl(name);
            using std::swap;
            swap(Base,replacement.Base); swap(Releasers,replacement.Releasers);
            swap(Info,replacement.Info); swap(FilePath,replacement.FilePath);
            swap(MissingAssets,replacement.MissingAssets);
            swap(ResolutionRect,replacement.ResolutionRect); swap(SimulationRect,replacement.SimulationRect);
            replacement.Dispose();
            IsLoading = false;
            return true;
        } catch (const std::exception& e) { if (error) { *error=e.what(); } }
        catch (...) { if (error) { *error="不明なエラー"; } }
        SetUseASyncLoadFlag(FALSE);
        if (DxLib_IsInit()) { WaitHandleASyncLoadAll(); }
        replacement.Dispose();
        IsLoading = false;
        if (Base && Info.Resolution.X > 0 && Info.Resolution.Y > 0) {
            SetGraphMode(Info.Resolution.X,Info.Resolution.Y,Info.ColorBit); SetDrawScreen(DX_SCREEN_BACK);
        }
        return false;
    }
    bool RequireFiles = false;
    std::unordered_set<std::string> MissingAssets, AllowedMissingAssets;
    void CheckAssetFiles(const json& node, const fs::path& directory) {
        if (node.is_object()) {
            for (auto it=node.begin();it!=node.end();++it) {
                if (it.key()=="FilePath" && it->is_string() && !it->get_ref<const std::string&>().empty()) {
                    const auto value=it->get<std::string>();
                    const auto path=directory/fs::path(std::u8string(value.begin(),value.end()));
                    std::error_code ec;
                    if (!fs::is_regular_file(path,ec) || ec) {
                        const auto key=catalog::Key(path);
                        MissingAssets.insert(key);
                        const bool unreadable = ec && ec!=std::errc::no_such_file_or_directory && ec!=std::errc::not_a_directory;
                        if (RequireFiles && (unreadable || !AllowedMissingAssets.contains(key))) { throw std::runtime_error("画像・音声・フォントの更新が完了していません: "+value); }
                    }
                } else { CheckAssetFiles(*it,directory); }
            }
        } else if (node.is_array()) { for (const auto& child:node) { CheckAssetFiles(child,directory); } }
    }

    // スキンを読み込む。設定ファイルが壊れている・値の型が違うなどで読めない時は、知らせて Default スキンで続ける
    void Load(const std::string& name, _ConfigData& config) {
        const std::string requested = name; // Config.SkinName を変更しても参照を無効化しない
        auto fail = [&](const std::string& what) {
            SetUseASyncLoadFlag(FALSE);
            IsLoading = false;
            const std::string message = "スキン「" + name + "」を読み込めませんでした (" + what + ")";
#ifndef __ANDROID__
            MessageBoxA(NULL, message.c_str(), "エラー", MB_ICONERROR);
#else
            ShowToast(message);
#endif
        };
        std::string error;
        if (TryLoad(requested, &error)) { return; }
        fail(error);
        if (requested != "Default") {
            config.SkinName = "Default";
            if (TryLoad("Default")) { return; }
        }
        if (!Base) { Base = new _Base(); } // 既存の有効なスキンは失敗時にも保持する
    }
    void LoadImpl(const std::string& name) {
        IsLoading = true;
        auto found = std::ranges::find(Infos, name, &SkinInfo::Name);
        if (found != Infos.end()) {
            FilePath = found->Path;
        }
        else { throw std::runtime_error("指定したスキンが見つかりません"); }

        const fs::path configPath = GetExecutablePath() / FilePath;
        std::ifstream ifs(configPath);
        json data = json::parse(ifs, nullptr, false);
        ifs.close();
        if (data.is_discarded() || !data.is_object()) { throw std::runtime_error("skinconfig.json の書式が正しくありません"); }

        const fs::path skinDirectory = configPath.parent_path();
        const auto utf8Directory = skinDirectory.generic_u8string();
        const std::string SkinDir = std::string(utf8Directory.begin(), utf8Directory.end()) + '/';
        CheckAssetFiles(data, skinDirectory);
        const json& Infodata = data.at("Info");
        const json& base = data.at("Base");
        if (!base.is_object()) { throw std::runtime_error("Base がありません"); }
        const json& Titledata = JsonChild(base,"Title");
        const json& ModeSelectdata = JsonChild(base,"ModeSelect");
        const json& SongSelectdata = JsonChild(base,"SongSelect");
        const json& DanSelectdata = JsonChild(base,"DanSelect");
        const json& MultiRoomdata = JsonChild(base,"MultiRoom");
        const json& Playingdata = JsonChild(base,"Playing");
        const json& Resultdata = JsonChild(base,"Result");
        const json& ConfigMenudata = JsonChild(base,"ConfigMenu");

        Info.Name = Infodata.at("Name");
        Info.Version = Infodata.at("Version");
        Info.Resolution = Infodata.at("Resolution").get<Pos2D<int>>();
        Info.SimulationDistance = Infodata.at("SimulationDistance").get<Pos2D<float>>();
        Info.ColorBit = Infodata.at("ColorBit");
        if (Info.Resolution.X < 1 || Info.Resolution.Y < 1 || Info.Resolution.X > 16384 || Info.Resolution.Y > 16384 ||
            !std::isfinite(Info.SimulationDistance.X) || !std::isfinite(Info.SimulationDistance.Y)) { throw std::runtime_error("画面サイズが不正です"); }

        SimulationRect = Rect2D<float>{
                Info.Resolution.Y * 0.5f - Info.SimulationDistance.Y,
                Info.Resolution.Y * 0.5f + Info.SimulationDistance.Y,
                Info.Resolution.X * 0.5f - Info.SimulationDistance.X,
                Info.Resolution.X * 0.5f + Info.SimulationDistance.X,
        };

        ResolutionRect = Rect2D<float>{
                0,
                (float)Info.Resolution.Y,
                0,
                (float)Info.Resolution.X,
        };

        SetGraphMode(
            Info.Resolution.X,
            Info.Resolution.Y,
            Info.ColorBit
        );
        SetDrawMode(DX_DRAWMODE_BILINEAR);
        SetDrawScreen(DX_SCREEN_BACK);

        if (!DxLib_IsInit()) {
            return;
        }

        std::vector<std::function<bool()>> validators;
        auto validate = [this,&validators,&skinDirectory](auto* resource,const json& node) {
            if (!RequireFiles || !node.is_object() || !node.contains("FilePath") || !node["FilePath"].is_string() || node["FilePath"].template get<std::string>().empty()) { return; }
            const auto value=node["FilePath"].template get<std::string>();
            if (MissingAssets.contains(catalog::Key(skinDirectory/fs::path(std::u8string(value.begin(),value.end()))))) { return; }
            validators.push_back([resource] {
                if constexpr (requires { resource->Handles; }) {
                    if (resource->Handles.empty()) { return false; }
                    for (int handle:resource->Handles) { int w=0,h=0; if (handle==-1 || GetGraphSize(handle,&w,&h)==-1 || w<1 || h<1) { return false; } }
                    return true;
                } else { return resource->Handle!=-1; }
            });
        };
#define ValLoad(base, type, keyname) \
        if (const json& _j = JsonChild(base##data, #type); _j.is_object()) { Base->base.type.keyname = _j.value(#keyname, Base->base.type.keyname); }
#define DataLoad(base, type, keyname) \
        Releasers.push_back([p = &Base->base.type.keyname] { p->Delete(); }); \
        validate(&Base->base.type.keyname,JsonChild(JsonChild(base##data,#type),#keyname)); \
        Base->base.type.keyname.Load(SkinDir, JsonChild(JsonChild(base##data, #type), #keyname))

        // 称号のフォント・位置。スキンに TitleName / TitleNamePos が無ければ、名前のフォントを 3/4 の大きさにして名前の上に置く
        auto LoadTitleNameFont = [&](const json& basedata, FontData& title, const FontData& name, Pos2D<float>& titlepos, const Pos2D<float>& namepos, float gap) {
            Releasers.push_back([p = &title] { p->Delete(); });
            const json& fonts = JsonChild(basedata, "Font");
            if (const json& tj = JsonChild(fonts, "TitleName"); tj.is_object()) {
                title.Load(SkinDir, tj);
            }
            else if (const json& nj = JsonChild(fonts, "PlayerName"); nj.is_object()) {
                json j = nj;
                j["Size"] = std::max(1.0f, name.Size * 0.75f);
                title.Load(SkinDir, j);
            }
            if (std::isnan(titlepos.X) || std::isnan(titlepos.Y)) {
                titlepos = { namepos.X, namepos.Y - (name.Size * 0.5f + title.Size * 0.6f + gap) };
            }
        };

        Dispose();
        ClearStrlenCache();
        Base = new _Base();

        // 画像は DxLib の非同期読み込みで並列に読み込む (フォント・サウンドは各 Load 内で同期読み込みに切り替える)
        SetUseASyncLoadFlag(TRUE);

#pragma region Base

#pragma region Title

        DataLoad(Title, Image, BackGround);
        DataLoad(Title, Image, Logo);

        DataLoad(Title, SE, Don);
        DataLoad(Title, SE, Ka);

#pragma endregion

#pragma region ModeSelect

        ValLoad(ModeSelect, Config, ModesPos);
        ValLoad(ModeSelect, Config, BoxDistance);

        DataLoad(ModeSelect, Image, BackGround);
        DataLoad(ModeSelect, Image, Box);

        DataLoad(ModeSelect, Font, Modes);

        DataLoad(ModeSelect, SE, Don);
        DataLoad(ModeSelect, SE, Ka);

#pragma endregion

#pragma region SongSelect

        ValLoad(SongSelect, Config, BoxDistance);
        ValLoad(SongSelect, Config, BoxSlice);
        ValLoad(SongSelect, Config, SongBoxListPos);
        ValLoad(SongSelect, Config, BoxTitlePos);
        ValLoad(SongSelect, Config, BoxSubTitlePos);
        ValLoad(SongSelect, Config, CoursePos);
        ValLoad(SongSelect, Config, LevelPos);
        ValLoad(SongSelect, Config, HighScorePos);
        ValLoad(SongSelect, Config, KeyWordPos);
        ValLoad(SongSelect, Config, CourseBoxDistance);

        DataLoad(SongSelect, Image, BackGround);
        DataLoad(SongSelect, Image, Box);
        DataLoad(SongSelect, Image, TitleBox);
        DataLoad(SongSelect, Image, CourseBox);
        DataLoad(SongSelect, Image, SearchBox);
        DataLoad(SongSelect, Image, Crown);
        DataLoad(SongSelect, Image, Back);
        DataLoad(SongSelect, Image, Search);
        DataLoad(SongSelect, Image, Config);
        DataLoad(SongSelect, Image, Random);

        DataLoad(SongSelect, Font, Title);
        DataLoad(SongSelect, Font, SubTitle);
        DataLoad(SongSelect, Font, BoxTitle);
        DataLoad(SongSelect, Font, BoxSubTitle);
        DataLoad(SongSelect, Font, Course);
        DataLoad(SongSelect, Font, Level);
        DataLoad(SongSelect, Font, HighScore);
        DataLoad(SongSelect, Font, KeyWord);

        DataLoad(SongSelect, SE, Don);
        DataLoad(SongSelect, SE, Ka);

#pragma endregion

#pragma region DanSelect

        ValLoad(DanSelect, Config, BoxDistance);
        ValLoad(DanSelect, Config, BoxSlice);
        ValLoad(DanSelect, Config, SongBoxListPos);
        ValLoad(DanSelect, Config, KeyWordPos);

        DataLoad(DanSelect, Image, BackGround);
        DataLoad(DanSelect, Image, Box);
        DataLoad(DanSelect, Image, SearchBox);
        DataLoad(DanSelect, Image, Crown);
        DataLoad(DanSelect, Image, Back);
        DataLoad(DanSelect, Image, Search);
        DataLoad(DanSelect, Image, Config);
        DataLoad(DanSelect, Image, Random);

        DataLoad(DanSelect, Font, Title);
        DataLoad(DanSelect, Font, SubTitle);
        DataLoad(DanSelect, Font, KeyWord);

        DataLoad(DanSelect, SE, Don);
        DataLoad(DanSelect, SE, Ka);

#pragma endregion

#pragma region MultiRoom

        ValLoad(MultiRoom, Config, PlayerPos);
        ValLoad(MultiRoom, Config, TitlePos);
        ValLoad(MultiRoom, Config, SubTitlePos);
        ValLoad(MultiRoom, Config, CoursePos);
        ValLoad(MultiRoom, Config, LevelPos);
        ValLoad(MultiRoom, Config, BoxDistance);

        DataLoad(MultiRoom, Image, BackGround);
        DataLoad(MultiRoom, Image, PlayerBox);
        DataLoad(MultiRoom, Image, TitleBox);
        DataLoad(MultiRoom, Image, Crown);
        DataLoad(MultiRoom, Image, Back);
        DataLoad(MultiRoom, Image, Config);
        DataLoad(MultiRoom, Image, Host);

        DataLoad(MultiRoom, Font, String);
        DataLoad(MultiRoom, Font, Player);
        DataLoad(MultiRoom, Font, Title);
        DataLoad(MultiRoom, Font, SubTitle);
        DataLoad(MultiRoom, Font, Course);
        DataLoad(MultiRoom, Font, Level);

        DataLoad(MultiRoom, SE, Don);
        DataLoad(MultiRoom, SE, Ka);

#pragma endregion

#pragma region Playing

        ValLoad(Playing, Config, TitlePos);
        ValLoad(Playing, Config, SubTitlePos);
        ValLoad(Playing, Config, PlayerNamePos);
        ValLoad(Playing, Config, TitleNamePos);
        ValLoad(Playing, Config, ExamNamePos);
        ValLoad(Playing, Config, ExamValPos);
        ValLoad(Playing, Config, MultiPlayLaneDistance);
        ValLoad(Playing, Config, LaneExtendRate);
        ValLoad(Playing, Config, JudgeUpperExplosionFrameTime);
        ValLoad(Playing, Config, GoGoFireFrameTime);

        DataLoad(Playing, Image, BackGround);
        DataLoad(Playing, Image, DanBackGround);
        DataLoad(Playing, Image, LaneFrame);
        DataLoad(Playing, Image, Lane);
        DataLoad(Playing, Image, NormalLane);
        DataLoad(Playing, Image, ExpertLane);
        DataLoad(Playing, Image, MasterLane);
        DataLoad(Playing, Image, Base);
        DataLoad(Playing, Image, NamePlate);
        DataLoad(Playing, Image, MiniTaiko);
        DataLoad(Playing, Image, MiniTaiko_Don);
        DataLoad(Playing, Image, MiniTaiko_Ka);
        DataLoad(Playing, Image, ComboNumber);
        DataLoad(Playing, Image, ScoreNumber);
        DataLoad(Playing, Image, RollNumber);
        DataLoad(Playing, Image, Note);
        DataLoad(Playing, Image, JudgeUnderExplosion);
        DataLoad(Playing, Image, JudgeUpperExplosion);
        DataLoad(Playing, Image, GoGoFire);
        DataLoad(Playing, Image, JudgeString);
        DataLoad(Playing, Image, ProgressBar);
        DataLoad(Playing, Image, ExamProgressBar);
        DataLoad(Playing, Image, Back);
        DataLoad(Playing, Image, Pause);
        DataLoad(Playing, Image, PauseShade);
        DataLoad(Playing, Image, PausePanel);
        DataLoad(Playing, Image, PauseResume);
        DataLoad(Playing, Image, PauseRestart);
        DataLoad(Playing, Image, PauseExit);
        DataLoad(Playing, Image, PauseCursor);
        auto pauseFallback = [&](GraphData& image, const char* name, float x, float y, float w, float h) {
            if (!image.Handles.empty()) { return; }
            fs::path path = skinDirectory.parent_path()/"Default"/"Image"/"Playing"/"PauseMenu"/name;
            std::error_code ec;
            if (!fs::is_regular_file(path, ec)) { path = GetExecutablePath()/"Skins"/"Default"/"Image"/"Playing"/"PauseMenu"/name; }
            const auto utf8 = path.generic_u8string();
            image.Load(std::string(utf8.begin(), utf8.end()));
            const float sx = Info.Resolution.X/1280.0f, sy = Info.Resolution.Y/720.0f;
            image.Pos = {x*sx, y*sy}; image.Size = {w*sx, h*sy};
        };
        pauseFallback(Base->Playing.Image.PauseShade,"Shade.png",640,360,1280,720);
        pauseFallback(Base->Playing.Image.PausePanel,"Panel.png",640,360,720,480);
        pauseFallback(Base->Playing.Image.PauseResume,"Resume.png",640,326,420,64);
        pauseFallback(Base->Playing.Image.PauseRestart,"Restart.png",640,410,420,64);
        pauseFallback(Base->Playing.Image.PauseExit,"Exit.png",640,494,420,64);
        pauseFallback(Base->Playing.Image.PauseCursor,"Cursor.png",640,326,444,80);
        DataLoad(Playing, Image, KeyMapBox);
        DataLoad(Playing, Image, PlayerChar);
        ValLoad(Playing, Config, KeyMapDistance);
        ValLoad(Playing, Config, PlayerCharFrameTime);
        ValLoad(Playing, Config, PlayerCharFrames);
        ValLoad(Playing, Config, PlayerCharGoGoFrames);
        ValLoad(Playing, Config, PlayerCharClearFrames);
        ValLoad(Playing, Config, PlayerCharMissFrames);
        ValLoad(Playing, Config, PlayerCharLoopBeats);
        ValLoad(Playing, Config, PlayerCharJumpFrames);
        ValLoad(Playing, Config, PlayerCharJumpBeats);
        ValLoad(Playing, Config, PlayerCharJumpHeight);
        ValLoad(Playing, Config, PlayerCharColor);
        ValLoad(Playing, Config, PlayerCharGoGoLoopBeats);

        DataLoad(Playing, Font, Title);
        DataLoad(Playing, Font, SubTitle);
        DataLoad(Playing, Font, PlayerName);
        LoadTitleNameFont(Playingdata, Base->Playing.Font.TitleName, Base->Playing.Font.PlayerName, Base->Playing.Config.TitleNamePos, Base->Playing.Config.PlayerNamePos, 14.0f);
        DataLoad(Playing, Font, ExamName);
        DataLoad(Playing, Font, ExamVal);

        DataLoad(Playing, SE, Don);
        DataLoad(Playing, SE, Ka);
        DataLoad(Playing, SE, DonBig);
        DataLoad(Playing, SE, KaBig);
        DataLoad(Playing, SE, Balloon);
        DataLoad(Playing, SE, DanFall);
        DataLoad(Playing, SE, FullComboVoice);
        ValLoad(Playing, Config, FullComboVoiceDelay);
        // コンボボイス
        Base->Playing.SE.ComboVoice.clear();
        Releasers.push_back([p = &Base->Playing.SE.ComboVoice] { for (auto& v : *p) { v.second->Delete(); } p->clear(); });
        if (const json& voices = JsonChild(JsonChild(Playingdata, "SE"), "ComboVoice"); voices.is_array()) {
            for (const auto& v : voices) {
                if (!v.is_object() || !v.contains("Combo") || !v.contains("FilePath")) { continue; }
                const auto combo = v.value("Combo", (uint64_t)0);
                Base->Playing.SE.ComboVoice.push_back({combo,std::make_unique<SoundData>()});
                auto& sound = Base->Playing.SE.ComboVoice.back().second;
                sound->Load(SkinDir, v);
                if (sound->Handle == -1) { Base->Playing.SE.ComboVoice.pop_back(); }
            }
            std::ranges::sort(Base->Playing.SE.ComboVoice, {}, &std::pair<uint64_t, std::unique_ptr<SoundData>>::first);
        }

#pragma endregion

#pragma region Result

        ValLoad(Result, Config, TitlePos);
        ValLoad(Result, Config, SubTitlePos);
        ValLoad(Result, Config, PlayerNamePos);
        ValLoad(Result, Config, TitleNamePos);
        ValLoad(Result, Config, ScorePos);
        ValLoad(Result, Config, AccracyPos);
        ValLoad(Result, Config, GoodPos);
        ValLoad(Result, Config, OkPos);
        ValLoad(Result, Config, BadPos);
        ValLoad(Result, Config, RollPos);
        ValLoad(Result, Config, MaxComboPos);

        DataLoad(Result, Image, BackGround);
        DataLoad(Result, Image, JudgeScore);
        DataLoad(Result, Image, Accuracy);
        DataLoad(Result, Image, Score);
        DataLoad(Result, Image, Number);
        DataLoad(Result, Image, Crown);
        DataLoad(Result, Image, Back);

        DataLoad(Result, Font, PlayerName);
        LoadTitleNameFont(Resultdata, Base->Result.Font.TitleName, Base->Result.Font.PlayerName, Base->Result.Config.TitleNamePos, Base->Result.Config.PlayerNamePos, 2.0f);
        DataLoad(Result, Font, Title);
        DataLoad(Result, Font, SubTitle);

        DataLoad(Result, SE, Don);
        DataLoad(Result, SE, Ka);

#pragma endregion

#pragma region ConfigMenu

        ValLoad(ConfigMenu, Config, StringPos);
        ValLoad(ConfigMenu, Config, ValPos);
        ValLoad(ConfigMenu, Config, BoxDistance);

        DataLoad(ConfigMenu, Image, BackGround);
        DataLoad(ConfigMenu, Image, Box);
        DataLoad(ConfigMenu, Image, Back);

        DataLoad(ConfigMenu, Font, String);

        DataLoad(ConfigMenu, SE, Don);
        DataLoad(ConfigMenu, SE, Ka);

#pragma endregion

#pragma endregion

#undef ValLoad
#undef DataLoad

        SetUseASyncLoadFlag(FALSE);
        WaitHandleASyncLoadAll();
        for (auto& check:validators) { if (!check()) { throw std::runtime_error("更新された画像・音声・フォントを読み込めません"); } }

        IsLoading = false;

    }

    void Dispose() noexcept {
        // 画像・フォント・サウンドのハンドルを解放してから破棄する (スキン再読み込み時のリーク対策)
        for (auto& release : Releasers) {
            try { release(); } catch (...) {}
        }
        Releasers.clear();
        delete Base;
        Base = nullptr;
    }

    std::vector<std::function<void()>> Releasers;

    struct SkinInfo {
        std::string Name;
        std::string Path;
    };

    std::vector<SkinInfo> Infos;
    std::string FilePath;

    Rect2D<float> ResolutionRect;
    Rect2D<float> SimulationRect;

    struct Info {
        std::string Name;
        std::string Version;
        Pos2D<int> Resolution;
        Pos2D<float> SimulationDistance;
        int ColorBit = 32;
    } Info;

    struct _Base {
        struct _Title {
            struct _Config {
            } Config;
            struct _Image {
                GraphData BackGround;
                GraphData Logo;
            } Image;
            struct _Font {
            } Font;
            struct _SE {
                SoundData Don;
                SoundData Ka;
            } SE;
            struct _BGM {
            } BGM;
        } Title;
        struct _ModeSelect {
            struct _Config {
                Pos2D<float> ModesPos;
                float BoxDistance = 120;
            } Config;
            struct _Image {
                GraphData BackGround;
                GraphData Box;
            } Image;
            struct _Font {
                FontData Modes;
            } Font;
            struct _SE {
                SoundData Don;
                SoundData Ka;
            } SE;
            struct _BGM {
            } BGM;
        } ModeSelect;
        struct _SongSelect {
            struct _Config {
                Pos2D<float> BoxDistance;
                Pos2D<float> SongBoxListPos;
                Pos2D<float> BoxTitlePos;
                Pos2D<float> BoxSubTitlePos;
                Pos2D<float> CoursePos;
                Pos2D<float> LevelPos;
                Pos2D<float> HighScorePos;
                Pos2D<float> KeyWordPos;
                float CourseBoxDistance = 80;
                float BoxSlice = 0;                     // 箱の画像の角の大きさ (0 より大きいと 9 分割で伸ばす。角は伸ばさない)
            } Config;
            struct _Image {
                GraphData BackGround;
                GraphData Box;
                GraphData TitleBox;
                GraphData CourseBox;
                GraphData SearchBox;
                GraphData Crown;
                GraphData Back;
                GraphData Search;
                GraphData Config;
                GraphData Random;
            } Image;
            struct _Font {
                FontData Title;
                FontData SubTitle;
                FontData BoxTitle;
                FontData BoxSubTitle;
                FontData Course;
                FontData Level;
                FontData HighScore;
                FontData KeyWord;
            } Font;
            struct _SE {
                SoundData Don;
                SoundData Ka;
            } SE;
            struct _BGM {
            } BGM;
        } SongSelect;
        struct _DanSelect {
            struct _Config {
                float BoxSlice = 16;
                Pos2D<float> BoxDistance;
                Pos2D<float> SongBoxListPos;
                Pos2D<float> KeyWordPos;
            } Config;
            struct _Image {
                GraphData BackGround;
                GraphData Box;
                GraphData SearchBox;
                GraphData Crown;
                GraphData Back;
                GraphData Search;
                GraphData Config;
                GraphData Random;
            } Image;
            struct _Font {
                FontData Title;
                FontData SubTitle;
                FontData KeyWord;
            } Font;
            struct _SE {
                SoundData Don;
                SoundData Ka;
            } SE;
            struct _BGM {
            } BGM;
        } DanSelect;
        struct _MultiRoom {
            struct _Config {
                Pos2D<float> PlayerPos;
                Pos2D<float> TitlePos;
                Pos2D<float> SubTitlePos;
                Pos2D<float> CoursePos;
                Pos2D<float> LevelPos;
                float BoxDistance = 120;
            } Config;
            struct _Image {
                GraphData BackGround;
                GraphData PlayerBox;
                GraphData TitleBox;
                GraphData Crown;
                GraphData Back;
                GraphData Config;
                GraphData Host;
            } Image;
            struct _Font {
                FontData String;
                FontData Player;
                FontData Title;
                FontData SubTitle;
                FontData Course;
                FontData Level;
            } Font;
            struct _SE {
                SoundData Don;
                SoundData Ka;
            } SE;
            struct _BGM {
            } BGM;
        } MultiRoom;
        struct _Playing {
            struct _Config {
                Pos2D<float> TitlePos;
                Pos2D<float> SubTitlePos;
                Pos2D<float> PlayerNamePos;
                Pos2D<float> TitleNamePos{ NAN, NAN };
                Pos2D<float> ExamNamePos;
                Pos2D<float> ExamValPos;
                float MultiPlayLaneDistance = 175;
                double LaneExtendRate = 0.948;
                double JudgeUpperExplosionFrameTime = 12;
                double GoGoFireFrameTime = 60;
                double FullComboVoiceDelay = 1000;
                float KeyMapDistance = 128;
                double PlayerCharFrameTime = 100;
                double PlayerCharLoopBeats = 0, PlayerCharJumpBeats = 2;
                float PlayerCharJumpHeight = 0;
                uint32_t PlayerCharColor = 0xffffff;
                double PlayerCharGoGoLoopBeats = 0;
                std::vector<int> PlayerCharJumpFrames;
                std::vector<int> PlayerCharFrames, PlayerCharGoGoFrames, PlayerCharClearFrames, PlayerCharMissFrames;
            } Config;
            struct _Image {
                GraphData BackGround;


                GraphData DanBackGround;
                GraphData LaneFrame;
                GraphData Lane;
                GraphData NormalLane;
                GraphData ExpertLane;
                GraphData MasterLane;
                GraphData Base;
                GraphData NamePlate;
                GraphData MiniTaiko;
                GraphData MiniTaiko_Don;
                GraphData MiniTaiko_Ka;
                GraphData ComboNumber;
                GraphData ScoreNumber;
                GraphData RollNumber;
                GraphData Note;
                GraphData JudgeUnderExplosion;
                GraphData JudgeUpperExplosion;
                GraphData GoGoFire;
                GraphData JudgeString;
                GraphData ProgressBar;
                GraphData ExamProgressBar;
                GraphData Back;
                GraphData Pause;
                GraphData PauseShade, PausePanel, PauseResume, PauseRestart, PauseExit, PauseCursor;
                GraphData KeyMapBox;
                GraphData PlayerChar;
            } Image;
            struct _Font {
                FontData Title;
                FontData SubTitle;
                FontData PlayerName;
                FontData TitleName;
                FontData ExamName;
                FontData ExamVal;
            } Font;
            struct _SE {
                SoundData Don;
                SoundData Ka;
                SoundData Balloon;
                SoundData DanFall;
                SoundData DonBig, KaBig;
                SoundData FullComboVoice;
                std::vector<std::pair<uint64_t, std::unique_ptr<SoundData>>> ComboVoice;   // コンボ数ごとの声 ("ComboVoice": [{ "Combo": 50, "FilePath": "..." }])
            } SE;
            struct _BGM {
            } BGM;
        } Playing;
        struct _Result {
            struct _Config {
                Pos2D<float> TitlePos;
                Pos2D<float> SubTitlePos;
                Pos2D<float> PlayerNamePos;
                Pos2D<float> TitleNamePos{ NAN, NAN };
                Pos2D<float> ScorePos;
                Pos2D<float> AccracyPos;
                Pos2D<float> GoodPos;
                Pos2D<float> OkPos;
                Pos2D<float> BadPos;
                Pos2D<float> RollPos;
                Pos2D<float> MaxComboPos;
            } Config;
            struct _Image {
                GraphData BackGround;
                GraphData JudgeScore;
                GraphData Accuracy;
                GraphData Score;
                GraphData Number;


                GraphData Crown;

                GraphData Back;
            } Image;
            struct _Font {
                FontData Title;
                FontData SubTitle;
                FontData PlayerName;
                FontData TitleName;

            } Font;
            struct _SE {
                SoundData Don;
                SoundData Ka;
            } SE;
            struct _BGM {

            } BGM;
        } Result;
        struct _GameConfig {
            struct _Config {
                Pos2D<float> StringPos;
                Pos2D<float> ValPos;
                float BoxDistance = 80;
            } Config;
            struct _Image {
                GraphData BackGround;
                GraphData Box;
                GraphData Back;
            } Image;
            struct _Font {
                FontData String;
            } Font;
            struct _SE {
                SoundData Don;
                SoundData Ka;
            } SE;
            struct _BGM {
            } BGM;
        } ConfigMenu;
    } *Base = nullptr;
};

// 次郎形式の切り捨て (0 の方へ)。2 進数の誤差で 1000.9999… になった値は 1001 とみなす (10 進では割り切れる長さ)
inline double JiroTrunc(double v) {
    if (!std::isfinite(v)) { return v; }
    const double nearest = std::nearbyint(v);
    return std::abs(v - nearest) <= 1e-9 * std::max(1.0, std::abs(v)) ? nearest : std::trunc(v);
}

#ifdef __ANDROID__
enum class TouchType {
    Null = -1,
    Don,
    Ka,
    LeftDon,
    RightDon,
    LeftKa,
    RightKa,
    Other
};

struct Ellipse {
    Pos2D<int> center = { 640, 680 };
    double rx = 560.0;
    double ry = 480.0;
    double angle = 90.0;

    void Load(const _Skin& skin, const _ConfigData& config) {
        center = { skin.Info.Resolution.X / 2, skin.Info.Resolution.Y - 40 };
        rx = config.TouchTaikoWidth;
        ry = config.TouchTaikoHeight;
    }

    void Draw() {
        SetDrawBlendMode(DX_BLENDGRAPHTYPE_ALPHA, 255 / 3);
        DrawOvalAA(
            center.X, center.Y,
            rx, ry,
            angle,
            GetColor(0, 0, 0),
            FALSE);
        DrawOvalAA(
            center.X, center.Y,
            rx - 10.0f, ry - 10.0f,
            angle,
            GetColor(255, 255, 255),
            TRUE);
        SetDrawBlendMode(DX_BLENDGRAPHTYPE_ALPHA, 255);
    }

    constexpr bool is_inside(int x, int y) {
        double dx = (x - center.X) / rx;
        double dy = (y - center.Y) / ry;
        return ((dx * dx + dy * dy) <= 1.0);
    }
};

struct TouchData {
    Ellipse Geometry;
    int ExternalPressed = 0;

    TOUCHINPUTPOINT List[TOUCHINPUTPOINT_MAX];
    int Num = 0;

    // 面の外は画面全体を縁として扱い、メニューのボタンだけを呼び出し元で除外する。
    template<typename Func>
    void ProcessTaiko(TouchType type, const Func& func, const std::function<bool(float, float)>& excluded = {}) {
        ProcessExternal(type, func);
        for (int i = 0; i < Num; ++i) {
            if (excluded && excluded((float)List[i].PositionX, (float)List[i].PositionY)) { continue; }
            TOUCHINPUTPOINT one = List[i];
            TouchData single;
            single.Geometry = Geometry;
            single.List[0] = one;
            single.Num = 1;
            single.Process(type, func, GraphData(), false);
        }
    }
    template<typename Func>
    void ProcessExternal(TouchType type, const Func& func) {
        const int mask = type == TouchType::LeftKa ? 1 : type == TouchType::LeftDon ? 2 :
            type == TouchType::RightDon ? 4 : type == TouchType::RightKa ? 8 :
            type == TouchType::Don ? 6 : type == TouchType::Ka ? 9 : 0;
        for (int lane = 0; lane < 4; ++lane) {
            if (!(mask & (1 << lane))) { continue; }
            for (int count = (ExternalPressed >> (lane * 4)) & 15; count > 0; --count) { func(); }
        }
    }
    template<typename Func>
    void Process(TouchType type, const Func& func, GraphData graph = GraphData(), bool external = true) {
        if (external) { ProcessExternal(type, func); }
        for (int i = 0; i < Num; ++i) {
            TouchType result = TouchType::Null;
            bool is_inside = Geometry.is_inside(List[i].PositionX, List[i].PositionY);
            bool LRFlag = Geometry.center.X <= List[i].PositionX;
            switch (type) {
            case TouchType::Other:
                if ((graph.Pos.X - (graph.Size.Width / 2)) < List[i].PositionX &&
                    (graph.Pos.Y - (graph.Size.Height / 2)) < List[i].PositionY &&
                    (graph.Pos.X + (graph.Size.Width / 2)) > List[i].PositionX &&
                    (graph.Pos.Y + (graph.Size.Height / 2)) > List[i].PositionY) {
                    func();
                }
                break;
            case TouchType::Don:
                if (is_inside) {
                    func();
                }
                break;
            case TouchType::LeftDon:
                if (is_inside && !LRFlag) {
                    func();
                }
                break;
            case TouchType::RightDon:
                if (is_inside && LRFlag) {
                    func();
                }
                break;
            case TouchType::Ka:
                if (!is_inside) {
                    func();
                }
                break;
            case TouchType::LeftKa:
                if (!is_inside && !LRFlag) {
                    func();
                }
                break;
            case TouchType::RightKa:
                if (!is_inside && LRFlag) {
                    func();
                }
                break;
            }
        }
    }
};

#endif

enum class BranchType {
    Null,
    Normal,
    Expert,
    Master,
    NotDisplay,
};

enum class IfBranchType {
    Null,
    Perfect,
    Roll,
    Score,
};

enum class ABranchType {
    Null,
    Normal_Expert,
    Normal_Master,
    Expert_Normal,
    Expert_Master,
    Master_Normal,
    Master_Expert,
};

enum class JudgeType {
    None = -1,
    Good,
    Ok,
    Bad,
    Roll
};

struct BranchJudge {
    BranchType NowBranchFlag = BranchType::Null;
    ABranchType NowBranchAnimation = ABranchType::Null;
    bool LevelHold = false;
    uint64_t Score = 0;
    uint64_t Good = 0;
    uint64_t Ok = 0;
    uint64_t Bad = 0;
    uint64_t Roll = 0;
    uint64_t HitNote = 0;
    double Accuracy = 0.0;

    void Init() {
        Score = 0;
        Good = 0;
        Ok = 0;
        Bad = 0;
        HitNote = 0;
        Roll = 0;
        Accuracy = 0.0;
    }

    auto operator<=>(const BranchJudge&) const = default;
};

struct RollData {
    uint64_t NowCount = 0;
    bool IsEnd = false;

    void Init() {
        NowCount = 0;
        IsEnd = false;
    }

    auto operator<=>(const RollData&) const = default;
};

struct JudgeData {

    JudgeType HitJudge = JudgeType::None;
    BranchJudge Branch = BranchJudge();
    RollData Rolls = RollData();
    uint64_t Score = 0;
    uint64_t Good = 0;
    uint64_t Ok = 0;
    uint64_t Bad = 0;
    uint64_t Roll = 0;
    uint64_t Combo = 0;
    uint64_t MaxCombo = 0;
    uint64_t HitNote = 0;
    double ScoreRateGood = 0.0;
    double ScoreRateOk = 0.0;
    double Accuracy = 0;
    char NoteType = '\0';

    void Hit(JudgeType type, uint64_t addscore, char note) {

        HitJudge = type;
        NoteType = note;

        switch (type) {
        case JudgeType::Good:
            ++Good;
            ++Branch.Good;
            ++Combo;
            Score = numeric::Cast<uint64_t>(static_cast<long double>(Score) + addscore * ScoreRateGood);
            Branch.Score = numeric::Cast<uint64_t>(static_cast<long double>(Branch.Score) + addscore * ScoreRateGood);
            ++HitNote;
            ++Branch.HitNote;
            break;
        case JudgeType::Ok:
            ++Ok;
            ++Branch.Ok;
            ++Combo;
            Score = numeric::Cast<uint64_t>(static_cast<long double>(Score) + addscore / 2 * ScoreRateOk);
            Branch.Score = numeric::Cast<uint64_t>(static_cast<long double>(Branch.Score) + addscore / 2 * ScoreRateOk);
            ++HitNote;
            ++Branch.HitNote;
            break;
        case JudgeType::Bad:
            ++Bad;
            ++Branch.Bad;
            Combo = 0;
            ++HitNote;
            ++Branch.HitNote;
            break;
        case JudgeType::Roll:
            ++Roll;
            ++Branch.Roll;
            Score += 100;
            break;
        }

        if (Combo > MaxCombo) { ++MaxCombo; }

        // 精度 (良 = 1, 可 = 0.5)。以前は今回の判定を数える前に計算していたため 1 打ずつ遅れていた
        if (HitNote != 0) {
            Accuracy = ((Good / (double)HitNote) + ((Ok / (double)HitNote) * 0.5)) * 100;
        }
        if (Branch.HitNote != 0) {
            Branch.Accuracy = ((Branch.Good / (double)Branch.HitNote) + ((Branch.Ok / (double)Branch.HitNote) * 0.5)) * 100;
        }
    }

    auto operator<=>(const JudgeData&) const = default;
};

enum class HitType : int {
    Null = -2,
    Empty,
    DonLeft,
    KaLeft,
    DonRight,
    KaRight,
    DonBig,
    KaBig,
    Enter,
    Back
};
