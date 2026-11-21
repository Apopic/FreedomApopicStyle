#pragma once
#ifndef __ANDROID__
#include "resource.h"
#endif
#include "Include.h"

#ifndef __ANDROID__

const fs::path& GetExecutablePath() {
    // スコア・設定・譜面読み込みなどで頻繁に呼ばれるため、起動後 1 回だけ求める
    static const fs::path path = [] {
        std::vector<wchar_t> buffer(MAX_PATH);
        while (true) {
            DWORD len = GetModuleFileNameW(NULL, buffer.data(), (DWORD)buffer.size());
            if (len == 0) return fs::path();
            if (len < buffer.size()) {
                return fs::path(buffer.data()).parent_path();
            }
            buffer.resize(buffer.size() * 2);
        }
    }();
    return path;
}

#else

const fs::path& GetExecutablePath() {
    static const fs::path dir = [] {
        fs::path d = "/storage/emulated/0/Documents/FAS";
        std::error_code ec;
        fs::create_directories(d, ec);
        return d;
    }();
    return dir;
}

void ShowToast(const std::string& text) {

    JNIEnv* env;
    const ANativeActivity* NativeActivity;
    NativeActivity = GetNativeActivity();

    if (NativeActivity->vm->AttachCurrentThreadAsDaemon(&env, NULL) != JNI_OK) {
        return;
    }

    jclass jclass_MainActivity = env->GetObjectClass(NativeActivity->clazz);
    jmethodID jmethodID_ShowToast = env->GetMethodID(jclass_MainActivity, "ShowToast", "(Ljava/lang/String;)V");
    jstring jstr = env->NewStringUTF(text.c_str());
    env->CallVoidMethod(NativeActivity->clazz, jmethodID_ShowToast, jstr);
    env->DeleteLocalRef(jstr);
    env->DeleteLocalRef(jclass_MainActivity);
    NativeActivity->vm->DetachCurrentThread();
}

std::string GetInputText(std::string theme = "") {

    JNIEnv* env;
    const ANativeActivity* NativeActivity;
    int InputEnd;
    char InputString[1024] = {};

    NativeActivity = GetNativeActivity();

    {
        if (NativeActivity->vm->AttachCurrentThreadAsDaemon(&env, NULL) != JNI_OK) {
            return "";
        }

        jclass jclass_MainActivity = env->GetObjectClass(NativeActivity->clazz);
        jmethodID jmethodID_StartInputDialog = env->GetMethodID(jclass_MainActivity, "StartInputStringDialog", "(Ljava/lang/String;)V");
        jstring jtheme = env->NewStringUTF(theme.c_str());
        env->CallVoidMethod(NativeActivity->clazz, jmethodID_StartInputDialog, jtheme);
        env->DeleteLocalRef(jtheme);
        env->DeleteLocalRef(jclass_MainActivity);
        NativeActivity->vm->DetachCurrentThread();
    }

    InputEnd = 0;

    while (ProcessMessage() == 0 && InputEnd == 0) {

        if (NativeActivity->vm->AttachCurrentThreadAsDaemon(&env, NULL) != JNI_OK) {
            return "";
        }

        jclass jclass_MainActivity = env->GetObjectClass(NativeActivity->clazz);
        jfieldID jfieldID_InputEnd = env->GetFieldID(jclass_MainActivity, "InputEnd", "I");
        InputEnd = env->GetIntField(NativeActivity->clazz, jfieldID_InputEnd);

        if (InputEnd == 1) {
            jfieldID jfieldID_InputString = env->GetFieldID(jclass_MainActivity, "InputString",
                "Ljava/lang/String;");
            jstring jstring_InputString = (jstring)env->GetObjectField(NativeActivity->clazz,
                jfieldID_InputString);
            const char* chars_InputString = env->GetStringUTFChars(jstring_InputString, NULL);
            strncpy(InputString, chars_InputString, sizeof(InputString) - 1);
            env->ReleaseStringUTFChars(jstring_InputString, chars_InputString);
            env->DeleteLocalRef(jstring_InputString);
        }
        env->DeleteLocalRef(jclass_MainActivity);
        NativeActivity->vm->DetachCurrentThread();
    }
    return InputString;
}
#endif

class _ConfigData {
public:

    _ConfigData() {
        Load();
    }

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
        JSONDATA(RecordPlay);
        JSONDATA(RecordFps);
        JSONDATA(RecordCrf);
        JSONDATA(RecordCodec);
        JSONDATA(RecordFormat);
        JSONDATA(RecordAudio);
        JSONDATA(ScoreMeter);
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

#ifndef __ANDROID__
        UpdateKeyNames();
#endif
    }

    void Write() {

#define JSONDATA(name) {#name, name}
        nlohmann::ordered_json data = {
                JSONDATA(PlayerName),
                JSONDATA(TitleName),
                JSONDATA(AutoPlay),
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
                JSONDATA(RecordPlay),
                JSONDATA(RecordFps),
                JSONDATA(RecordCrf),
                JSONDATA(RecordCodec),
                JSONDATA(RecordFormat),
                JSONDATA(RecordAudio),
                JSONDATA(ScoreMeter),
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
        ofs << data.dump(4) << "\n";
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
    bool TrainingMode = false;
    bool HitNoteDisp = true;
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
    std::vector<int> EditorGrids{ 4, 8, 12, 16, 24, 32, 48 };    // 譜面制作でよく使うグリッドの分割数
    bool RecordPlay = false;            // 演奏を録画して recordings フォルダに MP4 で保存する
    int RecordFps = 60;
    int RecordCrf = 20;                 // 画質 (libx264 の CRF。小さいほど高画質・大きいファイル)
    std::string RecordCodec = "libx264";    // 映像のエンコーダ (libx264 / h264_nvenc / h264_qsv / h264_amf)
    std::string RecordFormat = "mp4";       // 保存する形式 (mp4 / mkv / mov / webm / gif / m4a / mp3 / wav / flac)
    int RecordAudio = 0;                    // 0: ゲームの音を録音 (録れなかった時は譜面から作る) / 1: 譜面から作る (曲 + 効果音)
    bool ScoreMeter = false;            // 演奏中に判定のずれをメーターで表示する
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

} Config;

class _Skin {
public:

    void Init() {
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(GetExecutablePath() / "Skins", ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (it->path().extension() != ".json") {
                continue;
            }
            std::ifstream ifs(it->path());
            json data = json::parse(ifs, nullptr, false);
            if (data.is_discarded() || !data.is_object() || !data.contains("Info") || !data["Info"].is_object()) {
                continue;
            }
            Infos.push_back(
                SkinInfo{
                        data["Info"].value("Name", std::string()),
                        it->path().string()
                }
            );
        }
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

    void Load(const std::string& name) {
        IsLoading = true;
        auto found = std::ranges::find(Infos, name, &SkinInfo::Name);
        if (found != Infos.end()) {
            FilePath = found->Path;
        }
        else if (FilePath.empty() && !Infos.empty()) {
            // 指定スキンが見つからない場合は先頭のスキンで起動する
            FilePath = Infos.front().Path;
        }

        std::ifstream ifs(GetExecutablePath() / FilePath);
        json data = json::parse(ifs);
        ifs.close();

#ifndef __ANDROID__
        std::string SkinDir = fs::path(FilePath).parent_path().string() + "\\";
#else
        std::string SkinDir = std::string(GetExecutablePath() / fs::path(FilePath).parent_path()) + "\\";
#endif

        const json& Infodata = data["Info"];
        const json& Titledata = data["Base"]["Title"];
        const json& ModeSelectdata = data["Base"]["ModeSelect"];
        const json& SongSelectdata = data["Base"]["SongSelect"];
        const json& DanSelectdata = data["Base"]["DanSelect"];
        const json& MultiRoomdata = data["Base"]["MultiRoom"];
        const json& Playingdata = data["Base"]["Playing"];
        const json& Resultdata = data["Base"]["Result"];
        const json& ConfigMenudata = data["Base"]["ConfigMenu"];

        Info.Name = Infodata["Name"];
        Info.Version = Infodata["Version"];
        Info.Resolution = Infodata["Resolution"].get<Pos2D<int>>();
        Info.SimulationDistance = Infodata["SimulationDistance"].get<Pos2D<float>>();
        Info.ColorBit = Infodata["ColorBit"];

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

#define ValLoad(base, type, keyname) \
        if (const json& _j = JsonChild(base##data, #type); _j.is_object()) { Base->base.type.keyname = _j.value(#keyname, Base->base.type.keyname); }
#define DataLoad(base, type, keyname) \
        Base->base.type.keyname.Load(SkinDir, JsonChild(JsonChild(base##data, #type), #keyname)); \
        Releasers.push_back([p = &Base->base.type.keyname] { p->Delete(); })

        // 称号のフォント・位置。スキンに TitleName / TitleNamePos が無ければ、名前のフォントを 3/4 の大きさにして名前の上に置く
        auto LoadTitleNameFont = [&](const json& basedata, FontData& title, const FontData& name, Pos2D<float>& titlepos, const Pos2D<float>& namepos, float gap) {
            const json& fonts = JsonChild(basedata, "Font");
            if (const json& tj = JsonChild(fonts, "TitleName"); tj.is_object()) {
                title.Load(SkinDir, tj);
            }
            else if (const json& nj = JsonChild(fonts, "PlayerName"); nj.is_object()) {
                json j = nj;
                j["Size"] = std::max(1.0f, name.Size * 0.75f);
                title.Load(SkinDir, j);
            }
            Releasers.push_back([p = &title] { p->Delete(); });
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

        DataLoad(Playing, Font, Title);
        DataLoad(Playing, Font, SubTitle);
        DataLoad(Playing, Font, PlayerName);
        LoadTitleNameFont(Playingdata, Base->Playing.Font.TitleName, Base->Playing.Font.PlayerName, Base->Playing.Config.TitleNamePos, Base->Playing.Config.PlayerNamePos, 14.0f);
        DataLoad(Playing, Font, ExamName);
        DataLoad(Playing, Font, ExamVal);

        DataLoad(Playing, SE, Don);
        DataLoad(Playing, SE, Ka);
        DataLoad(Playing, SE, Balloon);
        DataLoad(Playing, SE, DanFall);
        ValLoad(Playing, Config, PlayerCharFrameTime);
        ValLoad(Playing, Config, RunnerFrameTime);
        ValLoad(Playing, Config, RunnerTime);
        ValLoad(Playing, Config, RunnerFrom);
        ValLoad(Playing, Config, RunnerTo);
        ValLoad(Playing, Config, RunnerMax);
        ValLoad(Playing, Config, DancerFrameTime);
        ValLoad(Playing, Config, DancerCount);
        ValLoad(Playing, Config, DancerDistance);
        DataLoad(Playing, Image, PlayerChar);
        DataLoad(Playing, Image, PlayerCharGoGo);
        DataLoad(Playing, Image, PlayerCharClear);
        DataLoad(Playing, Image, PlayerCharCombo);
        DataLoad(Playing, Image, Runner);
        DataLoad(Playing, Image, Dancer);
        // コンボボイス
        Base->Playing.SE.ComboVoice.clear();
        if (const json& voices = JsonChild(JsonChild(Playingdata, "SE"), "ComboVoice"); voices.is_array()) {
            for (const auto& v : voices) {
                if (!v.is_object() || !v.contains("Combo") || !v.contains("FilePath")) { continue; }
                auto sound = std::make_unique<SoundData>();
                sound->Load(SkinDir, v);
                if (sound->Handle == -1) { continue; }
                Base->Playing.SE.ComboVoice.push_back({ v.value("Combo", (uint64_t)0), std::move(sound) });
            }
            std::ranges::sort(Base->Playing.SE.ComboVoice, {}, &std::pair<uint64_t, std::unique_ptr<SoundData>>::first);
        }
        Releasers.push_back([p = &Base->Playing.SE.ComboVoice] { for (auto& v : *p) { v.second->Delete(); } p->clear(); });

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

        IsLoading = false;

    }

    void Dispose() {
        // 画像・フォント・サウンドのハンドルを解放してから破棄する (スキン再読み込み時のリーク対策)
        for (auto& release : Releasers) {
            release();
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
                float BoxDistance;
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
                float CourseBoxDistance;
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
                float BoxDistance;
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
                float MultiPlayLaneDistance;
                double LaneExtendRate;
                double JudgeUpperExplosionFrameTime;
                double GoGoFireFrameTime;
                // キャラクター (スキンに画像がある時だけ表示する。デフォルトスキンには無い)
                double PlayerCharFrameTime = 100;       // 1 コマの長さ (ms)
                double RunnerFrameTime = 60;
                double RunnerTime = 1200;               // ランナーが走り抜ける時間 (ms)
                Pos2D<float> RunnerFrom{ 0, 0 };        // ランナーの出発位置 (Runner の Pos からのずれ)
                Pos2D<float> RunnerTo{ 1280, 0 };       // ランナーの到着位置
                int RunnerMax = 16;                     // 同時に走るランナーの数の上限
                double DancerFrameTime = 100;
                int DancerCount = 5;                    // 踊り子の人数 (クリアの目安までに 1 人ずつ増える)
                Pos2D<float> DancerDistance{ 160, 0 };  // 踊り子の間隔
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
                GraphData PlayerChar;           // 通常 (横に並べたコマを繰り返す)
                GraphData PlayerCharGoGo;       // ゴーゴータイム中 (無ければ通常)
                GraphData PlayerCharClear;      // クリアの目安を超えている時 (無ければ通常)
                GraphData PlayerCharCombo;      // 10 コンボごとに 1 回流す
                GraphData Runner;               // 良・可で叩くたびに走り抜ける (縦に並べた段は種類。順番に使う)
                GraphData Dancer;               // 踊り子 (縦に並べた段が 1 人ずつ)
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
                float BoxDistance;
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
} Skin;

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

    void Load() {
        center = { Skin.Info.Resolution.X / 2, Skin.Info.Resolution.Y - 40 };
        rx = Config.TouchTaikoWidth;
        ry = Config.TouchTaikoHeight;
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
} ellipse;

struct TouchData {

    TOUCHINPUTPOINT List[TOUCHINPUTPOINT_MAX];
    int Num = 0;

    template<typename Func>
    void Process(TouchType type, const Func& func, GraphData graph = GraphData()) {
        for (int i = 0; i < Num; ++i) {
            TouchType result = TouchType::Null;
            bool is_inside = ellipse.is_inside(List[i].PositionX, List[i].PositionY);
            bool LRFlag = ellipse.center.X <= List[i].PositionX;
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
            Score += addscore * ScoreRateGood;
            Branch.Score += addscore * ScoreRateGood;
            ++HitNote;
            ++Branch.HitNote;
            break;
        case JudgeType::Ok:
            ++Ok;
            ++Branch.Ok;
            ++Combo;
            Score += addscore / 2 * ScoreRateOk;
            Branch.Score += addscore / 2 * ScoreRateOk;
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

class Game {
public:

    enum class Scene {
        Null,
        End,
        Title,
        ModeSelect,
        SongSelect,
        DanSelect,
        MultiRoom,
        Loading,
        Playing,
        Result,
        ConfigMenu,
        Editor,
        Count
    };

    Scene NowScene = Scene::Title;
    Scene MemScene = Scene::Null;
    Scene PrevScene = Scene::Null;

#ifdef __ANDROID__
    TouchData Touch;
#endif

    bool IsDanSelect() const {
        return (NowScene == Scene::DanSelect);
    }

    void TitleInit() {
        Skin.Base->Title.SE.Don.SetVolume(Config.SEVolume);
        Skin.Base->Title.SE.Ka.SetVolume(Config.SEVolume);
    }
    void TitleDraw() {
        Skin.Base->Title.Image.BackGround.Draw({});
        Skin.Base->Title.Image.Logo.Draw({});
    }
    void TitleProc() {

        auto DonKeyProc = [&] {
            Skin.Base->Title.SE.Don.Play();
            NowScene = Scene::ModeSelect;
            };

#ifndef __ANDROID__
        Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] {
            EndFlag = true;
            });
        Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, DonKeyProc);
        Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, DonKeyProc);
        Input.HitKeyProcess(VK_RETURN, KeyState::Down, DonKeyProc);
#else
        Touch.Process(TouchType::Don, DonKeyProc);
#endif
    }

    enum class Mode {
        Single,
        Multi,
        Dan,
        Edit,
        Config,
        End,
        Count
    } ModeSelector = Mode::Single;

    void ModeSelectInit() {
        Chart.Init(true);
        DemoSongPlayBlank.Reset();
        DemoSong.Delete();
        BoxDataIndex = 0;
        Skin.Base->ModeSelect.SE.Don.SetVolume(Config.SEVolume);
        Skin.Base->ModeSelect.SE.Ka.SetVolume(Config.SEVolume);
#ifndef __ANDROID__
#endif
    }

    // モードの箱の間隔 (箱の高さの 1.25 倍まで詰め、画面に収まらなければさらに詰める)
    float ModeBoxDistance() const {
        const auto& box = Skin.Base->ModeSelect.Image.Box;
        float d = Skin.Base->ModeSelect.Config.BoxDistance;
        if (box.Size.Height > 0) { d = std::min(d, box.Size.Height * 1.18f); }
        const float room = (float)Skin.Info.Resolution.Y - box.Size.Height - 32.0f;
        return std::max(1.0f, std::min(d, room / std::max((int)Mode::Count - 1, 1)));
    }
    // 箱の並び全体を画面の中央より少し上に置くための縦のずらし (スキンの位置からの差)
    float ModeBoxOffset() const {
        const auto& box = Skin.Base->ModeSelect.Image.Box;
        const float resy = (float)Skin.Info.Resolution.Y;
        const float block = ModeBoxDistance() * ((int)Mode::Count - 1);
        const float top = std::max(box.Size.Height * 0.5f + 16.0f, (resy - block) * 0.5f);
        return top - box.Pos.Y;
    }
    // ホイールで素早く動かした時に移動の音が重ならないよう、鳴らす間隔を空ける
    std::chrono::steady_clock::time_point WheelSoundAt{};
    bool WheelSoundReady() {
        const auto now = std::chrono::steady_clock::now();
        if (now - WheelSoundAt < std::chrono::milliseconds(35)) { return false; }
        WheelSoundAt = now;
        return true;
    }
    void ModeSelectDraw() {
        Skin.Base->ModeSelect.Image.BackGround.Draw({ 0, 0 });
        const float distance = ModeBoxDistance();
        const float offset = ModeBoxOffset();
        for (size_t i = 0; i < (size_t)Mode::Count; ++i) {
            unsigned int c = 100 * ((size_t)ModeSelector == i);
            SetDrawAddColor(c, c, c);
            Skin.Base->ModeSelect.Image.Box.Draw({ 0, offset + distance * i });
            SetDrawAddColor(0, 0, 0);
            Skin.Base->ModeSelect.Font.Modes.Draw({
            Skin.Base->ModeSelect.Config.ModesPos.X,
            Skin.Base->ModeSelect.Config.ModesPos.Y + offset + (distance * i) }, GetColor(255, 255, 255), GetColor(0, 0, 0), magic_enum::enum_name((Mode)i).data());
        }
    }

    void ModeSelectProc() {

        auto BackProc = [&] {
            NowScene = Scene::Title;
            };

        auto DonKeyProc = [&] {
            Skin.Base->ModeSelect.SE.Don.Play();
            switch (ModeSelector) {
            case Mode::Single:
                NowScene = Scene::SongSelect;
                break;
            case Mode::Multi:
                NowScene = Scene::MultiRoom;
                break;
            case Mode::Dan:
                NowScene = Scene::DanSelect;
                break;
            case Mode::Edit:
                NowScene = Scene::Editor;
                break;
            case Mode::Config:
                PrevScene = Scene::ModeSelect;
                NowScene = Scene::ConfigMenu;
                break;
            case Mode::End:
                NowScene = Scene::Title;
                break;
            }
            };

        auto KaKeyProc = [&](bool direction) {
            Skin.Base->ModeSelect.SE.Ka.Play();
            constexpr int count = (int)Mode::Count;
            ModeSelector = (Mode)(((int)ModeSelector + (direction ? 1 : -1) + count) % count);
            };

#ifndef __ANDROID__
        if (const int wheel = Input.GetMouseWheel(); wheel != 0) {
            KaKeyProc(wheel < 0);
        }

        Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, BackProc);
        Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, DonKeyProc);
        Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, DonKeyProc);
        Input.HitKeyProcess(VK_RETURN, KeyState::Down, DonKeyProc);

        Input.HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { KaKeyProc(false); }, Config.KeyHoldProcInterval);
        Input.HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { KaKeyProc(true); }, Config.KeyHoldProcInterval);
        Input.HitKeyesProcess({ VK_UP,VK_LEFT }, KeyState::Down, [&] { KaKeyProc(false); }, Config.KeyHoldProcInterval);
        Input.HitKeyesProcess({ VK_DOWN,VK_RIGHT }, KeyState::Down, [&] { KaKeyProc(true); }, Config.KeyHoldProcInterval);
#else
        Touch.Process(TouchType::Don, DonKeyProc);
        Touch.Process(TouchType::LeftKa, [&] {KaKeyProc(false);});
        Touch.Process(TouchType::RightKa, [&] {KaKeyProc(true);});
#endif
    }

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
    static std::string PathToU8(const fs::path& path) {
        auto u8 = path.u8string();
        return std::string(u8.begin(), u8.end());
    }
    static fs::path U8ToPath(const std::string& str) {
        return fs::path(std::u8string(str.begin(), str.end()));
    }

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
        std::string SongLink = "";
        std::string MovieLink = "";
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
        bool Load(const fs::path& path, const TextLines* preloaded = nullptr, bool measure = false) {
            TextLines owned;
            const TextLines* text = preloaded;
            if (text == nullptr) {
                if (!owned.Load(path)) {
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
                                    song.TitleStrlen = GetStrlen(song.Title, Skin.Base->Playing.Font.Title.Handle);
                                    song.SubtitleStrlen = GetStrlen(song.Subtitle, Skin.Base->Playing.Font.SubTitle.Handle);
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
                            TitleStrlen.Playing = GetStrlen(data, Skin.Base->Playing.Font.Title.Handle);
                            TitleStrlen.Result = GetStrlen(data, Skin.Base->Result.Font.Title.Handle);
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
                            SubtitleStrlen.Playing = GetStrlen(data, Skin.Base->Playing.Font.SubTitle.Handle);
                            SubtitleStrlen.Result = GetStrlen(data, Skin.Base->Result.Font.SubTitle.Handle);
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
                    Exsubstr(line, "SONGLINK:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        SongLink = data;
                        });
                    Exsubstr(line, "MOVIELINK:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        MovieLink = data;
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
            catch (...) {
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
                { "sl", SongLink }, { "ml", MovieLink }, { "dan", IsDan },
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
                SongLink = j.at("sl").get<std::string>();
                MovieLink = j.at("ml").get<std::string>();
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

        bool IsGenre() {
            return m_Genre != nullptr;
        };

        Color3<int> GenreColor{};
        Color3<int> FontColor{};
        Color3<int> FontEdgeColor{};

        ChartData* GetChart() {
            return m_Chart;
        };
        GenreData* GetGenre() {
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
    static bool GenreHasCharts(const GenreData& genre, bool dan) {
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

    int BoxDataIndex = 0;
    Timer DemoSongPlayBlank;
    double DemoSongPlayBlankTime() const {
        return 0.25;
    }

    struct ScoreData {
        uint64_t Score = 0;
        int Crown = 0;
    };

    SoundData DemoSong = SoundData();

    size_t CourseIndex = (size_t)CourseType::Easy;
    bool IsCourseSelect = false;

    // ---- 譜面キャッシュ ----
    // 譜面ファイルのサイズと更新日時が一致すればパースを省略する。形式を変えたら ChartCacheVersion を上げること。
    static constexpr int ChartCacheVersion = 3;

    struct ChartScanItem {
        ChartData* Chart = nullptr;
        std::string FolderGenre;
        uintmax_t Size = 0;
        int64_t WriteTime = 0;
    };

    // ディレクトリを 1 回だけ列挙し、genre.ini があればジャンルとしてまとめる
    void ScanChartDirectory(std::vector<std::unique_ptr<BoxData>>& out, const fs::path& dirpath, const GenreData* genre, std::vector<ChartScanItem>& items) {
        std::vector<fs::directory_entry> entries;
        std::error_code ec;
        for (fs::directory_iterator it(dirpath, ec), end; !ec && it != end; it.increment(ec)) {
            entries.push_back(*it);
        }

        auto genreit = std::ranges::find_if(entries, [](const fs::directory_entry& e) { return e.path().filename() == "genre.ini"; });
        if (genreit != entries.end()) {
            auto box = std::make_unique<BoxData>(new GenreData(genreit->path()));
            box->SetBoxColor();
            GenreData* newgenre = box->GetGenre();
            out.push_back(std::move(box));
            ScanChartEntries(newgenre->Datas, entries, newgenre, items);
            return;
        }
        ScanChartEntries(out, entries, genre, items);
    }
    void ScanChartEntries(std::vector<std::unique_ptr<BoxData>>& out, const std::vector<fs::directory_entry>& entries, const GenreData* genre, std::vector<ChartScanItem>& items) {
        for (const auto& entry : entries) {
            std::error_code ec;
            if (entry.path().extension() == ".tja") {
                ChartScanItem item;
                item.Chart = new ChartData();
                item.Chart->ChartPath = entry.path();
                item.FolderGenre = genre != nullptr ? genre->Name : std::string();
                // directory_entry は列挙時の属性をキャッシュしているので追加のファイルアクセスは発生しない
                item.Size = entry.file_size(ec);
                item.WriteTime = entry.last_write_time(ec).time_since_epoch().count();
                auto box = std::make_unique<BoxData>(item.Chart);
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

    // 譜面フォルダの設定値 (UTF-8) を実際のパスにする。相対パスは実行ファイルのフォルダから
    static fs::path ResolveSongDirectory(const std::string& dir) {
        const fs::path path = U8ToPath(dir);
        fs::path resolved = (path.is_absolute() ? path : GetExecutablePath() / path).lexically_normal();
        if (!resolved.has_filename() && resolved.has_relative_path()) {
            resolved = resolved.parent_path();      // 末尾の区切り文字を取る ("Songs\" と "Songs" を同じにする)
        }
        return resolved;
    }
    // 実行ファイルのフォルダからの相対パス (別ドライブなど相対にできなければ空)
    static std::string ToRelativeSongDirectory(const fs::path& absolute) {
        std::error_code ec;
        const fs::path rel = fs::relative(absolute, GetExecutablePath(), ec);
        if (ec || rel.empty() || rel.is_absolute()) { return {}; }
        const std::string str = PathToU8(rel);
        return str == "." ? std::string(".") : str;
    }

    void EnumChart(const std::vector<std::string>& dir) {
        if (dir.empty()) { return; }
        BoxDatas.clear();
        TempBoxDatas.clear();

        std::vector<ChartScanItem> items;
        for (const auto& d : dir) {
            const fs::path dirpath = ResolveSongDirectory(d);
            std::error_code ec;
            if (!fs::is_directory(dirpath, ec)) { continue; }
            ScanChartDirectory(TempBoxDatas, dirpath, nullptr, items);
        }

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

        // 変更のあった譜面だけを並列でパース (Load は measure=false なら DxLib を呼ばないのでスレッドセーフ)
#ifndef __ANDROID__
        std::for_each(std::execution::par, needparse.begin(), needparse.end(), [&](size_t i) {
#else
        std::for_each(needparse.begin(), needparse.end(), [&](size_t i) {      // NDK の libc++ には並列アルゴリズムが無い
#endif
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
            });
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

        BoxDatasUpdate();
        BoxDataIndex = BoxDatas.empty() ? 0 : std::clamp<int>(BoxDataIndex, 0, (int)BoxDatas.size() - 1);
    }
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

    void ShowSelectMessage(std::string message) {
        SelectMessage = std::move(message);
        SelectMessageUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    }

    // 全角英数字・記号と全角空白を半角にし、英字を小文字にそろえる (IME で入力しても検索できるように)
    static std::string NormalizeSearchText(std::string_view in) {
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

    static std::optional<double> ParseSearchNumber(std::string_view v) {
        double value = 0;
        auto result = fast_float::from_chars(v.data(), v.data() + v.size(), value);
        if (v.empty() || result.ec != std::errc() || result.ptr != v.data() + v.size()) {
            return std::nullopt;
        }
        return value;
    }
    static int ParseSearchCourse(std::string_view v) {
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

    static SearchQuery ParseSearch(std::string_view text) {
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

    bool MatchChart(const ChartData& chart, const SearchQuery& q) const {
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

    // ---- お気に入り (favorites.json に保存。ゲームのフォルダ内の譜面は相対パスで記録し、フォルダを移動しても残るようにする) ----
    static std::string FavoriteKey(const fs::path& chartpath) {
        const fs::path rel = chartpath.lexically_relative(GetExecutablePath());
        const fs::path& key = (!rel.empty() && *rel.begin() != "..") ? rel : chartpath;
        const auto u8 = key.generic_u8string();
        return std::string(u8.begin(), u8.end());
    }
    void LoadFavorites() {
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
    void SaveFavorites() {
        std::vector<std::string> list(Favorites.begin(), Favorites.end());
        std::ranges::sort(list);
        const json data = { { "version", 1 }, { "charts", list } };
        std::ofstream ofs(GetExecutablePath() / "favorites.json");
        ofs << data.dump(2) << "\n";
    }
    bool IsFavorite(const ChartData& chart) const {
        return Favorites.contains(FavoriteKey(chart.ChartPath));
    }
    // ジャンル内の譜面を (サブジャンルも含めて) 集める
    void CollectGenreCharts(const GenreData& genre, std::vector<const ChartData*>& out) const {
        for (const auto& data : genre.Datas) {
            if (data->IsGenre()) {
                CollectGenreCharts(*data->GetGenre(), out);
            }
            else if (data->GetChart()->IsDan == IsDanSelect()) {
                out.push_back(data->GetChart());
            }
        }
    }
    // 譜面ならその譜面を、ジャンルなら中の譜面をまとめて登録・解除する (全部登録済みなら解除、それ以外は全部登録)
    void ToggleFavorite(BoxData* box) {
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
    void ToggleFavoriteSelected() {
        if (!BoxDatas.empty()) {
            ToggleFavorite(BoxDatas[BoxDataIndex]);
        }
    }
    void ToggleFavoriteOnly() {
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
    // 一覧を作り直す。条件 (検索・お気に入りのみ) があれば、ジャンルの開閉に関係なく該当する譜面を一覧にする
    void BoxDatasUpdate() {
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

    // 選曲画面の上部: 検索条件・件数・お知らせ / 検索欄を開いている間は書き方の例
    void DrawSearchStatus(const FontData& font) {
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
            const auto lines = WrapText("Space: 絞り込み  F5: お気に入り  F6: お気に入りのみ表示  F7: 譜面制作で開く", sfont.Handle, resx - 32);
            const float lh = ConfigLineHeight();
            const float top = resy - 8 - lh * (float)lines.size();
            UIBox({ 0, top - 4, resx, resy }, GetColor(0, 0, 0), 120);
            float y = top + lh * 0.5f;
            for (const auto& line : lines) {
                UIText(sfont, 16, y, line, GetColor(225, 225, 225));
                y += lh;
            }
        }
#endif
    }
    void DrawFavoriteMark(const GraphData& box, const FontData& font, float y) {
        const float x = box.Pos.X - box.Size.Width * 0.5f + font.Size;
        UIText(font, x, y, "★", GetColor(255, 215, 0), 1);
    }

    std::vector<uint8_t> FileToMem(const fs::path& path) {
        std::vector<std::uint8_t> buffer;
        if (fs::exists(path)) {
            size_t size = fs::file_size(path);
            buffer.resize(size);
            std::ifstream ifs(path, std::ios::binary);
            ifs.read(reinterpret_cast<char*>(buffer.data()), size);
        }
        return buffer;
    }
    void MemToFile(const fs::path& path, const std::vector<uint8_t>& datas) {
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(datas.data()), datas.size());
    }
#ifndef __ANDROID__
    void FileImport() {

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

                if (path.extension() != ".zip") {
                    fs::path dir = ImportFolderPath / path.filename();
                    if (fs::is_directory(path)) {
                        fs::copy(path, dir, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                    }
                    else {
                        fs::copy(path, dir, fs::copy_options::overwrite_existing);
                    }
                    continue;
                }

                std::ifstream ifs(path, std::ios::binary);
                IStreamFile istream(ifs);
                UnZipper unzip(istream);

                for (auto&& entry : unzip.listFiles()) {
                    std::string name = entry.fileName();
                    if (name.empty()) { continue; }
                    if (name.back() == '/') { name.pop_back(); }

                    // "../" などでインポート先の外へ書き込むエントリ (Zip Slip) を拒否する
                    const fs::path dest = fs::weakly_canonical(ImportFolderPath / StrToWStr(name));
                    auto [rootEnd, destIt] = std::mismatch(ImportRoot.begin(), ImportRoot.end(), dest.begin(), dest.end());
                    if (rootEnd != ImportRoot.end()) { continue; }

                    if (entry.isDir()) {
                        fs::create_directories(dest);
                        continue;
                    }
                    fs::create_directories(dest.parent_path());
                    MemToFile(dest, entry.readContent());
                }
            }
            catch (const std::exception&) {
                MessageBox(NULL, TEXT(std::string(file + "のインポートに失敗しました").c_str()), TEXT("エラー"), MB_ICONERROR);
            }
        }

        EnumChart(Config.SongDirectories);
    }
#endif
#ifndef __ANDROID__
    // ---- SONGLINK / MOVIELINK のダウンロード ----
    // cmd / PowerShell を経由せず yt-dlp を CreateProcessW で直接起動する。
    // シェルが介在しないため、URL に記号が含まれていてもコマンドとして解釈されることはない。

    // CommandLineToArgvW の規則に従って引数をクォートする
    static std::wstring QuoteArg(const std::wstring& arg) {
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

    // プロセスの終了を待つ間もメッセージ処理と画面更新を続け、ウィンドウが「応答なし」にならないようにする
    bool RunProcess(const std::vector<std::wstring>& args, const fs::path& workdir, const char* status) {
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

    // ---- 外部ツール (ffmpeg / yt-dlp) ----
    // ゲームには同梱せず、必要になった時にプレイヤーの同意を得て配布元から導入する (Library/ToolInstaller.hpp)
    enum class ToolKind { Ffmpeg, YtDlp };
    static const char* ToolName(ToolKind kind) { return kind == ToolKind::Ffmpeg ? "ffmpeg" : "yt-dlp"; }
    // ゲームのフォルダ → PATH の順に探す (見つからなければ空)
    static fs::path FindTool(ToolKind kind) {
        if (kind == ToolKind::Ffmpeg) { return rec::FindFfmpeg(GetExecutablePath()); }
        std::error_code ec;
        const fs::path local = GetExecutablePath() / "yt-dlp.exe";
        if (fs::is_regular_file(local, ec)) { return local; }
        wchar_t buf[MAX_PATH]{};
        if (SearchPathW(nullptr, L"yt-dlp.exe", nullptr, MAX_PATH, buf, nullptr) > 0) { return fs::path(buf); }
        return {};
    }
    // 導入の説明 (配布元とライセンス) を見せて同意を得る
    static bool ConfirmToolInstall(ToolKind kind, const char* reason) {
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
    // 導入を実行する (終わるまで進み具合を表示する。Esc で中止)
    bool InstallTool(ToolKind kind) {
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
        Input.ResyncKeys();
        return ok;
    }
    // ツールが無ければ導入を提案する (導入済み・導入できたら true)
    bool EnsureTool(ToolKind kind, const char* reason) {
        if (!FindTool(kind).empty()) { return true; }
        return ConfirmToolInstall(kind, reason) && InstallTool(kind) && !FindTool(kind).empty();
    }

    void MediaDownload(const std::string& link, const fs::path& path, const char* confirm, const char* failed, const wchar_t* ext, const std::vector<std::wstring>& options) {
        if (link.empty() || fs::exists(path)) {
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
    // Android: MainActivity.DownloadMedia (youtubedl-android) を別スレッドで呼び、終わるまで進み具合を表示する
    bool AndroidDownloadMedia(const std::string& link, const fs::path& path, bool audio) {
        if (!link.starts_with("https://") && !link.starts_with("http://")) { ShowToast("リンクは http:// または https:// で始まる必要があります"); return false; }
        const fs::path workdir = GetExecutablePath() / "download";
        std::error_code ec;
        fs::create_directories(workdir, ec);
        struct Task { std::atomic_bool Done = false; std::string Result, Error; };
        auto task = std::make_shared<Task>();
        std::thread([task, link, workdir, audio] {
            const ANativeActivity* activity = GetNativeActivity();
            JNIEnv* env = nullptr;
            if (activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) { task->Error = "JNI"; task->Done = true; return; }
            jclass cls = env->GetObjectClass(activity->clazz);
            jmethodID method = env->GetMethodID(cls, "DownloadMedia", "(Ljava/lang/String;Ljava/lang/String;Z)Ljava/lang/String;");
            if (method != nullptr) {
                jstring jurl = env->NewStringUTF(link.c_str());
                jstring jdir = env->NewStringUTF(workdir.string().c_str());
                auto jres = (jstring)env->CallObjectMethod(activity->clazz, method, jurl, jdir, (jboolean)audio);
                if (jres != nullptr) {
                    const char* chars = env->GetStringUTFChars(jres, nullptr);
                    task->Result = chars;
                    env->ReleaseStringUTFChars(jres, chars);
                    env->DeleteLocalRef(jres);
                }
                env->DeleteLocalRef(jurl);
                env->DeleteLocalRef(jdir);
                if (task->Result.empty()) {
                    jfieldID field = env->GetFieldID(cls, "DownloadError", "Ljava/lang/String;");
                    if (auto jerr = field ? (jstring)env->GetObjectField(activity->clazz, field) : nullptr) {
                        const char* chars = env->GetStringUTFChars(jerr, nullptr);
                        task->Error = chars;
                        env->ReleaseStringUTFChars(jerr, chars);
                        env->DeleteLocalRef(jerr);
                    }
                }
            }
            if (env->ExceptionCheck()) { env->ExceptionClear(); }
            env->DeleteLocalRef(cls);
            activity->vm->DetachCurrentThread();
            task->Done = true;
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
            fs::remove(task->Result, ec);
        }
        return !ec && fs::exists(path, ec);
    }
#endif
    void SongDownload(const std::string& link, const fs::path& path) {
#ifndef __ANDROID__
        MediaDownload(link, path, "音源ファイルがありません。ダウンロードしますか？", "音源のダウンロードに失敗しました", L"ogg", { L"-x", L"--audio-format", L"vorbis" });
#else
        std::error_code ec;
        if (link.empty() || fs::exists(path, ec)) { return; }
        AndroidDownloadMedia(link, path, true);
#endif
    }
    void MovieDownload(const std::string& link, const fs::path& path) {
#ifndef __ANDROID__
        MediaDownload(link, path, "動画ファイルがありません。ダウンロードしますか？", "動画のダウンロードに失敗しました", L"avi", { L"--recode-video", L"avi" });
#endif
    }

    void SongSelectInit() {

#ifndef __ANDROID__
        SetDragFileValidFlag(TRUE);
#endif
        Skin.Base->SongSelect.SE.Don.SetVolume((Config.SEVolume));
        Skin.Base->SongSelect.SE.Ka.SetVolume((Config.SEVolume));
        SelectSceneEnter();

        BoxDatasUpdate();
        if (BoxDatas.empty() && !Filter.Empty()) {
            // 前回の絞り込み条件に一致する譜面がなくなっていたら解除する
            Filter = FilterConditions();
            BoxDatasUpdate();
        }

        if (BoxDatas.empty()) {
#ifndef __ANDROID__
            MessageBox(NULL, TEXT("譜面がありません。"), TEXT("エラー"), MB_ICONERROR);
#else
            ShowToast("譜面がありません。");
#endif
            NowScene = Scene::ModeSelect;
            return;
        }
        BoxDataIndex = std::clamp<int>(BoxDataIndex, 0, (int)BoxDatas.size() - 1);
    }
    void SongSelectEnd() {

#ifndef __ANDROID__
        SetDragFileValidFlag(FALSE);
#endif

        DemoSongPlayBlank.Reset();
        DemoSong.Delete();
        IsCourseSelect = false;
        SelectSceneLeave();
    }
    void SongSelectDraw() {

        Skin.Base->SongSelect.Image.BackGround.Draw({});

        if (!IsCourseSelect) {

            for (int i = (BoxDataIndex - 6); i < (BoxDataIndex + 6); ++i) {
                if (i < 0 || i >= BoxDatas.size()) {
                    continue;
                }

                Pos2D<float> pos = {
                        0,
                        (i - BoxDataIndex) * Skin.Base->SongSelect.Config.BoxDistance.Y
                };
                pos = {
                        pos.X + Skin.Base->SongSelect.Config.SongBoxListPos.X,
                        pos.Y + Skin.Base->SongSelect.Config.SongBoxListPos.Y
                };

                unsigned int c = 200 * (BoxDataIndex == i);
                SetDrawAddColor(
                    (BoxDatas[i]->GenreColor.R + c),
                    (BoxDatas[i]->GenreColor.G + c),
                    (BoxDatas[i]->GenreColor.B + c)
                );
                Skin.Base->SongSelect.Image.Box.Draw({ 0, pos.Y });
                SetDrawAddColor(0, 0, 0);

                if (BoxDatas[i]->IsGenre()) {
                    if (BoxDatas[i]->GetGenre()->IsOpen) {
                        Skin.Base->SongSelect.Font.Title.Draw(
                            pos,
                            GetColor(255, 255, 255),
                            GetColor(0, 0, 0),
                            "Close"
                        );
                    }
                    else {
                        Skin.Base->SongSelect.Font.Title.Draw(
                            pos,
                            GetColor(255, 255, 255),
                            GetColor(0, 0, 0),
                            BoxDatas[i]->GetGenre()->Name
                        );
                    }
                }
                else {
                    Skin.Base->SongSelect.Font.Title.Draw(
                        pos,
                        GetColor(255, 255, 255),
                        GetColor(0, 0, 0),
                        BoxDatas[i]->GetChart()->Title
                    );
                    if (IsFavorite(*BoxDatas[i]->GetChart())) {
                        DrawFavoriteMark(Skin.Base->SongSelect.Image.Box, Skin.Base->SongSelect.Font.Title, pos.Y);
                    }
                }
            }
            SetDrawBlendMode(0, 0);

#ifdef __ANDROID__
            Skin.Base->SongSelect.Image.Config.Draw({});
            Skin.Base->SongSelect.Image.Random.Draw({});
            Skin.Base->SongSelect.Image.Search.Draw({});
#endif

        }
        else {
            Skin.Base->SongSelect.Image.TitleBox.Draw({});
            Skin.Base->SongSelect.Font.BoxTitle.Draw(
                Skin.Base->SongSelect.Config.BoxTitlePos,
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                BoxDatas[BoxDataIndex]->GetChart()->Title
            );
            Skin.Base->SongSelect.Font.BoxSubTitle.Draw(
                Skin.Base->SongSelect.Config.BoxSubTitlePos,
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                BoxDatas[BoxDataIndex]->GetChart()->Subtitle
            );

            for (size_t i = 0; i <= (size_t)CourseType::Edit; ++i) {

                unsigned int c = 100 * (CourseIndex == i);
                float y = Skin.Base->SongSelect.Config.CourseBoxDistance * i;
                SetDrawAddColor(c, c, c);
                Skin.Base->SongSelect.Image.CourseBox.Draw({ 0, y });
                SetDrawAddColor(0, 0, 0);

                if (!BoxDatas[BoxDataIndex]->GetChart()->Courses[i].IsPlayable) {
                    continue;
                }

                Skin.Base->SongSelect.Font.Course.Draw({ Skin.Base->SongSelect.Config.CoursePos.X,Skin.Base->SongSelect.Config.CoursePos.Y + y }, GetColor(255, 255, 255), GetColor(0, 0, 0), magic_enum::enum_name((CourseType)i).data());
                Skin.Base->SongSelect.Font.Level.Draw({ Skin.Base->SongSelect.Config.LevelPos.X,Skin.Base->SongSelect.Config.LevelPos.Y + y }, GetColor(255, 255, 255), GetColor(0, 0, 0), "★×" + std::to_string(BoxDatas[BoxDataIndex]->GetChart()->Courses[i].Level));

                ScoreData Score = ScoreDataLoad(BoxDatas[BoxDataIndex]->GetChart()->ChartPath.string(), i);

                Skin.Base->SongSelect.Font.HighScore.Draw({ Skin.Base->SongSelect.Config.HighScorePos.X,Skin.Base->SongSelect.Config.HighScorePos.Y + y }, GetColor(255, 255, 255), GetColor(0, 0, 0), std::to_string(Score.Score));
                Skin.Base->SongSelect.Image.Crown.Draw({ 0, y }, Score.Crown);
            }
        }
        if (!IsCourseSelect) {
            DrawSearchStatus(Skin.Base->SongSelect.Font.KeyWord);
            if (FilterUI.Open) {
                DrawFilterPanel(Skin.Base->SongSelect.Font.KeyWord);
            }
        }
#ifdef __ANDROID__
        SetDrawBlendMode(0, 0);
        Skin.Base->SongSelect.Image.Back.Draw({});
#endif
    }
    void SongSelectProc() {

        if (BoxDatas.empty()) {
            NowScene = !IsMulti ? Scene::ModeSelect : Scene::MultiRoom;
            return;
        }
        if (FilterUI.Open) {
            FilterPanelProc();
            return;
        }
        const PointerState pointer = ReadPointer();

        auto BackInputProc = [&] {
            if (IsCourseSelect) {
                IsCourseSelect = false;
            }
            else {
                NowScene = !IsMulti ? Scene::ModeSelect : Scene::MultiRoom;
            }
            };

#ifndef __ANDROID__
        Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
#else
        Touch.Process(TouchType::Other, BackInputProc, Skin.Base->SongSelect.Image.Back);
#endif

        auto DonInputProc = [&] {
            Skin.Base->SongSelect.SE.Don.Play();
            if (BoxDatas[BoxDataIndex]->IsGenre()) {
                bool& _f = BoxDatas[BoxDataIndex]->GetGenre()->IsOpen;
                _f = !_f;
                BoxDatasUpdate();
            }
            else {
                if (!IsCourseSelect) {
                    IsCourseSelect = true;
                }
                else if (BoxDatas[BoxDataIndex]->GetChart()->Courses[CourseIndex].IsPlayable) {
                    SongDownload(BoxDatas[BoxDataIndex]->GetChart()->SongLink, BoxDatas[BoxDataIndex]->GetChart()->SongPath);
                    MovieDownload(BoxDatas[BoxDataIndex]->GetChart()->MovieLink, BoxDatas[BoxDataIndex]->GetChart()->MoviePath);
                    DemoSong.Delete();
                    PrevScene = Scene::SongSelect;
                    NowScene = Scene::Loading;
                }
            }
            };
        auto KaInputProc = [&](bool direction) {
            Skin.Base->SongSelect.SE.Ka.Play();
            if (!direction) {
                if (!IsCourseSelect) {
                    BoxDataIndex = BoxDataIndex == 0 ? BoxDatas.size() - 1 : BoxDataIndex - 1;
                    DemoSongPlayBlank.Reset();
                    DemoSong.Delete();
                }
                else {
                    CourseIndex -= CourseIndex > 0;
                }
            }
            else {
                if (!IsCourseSelect) {
                    BoxDataIndex = BoxDataIndex == BoxDatas.size() - 1 ? 0 : BoxDataIndex + 1;
                    DemoSongPlayBlank.Reset();
                    DemoSong.Delete();
                }
                else {
                    CourseIndex += CourseIndex < (size_t)CourseType::Edit;
                }
            }
            };
        if (!BoxDatas[BoxDataIndex]->IsGenre()) {
            if (!DemoSongPlayBlank.IsRunning()) {
                DemoSong.Delete();
                DemoSongPlayBlank.Start();
            }
            else if (DemoSongPlayBlank.GetElapsed().Second() > DemoSongPlayBlankTime() && !DemoSong.IsPlay()) {
                SetCreateSoundDataType(DX_SOUNDDATATYPE_FILE);
                DemoSong.Load(BoxDatas[BoxDataIndex]->GetChart()->SongPath.string());
                SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMNOPRESS);
                DemoSong.SetCurrent(BoxDatas[BoxDataIndex]->GetChart()->DemoStart * 1000.0);
                DemoSong.SetVolume(BoxDatas[BoxDataIndex]->GetChart()->SongVolume * (Config.SongVolume / 100));
                DemoSong.Play(FALSE);
            }
        }
        auto RandomInputProc = [&] {
            Skin.Base->SongSelect.SE.Ka.Play();
            BoxDataIndex = GetRand(BoxDatas.size() - 1);
            DemoSongPlayBlank.Stop();
            DemoSong.Delete();
            };
        auto SearchInputProc = [&]() {
            Skin.Base->SongSelect.SE.Don.Play();
            OpenFilterPanel();
            };

        auto ConfigInputProc = [&] {
            Skin.Base->SongSelect.SE.Don.Play();
            NowScene = Scene::ConfigMenu;
            PrevScene = Scene::SongSelect;
            };

        // ---- マウス: クリックで選択、選択中をクリックで決定、右クリックで戻る、中クリックでお気に入り ----
        auto SelectBox = [&](int index, bool wheel = false) {
            if (!wheel || WheelSoundReady()) { Skin.Base->SongSelect.SE.Ka.Play(); }
            BoxDataIndex = index;
            DemoSongPlayBlank.Reset();
            DemoSong.Delete();
            };
        if (!IsCourseSelect) {
            const int hit = HitTestSelectBox(Skin.Base->SongSelect.Image.Box, Skin.Base->SongSelect.Config.SongBoxListPos.Y,
                Skin.Base->SongSelect.Config.BoxDistance.Y, pointer);
            if (hit >= 0 && pointer.LeftPressed) {
                if (hit == BoxDataIndex) { DonInputProc(); }
                else { SelectBox(hit); }
            }
            if (pointer.Wheel != 0) {
                const int size = (int)BoxDatas.size();
                SelectBox(((BoxDataIndex - pointer.Wheel) % size + size) % size, true);
            }
        }
        else {
            const GraphData& box = Skin.Base->SongSelect.Image.CourseBox;
            for (size_t i = 0; i <= (size_t)CourseType::Edit; ++i) {
                if (!pointer.LeftPressed) { break; }
                const float cy = box.Pos.Y + Skin.Base->SongSelect.Config.CourseBoxDistance * i;
                const UIRect r{ box.Pos.X - box.Size.Width * 0.5f, cy - box.Size.Height * 0.5f, box.Pos.X + box.Size.Width * 0.5f, cy + box.Size.Height * 0.5f };
                if (r.Contains(pointer.X, pointer.Y)) {
                    if (i == CourseIndex) { DonInputProc(); }
                    else { Skin.Base->SongSelect.SE.Ka.Play(); CourseIndex = i; }
                    break;
                }
            }
            if (pointer.Wheel != 0) {
                if (WheelSoundReady()) { Skin.Base->SongSelect.SE.Ka.Play(); }
                CourseIndex = (size_t)std::clamp((int)CourseIndex - pointer.Wheel, 0, (int)CourseType::Edit);
            }
        }
        if (NowScene != Scene::SongSelect) {
            return;
        }

#ifndef __ANDROID__

        {

            Input.HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { KaInputProc(false); }, Config.KeyHoldProcInterval);
            Input.HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { KaInputProc(true); }, Config.KeyHoldProcInterval);
            Input.HitKeyesProcess({ VK_UP, VK_LEFT }, KeyState::Down, [&] { KaInputProc(false); }, Config.KeyHoldProcInterval);
            Input.HitKeyesProcess({ VK_DOWN, VK_RIGHT }, KeyState::Down, [&] { KaInputProc(true); }, Config.KeyHoldProcInterval);

            Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, DonInputProc);
            Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, DonInputProc);
            Input.HitKeyProcess(VK_RETURN, KeyState::Down, DonInputProc);

            if (!IsCourseSelect) {

                Input.HitKeyProcess(VK_TAB, KeyState::Down, RandomInputProc);
                Input.HitKeyProcess(VK_SPACE, KeyState::Down, SearchInputProc);

                Input.HitKeyProcess(VK_F1, KeyState::Down, ConfigInputProc);
                Input.HitKeyProcess(VK_F2, KeyState::Down, [&] {
                    Skin.Base->SongSelect.SE.Don.Play();
                    EnumChart(Config.SongDirectories);
                    });
                Input.HitKeyProcess(VK_F3, KeyState::Down, [&] {
                    Skin.Base->SongSelect.SE.Don.Play();
                    Config.Load();
                    });
                Input.HitKeyProcess(VK_F4, KeyState::Down, [&] {
                    Skin.Base->SongSelect.SE.Don.Play();
                    Skin.Load(Config.SkinName);
                    });

                Input.HitKeyProcess(VK_F5, KeyState::Down, [&] {
                    (IsDanSelect() ? Skin.Base->DanSelect.SE.Don : Skin.Base->SongSelect.SE.Don).Play();
                    ToggleFavoriteSelected();
                    });
                Input.HitKeyProcess(VK_F6, KeyState::Down, [&] {
                    (IsDanSelect() ? Skin.Base->DanSelect.SE.Ka : Skin.Base->SongSelect.SE.Ka).Play();
                    ToggleFavoriteOnly();
                    DemoSongPlayBlank.Reset();
                    DemoSong.Delete();
                    });
                Input.HitKeyProcess(VK_F7, KeyState::Down, [&] {
                    // 選んでいる譜面を譜面制作モードで開く
                    if (IsMulti || BoxDatas.empty() || BoxDatas[BoxDataIndex]->IsGenre()) { return; }
                    if (Editor.Open && Editor.Dirty) {
                        ShowSelectMessage("譜面制作モードに保存していない譜面があります");
                        return;
                    }
                    if (EditorOpen(BoxDatas[BoxDataIndex]->GetChart()->ChartPath)) {
                        DemoSong.Delete();
                        NowScene = Scene::Editor;
                    }
                    });

                FileImport();
            }

            return;
        }

#else
        if (!IsCourseSelect) {
            Touch.Process(TouchType::Other, ConfigInputProc, Skin.Base->SongSelect.Image.Config);
            Touch.Process(TouchType::Other, SearchInputProc, Skin.Base->SongSelect.Image.Search);
        }
        Touch.Process(TouchType::Don, DonInputProc);
        Touch.Process(TouchType::LeftKa, [&] {KaInputProc(false);});
        Touch.Process(TouchType::RightKa, [&] {KaInputProc(true);});
#endif

    }

    void DanSelectInit() {

#ifndef __ANDROID__
        SetDragFileValidFlag(TRUE);
#endif

        Skin.Base->DanSelect.SE.Don.SetVolume((Config.SEVolume));
        Skin.Base->DanSelect.SE.Ka.SetVolume((Config.SEVolume));
        Dan.Reset();
        CourseIndex = 0;
        SelectSceneEnter();

        BoxDatasUpdate();
        if (BoxDatas.empty() && !Filter.Empty()) {
            // 前回の絞り込み条件に一致する譜面がなくなっていたら解除する
            Filter = FilterConditions();
            BoxDatasUpdate();
        }

        if (BoxDatas.empty()) {
#ifndef __ANDROID__
            MessageBox(NULL, TEXT("譜面がありません。"), TEXT("エラー"), MB_ICONERROR);
#else
            ShowToast("譜面がありません。");
#endif
            NowScene = Scene::ModeSelect;
            return;
        }
        BoxDataIndex = std::clamp<int>(BoxDataIndex, 0, (int)BoxDatas.size() - 1);
    }
    void DanSelectEnd() {
#ifndef __ANDROID__
        SetDragFileValidFlag(FALSE);
#endif
        SelectSceneLeave();
    }
    void DanSelectDraw() {

        Skin.Base->DanSelect.Image.BackGround.Draw({});

        for (int i = (BoxDataIndex - 6); i < (BoxDataIndex + 6); ++i) {
            if (i < 0 || i >= BoxDatas.size()) {
                continue;
            }

            Pos2D<float> pos = {
                    0,
                    (i - BoxDataIndex) * Skin.Base->DanSelect.Config.BoxDistance.Y
            };
            pos = {
                    pos.X + Skin.Base->DanSelect.Config.SongBoxListPos.X,
                    pos.Y + Skin.Base->DanSelect.Config.SongBoxListPos.Y
            };

            unsigned int c = 200 * (BoxDataIndex == i);
            SetDrawAddColor(
                (BoxDatas[i]->GenreColor.R + c),
                (BoxDatas[i]->GenreColor.G + c),
                (BoxDatas[i]->GenreColor.B + c)
            );
            Skin.Base->DanSelect.Image.Box.Draw({ 0, pos.Y });
            SetDrawAddColor(0, 0, 0);

            if (BoxDatas[i]->IsGenre()) {
                if (BoxDatas[i]->GetGenre()->IsOpen) {
                    Skin.Base->DanSelect.Font.Title.Draw(
                        pos,
                        GetColor(255, 255, 255),
                        GetColor(0, 0, 0),
                        "Close"
                    );
                }
                else {
                    Skin.Base->DanSelect.Font.Title.Draw(
                        pos,
                        GetColor(255, 255, 255),
                        GetColor(0, 0, 0),
                        BoxDatas[i]->GetGenre()->Name
                    );
                }
            }
            else {
                Skin.Base->DanSelect.Font.Title.Draw(
                    pos,
                    GetColor(255, 255, 255),
                    GetColor(0, 0, 0),
                    BoxDatas[i]->GetChart()->Title
                );
                if (IsFavorite(*BoxDatas[i]->GetChart())) {
                    DrawFavoriteMark(Skin.Base->DanSelect.Image.Box, Skin.Base->DanSelect.Font.Title, pos.Y);
                }
                Skin.Base->DanSelect.Image.Crown.Draw(pos, ScoreDataLoad(BoxDatas[i]->GetChart()->ChartPath.string(), (int)CourseType::Dan).Crown);
            }
        }
        SetDrawBlendMode(0, 0);

        if (!FilterUI.Open && !BoxDatas.empty() && !BoxDatas[BoxDataIndex]->IsGenre()) {
            DrawDanInfo(*BoxDatas[BoxDataIndex]->GetChart());
        }
        DrawSearchStatus(Skin.Base->DanSelect.Font.KeyWord);
        if (FilterUI.Open) {
            DrawFilterPanel(Skin.Base->DanSelect.Font.KeyWord);
        }

#ifdef __ANDROID__
        SetDrawBlendMode(0, 0);
        Skin.Base->DanSelect.Image.Back.Draw({});
        Skin.Base->DanSelect.Image.Config.Draw({});
        Skin.Base->DanSelect.Image.Random.Draw({});
        Skin.Base->DanSelect.Image.Search.Draw({});
#endif
    }
    void DanSelectProc() {

        if (BoxDatas.empty()) {
            NowScene = Scene::ModeSelect;
            return;
        }
        if (FilterUI.Open) {
            FilterPanelProc();
            return;
        }
        const PointerState pointer = ReadPointer();

        auto BackInputProc = [&] {
            NowScene = Scene::ModeSelect;
            };

#ifndef __ANDROID__
        Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
#else
        Touch.Process(TouchType::Other, BackInputProc, Skin.Base->DanSelect.Image.Back);
#endif

        auto DonInputProc = [&] {
            Skin.Base->DanSelect.SE.Don.Play();
            if (BoxDatas[BoxDataIndex]->IsGenre()) {
                bool& _f = BoxDatas[BoxDataIndex]->GetGenre()->IsOpen;
                _f = !_f;
                BoxDatasUpdate();
            }
            else if (BoxDatas[BoxDataIndex]->GetChart()->Courses[(int)CourseType::Dan].IsPlayable) {
                CourseIndex = (int)CourseType::Dan;
                PrevScene = Scene::DanSelect;
                NowScene = Scene::Loading;
            }
            };
        auto KaInputProc = [&](bool direction) {
            Skin.Base->DanSelect.SE.Ka.Play();
            if (!direction) {
                BoxDataIndex = BoxDataIndex == 0 ? BoxDatas.size() - 1 : BoxDataIndex - 1;
            }
            else {
                BoxDataIndex = BoxDataIndex == BoxDatas.size() - 1 ? 0 : BoxDataIndex + 1;
            }
            };
        auto RandomInputProc = [&] {
            Skin.Base->DanSelect.SE.Ka.Play();
            BoxDataIndex = GetRand(BoxDatas.size() - 1);
            };
        auto SearchInputProc = [&]() {
            Skin.Base->DanSelect.SE.Don.Play();
            OpenFilterPanel();
            };

        auto ConfigInputProc = [&] {
            Skin.Base->DanSelect.SE.Don.Play();
            NowScene = Scene::ConfigMenu;
            PrevScene = Scene::DanSelect;
            };

        // ---- マウス ----
        {
            const int hit = HitTestSelectBox(Skin.Base->DanSelect.Image.Box, Skin.Base->DanSelect.Config.SongBoxListPos.Y,
                Skin.Base->DanSelect.Config.BoxDistance.Y, pointer);
            if (hit >= 0 && pointer.LeftPressed) {
                if (hit == BoxDataIndex) { DonInputProc(); }
                else { Skin.Base->DanSelect.SE.Ka.Play(); BoxDataIndex = hit; }
            }
            if (pointer.Wheel != 0) {
                if (WheelSoundReady()) { Skin.Base->DanSelect.SE.Ka.Play(); }
                const int size = (int)BoxDatas.size();
                BoxDataIndex = ((BoxDataIndex - pointer.Wheel) % size + size) % size;
            }
        }
        if (NowScene != Scene::DanSelect) {
            return;
        }

#ifndef __ANDROID__
        {

            Input.HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { KaInputProc(false); }, Config.KeyHoldProcInterval);
            Input.HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { KaInputProc(true); }, Config.KeyHoldProcInterval);
            Input.HitKeyesProcess({ VK_UP, VK_LEFT }, KeyState::Down, [&] { KaInputProc(false); }, Config.KeyHoldProcInterval);
            Input.HitKeyesProcess({ VK_DOWN, VK_RIGHT }, KeyState::Down, [&] { KaInputProc(true); }, Config.KeyHoldProcInterval);

            Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, DonInputProc);
            Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, DonInputProc);
            Input.HitKeyProcess(VK_RETURN, KeyState::Down, DonInputProc);

            if (!IsCourseSelect) {

                Input.HitKeyProcess(VK_TAB, KeyState::Down, RandomInputProc);
                Input.HitKeyProcess(VK_SPACE, KeyState::Down, SearchInputProc);

                Input.HitKeyProcess(VK_F1, KeyState::Down, ConfigInputProc);
                Input.HitKeyProcess(VK_F2, KeyState::Down, [&] {
                    Skin.Base->DanSelect.SE.Don.Play();
                    EnumChart(Config.SongDirectories);
                    });
                Input.HitKeyProcess(VK_F3, KeyState::Down, [&] {
                    Skin.Base->DanSelect.SE.Don.Play();
                    Config.Load();
                    });
                Input.HitKeyProcess(VK_F4, KeyState::Down, [&] {
                    Skin.Base->DanSelect.SE.Don.Play();
                    Skin.Load(Config.SkinName);
                    });

                Input.HitKeyProcess(VK_F5, KeyState::Down, [&] {
                    (IsDanSelect() ? Skin.Base->DanSelect.SE.Don : Skin.Base->SongSelect.SE.Don).Play();
                    ToggleFavoriteSelected();
                    });
                Input.HitKeyProcess(VK_F6, KeyState::Down, [&] {
                    (IsDanSelect() ? Skin.Base->DanSelect.SE.Ka : Skin.Base->SongSelect.SE.Ka).Play();
                    ToggleFavoriteOnly();
                    DemoSongPlayBlank.Reset();
                    DemoSong.Delete();
                    });
                Input.HitKeyProcess(VK_F7, KeyState::Down, [&] {
                    // 選んでいる譜面を譜面制作モードで開く
                    if (IsMulti || BoxDatas.empty() || BoxDatas[BoxDataIndex]->IsGenre()) { return; }
                    if (Editor.Open && Editor.Dirty) {
                        ShowSelectMessage("譜面制作モードに保存していない譜面があります");
                        return;
                    }
                    if (EditorOpen(BoxDatas[BoxDataIndex]->GetChart()->ChartPath)) {
                        DemoSong.Delete();
                        NowScene = Scene::Editor;
                    }
                    });

                FileImport();
            }

            return;
        }
#else
        if (!IsCourseSelect) {
            Touch.Process(TouchType::Other, ConfigInputProc, Skin.Base->DanSelect.Image.Config);
            Touch.Process(TouchType::Other, SearchInputProc, Skin.Base->DanSelect.Image.Search);
        }

        Touch.Process(TouchType::Don, DonInputProc);
        Touch.Process(TouchType::LeftKa, [&] {KaInputProc(false);});
        Touch.Process(TouchType::RightKa, [&] {KaInputProc(true);});
#endif
    }

    // ===================================================================== オンラインプレイ
    // サーバーとの通信は fasnet::Connection の送受信スレッドが行い、
    // メインスレッドは OnlineUpdate で受信済みメッセージを処理するだけにする。

    struct OnlineConnectTask {
        std::atomic<int> State = 0;     // 0: 接続中, 1: 成功, 2: 失敗
        fasnet::NetSocket Sock;
    };

    struct OnlineData {
        fasnet::Connection Net;
        std::shared_ptr<OnlineConnectTask> Connecting;
        uint32_t MyId = 0;
        fasnet::RoomState Room;
        std::string Status;                         // 接続失敗・切断などの表示
        std::optional<fasnet::SongData> Song;       // ホストから配布された曲 (メモリ上のみ。読み込み後に消去する)
        fasnet::SongReceiver Incoming;              // ストリーム受信中の曲
        std::string Target;                         // 接続先 (表示用)
        int JoinMenu = 0;                           // 0: サーバーに接続, 1: 接続先を変更, 2: サーバーを起動してホスト
        std::vector<fasnet::AddressEntry> Addresses;    // このサーバーの接続先 (サーバーから受け取る。招待用)
        int AddressIndex = 0;                       // 表示している接続先
        std::string Notice;                         // 待機室に一時的に出すお知らせ
        std::chrono::steady_clock::time_point NoticeUntil{};
        bool AddressEditing = false;                // 「アドレス:ポート」の入力中
        int AddressHandle = 0;
        uint32_t LoadedSerial = 0;                  // 読み込み済みの曲の通し番号
        uint32_t FailedSerial = 0;                  // 読み込みに失敗した曲 (再試行しない)
        bool SongRequested = false;                 // ホストが曲を送信し、受理待ち
        double SongSpeed = 1.0;
        bool StartPending = false;
        std::chrono::steady_clock::time_point StartAt{};
        double RttMs = 0.0;
        std::chrono::steady_clock::time_point LastPing{};
        std::vector<uint32_t> LaneIds;              // 演奏中のレーン番号 -> PlayerId (0 番は自分)
        bool FinishSent = false;
        bool TransferMode = false;                  // ホスト移譲の相手を選んでいる
        int Cursor = 0;
    } Online;

    std::vector<fasnet::Connection::Message> OnlineInbox;

    bool IsMulti = false;   // サーバーのルームに参加している

    static uint64_t SteadyMs() {
        return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    const fasnet::PlayerInfo* MyPlayer() const {
        return Online.Room.Find(Online.MyId);
    }
    bool IsHost() const {
        const auto* me = MyPlayer();
        return IsMulti && me != nullptr && me->IsHost;
    }
    // 演奏画面に並べるレーン数
    size_t LaneCount() const {
        return IsMulti ? std::max<size_t>(Online.LaneIds.size(), 1) : 1;
    }
    static fasnet::ScoreState ToScore(const JudgeData& judge) {
        fasnet::ScoreState score;
        score.Score = judge.Score;
        score.Good = judge.Good;
        score.Ok = judge.Ok;
        score.Bad = judge.Bad;
        score.Roll = judge.Roll;
        score.Combo = judge.Combo;
        score.MaxCombo = judge.MaxCombo;
        score.HitNote = judge.HitNote;
        score.Accuracy = judge.Accuracy;
        return score;
    }
    static void FromScore(JudgeData& judge, const fasnet::ScoreState& score) {
        judge.Score = score.Score;
        judge.Good = score.Good;
        judge.Ok = score.Ok;
        judge.Bad = score.Bad;
        judge.Roll = score.Roll;
        judge.Combo = score.Combo;
        judge.MaxCombo = score.MaxCombo;
        judge.HitNote = score.HitNote;
        judge.Accuracy = score.Accuracy;
    }

    void OnlineConnect(const std::string& address, uint16_t port) {
        OnlineDisconnect();
        auto task = std::make_shared<OnlineConnectTask>();
        Online.Connecting = task;
        Online.Target = address + ":" + std::to_string(port);
        std::thread([task, address, port] {
            bool ok = false;
            if (auto addr = IPAddress::SolveHostName(address)) {
                ok = task->Sock.Connect(addr->Port(port));
            }
            task->State = ok ? 1 : 2;
            }).detach();
    }
    void OnlineConnectByAddress() {
        OnlineConnect(Config.ServerAddress, Config.ServerPort);
    }
#ifndef __ANDROID__
    // MultiServer.exe の場所: ゲームと同じフォルダ、またはビルド出力の MultiServer フォルダ
    static std::optional<fs::path> FindMultiServer() {
        const fs::path exe = GetExecutablePath();
        for (const fs::path& path : { exe / "MultiServer.exe", exe / "MultiServer" / "MultiServer.exe", exe.parent_path() / "MultiServer" / "MultiServer.exe" }) {
            std::error_code ec;
            if (fs::is_regular_file(path, ec)) { return path.lexically_normal(); }
        }
        return std::nullopt;
    }
    // この PC でサーバーを起動 (起動済みならそのまま) して接続し、ホストになる。
    // サーバーはゲームが終了してルームが空になると自動で終了する
    void OnlineHostLocal() {
        const auto server = FindMultiServer();
        if (!server) {
            OnlineDisconnect();
            Online.Status = "MultiServer.exe が見つかりません (ゲームと同じフォルダに置いてください)";
            return;
        }
        OnlineDisconnect();
        auto task = std::make_shared<OnlineConnectTask>();
        Online.Connecting = task;
        const uint16_t port = Config.ServerPort;
        Online.Target = "この PC:" + std::to_string(port);
        const std::wstring exe = server->wstring();
        const std::wstring dir = server->parent_path().wstring();
        const DWORD pid = GetCurrentProcessId();
        std::thread([task, port, exe, dir, pid] {
            auto addr = IPAddress::SolveHostName("127.0.0.1");
            if (!addr) { task->State = 2; return; }
            auto tryconnect = [&] { return task->Sock.Connect(addr->Port(port)); };
            if (tryconnect()) { task->State = 1; return; }     // 既に起動している

            std::wstring cmd = L"\"" + exe + L"\" " + std::to_wstring(port) + L" --autostart --minimized --parent " + std::to_wstring(pid);
            STARTUPINFOW si{ sizeof(si) };
            PROCESS_INFORMATION pi{};
            if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
                task->State = 2;
                return;
            }
            CloseHandle(pi.hThread);
            // 待ち受けを始めるまで最大 8 秒待つ (サーバーが起動に失敗して終了したら諦める)
            const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(8);
            bool ok = false;
            while (!ok && std::chrono::steady_clock::now() < limit) {
                if (WaitForSingleObject(pi.hProcess, 200) == WAIT_OBJECT_0) { break; }
                task->Sock = fasnet::NetSocket();
                ok = tryconnect();
            }
            CloseHandle(pi.hProcess);
            task->State = ok ? 1 : 2;
            }).detach();
    }
#endif
    // 「アドレス:ポート」(ポート省略時は設定のポート) を設定に保存して接続する
    bool OnlineSetTarget(const std::string& text) {
        const auto target = fasnet::SplitHostPort(text, Config.ServerPort);
        if (!target) {
            OnlineDisconnect();
            Online.Status = "接続先が正しくありません (例: 192.168.0.2:8080)";
            return false;
        }
        Config.ServerAddress = target->first;
        Config.ServerPort = target->second;
        Config.Write();
        return true;
    }
    void OnlineWipeSong() {
        if (Online.Song) {
            Online.Song->Wipe();
        }
        Online.Song.reset();
        Online.Incoming.Clear();
    }
    void OnlineDisconnect() {
        Online.Net.Close();
        Online.Connecting.reset();     // 接続中のスレッドは完了後に自動で破棄される
        Online.MyId = 0;
        Online.Room = fasnet::RoomState();
        OnlineWipeSong();
        Online.LoadedSerial = 0;
        Online.FailedSerial = 0;
        Online.SongRequested = false;
        Online.StartPending = false;
        Online.LaneIds.clear();
        Online.FinishSent = false;
        Online.TransferMode = false;
        Online.Cursor = 0;
        Online.RttMs = 0.0;
        Online.Status.clear();
        Online.Addresses.clear();
        Online.AddressIndex = 0;
        Online.Notice.clear();
        IsMulti = false;
    }
    // 接続が切れた: オンライン関連の画面にいれば待機室へ戻して理由を表示する
    void OnlineLost(const std::string& message) {
        OnlineDisconnect();
        Online.Status = message;
        if (NowScene == Scene::Playing || NowScene == Scene::Result || NowScene == Scene::Loading || NowScene == Scene::SongSelect) {
            Chart.Init(true);
            NowScene = Scene::MultiRoom;
        }
    }

    void OnlineUpdate() {
        if (Online.Connecting) {
            const int state = Online.Connecting->State.load();
            if (state == 1) {
                fasnet::SessionConfig config;
                Online.Net.Start(std::move(Online.Connecting->Sock), config);
                Online.Connecting.reset();
                Online.Net.Send(fasnet::Msg::Hello, fasnet::Hello{ fasnet::ProtocolVersion, Config.PlayerName, Config.TitleName });
            }
            else if (state == 2) {
                Online.Connecting.reset();
                Online.Status = "サーバーに接続できませんでした (" + Online.Target + ")";
            }
        }
        if (!Online.Net.IsStarted()) {
            return;
        }
        // 切断の直前に届いた Reject (参加を断られた理由) を先に処理する
        Online.Net.Poll(OnlineInbox);
        for (const auto& msg : OnlineInbox) {
            HandleOnlineMessage(msg);
            if (!Online.Net.IsStarted()) {
                return;     // Reject などで切断した
            }
        }
        if (!Online.Net.IsAlive()) {
            OnlineLost(!IsMulti
                ? "参加できませんでした (サーバーが応答しないか、バージョンが違います)"
                : "サーバーとの接続が切れました");
            return;
        }

        // 演奏開始の同期に使う往復遅延を定期的に測る
        const auto now = std::chrono::steady_clock::now();
        if (IsMulti && now - Online.LastPing > std::chrono::seconds(2)) {
            Online.LastPing = now;
            Online.Net.Send(fasnet::Msg::Ping, fasnet::U64Value{ SteadyMs() });
        }
    }

    void HandleOnlineMessage(const fasnet::Connection::Message& msg) {
        using fasnet::Msg;
        switch (msg.Type) {
        case Msg::Welcome: {
            fasnet::Welcome welcome;
            if (fasnet::Decode(msg.Payload, welcome)) {
                Online.MyId = welcome.PlayerId;
                Online.Status.clear();
                // 招待用の接続先。LAN のものを優先して表示する
                Online.Addresses = std::move(welcome.Addresses);
                Online.AddressIndex = 0;
                for (size_t i = 0; i < Online.Addresses.size(); ++i) {
                    if (Online.Addresses[i].Label.starts_with("LAN")) { Online.AddressIndex = (int)i; break; }
                }
                IsMulti = true;
            }
            break;
        }
        case Msg::Reject: {
            fasnet::Text reason;
            fasnet::Decode(msg.Payload, reason);
            OnlineDisconnect();
            Online.Status = "参加できませんでした: " + reason.Value;
            break;
        }
        case Msg::RoomState: {
            fasnet::RoomState room;
            if (fasnet::Decode(msg.Payload, room)) {
                const fasnet::RoomPhase prev = Online.Room.Phase;
                Online.Room = std::move(room);
                OnRoomChanged(prev);
            }
            break;
        }
        case Msg::SongBegin: {
            fasnet::SongInfo info;
            if (fasnet::Decode(msg.Payload, info)) {
                OnlineWipeSong();
                Online.Incoming.Begin(info);
                Online.LoadedSerial = 0;
            }
            break;
        }
        case Msg::SongChunk: {
            fasnet::SongChunk chunk;
            if (fasnet::Decode(msg.Payload, chunk)) {
                Online.Incoming.Append(chunk);      // 古い曲の残りなど、合わないチャンクは捨てる
                fasnet::crypto::SecureZero(chunk.Data);
            }
            break;
        }
        case Msg::SongEnd: {
            fasnet::U32Value serial;
            if (fasnet::Decode(msg.Payload, serial) && Online.Incoming.IsActive() && serial.Value == Online.Incoming.Info().Serial) {
                if (auto song = Online.Incoming.Take()) {
                    Online.Song = std::move(*song);
                }
                else {
                    Online.Incoming.Clear();
                }
            }
            break;
        }
        case Msg::Start: {
            fasnet::StartInfo info;
            if (fasnet::Decode(msg.Payload, info)) {
                OnOnlineStart(info.DelayMs);
            }
            break;
        }
        case Msg::Hit: {
            fasnet::HitEvent hit;
            if (fasnet::Decode(msg.Payload, hit)) {
                ApplyRemoteHit(hit);
            }
            break;
        }
        case Msg::Pong: {
            fasnet::U64Value sent;
            if (fasnet::Decode(msg.Payload, sent)) {
                const double rtt = (double)(SteadyMs() - sent.Value);
                Online.RttMs = Online.RttMs == 0.0 ? rtt : Online.RttMs * 0.8 + rtt * 0.2;
            }
            break;
        }
        default:
            break;
        }
    }

    void OnRoomChanged(fasnet::RoomPhase prev) {
        const auto& room = Online.Room;

        // ホスト: 送った曲が受理されたら、その通し番号で準備完了を通知する
        if (IsHost() && Online.SongRequested && room.Phase == fasnet::RoomPhase::SongSelected) {
            Online.SongRequested = false;
            Online.LoadedSerial = room.SongSerial;
            Online.Net.Send(fasnet::Msg::Ready, fasnet::U32Value{ room.SongSerial });
        }

        // 全員の演奏が終わったらリザルトへ
        if (room.Phase == fasnet::RoomPhase::Result && NowScene == Scene::Playing && Online.FinishSent) {
            CopyRoomScores();
            NowScene = Scene::Result;
        }
        if (NowScene == Scene::Result) {
            CopyRoomScores();
        }

        // ホストが待機室に戻した (演奏の中断を含む)
        if (room.Phase == fasnet::RoomPhase::Lobby && prev != fasnet::RoomPhase::Lobby) {
            OnlineWipeSong();
            Online.LoadedSerial = 0;
            Online.StartPending = false;
            if (NowScene == Scene::Result || NowScene == Scene::Playing || NowScene == Scene::Loading) {
                Chart.Init(true);
                NowScene = Scene::MultiRoom;
            }
        }

        Online.Cursor = std::clamp(Online.Cursor, 0, std::max((int)room.Players.size() - 1, 0));
    }

    void OnOnlineStart(uint32_t delayms) {
        if (!IsMulti || Online.LoadedSerial == 0 || Online.LoadedSerial != Online.Room.SongSerial) {
            return;     // 読み込んでいない曲では開始しない
        }
        // サーバーからの到着が片道分遅れているので、往復遅延の半分だけ早めて全員の開始を揃える
        const double wait = std::max(0.0, (double)delayms - Online.RttMs * 0.5);
        Online.StartAt = std::chrono::steady_clock::now() + std::chrono::milliseconds((int64_t)wait);
        Online.StartPending = true;
        Online.FinishSent = false;
        PrevScene = Scene::MultiRoom;
        NowScene = Scene::Playing;
    }

    void CopyRoomScores() {
        for (size_t lane = 1; lane < Online.LaneIds.size() && lane < Chart.Judge.size(); ++lane) {
            if (const auto* player = Online.Room.Find(Online.LaneIds[lane])) {
                FromScore(Chart.Judge[lane], player->Score);
            }
        }
    }

#ifndef __ANDROID__
    static constexpr int JoinMenuCount = 3;
#else
    static constexpr int JoinMenuCount = 2;     // サーバーの起動は Windows のみ
#endif

    // ===================================================================== Discord (ステータス表示・参加ボタン)
    // 画面ごとの状態を Discord のプロフィールに表示し、オンラインのルームにいる間はサーバーの「アドレス:ポート」を「参加」用の秘密として渡す。
    // フレンドが「参加」を押すと、その人のゲームにこの秘密が届き、そのサーバーに接続する。
    struct DiscordState {
        std::chrono::steady_clock::time_point NextUpdate{};
        Scene LastScene = Scene::Title;
        int64_t PlayStart = 0;
#ifndef __ANDROID__
        std::vector<DiscordRpc::Event> Events;
        std::optional<DiscordRpc::Event> Request;       // 参加の希望 (待機室で Y / N)
#endif
        std::chrono::steady_clock::time_point RequestUntil{};
    } Discord;

    // 使うアプリケーション ID (Config で無効なら空 = 停止)
    static std::string DiscordAppId() {
#ifndef __ANDROID__
        return Config.DiscordRPC ? std::string(DiscordRpc::ApplicationId) : std::string();
#else
        return {};      // Android には Discord のローカル接続が無い
#endif
    }
    // Discord の文字列は 2～128 バイト (UTF-8 の途中では切らない)
    static std::string DiscordText(std::string text) {
        if (text.size() > 128) {
            size_t n = 125;
            while (n > 0 && ((unsigned char)text[n] & 0xC0) == 0x80) { --n; }
            text = text.substr(0, n) + "...";
        }
        while (text.size() < 2) { text += ' '; }
        return text;
    }
    // 他の人に渡す接続先: 外部 (インターネット) のものがあればそれ、無ければ LAN
    const fasnet::AddressEntry* DiscordJoinAddress() const {
        for (const char* prefix : { "外部", "LAN" }) {
            for (const auto& a : Online.Addresses) {
                if (a.Label.starts_with(prefix)) { return &a; }
            }
        }
        return nullptr;
    }
    void DiscordUpdate() {
#ifndef __ANDROID__
        auto& rpc = DiscordRpc::GetInstance();
        if (DiscordAppId().empty()) { return; }

        rpc.PollEvents(Discord.Events);
        for (const auto& e : Discord.Events) {
            if (e.Kind == DiscordRpc::Event::Type::Join) {
                // 演奏中・読み込み中は参加しない (オンラインのルームにいる時も)
                if (!IsMulti && !e.Secret.empty() && NowScene != Scene::Playing && NowScene != Scene::Loading) {
                    NowScene = Scene::MultiRoom;
                    // 招待の接続先は設定に保存しない (自分のいつものサーバーを上書きしない)
                    if (const auto target = fasnet::SplitHostPort(e.Secret)) {
                        OnlineConnect(target->first, target->second);
                    }
                    else {
                        Online.Status = "Discord から受け取った接続先が正しくありません";
                    }
                }
            }
            else if (IsMulti) {
                Discord.Request = e;
                Discord.RequestUntil = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            }
            else {
                rpc.RespondJoinRequest(e.UserId, false);    // ルームにいないので参加できない
            }
        }
        if (Discord.Request && std::chrono::steady_clock::now() > Discord.RequestUntil) {
            Discord.Request.reset();
        }

        if (NowScene == Scene::Playing && Discord.LastScene != Scene::Playing) {
            Discord.PlayStart = (int64_t)std::time(nullptr);
        }
        Discord.LastScene = NowScene;
        const auto now = std::chrono::steady_clock::now();
        if (now < Discord.NextUpdate) { return; }
        Discord.NextUpdate = now + std::chrono::milliseconds(500);

        std::string details, state;
        const auto& chart = Chart.OriginalData;
        auto Song = [&] { return Dan.Active ? "段位道場: " + Dan.Course.Title : chart.Title; };
        auto Course = [&] {
            if (Dan.Active) { return std::to_string(Dan.SongIndex + 1) + " 曲目"; }
            std::string text = CourseLabel((int)CourseIndex);
            if (CourseIndex < (size_t)CourseType::Count) { text += "  ★" + std::to_string(chart.Courses[CourseIndex].Level); }
            return text;
        };
        switch (NowScene) {
        case Scene::SongSelect: details = IsMulti ? "オンライン: 曲を選んでいます" : "曲を選んでいます"; break;
        case Scene::DanSelect: details = "段位を選んでいます"; break;
        case Scene::Loading:
        case Scene::Playing: details = Song(); state = Course() + (IsMulti ? "  (オンライン)" : ""); break;
        case Scene::Result: details = "リザルト: " + Song(); state = "スコア " + std::to_string(Chart.Judge.empty() ? 0 : Chart.Judge[0].Score); break;
        case Scene::MultiRoom: details = IsMulti ? "オンライン: 待機室" : "オンライン: 参加先を選んでいます"; break;
        case Scene::ConfigMenu: details = "設定中"; break;
        case Scene::Editor: details = "譜面を作っています"; break;
        default: details = "メニュー"; break;
        }

        nlohmann::json activity = { { "details", DiscordText(details) }, { "assets", { { "large_image", "logo" }, { "large_text", "FreedomApopicStyle" } } }, { "instance", false } };
        if (!state.empty()) { activity["state"] = DiscordText(state); }
        if (NowScene == Scene::Playing && Discord.PlayStart > 0) { activity["timestamps"] = { { "start", Discord.PlayStart } }; }
        if (IsMulti) {
            const size_t players = Online.Room.Players.size();
            if (!activity.contains("state")) { activity["state"] = DiscordText("ルーム " + std::to_string(players) + " / " + std::to_string(fasnet::MaxPlayers) + " 人"); }
            // 参加ボタン: 演奏中でなく、空きがあり、他の人が使える接続先がある時だけ
            const auto* address = DiscordJoinAddress();
            if (address != nullptr && Online.Room.Phase != fasnet::RoomPhase::Playing && players < fasnet::MaxPlayers) {
                char id[32];
                std::snprintf(id, sizeof(id), "fas-%016llx", (unsigned long long)std::hash<std::string>{}(address->Text()));
                activity["party"] = { { "id", id }, { "size", { (int)std::max<size_t>(players, 1), (int)fasnet::MaxPlayers } } };
                activity["secrets"] = { { "join", address->Text() } };
            }
        }
        rpc.SetActivity(activity);
#endif
    }

    void MultiRoomInit() {
        Skin.Base->MultiRoom.SE.Don.SetVolume(Config.SEVolume);
        Skin.Base->MultiRoom.SE.Ka.SetVolume(Config.SEVolume);
        Online.TransferMode = false;
    }
    void MultiRoomEnd() {
        Online.TransferMode = false;
#ifndef __ANDROID__
        if (Online.AddressEditing) {
            DeleteKeyInput(Online.AddressHandle);
            Online.AddressHandle = 0;
            Online.AddressEditing = false;
        }
#endif
    }
    // 文字列を最大幅に収めて描く (収まらなければ末尾を省略)
    void FitDraw(FontData& font, Pos2D<float> pos, unsigned int color, unsigned int edge, const std::string& text, float maxwidth) {
        font.Draw(pos, color, edge, FitText(text, font.Handle, std::max(maxwidth, 32.0f), false));
    }
    // 待機室の名前の位置と幅: 箱 (PlayerBox) の中で、左側の王冠 (Crown) より右に収める
    float PlayerNameLeft() const {
        const auto& box = Skin.Base->MultiRoom.Image.PlayerBox;
        const auto& crown = Skin.Base->MultiRoom.Image.Crown;
        float left = box.Size.Width > 0 ? box.Pos.X - box.Size.Width * 0.5f + 16.0f : 16.0f;
        if (crown.Size.Width > 0) { left = std::max(left, crown.Pos.X + crown.Size.Width * 0.5f + 12.0f); }
        return left;
    }
    float PlayerNameRight() const {
        const auto& box = Skin.Base->MultiRoom.Image.PlayerBox;
        return box.Size.Width > 0 ? box.Pos.X + box.Size.Width * 0.5f - 16.0f : (float)Skin.Info.Resolution.X - 16.0f;
    }
    float PlayerLabelX() const {
        const auto& font = Skin.Base->MultiRoom.Font.Player;
        const float x = Skin.Base->MultiRoom.Config.PlayerPos.X;
        // 左揃えのスキンで王冠と重なる位置なら、王冠の右から書く
        return font.Position == FontData::Left ? std::max(x, PlayerNameLeft()) : x;
    }
    float PlayerLabelWidth() const {
        const auto& font = Skin.Base->MultiRoom.Font.Player;
        const float x = PlayerLabelX(), left = PlayerNameLeft(), right = PlayerNameRight();
        switch (font.Position) {
        case FontData::Center: return 2.0f * std::min(x - left, right - x);
        case FontData::Right: return x - left;
        default: return right - x;
        }
    }

    void MultiRoomDraw() {

        SetDrawBlendMode(0, 0);
        Skin.Base->MultiRoom.Image.BackGround.Draw({});

#ifdef __ANDROID__
        Skin.Base->MultiRoom.Image.Back.Draw({});
#endif
        auto& font = Skin.Base->MultiRoom.Font;
        const unsigned int white = GetColor(255, 255, 255);
        const unsigned int black = GetColor(0, 0, 0);
        const float bottom = (float)Skin.Info.Resolution.Y - 32.0f;
        const float resx = (float)Skin.Info.Resolution.X;

        if (!IsMulti) {
            if (Online.Connecting || Online.Net.IsStarted()) {
                FitDraw(font.String, { 16, 16 }, white, black, "サーバーに接続しています... (" + Online.Target + ")", resx - (16) - 16.0f);
                FitDraw(font.String, { 16, bottom }, white, black, "Esc: 中止", resx - (16) - 16.0f);
                return;
            }
            if (!Online.Status.empty()) {
                FitDraw(font.String, { 16, 16 }, GetColor(255, 160, 160), black, Online.Status, resx - (16) - 16.0f);
            }
            const std::string items[3] = {
                "サーバーに接続  (" + Config.ServerAddress + ":" + std::to_string(Config.ServerPort) + ")",
                "接続先 (アドレス:ポート) を入力して接続",
                "この PC でサーバーを起動してホストになる  (ポート " + std::to_string(Config.ServerPort) + ")",
            };
            for (int i = 0; i < JoinMenuCount; ++i) {
                const bool selected = Online.JoinMenu == i;
                FitDraw(font.String, { 48, 96.0f + 56.0f * i }, selected ? GetColor(255, 220, 0) : white, black, (selected ? "> " : "  ") + items[i], resx - (48) - 16.0f);
            }
#ifndef __ANDROID__
            if (Online.AddressEditing) {
                const int x = 72, y = 96 + 56 * JoinMenuCount + 16;
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 200);
                DrawBox(x - 12, y - 8, Skin.Info.Resolution.X - 48, y + (int)font.String.Size + 16, GetColor(0, 0, 0), TRUE);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                DrawBox(x - 12, y - 8, Skin.Info.Resolution.X - 48, y + (int)font.String.Size + 16, GetColor(255, 220, 0), FALSE);
                DrawKeyInputString(x, y, Online.AddressHandle);
                FitDraw(font.String, { 16, bottom }, white, black, "アドレス:ポート を入力して Enter (例: 192.168.0.2:8080 / Esc: キャンセル)", resx - (16) - 16.0f);
                return;
            }
#endif
            FitDraw(font.String, { 16, bottom }, white, black, "↑↓: 選択  /  Enter: 決定  /  Esc: 戻る", resx - (16) - 16.0f);
            return;
        }

        const auto& room = Online.Room;
        Skin.Base->MultiRoom.Image.TitleBox.Draw({});
        if (room.Phase != fasnet::RoomPhase::Lobby) {
            FitDraw(font.Title, Skin.Base->MultiRoom.Config.TitlePos, white, black, room.Title, resx - 32.0f);
            FitDraw(font.SubTitle, Skin.Base->MultiRoom.Config.SubTitlePos, white, black, room.Subtitle, resx - 32.0f);
            if (room.CourseIndex >= 0 && room.CourseIndex < (int)CourseType::Count) {
                font.Course.Draw(Skin.Base->MultiRoom.Config.CoursePos, white, black, magic_enum::enum_name((CourseType)room.CourseIndex).data());
            }
            font.Level.Draw(Skin.Base->MultiRoom.Config.LevelPos, white, black, "★×" + std::to_string(room.Level));
        }

        const float distance = Skin.Base->MultiRoom.Config.BoxDistance > 0 ? Skin.Base->MultiRoom.Config.BoxDistance : 100.0f;
        for (size_t i = 0; i < room.Players.size(); i++) {
            const auto& player = room.Players[i];
            const float y = distance * i;

            const bool selected = Online.TransferMode && Online.Cursor == (int)i;
            const unsigned int add = selected ? 120 : (player.Id == Online.MyId ? 40 : 0);
            SetDrawAddColor(add, add, add);
            Skin.Base->MultiRoom.Image.PlayerBox.Draw({ 0, y });
            SetDrawAddColor(0, 0, 0);

            unsigned int color = white;
            const char* state = "";
            switch (player.State) {
            case fasnet::PlayerState::Lobby: state = "待機"; break;
            case fasnet::PlayerState::Loading: state = "読込中"; color = GetColor(160, 160, 160); break;
            case fasnet::PlayerState::Ready: state = "準備完了"; color = GetColor(255, 255, 0); break;
            case fasnet::PlayerState::Playing: state = "演奏中"; color = GetColor(0, 255, 255); break;
            case fasnet::PlayerState::Finished: state = "終了"; color = GetColor(0, 255, 255); break;
            }
            const std::string label = player.Name + "  [" + state + "]";     // 称号は待機室の箱には出さない (演奏画面・リザルトで表示)
            // 名前は箱の中で、ホストの王冠と重ならない幅に収める (長い名前・称号は省略する)
            const std::string fitted = FitText(label, font.Player.Handle, PlayerLabelWidth(), false);
            font.Player.Draw(
                { PlayerLabelX(), Skin.Base->MultiRoom.Config.PlayerPos.Y + y },
                color, black,
                GetStrlenCached(fitted, font.Player.Handle),
                fitted
            );
            if (player.IsHost) {
                Skin.Base->MultiRoom.Image.Crown.Draw({ 0, y }, 3);
            }
        }

        // 操作説明と通信状況
        std::string help;
        const bool allready = !room.Players.empty() && std::ranges::all_of(room.Players, [](const fasnet::PlayerInfo& p) { return p.State == fasnet::PlayerState::Ready; });
        if (Online.TransferMode) {
            help = "↑↓: 移譲先を選択  /  Enter: 決定  /  Esc: キャンセル";
        }
        else if (IsHost()) {
            help = allready && room.Phase == fasnet::RoomPhase::SongSelected ? "Enter: 演奏開始" : "Enter: 曲を選ぶ";
            help += "  /  Tab: ホスト移譲  /  F1: 設定  /  Esc: 退出";
        }
        else {
            help = "ホストの操作を待っています  /  F1: 設定  /  Esc: 退出";
        }
        FitDraw(font.String, { 16, bottom }, white, black, help, resx - (16) - 16.0f);

        std::string status = "Ping " + std::to_string((int)Online.RttMs) + "ms";
        status += "  " + Online.Target;
        if (const size_t pending = Online.Net.PendingBulkBytes(); pending > 0) {
            status += "  曲データを送信中 (残り " + FormatNumber(pending / (1024.0 * 1024.0), 1) + "MB)";
        }
        else if (!IsHost() && Online.Incoming.IsActive() && Online.Incoming.Info().Serial == room.SongSerial) {
            const double total = std::max<double>((double)Online.Incoming.Total(), 1.0);
            status += "  曲データを受信中 " + std::to_string((int)(100.0 * Online.Incoming.Received() / total)) + "%";
        }
        FitDraw(font.String, { 16, bottom - 40 }, white, black, status, resx - (16) - 16.0f);

        // 招待用の接続先 (C: コピー / V: 切り替え)
        if (!Online.Addresses.empty()) {
            const auto& a = Online.Addresses[std::clamp(Online.AddressIndex, 0, (int)Online.Addresses.size() - 1)];
            FitDraw(font.String, { 16, bottom - 80 }, GetColor(255, 230, 120), black, "接続先 (" + a.Label + "): " + a.Text() + (Online.Addresses.size() > 1 ? "   C: コピー  V: 切り替え" : "   C: コピー"), resx - (16) - 16.0f);
        }
        if (std::chrono::steady_clock::now() < Online.NoticeUntil) {
            FitDraw(font.String, { 16, bottom - 120 }, GetColor(160, 255, 160), black, Online.Notice, resx - (16) - 16.0f);
        }
#ifndef __ANDROID__
        if (Discord.Request) {
            FitDraw(font.String, { 16, bottom - 160 }, GetColor(160, 200, 255), black, "Discord: " + Discord.Request->UserName + " が参加を希望しています  Y: 許可  N: 断る", resx - (16) - 16.0f);
        }
#endif

#ifdef __ANDROID__
        Skin.Base->MultiRoom.Image.Config.Draw({});
        Skin.Base->MultiRoom.Image.Host.Draw({});
#endif
    }
    void MultiRoomProc() {

        auto BackInputProc = [&] {
            if (Online.TransferMode) {
                Online.TransferMode = false;
                return;
            }
            OnlineDisconnect();
            NowScene = Scene::ModeSelect;
            };

        if (!IsMulti) {
#ifndef __ANDROID__
            // 接続先の入力中
            if (Online.AddressEditing) {
                const int state = CheckKeyInput(Online.AddressHandle);
                if (state != 0) {
                    if (state == 1) {
                        char buf[256]{};
                        GetKeyInputString(buf, Online.AddressHandle);
                        if (OnlineSetTarget(buf)) { OnlineConnectByAddress(); }
                    }
                    DeleteKeyInput(Online.AddressHandle);
                    Online.AddressHandle = 0;
                    Online.AddressEditing = false;
                    // 確定に使った Enter が次のフレームで「参加」として誤動作しないようにする
                    Input.ResyncKeys();
                }
                return;
            }
#endif
            const bool busy = Online.Connecting || Online.Net.IsStarted();
            auto JoinProc = [&] {
                if (busy) { return; }
                Skin.Base->MultiRoom.SE.Don.Play();
                if (Online.JoinMenu == 0) {
                    OnlineConnectByAddress();
                    return;
                }
#ifndef __ANDROID__
                if (Online.JoinMenu == 2) {
                    OnlineHostLocal();
                    return;
                }
                Online.AddressHandle = MakeKeyInput(255, TRUE, TRUE, FALSE);
                SetActiveKeyInput(Online.AddressHandle);
                SetKeyInputStringFont(Skin.Base->MultiRoom.Font.String.Handle);
                SetKeyInputString((Config.ServerAddress + ":" + std::to_string(Config.ServerPort)).c_str(), Online.AddressHandle);
                Online.AddressEditing = true;
#else
                if (OnlineSetTarget(GetInputText("アドレス:ポート"))) { OnlineConnectByAddress(); }
#endif
                };
            auto MoveProc = [&](int dir) {
                if (busy) { return; }
                Skin.Base->MultiRoom.SE.Ka.Play();
                Online.JoinMenu = std::clamp(Online.JoinMenu + dir, 0, JoinMenuCount - 1);
                };
#ifndef __ANDROID__
            Input.HitKeyProcess(VK_UP, KeyState::Down, [&] { MoveProc(-1); });
            Input.HitKeyProcess(VK_DOWN, KeyState::Down, [&] { MoveProc(1); });
            Input.HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { MoveProc(-1); });
            Input.HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { MoveProc(1); });
            Input.HitKeyProcess(VK_RETURN, KeyState::Down, JoinProc);
            Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, JoinProc);
            Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, JoinProc);
            Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
#else
            Touch.Process(TouchType::LeftKa, [&] { MoveProc(-1); });
            Touch.Process(TouchType::RightKa, [&] { MoveProc(1); });
            Touch.Process(TouchType::Don, JoinProc);
            Touch.Process(TouchType::Other, BackInputProc, Skin.Base->MultiRoom.Image.Back);
#endif
            return;
        }

        const auto& room = Online.Room;

        // 配布された曲をまだ読み込んでいなければ読み込む
        if (!IsHost() && room.Phase == fasnet::RoomPhase::SongSelected && Online.Song
            && Online.Song->Serial == room.SongSerial && Online.LoadedSerial != room.SongSerial && Online.FailedSerial != room.SongSerial) {
            PrevScene = Scene::MultiRoom;
            NowScene = Scene::Loading;
            return;
        }

        auto DonInputProc = [&] {
            if (Online.TransferMode) {
                if (Online.Cursor >= 0 && Online.Cursor < (int)room.Players.size() && room.Players[Online.Cursor].Id != Online.MyId) {
                    Skin.Base->MultiRoom.SE.Don.Play();
                    Online.Net.Send(fasnet::Msg::TransferHost, fasnet::U32Value{ room.Players[Online.Cursor].Id });
                }
                Online.TransferMode = false;
                return;
            }
            if (!IsHost() || room.Phase == fasnet::RoomPhase::Playing) {
                return;
            }
            Skin.Base->MultiRoom.SE.Don.Play();
            const bool allready = std::ranges::all_of(room.Players, [](const fasnet::PlayerInfo& p) { return p.State == fasnet::PlayerState::Ready; });
            if (room.Phase == fasnet::RoomPhase::SongSelected && allready) {
                Online.Net.Send(fasnet::Msg::StartRequest);
            }
            else {
                PrevScene = Scene::MultiRoom;
                NowScene = Scene::SongSelect;
            }
            };

        auto KaInputProc = [&](bool direction) {
            if (!Online.TransferMode) {
                return;
            }
            Skin.Base->MultiRoom.SE.Ka.Play();
            Online.Cursor = std::clamp(Online.Cursor + (direction ? 1 : -1), 0, std::max((int)room.Players.size() - 1, 0));
            };

        auto HostInputProc = [&] {
            if (IsHost() && room.Players.size() >= 2 && room.Phase != fasnet::RoomPhase::Playing) {
                Skin.Base->MultiRoom.SE.Don.Play();
                Online.TransferMode = true;
                Online.Cursor = 0;
            }
            };

        auto ConfigInputProc = [&] {
            if (room.Phase != fasnet::RoomPhase::Playing) {
                Skin.Base->MultiRoom.SE.Don.Play();
                PrevScene = Scene::MultiRoom;
                NowScene = Scene::ConfigMenu;
            }
            };

#ifndef __ANDROID__
        Input.HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { KaInputProc(false); }, Config.KeyHoldProcInterval);
        Input.HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { KaInputProc(true); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_UP, KeyState::Down, [&] { KaInputProc(false); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_DOWN, KeyState::Down, [&] { KaInputProc(true); }, Config.KeyHoldProcInterval);

        Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, DonInputProc);
        Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, DonInputProc);
        Input.HitKeyProcess(VK_RETURN, KeyState::Down, DonInputProc);

        Input.HitKeyProcess(VK_TAB, KeyState::Down, HostInputProc);
        Input.HitKeyProcess(VK_F1, KeyState::Down, ConfigInputProc);
        Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
        Input.HitKeyProcess('C', KeyState::Down, [&] {
            if (Online.Addresses.empty()) { return; }
            const auto& a = Online.Addresses[std::clamp(Online.AddressIndex, 0, (int)Online.Addresses.size() - 1)];
            if (SetClipboardText(a.Text().c_str()) == 0) {
                Skin.Base->MultiRoom.SE.Don.Play();
                Online.Notice = "接続先をコピーしました (" + a.Label + ")";
                Online.NoticeUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            }
            });
        auto IsDrumKey = [&](int key) {
            for (const auto* keys : { &Config.DonInputLeft, &Config.DonInputRight, &Config.KaInputLeft, &Config.KaInputRight }) {
                if (std::ranges::find(*keys, key) != keys->end()) { return true; }
            }
            return false;
        };
        if (Discord.Request && !IsDrumKey('Y') && !IsDrumKey('N')) {
            Input.HitKeyProcess('Y', KeyState::Down, [&] {
                DiscordRpc::GetInstance().RespondJoinRequest(Discord.Request->UserId, true);
                Discord.Request.reset();
                Skin.Base->MultiRoom.SE.Don.Play();
                });
            Input.HitKeyProcess('N', KeyState::Down, [&] {
                if (!Discord.Request) { return; }
                DiscordRpc::GetInstance().RespondJoinRequest(Discord.Request->UserId, false);
                Discord.Request.reset();
                Skin.Base->MultiRoom.SE.Ka.Play();
                });
        }
        Input.HitKeyProcess('V', KeyState::Down, [&] {
            if (Online.Addresses.size() < 2) { return; }
            Skin.Base->MultiRoom.SE.Ka.Play();
            Online.AddressIndex = (Online.AddressIndex + 1) % (int)Online.Addresses.size();
            });
#else
        Touch.Process(TouchType::LeftKa, [&] { KaInputProc(false); });
        Touch.Process(TouchType::RightKa, [&] { KaInputProc(true); });
        Touch.Process(TouchType::Don, DonInputProc);
        Touch.Process(TouchType::Other, HostInputProc, Skin.Base->MultiRoom.Image.Host);
        Touch.Process(TouchType::Other, ConfigInputProc, Skin.Base->MultiRoom.Image.Config);
        Touch.Process(TouchType::Other, BackInputProc, Skin.Base->MultiRoom.Image.Back);
#endif
    }

    double ScoreRateCalc(double judge, double basis) {
        const double c = 0.9;
        const double b = basis;
        const double m = 10;
        const double d = std::pow(b, std::pow(m, -1 / c));
        return 1 / std::pow(std::log(judge * (b - d) / b + d) / std::log(b), c);
    }

    void SetSpeed() {

        double SongSpeed = !IsMulti ? Config.SongSpeed : Online.SongSpeed;

#ifndef __ANDROID__
        if (!Chart.OriginalData.MoviePath.empty() && !Chart.Movie.IsLoaded()) {
            Chart.Movie.Load(Chart.OriginalData.MoviePath.string(), SongSpeed);
        }
#endif

        Chart.SongData.SetVolume(Chart.OriginalData.SongVolume * (Config.SongVolume / 100));
        int freq = Chart.SongData.Frequency * SongSpeed;
        Chart.SongData.SetFrequency(freq);
        Chart.SongSpeed = (double)freq / Chart.SongData.Frequency;
    }

	uint64_t AllNoteCounter(std::vector<std::string_view>& lines) {
		
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

    void LoadingDraw() {
        DrawFormatString(0, 8, GetColor(255, 255, 255), "ChartLoading...");
    }
    void LoadingProc() {

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
            loaded = LoadData.Load(chartpath, textptr.get(), true);
            Online.SongSpeed = Config.SongSpeed;
        }
        else if (!OnlineGuest) {
            chartpath = BoxDatas[BoxDataIndex]->GetChart()->ChartPath;
            loaded = textptr->Load(chartpath) && LoadData.Load(chartpath, textptr.get(), true);
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
            loaded = LoadData.Load(chartpath, textptr.get(), true);
        }

        if (!loaded || CourseIndex >= (size_t)CourseType::Count || !LoadData.Courses[CourseIndex].IsPlayable) {
#ifndef __ANDROID__
            MessageBox(NULL, TEXT(std::string(chartpath.string() + "の読み込みに失敗しました").c_str()), TEXT("エラー"), MB_ICONERROR);
#else
            ShowToast(chartpath.string() + "の読み込みに失敗しました");
#endif
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

        // ノーツ解析では行を書き換えるため、行ビューの配列だけ複製する (文字列本体は text が保持)
        std::vector<std::string_view> lines = text.lines;

        // 段位の 2 曲目以降は判定 (累計スコア) を引き継ぐ
        Chart.Init(!DanContinue);

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

        double BranchAddTime = 0;

        // 通常: 分岐ごとに #BPMCHANGE / #MEASURE / #DELAY で時刻を計算し、分岐の後は最後に書かれた分岐の続きから進める。
        // 次郎形式: 分岐中の #BPMCHANGE / #MEASURE / #DELAY は最初に書かれた分岐だけが有効で、
        //           2 つ目以降の分岐は最初の分岐の小節ごとの時刻・BPM・拍子を使い、分岐の後は最初の分岐の終わりから続ける
        const bool jiro = Config.JiroChartMode;
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
            double Bpm = 120, Ratio = 1;    // 今の位置の計算に使う BPM と拍子
            double MeasureStart = 0, MeasureDelay = 0, HeadDelay = 0, Next = 0;
            int Index = 0, Count = 1, PrevCount = 1;
            double LastTime = 0;            // 最後に計算した位置の時刻
            double HeadStart = 0;           // 2 つ目以降の分岐: 小節の頭
            double Beat() const { return 60000.0 / Bpm; }
            double PosTime(int k) const {
                const double t = EvHere
                    ? std::trunc(Beat() * (double)(k - EvIndex) / Count * 4.0 * Ratio) + EvTime
                    : std::trunc(Beat() * (double)k / Count * 4.0 * Ratio) + MeasureStart;
                return std::max(0.0, t + MeasureDelay);
            }
        } jc;
        // 次郎は命令の値を決まった文字数だけ読む (#BPMCHANGE・#SCROLL は 8 文字、#MEASURE・#DELAY は 10 文字。命令名の直後から)
        auto JiroArg = [&](std::string_view line, size_t keylen, size_t n, std::string_view data) -> std::string_view {
            if (!jiro || line.size() <= keylen) { return data; }
            return strtrim(line.substr(keylen, std::min(n, line.size() - keylen)));
        };
        auto TimingCommandEnabled = [&] { return !jiro || !NowBranchStartFlag || BranchCount < 2; };
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
                        int Uindex = data.rfind("+") == std::string_view::npos ? 0 : data.rfind("+");
                        int Dindex = data.rfind("-") == std::string_view::npos ? 0 : data.rfind("-");

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
                    }
                    Chart.ScrollEvents.push_back({ MainData.AbsTime, MainData.Scroll, MainData.IsBranch });
                    });
                if (TimingCommandEnabled()) {
                    Exsubstr(lines[i], "#BPMCHANGE", [&](std::string_view data) {
                        double bpm = svtov<double>(JiroArg(lines[i], 10, 8, data));
                        if (jiro) {
                            // 次郎: 使えない値 (0 など) は直前の BPM のまま
                            if (!std::isfinite(bpm) || std::abs(bpm) < 0.000112 || std::abs(bpm) > 1000000.0) { bpm = MainData.BPM; }
                        }
                        // BPM 0 は小節の長さが無限大になる (次郎でも止まる) ため、記述の誤りとして扱う
                        else if (bpm == 0.0 || !std::isfinite(bpm)) { throw std::invalid_argument("BPMCHANGE"); }
                        MainData.BPM = bpm;
                        MainData.BpmChangeFlag = true;
                        // BM/HBSCROLL の拍マップと BPM 表示は最初の分岐の BPM を使う
                        if (FirstBranchOrOutside()) {
                            if (jiro) { jc.Pending = true; jc.PendingBpm = bpm; }     // 時刻は次の位置で決まる
                            else { Chart.BpmEvents.push_back({ MainData.AbsTime, MainData.BPM }); }
                        }
                        });
                    Exsubstr(lines[i], "#MEASURE", [&](std::string_view data) {
                        auto sp = split(JiroArg(lines[i], 8, 10, data), '/');
                        if (sp.size() < 2 || svtov<double>(sp[1]) == 0.0) { throw std::invalid_argument("MEASURE"); }
                        const double measure = svtov<double>(sp[0]) / svtov<double>(sp[1]);
                        // 次郎形式: 小節の途中の #MEASURE は次の小節から効く
                        if (jiro && AddBarline) { PendingMeasure = measure; }
                        else { MainData.Measure = measure; }
                        });
                    Exsubstr(lines[i], "#DELAY", [&](std::string_view data) {
                        double delay = svtov<double>(JiroArg(lines[i], 6, 10, data)) * 1000;
                        if (jiro) {
                            // 次郎: ミリ秒の整数に切り捨て (小数第 4 位以下は効かない)。0.0005 秒未満は無視
                            if (!std::isfinite(delay) || std::abs(delay) < 0.5 || std::abs(delay) > 1e9) { return; }
                            delay = std::trunc(delay);
                            const bool head = !AddBarline;
                            // 次郎は命令の位置の時刻 (この #DELAY を足す前) から止め始めを決める
                            const double here = !jc.Started ? MainData.AbsTime : head ? jc.Next + jc.HeadDelay : jc.PosTime(jc.Index);
                            if (head) { jc.HeadDelay += delay; }
                            else { jc.MeasureDelay += delay; }
                            jc.TotalDelay += delay;
                            if (delay > 0 && FirstBranchOrOutside()) {
                                // 次郎と同じく、止め始めは「命令の位置 + 5ms - 1 区切り」
                                const int count = head ? jc.PrevCount : jc.Count;
                                const double sh = std::trunc(jc.Ratio * 4.0 / std::max(count, 1) * std::trunc(jc.Beat()));
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
            catch (const std::invalid_argument&) {
#ifndef __ANDROID__
                MessageBox(NULL, TEXT(std::string(std::to_string(i + 1) + "行目の記述が不正です。").c_str()), TEXT("エラー"), MB_ICONERROR);
#else
                ShowToast(std::string(std::to_string(i + 1) + "行目の記述が不正です。").c_str());
#endif
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
                jc.Next = std::max(0.0, jc.EvTime + std::trunc(jc.Beat() * jc.EvRemain) + std::trunc(jc.Beat() * jc.After) + jc.TotalDelay);
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
                                    jc.EvTime = jc.Next = std::max(0.0, std::trunc(MainData.AbsTime - jc.HeadDelay));
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
                                jc.EvTime = jc.PosTime(jc.Index);
                                jc.Bpm = jc.PendingBpm;
                                jc.EvIndex = jc.Index;
                                jc.EvHere = true;
                                jc.After = 0;
                                jc.TotalDelay = 0;
                                jc.MeasureDelay = 0;
                                jc.Pending = false;
                                Chart.BpmEvents.push_back({ jc.EvTime, jc.Bpm });
                            }
                            MainData.AbsTime = jc.PosTime(jc.Index);
                        }
                        else {
                            // 2 つ目以降の分岐: 最初の分岐の小節の頭から
                            MainData.AbsTime = std::max(0.0, std::trunc(60000.0 / MainData.BPM * (double)jc.Index / jc.Count * 4.0 * MainData.Measure) + jc.HeadStart);
                        }
                        jc.LastTime = MainData.AbsTime;
                    }

                    double barlinetime = (240000 / MainData.BPM) * MainData.Measure;
                    double divtime = barlinetime / (EmptyFlag ? 1 : BarlineNoteCount);

                    MainData.RelaTime = divtime;

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
                        !MainData.LevelHold;

                    if (MainData.NoteType >= '1' && MainData.NoteType <= '4') {
                        NoteCount += MainData.IsBranch == BranchType::Null || MainData.IsBranch == BranchType::Normal;
                    }
                    if (MainData.NoteType == '0') {
                        MainData.HitFlag = true;
                    }
                    if (GetRand(99) < Config.RandomRate) {
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
                        else { MainData.AbsTime = std::max(0.0, std::trunc(60000.0 / MainData.BPM * (double)jc.Index / jc.Count * 4.0 * MainData.Measure) + jc.HeadStart); }
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
                note.AbsTime = std::max(0.0, std::trunc(note.AbsTime));
                if (note.RollEndTime != 0) { note.RollEndTime = std::max(0.0, std::trunc(note.RollEndTime)); }
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
        SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMNOPRESS);
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
        SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMNOPRESS);

        SetSpeed();

        Skin.Base->Playing.SE.Don.SetVolume(Chart.OriginalData.SeVolume * (Config.SEVolume / 100));
        Skin.Base->Playing.SE.Ka.SetVolume(Chart.OriginalData.SeVolume * (Config.SEVolume / 100));
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
        if (Editor.Testing && Editor.TestMeasure > 0 && Config.TrainingMode) {
            uint64_t barline = 0;
            for (size_t i = 0; i < Chart.RawNoteDatas.size(); ++i) {
                if (!Chart.RawNoteDatas[i].BarlineDisplay) { continue; }
                if (barline == (uint64_t)Editor.TestMeasure) {
                    Training.NoteDataIndex = i;
                    Training.BarlineIndex = barline;
                    Training.Offset = Chart.RawNoteDatas[i].AbsTime;
                    for (auto& note : Chart.RawNoteDatas) {
                        if (note.AbsTime < Training.Offset) { note.HitFlag = true; }   // それより前の音符は見逃しにしない
                    }
                    break;
                }
                ++barline;
            }
        }
        if (!Config.TrainingMode || Chart.IsDanMode()) {
            WaitVSync(10);
            Chart.NowTime.Start();
        }
#ifndef __ANDROID__
        LastHitTick = RawKeyboard::Now();
#endif

        NowScene = Scene::Playing;
    }

    enum class AlphaType {
        Hidden,
        Sudden
    };

    enum class ScrollType {
        Normal,
        BMSCROLL,
        HBSCROLL,
    };

    struct NoteData {

        double AbsTime = 0;
        double RelaTime = 0;
        double PosTime = 0;
        bool BpmChangeFlag = false;

        double BigNoteTime = 0;

        double BPM = 0;
        double Measure = 1;

        BranchType IsBranch = BranchType::Null;
        bool BranchStart = false;
        bool Section = false;
        bool LevelHold = false;

        double Scroll = 1;
        double Scrolli = 0;

        bool GoGoStart = false;
        bool GoGoEnd = false;

        char NoteType = '\0';
        double RollEndTime = 0;
        size_t RollEndIndex = 0;
        unsigned short RollFlag = 0;
        unsigned short BalloonFlag = 0;
        uint64_t BalloonCount = 0;
        bool BarlineDisplay = false;
        bool HitFlag = false;
    };

    struct HitNoteData {

        HitNoteData() {}
        HitNoteData(char type, JudgeType judgetype) {

            if (type == '6') {
                FlyingNote.Type = 8 + 48;
            }
            else {
                FlyingNote.Type = type;
            }

            JudgeUnderExplosion.Type = judgetype;
            JudgeUnderExplosion.Big = type == '3' || type == '4';
            JudgeString.Type = judgetype;

            switch (judgetype)
            {
            case JudgeType::Bad:
                JudgeString.IsActive = true;
                break;
            case JudgeType::Roll:
                FlyingNote.IsActive = true;
                break;
            default:
                JudgeUpperExplosion.IsActive = true;
                JudgeUnderExplosion.IsActive = true;
                FlyingNote.IsActive = true;
                JudgeString.IsActive = true;
                break;
            }

            MoveTimer.Reset();
        }

        Timer MoveTimer;
        double MoveElapsedTime = 0;

        struct FlyingNote {
            bool IsActive = false;
            char Type = '0';
            double MoveTime() { return 500; }
        } FlyingNote;

        struct JudgeString {
            bool IsActive = false;
            JudgeType Type = JudgeType::None;
            double MoveTime() { return 500; }
        } JudgeString;

        struct JudgeUnderExplosion {
            bool IsActive = false;
            bool Big = false;
            JudgeType Type = JudgeType::None;
        } JudgeUnderExplosion;

        struct JudgeUpperExplosion {
            bool IsActive = false;
            bool Big = false;
            JudgeType Type = JudgeType::None;
        } JudgeUpperExplosion;
    };

    struct BranchData {
        double AbsTime = 0;             // 判定する時刻 (1 つ前の小節の頭。次郎形式の連打数の分岐は分岐の頭)
        double StartMs = 0;             // 分岐の始まり
        bool Evaluated = false;         // 次郎形式: 判定が確定した
        bool Start = false;
        bool BranchFlag = false;
        IfBranchType Type = IfBranchType::Null;
        double ExpertBranch = 0;
        double MasterBranch = 0;
    };

    struct PlayData {

        void Init(bool dan_init) {
            if (dan_init) {
                Judge.clear();
                AllNoteCount = 0;
                DanBalloonIndex = 0;
            }
            else {
                Judge[0].Branch.Init();
                Judge[0].Rolls.Init();
            }
            RawNoteDatas.clear();
            BranchDatas.clear();
            BpmEvents.clear();
            DelayEvents.clear();
            SongData.Delete();
            Movie.Delete();
            NowTime.Reset();
            BranchAnimationTimer.Reset();
            RollViewEndTimer.Reset();
            WaitRollTime.Reset();
            ScrollType = ScrollType::Normal;
            NowBranchFlag = BranchType::Null;
            NowBranchAnimation = ABranchType::Null;
            OriginalData = ChartData();
            AutoPlayLR = false;
            LevelHold = false;
            NowGoGo = false;
            AddScore = 0;
            AllBarlineCount = 0;
            ScanStart = 0;
            ScanTime = -std::numeric_limits<double>::infinity();
            ScrollEvents.clear();
            for (auto& lane : JiroLanes) { lane.clear(); }
            for (auto& c : JiroCursor) { c = 0; }
            JiroNext = BranchType::Normal;
            JiroShown = 1;
            JiroAnimating = false;
            JiroUpcomingStart = std::numeric_limits<double>::infinity();
            BranchAnimationTime = 0.2;
            SongBlankTime = 0;
            SongSpeed = 1.0;
            NowBPM = 0;
        }

        bool IsDanMode() const {
            return OriginalData.IsDan;
        }
        bool IsBranchChart() const {
            return !BranchDatas.empty();
        }

        std::vector<NoteData> RawNoteDatas = std::vector<NoteData>();
        std::vector<JudgeData> Judge = std::vector<JudgeData>();

        std::vector<BranchData> BranchDatas = std::vector<BranchData>();
        ChartData OriginalData;

        // ---- BPM 区間と #DELAY (次郎と同じ扱い) ----
        // 区間は「書かれた順」に並べ、時刻 t の区間は「時刻が t 以前の、書かれた順で最後の区間」とする。
        // 負の BPM・負の #MEASURE で時刻が戻る譜面 (逆走・瞬間移動・痙攣などのギミック) もこの規則で次郎と同じに見える
        struct TimedValue {
            double Time = 0;
            double Value = 0;
        };
        struct DelayEvent {
            double Time = 0;
            double Length = 0;      // ms (正の値だけ)
            double Bpm = 120;       // 書かれた時点の BPM (止めた分の表示距離に使う)
        };
        std::vector<TimedValue> BpmEvents;      // #BPMCHANGE (時刻, BPM)。先頭は BPM: の値 (時刻 -∞)
        std::vector<DelayEvent> DelayEvents;    // 正の #DELAY (開始時刻順)

        // 「時刻 t 以前で最後の要素」を O(log n) で引く (後ろからの最小時刻の表)
        struct LastAtOrBefore {
            std::vector<double> SuffixMin;
            template <class F> void Assign(size_t n, F time) {
                SuffixMin.assign(n, 0);
                double low = std::numeric_limits<double>::infinity();
                for (size_t i = n; i-- > 0;) { low = std::min(low, time(i)); SuffixMin[i] = low; }
            }
            // 見つからなければ -1
            ptrdiff_t Find(double t) const {
                auto it = std::upper_bound(SuffixMin.begin(), SuffixMin.end(), t);
                return (ptrdiff_t)(it - SuffixMin.begin()) - 1;
            }
        };
        LastAtOrBefore BpmIndex;
        std::vector<double> SectionMeasures;    // [i] = 区間 0..i-1 を通した表示距離 (小節単位)
        std::vector<double> DelayPrefix;        // [i] = DelayEvents[0..i-1] の表示距離の合計 (小節単位)

        void BuildTiming(double initbpm, bool integerms) {
            if (integerms) {
                for (auto& e : BpmEvents) { e.Time = std::max(0.0, std::trunc(e.Time)); }
                for (auto& e : DelayEvents) { e.Time = std::max(0.0, std::trunc(e.Time)); e.Length = std::trunc(e.Length); }
            }
            BpmEvents.insert(BpmEvents.begin(), { -std::numeric_limits<double>::infinity(), initbpm });
            for (auto& e : BpmEvents) { if (e.Value == 0.0) { e.Value = 1e-9; } }   // 0 除算を避ける
            PxPrefixCache.clear();
            BpmIndex.Assign(BpmEvents.size(), [&](size_t i) { return BpmEvents[i].Time; });
            // SectionMeasures[k] = 区間 1..k-1 の距離の合計 (区間 0 は BPM: の値で、足すことはない)
            SectionMeasures.assign(BpmEvents.size(), 0);
            for (size_t i = 1; i < BpmEvents.size(); ++i) {
                const double span = i - 1 == 0 ? 0 : (BpmEvents[i].Time - BpmEvents[i - 1].Time) * BpmEvents[i - 1].Value / 240000.0;
                SectionMeasures[i] = SectionMeasures[i - 1] + span;
            }
            std::ranges::stable_sort(DelayEvents, {}, &DelayEvent::Time);
            DelayPrefix.assign(DelayEvents.size() + 1, 0);
            for (size_t i = 0; i < DelayEvents.size(); ++i) {
                DelayPrefix[i + 1] = DelayPrefix[i] + DelayEvents[i].Length * DelayEvents[i].Bpm / 240000.0;
            }
        }
        // 次郎形式の 1px 単位の距離: 区間ごとの距離を px に直して切り捨ててから足す (unit = 1 小節の px)
        mutable std::unordered_map<double, std::vector<double>> PxPrefixCache;
        const std::vector<double>& PxPrefix(double unit) const {
            if (auto it = PxPrefixCache.find(unit); it != PxPrefixCache.end()) { return it->second; }
            if (PxPrefixCache.size() > 64) { PxPrefixCache.clear(); }
            std::vector<double> p(BpmEvents.size(), 0);
            for (size_t i = 1; i < BpmEvents.size(); ++i) {
                const double span = i - 1 == 0 ? 0 : std::trunc((BpmEvents[i].Time - BpmEvents[i - 1].Time) * BpmEvents[i - 1].Value / 240000.0 * unit);
                p[i] = p[i - 1] + span;
            }
            return PxPrefixCache.emplace(unit, std::move(p)).first->second;
        }
        double ScrollPx(double notetime, double now, double unit) const {
            if (BpmEvents.empty()) { return std::trunc(ScrollDistance(notetime, now) * unit); }
            const size_t cur = SectionAt(now), note = SectionAt(notetime);
            const size_t next = cur + 1;
            auto px = [&](double dt, double bpm) { return std::trunc(dt * bpm / 240000.0 * unit); };
            double x;
            if (note == 0 || next >= BpmEvents.size() || notetime < BpmEvents[next].Time) {
                x = px(notetime - now, BpmEvents[note].Value);
            }
            else {
                x = px(BpmEvents[next].Time - now, BpmEvents[cur].Value);
                if (note > next) { const auto& p = PxPrefix(unit); x += p[note] - p[next]; }
                x += px(notetime - BpmEvents[note].Time, BpmEvents[note].Value);
            }
            if (!DelayEvents.empty() && notetime >= now) {
                auto lo = std::ranges::lower_bound(DelayEvents, now, {}, &DelayEvent::Time);
                auto hi = std::ranges::upper_bound(DelayEvents, notetime, {}, &DelayEvent::Time);
                for (auto it = lo; it < hi; ++it) { x -= px(it->Length, it->Bpm); }
            }
            return x;
        }
        size_t SectionAt(double t) const {
            const ptrdiff_t i = BpmIndex.Find(t);
            return i < 0 ? 0 : (size_t)i;
        }
        double BpmAt(double t) const {
            return BpmEvents.empty() ? NowBPM : BpmEvents[SectionAt(t)].Value;
        }
        // BM/HBSCROLL: #DELAY の間は表示上の時刻を止める
        double FrozenTime(double now) const {
            for (const auto& d : DelayEvents) {
                if (d.Time > now) { break; }
                if (now <= d.Time + d.Length) { return d.Time; }
            }
            return now;
        }
        // BM/HBSCROLL の表示距離 (小節単位、SCROLL 前)。次郎と同じく、今の区間から音符の区間まで書かれた順に区間を足す
        double ScrollDistance(double notetime, double now) const {
            if (BpmEvents.empty()) { return (notetime - now) * NowBPM / 240000.0; }
            const size_t cur = SectionAt(now), note = SectionAt(notetime);
            const size_t next = cur + 1;
            double dist;
            if (note == 0 || next >= BpmEvents.size() || notetime < BpmEvents[next].Time) {
                dist = (notetime - now) * BpmEvents[note].Value / 240000.0;
            }
            else {
                dist = (BpmEvents[next].Time - now) * BpmEvents[cur].Value / 240000.0;
                if (note > next) { dist += SectionMeasures[note] - SectionMeasures[next]; }
                dist += (notetime - BpmEvents[note].Time) * BpmEvents[note].Value / 240000.0;
            }
            // これから来る #DELAY (今 <= 開始 <= 音符) の分だけ手前に寄せる
            if (!DelayEvents.empty() && notetime >= now) {
                auto lo = std::ranges::lower_bound(DelayEvents, now, {}, &DelayEvent::Time);
                auto hi = std::ranges::upper_bound(DelayEvents, notetime, {}, &DelayEvent::Time);
                if (hi > lo) { dist -= DelayPrefix[hi - DelayEvents.begin()] - DelayPrefix[lo - DelayEvents.begin()]; }
            }
            return dist;
        }

        uint64_t AddScore = 0;
        uint64_t AllNoteCount = 0;

        Timer BranchAnimationTimer;
        double BranchAnimationTime = 0.2;
        BranchType NowBranchFlag = BranchType::Null;
        ABranchType NowBranchAnimation = ABranchType::Null;
        bool LevelHold = false;

        SoundData SongData;
        double SongBlankTime = 0;
        double SongSpeed = 1.0;

        MovieData Movie;

        Timer NowTime;
        ScrollType ScrollType = ScrollType::Normal;
        double NowBPM = 0;
        bool NowGoGo = false;

        bool AutoPlayLR = false;
        Timer WaitRollTime;

        Timer RollViewEndTimer;
        double RollViewEndTime = 0.75;

        size_t AllBarlineCount = 0;

        // ---- 次郎形式 (JiroChartMode) ----
        struct ScrollEvent { double Time; double Value; BranchType Branch; };
        std::vector<ScrollEvent> ScrollEvents;      // 書かれた順 (分岐の中の #SCROLL はその分岐だけ)
        std::vector<size_t> JiroLanes[3];           // 分岐ごとの、判定する音符の並び (書かれた順)
        size_t JiroCursor[3]{};                     // 次に判定する音符 (書かれた順に 1 つずつ進む)
        BranchType JiroNext = BranchType::Normal;   // 判定で決まった次の分岐
        int JiroShown = 1;                          // 表示している分岐 (1: 普通, 2: 玄人, 3: 達人。1 段ずつ 100ms で動く)
        bool JiroAnimating = false;
        std::chrono::steady_clock::time_point JiroAnimTick{};
        double JiroUpcomingStart = std::numeric_limits<double>::infinity();

        static int JiroLaneOf(BranchType b) { return b == BranchType::Expert ? 1 : b == BranchType::Master ? 2 : 0; }
        static BranchType JiroBranchOf(int lane) { return lane == 1 ? BranchType::Expert : lane == 2 ? BranchType::Master : BranchType::Normal; }
        void BuildJiroLanes() {
            for (int lane = 0; lane < 3; ++lane) {
                JiroLanes[lane].clear();
                JiroCursor[lane] = 0;
                for (size_t i = 0; i < RawNoteDatas.size(); ++i) {
                    const auto& n = RawNoteDatas[i];
                    const bool target = (n.NoteType >= '1' && n.NoteType <= '7') || n.NoteType == '9';
                    if (target && (n.IsBranch == BranchType::Null || JiroLaneOf(n.IsBranch) == lane && n.IsBranch != BranchType::NotDisplay)) {
                        JiroLanes[lane].push_back(i);
                    }
                }
            }
        }
        // 次郎と同じく、時刻がその時刻以前の最後の (書かれた順で後ろの) BPM と SCROLL を使う
        double JiroBpmAt(double t) const { return BpmAt(t); }
        // 分岐ごとの #SCROLL の並び (分岐の外の #SCROLL はすべての分岐に入る)
        std::vector<double> ScrollLaneValue[3];
        LastAtOrBefore ScrollLaneIndex[3];
        void BuildScrollLanes(bool integerms) {
            for (int lane = 0; lane < 3; ++lane) {
                std::vector<double> times;
                ScrollLaneValue[lane].clear();
                for (const auto& ev : ScrollEvents) {
                    if (ev.Branch == BranchType::Null || ev.Branch == BranchType::NotDisplay || JiroLaneOf(ev.Branch) == lane) {
                        times.push_back(integerms ? std::max(0.0, std::trunc(ev.Time)) : ev.Time);
                        ScrollLaneValue[lane].push_back(ev.Value);
                    }
                }
                ScrollLaneIndex[lane].Assign(times.size(), [&](size_t i) { return times[i]; });
            }
        }
        double JiroScrollAt(double t, BranchType branch) const {
            const int lane = JiroLaneOf(branch);
            const ptrdiff_t i = ScrollLaneIndex[lane].Find(t);
            return i < 0 ? 1.0 : ScrollLaneValue[lane][(size_t)i];
        }

        // 演奏中の音符の走査は、終わった先頭部分を飛ばす (毎フレーム・打撃ごとの全件走査を避ける)。
        // 音符は分岐ごとに並ぶため時刻順ではないので、先頭から「確実に終わった」音符だけを進める
        size_t ScanStart = 0;
        double ScanTime = -std::numeric_limits<double>::infinity();
        static constexpr double ScanMargin = 2000.0;   // 判定・連打の終わりからこれだけ過ぎた音符は処理しない
        void UpdateScanStart(double now, double judgebad) {
            if (now < ScanTime) {
                ScanStart = 0;      // トレーニングモードで巻き戻した
            }
            ScanTime = now;
            while (ScanStart < RawNoteDatas.size()) {
                const auto& n = RawNoteDatas[ScanStart];
                const double end = std::max(n.AbsTime, n.RollEndTime) + judgebad + ScanMargin;
                if (end >= now || n.RollFlag == 1 || n.BalloonFlag == 1) { break; }
                ++ScanStart;
            }
        }
        std::span<NoteData> ActiveNotes() { return std::span<NoteData>(RawNoteDatas).subspan(std::min(ScanStart, RawNoteDatas.size())); }

        size_t DanBalloonIndex = 0;

    };

    PlayData Chart;

    Timer MiniTaikoFlash[16];
    double MiniTaikoFlashTime = 160;

    int RefreshRate = 60;

    double ChartNowTime(uint64_t elapsed, bool FrameCounter = false, double fastdrawrate = 0, double extendrate = 1) const {
        double ret = 0;
        if (FrameCounter) {
            double hz = RefreshRate * extendrate;
            double frame = ((long long)((Chart.NowTime.GetElapsed().Second() * RefreshRate) * hz) + fastdrawrate) / hz;
            ret = (1000.0 * frame) / (double)RefreshRate;
        }
        else {
            ret = Chart.NowTime.GetElapsed().Second() * elapsed;
        }
        return ret * Chart.SongSpeed;
    };


    struct _HitNote {
    private:

        static const size_t m_size = 64;

    public:

        _HitNote() {};
        ~_HitNote() {};

        HitNoteData Datas[m_size]{};
        size_t Index = 0;
        size_t Size() { return m_size; }

        void Add(HitNoteData&& data) {
            Datas[Index] = std::move(data);
            ++Index;
            if (!(Index < m_size)) {
                Index = 0;
            }
        }
    };

    std::vector<_HitNote> HitNote = std::vector<_HitNote>();
    std::vector<std::string> Names = std::vector<std::string>();
    std::vector<std::string> Titles;    // レーンごとの称号 (オンライン)
    const std::string& LaneTitle(size_t lane) const {
        static const std::string empty;
        if (!IsMulti) { return lane == 0 ? Config.TitleName : empty; }
        return lane < Titles.size() ? Titles[lane] : empty;
    }

    struct TrainingData {

        Timer BarlineMoveTimer;
        double BarlineMoveTime = 0.05;
        double Offset = 0;
        double MemNowTime = 0;
        uint64_t NoteDataIndex = 0;
        uint64_t BarlineIndex = 0;
        BranchType MemBranchFlag = BranchType::Null;

        void Init() {
            BarlineMoveTimer.Reset();
            BarlineMoveTime = 0.05;
            Offset = 0;
            MemNowTime = 0;
            NoteDataIndex = 0;
            BarlineIndex = 0;
            MemBranchFlag = BranchType::Null;
        }
    } Training;

    // ===================================================================== 段位道場
    // 段位譜面は最初に 1 回だけ読み込み、課題曲ごとに同じテキストからノーツを組み立てる。
    // 合格条件は「全体条件」と「曲ごとの個別条件」を同じ仕組みで判定する。

    enum class ExamStatus {
        NotYet,     // 未達 (まだ判定できない)
        Red,        // 合格
        Gold,       // 金合格
        Fail,       // 不合格
    };

    struct DanExam {
        ExamData Exam;
        int Song = -1;                  // -1: 全体条件, 0 以上: その曲だけの条件
        double Value = 0.0;
        ExamStatus Status = ExamStatus::NotYet;
        bool Final = false;             // 判定が確定した
    };

    struct DanCounters {
        double Accuracy = 0.0;
        uint64_t Good = 0, Ok = 0, Bad = 0, Score = 0, Roll = 0, HitNote = 0, MaxCombo = 0;
    };

    struct DanState {
        bool Active = false;
        size_t SongIndex = 0;
        ChartData Course;                       // 段位全体のヘッダ
        std::shared_ptr<TextLines> Text;        // 譜面テキスト (課題曲ごとに読み直さない)
        std::vector<DanExam> Exams;
        JudgeData SongStart;                    // 現在の曲を始めた時点の累計
        uint64_t SongCombo = 0;
        uint64_t SongMaxCombo = 0;
        uint64_t LastCombo = 0;
        bool Failed = false;
        SoundData NextSong;                     // 演奏中に先読みした次の曲
        size_t NextSongIndex = SIZE_MAX;

        void Reset() {
            NextSong.Delete();
            *this = DanState();
        }
    } Dan;

    static const char* ExamLabel(ExamTypes type) {
        switch (type) {
        case ExamTypes::Accuracy: return "精度";
        case ExamTypes::Good: return "良";
        case ExamTypes::Ok: return "可";
        case ExamTypes::Bad: return "不可";
        case ExamTypes::Score: return "スコア";
        case ExamTypes::Roll: return "連打";
        case ExamTypes::HitNote: return "叩けた数";
        case ExamTypes::MaxCombo: return "最大コンボ";
        default: return "?";
        }
    }
    static std::string ExamValueText(ExamTypes type, double value) {
        if (type == ExamTypes::Accuracy) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f%%", value);
            return buf;
        }
        return std::to_string((int64_t)value);
    }
    static std::string ExamConditionText(const ExamData& exam) {
        return std::string(ExamLabel(exam.ExamType)) + " " + ExamValueText(exam.ExamType, exam.PassVal[0])
            + (exam.Range == ExamRange::Less ? " 未満" : " 以上")
            + " (金 " + ExamValueText(exam.ExamType, exam.PassVal[1]) + ")";
    }

    DanCounters DanTotalCounters() const {
        const JudgeData& j = Chart.Judge[0];
        return { j.Accuracy, j.Good, j.Ok, j.Bad, j.Score, j.Roll, j.HitNote, j.MaxCombo };
    }
    DanCounters DanSongCounters() const {
        const JudgeData& j = Chart.Judge[0];
        const JudgeData& b = Dan.SongStart;
        DanCounters c;
        c.Good = j.Good - b.Good;
        c.Ok = j.Ok - b.Ok;
        c.Bad = j.Bad - b.Bad;
        c.Score = j.Score - b.Score;
        c.Roll = j.Roll - b.Roll;
        c.HitNote = j.HitNote - b.HitNote;
        c.MaxCombo = Dan.SongMaxCombo;
        c.Accuracy = c.HitNote != 0 ? (c.Good + c.Ok * 0.5) / (double)c.HitNote * 100.0 : 0.0;
        return c;
    }
    static double ExamCounterValue(ExamTypes type, const DanCounters& c) {
        switch (type) {
        case ExamTypes::Accuracy: return c.Accuracy;
        case ExamTypes::Good: return (double)c.Good;
        case ExamTypes::Ok: return (double)c.Ok;
        case ExamTypes::Bad: return (double)c.Bad;
        case ExamTypes::Score: return (double)c.Score;
        case ExamTypes::Roll: return (double)c.Roll;
        case ExamTypes::HitNote: return (double)c.HitNote;
        case ExamTypes::MaxCombo: return (double)c.MaxCombo;
        default: return 0.0;
        }
    }
    // 以上 (m): 金 >= PassVal[1], 赤 >= PassVal[0], 終了時に届いていなければ不合格
    // 未満 (l): PassVal[0] 以上になった時点で不合格、PassVal[1] 未満なら金
    static ExamStatus EvaluateExam(const ExamData& exam, double value, bool finished) {
        if (exam.Range == ExamRange::Less) {
            if (value >= exam.PassVal[0]) { return ExamStatus::Fail; }
            return value < exam.PassVal[1] ? ExamStatus::Gold : ExamStatus::Red;
        }
        if (value >= exam.PassVal[1]) { return ExamStatus::Gold; }
        if (value >= exam.PassVal[0]) { return ExamStatus::Red; }
        return finished ? ExamStatus::Fail : ExamStatus::NotYet;
    }

    // songfinished: 現在の曲が終わった (この曲の個別条件と、最終曲なら全体条件を確定する)
    void UpdateDanExams(bool songfinished) {
        if (!Dan.Active || Chart.Judge.empty()) {
            return;
        }
        // 曲内のコンボ (前の曲から続くコンボは数えない)
        const uint64_t combo = Chart.Judge[0].Combo;
        Dan.SongCombo = combo < Dan.LastCombo ? combo : Dan.SongCombo + (combo - Dan.LastCombo);
        Dan.LastCombo = combo;
        Dan.SongMaxCombo = std::max(Dan.SongMaxCombo, Dan.SongCombo);

        const bool lastsong = Dan.SongIndex + 1 >= Dan.Course.DanSongs.size();
        const DanCounters total = DanTotalCounters();
        const DanCounters song = DanSongCounters();

        for (auto& e : Dan.Exams) {
            if (e.Final || (e.Song >= 0 && (size_t)e.Song != Dan.SongIndex)) {
                continue;
            }
            const bool global = e.Song < 0;
            const bool finished = songfinished && (!global || lastsong);
            e.Value = ExamCounterValue(e.Exam.ExamType, global ? total : song);
            e.Status = EvaluateExam(e.Exam, e.Value, finished);
            if (e.Status == ExamStatus::Fail) {
                e.Final = true;
                if (!Dan.Failed) {
                    Dan.Failed = true;
                    Skin.Base->Playing.SE.DanFall.Play();
                }
            }
            if (finished) {
                e.Final = true;
            }
        }
    }
    // 0: 不合格, 2: 合格, 3: 金合格 (王冠画像の番号)
    int DanCrown() const {
        if (Dan.Failed) { return 0; }
        bool gold = true;
        for (const auto& e : Dan.Exams) {
            if (e.Status == ExamStatus::Fail || e.Status == ExamStatus::NotYet) { return 0; }
            gold = gold && e.Status == ExamStatus::Gold;
        }
        return gold ? 3 : 2;
    }

    // 段位選択画面: 選択中の段位の課題曲と条件を表示する
    void DrawDanInfo(const ChartData& chart) const {
        if (!chart.IsDan) {
            return;
        }
        std::vector<std::pair<std::string, unsigned int>> lines;
        const unsigned int white = GetColor(255, 255, 255);
        const unsigned int gray = GetColor(190, 190, 190);
        lines.push_back({ "■ 課題曲", white });
        for (size_t i = 0; i < chart.DanSongs.size(); ++i) {
            const auto& song = chart.DanSongs[i];
            lines.push_back({ std::to_string(i + 1) + ". " + song.Title + (song.Level ? "  ★" + std::to_string(song.Level) : ""), white });
            for (const auto& exam : song.Exams) {
                lines.push_back({ "    " + ExamConditionText(exam), gray });
            }
        }
        lines.push_back({ "", white });
        lines.push_back({ "■ 合格条件", white });
        for (const auto& exam : chart.ExamDatas) {
            lines.push_back({ "  " + ExamConditionText(exam), white });
        }

        const int lineheight = GetFontSize() + 8;
        const int x = Skin.Info.Resolution.X / 2 + 40;
        const int y = 80;
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 160);
        DrawBox(x - 16, y - 16, Skin.Info.Resolution.X - 24, y + lineheight * (int)lines.size() + 8, GetColor(0, 0, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        for (size_t i = 0; i < lines.size(); ++i) {
            DrawString(x, y + lineheight * (int)i, lines[i].first.c_str(), lines[i].second);
        }
    }

    void SetDrawBranchArea(Pos2D<float> DelayPos) const {
        if (Chart.IsBranchChart()) {
            SetDrawArea(
                Skin.Base->Playing.Image.Lane.Pos.X - Skin.Base->Playing.Image.Lane.Size.Width / 2,
                Skin.Base->Playing.Image.Lane.Pos.Y - Skin.Base->Playing.Image.Lane.Size.Height / 2 + DelayPos.Y,
                Skin.Base->Playing.Image.Lane.Pos.X + Skin.Base->Playing.Image.Lane.Size.Width / 2,
                Skin.Base->Playing.Image.Lane.Pos.Y + Skin.Base->Playing.Image.Lane.Size.Height / 2 + DelayPos.Y
            );
        }
    }
    // ---- スキンのキャラクター (プレイヤーキャラ・ランナー・踊り子・コンボボイス) ----
    struct SkinCharState {
        uint64_t LastCombo = 0;
        std::vector<std::pair<std::chrono::steady_clock::time_point, int>> Runners;   // 走り始めた時刻, 種類
        int RunnerKind = 0;
        std::chrono::steady_clock::time_point ComboAnimAt{};
        bool ComboAnim = false;
    } SkinChar;
    static bool HasImage(const GraphData& g) { return !g.Handles.empty() && g.Handles[0] != -1; }
    // 魂ゲージの代わりの目安 (0～100): 精度 × 進み具合。最後には精度と同じになる (精度 75% 以上がクリア)
    double SkinGauge() const {
        if (Chart.Judge.empty() || Chart.AllNoteCount == 0) { return 0.0; }
        const auto& j = Chart.Judge[0];
        return std::clamp(j.Accuracy * (double)j.HitNote / (double)Chart.AllNoteCount, 0.0, 100.0);
    }
    void SkinCharReset() {
        SkinChar = SkinCharState();
    }
    // コンボの増え方を見て、ランナー・コンボのアニメーション・コンボボイスを出す
    void SkinCharUpdate() {
        if (Chart.Judge.empty()) { return; }
        auto& pl = Skin.Base->Playing;
        const uint64_t combo = Chart.Judge[0].Combo;
        const auto now = std::chrono::steady_clock::now();
        if (combo > SkinChar.LastCombo) {
            if (HasImage(pl.Image.Runner)) {
                for (uint64_t c = SkinChar.LastCombo; c < combo && (int)SkinChar.Runners.size() < std::max(pl.Config.RunnerMax, 1); ++c) {
                    SkinChar.Runners.push_back({ now, SkinChar.RunnerKind++ });
                }
            }
            if (combo / 10 > SkinChar.LastCombo / 10 && HasImage(pl.Image.PlayerCharCombo)) {
                SkinChar.ComboAnim = true;
                SkinChar.ComboAnimAt = now;
            }
            for (const auto& [at, voice] : pl.SE.ComboVoice) {
                if (SkinChar.LastCombo < at && at <= combo) { voice->Play(); }
            }
        }
        SkinChar.LastCombo = combo;
        std::erase_if(SkinChar.Runners, [&](const auto& r) {
            return std::chrono::duration<double, std::milli>(now - r.first).count() > pl.Config.RunnerTime;
        });
    }
    void SkinCharDraw() {
        auto& pl = Skin.Base->Playing;
        const auto now = std::chrono::steady_clock::now();
        const double wall = std::chrono::duration<double, std::milli>(now.time_since_epoch()).count();
        auto frame = [](const GraphData& g, double t, double frametime, int row) {
            const int cols = std::max(g.Div.X, 1);
            const int f = (int)(t / std::max(frametime, 1.0)) % cols;
            return (row % std::max(g.Div.Y, 1)) * cols + f;
        };
        const double gauge = SkinGauge();
        // 踊り子: 目安が増えるごとに 1 人ずつ
        if (HasImage(pl.Image.Dancer) && pl.Config.DancerCount > 0) {
            const int count = std::min(pl.Config.DancerCount, (int)(gauge / (75.0 / pl.Config.DancerCount)));
            for (int i = 0; i < count; ++i) {
                pl.Image.Dancer.Draw({ pl.Config.DancerDistance.X * i, pl.Config.DancerDistance.Y * i }, frame(pl.Image.Dancer, wall + i * 37.0, pl.Config.DancerFrameTime, i));
            }
        }
        // プレイヤーキャラ: コンボのアニメーション > ゴーゴー > クリア > 通常
        if (SkinChar.ComboAnim) {
            const double t = std::chrono::duration<double, std::milli>(now - SkinChar.ComboAnimAt).count();
            const int frames = std::max(pl.Image.PlayerCharCombo.Div.X, 1);
            if (t < frames * pl.Config.PlayerCharFrameTime) {
                pl.Image.PlayerCharCombo.Draw({}, (int)(t / std::max(pl.Config.PlayerCharFrameTime, 1.0)));
            }
            else { SkinChar.ComboAnim = false; }
        }
        if (!SkinChar.ComboAnim) {
            const GraphData* g = &pl.Image.PlayerChar;
            if (Chart.NowGoGo && HasImage(pl.Image.PlayerCharGoGo)) { g = &pl.Image.PlayerCharGoGo; }
            else if (gauge >= 75.0 && HasImage(pl.Image.PlayerCharClear)) { g = &pl.Image.PlayerCharClear; }
            if (HasImage(*g)) { const_cast<GraphData*>(g)->Draw({}, frame(*g, wall, pl.Config.PlayerCharFrameTime, 0)); }
        }
        // ランナー: 出発位置から到着位置へ
        for (const auto& [at, kind] : SkinChar.Runners) {
            const double t = std::chrono::duration<double, std::milli>(now - at).count();
            const float r = (float)std::clamp(t / std::max(pl.Config.RunnerTime, 1.0), 0.0, 1.0);
            const Pos2D<float> pos{ pl.Config.RunnerFrom.X + (pl.Config.RunnerTo.X - pl.Config.RunnerFrom.X) * r, pl.Config.RunnerFrom.Y + (pl.Config.RunnerTo.Y - pl.Config.RunnerFrom.Y) * r };
            pl.Image.Runner.Draw(pos, frame(pl.Image.Runner, t, pl.Config.RunnerFrameTime, kind));
        }
    }
    void PlayingInit() {
        SkinCharReset();
        BeginPlayStats();
#ifndef __ANDROID__
        RecorderStart();
#endif
        if (IsMulti) {
            // 自分を 0 番レーンに、他のプレイヤーをルームの並び順で続ける
            Online.LaneIds.assign(1, Online.MyId);
            for (const auto& player : Online.Room.Players) {
                if (player.Id != Online.MyId && Online.LaneIds.size() < fasnet::MaxPlayers) {
                    Online.LaneIds.push_back(player.Id);
                }
            }
            Names.clear();
            Titles.clear();
            for (uint32_t id : Online.LaneIds) {
                const auto* player = Online.Room.Find(id);
                Names.push_back(player != nullptr ? player->Name : Config.PlayerName);
                Titles.push_back(player != nullptr ? player->Title : Config.TitleName);
            }
            Chart.Judge.resize(std::max(Chart.Judge.size(), LaneCount()));
            HitNote.resize(std::max(HitNote.size(), LaneCount()));
            for (size_t lane = 1; lane < Chart.Judge.size(); ++lane) {
                Chart.Judge[lane] = JudgeData();
            }
        }
        for (auto&& taiko : MiniTaikoFlash) {
            taiko.Reset();
        }
    }
    void PlayingEnd() {
#ifndef __ANDROID__
        RecorderStop();
#endif
        Chart.SongData.Delete();
        Chart.Movie.Delete();
        if (NowScene == Scene::Result) {
            FinishPlayStats();
        }
    }
    void PlayingDraw() {

        const double NowTime = ChartNowTime(1000, Config.FrameCountTimer, Config.FastDrawRate, Config.FrameExtendRate) + Training.Offset;
        const double ScrollNow = Chart.ScrollType == ScrollType::Normal ? NowTime : Chart.FrozenTime(NowTime);
        Chart.NowBPM = Chart.BpmAt(NowTime);

        Skin.Base->Playing.Image.BackGround.Draw({});
        SkinCharUpdate();
        SkinCharDraw();

#ifndef __ANDROID__
        if (Chart.Movie.IsLoaded()) {
            // 譜面の進行に再生位置を合わせる (一時停止・巻き戻し・ずれは Sync がシークで補正する)
            // MOVIEOFFSET は従来通り譜面の時刻 (0 = 演奏開始) を基準に、正なら遅らせて負なら先に進める
            Chart.Movie.Sync(NowTime - Chart.OriginalData.MovieOffset * 1000.0, Chart.NowTime.IsRunning());

            if (Config.BGBrightness > 0) {
                Chart.Movie.Draw(0.0f, 0.0f, (float)Skin.Info.Resolution.X, (float)Skin.Info.Resolution.Y);
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(255 * (1 - (Config.BGBrightness / 100))));
                DrawFillBox(0, 0, Skin.Info.Resolution.X, Skin.Info.Resolution.Y, GetColor(0, 0, 0));
                SetDrawBlendMode(0, 0);
            }
        }
#endif
        int idx = 0;

        do {

            Pos2D<float> DelayPos = {
                    0.0f,
                    idx * Skin.Base->Playing.Config.MultiPlayLaneDistance
            };

            if (LaneCount() == 4) {
                DelayPos.Y -= Skin.Base->Playing.Config.MultiPlayLaneDistance;
            }

#ifndef __ANDROID__
            if (Chart.Movie.Handle != -1) {
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 225);
            }
            Skin.Base->Playing.Image.LaneFrame.Draw(DelayPos);
            Skin.Base->Playing.Image.Lane.Draw(DelayPos);
            SetDrawBlendMode(0, 0);
#else
            SetDrawBlendMode(0, 0);
            Skin.Base->Playing.Image.LaneFrame.Draw(DelayPos);
            Skin.Base->Playing.Image.Lane.Draw(DelayPos);
#endif

            SetDrawBranchArea(DelayPos);

            switch (Config.JiroChartMode && Chart.NowBranchFlag != BranchType::Null ? Chart.JiroBranchOf(Chart.JiroShown - 1) : Chart.NowBranchFlag) {
            case BranchType::Normal:
                Skin.Base->Playing.Image.NormalLane.Draw(DelayPos);
                break;
            case BranchType::Expert:
                Skin.Base->Playing.Image.ExpertLane.Draw(DelayPos);
                break;
            case BranchType::Master:
                Skin.Base->Playing.Image.MasterLane.Draw(DelayPos);
                break;
            }

            double NoteBranchMotion = 0.0;

            if (Chart.NowBranchAnimation != ABranchType::Null) {
                double _one = Chart.BranchAnimationTimer.GetElapsed().Second() / Chart.BranchAnimationTime;
                float _motiony = Skin.Base->Playing.Image.Lane.Size.Height * GetEasingRate(_one, ease::Base::Out, ease::Line::Sine);
                NoteBranchMotion = _motiony;
                if (_one > 1.0) {
                    Chart.NowBranchAnimation = ABranchType::Null;
                    Chart.BranchAnimationTimer.Reset();
                }
                switch (Chart.NowBranchAnimation) {
                case ABranchType::Normal_Expert:
                    Skin.Base->Playing.Image.NormalLane.Draw({ 0, _motiony + DelayPos.Y });
                    Skin.Base->Playing.Image.ExpertLane.Draw({ 0, Skin.Base->Playing.Image.Lane.Size.Height + _motiony + DelayPos.Y });
                    break;
                case ABranchType::Normal_Master:
                    Skin.Base->Playing.Image.NormalLane.Draw({ 0, _motiony + DelayPos.Y });
                    Skin.Base->Playing.Image.MasterLane.Draw({ 0, Skin.Base->Playing.Image.Lane.Size.Height + _motiony + DelayPos.Y });
                    break;
                case ABranchType::Expert_Normal:
                    Skin.Base->Playing.Image.ExpertLane.Draw({ 0, _motiony + DelayPos.Y });
                    Skin.Base->Playing.Image.NormalLane.Draw({ 0, Skin.Base->Playing.Image.Lane.Size.Height + _motiony + DelayPos.Y });
                    break;
                case ABranchType::Expert_Master:
                    Skin.Base->Playing.Image.ExpertLane.Draw({ 0, _motiony + DelayPos.Y });
                    Skin.Base->Playing.Image.MasterLane.Draw({ 0, Skin.Base->Playing.Image.Lane.Size.Height + _motiony + DelayPos.Y });
                    break;
                case ABranchType::Master_Normal:
                    Skin.Base->Playing.Image.MasterLane.Draw({ 0, _motiony + DelayPos.Y });
                    Skin.Base->Playing.Image.NormalLane.Draw({ 0, Skin.Base->Playing.Image.Lane.Size.Height + _motiony + DelayPos.Y });
                    break;
                case ABranchType::Master_Expert:
                    Skin.Base->Playing.Image.MasterLane.Draw({ 0, _motiony + DelayPos.Y });
                    Skin.Base->Playing.Image.ExpertLane.Draw({ 0, Skin.Base->Playing.Image.Lane.Size.Height + _motiony + DelayPos.Y });
                    break;
                }
            }
            SetDrawAreaFull();
            SetDrawBlendMode(0, 0);

            Skin.Base->Playing.Image.ProgressBar.Draw(DelayPos, 0);
            if (Chart.Judge[idx].HitNote > 0) {

                double Ratio = ((double)Chart.Judge[idx].Good + (double)Chart.Judge[idx].Ok * 0.5) / Chart.AllNoteCount;
                float Width = Skin.Base->Playing.Image.ProgressBar.Size.Width * Ratio;
                float MaxWidth = Skin.Base->Playing.Image.ProgressBar.Size.Width;

                Skin.Base->Playing.Image.ProgressBar.RectDraw(
                    DelayPos,
                    { 0, Skin.Base->Playing.Image.ProgressBar.Size.Height },
                    { Width < MaxWidth ? Width : MaxWidth,
                      Skin.Base->Playing.Image.ProgressBar.Size.Height },
                    1
                );
            }

            Skin.Base->Playing.Image.Note.Draw(DelayPos);

            if (Chart.NowGoGo) {
                Skin.Base->Playing.Image.GoGoFire.Draw(DelayPos, (size_t)(NowTime / Skin.Base->Playing.Config.GoGoFireFrameTime) % Skin.Base->Playing.Image.GoGoFire.Div.X);
            }

            {

                size_t i = HitNote[idx].Index;
                const double JudgeUnderExplosionTime = Skin.Base->Playing.Config.JudgeUpperExplosionFrameTime * Skin.Base->Playing.Image.JudgeUnderExplosion.Div.X;

                for (size_t c = 0; c < HitNote[idx].Size(); ++c) {
                    auto&& data = HitNote[idx].Datas[i];
                    if (!data.MoveTimer.IsRunning()) {
                        data.MoveTimer.Start();
                    }
                    data.MoveElapsedTime = data.MoveTimer.GetElapsed().MilliSecond();
                    if (data.JudgeUnderExplosion.IsActive && data.MoveElapsedTime < JudgeUnderExplosionTime) {
                        size_t drawindex = data.MoveElapsedTime / Skin.Base->Playing.Config.JudgeUpperExplosionFrameTime;
                        drawindex += (2 * Skin.Base->Playing.Image.JudgeUnderExplosion.Div.X) * data.JudgeUnderExplosion.Big;
                        if (data.JudgeUnderExplosion.Type == JudgeType::Ok) {
                            drawindex += Skin.Base->Playing.Image.JudgeUnderExplosion.Div.X;
                        }
                        Skin.Base->Playing.Image.JudgeUnderExplosion.Draw(DelayPos, drawindex);
                    }
                    else {
                        data.JudgeUnderExplosion.IsActive = false;
                    }

                    ++i;
                    if (!(i < HitNote[idx].Size())) {
                        i = 0;
                    }
                }
            }

            auto NoteAlpha = [&](double _one, AlphaType Type) {
                int alpha = 255;
                _one = std::clamp(_one, 0.0, 1.0);
                switch (Type) {
                case AlphaType::Hidden:
                    alpha = 255 * _one;
                    break;
                case AlphaType::Sudden:
                    alpha = alpha * (1 - _one);
                    break;
                }
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
                };

            const Pos2D<double>& NoteOrigin = {
                    Skin.Base->Playing.Image.Note.Pos.X,
                    Skin.Base->Playing.Image.Note.Pos.Y + DelayPos.Y
            };

            // 次郎1 と同じ表示位置:
            //   通常      : (音符の時刻 - 現在時刻) × 音符の BPM × SCROLL
            //   BM/HB     : (音符の拍位置 - 現在の拍位置) × SCROLL。拍は BPM 区間ごとに積分し、#DELAY の間は止まる
            //               BMSCROLL は #SCROLL を無視する (ハイスピードは有効)
            auto GetNotePos = [&](NoteData& data)->Pos2D<double> {
                const bool BMScroll = ScrollType::BMSCROLL == Chart.ScrollType;
                double distance = 0;
                double scroll = data.Scroll;
                if (ScrollType::Normal == Chart.ScrollType) {
                    if (Config.JiroChartMode) {
                        // 次郎と同じく、音符の時刻で BPM と SCROLL を引く (書いた位置の値ではない。ダミーノーツ・疑似停止の見え方)
                        const BranchType branch = data.IsBranch == BranchType::Null || data.IsBranch == BranchType::NotDisplay ? Chart.NowBranchFlag : data.IsBranch;
                        distance = (data.AbsTime - NowTime) / (240 / Chart.JiroBpmAt(data.AbsTime));
                        scroll = Chart.JiroScrollAt(data.AbsTime, branch) * Config.ChartSpeed;
                    }
                    else {
                        distance = (data.AbsTime - NowTime) / (240 / data.BPM);
                    }
                }
                else {
                    distance = Chart.ScrollDistance(data.AbsTime, ScrollNow) * 1000.0;     // 通常と同じ単位 (ms × BPM / 240)
                    if (Config.JiroChartMode && !BMScroll) {
                        const BranchType branch = data.IsBranch == BranchType::Null || data.IsBranch == BranchType::NotDisplay ? Chart.NowBranchFlag : data.IsBranch;
                        scroll = Chart.JiroScrollAt(data.AbsTime, branch) * Config.ChartSpeed;
                    }
                }

                Pos2D<double> _ret = {
                        distance * Skin.Base->Playing.Config.LaneExtendRate * (BMScroll ? Config.ChartSpeed : scroll),
                        distance * Skin.Base->Playing.Config.LaneExtendRate * (BMScroll ? 0 : data.Scrolli)
                };
                // 次郎形式: 位置は 1px 単位 (区間ごとに切り捨てて足す。#BPMCHANGE の瞬間に音符が少し戻って見えるのも次郎と同じ)
                if (Config.JiroChartMode) {
                    const double unit = 1000.0 * Skin.Base->Playing.Config.LaneExtendRate * (BMScroll ? Config.ChartSpeed : scroll);
                    _ret.X = ScrollType::Normal == Chart.ScrollType ? std::trunc(_ret.X) : Chart.ScrollPx(data.AbsTime, ScrollNow, unit);
                }

                Pos2D<float> SkinPos = {
                        Skin.Base->Playing.Image.Lane.Size.Width,
                        Skin.Base->Playing.Image.Lane.Size.Height
                };

                _ret = {
                        _ret.X + NoteOrigin.X,
                        _ret.Y + NoteOrigin.Y
                };

                return _ret;

                };

#define InRange(x, y) (x > Skin.SimulationRect.Left && x < Skin.SimulationRect.Right && y > Skin.SimulationRect.Top && y < Skin.SimulationRect.Bottom)

            std::complex<double> n1{};
            std::complex<double> n2{};
            std::complex<double> n3{};
            std::complex<double> n4{};
            std::complex<double> facing{};
            Pos2D<double> NotePos{};

            const std::complex<double> n0 = { Skin.Base->Playing.Image.Note.Size.Width / 2, Skin.Base->Playing.Image.Note.Size.Height / 2 };
            const double n0r = std::abs(n0);
            const double narr[4] = {
                    std::arg(std::complex<double>{ n0.real() * -1, n0.imag() * -1 }),
                    std::arg(std::complex<double>{ n0.real() * 1, n0.imag() * -1 }),
                    std::arg(std::complex<double>{ n0.real() * 1, n0.imag() * 1 }),
                    std::arg(std::complex<double>{ n0.real() * -1, n0.imag() * 1 })
            };

            SetDrawBranchArea(DelayPos);

            for (auto&& data : Chart.RawNoteDatas | std::ranges::views::reverse) {

                double NoteTheta = atan2(data.Scrolli, data.Scroll);
                double BranchDelayPos = 0.0;

                NotePos = GetNotePos(data);

                bool NoteBranch = data.IsBranch == Chart.NowBranchFlag || data.IsBranch == BranchType::Null;
                // 次郎形式: 次の分岐の区間の音符は、決まった分岐 (JiroNext) のものを先に表示する
                if (Config.JiroChartMode && data.IsBranch != BranchType::Null && data.IsBranch != BranchType::NotDisplay) {
                    NoteBranch = data.AbsTime >= Chart.JiroUpcomingStart ? data.IsBranch == Chart.JiroNext : data.IsBranch == Chart.NowBranchFlag;
                }

                switch (Chart.NowBranchAnimation) {
                case ABranchType::Normal_Expert:
                    if (data.IsBranch == BranchType::Normal) { NoteBranch = true; BranchDelayPos += NoteBranchMotion; }
                    if (data.IsBranch == BranchType::Expert) { NoteBranch = true; BranchDelayPos -= Skin.Base->Playing.Image.Lane.Size.Height - NoteBranchMotion; }
                    break;
                case ABranchType::Normal_Master:
                    if (data.IsBranch == BranchType::Normal) { NoteBranch = true; BranchDelayPos += NoteBranchMotion; }
                    if (data.IsBranch == BranchType::Master) { NoteBranch = true; BranchDelayPos -= Skin.Base->Playing.Image.Lane.Size.Height - NoteBranchMotion; }
                    break;
                case ABranchType::Expert_Normal:
                    if (data.IsBranch == BranchType::Expert) { NoteBranch = true; BranchDelayPos -= NoteBranchMotion; }
                    if (data.IsBranch == BranchType::Normal) { NoteBranch = true; BranchDelayPos += Skin.Base->Playing.Image.Lane.Size.Height - NoteBranchMotion; }
                    break;
                case ABranchType::Expert_Master:
                    if (data.IsBranch == BranchType::Expert) { NoteBranch = true; BranchDelayPos += NoteBranchMotion; }
                    if (data.IsBranch == BranchType::Master) { NoteBranch = true; BranchDelayPos -= Skin.Base->Playing.Image.Lane.Size.Height - NoteBranchMotion; }
                    break;
                case ABranchType::Master_Normal:
                    if (data.IsBranch == BranchType::Master) { NoteBranch = true; BranchDelayPos -= NoteBranchMotion; }
                    if (data.IsBranch == BranchType::Normal) { NoteBranch = true; BranchDelayPos += Skin.Base->Playing.Image.Lane.Size.Height - NoteBranchMotion; }
                    break;
                case ABranchType::Master_Expert:
                    if (data.IsBranch == BranchType::Master) { NoteBranch = true; BranchDelayPos -= NoteBranchMotion; }
                    if (data.IsBranch == BranchType::Expert) { NoteBranch = true; BranchDelayPos += Skin.Base->Playing.Image.Lane.Size.Height - NoteBranchMotion; }
                    break;
                }

                NotePos.Y += BranchDelayPos;

                if (!NoteBranch) { continue; }
                if (data.BarlineDisplay) {
                    SetDrawBlendMode(0, 0);
                    if (InRange(NotePos.X, NotePos.Y)) {
                        DrawLineAA(
                            NotePos.X,
                            NotePos.Y - 65,
                            NotePos.X,
                            NotePos.Y + 65,
                            GetColor(255, 255, 255 * !data.Section)
                        );
                    }
                }

                if (data.NoteType == '0') {
                    continue;
                }

                if (data.NoteType >= '1' &&
                    data.NoteType <= '4') {
                    if (InRange(NotePos.X, NotePos.Y)) {
                        double hidden = Config.HiddenLevel;
                        double sudden = Config.SuddenLevel;
                        bool hiddenflag = hidden > 0.0;
                        bool suddenflag = sudden > 0.0;
                        bool multiflag = (IsMulti && idx >= 1);
                        if (hiddenflag || suddenflag || multiflag) {
                            double _abs = std::abs(std::complex<double>{ NotePos.X - NoteOrigin.X, NotePos.Y - NoteOrigin.Y })* (data.AbsTime < NowTime ? -1 : 1);
                            double leveling = (Skin.Base->Playing.Image.Lane.Size.Width / DX_PI);
                            double feedrange = (Skin.Base->Playing.Image.Lane.Size.Width / DX_TWO_PI);
                            if (multiflag) {
                                NoteAlpha((_abs - (leveling * 0.025)) / feedrange, AlphaType::Hidden);
                            }
                            if (hiddenflag) {
                                NoteAlpha((_abs - (leveling * hidden)) / feedrange, AlphaType::Hidden);
                            }
                            if (suddenflag) {
                                NoteAlpha(((_abs - (Skin.Base->Playing.Image.Lane.Size.Width)) + (leveling * sudden)) / feedrange, AlphaType::Sudden);
                            }
                        }
                        Skin.Base->Playing.Image.Note.Draw(
                            {
                                    (float)(NotePos.X - NoteOrigin.X),
                                    (float)(NotePos.Y - NoteOrigin.Y) + DelayPos.Y
                            },
                            data.NoteType - 48
                        );
                    }
                    continue;
                }
                SetDrawBlendMode(0, 0);

                if (data.NoteType >= '5' &&
                    data.NoteType <= '6') {
                    const Pos2D<double>& cnote = NotePos;
                    Pos2D<double> dnote = GetNotePos(Chart.RawNoteDatas[data.RollEndIndex]);
                    dnote.Y += BranchDelayPos;

                    bool DispFlag =
                        InRange(cnote.X, cnote.Y) ||
                        (data.AbsTime < NowTime && data.RollEndTime > NowTime) ||
                        InRange(dnote.X, dnote.Y);

                    if (DispFlag) {

                        bool BigRollFlag = data.NoteType == '6';

                        const std::complex<double>& cdnote = { dnote.X - cnote.X, dnote.Y - cnote.Y };

                        double RollTheta = std::arg(cdnote);
                        facing = std::polar(n0.real() - 3, RollTheta);

                        n1 = std::polar(n0r, narr[0] + RollTheta);
                        n2 = std::polar(n0r, narr[1] + RollTheta);
                        n3 = std::polar(n0r, narr[2] + RollTheta);
                        n4 = std::polar(n0r, narr[3] + RollTheta);

                        DrawModiGraphF(
                            cnote.X + facing.real() + n1.real(),
                            cnote.Y + facing.imag() + n1.imag(),
                            dnote.X - facing.real() + n2.real(),
                            dnote.Y - facing.imag() + n2.imag(),
                            dnote.X - facing.real() + n3.real(),
                            dnote.Y - facing.imag() + n3.imag(),
                            cnote.X + facing.real() + n4.real(),
                            cnote.Y + facing.imag() + n4.imag(),
                            Skin.Base->Playing.Image.Note.Handles[BigRollFlag ? 9 : 6],
                            TRUE
                        );
                        DrawModiGraphF(
                            dnote.X + n1.real(),
                            dnote.Y + n1.imag(),
                            dnote.X + n2.real(),
                            dnote.Y + n2.imag(),
                            dnote.X + n3.real(),
                            dnote.Y + n3.imag(),
                            dnote.X + n4.real(),
                            dnote.Y + n4.imag(),
                            Skin.Base->Playing.Image.Note.Handles[BigRollFlag ? 10 : 7],
                            TRUE
                        );
                        DrawModiGraphF(
                            cnote.X + n1.real(),
                            cnote.Y + n1.imag(),
                            cnote.X + n2.real(),
                            cnote.Y + n2.imag(),
                            cnote.X + n3.real(),
                            cnote.Y + n3.imag(),
                            cnote.X + n4.real(),
                            cnote.Y + n4.imag(),
                            Skin.Base->Playing.Image.Note.Handles[BigRollFlag ? 8 : 5],
                            TRUE
                        );

                    }
                }

                if (data.NoteType == '7' ||
                    data.NoteType == '9') {

                    if (data.BalloonFlag == 1) {
                        NotePos = NoteOrigin;
                    }
                    if (data.BalloonFlag == 2) {
                        NotePos = GetNotePos(Chart.RawNoteDatas[data.RollEndIndex]);
                    }

                    if (InRange(NotePos.X, NotePos.Y)) {

                        bool KusudamaFlag = data.NoteType == '9';

                        facing = std::polar(n0.real() * 2, NoteTheta);

                        n1 = std::polar(n0r, narr[0] + NoteTheta);
                        n2 = std::polar(n0r, narr[1] + NoteTheta);
                        n3 = std::polar(n0r, narr[2] + NoteTheta);
                        n4 = std::polar(n0r, narr[3] + NoteTheta);

                        DrawModiGraphF(
                            NotePos.X + n1.real(),
                            NotePos.Y + n1.imag(),
                            NotePos.X + n2.real(),
                            NotePos.Y + n2.imag(),
                            NotePos.X + n3.real(),
                            NotePos.Y + n3.imag(),
                            NotePos.X + n4.real(),
                            NotePos.Y + n4.imag(),
                            Skin.Base->Playing.Image.Note.Handles[KusudamaFlag ? 13 : 11],
                            TRUE
                        );
                        DrawModiGraphF(
                            NotePos.X + facing.real() + n1.real(),
                            NotePos.Y + facing.imag() + n1.imag(),
                            NotePos.X + facing.real() + n2.real(),
                            NotePos.Y + facing.imag() + n2.imag(),
                            NotePos.X + facing.real() + n3.real(),
                            NotePos.Y + facing.imag() + n3.imag(),
                            NotePos.X + facing.real() + n4.real(),
                            NotePos.Y + facing.imag() + n4.imag(),
                            Skin.Base->Playing.Image.Note.Handles[KusudamaFlag ? 14 : 12],
                            TRUE
                        );
                    }
                }
#undef InRange
            }
            SetDrawAreaFull();

            Skin.Base->Playing.Image.Base.Draw(DelayPos);
            Skin.Base->Playing.Image.NamePlate.Draw(DelayPos);
            Skin.Base->Playing.Image.MiniTaiko.Draw(DelayPos);

            if (const std::string& title = LaneTitle(idx); !title.empty()) {
                Skin.Base->Playing.Font.TitleName.Draw({
                    Skin.Base->Playing.Config.TitleNamePos.X,
                    Skin.Base->Playing.Config.TitleNamePos.Y + (IsMulti ? DelayPos.Y : 0.0f) },
                    GetColor(255, 255, 255),
                    GetColor(0, 0, 0),
                    title
                );
            }
            if (!IsMulti) {
                Skin.Base->Playing.Font.PlayerName.Draw(
                    Skin.Base->Playing.Config.PlayerNamePos,
                    GetColor(255, 255, 255),
                    GetColor(0, 0, 0),
                    Config.PlayerName
                );
            }
            else {
                Skin.Base->Playing.Font.PlayerName.Draw({
                Skin.Base->Playing.Config.PlayerNamePos.X,
                Skin.Base->Playing.Config.PlayerNamePos.Y + DelayPos.Y },
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                Names[idx]
                );
            }

            {

                size_t i = HitNote[idx].Index;

                for (size_t c = 0; c < HitNote[idx].Size(); ++c) {
                    auto& data = HitNote[idx].Datas[i];
                    if (!data.MoveTimer.IsRunning()) {
                        data.MoveTimer.Start();
                    }

                    if (data.FlyingNote.IsActive && Config.HitNoteDisp && !IsMulti && data.MoveElapsedTime < data.FlyingNote.MoveTime()) {

                        float _one = (data.MoveElapsedTime / data.FlyingNote.MoveTime());

                        std::complex<float> _pos1 = { 840, -90 };
                        std::complex<float> _pos2 = std::polar(280.0f, (DX_PI_F / 2) + std::arg(_pos1));
                        std::complex<float> _r = { (_pos1.real() / 2) + _pos2.real(), (_pos1.imag() / 2) + _pos2.imag() };
                        float mem0arg = std::arg(_r);
                        _r = { _r.real() + -840.0f, _r.imag() + 90.0f };
                        float mem1arg = std::arg(_r);
                        _r = { (_pos1.real() / 2) + _pos2.real(), (_pos1.imag() / 2) + _pos2.imag() };
                        float allarg = mem1arg - mem0arg;
                        std::complex<float> _c1 = std::polar(std::abs(_r), allarg * _one + mem0arg);

                        Pos2D<float> Pos = {
                                _r.real() - _c1.real(),
                                _r.imag() - _c1.imag()
                        };

                        Skin.Base->Playing.Image.Note.Draw(Pos, data.FlyingNote.Type - 48);
                    }
                    else {
                        data.FlyingNote.IsActive = false;
                    }

                    if (data.JudgeString.IsActive && data.MoveElapsedTime < data.JudgeString.MoveTime()) {
                        double alpha = 255 * (1 - GetEasingRate(data.MoveElapsedTime / data.JudgeString.MoveTime(), ease::Base::In, ease::Line::Cubic));
                        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
                        Skin.Base->Playing.Image.JudgeString.Draw(DelayPos, (size_t)data.JudgeString.Type);
                        SetDrawBlendMode(0, 0);
                    }
                    else {
                        data.JudgeString.IsActive = false;
                    }

                    if (!data.FlyingNote.IsActive && !data.JudgeUnderExplosion.IsActive && !data.JudgeString.IsActive) {
                        data = HitNoteData();
                    }

                    ++i;
                    if (!(i < HitNote[idx].Size())) {
                        i = 0;
                    }
                }
            }

            auto TaikoAlpha = [&](size_t index) {
                double alpha = 255 * (1 - GetEasingRate(MiniTaikoFlash[index].GetElapsed().MilliSecond() / MiniTaikoFlashTime, ease::Base::In, ease::Line::Cubic));
                if (alpha < 0) { MiniTaikoFlash[index].Reset(); }
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
                };

            if (MiniTaikoFlash[0 + (4 * idx)].IsRunning()) {
                TaikoAlpha(0 + (4 * idx));
                Skin.Base->Playing.Image.MiniTaiko_Don.Draw({ Skin.Base->Playing.Image.MiniTaiko_Don.Size.Width * -0.5f, DelayPos.Y }, 0);
            }
            if (MiniTaikoFlash[1 + (4 * idx)].IsRunning()) {
                TaikoAlpha(1 + (4 * idx));
                Skin.Base->Playing.Image.MiniTaiko_Ka.Draw({ Skin.Base->Playing.Image.MiniTaiko_Ka.Size.Width * -0.5f, DelayPos.Y }, 0);
            }
            if (MiniTaikoFlash[2 + (4 * idx)].IsRunning()) {
                TaikoAlpha(2 + (4 * idx));
                Skin.Base->Playing.Image.MiniTaiko_Don.Draw({ Skin.Base->Playing.Image.MiniTaiko_Don.Size.Width * 0.5f, DelayPos.Y }, 1);
            }
            if (MiniTaikoFlash[3 + (4 * idx)].IsRunning()) {
                TaikoAlpha(3 + (4 * idx));
                Skin.Base->Playing.Image.MiniTaiko_Ka.Draw({ Skin.Base->Playing.Image.MiniTaiko_Ka.Size.Width * 0.5f, DelayPos.Y }, 1);
            }
            SetDrawBlendMode(0, 0);

            auto ComboDraw = [&](uint64_t num) {
                int digit = exmath::digit(num);
                float offset = Skin.Base->Playing.Image.ComboNumber.Size.Width * (digit - 1) / 2;
                int i = 0;
                do {
                    Skin.Base->Playing.Image.ComboNumber.Draw({ offset, DelayPos.Y }, num % 10);
                    num /= 10;
                    ++i;
                    offset -= Skin.Base->Playing.Image.ComboNumber.Size.Width;
                } while (i < digit);
                };

            auto ScoreDraw = [&](uint64_t num) {
                int digit = exmath::digit(num);
                float offset = Skin.Base->Playing.Image.ScoreNumber.Size.Width - (digit - 1) + digit;
                int i = 0;
                do {
                    Skin.Base->Playing.Image.ScoreNumber.Draw({ offset, DelayPos.Y }, num % 10);
                    num /= 10;
                    ++i;
                    offset -= Skin.Base->Playing.Image.ScoreNumber.Size.Width;
                } while (i < digit);
                };
            auto RollDraw = [&](uint64_t num) {
                int digit = exmath::digit(num);
                float offset = Skin.Base->Playing.Image.RollNumber.Size.Width - (digit - 1) + digit;
                int i = 0;
                do {
                    Skin.Base->Playing.Image.RollNumber.Draw({ offset, DelayPos.Y }, num % 10);
                    num /= 10;
                    ++i;
                    offset -= Skin.Base->Playing.Image.RollNumber.Size.Width;
                } while (i < digit);
                };

            if (Chart.Judge[idx].Rolls.NowCount > 0) {
                RollDraw(Chart.Judge[idx].Rolls.NowCount);
            }
            if (Chart.Judge[idx].Combo >= 3) {
                ComboDraw(Chart.Judge[idx].Combo);
            }
            ScoreDraw(Chart.Judge[idx].Score);

            idx++;

        } while (idx < (int)LaneCount() && IsMulti);

        if (LaneCount() < 4) {
            if (!Chart.IsDanMode()) {
                if (Chart.OriginalData.TitleDisplay) {
                    Skin.Base->Playing.Font.Title.Draw(
                        Skin.Base->Playing.Config.TitlePos,
                        GetColor(255, 255, 255),
                        GetColor(0, 0, 0),
                        Chart.OriginalData.TitleStrlen.Playing,
                        Chart.OriginalData.Title
                    );
                }
                if (Chart.OriginalData.SubtitleDisplay) {
                    Skin.Base->Playing.Font.SubTitle.Draw(
                        Skin.Base->Playing.Config.SubTitlePos,
                        GetColor(255, 255, 255),
                        GetColor(0, 0, 0),
                        Chart.OriginalData.SubtitleStrlen.Playing,
                        Chart.OriginalData.Subtitle
                    );
                }
            }
            else if (Dan.Active && Dan.SongIndex < Dan.Course.DanSongs.size()) {
                const auto& song = Dan.Course.DanSongs[Dan.SongIndex];
                const std::string title = "[" + std::to_string(Dan.SongIndex + 1) + "/" + std::to_string(Dan.Course.DanSongs.size()) + "] " + song.Title;
                Skin.Base->Playing.Font.Title.Draw(
                    Skin.Base->Playing.Config.TitlePos,
                    GetColor(255, 255, 255),
                    GetColor(0, 0, 0),
                    GetStrlenCached(title, Skin.Base->Playing.Font.Title.Handle),
                    title
                );
                Skin.Base->Playing.Font.SubTitle.Draw(
                    Skin.Base->Playing.Config.SubTitlePos,
                    GetColor(255, 255, 255),
                    GetColor(0, 0, 0),
                    song.SubtitleStrlen,
                    song.Subtitle
                );
            }
        }

        if (!IsMulti) {
            if (Config.TrainingMode) {
                if (!Chart.NowTime.IsRunning()) {
                    DrawFormatString(
                        Skin.Base->Playing.Image.Note.Pos.X,
                        Skin.Base->Playing.Image.Note.Pos.Y - 100,
                        GetColor(255, 255, 255),
                        "(%d/%d)",
                        Training.BarlineIndex, Chart.AllBarlineCount
                    );
                }
            }
            if (Dan.Active) {

                Skin.Base->Playing.Image.DanBackGround.Draw({});

                // 全体条件と、現在の曲の個別条件を表示する
                size_t row = 0;
                for (const auto& e : Dan.Exams) {
                    if (e.Song >= 0 && (size_t)e.Song != Dan.SongIndex) {
                        continue;
                    }
                    const float y = 120.0f * row++;
                    auto& bar = Skin.Base->Playing.Image.ExamProgressBar;
                    bar.Draw({ 0, y }, 0);

                    const bool less = e.Exam.Range == ExamRange::Less;
                    const double red = e.Exam.PassVal[0];
                    // 以上: 赤合格に対する達成度 / 未満: 残りの許容量
                    const double shown = less ? std::max(0.0, red - e.Value) : e.Value;
                    const double ratio = red > 0 ? std::clamp(shown / red, 0.0, 1.0) : 1.0;
                    if (e.Status != ExamStatus::Fail) {
                        bar.RectDraw({ 0, y }, { 0, bar.Size.Height }, { (float)(bar.Size.Width * ratio), bar.Size.Height }, 1);
                    }

                    unsigned int color = GetColor(255, 255, 255);
                    switch (e.Status) {
                    case ExamStatus::Red: color = GetColor(255, 110, 110); break;
                    case ExamStatus::Gold: color = GetColor(255, 215, 0); break;
                    case ExamStatus::Fail: color = GetColor(128, 128, 128); break;
                    default: break;
                    }
                    const std::string name = std::string(e.Song >= 0 ? "[曲] " : "") + ExamLabel(e.Exam.ExamType)
                        + " " + ExamValueText(e.Exam.ExamType, red) + (less ? "未満" : "以上");
                    const std::string value = ExamValueText(e.Exam.ExamType, shown);

                    Skin.Base->Playing.Font.ExamName.Draw(
                        { Skin.Base->Playing.Config.ExamNamePos.X, Skin.Base->Playing.Config.ExamNamePos.Y + y },
                        GetColor(255, 255, 255), GetColor(0, 0, 0),
                        GetStrlenCached(name, Skin.Base->Playing.Font.ExamName.Handle), name);
                    Skin.Base->Playing.Font.ExamVal.Draw(
                        { Skin.Base->Playing.Config.ExamValPos.X, Skin.Base->Playing.Config.ExamValPos.Y + y },
                        color, GetColor(0, 0, 0),
                        GetStrlenCached(value, Skin.Base->Playing.Font.ExamVal.Handle), value);
                }
            }
        }

#ifdef __ANDROID__
        if (!IsMulti) {
            Skin.Base->Playing.Image.Back.Draw({});
            if (Config.TrainingMode && !Chart.IsDanMode()) {
                Skin.Base->Playing.Image.Pause.Draw({});
            }
        }
#endif

        if (IsMulti && Online.StartPending) {
            const auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(Online.StartAt - std::chrono::steady_clock::now()).count();
            const std::string text = "まもなく開始 " + std::to_string(std::max<int64_t>(0, (remain + 999) / 1000));
            Skin.Base->Playing.Font.Title.Draw(
                { Skin.Info.Resolution.X * 0.5f, Skin.Info.Resolution.Y * 0.5f },
                GetColor(255, 255, 255), GetColor(0, 0, 0),
                GetStrlenCached(text, Skin.Base->Playing.Font.Title.Handle) / 2, text);
        }

        if (Editor.Testing) {
            const FontData& sfont = ConfigSmallFont();
            UIText(sfont, 16, Skin.Info.Resolution.Y - sfont.Size * 2.2f, std::string("テストプレイ  オート: ") + (Config.AutoPlay ? "ON" : "OFF") + "  (F1: 切り替え / Esc: 編集に戻る)", GetColor(255, 220, 90));
        }
        // スコアメーター: 画面の中央・最下部
        if (Config.ScoreMeter) {
            const float width = std::min(360.0f, Skin.Info.Resolution.X - 32.0f);
            const HitErrorSummary summary = SummarizeHitErrors(HitStats.Play, HitErrorLimit());
            DrawHitErrorMeter(Skin.Info.Resolution.X * 0.5f, Skin.Info.Resolution.Y - 16.0f, width, &summary, true, nullptr);
        }

        if (Config.ViewDebug) {
            DrawFormatString(0, 0, GetColor(255, 255, 255), "\n\n\nNowTime:%lf\nBPM:%lf", ChartNowTime(1) / Chart.SongSpeed, Chart.NowBPM * Chart.SongSpeed);
        }
    }
    // 自分の打撃を他プレイヤーへ送る (判定とスコアも一緒に送り、受信側は再計算しない)
    void HitAction(HitType type) {
        if (!IsMulti || NowScene != Scene::Playing || Online.Room.Players.size() < 2) {
            return;
        }
        fasnet::HitEvent hit;
        hit.PlayerId = Online.MyId;
        hit.Lane = (int8_t)type;
        hit.Judge = (int8_t)Chart.Judge[0].HitJudge;
        hit.NoteType = Chart.Judge[0].NoteType;
        hit.Score = ToScore(Chart.Judge[0]);
        Online.Net.Send(fasnet::Msg::Hit, hit);
    }
    void ApplyRemoteHit(const fasnet::HitEvent& hit) {
        if (NowScene != Scene::Playing) {
            return;
        }
        auto it = std::ranges::find(Online.LaneIds, hit.PlayerId);
        if (it == Online.LaneIds.end() || it == Online.LaneIds.begin()) {
            return;
        }
        const size_t lane = (size_t)(it - Online.LaneIds.begin());
        if (lane >= Chart.Judge.size() || lane >= HitNote.size() || lane >= 4) {
            return;
        }
        const JudgeType judge = (JudgeType)hit.Judge;
        switch ((HitType)hit.Lane) {
        case HitType::DonLeft:
        case HitType::KaLeft:
        case HitType::DonRight:
        case HitType::KaRight:
            MiniTaikoFlash[hit.Lane + 4 * lane].Start();
            if (judge != JudgeType::None) {
                HitNote[lane].Add(HitNoteData(hit.NoteType, judge));
            }
            break;
        case HitType::DonBig:
        case HitType::KaBig:
            MiniTaikoFlash[(hit.Lane - 4) + 4 * lane].Start();
            MiniTaikoFlash[(hit.Lane - 2) + 4 * lane].Start();
            if (judge != JudgeType::None) {
                HitNote[lane].Add(HitNoteData(hit.Lane == (int)HitType::DonBig ? '3' : '4', judge));
            }
            break;
        default:
            break;
        }
        FromScore(Chart.Judge[lane], hit.Score);
    }
    // 次郎1 の分岐条件: 値 < 達人の条件 なら (値 < 玄人の条件 ? 普通 : 玄人)、そうでなければ達人
    //   精度 (p) と連打数 (r) は最後の #SECTION から、スコア (s) は曲の最初からの値で判定する
    // 分岐の条件から次の分岐を決める (値 < 達人の条件 なら (値 < 玄人の条件 ? 普通 : 玄人)、そうでなければ達人)
    BranchType BranchDecide(const BranchData& branchdata) const {
        const JudgeData& judge = Chart.Judge[0];
        double value = 0;
        switch (branchdata.Type) {
        case IfBranchType::Perfect: value = judge.Branch.Accuracy; break;
        case IfBranchType::Roll: value = (double)judge.Branch.Roll; break;
        case IfBranchType::Score: value = (double)judge.Score; break;
        default: return Chart.NowBranchFlag;
        }
        return value < branchdata.MasterBranch ? (value < branchdata.ExpertBranch ? BranchType::Normal : BranchType::Expert) : BranchType::Master;
    }
    static ABranchType BranchAnimationOf(BranchType from, BranchType to) {
        if (from == to) { return ABranchType::Null; }
        if (from == BranchType::Normal) { return to == BranchType::Expert ? ABranchType::Normal_Expert : ABranchType::Normal_Master; }
        if (from == BranchType::Expert) { return to == BranchType::Normal ? ABranchType::Expert_Normal : ABranchType::Expert_Master; }
        if (from == BranchType::Master) { return to == BranchType::Normal ? ABranchType::Master_Normal : ABranchType::Master_Expert; }
        return ABranchType::Null;
    }
    // 分岐を始める前の片付け: 今の分岐の、分岐の始まりより前の音符は残し、他の分岐の音符は表示しない
    void BranchCleanup(double startms) {
        for (auto& note : Chart.RawNoteDatas) {
            if (note.AbsTime >= startms) { continue; }
            if (note.IsBranch != Chart.NowBranchFlag && note.IsBranch != BranchType::Null) {
                note.HitFlag = true;
                note.IsBranch = BranchType::NotDisplay;
            }
            else if (note.IsBranch == Chart.NowBranchFlag) {
                note.IsBranch = BranchType::Null;
            }
        }
    }
    // 次郎形式の分岐 (taikojiro の Game_EvaluateBranch と同じタイミング):
    //   ・次の分岐 (始まりがまだ来ていない最初の分岐) を、判定の時刻の 200ms 前から判定の時刻を過ぎるまで毎フレーム判定し直す
    //     (判定の時刻 = 1 つ前の小節の頭。連打数の分岐は分岐の頭)
    //   ・音符の分岐は、分岐の始まりの 50ms 前に切り替える
    //   ・分岐の表示は、決まった分岐へ 1 段ずつ 100ms かけて動く (普通→達人は玄人を通って 200ms)
    void JiroBranchUpdate(double now) {
        auto& branches = Chart.BranchDatas;
        if (Chart.NowBranchFlag == BranchType::Null || branches.empty()) { return; }
        size_t index = 0;
        for (size_t k = 1; k < branches.size(); ++k) {
            if (branches[k - 1].StartMs <= now) { index = k; }
        }
        auto& br = branches[index];
        Chart.JiroUpcomingStart = br.Start ? br.StartMs : std::numeric_limits<double>::infinity();
        if (!Chart.LevelHold && !br.Evaluated && br.AbsTime - 200.0 <= now) {
            if (br.AbsTime < now) { br.Evaluated = true; }
            Chart.JiroNext = BranchDecide(br);
            // 分岐が決まって分岐のアニメーションが始まった時から、決まった分岐の音符を判定する
            // (分岐の始まりの直前に早めに叩いても、新しい分岐の最初の音符を判定できるように)
            // 前の区間も分岐していて、今の分岐の音符が分岐の始まりまでに残っている時は、それを判定し終えるまで切り替えない
            if (br.Start && Chart.NowBranchFlag != Chart.JiroNext) {
                const int lane = Chart.JiroLaneOf(Chart.NowBranchFlag);
                bool pending = false;
                for (size_t k = Chart.JiroCursor[lane]; k < Chart.JiroLanes[lane].size(); ++k) {
                    const auto& n = Chart.RawNoteDatas[Chart.JiroLanes[lane][k]];
                    if (n.AbsTime >= br.StartMs) { break; }
                    if (n.IsBranch != BranchType::Null && !n.HitFlag) { pending = true; break; }
                }
                if (!pending) { Chart.NowBranchFlag = Chart.JiroNext; }
            }
        }
        if (br.Start && br.StartMs - 50.0 < now) {
            BranchCleanup(br.StartMs);
            Chart.NowBranchFlag = Chart.JiroNext;
            br.Start = false;
            br.BranchFlag = false;
        }
        const int target = Chart.JiroLaneOf(Chart.JiroNext) + 1;
        if (Chart.JiroShown != target) {
            const auto wall = std::chrono::steady_clock::now();
            const int dir = target > Chart.JiroShown ? 1 : -1;
            if (!Chart.JiroAnimating) {
                Chart.JiroAnimating = true;
                Chart.JiroAnimTick = wall;
                Chart.NowBranchAnimation = BranchAnimationOf(Chart.JiroBranchOf(Chart.JiroShown - 1), Chart.JiroBranchOf(Chart.JiroShown - 1 + dir));
                Chart.BranchAnimationTime = 0.1;
                Chart.BranchAnimationTimer.Start();
            }
            else if (wall - Chart.JiroAnimTick > std::chrono::milliseconds(99)) {
                Chart.JiroAnimating = false;
                Chart.JiroShown += dir;
            }
        }
    }
    // 次郎形式の判定の進み方 (taikojiro の Game_PassNotes と同じ):
    //   ・判定する音符は分岐ごとに書かれた順に 1 つずつ。今の音符を過ぎた (不可の幅を越えた、または次の音符が可の幅に入った) ら
    //     見逃し (不可) にして次へ進む (1 フレームに 1 つ)。書かれた順と時刻の順が違う音符 (ダミーノーツ) はここで見逃しになる
    //   ・見逃した音符は消えずに流れていく。他の分岐の音符は時刻を過ぎたら判定せずに終わらせる
    void JiroPassNotes(double now) {
        const double judgenow = now - Config.JudgeOffset;
        const int lane = Chart.JiroLaneOf(Chart.NowBranchFlag);
        for (int other = 0; other < 3; ++other) {
            if (other == lane) { continue; }
            auto& list = Chart.JiroLanes[other];
            size_t& cur = Chart.JiroCursor[other];
            while (cur < list.size() && Chart.RawNoteDatas[list[cur]].AbsTime <= now) {
                auto& n = Chart.RawNoteDatas[list[cur]];
                if (n.IsBranch != BranchType::Null && n.NoteType >= '1' && n.NoteType <= '4') { n.HitFlag = true; }
                ++cur;
            }
        }
        auto& list = Chart.JiroLanes[lane];
        size_t& cur = Chart.JiroCursor[lane];
        if (cur >= list.size()) { return; }
        auto& n = Chart.RawNoteDatas[list[cur]];
        if (n.NoteType >= '5' && n.NoteType <= '9') {
            if (n.HitFlag || (n.RollEndTime != 0 && n.RollEndTime < now)) { ++cur; }     // 連打・風船は終わったら次へ
            return;
        }
        const bool last = cur + 1 >= list.size();
        const double next = last ? 0.0 : Chart.RawNoteDatas[list[cur + 1]].AbsTime;
        const bool passed = n.HitFlag || n.NoteType < '1' || n.NoteType > '4'
            || n.AbsTime + Config.JudgeBad < judgenow
            || (!last && n.AbsTime + Config.JudgeOk <= judgenow && next - Config.JudgeOk < judgenow);
        if (!passed || n.BigNoteTime != 0) { return; }
        if (!n.HitFlag && n.NoteType >= '1' && n.NoteType <= '4') {
            if (!Config.AutoPlay) {
                Chart.Judge[0].Hit(JudgeType::Bad, 0, '\0');
                HitAction(HitType::Empty);
            }
            n.HitFlag = true;       // 種類は残すので、見逃した音符はそのまま流れていく
        }
        ++cur;
    }
    // 次郎形式で今判定する音符 (打った音符は先に飛ばす)。無ければ nullptr
    NoteData* JiroCursorNote() {
        const int lane = Chart.JiroLaneOf(Chart.NowBranchFlag);
        auto& list = Chart.JiroLanes[lane];
        size_t& cur = Chart.JiroCursor[lane];
        while (cur < list.size() && Chart.RawNoteDatas[list[cur]].HitFlag && Chart.RawNoteDatas[list[cur]].NoteType == '\0') { ++cur; }
        return cur < list.size() ? &Chart.RawNoteDatas[list[cur]] : nullptr;
    }

    void BranchChange(const BranchData& branchdata) {
        const JudgeData& judge = Chart.Judge[0];
        double value = 0;
        switch (branchdata.Type) {
        case IfBranchType::Perfect: value = judge.Branch.Accuracy; break;
        case IfBranchType::Roll: value = (double)judge.Branch.Roll; break;
        case IfBranchType::Score: value = (double)judge.Score; break;
        default: return;
        }
        const BranchType next = value < branchdata.MasterBranch
            ? (value < branchdata.ExpertBranch ? BranchType::Normal : BranchType::Expert)
            : BranchType::Master;

        auto animation = [](BranchType from, BranchType to) {
            if (from == to) { return ABranchType::Null; }
            if (from == BranchType::Normal) { return to == BranchType::Expert ? ABranchType::Normal_Expert : ABranchType::Normal_Master; }
            if (from == BranchType::Expert) { return to == BranchType::Normal ? ABranchType::Expert_Normal : ABranchType::Expert_Master; }
            if (from == BranchType::Master) { return to == BranchType::Normal ? ABranchType::Master_Normal : ABranchType::Master_Expert; }
            return ABranchType::Null;
        };
        Chart.NowBranchAnimation = animation(Chart.NowBranchFlag, next);
        Chart.NowBranchFlag = next;
    }
    void PlayingProc() {

        // オンライン: サーバーが決めた時刻 (往復遅延を補正済み) になったら全員同時に開始する
        if (IsMulti && Online.StartPending && std::chrono::steady_clock::now() >= Online.StartAt) {
            Online.StartPending = false;
            Chart.NowTime.Start();
#ifndef __ANDROID__
            LastHitTick = RawKeyboard::Now();
#endif
        }

        const double NowTime = ChartNowTime(1000) + Training.Offset;
        Chart.UpdateScanStart(NowTime, Config.JudgeBad);
#ifndef __ANDROID__
        const int64_t NowTick = RawKeyboard::Now();
        bool SkipKeyEvents = false;
#endif

        if (Chart.NowTime.IsRunning()) {
            if (Chart.SongBlankTime < NowTime && Chart.SongBlankTime + 128 > NowTime && !Chart.SongData.IsPlay()) {
                if (Training.BarlineIndex < Chart.AllBarlineCount) {
                    // 開始タイミングを検出したフレームまでの遅れ (最大 1 フレーム程度) だけ先から再生し、譜面と音を揃える
                    Chart.SongData.SetCurrent(NowTime - Chart.SongBlankTime);
                    Chart.SongData.Play(FALSE);
#ifndef __ANDROID__
                    RecorderNoteSong(NowTime - Chart.SongBlankTime);
#endif
                }
            }
            else if (Chart.SongBlankTime + 5000 < NowTime && !Chart.SongData.IsPlay()) {
                if (Dan.Active) {
                    // 曲の終わり: この曲の個別条件 (最終曲なら全体条件も) を確定し、次の曲かリザルトへ
                    UpdateDanExams(true);
                    if (!Dan.Failed && Dan.SongIndex + 1 < Dan.Course.DanSongs.size()) {
                        ++Dan.SongIndex;
                        NowScene = Scene::Loading;
                    }
                    else {
                        NowScene = Scene::Result;
                    }
                    return;
                }
                if (!IsMulti && Config.TrainingMode) {
                    RecorderAbort();
                    NowScene = Scene::Loading;
                    return;
                }
                if (!IsMulti) {
                    NowScene = Scene::Result;
                    return;
                }
                // オンライン: 自分の結果を送り、全員が終わるまで待つ (OnRoomChanged でリザルトへ)
                if (!Online.FinishSent) {
                    Online.FinishSent = true;
                    Online.Net.Send(fasnet::Msg::Finish, ToScore(Chart.Judge[0]));
                }
            }
        }

        if (Config.JiroChartMode) {
            JiroBranchUpdate(NowTime);
        }
        else if (Chart.NowBranchFlag != BranchType::Null && !Chart.LevelHold) {
            for (size_t i = 0, size = Chart.BranchDatas.size(); i < size; ++i) {
                if (Chart.BranchDatas[i].AbsTime < NowTime && Chart.BranchDatas[i].Start) {
                    for (size_t j = 0, nsize = Chart.RawNoteDatas.size(); j < nsize; ++j) {
                        if (Chart.RawNoteDatas[j].AbsTime >= Chart.BranchDatas[i].StartMs) { continue; }
                        if (Chart.RawNoteDatas[j].IsBranch != Chart.NowBranchFlag && Chart.RawNoteDatas[j].IsBranch != BranchType::Null) {
                            Chart.RawNoteDatas[j].HitFlag = true;
                            Chart.RawNoteDatas[j].IsBranch = BranchType::NotDisplay;
                        }
                        else if (Chart.RawNoteDatas[j].IsBranch == Chart.NowBranchFlag) {
                            Chart.RawNoteDatas[j].IsBranch = BranchType::Null;
                        }
                    }
                    BranchChange(Chart.BranchDatas[i]);
                    Chart.BranchDatas[i].BranchFlag = false;
                    Chart.BranchDatas[i].Start = false;
                    Chart.BranchAnimationTimer.Start();
                }
            }
        }

        if (IsMulti) {
            // 送信する判定情報は打撃ごとに作るので、前フレームの値を残さない
            Chart.Judge[0].NoteType = '\0';
            Chart.Judge[0].HitJudge = JudgeType::None;
        }
        else {
            if (Config.TrainingMode && !Chart.IsDanMode()) {
                if (!Chart.NowTime.IsRunning()) {
                    if (!Training.BarlineMoveTimer.IsRunning()) {

                        auto MoveInputProc = [&](bool direction) {
                            if (direction) {
                                if (Training.BarlineIndex < Chart.AllBarlineCount) {
                                    auto find = std::ranges::find_if(std::next(
                                        Chart.RawNoteDatas.begin() + Training.NoteDataIndex),
                                        Chart.RawNoteDatas.end(),
                                        &NoteData::BarlineDisplay);
                                    if (find != Chart.RawNoteDatas.end()) {
                                        Training.NoteDataIndex = std::distance(
                                            Chart.RawNoteDatas.begin(), find);
                                        ++Training.BarlineIndex;
                                        Training.MemNowTime = NowTime;
                                        Training.BarlineMoveTimer.Start();
                                    }
                                }
                            }
                            else {
                                if (Training.BarlineIndex > 0) {
                                    auto find = std::ranges::find_if(std::reverse_iterator(
                                        Chart.RawNoteDatas.begin() + Training.NoteDataIndex),
                                        Chart.RawNoteDatas.rend(),
                                        &NoteData::BarlineDisplay);
                                    if (find != Chart.RawNoteDatas.rend()) {
                                        Training.NoteDataIndex = std::distance(
                                            Chart.RawNoteDatas.begin(), std::prev(find.base()));
                                        --Training.BarlineIndex;
                                        Training.MemNowTime = NowTime;
                                        Training.BarlineMoveTimer.Start();
                                    }
                                }
                            }
                            };

                        auto WarpInputProc = [&](bool direction) {
                            if (direction) {
                                auto find = std::ranges::find_if(Chart.RawNoteDatas.begin(),
                                    Chart.RawNoteDatas.end(),
                                    &NoteData::BarlineDisplay);
                                if (find != Chart.RawNoteDatas.end()) {
                                    Training.NoteDataIndex = std::distance(
                                        Chart.RawNoteDatas.begin(), find);
                                    Training.BarlineIndex = 0;
                                    Training.MemNowTime = NowTime;
                                    Training.BarlineMoveTimer.Start();
                                }
                            }
                            else {
                                auto find = std::ranges::find_if(Chart.RawNoteDatas.rbegin(),
                                    Chart.RawNoteDatas.rend(),
                                    &NoteData::BarlineDisplay);
                                if (find != Chart.RawNoteDatas.rend()) {
                                    Training.NoteDataIndex = std::distance(
                                        Chart.RawNoteDatas.begin(), std::prev(find.base()));
                                    Training.BarlineIndex = Chart.AllBarlineCount;
                                    Training.MemNowTime = NowTime;
                                    Training.BarlineMoveTimer.Start();

                                }
                            }
                            };

                        auto StartInputProc = [&] {
#ifndef __ANDROID__
                            // 再開に使った打鍵を演奏の打撃として二重に扱わない
                            SkipKeyEvents = true;
                            LastHitTick = NowTick;
#endif
                            Chart.NowTime.Start();
                            if (Chart.SongBlankTime < NowTime
                                && Training.BarlineIndex > 0
                                && Training.BarlineIndex < Chart.AllBarlineCount) {
                                Chart.SongData.SetCurrent(NowTime - Chart.SongBlankTime);
                                Chart.SongData.Play(FALSE);
                            }
                            };

                        auto BranchChangeProc = [&](bool direction) {
                            if (!Chart.IsBranchChart()) { return; }
                            if (direction) {
                                Chart.NowBranchFlag = (Chart.NowBranchFlag < BranchType::Master)
                                    ? (BranchType)((int)Chart.NowBranchFlag + 1)
                                    : BranchType::Master;
                            }
                            else {
                                Chart.NowBranchFlag = (Chart.NowBranchFlag > BranchType::Normal)
                                    ? (BranchType)((int)Chart.NowBranchFlag - 1)
                                    : BranchType::Normal;
                            }
                            Training.MemBranchFlag = Chart.NowBranchFlag;
                            };

#ifndef __ANDROID__
                        Input.HitKeyProcess(VK_NEXT, KeyState::Down, [&] { MoveInputProc(false); }, Config.KeyHoldProcInterval);
                        Input.HitKeyProcess(VK_PRIOR, KeyState::Down, [&] { MoveInputProc(true); }, Config.KeyHoldProcInterval);
                        Input.HitKeyProcess(VK_DOWN, KeyState::Down, [&] { BranchChangeProc(false); });
                        Input.HitKeyProcess(VK_UP, KeyState::Down, [&] { BranchChangeProc(true); });
                        Input.HitKeyProcess(VK_END, KeyState::Down, [&] { WarpInputProc(false); });
                        Input.HitKeyProcess(VK_HOME, KeyState::Down, [&] { WarpInputProc(true); });

                        Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, StartInputProc);
                        Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, StartInputProc);
                        Input.HitKeyProcess(VK_SPACE, KeyState::Down, StartInputProc);
                        Input.HitKeyProcess(VK_RETURN, KeyState::Down, StartInputProc);
#else
                        Touch.Process(TouchType::RightKa, [&] { MoveInputProc(false); });
                        Touch.Process(TouchType::LeftKa, [&] { MoveInputProc(true); });
                        Touch.Process(TouchType::Don, StartInputProc);
#endif
                    }

                    else {
                        const double MoveTime = Training.BarlineMoveTimer.GetElapsed().Second();
                        double Rate = GetEasingRate(MoveTime / Training.BarlineMoveTime,
                            ease::Base::In, ease::Line::Linear);
                        Training.Offset = std::lerp(Training.MemNowTime,
                            Chart.RawNoteDatas[Training.NoteDataIndex].AbsTime,
                            Rate);
                        if (Rate >= 1.0) {
                            Training.BarlineMoveTimer.Reset();
                        }
                    }
                }
            }

            if (Dan.Active) {
                UpdateDanExams(false);
                // 次の曲を演奏中に非同期で先読みし、曲間の待ち時間を減らす
                const size_t next = Dan.SongIndex + 1;
                if (Chart.NowTime.IsRunning() && !Dan.Failed && next < Dan.Course.DanSongs.size() && Dan.NextSongIndex != next) {
                    Dan.NextSongIndex = next;
                    SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMNOPRESS);
                    Dan.NextSong.LoadAsync(Dan.Course.DanSongs[next].WavePath.string());
                }
            }

            auto BackInputProc = [&] {
                RecorderAbort();        // 選曲画面への途中退出: 録画は保存しない
                Training.Init();
                Dan.Reset();
				Chart.Init(true);
                NowScene = PrevScene;
                };
            auto ReLoadInputProc = [&] {
                if (Chart.IsDanMode()) { return; }
                RecorderAbort();        // 譜面のリロード: 録画は保存しない
                NowScene = Scene::Loading;
                };

#ifndef __ANDROID__
            Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
            Input.HitKeyProcess(VK_TAB, KeyState::Down, ReLoadInputProc);
            // 譜面制作のテストプレイ: F1 でオートプレイと通常のプレイをその場で切り替える
            if (Editor.Testing) {
                Input.HitKeyProcess(VK_F1, KeyState::Down, [&] {
                    Config.AutoPlay = !Config.AutoPlay;
                    PlayHitSe(1);
                    });
            }
#else
            Touch.Process(TouchType::Other, BackInputProc, Skin.Base->Playing.Image.Back);
            Touch.Process(TouchType::Other, ReLoadInputProc, Skin.Base->Playing.Image.Pause);
#endif
        }

        auto JudgeNote = [&](double nowtime, char type) {

            auto& Judge = Chart.Judge[0];
            size_t rollcount = 0;
            size_t ballooncount = 0;
            NoteData* balloondata = nullptr;

            bool NextImage = false;

            for (auto&& data : Chart.ActiveNotes()) {

                bool NoteBranch = data.IsBranch == Chart.NowBranchFlag || data.IsBranch == BranchType::Null;

                if (!NoteBranch) {
                    continue;
                }
                if (data.HitFlag) {
                    continue;
                }
                // 次郎形式: ドン・カッは、書かれた順で今判定する音符だけを叩ける
                if (Config.JiroChartMode && data.NoteType >= '1' && data.NoteType <= '4' && data.BigNoteTime == 0 && &data != JiroCursorNote()) {
                    continue;
                }

                if (data.RollFlag == 1) {
                    ++rollcount;
                    NextImage = data.NoteType == '6';
                }

                if (data.BalloonFlag == 1) {
                    balloondata = &data;
                }

                if (data.BigNoteTime != 0) {
                    if (Config.JudgeGood < nowtime - data.BigNoteTime) {
                        data.NoteType -= 2;
                    }
                    nowtime = data.BigNoteTime;
                }

                const double _HitError = (data.AbsTime - nowtime) + Config.JudgeOffset;
                const bool GoodHit =
                    _HitError > -Config.JudgeGood && _HitError < Config.JudgeGood;
                const bool OkHit =
                    _HitError > -Config.JudgeOk && _HitError < Config.JudgeOk;
                const bool BadHit =
                    _HitError > -Config.JudgeBad && _HitError < Config.JudgeBad;
                bool TypeMatch = type == data.NoteType;

                switch (data.NoteType) {
                case '3':
                case '4':
                    TypeMatch = type == data.NoteType - 2;
                    break;
                }

                if (!(BadHit && TypeMatch)) { continue; }

                switch (data.NoteType) {
                case '3':
                case '4':
                    if (data.BigNoteTime == 0) {
                        data.BigNoteTime = nowtime;
                        return;
                    }
                }

                if (GoodHit) {
                    HitNote[0].Add(HitNoteData(data.NoteType, JudgeType::Good));
                    Judge.Hit(JudgeType::Good, Chart.AddScore, type);
                }
                else if (OkHit) {
                    HitNote[0].Add(HitNoteData(data.NoteType, JudgeType::Ok));
                    Judge.Hit(JudgeType::Ok, Chart.AddScore, type);
                }
                else if (BadHit) {
                    HitNote[0].Add(HitNoteData('\0', JudgeType::Bad));
                    Judge.Hit(JudgeType::Bad, 0, type);
                }
                RecordHitError(-_HitError);     // _HitError は「早いほど正」なので符号を逆にして「遅いほど正」にする

                data.HitFlag = true;
                data.NoteType = '\0';

                return;
            }

            if (rollcount > 0) {
                HitNote[0].Add(HitNoteData(NextImage ? '6' : '5', JudgeType::Roll));
                Judge.Hit(JudgeType::Roll, 100, NextImage ? '6' : '5');
                if (Chart.RollViewEndTimer.IsRunning() && Chart.Judge[0].Rolls.IsEnd) {
                    Chart.Judge[0].Rolls.IsEnd = false;
                    Chart.Judge[0].Rolls.NowCount = 0;
                    Chart.RollViewEndTimer.Reset();
                }
                Chart.Judge[0].Rolls.NowCount++;
            }

            if (type == '1' && balloondata != nullptr) {
                --balloondata->BalloonCount;
                Chart.Judge[0].Rolls.NowCount = balloondata->BalloonCount;
                Chart.RollViewEndTimer.Reset();
                Judge.Hit(JudgeType::Roll, 100, '\0');
                if (balloondata->BalloonCount <= 0) {
                    PlayHitSe(2);
                    HitNote[0].Add(HitNoteData('3', JudgeType::Roll));
                    balloondata->NoteType = '0';
                    balloondata->HitFlag = true;
                    balloondata->BalloonFlag = 2;
                    Judge.NoteType = '3';
                }
            }
            };

        if (Chart.RollViewEndTimer.GetElapsed().Second() > Chart.RollViewEndTime) {
            for (auto&& judge : Chart.Judge) {
                judge.Rolls.NowCount = 0;
                judge.Rolls.IsEnd = false;
            }
            Chart.RollViewEndTimer.Reset();
        }

        // lane は HitType の DonLeft/KaLeft/DonRight/KaRight (= MiniTaikoFlash の添字)
        auto HitLane = [&](int lane, double hittime) {
            const bool don = (lane % 2) == 0;
            Chart.Judge[0].HitJudge = JudgeType::None;
            Chart.Judge[0].NoteType = '\0';
            (don ? Skin.Base->Playing.SE.Don : Skin.Base->Playing.SE.Ka).Play();
            MiniTaikoFlash[lane].Start();
            JudgeNote(hittime, don ? '1' : '2');
            HitAction((HitType)lane);
            };

#ifndef __ANDROID__
        // 見逃し判定より先に、押した瞬間の時刻で判定する (描画待ちで遅れた分だけ判定が遅れない)
        if (RawKeyboard::GetInstance().IsActive() && Chart.NowTime.IsRunning() && !Config.AutoPlay && !SkipKeyEvents) {
            const std::vector<int>* lanes[4] = { &Config.DonInputLeft, &Config.KaInputLeft, &Config.DonInputRight, &Config.KaInputRight };
            for (const auto& e : KeyEvents) {
                if (!e.Down) { continue; }
                // 前フレーム以前の打鍵 (ロード中など) は前フレーム時刻に丸める
                const int64_t tick = std::max(e.Time, LastHitTick);
                const double hittime = NowTime - RawKeyboard::TicksToMs(NowTick - tick) * Chart.SongSpeed;
                for (int lane = 0; lane < 4; ++lane) {
                    if (std::ranges::find(*lanes[lane], (int)e.VKey) != lanes[lane]->end()) {
                        HitLane(lane, hittime);
                    }
                }
            }
        }
        LastHitTick = NowTick;
#endif

        if (Config.JiroChartMode && Chart.NowTime.IsRunning()) {
            JiroPassNotes(NowTime);
        }

        for (auto&& data : Chart.ActiveNotes()) {

            bool NoteBranch = data.IsBranch == Chart.NowBranchFlag || data.IsBranch == BranchType::Null;
            bool HitFlag = data.AbsTime < NowTime;

            if (data.Section && HitFlag) {
                data.Section = false;
                Chart.Judge[0].Branch.Init();
            }

            if (!NoteBranch) { continue; }
            if (data.LevelHold && HitFlag) {
                Chart.LevelHold = true;
            }
            if (data.GoGoStart && HitFlag) {
                Chart.NowGoGo = true;
            }
            if (data.GoGoEnd && HitFlag) {
                Chart.NowGoGo = false;
            }

            if (data.AbsTime - Config.JudgeBad > NowTime) { continue; }
            if (data.HitFlag) { continue; }

            const double _HitError = (data.AbsTime - NowTime) + Config.JudgeOffset;
            const bool BadHit = _HitError > -Config.JudgeBad && _HitError < Config.JudgeBad;

            if (data.BigNoteTime != 0 && Config.JudgeGood < NowTime - data.BigNoteTime) {
                JudgeNote(NowTime, data.NoteType - 2);
            }

            if (!data.HitFlag &&
                !Config.JiroChartMode &&       // 次郎形式の見逃しは JiroPassNotes で書かれた順に判定する
                (data.NoteType >= '1' && data.NoteType <= '4') &&
                data.BigNoteTime == 0 &&
                _HitError < -Config.JudgeBad) {
                Chart.Judge[0].Hit(JudgeType::Bad, 0, '\0');
                HitAction(HitType::Empty);
                data.HitFlag = true;
            }

            if ((data.NoteType >= '5' && data.NoteType <= '6') &&
                HitFlag) {
                data.RollFlag = 1;
                if (data.RollEndTime < NowTime) {
                    data.RollFlag = 2;
                    data.HitFlag = true;
                    Chart.Judge[0].Rolls.IsEnd = true;
                    Chart.RollViewEndTimer.Start();
                }
            }

            if ((data.NoteType == '7' || data.NoteType == '9') &&
                HitFlag) {
                data.BalloonFlag = 1;
                if (data.RollEndTime < NowTime) {
                    data.BalloonFlag = 2;
                    data.HitFlag = true;
                    Chart.Judge[0].Rolls.NowCount = 0;
                }
            }
        }

        if (Chart.NowTime.IsRunning()) {
            if (Config.AutoPlay) {

                size_t RollCount = 0;
                NoteData* BalloonData = nullptr;
                bool NextImage = false;

                for (auto&& data : Chart.ActiveNotes()) {

                    bool NoteBranch = data.IsBranch == Chart.NowBranchFlag || data.IsBranch == BranchType::Null;
                    bool HitFlag = data.AbsTime < NowTime;
                    bool IsHitNote = (data.NoteType >= '1' && data.NoteType <= '4');

                    if (data.Section && HitFlag) {
                        data.Section = false;
                        Chart.Judge[0].Branch.Init();
                    }

                    if (!NoteBranch) {
                        continue;
                    }

                    if (data.RollFlag == 1) {
                        ++RollCount;
                        NextImage = data.NoteType == '6';
                    }

                    if (data.BalloonFlag == 1) {
                        BalloonData = &data;
                    }

                    if (HitFlag && !data.HitFlag && IsHitNote) {
                        HitNote[0].Add(HitNoteData(data.NoteType, JudgeType::Good));
                        Chart.Judge[0].Hit(JudgeType::Good, 0, data.NoteType);
                        switch (data.NoteType) {
                        case '1':
                            PlayHitSe(0);
                            MiniTaikoFlash[0 + Chart.AutoPlayLR * 2].Start();
                            Chart.AutoPlayLR = !Chart.AutoPlayLR;
                            HitAction((HitType)(0 + Chart.AutoPlayLR * 2));
                            break;
                        case '2':
                            PlayHitSe(1);
                            MiniTaikoFlash[1 + Chart.AutoPlayLR * 2].Start();
                            Chart.AutoPlayLR = !Chart.AutoPlayLR;
                            HitAction((HitType)(1 + Chart.AutoPlayLR * 2));
                            break;
                        case '3':
                            PlayHitSe(0);
                            PlayHitSe(0);
                            MiniTaikoFlash[0].Start();
                            MiniTaikoFlash[2].Start();
                            HitAction(HitType::DonBig);
                            break;
                        case '4':
                            PlayHitSe(1);
                            PlayHitSe(1);
                            MiniTaikoFlash[1].Start();
                            MiniTaikoFlash[3].Start();
                            HitAction(HitType::KaBig);
                            break;
                        }
                        data.NoteType = '\0';
                        data.HitFlag = true;
                    }
                }

                if (RollCount > 0 && !Chart.WaitRollTime.IsRunning()) {
                    PlayHitSe(0);
                    if (Chart.RollViewEndTimer.IsRunning() && Chart.Judge[0].Rolls.IsEnd) {
                        for (auto&& judge : Chart.Judge) {
                            judge.Rolls.NowCount = 0;
                            judge.Rolls.IsEnd = false;
                        }
                        Chart.RollViewEndTimer.Reset();
                    }
                    Chart.AutoPlayLR = !Chart.AutoPlayLR;
                    Chart.Judge[0].Roll++;
                    Chart.Judge[0].Branch.Roll++;
                    Chart.Judge[0].Rolls.NowCount++;
                    HitNote[0].Add(HitNoteData(NextImage ? '6' : '5', JudgeType::Roll));
                    Chart.WaitRollTime.Start();
                    HitAction((HitType)(0 + Chart.AutoPlayLR * 2));
                    Chart.Judge[0].NoteType = NextImage ? '6' : '5';
                }
                if (BalloonData != nullptr && !Chart.WaitRollTime.IsRunning()) {
                    PlayHitSe(0);
                    Chart.AutoPlayLR = !Chart.AutoPlayLR;
                    Chart.Judge[0].Roll++;
                    Chart.Judge[0].Branch.Roll++;
                    --BalloonData->BalloonCount;
                    Chart.Judge[0].Rolls.NowCount = BalloonData->BalloonCount;
                    Chart.RollViewEndTimer.Reset();
                    Chart.WaitRollTime.Start();
                    HitAction((HitType)(0 + Chart.AutoPlayLR * 2));
                    if (BalloonData->BalloonCount <= 0) {
                        PlayHitSe(2);
                        HitNote[0].Add(HitNoteData('3', JudgeType::Roll));
                        BalloonData->NoteType = '0';
                        BalloonData->HitFlag = true;
                        BalloonData->BalloonFlag = 2;
                        Chart.Judge[0].NoteType = '3';
                    }
                }
                if (Chart.WaitRollTime.GetElapsed().Second() > 1.0 / Config.RollSpeed) {
                    Chart.WaitRollTime.Reset();
                }
            }
            else {
#ifndef __ANDROID__
                // Raw Input 有効時は判定済みなので、メニュー用のキー状態だけ更新する
                // Raw Input のイベントが一度も届いていない環境 (他のウィンドウに奪われている等) では、ウィンドウのキー入力で判定する
                const bool raw = RawKeyboard::GetInstance().IsActive() && RawEventsSeen;
                auto LegacyHit = [&](int lane) {
                    if (raw) { return; }
                    LegacyHitTick = RawKeyboard::Now();
                    HitLane(lane, NowTime);
                };
                Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, [&] { LegacyHit((int)HitType::DonLeft); });
                Input.HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { LegacyHit((int)HitType::KaLeft); });
                Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, [&] { LegacyHit((int)HitType::DonRight); });
                Input.HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { LegacyHit((int)HitType::KaRight); });
#else
                Touch.Process(TouchType::LeftDon, [&] { HitLane((int)HitType::DonLeft, NowTime); });
                Touch.Process(TouchType::LeftKa, [&] { HitLane((int)HitType::KaLeft, NowTime); });
                Touch.Process(TouchType::RightDon, [&] { HitLane((int)HitType::DonRight, NowTime); });
                Touch.Process(TouchType::RightKa, [&] { HitLane((int)HitType::KaRight, NowTime); });
#endif
            }
        }
    }

    int ResultIndex = 0;
    int CrownIndex = 0;

    json ScoreCache;
    bool IsScoreCacheLoaded = false;

    const json& GetScoreCache() {
        if (!IsScoreCacheLoaded) {
            IsScoreCacheLoaded = true;
            ScoreCache = json::object();
            std::ifstream ifs(GetExecutablePath() / "scoredata.json", std::ios::binary);
            if (ifs.is_open()) {
                std::vector<uint8_t> bson_vec((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                json data = json::from_bson(bson_vec, true, false);
                if (data.is_object()) {
                    ScoreCache = std::move(data);
                }
            }
        }
        return ScoreCache;
    }
    void ScoreDataSave(uint64_t score, int crown) {
        if (Editor.Testing) { return; }     // 譜面制作のテストプレイは記録しない

        std::string ChartPath = Chart.OriginalData.ChartPath.string();
        std::string CourseName = magic_enum::enum_name((CourseType)CourseIndex).data();

        GetScoreCache();
        ScoreCache[ChartPath][CourseName]["Score"] = score;
        ScoreCache[ChartPath][CourseName]["Crown"] = crown;

        std::vector<uint8_t> v_bson = json::to_bson(ScoreCache);
        std::ofstream ofs(GetExecutablePath() / "scoredata.json", std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(v_bson.data()), v_bson.size());
    }
    ScoreData ScoreDataLoad(const std::string& chartpath, int courseindex) {

        ScoreData Score;
        std::string CourseName = magic_enum::enum_name((CourseType)courseindex).data();
        const json& data = GetScoreCache();

        auto chart = data.find(chartpath);
        if (chart == data.end() || !chart->is_object()) {
            return Score;
        }
        auto course = chart->find(CourseName);
        if (course == chart->end() || !course->is_object()) {
            return Score;
        }
        Score.Score = course->value("Score", Score.Score);
        Score.Crown = course->value("Crown", Score.Crown);
        return Score;
    }
    void ResultEnd() {

        if (IsMulti) {
            ResultIndex = 0;
            Online.FinishSent = false;
            Online.LaneIds.clear();
            Chart.Init(true);
            return;
        }
        else if (const ScoreData saved = ScoreDataLoad(Chart.OriginalData.ChartPath.string(), CourseIndex);
            Chart.Judge[0].Score > saved.Score || CrownIndex > saved.Crown) {
            // スコアと王冠はそれぞれ最高記録を残す (王冠だけ更新した場合も保存する)
            ScoreDataSave(std::max(Chart.Judge[0].Score, saved.Score), std::max(CrownIndex, saved.Crown));
        }

        CrownIndex = 0;
        Dan.Reset();
        Chart.Init(true);
    }
    void ResultDraw() {

        SetDrawBlendMode(0, 0);
        Skin.Base->Result.Image.BackGround.Draw({});
        Skin.Base->Result.Image.JudgeScore.Draw({});
        Skin.Base->Result.Image.Accuracy.Draw({});
        Skin.Base->Result.Image.Score.Draw({});

        auto ScoreDraw = [&](uint64_t num) {
            int digit = exmath::digit(num);
            float offset = 0;
            int i = 0;
            do {
                Skin.Base->Result.Image.Number.Draw({ offset + Skin.Base->Result.Config.ScorePos.X, Skin.Base->Result.Config.ScorePos.Y }, num % 10);
                num /= 10;
                ++i;
                offset -= Skin.Base->Result.Image.Number.Size.Width;
            } while (i < digit);
            };
        auto AccuracyDraw = [&](double Rate) {
            int iRate = Rate * 100;
            int digit = exmath::digit(iRate) + 2;
            if (iRate == 0) {
                digit = 5;
            }

            float offset = 0;
            Skin.Base->Result.Image.Number.Draw({ offset + Skin.Base->Result.Config.AccracyPos.X, Skin.Base->Result.Config.AccracyPos.Y }, 11);
            offset -= Skin.Base->Result.Image.Number.Size.Width;
            --digit;

            int i = 0;
            do {
                if (i == 2) {
                    Skin.Base->Result.Image.Number.Draw({ offset + Skin.Base->Result.Config.AccracyPos.X, Skin.Base->Result.Config.AccracyPos.Y }, 12);
                }
                else {
                    Skin.Base->Result.Image.Number.Draw({ offset + Skin.Base->Result.Config.AccracyPos.X, Skin.Base->Result.Config.AccracyPos.Y }, iRate % 10);
                    iRate /= 10;
                }
                ++i;
                offset -= Skin.Base->Result.Image.Number.Size.Width;
            } while (i < digit);
            };
        auto JudgeDraw = [&](const Pos2D<float>& Pos, uint64_t num) {
            int digit = exmath::digit(num) + 1;
            float offset = Skin.Base->Result.Image.Number.Size.Width * digit;
            Skin.Base->Result.Image.Number.Draw({ offset + Pos.X, Pos.Y }, 10);
            offset -= Skin.Base->Result.Image.Number.Size.Width;
            --digit;
            size_t i = 0;
            do {
                Skin.Base->Result.Image.Number.Draw({ offset + Pos.X, Pos.Y }, num % 10);
                num /= 10;
                ++i;
                offset -= Skin.Base->Result.Image.Number.Size.Width;
            } while (i < digit);
            };

        JudgeData Judge = Chart.Judge[ResultIndex];

        ScoreDraw(Judge.Score);
        AccuracyDraw(Judge.Accuracy);
        JudgeDraw(Skin.Base->Result.Config.GoodPos, Judge.Good);
        JudgeDraw(Skin.Base->Result.Config.OkPos, Judge.Ok);
        JudgeDraw(Skin.Base->Result.Config.BadPos, Judge.Bad);
        JudgeDraw(Skin.Base->Result.Config.RollPos, Judge.Roll);
        JudgeDraw(Skin.Base->Result.Config.MaxComboPos, Judge.MaxCombo);

        CrownIndex = 0;
        if (!Chart.IsDanMode()) {
            if (Judge.Accuracy >= 75) {
                CrownIndex = 1;
            }
            if (Judge.Accuracy >= 90) {
                CrownIndex = 2;
            }
            if (Judge.Accuracy >= 90 && Judge.Bad == 0) {
                CrownIndex = 3;
            }
            if (Judge.Accuracy >= 90 && Judge.Bad == 0 && Judge.Ok == 0) {
                CrownIndex = 4;
            }
        }
        else {
            CrownIndex = DanCrown();

            // 各条件の結果を一覧表示する
            const int lineheight = GetFontSize() + 6;
            const int y0 = Skin.Info.Resolution.Y - 24 - lineheight * (int)Dan.Exams.size();
            for (size_t i = 0; i < Dan.Exams.size(); ++i) {
                const auto& e = Dan.Exams[i];
                const char* status = "";
                unsigned int color = GetColor(255, 255, 255);
                switch (e.Status) {
                case ExamStatus::Gold: status = "金合格"; color = GetColor(255, 215, 0); break;
                case ExamStatus::Red: status = "合格"; color = GetColor(255, 110, 110); break;
                default: status = "不合格"; color = GetColor(160, 160, 160); break;
                }
                const std::string prefix = e.Song >= 0 ? std::to_string(e.Song + 1) + "曲目 " : "全体 ";
                const std::string line = prefix + ExamConditionText(e.Exam) + "  結果: " + ExamValueText(e.Exam.ExamType, e.Value) + "  " + status;
                DrawString(24, y0 + lineheight * (int)i, line.c_str(), color);
            }
        }

        Skin.Base->Result.Image.Crown.Draw({}, CrownIndex);
        Skin.Base->Result.Font.Title.Draw(
            Skin.Base->Result.Config.TitlePos,
            GetColor(255, 255, 255),
            GetColor(0, 0, 0),
            Chart.OriginalData.TitleStrlen.Result,
            Chart.OriginalData.Title
        );
        Skin.Base->Result.Font.SubTitle.Draw(
            Skin.Base->Result.Config.SubTitlePos,
            GetColor(255, 255, 255),
            GetColor(0, 0, 0),
            Chart.OriginalData.SubtitleStrlen.Result,
            Chart.OriginalData.Subtitle
        );
        if (const std::string& title = LaneTitle(IsMulti ? (size_t)ResultIndex : 0); !title.empty()) {
            Skin.Base->Result.Font.TitleName.Draw(
                Skin.Base->Result.Config.TitleNamePos,
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                GetStrlenCached(title, Skin.Base->Result.Font.TitleName.Handle),
                title
            );
        }
        if (!IsMulti) {
            Skin.Base->Result.Font.PlayerName.Draw(
                Skin.Base->Result.Config.PlayerNamePos,
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                GetStrlenCached(Config.PlayerName, Skin.Base->Result.Font.PlayerName.Handle),
                Config.PlayerName
            );
        }
        else {
            Skin.Base->Result.Font.PlayerName.Draw(
                Skin.Base->Result.Config.PlayerNamePos,
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                GetStrlenCached(Names[ResultIndex], Skin.Base->Result.Font.PlayerName.Handle),
                Names[ResultIndex]
            );
        }

        if (Config.ScoreMeter && (!IsMulti || ResultIndex == 0) && HitStats.LastPlay.Count > 0) {
            auto& font = ConfigSmallFont();
            const float resx = (float)Skin.Info.Resolution.X, resy = (float)Skin.Info.Resolution.Y;
            DrawHitErrorMeter(resx * 0.5f, resy - font.Size * 2.0f - 30.0f, 420.0f, &HitStats.LastPlay, false, &font);
        }

#ifdef __ANDROID__
        if (!IsMulti || IsHost()) {
            Skin.Base->Result.Image.Back.Draw({}); 
        }
#endif
    }
    void ResultProc() {

        auto BackInputProc = [&] {
            if (!IsMulti) {
                NowScene = PrevScene;
            }
            else if (IsHost()) {
                // 全員を待機室へ戻す (RoomState を受け取ると各自が待機室へ移動する)
                Online.Net.Send(fasnet::Msg::ReturnLobby);
            }
            };

#ifndef __ANDROID__
        Input.HitKeyProcess(VK_RETURN, KeyState::Down, BackInputProc);
#else
        Touch.Process(TouchType::Other, BackInputProc, Skin.Base->Result.Image.Back);
#endif

        if (IsMulti && LaneCount() >= 2) {

#ifndef __ANDROID__
            Input.HitKeyProcess(VK_LEFT, KeyState::Down, [&] {
                Skin.Base->SongSelect.SE.Ka.Play();
                ResultIndex = std::max(ResultIndex - 1, 0);
                }, Config.KeyHoldProcInterval);
            Input.HitKeyProcess(VK_RIGHT, KeyState::Down, [&] {
                Skin.Base->SongSelect.SE.Ka.Play();
                ResultIndex = std::clamp(ResultIndex + 1, 0, std::max((int)LaneCount() - 1, 0));
                }, Config.KeyHoldProcInterval);
#else
            Touch.Process(TouchType::LeftKa, [&] {
                Skin.Base->SongSelect.SE.Ka.Play();
                ResultIndex = std::max(ResultIndex - 1, 0);
                });
            Touch.Process(TouchType::RightKa, [&] {
                Skin.Base->SongSelect.SE.Ka.Play();
                ResultIndex = std::clamp(ResultIndex + 1, 0, std::max((int)LaneCount() - 1, 0));
                });
#endif
        }
    }

    // ===================================================================== 判定のずれ (スコアメーター・オフセット自動調整)
    // ずれ = 叩いた時刻 - 音符の時刻 - 判定オフセット (ミリ秒。正 = 遅い、負 = 早い)。演奏中の表示だけに使い、記録はしない。
    // 判定オフセットの自動調整は、設定画面でテスト譜面を叩いて行う (オフセット自動調整)。

    struct HitErrorSummary {
        size_t Count = 0;
        double Mean = 0.0;
        double StdDev = 0.0;
    };
    struct HitErrorStats {
        static constexpr size_t MaxRecent = 48;
        static constexpr double RecentMs = 3000.0;
        std::vector<float> Play;            // この演奏の値 (段位は全曲分。保存はしない)
        struct Recent { float Error; std::chrono::steady_clock::time_point At; };
        std::deque<Recent> RecentHits;      // メーターに残す最近の打撃
        HitErrorSummary LastPlay;           // 直前の演奏 (リザルト表示用)
    } HitStats;

    static std::string FormatSigned(double v, int decimals) {
        const double scale = std::pow(10.0, decimals);
        const double rounded = std::round(v * scale) / scale;
        std::string str = FormatNumber(rounded == 0 ? 0.0 : rounded, decimals);
        return (rounded > 0 ? "+" : "") + str;
    }
    // 大きく外れた打撃 (可の判定幅の外) を除いて平均とばらつきを求める
    static HitErrorSummary SummarizeHitErrors(const std::vector<float>& errors, double limit) {
        HitErrorSummary sum;
        double total = 0, square = 0;
        for (float e : errors) {
            if (std::abs(e) > limit) { continue; }
            total += e;
            square += (double)e * e;
            ++sum.Count;
        }
        if (sum.Count > 0) {
            sum.Mean = total / (double)sum.Count;
            sum.StdDev = std::sqrt(std::max(0.0, square / (double)sum.Count - sum.Mean * sum.Mean));
        }
        return sum;
    }
    static double HitErrorLimit() { return std::max(1.0, Config.JudgeOk); }

    void RecordHitError(double error) {
        if (Config.AutoPlay || !std::isfinite(error)) { return; }
        HitStats.Play.push_back((float)error);
        HitStats.RecentHits.push_back({ (float)error, std::chrono::steady_clock::now() });
        while (HitStats.RecentHits.size() > HitErrorStats::MaxRecent) { HitStats.RecentHits.pop_front(); }
    }
    void BeginPlayStats() {
        // 段位の 2 曲目以降は 1 曲目から続けて集計する
        if (!(Dan.Active && Dan.SongIndex > 0)) {
            HitStats.Play.clear();
        }
        HitStats.RecentHits.clear();
    }
    void FinishPlayStats() {
        HitStats.LastPlay = SummarizeHitErrors(HitStats.Play, HitErrorLimit());
    }

    // 判定のずれのメーター (中央 = ぴったり、左 = 早い、右 = 遅い)。
    // 色の帯は判定幅 (橙: 良 / 白: 可 / 青: 不可)、白い縦線は最近の打撃、▲ は平均、薄い帯はばらつき (±σ)
    void DrawHitErrorMeter(float cx, float cy, float width, const HitErrorSummary* summary, bool recent, const FontData* font) {
        const double bad = std::max(1.0, Config.JudgeBad);
        const float half = width * 0.5f;
        const float scale = half / (float)bad;
        auto X = [&](double e) { return cx + (float)std::clamp(e, -bad, bad) * scale; };
        const float h = 9.0f;
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 210);
        DrawBoxAA(X(-bad), cy - 3, X(bad), cy + 3, GetColor(70, 120, 230), TRUE);
        DrawBoxAA(X(-Config.JudgeOk), cy - 3, X(Config.JudgeOk), cy + 3, GetColor(235, 235, 235), TRUE);
        DrawBoxAA(X(-Config.JudgeGood), cy - 3, X(Config.JudgeGood), cy + 3, GetColor(255, 170, 40), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawLineAA(cx, cy - h - 2, cx, cy + h + 2, GetColor(255, 255, 255), 2.0f);

        auto zone = [&](double e) {
            const double a = std::abs(e);
            return a < Config.JudgeGood ? GetColor(255, 210, 80) : a < Config.JudgeOk ? GetColor(255, 255, 255) : GetColor(120, 170, 255);
        };
        if (recent) {
            const auto now = std::chrono::steady_clock::now();
            for (const auto& r : HitStats.RecentHits) {
                const double age = std::chrono::duration<double, std::milli>(now - r.At).count();
                if (age > HitErrorStats::RecentMs) { continue; }
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp((int)(255 * (1.0 - age / HitErrorStats::RecentMs)), 0, 255));
                DrawBoxAA(X(r.Error) - 1.5f, cy - h, X(r.Error) + 1.5f, cy + h, zone(r.Error), TRUE);
            }
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        }
        if (summary != nullptr && summary->Count > 0) {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 80);
            DrawBoxAA(X(summary->Mean - summary->StdDev), cy - h, X(summary->Mean + summary->StdDev), cy + h, GetColor(255, 255, 255), TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            const float mx = X(summary->Mean);
            DrawTriangleAA(mx, cy - h - 1, mx - 6, cy - h - 11, mx + 6, cy - h - 11, GetColor(255, 80, 80), TRUE);
            if (font != nullptr && font->Handle != -1) {
                UIText(*font, cx, cy + h + 10 + font->Size * 0.5f, HitErrorText(*summary), GetColor(255, 255, 255), 1);
            }
        }
    }
    static std::string HitErrorText(const HitErrorSummary& sum) {
        if (sum.Count == 0) { return "まだ記録がありません"; }
        const char* tendency = std::abs(sum.Mean) < 1.0 ? "" : sum.Mean > 0 ? " (遅め)" : " (早め)";
        return "平均 " + FormatSigned(sum.Mean, 1) + "ms" + tendency + "  ばらつき " + FormatNumber(sum.StdDev, 1) + "ms  (" + std::to_string(sum.Count) + " 打)";
    }

    // ===================================================================== おすすめ設定
    // この PC の画面 (リフレッシュレート)・CPU・既定の音声デバイスに合わせて、表示とサウンドの設定を決める。
    // 演奏オプション・判定・オフセット・名前・キー・フォルダなどは変えない。

    struct DeviceInfo {
        int RefreshRate = 60;
        unsigned Threads = 4;
        bool AudioOk = false;
        int MixRate = 0;                // 共有モードのサンプリングレート
        int PeriodSamples = 0;          // 既定のデバイス周期 (サンプル数)
    };
    static DeviceInfo DetectDevice() {
        DeviceInfo dev;
        dev.Threads = std::max(1u, std::thread::hardware_concurrency());
#ifndef __ANDROID__
        HMONITOR monitor = MonitorFromWindow(GetMainWindowHandle(), MONITOR_DEFAULTTOPRIMARY);
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (GetMonitorInfoW(monitor, &info) && EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1) {
            dev.RefreshRate = (int)mode.dmDisplayFrequency;
        }
        // 音声デバイスの問い合わせは COM の状態を変えないよう別スレッドで行う
        std::thread([&dev] {
            const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            IMMDeviceEnumerator* enumerator = nullptr;
            IMMDevice* device = nullptr;
            IAudioClient* client = nullptr;
            WAVEFORMATEX* format = nullptr;
            if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator))
                && SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))
                && SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client))
                && SUCCEEDED(client->GetMixFormat(&format)) && format != nullptr) {
                REFERENCE_TIME period = 0, minimum = 0;
                dev.MixRate = (int)format->nSamplesPerSec;
                if (SUCCEEDED(client->GetDevicePeriod(&period, &minimum)) && period > 0) {
                    dev.PeriodSamples = (int)((int64_t)period * dev.MixRate / 10000000);
                }
                dev.AudioOk = dev.MixRate > 0;
            }
            if (format) { CoTaskMemFree(format); }
            if (client) { client->Release(); }
            if (device) { device->Release(); }
            if (enumerator) { enumerator->Release(); }
            if (SUCCEEDED(init)) { CoUninitialize(); }
            }).join();
#endif
        return dev;
    }

    struct RecommendChange {
        std::string Name, Before, After;
        bool Restart = false;
        std::function<void()> Apply;
    };
    std::vector<RecommendChange> BuildRecommendations() {
        std::vector<RecommendChange> out;
        const DeviceInfo dev = DetectDevice();
        auto Boolean = [&](const char* name, bool& v, bool want, bool restart) {
            if (v != want) { out.push_back({ name, v ? "ON" : "OFF", want ? "ON" : "OFF", restart, [&v, want] { v = want; } }); }
        };
        auto Integer = [&](const char* name, int& v, int want, bool restart, std::function<std::string(int)> text) {
            if (v != want) { out.push_back({ name, text(v), text(want), restart, [&v, want] { v = want; } }); }
        };
        auto Real = [&](const char* name, double& v, double want, int decimals) {
            if (std::abs(v - want) > 1e-9) { out.push_back({ name, FormatNumber(v, decimals), FormatNumber(want, decimals), false, [&v, want] { v = want; } }); }
        };
        auto Plain = [](int v) { return std::to_string(v); };

        // 表示: 垂直同期 + 高速入力 (打鍵は Raw Input の時刻で判定するので、描画は垂直同期に合わせて滑らかにする)。
        // 描画開始の割合は CPU に余裕が無いほど早めにして、垂直同期に間に合わないコマ落ちを防ぐ
        Boolean("垂直同期", Config.WaitVSync, true, true);
        Boolean("高速入力", Config.FastInput, true, false);
        Boolean("フレーム単位の時間", Config.FrameCountTimer, true, false);
        Real("描画タイミング", Config.FastDrawRate, dev.Threads <= 4 || dev.RefreshRate >= 200 ? 0.3 : 0.5, 2);

#ifndef __ANDROID__
        // サウンド: WASAPI 共有モードで、デバイスのサンプリングレートと周期に合わせる (変換と余分なバッファを避ける)
        if (dev.AudioOk) {
            static const int rates[] = { 44100, 48000, 96000, 192000 };
            const int rate = std::ranges::find(rates, dev.MixRate) != std::end(rates) ? dev.MixRate : 48000;
            const int period = dev.PeriodSamples > 0 ? dev.PeriodSamples : rate / 100;
            const int buffer = std::clamp((period + 31) / 32 * 32, 128, 2048);
            Integer("出力方式", Config.SoundDeviceType, 1, true, [](int v) {
                static const char* names[] = { "既定", "WASAPI", "XAudio2", "MME (waveOut)", "ASIO" };
                return std::string(v >= 0 && v < 5 ? names[v] : "?");
                });
            Boolean("排他モード", Config.Exclusive, false, true);
            Integer("サンプリングレート", Config.SampleRate, rate, true, [](int v) { return std::to_string(v) + " Hz"; });
            Integer("バッファサイズ", Config.BufferSize, buffer, true, Plain);
            Real("判定オフセット", Config.JudgeOffset, RecommendedJudgeOffset(dev, 1, false, buffer, rate), 0);
        }
        else {
            Real("判定オフセット", Config.JudgeOffset, RecommendedJudgeOffset(dev, Config.SoundDeviceType, Config.Exclusive, Config.BufferSize, Config.SampleRate), 0);
        }
#endif
        return out;
    }

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
        unsigned char CapturePrev[256]{};        // 入力待ちを始めた時のキーの状態 (押しっぱなしのキーを拾わない)
        bool FoldersDirty = false;      // フォルダの数が変わったので行を作り直す
        std::vector<char> FolderExists;
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

    void ShowConfigMessage(std::string message) {
        ConfigUI.Message = std::move(message);
        ConfigUI.MessageUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    }

    static std::string FormatNumber(double value, int decimals) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*f", decimals, value);
        return buf;
    }

    // ---- 文字列の折り返し・省略 (描画用。結果はキャッシュする) ----
    mutable std::unordered_map<std::string, std::vector<std::string>> TextWrapCache;
    mutable std::unordered_map<std::string, std::string> TextFitCache;

    static size_t Utf8CharLen(unsigned char c) {
        return c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
    }
    static std::string TextCacheKey(const std::string& str, int handle, float width, char mode) {
        std::string key;
        key.reserve(str.size() + 16);
        key += mode;
        key.append(reinterpret_cast<const char*>(&handle), sizeof(handle));
        const int w = (int)width;
        key.append(reinterpret_cast<const char*>(&w), sizeof(w));
        key += str;
        return key;
    }
    // 幅 width に収まるように行を分ける。空白があればそこで、なければ文字の境目で折り返す
    std::vector<std::string> WrapText(const std::string& str, int handle, float width) const {
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
    // 幅に収まらなければ省略記号を付けて縮める (front: 先頭を省略してパスの末尾を見せる)
    std::string FitText(const std::string& str, int handle, float width, bool front) const {
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

    void BuildConfigItems() {
        auto& cats = ConfigUI.Categories;
        cats.clear();

        auto Bool = [](std::string name, std::string help, bool& v, bool restart = false) {
            ConfigItem it;
            it.Name = std::move(name);
            it.Help = std::move(help);
            it.Type = ConfigItemType::Bool;
            it.Restart = restart;
            it.Get = [&v] { return std::string(v ? "ON" : "OFF"); };
            it.Step = [&v](int) { v = !v; };
            return it;
        };
        auto Number = [](std::string name, std::string help, auto& v, double min, double max, double step, int decimals, bool restart = false) {
            using T = std::remove_reference_t<decltype(v)>;
            ConfigItem it;
            it.Name = std::move(name);
            it.Help = std::move(help);
            it.Type = ConfigItemType::Number;
            it.Restart = restart;
            it.Get = [&v, decimals] { return FormatNumber((double)v, decimals); };
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
                if (sv.empty() || result.ec != std::errc() || result.ptr != sv.data() + sv.size()) {
                    return false;
                }
                assign(value);
                return true;
            };
            return it;
        };
        // 整数の選択肢 (一覧に無い値が設定されていても、そのまま表示して近い選択肢へ移動できる)
        auto IntChoice = [](std::string name, std::string help, int& v, std::vector<std::pair<int, std::string>> choices, bool restart = false) {
            ConfigItem it;
            it.Name = std::move(name);
            it.Help = std::move(help);
            it.Type = ConfigItemType::Choice;
            it.Restart = restart;
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
        auto Text = [](std::string name, std::string help, std::string& v, size_t maxlength, bool allowempty = false) {
            ConfigItem it;
            it.Name = std::move(name);
            it.Help = std::move(help);
            it.Type = ConfigItemType::Text;
            it.Get = [&v] { return v; };
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
            ConfigItem recommend;
            recommend.Name = "Recommended";
            recommend.Help = "この PC に合わせて表示とサウンドを設定する";
            recommend.Type = ConfigItemType::Action;
            recommend.Get = [] { return std::string("この PC に合わせて設定…"); };
            recommend.Step = [this](int) { OpenRecommendPanel(); };
            game.Items.push_back(std::move(recommend));
        }
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
            game.Items.push_back(std::move(skin));
        }
        game.Items.push_back(Bool("AutoPlay", "譜面を自動で演奏する", Config.AutoPlay));
        game.Items.push_back(Bool("TrainingMode", "小節単位で移動しながら練習する", Config.TrainingMode));
        game.Items.push_back(Number("RandomRate", "音符の色を入れ替える割合 (%)", Config.RandomRate, 0, 100, 5, 0));
        game.Items.push_back(Number("ChartSpeed", "譜面のスクロール速度", Config.ChartSpeed, 0.1, 10.0, 0.1, 1));
        game.Items.push_back(Number("SongSpeed", "曲の再生速度", Config.SongSpeed, 0.5, 2.0, 0.05, 2));
        game.Items.push_back(Number("HiddenLevel", "判定枠の手前で音符を消す強さ", Config.HiddenLevel, 0.0, 10.0, 0.1, 1));
        game.Items.push_back(Number("SuddenLevel", "音符が判定枠の近くまで見えない強さ", Config.SuddenLevel, 0.0, 10.0, 0.1, 1));
        game.Items.push_back(Number("RollSpeed", "オートプレイ時の 1 秒あたりの連打数", Config.RollSpeed, 1, 60, 1, 0));
        game.Items.push_back(Bool("JiroChartMode", "太鼓さん次郎と同じ扱いにする (ダミーノーツ・疑似停止・分岐のタイミングなど)", Config.JiroChartMode));
#ifdef __ANDROID__
        game.Items.push_back(Number("TouchTaikoWidth", "タッチ太鼓の横幅", Config.TouchTaikoWidth, 100, 1500, 10, 0));
        game.Items.push_back(Number("TouchTaikoHeight", "タッチ太鼓の縦幅", Config.TouchTaikoHeight, 50, 1000, 10, 0));
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
        judge.Items.push_back(Bool("ScoreMeter", "判定のずれをメーターで表示する", Config.ScoreMeter));
        {
            ConfigItem calib;
            calib.Name = "AutoOffset";
            calib.Help = "音声デバイスの遅延から判定オフセットを決める";
            calib.Type = ConfigItemType::Action;
            calib.Get = [] { return std::string("自動で設定"); };
            calib.Step = [this](int) { AutoAdjustOffset(); };
            judge.Items.push_back(std::move(calib));
        }
        cats.push_back(std::move(judge));

        // ---- 表示 ----
        ConfigCategory view{ "表示" };
        view.Items.push_back(Number("BGBrightness", "BGMOVIE の明るさ (0 で表示しない)", Config.BGBrightness, 0, 100, 5, 0));
        view.Items.push_back(Bool("HitNoteDisp", "叩いた音符を魂ゲージへ飛ばす演出", Config.HitNoteDisp));
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
#ifndef __ANDROID__
        view.Items.push_back(Bool("RecordPlay", "演奏を録画して recordings フォルダに MP4 で保存する (ffmpeg が必要)", Config.RecordPlay));
        {
            // ON にした時に ffmpeg が無ければ導入を提案する
            auto& item = view.Items.back();
            item.Step = [this, step = item.Step](int n) {
                step(n);
                if (Config.RecordPlay && !EnsureTool(ToolKind::Ffmpeg, "録画には ffmpeg が必要です。")) {
                    Config.RecordPlay = false;
                    Config.Write();
                }
            };
        }
        view.Items.push_back(IntChoice("RecordFps", "録画のフレームレート", Config.RecordFps, { { 30, "30 fps" }, { 60, "60 fps" }, { 120, "120 fps" } }));
        {
            // 保存する形式 (動画・音声のみ)
            ConfigItem it;
            it.Name = "RecordFormat";
            it.Help = "保存する形式 (MP4 / MKV / MOV / WebM / GIF)";
            it.Type = ConfigItemType::Choice;
            it.Get = [] { return std::string(rec::FindFormat(Config.RecordFormat).Label); };
            it.Step = [](int dir) {
                const int n = (int)std::size(rec::Formats);
                int index = (int)(&rec::FindFormat(Config.RecordFormat) - rec::Formats);
                index = ((index + (dir > 0 ? 1 : -1)) % n + n) % n;
                Config.RecordFormat = rec::Formats[index].Ext;
            };
            view.Items.push_back(std::move(it));
        }
        view.Items.push_back(IntChoice("RecordAudio", "録画の音声。録音: ゲームの音を録る (録れない時は譜面から合成) / 合成: 曲と叩いた効果音を譜面から作る", Config.RecordAudio, { { 0, "録音" }, { 1, "合成" } }));
        view.Items.push_back(Number("RecordCrf", "録画の画質 (小さいほど高画質。libx264 の時)", Config.RecordCrf, 14, 32, 1, 0));
        view.Items.push_back(Text("RecordCodec", "映像のエンコーダ (libx264 / h264_nvenc / h264_qsv / h264_amf)", Config.RecordCodec, 32));
#endif
        cats.push_back(std::move(view));

        // ---- サウンド ----
        ConfigCategory sound{ "サウンド" };
        sound.Items.push_back(Number("SongVolume", "曲の音量 (%)", Config.SongVolume, 0, 100, 1, 0));
        sound.Items.push_back(Number("SEVolume", "ドン・カッなどの効果音の音量 (%)", Config.SEVolume, 0, 100, 1, 0));
#ifndef __ANDROID__
        sound.Items.push_back(IntChoice("SoundDeviceType", "音声の出力方式", Config.SoundDeviceType,
            { { 0, "既定" }, { 1, "WASAPI" }, { 2, "XAudio2" }, { 3, "MME (waveOut)" }, { 4, "ASIO" } }, true));
#endif
        sound.Items.push_back(Bool("Exclusive", "WASAPI を排他モードで使う (遅延が小さくなる)", Config.Exclusive, true));
        sound.Items.push_back(IntChoice("SampleRate", "出力のサンプリングレート", Config.SampleRate,
            { { 44100, "44100 Hz" }, { 48000, "48000 Hz" }, { 96000, "96000 Hz" }, { 192000, "192000 Hz" } }, true));
        sound.Items.push_back(Number("BufferSize", "小さいほど低遅延 (途切れる時は大きく)", Config.BufferSize, 32, 8192, 32, 0, true));
        cats.push_back(std::move(sound));

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
        ConfigCategory toolcat{ "ツール" };
        for (ToolKind kind : { ToolKind::Ffmpeg, ToolKind::YtDlp }) {
            ConfigItem it;
            it.Name = ToolName(kind);
            it.Help = kind == ToolKind::Ffmpeg
                ? "録画・音源の変換に使う。配布元 (BtbN/FFmpeg-Builds, GPL) からダウンロードして導入・更新する"
                : "SONGLINK / MOVIELINK のダウンロードに使う。配布元 (github.com/yt-dlp, Unlicense) からダウンロードして導入・更新する";
            it.Type = ConfigItemType::Action;
            it.Get = [kind] {
                const fs::path path = FindTool(kind);
                if (path.empty()) { return std::string("未導入 (選んで導入)"); }
                std::error_code ec;
                const bool local = fs::equivalent(path.parent_path(), GetExecutablePath(), ec);
                return std::string(local ? "導入済み (選んで更新)" : "PATH にあります (選んでゲームのフォルダに導入)");
            };
            it.Step = [this, kind](int) {
                if (ConfirmToolInstall(kind, FindTool(kind).empty() ? "外部ツールを導入します。" : "最新版に更新します。")) {
                    InstallTool(kind);
                }
            };
            toolcat.Items.push_back(std::move(it));
        }
        {
            ConfigItem it;
            it.Name = "ConvertJiroSkin";
            it.Help = "太鼓さん次郎のスキン (img フォルダのあるフォルダ) を選び、FAS のスキンに変換する (Default スキンを土台にする)";
            it.Type = ConfigItemType::Action;
            it.Get = [] { return std::string("フォルダを選んで変換…"); };
            it.Step = [this](int) { FolderBrowse(JiroSkinPickTarget); };
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
            keys.Items.push_back(std::move(it));
        }
        keys.Items.push_back(Number("KeyHoldProcInterval", "メニューでキーを押し続けた時の移動間隔 (秒)", Config.KeyHoldProcInterval, 0.02, 1.0, 0.01, 2));
        cats.push_back(std::move(keys));
#endif
    }

    static const char* ConfigLaneKey(int lane) {
        static const char* names[] = { "KaInputLeft", "DonInputLeft", "DonInputRight", "KaInputRight" };
        return lane >= 0 && lane < 4 ? names[lane] : "";
    }
    static const char* ConfigLaneName(int lane) {
        static const char* names[] = { "カッ (左)", "ドン (左)", "ドン (右)", "カッ (右)" };
        return lane >= 0 && lane < 4 ? names[lane] : "";
    }

    // ---- 譜面フォルダ ----
    void BuildFolderItems(std::vector<ConfigItem>& items) {
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
    static bool SameSongDirectory(const std::string& a, const std::string& b) {
        const fs::path pa = ResolveSongDirectory(a), pb = ResolveSongDirectory(b);
        std::error_code ec;
        const bool same = fs::equivalent(pa, pb, ec);
        return ec ? pa == pb : same;
    }
    // index: -1 で追加、0～ で置き換え。同じフォルダが既にあれば登録しない
    bool SetSongDirectory(int index, const std::string& value) {
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
    void FolderRemove(int index) {
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
    static std::wstring PickFolderDialog(HWND owner, const std::wstring& initial) {
        const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        std::wstring result;
        IFileOpenDialog* dialog = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
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
                    PWSTR path = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr) {
                        result = path;
                        CoTaskMemFree(path);
                    }
                    item->Release();
                }
            }
            dialog->Release();
        }
        if (SUCCEEDED(init)) {
            CoUninitialize();
        }
        return result;
    }
#endif
#ifndef __ANDROID__
    // ---- 太鼓さん次郎のスキンを FAS のスキンに変換する ----
    // 次郎のスキン (img フォルダの画像・snd フォルダの音・img\imgdet.ini) を読み、Default スキンを土台に、
    // 対応がはっきりしている部品 (音符・背景・レーン・ゴーゴーの炎・プレイヤーキャラ・踊り子・効果音) を差し替えた新しいスキンを作る。
    // 位置・大きさは Default スキンのまま (画像だけを差し替えて、FAS の画面の配置に合わせる)
    static constexpr int JiroSkinPickTarget = -3;
    // ---- 太鼓さん次郎2 のスキン (theme\default の Shift_JIS の CSV。#PARTS,名前,画像,x,y,幅,高さ,横のコマ数,縦のコマ数) ----
    struct Jiro2Part { fs::path File; double X = NAN, Y = NAN, W = NAN, H = NAN; int Cols = 1, Rows = 1; };
    static std::wstring SjisToWide(const std::string& s) {
        if (s.empty()) { return {}; }
        const int n = MultiByteToWideChar(932, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring w(n, L'\0');
        MultiByteToWideChar(932, 0, s.data(), (int)s.size(), w.data(), n);
        return w;
    }
    // CSV を読む (#INCLUDE も辿る)。parts: #PARTS、se: #SE (名前, ファイル, 値)
    void ReadJiro2Csv(const fs::path& file, std::map<std::string, Jiro2Part>& parts, std::vector<std::tuple<std::string, fs::path, std::string>>& se, int depth = 0) {
        std::error_code ec;
        if (depth > 8 || !fs::is_regular_file(file, ec)) { return; }
        std::ifstream in(file, std::ios::binary);
        std::string line;
        const fs::path dir = file.parent_path();
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') { line.pop_back(); }
            if (line.empty() || line[0] != '#') { continue; }
            std::vector<std::string> f;
            size_t start = 0;
            for (size_t i = 0; i <= line.size(); ++i) {
                if (i == line.size() || line[i] == ',') { f.push_back(std::string(strtrim(std::string_view(line).substr(start, i - start)))); start = i + 1; }
            }
            auto num = [&](size_t i) { return i < f.size() && !f[i].empty() ? std::atof(f[i].c_str()) : NAN; };
            if (f[0] == "#INCLUDE" && f.size() >= 2) { ReadJiro2Csv(dir / SjisToWide(f[1]), parts, se, depth + 1); }
            else if (f[0] == "#PARTS" && f.size() >= 3) {
                Jiro2Part p;
                if (!f[2].empty()) { p.File = dir / SjisToWide(f[2]); }
                p.X = num(3); p.Y = num(4); p.W = num(5); p.H = num(6);
                p.Cols = std::max(1, (int)(std::isnan(num(7)) ? 1 : num(7)));
                p.Rows = std::max(1, (int)(std::isnan(num(8)) ? 1 : num(8)));
                parts[f[1]] = p;
            }
            else if (f[0] == "#SE" && f.size() >= 3) {
                se.push_back({ f[1], dir / SjisToWide(f[2]), f.size() >= 4 ? f[3] : std::string() });
            }
            else if (f[0] == "#SINGLE" || f[0] == "#SONGSELECT" || f[0] == "#SOUND") {
                if (f.size() >= 2) { ReadJiro2Csv(dir / SjisToWide(f[1]), parts, se, depth + 1); }
            }
        }
    }
    std::string ConvertJiro2Skin(const fs::path& theme, const fs::path& dest, json& conf, std::string name) {
        std::error_code ec;
        std::map<std::string, Jiro2Part> parts;
        std::vector<std::tuple<std::string, fs::path, std::string>> se;
        ReadJiro2Csv(theme, parts, se);
        json& pl = conf["Base"]["Playing"];
        std::vector<std::string> done;
        int copied = 0;
        auto copy = [&](const fs::path& src, const char* sub) -> std::string {
            const fs::path to = dest / sub / "Jiro" / (std::to_wstring(copied++) + L"_" + src.filename().wstring());
            fs::create_directories(to.parent_path(), ec);
            fs::copy_file(src, to, fs::copy_options::overwrite_existing, ec);
            return PathToU8(to.lexically_relative(dest)).c_str();
        };
        auto image = [&](json& node, const char* part, const char* label, bool usesize, bool usepos) {
            auto it = parts.find(part);
            if (it == parts.end() || it->second.File.empty() || !fs::is_regular_file(it->second.File, ec)) { return false; }
            const auto& p = it->second;
            std::string rel = copy(p.File, "Image");
            std::ranges::replace(rel, '\\', '/');
            node["FilePath"] = rel;
            node["Div"] = { { "X", p.Cols }, { "Y", p.Rows } };
            if (usesize && !std::isnan(p.W) && !std::isnan(p.H) && p.W > 1 && p.H > 1) { node["Size"] = { { "Width", p.W }, { "Height", p.H } }; }
            if (usepos && !std::isnan(p.X) && !std::isnan(p.Y)) { node["Pos"] = { { "X", p.X }, { "Y", p.Y } }; }
            done.push_back(label);
            return true;
        };
        image(pl["Image"]["Note"], "NOTE", "音符", false, false);
        image(pl["Image"]["BackGround"], "BG", "演奏画面の背景", false, false);
        image(pl["Image"]["Lane"], "SFIELD", "レーン", false, false);
        image(pl["Image"]["GoGoFire"], "GOGOFIRE", "ゴーゴーの炎", false, false);
        image(pl["Image"]["JudgeString"], "JUDGE", "判定の文字", false, false);
        if (image(pl["Image"]["ComboNumber"], "NUMBER_COMBO", "コンボの数字", true, false)) {}
        if (image(pl["Image"]["PlayerChar"], "PLAYERCHAR", "プレイヤーキャラ", false, true)) {
            auto& pc = pl["Image"]["PlayerChar"];
            int w = 0, h = 0;
            if (GetImageSize_File(pc["FilePath"].get<std::string>().insert(0, PathToU8(dest) + "/").c_str(), &w, &h) == 0) {
                pc["Size"] = { { "Width", w / std::max(1, parts["PLAYERCHAR"].Cols) }, { "Height", h / std::max(1, parts["PLAYERCHAR"].Rows) } };
            }
        }
        if (image(pl["Image"]["Runner"], "RUNNER", "ランナー", true, false)) {
            pl["Image"]["Runner"]["Pos"] = { { "X", 0 }, { "Y", 400 } };
        }
        if (image(pl["Image"]["Dancer"], "DANCER", "踊り子", true, true)) {
            pl["Config"]["DancerCount"] = std::max(1, parts["DANCER"].Rows);
        }
        if (conf["Base"].contains("SongSelect")) { image(conf["Base"]["SongSelect"]["Image"]["BackGround"], "SONGSELECT_BG", "選曲画面の背景", false, false); }
        if (conf["Base"].contains("Result")) { image(conf["Base"]["Result"]["Image"]["BackGround"], "RESULT_BG_CLEAR", "リザルトの背景", false, false); }
        // 効果音とコンボボイス
        json voices = json::array();
        for (const auto& [key, file, value] : se) {
            if (!fs::is_regular_file(file, ec)) { continue; }
            std::string rel = copy(file, "Sound");
            std::ranges::replace(rel, '\\', '/');
            if (key == "DONG") { pl["SE"]["Don"]["FilePath"] = rel; done.push_back("ドンの音"); }
            else if (key == "KA") { pl["SE"]["Ka"]["FilePath"] = rel; done.push_back("カッの音"); }
            else if (key == "BALLOON") { pl["SE"]["Balloon"]["FilePath"] = rel; done.push_back("風船の音"); }
            else if (key == "COMBOVOICE" && std::atoi(value.c_str()) > 0) { voices.push_back({ { "Combo", std::atoi(value.c_str()) }, { "FilePath", rel } }); }
        }
        if (!voices.empty()) { pl["SE"]["ComboVoice"] = voices; done.push_back("コンボボイス"); }
        std::ofstream ofs(dest / "skinconfig.json");
        ofs << conf.dump(4) << "\n";
        ofs.close();
        if (!ofs) { return "スキンの設定を書き込めませんでした"; }
        Skin.Infos.push_back(_Skin::SkinInfo{ name, PathToU8(dest / "skinconfig.json") });
        std::string msg = "スキン「" + name + "」を作りました (次郎2 の形式。SkinName で選べます)。変換: ";
        for (size_t i = 0; i < done.size(); ++i) { msg += (i ? "・" : "") + done[i]; }
        return msg;
    }
    std::string ConvertJiroSkin(const fs::path& source) {
        std::error_code ec;
        // 次郎2: theme\default\default.csv (本体フォルダ・theme・theme\default のどれを選んでもよい)
        // 最初に読む CSV は default.csv か「フォルダ名.csv」
        fs::path jiro2;
        std::vector<fs::path> candidates{ source, source / "default", source / "theme" / "default" };
        for (auto it = fs::directory_iterator(source / "theme", ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
            if (it->is_directory(ec)) { candidates.push_back(it->path()); }
        }
        for (const fs::path& c : candidates) {
            for (const fs::path& csv : { c / "default.csv", c / (c.filename().wstring() + L".csv") }) {
                if (jiro2.empty() && fs::is_regular_file(csv, ec)) { jiro2 = csv; }
            }
        }
        // 次郎の本体フォルダ (img がある) か、img フォルダそのものを選んでもよい
        fs::path root = source;
        if (!fs::is_directory(root / "img", ec) && (fs::exists(root / "notes.png", ec) || fs::exists(root / "notes.bmp", ec))) { root = root.parent_path(); }
        const fs::path img = root / "img", snd = root / "snd";
        auto find = [&](const fs::path& dir, const std::string& name) -> fs::path {
            for (const char* ext : { ".png", ".bmp" }) {
                const fs::path p = dir / (name + ext);
                if (fs::is_regular_file(p, ec)) { return p; }
            }
            return {};
        };
        if (jiro2.empty() && find(img, "notes").empty()) {
            return "次郎のスキンが見つかりません (次郎1: img フォルダの notes.png / notes.bmp、次郎2: theme\\default\\default.csv がありません)";
        }
        const fs::path skins = GetExecutablePath() / "Skins";
        const fs::path base = skins / "Default";
        if (!fs::is_regular_file(base / "skinconfig.json", ec)) { return "土台にする Default スキンがありません"; }
        // 新しいスキンの名前とフォルダ
        std::string name = (jiro2.empty() ? "Jiro_" : "Jiro2_") + PathToU8((jiro2.empty() ? root : jiro2.parent_path()).filename());
        fs::path dest = skins / U8ToPath(name);
        for (int n = 2; fs::exists(dest, ec); ++n) { dest = skins / U8ToPath(name + "_" + std::to_string(n)); }
        name = PathToU8(dest.filename());
        fs::copy(base, dest, fs::copy_options::recursive, ec);
        if (ec) { return "スキンのフォルダを作れませんでした: " + PathToU8(dest); }
        std::ifstream ifs(dest / "skinconfig.json");
        json conf = json::parse(ifs, nullptr, false);
        ifs.close();
        if (!conf.is_object()) { return "Default スキンの設定を読めませんでした"; }
        conf["Info"]["Name"] = name;
        if (!jiro2.empty()) { return ConvertJiro2Skin(jiro2, dest, conf, name); }
        json& pl = conf["Base"]["Playing"];
        fs::create_directories(dest / "Image" / "Jiro", ec);
        fs::create_directories(dest / "Sound" / "Jiro", ec);
        std::vector<std::string> done, missing;
        const fs::path ini = img / "imgdet.ini";
        auto iniint = [&](const char* key, int def) { return (int)GetPrivateProfileIntW(L"ImageSettings", StrToWStr(key).c_str(), def, ini.wstring().c_str()); };
        // 画像を写して、設定の FilePath・Div を書き換える (大きさ・位置は Default のまま)
        auto image = [&](json& node, const std::string& jiro, int divx, int divy, const char* label) {
            const fs::path src = find(img, jiro);
            if (src.empty()) { missing.push_back(jiro); return false; }
            const fs::path to = dest / "Image" / "Jiro" / src.filename();
            fs::copy_file(src, to, fs::copy_options::overwrite_existing, ec);
            node["FilePath"] = "Image/Jiro/" + PathToU8(src.filename());
            node["Div"] = { { "X", std::max(divx, 1) }, { "Y", std::max(divy, 1) } };
            done.push_back(label);
            return true;
        };
        auto imagesize = [&](const std::string& jiro, int& w, int& h) {
            w = h = 0;
            const fs::path src = find(img, jiro);
            return !src.empty() && GetImageSize_File(PathToU8(src).c_str(), &w, &h) == 0 && w > 0 && h > 0;
        };
        int w = 0, h = 0;
        // 音符: 次郎の notes は「判定枠・ドン・カッ・大ドン・大カッ・連打・帯・終わり・大連打・帯・終わり・風船…」の順で FAS と同じ
        if (imagesize("notes", w, h)) {
            const int frames = std::max(iniint("NoteFrames", 1), 1);
            const int cell = h / frames;
            image(pl["Image"]["Note"], "notes", cell > 0 ? w / cell : 1, frames, "音符");
        }
        image(pl["Image"]["BackGround"], "bg", 1, 1, "演奏画面の背景");
        image(pl["Image"]["Lane"], "sfieldbg", 1, 1, "レーン");
        if (imagesize("fire", w, h)) {
            image(pl["Image"]["GoGoFire"], "fire", iniint("FireFrames", 4), 1, "ゴーゴーの炎");
            pl["Config"]["GoGoFireFrameTime"] = iniint("FireInterval", 50);
        }
        // プレイヤーキャラ (4 コマ × 5 段。1 段目を使う)
        if (imagesize("playerchar_p1", w, h)) {
            json& pc = pl["Image"]["PlayerChar"];
            pc["Pos"] = { { "X", 160 }, { "Y", 110 } };
            pc["Size"] = { { "Width", w / 4 * 1.5 }, { "Height", h / 5 * 1.5 } };
            image(pc, "playerchar_p1", 4, 5, "プレイヤーキャラ");
        }
        // 踊り子: dancer_1～5 (コマ × 3 段) の 1 段目を縦に並べた 1 枚にまとめる
        {
            std::vector<int> sheets;
            int cw = 0, chh = 0, frames = 0;
            for (int i = 1; i <= 5; ++i) {
                const fs::path src = find(img, "dancer_" + std::to_string(i));
                if (src.empty()) { break; }
                const int si = LoadSoftImage(PathToU8(src).c_str());
                if (si == -1) { break; }
                int sw = 0, shh = 0;
                GetSoftImageSize(si, &sw, &shh);
                if (sheets.empty()) { chh = shh / 3; cw = chh; frames = cw > 0 ? sw / cw : 0; }
                sheets.push_back(si);
            }
            if (!sheets.empty() && cw > 0 && frames > 0) {
                const int out = MakeARGB8ColorSoftImage(cw * frames, chh * (int)sheets.size());
                FillSoftImage(out, 0, 0, 0, 0);
                for (size_t i = 0; i < sheets.size(); ++i) {
                    BltSoftImage(0, 0, cw * frames, chh, sheets[i], 0, chh * (int)i, out);
                }
                const fs::path to = dest / "Image" / "Jiro" / "dancer.png";
                if (SaveSoftImageToPng(PathToU8(to).c_str(), out, 0) == 0) {
                    json& d = pl["Image"]["Dancer"];
                    d["FilePath"] = "Image/Jiro/dancer.png";
                    d["Div"] = { { "X", frames }, { "Y", (int)sheets.size() } };
                    d["Size"] = { { "Width", cw * 1.5 }, { "Height", chh * 1.5 } };
                    d["Pos"] = { { "X", 320 }, { "Y", 560 } };
                    pl["Config"]["DancerCount"] = (int)sheets.size();
                    pl["Config"]["DancerDistance"] = { { "X", 160 }, { "Y", 0 } };
                    done.push_back("踊り子");
                }
                DeleteSoftImage(out);
            }
            for (int si : sheets) { DeleteSoftImage(si); }
            if (sheets.empty()) { missing.push_back("dancer_1～5"); }
        }
        // 他の画面の背景
        if (conf["Base"].contains("SongSelect")) { image(conf["Base"]["SongSelect"]["Image"]["BackGround"], "songselectbg", 1, 1, "選曲画面の背景"); }
        if (conf["Base"].contains("Result")) { image(conf["Base"]["Result"]["Image"]["BackGround"], "resultbg_clear", 1, 1, "リザルトの背景"); }
        // 効果音
        auto sound = [&](json& node, const char* jiro, const char* label) {
            const fs::path src = snd / jiro;
            if (!fs::is_regular_file(src, ec)) { missing.push_back(std::string("snd/") + jiro); return; }
            fs::copy_file(src, dest / "Sound" / "Jiro" / jiro, fs::copy_options::overwrite_existing, ec);
            node["FilePath"] = std::string("Sound/Jiro/") + jiro;
            done.push_back(label);
        };
        sound(pl["SE"]["Don"], "dong.wav", "ドンの音");
        sound(pl["SE"]["Ka"], "ka.wav", "カッの音");
        sound(pl["SE"]["Balloon"], "balloon.wav", "風船の音");
        std::ofstream ofs(dest / "skinconfig.json");
        ofs << conf.dump(4) << "\n";
        ofs.close();
        if (!ofs) { return "スキンの設定を書き込めませんでした"; }
        Skin.Infos.push_back(_Skin::SkinInfo{ name, PathToU8(dest / "skinconfig.json") });
        std::string msg = "スキン「" + name + "」を作りました (SkinName で選べます)。変換: ";
        for (size_t i = 0; i < done.size(); ++i) { msg += (i ? "・" : "") + done[i]; }
        if (!missing.empty()) { msg += "  (無かったもの: " + std::to_string(missing.size()) + " 個)"; }
        return msg;
    }
#endif
    // target: -1 で追加、0～ でそのフォルダを置き換え
    void FolderBrowse(int target) {
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
            state->Result = PickFolderDialog(owner, initial);
            state->Done = true;
            }).detach();
        Skin.Base->Title.SE.Don.Play();
#else
        (void)target;
        ShowConfigMessage("パスを入力してください");
#endif
    }
    void FolderPickFinished(const std::wstring& result, int target) {
        if (result.empty() || target == -2) {
            return;
        }
#ifndef __ANDROID__
        if (target == JiroSkinPickTarget) {
            ShowConfigMessage(ConvertJiroSkin(fs::path(result)));
            return;
        }
#endif
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

    // ---- レイアウト (描画と当たり判定で共通) ----
    static constexpr const char* ConfigControlsText =
        "↑↓: 項目  ←→: 変更  Enter: 決定  Tab: カテゴリ  Esc: 戻る";
    static constexpr int FolderParts = 2;       // 0: パス (表示のみ), 1: 削除
    static constexpr int FolderAddParts = 2;    // 0: 参照して追加, 1: パスを入力して追加
    static const char* FolderAddLabel(int part) {
        static const char* labels[] = { "参照して追加…", "パスを入力…" };
        return part >= 0 && part < FolderAddParts ? labels[part] : "";
    }

    const FontData& ConfigFont() const { return Skin.Base->ConfigMenu.Font.String; }
    // 値・ボタン・説明欄用の小さいフォント (スキンの文字のフォントの約 7 割の大きさ。入力欄やボタンに収まるように)
    mutable FontData ConfigSmall;
    mutable int ConfigSmallSource = -2;
    mutable float ConfigSmallSourceSize = 0;
    const FontData& ConfigSmallFont() const {
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
    float ConfigLineHeight() const {
        const float size = ConfigSmallFont().Size;
        return size > 1 ? size * 1.35f : 24.0f;
    }
    float ConfigTextWidth(const char* str) const {
        const int handle = ConfigSmallFont().Handle;
        return handle != -1 ? (float)GetStrlenCached(str, handle) : 24.0f * (float)std::strlen(str) / 3.0f;
    }
    // 画面下の説明欄: 説明 2 行 + お知らせ 1 行 + 操作方法 (折り返した行数)
    float ConfigFooterHeight() const {
        const size_t controls = WrapText(ConfigControlsText, ConfigSmallFont().Handle, (float)Skin.Info.Resolution.X - 48.0f).size();
        return 12.0f + ConfigLineHeight() * (3.0f + (float)controls);
    }
    float ConfigRowHeight() const {
        const float d = Skin.Base->ConfigMenu.Config.BoxDistance;
        return d > 8 ? d : 64.0f;
    }
    UIRect ConfigListArea() const {
        const float top = 120.0f;
        const float bottom = std::max(top + ConfigRowHeight(), (float)Skin.Info.Resolution.Y - ConfigFooterHeight());
        return { 24.0f, top, (float)Skin.Info.Resolution.X - 24.0f, bottom };
    }
    int ConfigVisibleRows() const {
        const UIRect area = ConfigListArea();
        return std::max(1, (int)((area.B - area.T) / ConfigRowHeight()));
    }
    float ConfigRowCenter(int i) const {
        const float h = ConfigRowHeight();
        return ConfigListArea().T + (i - ConfigUI.Scroll) * h + h * 0.5f;
    }
    UIRect ConfigRowRect(int i) const {
        const float cy = ConfigRowCenter(i);
        const float h = ConfigRowHeight();
        const UIRect area = ConfigListArea();
        return { area.L, cy - h * 0.5f, area.R, cy + h * 0.5f };
    }
    // 項目の箱 (スキンの Box 画像) の右端と高さ。値・ボタンはこの中に収める
    float ConfigRowRight() const {
        const auto& box = Skin.Base->ConfigMenu.Image.Box;
        float right = ConfigListArea().R - 24.0f;
        if (box.Size.Width > 0) { right = std::min(right, box.Pos.X + box.Size.Width * 0.5f - 16.0f); }
        return right;
    }
    float ConfigBoxHeight() const {
        const auto& box = Skin.Base->ConfigMenu.Image.Box;
        return box.Size.Height > 0 ? std::min(ConfigRowHeight(), box.Size.Height) : ConfigRowHeight();
    }
    UIRect ConfigValueRect(int i) const {
        const float cy = ConfigRowCenter(i);
        const float h = ConfigBoxHeight() * 0.32f;
        float x = Skin.Base->ConfigMenu.Config.ValPos.X;
        if (x <= 0) { x = Skin.Info.Resolution.X * 0.5f; }
        const float right = ConfigRowRight();
        x = std::min(x, right - 120.0f);
        return { x, cy - h, std::min(x + 420.0f, right), cy + h };
    }
    UIRect ConfigArrowRect(int i, bool right) const {
        const UIRect v = ConfigValueRect(i);
        return right ? UIRect{ v.R - 48, v.T, v.R, v.B } : UIRect{ v.L, v.T, v.L + 48, v.B };
    }
    int ConfigKeySlotCount(const ConfigItem& item) const {
#ifndef __ANDROID__
        if (item.Keys == nullptr) { return 0; }
        const int n = (int)item.Keys->size();
        return n + (n < (int)_ConfigData::MaxKeysPerLane ? 1 : 0);   // 末尾は「+」(追加) の枠
#else
        (void)item;
        return 0;
#endif
    }
    UIRect ConfigKeySlotRect(int i, int slot, int count) const {
        const UIRect v = ConfigValueRect(i);
        const float right = ConfigRowRight();
        const float w = std::min(132.0f, (right - v.L) / (float)std::max(count, 1));
        return { v.L + w * slot, v.T, v.L + w * (slot + 1) - 6, v.B };
    }
    int ConfigPartCount(const ConfigItem& item) const {
        switch (item.Type) {
        case ConfigItemType::Key: return ConfigKeySlotCount(item);
        case ConfigItemType::Folder: return FolderParts;
        case ConfigItemType::FolderAdd: return FolderAddParts;
        default: return 1;
        }
    }
    UIRect ConfigFolderPartRect(int i, ConfigItemType type, int part) const {
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
    // 文字入力欄の位置
    UIRect ConfigEditRect(int i) const {
        if (i < 0 || i >= (int)ConfigUI.Categories[ConfigUI.Category].Items.size()) { return ConfigValueRect(std::max(i, 0)); }
        const ConfigItemType type = ConfigUI.Categories[ConfigUI.Category].Items[i].Type;
        if (type == ConfigItemType::Folder) { return ConfigFolderPartRect(i, type, 0); }
        if (type == ConfigItemType::FolderAdd) {
            const UIRect first = ConfigFolderPartRect(i, type, 0);
            return { first.L, first.T, ConfigRowRight(), first.B };
        }
        return ConfigValueRect(i);
    }
    UIRect ConfigTabRect(int i) const {
        const int n = std::max(1, (int)ConfigUI.Categories.size());
        const float w = (Skin.Info.Resolution.X - 48.0f) / n;
        return { 24.0f + w * i, 52.0f, 24.0f + w * (i + 1) - 6.0f, 100.0f };
    }
    // 戻るボタンは右上 (画面下は説明欄に使う)
    UIRect ConfigBackRect() const {
        return { (float)Skin.Info.Resolution.X - 184.0f, 6.0f, (float)Skin.Info.Resolution.X - 24.0f, 46.0f };
    }

    PointerState ReadPointer() {
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
        pt.Wheel = Input.GetMouseWheel();
#else
        if (GetTouchInputNum() > 0) {
            GetTouchInput(0, &pt.X, &pt.Y, nullptr, nullptr);
            pt.Left = true;
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

    void UIText(const FontData& font, float x, float y, const std::string& str, unsigned int color, int align = 0) {
        if (font.Handle == -1 || str.empty()) { return; }
        const float w = (float)GetStrlenCached(str, font.Handle);
        const float dx = align == 1 ? w * 0.5f : (align == 2 ? w : 0.0f);
        DrawStringFToHandle(x - dx, y - font.Size * 0.5f, str.c_str(), color, font.Handle, GetColor(0, 0, 0));
    }
    static void UIBox(const UIRect& r, unsigned int color, int alpha, bool fill = true) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
        DrawBoxAA(r.L, r.T, r.R, r.B, color, fill ? TRUE : FALSE, 2.0f);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    std::vector<ConfigItem>& ConfigItems() {
        return ConfigUI.Categories[ConfigUI.Category].Items;
    }
    void ConfigEnsureVisible() {
        const int n = (int)ConfigItems().size();
        const int vis = ConfigVisibleRows();
        ConfigUI.Selected = std::clamp(ConfigUI.Selected, 0, std::max(n - 1, 0));
        if (ConfigUI.Selected < ConfigUI.Scroll) { ConfigUI.Scroll = ConfigUI.Selected; }
        if (ConfigUI.Selected >= ConfigUI.Scroll + vis) { ConfigUI.Scroll = ConfigUI.Selected - vis + 1; }
        ConfigUI.Scroll = std::clamp(ConfigUI.Scroll, 0, std::max(n - vis, 0));
    }
    void ConfigSetCategory(int index) {
        const int n = (int)ConfigUI.Categories.size();
        ConfigUI.Category = ((index % n) + n) % n;
        ConfigUI.Selected = 0;
        ConfigUI.Scroll = 0;
        ConfigUI.KeySlot = 0;
    }
    void ConfigChanged() {
        Config.Write();
    }
    // フォルダの追加・削除で行の数が変わった時は、次のフレームの最初に行を作り直す
    // (操作中の項目への参照を壊さないため、変更した直後には作り直さない)
    void ConfigRebuildFoldersIfNeeded() {
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
    void ConfigStep(ConfigItem& item, int dir) {
        if (!item.Step) { return; }
        item.Step(dir);
        Skin.Base->Title.SE.Ka.Play();
        ConfigChanged();
    }
    void ConfigBeginEdit(ConfigItem& item) {
        if (!item.Set) { return; }
        Skin.Base->Title.SE.Don.Play();
#ifndef __ANDROID__
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
    void ConfigEndEdit(bool apply) {
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
    // キーを割り当てる。同じレーンで重複していれば移動し、他のレーンにあれば外す (1 つのキーで 2 レーン叩かれないように)
    void ConfigAssignKey(ConfigItem& item, int key) {
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
    void ConfigRemoveKey(ConfigItem& item, int slot) {
        if (item.Keys == nullptr || slot < 0 || slot >= (int)item.Keys->size()) { return; }
        item.Keys->erase(item.Keys->begin() + slot);
        Skin.Base->Title.SE.Ka.Play();
        ConfigChanged();
    }
#endif
    void ConfigFolderPart(ConfigItem& item, int part) {
        if (item.Type == ConfigItemType::FolderAdd) {
            if (part == 0) { FolderBrowse(-1); }
            else { ConfigBeginEdit(item); }
            return;
        }
        if (part == 1) { FolderRemove(item.Index); }
    }
    // 項目の決定操作 (Enter / ドン / クリック)
    void ConfigActivate(ConfigItem& item) {
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
    // WndProc が受け取ったキー状態から、前回からの間に押されたキーを集める (Raw Input が使えない環境でも動く)
    static std::vector<int> NewlyPressedKeys(byte (&prev)[256]) {
        std::vector<int> out;
        for (int key = 1; key < 256; ++key) {
            const bool down = __gotkeyboardinput[key] != 0;
            if (down && !prev[key]) { out.push_back(key); }
            prev[key] = down ? 1 : 0;
        }
        return out;
    }
#endif
    // ---- オフセット自動調整: 音声の出力遅延の目安から判定オフセットを決める ----
    // 叩く人は聞こえた音に合わせるので、音が遅れて出る分だけ判定を遅らせる (正の判定オフセット)。
    // 遅延 = ゲームの出力バッファ + デバイスの周期 (出力方式ごとの目安)
    static double EstimateOutputLatencyMs(int type, bool exclusive, int buffer, int rate, const DeviceInfo& dev) {
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
    double RecommendedJudgeOffset(const DeviceInfo& dev, int type, bool exclusive, int buffer, int rate) const {
        return std::clamp(std::round(EstimateOutputLatencyMs(type, exclusive, buffer, rate, dev)), 0.0, 500.0);
    }
    void AutoAdjustOffset() {
        const DeviceInfo dev = DetectDevice();
        const double before = Config.JudgeOffset;
        Config.JudgeOffset = RecommendedJudgeOffset(dev, Config.SoundDeviceType, Config.Exclusive, Config.BufferSize, Config.SampleRate);
        ConfigChanged();
        ShowConfigMessage("判定オフセット: " + FormatSigned(before, 0) + "ms → " + FormatSigned(Config.JudgeOffset, 0) + "ms (音声の遅延の目安)");
    }

    // ---- おすすめ設定の確認画面 ----
    void OpenRecommendPanel() {
        ConfigUI.Recommend = BuildRecommendations();
        if (ConfigUI.Recommend.empty()) {
            ShowConfigMessage("すでにこの PC へのおすすめの設定になっています");
            return;
        }
        ConfigUI.RecommendOpen = true;
        PointerPrev.Left = true;
    }
    UIRect RecommendPanelRect() const {
        const float x = (float)Skin.Info.Resolution.X, y = (float)Skin.Info.Resolution.Y;
        return { x * 0.15f, y * 0.12f, x * 0.85f, y * 0.88f };
    }
    UIRect RecommendButtonRect(int index) const {
        const UIRect r = RecommendPanelRect();
        const float w = 200.0f;
        const float cx = (r.L + r.R) * 0.5f + (index == 0 ? -w * 0.5f - 12.0f : w * 0.5f + 12.0f);
        return { cx - w * 0.5f, r.B - 66.0f, cx + w * 0.5f, r.B - 18.0f };
    }
    void ConfigRecommendProc(const PointerState& pt) {
        auto Apply = [&] {
            bool restart = false;
            for (auto& change : ConfigUI.Recommend) {
                change.Apply();
                restart = restart || change.Restart;
            }
            Config.Write();
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
        Input.HitKeyProcess(VK_RETURN, KeyState::Down, Apply);
        if (!ConfigUI.RecommendOpen) { return; }
        Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, Cancel);
#else
        Touch.Process(TouchType::Other, Cancel, Skin.Base->ConfigMenu.Image.Back);
#endif
    }
    void DrawRecommendPanel(const FontData& font) {
        const unsigned int white = GetColor(255, 255, 255), black = GetColor(0, 0, 0), accent = GetColor(255, 200, 40);
        const UIRect r = RecommendPanelRect();
        UIBox({ 0, 0, (float)Skin.Info.Resolution.X, (float)Skin.Info.Resolution.Y }, black, 130);
        UIBox(r, GetColor(18, 20, 32), 240);
        UIBox(r, white, 200, false);
        const float lh = ConfigLineHeight();
        float y = r.T + 20.0f + lh * 0.5f;
        UIText(font, r.L + 24, y, "この PC へのおすすめ設定 (次の項目を変更します)", white);
        y += lh * 1.4f;
        const float limit = r.B - 90.0f;
        for (const auto& change : ConfigUI.Recommend) {
            if (y > limit) { UIText(font, r.L + 40, y, "…", white); break; }
            UIText(font, r.L + 40, y, FitText(change.Name + (change.Restart ? " *" : "") + ":  " + change.Before + "  →  " + change.After, font.Handle, r.R - r.L - 64, false), white);
            y += lh;
        }
        const char* labels[] = { "適用 (Enter)", "キャンセル (Esc)" };
        for (int i = 0; i < 2; ++i) {
            const UIRect b = RecommendButtonRect(i);
            UIBox(b, i == 0 ? accent : GetColor(50, 54, 70), 230);
            UIBox(b, white, 160, false);
            UIText(font, (b.L + b.R) * 0.5f, (b.T + b.B) * 0.5f, labels[i], i == 0 ? black : white, 1);
        }
    }
    void ConfigMenuInit() {
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
        ConfigUI.DirsOnEnter = Config.SongDirectories;
        ConfigUI.NameOnEnter = Config.PlayerName;
        ConfigUI.TitleOnEnter = Config.TitleName;
        PointerPrev.Left = true;       // 画面を開いたクリックを項目の操作として扱わない
#ifndef __ANDROID__
        SetMouseDispFlag(TRUE);
#endif
    }
    void ConfigMenuEnd() {
        ConfigEndEdit(false);
        ConfigUI.Capturing = false;
#ifndef __ANDROID__
        // 開いたままのフォルダ選択ダイアログの結果は捨てる (ダイアログを閉じるまで次の選択は開かない)
        if (ConfigUI.Picker) { ConfigUI.PickerTarget = PickerDiscard; }
#endif
        Config.Write();
        if (Config.SkinName != ConfigUI.SkinOnEnter) {
            Skin.Load(Config.SkinName);
        }
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

    void ConfigMenuDraw() {
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
            UIBox(r, active ? accent : black, active ? 220 : 140);
            UIBox(r, white, 160, false);
            UIText(sfont, (r.L + r.R) * 0.5f, (r.T + r.B) * 0.5f, FitText(ConfigUI.Categories[i].Name, sfont.Handle, r.R - r.L - 8, false), active ? black : white, 1);
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
                    UIBox(r, current ? accent : black, current ? 200 : (addslot ? 90 : 150));
                    if (addslot) { UIBox(r, white, 110, false); }
                    std::string label = addslot ? std::string("+") : (names != nullptr && slot < (int)names->size() ? (*names)[slot] : std::string("?"));
                    if (current && ConfigUI.Capturing) { label = "..."; }
                    UIText(sfont, (r.L + r.R) * 0.5f, cy, FitText(label, sfont.Handle, r.R - r.L - 8, false), current ? black : white, 1);
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
                UIBox(del, current ? accent : GetColor(44, 48, 64), 225);
                UIBox(del, white, 120, false);
                UIText(sfont, (del.L + del.R) * 0.5f, cy, "削除", current ? black : white, 1);
                break;
            }
            case ConfigItemType::Action: {
                UIBox(v, selected ? accent : GetColor(44, 48, 64), 225);
                UIBox(v, white, 120, false);
                UIText(sfont, (v.L + v.R) * 0.5f, cy, FitText(item.Get(), sfont.Handle, v.R - v.L - 16, false), selected ? black : white, 1);
                break;
            }
            case ConfigItemType::FolderAdd: {
                for (int part = 0; part < FolderAddParts; ++part) {
                    const UIRect r = ConfigFolderPartRect(i, item.Type, part);
                    const bool current = selected && ConfigUI.KeySlot == part;
                    UIBox(r, current ? accent : GetColor(44, 48, 64), 225);
                    UIBox(r, white, 120, false);
                    if (!(ConfigUI.Editing && selected)) {
                        UIText(sfont, (r.L + r.R) * 0.5f, cy, FitText(FolderAddLabel(part), sfont.Handle, r.R - r.L - 12, false), current ? black : white, 1);
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
            DrawKeyInputString((int)v.L + 12, (int)(((v.T + v.B) * 0.5f) - font.Size * 0.5f), ConfigUI.EditHandle);
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
#else
        Skin.Base->ConfigMenu.Image.Back.Draw({});
#endif
    }

    void ConfigMenuProc() {
        if (ConfigUI.Categories.empty()) {
            ConfigMenuInit();
        }
        ConfigRebuildFoldersIfNeeded();
        auto Leave = [&] {
            Skin.Base->Title.SE.Don.Play();
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
                Input.ResyncKeys();
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
                Input.ResyncKeys();
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
                Input.ResyncKeys();
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
#ifndef __ANDROID__
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
        Input.HitKeyProcess(VK_UP, KeyState::Down, [&] { Move(-1); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_DOWN, KeyState::Down, [&] { Move(1); }, Config.KeyHoldProcInterval);
        Input.HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { Move(-1); }, Config.KeyHoldProcInterval);
        Input.HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { Move(1); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_LEFT, KeyState::Down, [&] { Change(-1); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_RIGHT, KeyState::Down, [&] { Change(1); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_RETURN, KeyState::Down, Activate);
        Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, Activate);
        Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, Activate);
        Input.HitKeyProcess(VK_TAB, KeyState::Down, [&] {
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
        };
        Input.HitKeyProcess(VK_DELETE, KeyState::Down, ClearKey);
        Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, Leave);
#else
        Touch.Process(TouchType::Other, Leave, Skin.Base->ConfigMenu.Image.Back);
#endif
    }

    // ===================================================================== 譜面制作モード
    // 小節を横 1 列に並べ、グリッド (分割数) に合わせてマウスで音符を置く。命令 (#BPMCHANGE など) は小節の中の位置に挿入する。
    // 譜面は tja::Document (Library/TjaDocument.hpp) で持ち、保存時に TJA に書き戻す (元が Shift_JIS ならそのまま Shift_JIS)。
    // テストプレイはメモリ上の譜面で演奏画面を開き、カーソルの小節から始める (トレーニングモードの小節移動を使う)。

    struct EditorButton {
        UIRect Rect;
        std::string Label;
        std::function<void()> Action;
        bool Active = false;            // 選択中 (パレットなど)
    };
    struct EditorState {
        tja::Document Doc;
        fs::path Path;
        bool Open = false;
        bool Dirty = false;
        bool WasSjis = false;           // 元のファイルが Shift_JIS だった
        int Course = 0;
        int Top = 0;                    // 一番上に表示している小節
        int Division = 16;              // グリッドの分割数 (1 小節あたり)
        char Palette = '1';
        int CursorMeasure = 0;
        int CursorSlot = 0;
        std::vector<tja::Document> Undo, Redo;
        enum class PanelType { None, Commands, Header, Grid, Estimate, Gimmick } Panel = PanelType::None;
        // 範囲選択 (Shift+クリック / Shift+矢印): 位置は小節番号 + 小節の中の分数
        bool Selecting = false;
        int SelMeasure = 0;
        int64_t SelNum = 0, SelDen = 1;
        struct ClipItem { int64_t Num = 0, Den = 1; char Type = '1'; std::string Command; };    // 選択の始まりからの位置 (小節単位の分数)
        std::vector<ClipItem> Clipboard;
        bool SavedAutoPlay = false;
        // 自動推定 (BPM・開始オフセット・拍子)
        bool Estimating = false;
        std::future<tempo::Result> EstimateTask;
        tempo::Result Estimate;
        bool Editing = false;           // 文字入力中
        int EditHandle = 0;
        std::string EditLabel;
        std::function<void(const std::string&)> OnEdit;
        std::string Message;
        std::chrono::steady_clock::time_point MessageUntil{};
        std::chrono::steady_clock::time_point ExitConfirmUntil{};
#ifndef __ANDROID__
        std::shared_ptr<FolderPickState> Picker;
        int PickerKind = 0;             // 1: 開く, 2: 新規の保存先, 3: 新規の音源, 4: WAVE の変更
        fs::path NewPath;
#endif
        bool Testing = false;           // テストプレイ中 (演奏画面から戻ったら元に戻す)
        bool SavedTraining = false;
        int TestMeasure = 0;
        std::string TestText;
        bool Rescan = false;            // 保存したので選曲画面の一覧を作り直す
        // 疑似再生 (編集画面のまま、カーソルの小節から音源と一緒に流す)
        bool Preview = false;
        double PreviewFrom = 0;         // 再生を始めた譜面の時刻 (ms)
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

    static double EditorCommandValue(const std::string& text, size_t keylength) {
        if (text.size() <= keylength) { return 0.0; }
        return std::atof(std::string(tja::Trim(std::string_view(text).substr(keylength))).c_str());
    }
    // 選べるグリッドの分割数 (1 小節あたり)。◀ ▶ は「よく使う分割」(Config.EditorGrids) だけを順に切り替える
    static constexpr int EditorDivisions[] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256 };
    static constexpr int EditorRows = 4;

    void EditorMessage(std::string text) {
        Editor.Message = std::move(text);
        Editor.MessageUntil = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    }
    tja::Course* EditorCourse() {
        return Editor.Open && Editor.Course >= 0 && Editor.Course < (int)Editor.Doc.Courses.size() ? &Editor.Doc.Courses[Editor.Course] : nullptr;
    }
    int EditorMeasureCount() {
        const auto* c = EditorCourse();
        return c != nullptr ? (int)c->Measures.size() : 0;
    }
    // 変更の前に呼ぶ (元に戻す用に今の状態を残す)
    void EditorChange() {
        Editor.Undo.push_back(Editor.Doc);
        if (Editor.Undo.size() > 200) { Editor.Undo.erase(Editor.Undo.begin()); }
        Editor.Redo.clear();
        Editor.Dirty = true;
    }
    void EditorClamp() {
        const int n = std::max(EditorMeasureCount(), 1);
        Editor.Course = std::clamp(Editor.Course, 0, std::max((int)Editor.Doc.Courses.size() - 1, 0));
        Editor.CursorMeasure = std::clamp(Editor.CursorMeasure, 0, n - 1);
        Editor.CursorSlot = std::clamp(Editor.CursorSlot, 0, Editor.Division - 1);
        if (Editor.CursorMeasure < Editor.Top) { Editor.Top = Editor.CursorMeasure; }
        if (Editor.CursorMeasure >= Editor.Top + EditorRows) { Editor.Top = Editor.CursorMeasure - EditorRows + 1; }
        Editor.Top = std::clamp(Editor.Top, 0, std::max(n - 1, 0));
    }

    // ---- ファイル ----
#ifndef __ANDROID__
    static std::wstring EditorFileDialog(HWND owner, bool save, bool audio, const std::wstring& folder, const std::wstring& name) {
        const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        std::wstring result;
        IFileDialog* dialog = nullptr;
        if (SUCCEEDED(CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
            const COMDLG_FILTERSPEC tjafilter[] = { { L"TJA 譜面 (*.tja)", L"*.tja" } };
            const COMDLG_FILTERSPEC audiofilter[] = { { L"音源 (*.ogg;*.wav;*.mp3)", L"*.ogg;*.wav;*.mp3" } };
            dialog->SetFileTypes(1, audio ? audiofilter : tjafilter);
            if (!audio) { dialog->SetDefaultExtension(L"tja"); }
            DWORD options = 0;
            dialog->GetOptions(&options);
            dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | (save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST));
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
                    PWSTR path = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr) {
                        result = path;
                        CoTaskMemFree(path);
                    }
                    item->Release();
                }
            }
            dialog->Release();
        }
        if (SUCCEEDED(init)) { CoUninitialize(); }
        return result;
    }
    void EditorPick(int kind, bool save, bool audio, const fs::path& folder, const std::wstring& name) {
        if (Editor.Picker) { return; }
        if (Config.FullScreen) {
            EditorMessage("フルスクリーンではファイルを選べません (ウィンドウモードにしてください)");
            return;
        }
        auto state = std::make_shared<FolderPickState>();
        Editor.Picker = state;
        Editor.PickerKind = kind;
        const HWND owner = GetMainWindowHandle();
        const std::wstring dir = folder.wstring();
        std::thread([state, owner, save, audio, dir, name] {
            state->Result = EditorFileDialog(owner, save, audio, dir, name);
            state->Done = true;
            }).detach();
    }
#endif
    fs::path EditorDefaultFolder() const {
        if (!Editor.Path.empty()) { return Editor.Path.parent_path(); }
        if (!Config.SongDirectories.empty()) { return ResolveSongDirectory(Config.SongDirectories.front()); }
        return GetExecutablePath();
    }
    bool EditorOpen(const fs::path& path) {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open()) {
            EditorMessage("譜面を開けませんでした: " + PathToU8(path.filename()));
            return false;
        }
        std::string raw((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        if (raw.starts_with("\xEF\xBB\xBF")) { raw.erase(0, 3); }
        TextLines text;
        text.LoadFromMemory(raw);
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
        Editor.Open = true;
        Editor.Dirty = false;
        Editor.Undo.clear();
        Editor.Redo.clear();
        Editor.Course = 0;
        // 鬼のコースがあれば最初に開く
        for (int i = 0; i < (int)Editor.Doc.Courses.size(); ++i) {
            if (ToLower(Editor.Doc.CourseName(i)) == "oni" || Editor.Doc.CourseName(i) == "3") { Editor.Course = i; break; }
        }
        Editor.Top = Editor.CursorMeasure = Editor.CursorSlot = 0;
        Editor.Panel = EditorState::PanelType::None;
        EditorMessage("開きました: " + PathToU8(path.filename()));
        return true;
    }
    // 元の文字コードで書く (Shift_JIS で表せない文字があれば UTF-8 (BOM 付き))
    bool EditorSave() {
        if (!Editor.Open || Editor.Path.empty()) { return false; }
        const std::string utf8 = Editor.Doc.Write();
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
    // SONGLINK の URL から音源をダウンロードして WAVE の場所に保存する (選曲画面の SONGLINK と同じ yt-dlp を使う)
    void EditorFetchSong() {
        std::string link(strtrim(tja::Document::GetValue(Editor.Doc.Header, "SONGLINK")));
        if (link.empty()) { EditorMessage("SONGLINK に音源の URL を入力してください"); return; }
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
        Input.ResyncKeys();
#endif
        PointerPrev.Left = PointerPrev.Right = true;
        EditorMessage(fs::exists(target, ec) ? "音源を保存しました: " + PathToU8(target.filename()) : std::string("音源を保存できませんでした"));
    }
    // MOVIELINK の URL から背景動画をダウンロードして BGMOVIE の場所に保存する (選曲画面の MOVIELINK と同じ)
    void EditorFetchMovie() {
#ifndef __ANDROID__
        std::string link(strtrim(tja::Document::GetValue(Editor.Doc.Header, "MOVIELINK")));
        if (link.empty()) { EditorMessage("MOVIELINK に動画の URL を入力してください"); return; }
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
        Input.ResyncKeys();
        PointerPrev.Left = PointerPrev.Right = true;
        EditorMessage(fs::exists(target, ec) ? "動画を保存しました: " + PathToU8(target.filename()) : std::string("動画を保存できませんでした"));
#endif
    }
    // ---- 自動推定: 音源から BPM・開始オフセット・拍子を推定する (Library/TempoEstimate.hpp) ----
    void EditorStartEstimate() {
        if (Editor.Estimating) { return; }
        const std::string wave(strtrim(tja::Document::GetValue(Editor.Doc.Header, "WAVE")));
        const fs::path path = (Editor.Path.parent_path() / U8ToPath(wave)).lexically_normal();
        std::error_code ec;
        if (wave.empty() || !fs::is_regular_file(path, ec)) { EditorMessage("WAVE の音源が見つかりません (ヘッダーで音源を指定してください)"); return; }
        // 音源のデコードは DxLib で行い (メインスレッド)、解析は別スレッドで行う
        const int handle = LoadSoftSound(PathToU8(path).c_str());
        if (handle == -1) { EditorMessage("音源を読み込めませんでした"); return; }
        int channels = 0, bits = 0, rate = 0, isfloat = 0;
        GetSoftSoundFormat(handle, &channels, &bits, &rate, &isfloat);
        const LONGLONG count = GetSoftSoundSampleNum(handle);
        const void* image = GetSoftSoundDataImage(handle);
        std::vector<float> mono;
        if (image != nullptr && count > 0 && channels > 0 && (bits == 16 || (bits == 32 && isfloat))) {
            mono.resize((size_t)count);
            for (LONGLONG i = 0; i < count; ++i) {
                float sum = 0.0f;
                for (int c = 0; c < channels; ++c) {
                    sum += bits == 16 ? ((const int16_t*)image)[i * channels + c] / 32768.0f : ((const float*)image)[i * channels + c];
                }
                mono[(size_t)i] = sum / (float)channels;
            }
        }
        DeleteSoftSound(handle);
        if (mono.empty()) { EditorMessage("この音源の形式は解析できません"); return; }
        Editor.Estimating = true;
        Editor.EstimateTask = std::async(std::launch::async, [samples = std::move(mono), rate] {
            return tempo::Estimate(samples.data(), samples.size(), rate);
        });
    }
    void EditorApplyEstimate() {
        const auto& r = Editor.Estimate;
        EditorChange();
        const double bpm = std::abs(r.Bpm - std::round(r.Bpm)) < 1e-6 ? std::round(r.Bpm) : r.Bpm;
        tja::Document::SetValue(Editor.Doc.Header, "BPM", FormatNumber(bpm, bpm == std::floor(bpm) ? 0 : 2));
        tja::Document::SetValue(Editor.Doc.Header, "OFFSET", FormatNumber(-r.FirstDownbeat, 3));
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
    // 選んだ音源から WAVE: の値を決める。ゲームのフォルダの外にある音源は譜面のフォルダへ複製し、譜面からの相対パスで書く
    // (譜面フォルダごと持ち運んだり配布したりしても音源が見つかるように)
    std::string EditorWaveValue(const fs::path& audio, const fs::path& chartdir) {
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
    void EditorNewChart(const fs::path& path, const fs::path& audio) {
        std::string wave;
        if (!audio.empty()) {
            wave = EditorWaveValue(audio, path.parent_path());
        }
        Editor.Doc = tja::Document::NewChart(PathToU8(path.stem()), wave);
        Editor.Path = path;
        Editor.Open = true;
        Editor.WasSjis = false;
        Editor.Undo.clear();
        Editor.Redo.clear();
        Editor.Course = 0;
        Editor.Top = Editor.CursorMeasure = Editor.CursorSlot = 0;
        if (EditorSave()) { EditorMessage("新しい譜面を作りました: " + PathToU8(path.filename())); }
    }
    static int CourseIndexFromName(std::string name) {
        name = ToLower(std::string(strtrim(name)));
        static const char* names[] = { "easy", "normal", "hard", "oni", "edit", "tower", "dan" };
        for (int i = 0; i < 7; ++i) { if (name == names[i]) { return i; } }
        if (name == "ura") { return 4; }
        int num = -1;
        std::from_chars(name.data(), name.data() + name.size(), num);
        return num >= 0 && num < (int)CourseType::Count ? num : (int)CourseType::Oni;
    }
    void EditorTestPlay() {
        if (!Editor.Open || Editor.Path.empty()) { return; }
        EditorStopPreview();
        Editor.TestText = Editor.Doc.Write();
        Editor.TestMeasure = Editor.CursorMeasure;
        Editor.Testing = true;
        Editor.SavedTraining = Config.TrainingMode;
        Editor.SavedAutoPlay = Config.AutoPlay;
        Config.TrainingMode = Editor.TestMeasure > 0;     // 途中の小節から: トレーニングモードの小節移動で始める
        CourseIndex = (size_t)CourseIndexFromName(Editor.Doc.CourseName(Editor.Course));
        Training.Init();
        PrevScene = Scene::Editor;
        NowScene = Scene::Loading;
    }

    // ---- レイアウト ----
    float EditorResX() const { return (float)Skin.Info.Resolution.X; }
    float EditorResY() const { return (float)Skin.Info.Resolution.Y; }
    // 編集画面の文字 (画面の高さに合わせた小さめの字。ボタン・ラベルが画面の幅に収まるように)
    mutable FontData EditorFontData;
    mutable int EditorFontSize = 0;
    const FontData& EditorFont() const {
        const int size = std::clamp((int)std::lround(EditorResY() / 44.0f), 14, 30);
        if (size != EditorFontSize || EditorFontData.Handle == -1) {
            EditorFontData.Delete();
            EditorFontSize = size;
            EditorFontData.Size = (float)size;
            EditorFontData.Handle = CreateFontToHandle(nullptr, size, 2, DX_FONTTYPE_ANTIALIASING_EDGE_8X8, DX_CHARSET_UTF8, 1);
        }
        return EditorFontData.Handle != -1 ? EditorFontData : ConfigSmallFont();
    }
    float EditorTextWidth(const std::string& str) const {
        const int handle = EditorFont().Handle;
        return handle != -1 ? (float)GetStrlenCached(str, handle) : 12.0f * (float)str.size();
    }
    float EditorLine() const { return EditorFont().Size * 1.4f; }
    mutable float EditorButtonsBottom = 0;      // ボタンの最後の段の下端 (EditorButtons で更新)
    // 小節番号・BPM を書く列の幅
    float EditorLabelWidth() const { return EditorTextWidth("BPM 0000.00") + 20.0f; }
    UIRect EditorGridArea() const {
        const float lh = EditorLine();
        const float top = std::max(EditorButtonsBottom, lh * 2.8f) + lh * 0.6f;
        return { 16.0f, top, EditorResX() - 16.0f, EditorResY() - lh * 2.2f };
    }
    float EditorRowHeight() const { const UIRect a = EditorGridArea(); return (a.B - a.T) / EditorRows; }
    UIRect EditorLane(int row) const {
        const UIRect a = EditorGridArea();
        const float h = EditorRowHeight();
        const float top = a.T + h * row;
        return { a.L + EditorLabelWidth(), top + h * 0.45f, a.R - 10.0f, top + h * 0.92f };
    }
    float EditorX(const UIRect& lane, double pos) const { return lane.L + (lane.R - lane.L) * (float)pos; }

    // ボタンのまとまり (枠と見出しを描く)
    struct EditorGroup { UIRect Rect; std::string Label; };
    std::vector<EditorGroup> EditorGroups;
    std::vector<EditorButton> EditorButtons() {
        std::vector<EditorButton> out;
        EditorGroups.clear();
        if (!Editor.Open) { return out; }
        const float lh = EditorLine();
        const float h = lh * 1.7f;              // ボタンの高さ
        const float pad = 6.0f, gap = 10.0f;
        const float left = 12.0f, right = EditorResX() - 12.0f;
        float x = left, y = 8.0f;
        struct Item { std::string Label; std::function<void()> Action; bool Active = false; };
        // まとまりごとに並べる (入りきらなければ次の段へ)。見出しはまとまりの左に小さく書く
        auto group = [&](const std::string& name, std::vector<Item> items) {
            const float labelw = EditorTextWidth(name) + 12.0f;
            float width = pad + labelw;
            for (const auto& it : items) { width += EditorTextWidth(it.Label) + 28.0f + 4.0f; }
            width += pad - 4.0f;
            if (x > left && x + width > right) { x = left; y += h + pad * 2 + 6.0f; }
            EditorGroups.push_back({ { x, y, x + width, y + h + pad * 2 }, name });
            float bx = x + pad + labelw;
            for (auto& it : items) {
                const float w = EditorTextWidth(it.Label) + 28.0f;
                out.push_back({ { bx, y + pad, bx + w, y + pad + h }, it.Label, std::move(it.Action), it.Active });
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
            { "保存", [this] { if (EditorSave()) { Skin.Base->Title.SE.Don.Play(); } } },
            { "テストプレイ", [this] { EditorTestPlay(); } },
            { "閉じる", [this] { EditorClose(); } },
        });
        group("再生", {
            { Editor.Preview ? "■ 停止" : "▶ 再生", [this] { EditorTogglePreview(); }, Editor.Preview },
            { "速度 " + FormatNumber(Editor.PreviewSpeed, 2) + "x", [this] { EditorSetPreviewSpeed(Editor.PreviewSpeed >= 2.0 - 1e-9 ? -6 : 1); } },
        });
        group("編集", {
            { "元に戻す", [this] { EditorUndo(false); } },
            { "やり直し", [this] { EditorUndo(true); } },
        });
        group("小節", {
            { "挿入", [this] {
                EditorChange();
                auto& ms = EditorCourse()->Measures;
                ms.insert(ms.begin() + std::min(Editor.CursorMeasure + 1, (int)ms.size()), tja::Measure{});
                ++Editor.CursorMeasure;
                EditorClamp();
            } },
            { "削除", [this] {
                auto& ms = EditorCourse()->Measures;
                if (ms.size() <= 1) { EditorMessage("最後の 1 小節は削除できません"); return; }
                EditorChange();
                ms.erase(ms.begin() + Editor.CursorMeasure);
                EditorClamp();
            } },
        });
        group("譜面", {
            { "ヘッダー", panel(EditorState::PanelType::Header), Editor.Panel == EditorState::PanelType::Header },
            { "命令", panel(EditorState::PanelType::Commands), Editor.Panel == EditorState::PanelType::Commands },
            { "ギミック", panel(EditorState::PanelType::Gimmick), Editor.Panel == EditorState::PanelType::Gimmick },
            { "自動推定", [this] { EditorStartEstimate(); } },
        });
        newline();
        group("コース", {
            { "◀", [this] { Editor.Course = (Editor.Course + (int)Editor.Doc.Courses.size() - 1) % (int)Editor.Doc.Courses.size(); EditorClamp(); } },
            { Editor.Doc.CourseName(Editor.Course), [] {} },
            { "▶", [this] { Editor.Course = (Editor.Course + 1) % (int)Editor.Doc.Courses.size(); EditorClamp(); } },
        });
        group("グリッド", {
            { "◀", [this] { EditorSetDivision(-1); } },
            { std::to_string(Editor.Division) + " 分割…", panel(EditorState::PanelType::Grid) },
            { "▶", [this] { EditorSetDivision(1); } },
        });
        static const std::pair<char, const char*> palette[] = {
            { '1', "ドン" }, { '2', "カッ" }, { '3', "大ドン" }, { '4', "大カッ" }, { '5', "連打" },
            { '6', "大連打" }, { '7', "風船" }, { '8', "連打終点" }, { '9', "くすだま" }, { '0', "休符" },
        };
        std::vector<Item> notes;
        for (const auto& [type, label] : palette) {
            notes.push_back({ std::string(1, type) + " " + label, [this, t = type] { Editor.Palette = t; }, Editor.Palette == type });
        }
        group("音符", std::move(notes));
        return out;
    }
    // ボタンの説明 (マウスを乗せると画面下に出す)
    static std::string EditorButtonHelp(const std::string& label) {
        static const std::pair<const char*, const char*> helps[] = {
            { "保存", "譜面を保存する (Ctrl+S)。初めて上書きする時は元のファイルを .bak として残す" },
            { "テストプレイ", "カーソルの小節から演奏してみる (F5)。F1 でオートプレイの切り替え、Esc で戻る" },
            { "▶ 再生", "編集画面のまま、カーソルの位置から音源と一緒に流す (Space)" },
            { "■ 停止", "疑似再生を止める (Space)" },
            { "速度", "疑似再生の速度 (0.25～2 倍)。押すたびに速くなり、2 倍の次は 0.25 倍に戻る。音の高さは変わらない" },
            { "ヘッダー", "曲名・BPM・音源 (WAVE)・OFFSET・コースなどを編集する" },
            { "命令", "#BPMCHANGE などの命令をカーソルの位置に入れる" },
            { "ギミック", "太鼓さん次郎のギミック (逆走・瞬間移動・疑似停止など) の書き方を計算して入れる" },
            { "挿入", "カーソルの小節の後ろに空の小節を入れる" },
            { "削除", "カーソルの小節を消す (Ctrl+Z で元に戻せる)" },
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
    std::vector<int> EditorGridList() const {
        std::vector<int> grids;
        for (int g : Config.EditorGrids) {
            if (std::ranges::find(EditorDivisions, g) != std::end(EditorDivisions) && std::ranges::find(grids, g) == grids.end()) { grids.push_back(g); }
        }
        std::ranges::sort(grids);
        if (grids.empty()) { grids = { 4, 8, 16 }; }
        return grids;
    }
    void EditorSelectDivision(int division) {
        const int old = Editor.Division;
        Editor.Division = std::max(division, 1);
        Editor.CursorSlot = std::clamp((int)std::lround((double)Editor.CursorSlot * Editor.Division / old), 0, Editor.Division - 1);
    }
    void EditorSetDivision(int dir) {
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
    // ---- 範囲選択・コピー・貼り付け ----
    struct EditorPos { int Measure; int64_t Num, Den; };
    EditorPos EditorCursorPos() const { return { Editor.CursorMeasure, Editor.CursorSlot, Editor.Division }; }
    static int EditorComparePos(const EditorPos& a, const EditorPos& b) {
        if (a.Measure != b.Measure) { return a.Measure < b.Measure ? -1 : 1; }
        return tja::ComparePos(a.Num, a.Den, b.Num, b.Den);
    }
    void EditorBeginSelection() {
        if (!Editor.Selecting) {
            Editor.Selecting = true;
            Editor.SelMeasure = Editor.CursorMeasure;
            Editor.SelNum = Editor.CursorSlot;
            Editor.SelDen = Editor.Division;
        }
    }
    // 選択範囲 (始まり <= 終わり。終わりの位置の音符も含む)
    std::pair<EditorPos, EditorPos> EditorSelection() const {
        EditorPos a{ Editor.SelMeasure, Editor.SelNum, Editor.SelDen }, b = EditorCursorPos();
        if (EditorComparePos(a, b) > 0) { std::swap(a, b); }
        return { a, b };
    }
    bool EditorInSelection(int measure, int64_t num, int64_t den) const {
        if (!Editor.Selecting) { return false; }
        const auto [a, b] = EditorSelection();
        const EditorPos p{ measure, num, den };
        return EditorComparePos(a, p) <= 0 && EditorComparePos(p, b) <= 0;
    }
    // (measure + num/den) - (start) を小節単位の分数で
    static void EditorRelative(const EditorPos& start, int measure, int64_t num, int64_t den, int64_t& outnum, int64_t& outden) {
        outden = std::lcm(den, start.Den);
        outnum = (int64_t)(measure - start.Measure) * outden + num * (outden / den) - start.Num * (outden / start.Den);
        tja::Reduce(outnum, outden);
    }
    bool EditorCopy(bool cut) {
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
    void EditorPaste() {
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
    void EditorUndo(bool redo) {
        auto& from = redo ? Editor.Redo : Editor.Undo;
        auto& to = redo ? Editor.Undo : Editor.Redo;
        if (from.empty()) { EditorMessage(redo ? "やり直せる操作がありません" : "元に戻せる操作がありません"); return; }
        to.push_back(std::move(Editor.Doc));
        Editor.Doc = std::move(from.back());
        from.pop_back();
        Editor.Dirty = true;
        EditorClamp();
    }
    void EditorClose() {
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
    }

    // ---- 命令・ヘッダー ----
    struct EditorCommandHit { int Measure = -1; int Index = -1; UIRect Rect; };
    std::vector<EditorCommandHit> EditorCommandRects() {
        std::vector<EditorCommandHit> out;
        const auto* course = EditorCourse();
        if (course == nullptr) { return out; }
        const FontData& sfont = EditorFont();
        for (int row = 0; row < EditorRows; ++row) {
            const int m = Editor.Top + row;
            if (m >= (int)course->Measures.size()) { break; }
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
    void EditorBeginEdit(std::string label, std::string initial, std::function<void(const std::string&)> done) {
#ifndef __ANDROID__
        Editor.EditHandle = MakeKeyInput(1024, TRUE, FALSE, FALSE);
        SetKeyInputString(initial.c_str(), Editor.EditHandle);
        SetActiveKeyInput(Editor.EditHandle);
        SetKeyInputStringFont(EditorFont().Handle);
        Editor.Editing = true;
        Editor.EditLabel = std::move(label);
        Editor.OnEdit = std::move(done);
#else
        done(GetInputText(label));
#endif
    }
    static const std::vector<std::pair<std::string, std::string>>& EditorCommandTemplates() {
        static const std::vector<std::pair<std::string, std::string>> list = {
            { "#BPMCHANGE", "#BPMCHANGE 120" }, { "#SCROLL", "#SCROLL 1" }, { "#MEASURE", "#MEASURE 4/4" }, { "#DELAY", "#DELAY 0.5" },
            { "#GOGOSTART", "#GOGOSTART" }, { "#GOGOEND", "#GOGOEND" }, { "#BARLINEOFF", "#BARLINEOFF" }, { "#BARLINEON", "#BARLINEON" },
            { "#SECTION", "#SECTION" }, { "#BRANCHSTART", "#BRANCHSTART p,80,90" }, { "#N", "#N" }, { "#E", "#E" }, { "#M", "#M" },
            { "#BRANCHEND", "#BRANCHEND" }, { "#LEVELHOLD", "#LEVELHOLD" }, { "自由入力…", "#" },
        };
        return list;
    }
    void EditorInsertCommand(const std::string& text) {
        const std::string t(strtrim(text));
        if (t.empty() || t == "#") { return; }
        EditorChange();
        tja::Document::AddCommand(EditorCourse()->Measures[Editor.CursorMeasure], Editor.CursorSlot, Editor.Division, t);
        EditorMessage("命令を挿入しました: " + t);
    }
    // ---- ギミック (太鼓さん次郎のギミック譜面の定番の書き方を、カーソルの位置の BPM・拍子から計算して挿入する) ----
    // 参考: 「太鼓さん次郎（無印）のギミック解説」(2reflexivity.blogspot.com) の各ギミックの構文。
    // 次郎形式 (JiroChartMode) と #HBSCROLL で、次郎と同じ見え方になる
    struct EditorFrac {
        double Num = 1, Den = 1;
        double Value() const { return Den != 0 ? Num / Den : 0; }
    };
    // "1/4" や "0.25" を分数として読む
    static std::optional<EditorFrac> EditorParseFrac(std::string_view text) {
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
    // 数値を短く書く (0.128 / 120000 / -0.004)
    static std::string EditorNum(double v) {
        if (std::abs(v) < 1e-12) { v = 0; }
        std::string out = std::format("{:.6f}", v);
        while (out.find('.') != std::string::npos && (out.back() == '0' || out.back() == '.')) {
            const bool dot = out.back() == '.';
            out.pop_back();
            if (dot) { break; }
        }
        return out == "-0" ? "0" : out;
    }
    static std::string EditorFracText(double num, double den) { return EditorNum(num) + "/" + EditorNum(den); }

    // カーソルの小節の頭の BPM と拍子 (#MEASURE)
    struct EditorTiming { double Bpm = 120; EditorFrac Measure{ 4, 4 }; };
    EditorTiming EditorTimingAt(int measure) {
        EditorTiming t;
        if (const auto v = tja::Document::GetValue(Editor.Doc.Header, "BPM"); !v.empty()) { t.Bpm = std::atof(v.c_str()); }
        const auto* course = EditorCourse();
        if (course == nullptr) { return t; }
        for (int m = 0; m < std::min(measure, (int)course->Measures.size()); ++m) {
            for (const auto& c : course->Measures[m].Commands) {
                if (tja::StartsWithKey(c.Text, "#BPMCHANGE")) { t.Bpm = EditorCommandValue(c.Text, 10); }
                else if (tja::StartsWithKey(c.Text, "#MEASURE")) {
                    if (auto f = EditorParseFrac(std::string_view(c.Text).substr(8))) { t.Measure = *f; }
                }
            }
        }
        // カーソルの小節の頭にある命令も含める
        if (measure < (int)course->Measures.size()) {
            for (const auto& c : course->Measures[measure].Commands) {
                if (c.Num != 0) { continue; }
                if (tja::StartsWithKey(c.Text, "#BPMCHANGE")) { t.Bpm = EditorCommandValue(c.Text, 10); }
                else if (tja::StartsWithKey(c.Text, "#MEASURE")) {
                    if (auto f = EditorParseFrac(std::string_view(c.Text).substr(8))) { t.Measure = *f; }
                }
            }
        }
        if (t.Bpm == 0) { t.Bpm = 120; }
        return t;
    }

    struct EditorGimmick {
        const char* Name;
        const char* Params;         // 入力欄の説明
        const char* Default;        // 入力欄の初期値
        const char* Help;
    };
    static const std::vector<EditorGimmick>& EditorGimmicks() {
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
        };
        return list;
    }
    // ギミックを TJA の行に組み立てる。戻り値: 挿入する行 (#START / #END で包んで tja::Document で読む)
    // place: 0 = カーソルの小節の前に小節として入れる, 1 = カーソルの位置に命令として入れる, 2 = コースの最後に入れる
    std::optional<std::vector<std::string>> EditorBuildGimmick(int index, const std::string& params, int& place, std::string& error) {
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
        default:
            return bad("不明なギミックです");
        }
        return out;
    }
    void EditorInsertGimmick(int index, const std::string& params) {
        auto* course = EditorCourse();
        if (course == nullptr) { return; }
        int place = 0;
        std::string error;
        const auto lines = EditorBuildGimmick(index, params, place, error);
        if (!lines) { EditorMessage("ギミックを挿入できません: " + error); return; }
        EditorChange();
        const std::string name = EditorGimmicks()[index].Name;
        if (place == 1) {
            auto& m = course->Measures[Editor.CursorMeasure];
            for (const auto& line : *lines) { tja::Document::AddCommand(m, Editor.CursorSlot, Editor.Division, line); }
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
                for (auto& t : added.Trailing) { course->Trailing.insert(course->Trailing.begin(), std::move(t)); }
            }
            else {
                const int at = Editor.CursorMeasure;
                // 元に戻す命令 (最後の ',' の後ろ) は、元のカーソルの小節の頭に入れる
                auto& target = course->Measures[at];
                std::vector<tja::Command> head;
                for (auto& t : added.Trailing) { head.push_back({ 0, 1, std::move(t) }); }
                target.Commands.insert(target.Commands.begin(), head.begin(), head.end());
                course->Measures.insert(course->Measures.begin() + at, added.Measures.begin(), added.Measures.end());
            }
        }
        // 次郎のギミックは #BMSCROLL / #HBSCROLL が無いと動かない
        const bool hasscroll = std::ranges::any_of(course->Header, [](const std::string& h) { return tja::StartsWithKey(h, "#HBSCROLL") || tja::StartsWithKey(h, "#BMSCROLL"); });
        if (!hasscroll && index != 2) { course->Header.push_back("#HBSCROLL"); }
        EditorClamp();
        EditorMessage("ギミックを挿入しました: " + name + (!hasscroll && index != 2 ? "  (#HBSCROLL を追加)" : "") + (Config.JiroChartMode ? "" : "  ※ Config の JiroChartMode を ON にすると次郎と同じ見え方になります"));
    }

    // ---- 疑似再生 ----
    // 小節ごとに「小節の中の位置 → 譜面の時刻」の折れ線を作る (#BPMCHANGE は途中でも効き、#DELAY は時刻を飛ばす。#MEASURE は小節の頭で効く)
    struct EditorTimeline {
        struct Measure { std::vector<std::pair<double, double>> Map; };    // (位置 0～1, 時刻 ms)
        std::vector<Measure> Measures;
        struct Note { double Time; char Type; };
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
    EditorTimeline EditorBuildTimeline() {
        EditorTimeline tl;
        const auto* course = EditorCourse();
        if (course == nullptr) { return tl; }
        double bpm = 120;
        if (const auto v = tja::Document::GetValue(Editor.Doc.Header, "BPM"); !v.empty() && std::atof(v.c_str()) != 0) { bpm = std::atof(v.c_str()); }
        double ratio = 1.0, pending = 0, t = 0;
        bool haspending = false;
        for (const auto& m : course->Measures) {
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
                    if (auto f = EditorParseFrac(std::string_view(c.Text).substr(8)); f && f->Den != 0) { pending = f->Value(); haspending = true; }
                }
            }
            advance(1.0);
            tl.Measures.push_back(std::move(out));
        }
        for (int i = 0; i < (int)course->Measures.size(); ++i) {
            for (const auto& n : course->Measures[i].Notes) {
                tl.Notes.push_back({ tl.TimeAt(i, (double)n.Num / std::max<int64_t>(n.Den, 1)), n.Type });
            }
        }
        std::ranges::stable_sort(tl.Notes, {}, &EditorTimeline::Note::Time);
        return tl;
    }
    double EditorPreviewNow() const {
        return Editor.PreviewFrom + std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Editor.PreviewClock).count() * Editor.PreviewSpeed;
    }
    static constexpr double EditorSpeeds[] = { 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0 };
    // 疑似再生の速度を変える (再生中なら今の位置から続ける。音源は周波数を変えるので音の高さも変わる)
    void EditorSetPreviewSpeed(int dir) {
        const double* begin = std::begin(EditorSpeeds), * end = std::end(EditorSpeeds);
        const double* it = std::find_if(begin, end, [&](double v) { return v >= Editor.PreviewSpeed - 1e-9; });
        int index = (int)(it - begin);
        if (it == end || std::abs(*it - Editor.PreviewSpeed) > 1e-9) { index = 3; }
        index = std::clamp(index + dir, 0, (int)std::size(EditorSpeeds) - 1);
        if (Editor.Preview) {
            const double now = EditorPreviewNow();
            Editor.PreviewFrom = now;
            Editor.PreviewClock = std::chrono::steady_clock::now();
        }
        EditorStopSong();
        Editor.PreviewSpeed = EditorSpeeds[index];
        EditorRequestStretch();
        EditorMessage("再生速度: " + FormatNumber(Editor.PreviewSpeed, 2) + " 倍 (音の高さは変わりません)");
    }
    // 今の速度で流す音源 (速度 1 は元の音源、それ以外は時間を伸び縮みさせた音源。まだできていなければ -1)
    int EditorActiveSong() const {
        if (std::abs(Editor.PreviewSpeed - 1.0) < 1e-9) { return Editor.SongHandle; }
        return std::abs(Editor.StretchSpeed - Editor.PreviewSpeed) < 1e-9 ? Editor.StretchHandle : -1;
    }
    void EditorStopSong() {
        for (int h : { Editor.SongHandle, Editor.StretchHandle }) { if (h != -1) { StopSoundMem(h); } }
        Editor.SongStarted = false;
    }
    // 音源を読み込む (WAVE が変わった時だけ)
    void EditorLoadSong() {
        const std::string wave = tja::Document::GetValue(Editor.Doc.Header, "WAVE");
        const fs::path path = wave.empty() ? fs::path() : Editor.Path.parent_path() / U8ToPath(wave);
        if (path == Editor.SongPath) { return; }
        EditorFreeSong();
        Editor.SongPath = path;
        std::error_code ec;
        if (!path.empty() && fs::is_regular_file(path, ec)) {
            SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMPRESS);
            Editor.SongHandle = LoadSoundMem(path.string().c_str());
        }
    }
    // 速度を変えた音源を別スレッドで作り始める
    void EditorRequestStretch() {
        if (std::abs(Editor.PreviewSpeed - 1.0) < 1e-9 || std::abs(Editor.StretchSpeed - Editor.PreviewSpeed) < 1e-9) { return; }
        if (Editor.StretchTask.valid() && std::abs(Editor.StretchTaskSpeed - Editor.PreviewSpeed) < 1e-9) { return; }
        EditorLoadSong();
        if (Editor.SongPath.empty()) { return; }
        if (!Editor.SongPcmTried) {
            // デコードは DxLib で (メインスレッド)。1 回だけ行い、速度を変えるたびに使い回す
            Editor.SongPcmTried = true;
            const int handle = LoadSoftSound(PathToU8(Editor.SongPath).c_str());
            if (handle != -1) {
                int channels = 0, bits = 0, rate = 0, isfloat = 0;
                GetSoftSoundFormat(handle, &channels, &bits, &rate, &isfloat);
                const LONGLONG count = GetSoftSoundSampleNum(handle);
                const void* image = GetSoftSoundDataImage(handle);
                if (image != nullptr && count > 0 && channels > 0 && (bits == 16 || (bits == 32 && isfloat))) {
                    Editor.SongPcm.resize((size_t)count * channels);
                    for (size_t i = 0; i < Editor.SongPcm.size(); ++i) {
                        Editor.SongPcm[i] = bits == 16 ? ((const int16_t*)image)[i] / 32768.0f : ((const float*)image)[i];
                    }
                    Editor.SongPcmChannels = channels;
                    Editor.SongPcmRate = rate;
                }
                DeleteSoftSound(handle);
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
    // 別スレッドで作った音源ができていれば、再生できる形にする
    void EditorPollStretch() {
        if (!Editor.StretchTask.valid() || Editor.StretchTask.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { return; }
        std::vector<float> out = Editor.StretchTask.get();
        if (out.empty() || Editor.SongPcmChannels <= 0) { return; }
        if (Editor.StretchHandle != -1) { DeleteSoundMem(Editor.StretchHandle); Editor.StretchHandle = -1; }
        const int ch = std::min(Editor.SongPcmChannels, 2);
        const int frames = (int)(out.size() / Editor.SongPcmChannels);
        const int soft = MakeSoftSoundCustom(ch, 16, Editor.SongPcmRate, frames, FALSE);
        if (soft == -1) { return; }
        if (auto* dst = (int16_t*)GetSoftSoundDataImage(soft)) {
            for (int f = 0; f < frames; ++f) {
                for (int c = 0; c < ch; ++c) {
                    dst[(size_t)f * ch + c] = (int16_t)std::lround(std::clamp(out[(size_t)f * Editor.SongPcmChannels + c], -1.0f, 1.0f) * 32767.0f);
                }
            }
            Editor.StretchHandle = LoadSoundMemFromSoftSound(soft);
            Editor.StretchSpeed = Editor.StretchHandle != -1 ? Editor.StretchTaskSpeed : 0;
        }
        DeleteSoftSound(soft);
    }
    // 音源の今の位置 (速度を変えた音源ではその時間に直す。ms)
    double EditorSongPosition(double charttime) const {
        const double offset = std::atof(tja::Document::GetValue(Editor.Doc.Header, "OFFSET").c_str()) * 1000.0;
        return (charttime + offset) / Editor.PreviewSpeed;
    }
    void EditorStopPreview() {
        if (!Editor.Preview) { return; }
        Editor.Preview = false;
        EditorStopSong();
    }
    void EditorReleaseSong() {
        EditorStopPreview();
        EditorFreeSong();
    }
    // 読み込んだ音源を消す (疑似再生は止めない)
    void EditorFreeSong() {
        EditorStopSong();
        if (Editor.StretchCancel) { *Editor.StretchCancel = true; }
        if (Editor.StretchTask.valid()) { Editor.StretchTask.wait(); Editor.StretchTask = {}; }
        if (Editor.SongHandle != -1) { DeleteSoundMem(Editor.SongHandle); Editor.SongHandle = -1; }
        if (Editor.StretchHandle != -1) { DeleteSoundMem(Editor.StretchHandle); Editor.StretchHandle = -1; }
        Editor.StretchSpeed = 0;
        Editor.SongPcm.clear();
        Editor.SongPcm.shrink_to_fit();
        Editor.SongPcmTried = false;
        Editor.SongPath.clear();
    }
    void EditorTogglePreview() {
        if (Editor.Preview) { EditorStopPreview(); return; }
        if (EditorCourse() == nullptr) { return; }
        const auto tl = EditorBuildTimeline();
        Editor.PreviewFrom = tl.TimeAt(Editor.CursorMeasure, (double)Editor.CursorSlot / Editor.Division);
        Editor.PreviewLast = Editor.PreviewFrom - 1.0;
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
    // 毎フレーム: 音源を流し、通り過ぎた音符の効果音を鳴らし、再生位置の小節が見えるようにスクロールする
    void EditorUpdatePreview() {
        EditorPollStretch();
        if (!Editor.Preview) { return; }
        const auto tl = EditorBuildTimeline();
        const double now = EditorPreviewNow();
        // 音源: 音源の範囲に入ったら (速度を変えた音源はできあがった時に) 今の位置から流す
        if (const int song = EditorActiveSong(); song != -1 && !Editor.SongStarted) {
            const double pos = EditorSongPosition(now);
            if (pos >= 0) {
                ChangeVolumeSoundMem((int)(255 * Config.SongVolume / 100), song);
                SetSoundCurrentTime((LONGLONG)pos, song);
                PlaySoundMem(song, DX_PLAYTYPE_BACK, FALSE);
                Editor.SongStarted = true;
            }
        }
        bool don = false, ka = false;
        for (const auto& n : tl.Notes) {
            if (n.Time <= Editor.PreviewLast) { continue; }
            if (n.Time > now) { break; }
            if (n.Type == '1' || n.Type == '3' || n.Type == '5' || n.Type == '6' || n.Type == '7' || n.Type == '9') { don = true; }
            else if (n.Type == '2' || n.Type == '4') { ka = true; }
        }
        if (don) { Skin.Base->Title.SE.Don.Play(); }
        if (ka) { Skin.Base->Title.SE.Ka.Play(); }
        Editor.PreviewLast = now;
        int m = 0;
        double pos = 0;
        if (tl.PosAt(now, m, pos)) {
            if (m < Editor.Top || m >= Editor.Top + EditorRows) { Editor.Top = std::clamp(m, 0, std::max(EditorMeasureCount() - 1, 0)); }
        }
        else if (!tl.Measures.empty() && now > tl.Measures.back().Map.back().second + 1000.0) {
            EditorStopPreview();    // 最後まで流した
        }
    }

    UIRect EditorPanelRect() const {
        return { EditorResX() * 0.18f, EditorResY() * 0.14f, EditorResX() * 0.82f, EditorResY() * 0.88f };
    }
    // パネルの行 (ヘッダー: 項目, 命令: テンプレート) の矩形
    UIRect EditorPanelRow(int i, int columns) const {
        const UIRect p = EditorPanelRect();
        const float lh = EditorLine() * 1.5f;
        const float colw = (p.R - p.L - 32.0f) / columns;
        const int col = i % columns, row = i / columns;
        return { p.L + 16.0f + colw * col, p.T + EditorLine() * 2.0f + lh * row, p.L + 16.0f + colw * (col + 1) - 8.0f, p.T + EditorLine() * 2.0f + lh * row + lh - 6.0f };
    }
    struct EditorHeaderField { const char* Key; bool CourseField; const char* Help; };
    static const std::vector<EditorHeaderField>& EditorHeaderFields() {
        static const std::vector<EditorHeaderField> fields = {
            { "TITLE", false, "曲名" }, { "SUBTITLE", false, "サブタイトル (--で始めると表示しない)" }, { "BPM", false, "最初の BPM" },
            { "WAVE", false, "音源ファイル (譜面からの相対パス)" }, { "SONGLINK", false, "音源をダウンロードする URL (取得ボタンで WAVE に保存)" },
            { "BGMOVIE", false, "背景動画ファイル (譜面からの相対パス)" }, { "MOVIELINK", false, "背景動画をダウンロードする URL (取得ボタンで BGMOVIE に保存)" },
            { "OFFSET", false, "音源の始まりと譜面のずれ (秒)" }, { "DEMOSTART", false, "選曲画面で流し始める位置 (秒)" },
            { "GENRE", false, "ジャンル" }, { "SONGVOL", false, "曲の音量" }, { "SEVOL", false, "効果音の音量" },
            { "COURSE", true, "コース (Easy / Normal / Hard / Oni / Edit)" }, { "LEVEL", true, "レベル" }, { "BALLOON", true, "風船の打数 (カンマ区切り)" },
            { "SCOREINIT", true, "初項" }, { "SCOREDIFF", true, "公差" },
        };
        return fields;
    }

    // ---- シーン ----
    void EditorInit() {
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
        SetMouseDispFlag(TRUE);
#endif
    }
    void EditorEnd() {
        EditorStopPreview();
#ifndef __ANDROID__
        if (Editor.Editing) {
            DeleteKeyInput(Editor.EditHandle);
            Editor.Editing = false;
        }
        if (!Editor.Testing) {
            SetMouseDispFlag(!Config.FullScreen);
        }
#endif
        if (Editor.Rescan && !Editor.Testing) {
            Editor.Rescan = false;
            EnumChart(Config.SongDirectories);
        }
    }

    void EditorDraw() {
        const FontData& font = ConfigFont();
        const FontData& sfont = EditorFont();
        const unsigned int white = GetColor(255, 255, 255), black = GetColor(0, 0, 0), gray = GetColor(150, 150, 150), accent = GetColor(255, 200, 40);
        const float resx = EditorResX(), resy = EditorResY(), lh = EditorLine();
        DrawBox(0, 0, (int)resx, (int)resy, GetColor(22, 24, 34), TRUE);

        if (!Editor.Open) {
            UIText(font, resx * 0.5f, resy * 0.25f, "譜面制作", white, 1);
            const char* labels[] = { "新しい譜面を作る…", "譜面を開く…", "戻る" };
            for (int i = 0; i < 3; ++i) {
                const UIRect b = EditorStartButton(i);
                UIBox(b, GetColor(50, 54, 70), 230);
                UIBox(b, white, 160, false);
                UIText(sfont, (b.L + b.R) * 0.5f, (b.T + b.B) * 0.5f, labels[i], white, 1);
            }
            UIText(sfont, resx * 0.5f, resy * 0.75f, FitText("選曲画面で F7 を押すと、選んでいる譜面を編集できます", sfont.Handle, resx - 32, false), gray, 1);
        }
        else {
            // ボタン
            const auto buttons = EditorButtons();
            for (const auto& g : EditorGroups) {
                UIBox(g.Rect, GetColor(34, 37, 50), 255);
                UIBox(g.Rect, GetColor(80, 86, 110), 255, false);
                UIText(sfont, g.Rect.L + 10.0f, (g.Rect.T + g.Rect.B) * 0.5f, g.Label, GetColor(170, 175, 195));
            }
            for (const auto& b : buttons) {
                UIBox(b.Rect, b.Active ? accent : GetColor(50, 54, 70), 230);
                UIBox(b.Rect, white, 120, false);
                UIText(sfont, (b.Rect.L + b.Rect.R) * 0.5f, (b.Rect.T + b.Rect.B) * 0.5f, b.Label, b.Active ? black : white, 1);
            }
            EditorDrawGrid(sfont);
        }

        // 下部: ファイル名・お知らせ・操作方法
        std::string bottom;
        std::string hoverhelp;
        if (Editor.Open && Editor.Panel == EditorState::PanelType::None) {
            for (const auto& b : EditorButtons()) {
                if (b.Rect.Contains(PointerPrev.X, PointerPrev.Y)) { hoverhelp = EditorButtonHelp(b.Label); break; }
            }
        }
        if (std::chrono::steady_clock::now() < Editor.MessageUntil) { bottom = Editor.Message; }
        else if (!hoverhelp.empty()) { bottom = hoverhelp; }
        else if (Editor.Open) {
            bottom = PathToU8(Editor.Path.filename()) + (Editor.Dirty ? " *" : "") + "   左クリック: 置く  右クリック: 消す  Shift+クリック: 範囲選択  Ctrl+C/X/V: コピー/切り取り/貼り付け  0-9: 種類  Space: 再生/停止  Ctrl+S: 保存  Ctrl+Z: 元に戻す  F5: テストプレイ  Esc: 閉じる";
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
            UIBox(p, GetColor(18, 20, 32), 240);
            UIBox(p, white, 200, false);
            if (Editor.Panel == EditorState::PanelType::Commands) {
                UIText(sfont, p.L + 16, p.T + lh, FitText("命令を挿入 (カーソルの位置: " + std::to_string(Editor.CursorMeasure + 1) + " 小節目 "
                    + std::to_string(Editor.CursorSlot) + "/" + std::to_string(Editor.Division) + ")  /  譜面の命令は クリック: 編集  右クリック: 削除", sfont.Handle, p.R - p.L - 32, false), white);
                const auto& list = EditorCommandTemplates();
                for (int i = 0; i < (int)list.size(); ++i) {
                    const UIRect r = EditorPanelRow(i, 4);
                    UIBox(r, GetColor(50, 54, 70), 230);
                    UIText(sfont, (r.L + r.R) * 0.5f, (r.T + r.B) * 0.5f, FitText(list[i].first, sfont.Handle, r.R - r.L - 8, false), white, 1);
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
                };
                float y = p.T + lh;
                for (const auto& line : lines) {
                    UIText(sfont, p.L + 16, y, FitText(line, sfont.Handle, p.R - p.L - 32, false), white);
                    y += lh * 1.4f;
                }
                const char* labels[] = { "適用 (Enter)", "キャンセル (Esc)" };
                for (int i = 0; i < 2; ++i) {
                    const UIRect b = EditorHeaderButton(i);
                    UIBox(b, i == 0 ? GetColor(255, 200, 40) : GetColor(60, 64, 84), 230);
                    UIText(sfont, (b.L + b.R) * 0.5f, (b.T + b.B) * 0.5f, labels[i], i == 0 ? black : white, 1);
                }
            }
            else if (Editor.Panel == EditorState::PanelType::Gimmick) {
                const EditorTiming t = EditorTimingAt(Editor.CursorMeasure);
                UIText(sfont, p.L + 16, p.T + lh, FitText("ギミックを挿入 (" + std::to_string(Editor.CursorMeasure + 1) + " 小節目の前 / BPM " + EditorNum(t.Bpm)
                    + " / 拍子 " + EditorFracText(t.Measure.Num, t.Measure.Den) + ")  クリックで数値を入れて挿入", sfont.Handle, p.R - p.L - 32, false), white);
                const auto& list = EditorGimmicks();
                int hover = -1;
                for (int i = 0; i < (int)list.size(); ++i) {
                    const UIRect r = EditorPanelRow(i, 3);
                    const bool over = r.Contains(PointerPrev.X, PointerPrev.Y);
                    if (over) { hover = i; }
                    UIBox(r, over ? GetColor(80, 86, 110) : GetColor(50, 54, 70), 230);
                    UIText(sfont, (r.L + r.R) * 0.5f, (r.T + r.B) * 0.5f, FitText(list[i].Name, sfont.Handle, r.R - r.L - 8, false), white, 1);
                }
                const std::string help = hover >= 0 ? std::string(list[hover].Help) : std::string("次郎形式 (Config の JiroChartMode) と #HBSCROLL で、太鼓さん次郎と同じ見え方になります");
                UIText(sfont, p.L + 16, p.B - lh * 1.2f, FitText(help, sfont.Handle, p.R - p.L - 32, false), gray);
            }
            else if (Editor.Panel == EditorState::PanelType::Grid) {
                UIText(sfont, p.L + 16, p.T + lh, FitText("グリッドの分割数  左クリック: この分割にする  右クリック: よく使う分割に入れる / 外す (★ が ◀ ▶ で切り替わる)", sfont.Handle, p.R - p.L - 32, false), white);
                const auto favorites = EditorGridList();
                for (int i = 0; i < (int)std::size(EditorDivisions); ++i) {
                    const UIRect r = EditorPanelRow(i, 4);
                    const int g = EditorDivisions[i];
                    const bool fav = std::ranges::find(favorites, g) != favorites.end();
                    const bool current = g == Editor.Division;
                    UIBox(r, current ? GetColor(255, 200, 40) : GetColor(50, 54, 70), 230);
                    UIText(sfont, (r.L + r.R) * 0.5f, (r.T + r.B) * 0.5f, (fav ? "★ " : "") + std::to_string(g) + " 分割", current ? black : white, 1);
                }
            }
            else {
                UIText(sfont, p.L + 16, p.T + lh, FitText("ヘッダー (クリックで編集。コースの項目は今のコースだけ)", sfont.Handle, p.R - p.L - 32, false), white);
                const auto& fields = EditorHeaderFields();
                for (int i = 0; i < (int)fields.size(); ++i) {
                    const UIRect r = EditorPanelRow(i, 2);
                    const auto& f = fields[i];
                    const std::string value = tja::Document::GetValue(f.CourseField ? EditorCourse()->Header : Editor.Doc.Header, f.Key);
                    UIBox(r, GetColor(36, 40, 54), 230);
                    UIText(sfont, r.L + 8, (r.T + r.B) * 0.5f, f.Key, f.CourseField ? GetColor(160, 210, 255) : white);
                    const float vx = r.L + (r.R - r.L) * 0.28f;
                    const bool button = std::string(f.Key) == "WAVE" || std::string(f.Key) == "SONGLINK" || std::string(f.Key) == "MOVIELINK";
                    const float vr = button ? r.R - EditorTextWidth("参照…") - 28.0f : r.R - 8.0f;
                    UIText(sfont, vx, (r.T + r.B) * 0.5f, FitText(value.empty() ? std::string(f.Help) : value, sfont.Handle, vr - vx, false), value.empty() ? gray : white);
                    if (button) {
                        const UIRect b = EditorWaveButton(i);
                        UIBox(b, GetColor(60, 64, 84), 230);
                        UIText(sfont, (b.L + b.R) * 0.5f, (b.T + b.B) * 0.5f, std::string(f.Key) == "WAVE" ? "参照…" : "取得", white, 1);
                    }
                }
                const char* labels[] = { "コースを追加", "このコースを削除" };
                for (int i = 0; i < 2; ++i) {
                    const UIRect b = EditorHeaderButton(i);
                    UIBox(b, GetColor(60, 64, 84), 230);
                    UIText(sfont, (b.L + b.R) * 0.5f, (b.T + b.B) * 0.5f, labels[i], white, 1);
                }
            }
        }
#ifndef __ANDROID__
        if (Editor.Editing) {
            const UIRect p{ resx * 0.15f, resy * 0.4f, resx * 0.85f, resy * 0.4f + lh * 3.2f };
            UIBox({ 0, 0, resx, resy }, black, 120);
            UIBox(p, GetColor(18, 20, 32), 245);
            UIBox(p, accent, 255, false);
            UIText(sfont, p.L + 16, p.T + lh * 0.8f, FitText(Editor.EditLabel + "  (Enter: 決定 / Esc: キャンセル)", sfont.Handle, p.R - p.L - 32, false), white);
            SetDrawArea((int)p.L + 12, (int)p.T, (int)p.R - 12, (int)p.B);
            DrawKeyInputString((int)p.L + 16, (int)(p.T + lh * 1.7f), Editor.EditHandle);
            SetDrawAreaFull();
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
    }
    UIRect EditorStartButton(int i) const {
        const float w = std::min(420.0f, EditorResX() - 64.0f), h = EditorLine() * 1.8f;
        const float cx = EditorResX() * 0.5f, y = EditorResY() * 0.38f + (h + 14.0f) * i;
        return { cx - w * 0.5f, y, cx + w * 0.5f, y + h };
    }
    UIRect EditorWaveButton(int row) const {
        const UIRect r = EditorPanelRow(row, 2);
        return { r.R - EditorTextWidth("参照…") - 24.0f, r.T + 2, r.R - 4, r.B - 2 };
    }
    UIRect EditorHeaderButton(int i) const {
        const UIRect p = EditorPanelRect();
        const float w = (p.R - p.L - 48.0f) / 2.0f, h = EditorLine() * 1.4f;
        const float x = p.L + 16.0f + (w + 16.0f) * i;
        return { x, p.B - h - 12.0f, x + w, p.B - 12.0f };
    }

    static unsigned int EditorNoteColor(char type) {
        switch (type) {
        case '1': case '3': return GetColor(235, 72, 52);
        case '2': case '4': return GetColor(64, 170, 235);
        case '5': case '6': return GetColor(250, 200, 40);
        case '7': case '9': return GetColor(250, 130, 40);
        case '8': return GetColor(200, 200, 200);
        default: return GetColor(200, 120, 255);
        }
    }
    // スキンの音符画像 (Playing.Image.Note の分割) の番号: 1 ドン 2 カッ 3 大ドン 4 大カッ 5 連打 6 連打の帯 7 連打の終わり
    // 8 大連打 9 大連打の帯 10 大連打の終わり 11 風船 13 くすだま
    static int EditorNoteImage(char type) {
        switch (type) {
        case '1': return 1; case '2': return 2; case '3': return 3; case '4': return 4;
        case '5': return 5; case '6': return 8; case '7': return 11; case '8': return 7; case '9': return 13;
        default: return -1;
        }
    }
    // 音符の画像を枠の高さに合わせた大きさで描く (画像の 1 コマは大音符が収まる大きさなので、レーンの高さの 1.2 倍にする)
    bool EditorDrawSkinNote(float x, float cy, float lane_h, int index, int alpha) {
        const auto& img = Skin.Base->Playing.Image.Note;
        if (index < 0 || index >= (int)img.Handles.size() || img.Size.Height <= 0) { return false; }
        const float h = lane_h * 1.2f, w = h * img.Size.Width / img.Size.Height;
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
        DrawExtendGraphF(x - w * 0.5f, cy - h * 0.5f, x + w * 0.5f, cy + h * 0.5f, img.Handles[index], TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        return true;
    }
    void EditorDrawNote(float x, float cy, float lane_h, char type, int alpha) {
        if (EditorDrawSkinNote(x, cy, lane_h, EditorNoteImage(type), alpha)) { return; }
        const bool big = type == '3' || type == '4' || type == '6' || type == '9';
        const float r = lane_h * (big ? 0.42f : 0.3f) * (type == '8' ? 0.6f : 1.0f);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
        DrawCircleAA(x, cy, r, 32, EditorNoteColor(type), TRUE);
        DrawCircleAA(x, cy, r, 32, GetColor(255, 255, 255), FALSE, 2.0f);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
    // 連打の帯 (スキンの帯の画像を伸ばす。無ければ単色)
    void EditorDrawRollBody(float x0, float x1, float cy, float lane_h, char type) {
        const auto& img = Skin.Base->Playing.Image.Note;
        const int body = type == '6' ? 9 : 6;
        if (x1 > x0 && body < (int)img.Handles.size() && img.Size.Height > 0 && (type == '5' || type == '6')) {
            const float h = lane_h * 1.2f;
            DrawExtendGraphF(x0, cy - h * 0.5f, x1, cy + h * 0.5f, img.Handles[body], TRUE);
            return;
        }
        UIBox({ x0, cy - lane_h * 0.22f, x1, cy + lane_h * 0.22f }, EditorNoteColor(type), 120);
    }
    void EditorDrawGrid(const FontData& sfont) {
        const auto* course = EditorCourse();
        if (course == nullptr) { return; }
        const unsigned int white = GetColor(255, 255, 255), gray = GetColor(150, 150, 150), accent = GetColor(255, 200, 40);
        const int count = (int)course->Measures.size();

        // 表示より前の小節で始まった連打と、各小節の頭の BPM
        double bpm = 120;
        if (const auto v = tja::Document::GetValue(Editor.Doc.Header, "BPM"); !v.empty()) { bpm = std::atof(v.c_str()); }
        bool rolling = false;
        char rolltype = '5';
        for (int m = 0; m < std::min(Editor.Top, count); ++m) {
            for (const auto& c : course->Measures[m].Commands) {
                if (tja::StartsWithKey(c.Text, "#BPMCHANGE")) { bpm = EditorCommandValue(c.Text, 10); }
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
            const int m = Editor.Top + row;
            if (m >= count) { break; }
            const auto& measure = course->Measures[m];
            const UIRect lane = EditorLane(row);
            const float cy = (lane.T + lane.B) * 0.5f, lh = lane.B - lane.T;

            // 小節の頭の #BPMCHANGE はこの小節の BPM として表示する
            for (const auto& c : measure.Commands) {
                if (c.Num == 0 && tja::StartsWithKey(c.Text, "#BPMCHANGE")) { bpm = EditorCommandValue(c.Text, 10); }
            }
            const float labelx = EditorGridArea().L;
            UIText(sfont, labelx, lane.T + sfont.Size * 0.6f, std::to_string(m + 1), m == Editor.CursorMeasure ? accent : white);
            UIText(sfont, labelx, lane.B - sfont.Size * 0.6f, FitText("BPM " + FormatNumber(bpm, bpm == std::floor(bpm) ? 0 : 2), sfont.Handle, lane.L - labelx - 8.0f, false), gray);

            UIBox(lane, GetColor(50, 50, 58), 255);
            // スキンのレーンの画像を敷く (無ければ単色)
            if (const auto& laneimg = Skin.Base->Playing.Image.Lane; !laneimg.Handles.empty()) {
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
                DrawExtendGraphF(lane.L, lane.T, lane.R, lane.B, laneimg.Handles[0], TRUE);
                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            }
            // グリッド (拍の線を明るく)
            const int beat = Editor.Division % 4 == 0 ? Editor.Division / 4 : 0;
            for (int k = 0; k <= Editor.Division; ++k) {
                const float x = EditorX(lane, (double)k / Editor.Division);
                const bool strong = k == 0 || k == Editor.Division;
                const bool beatline = beat > 0 && k % beat == 0;
                DrawLineAA(x, lane.T, x, lane.B, strong ? white : beatline ? GetColor(130, 130, 150) : GetColor(80, 80, 92), strong ? 2.0f : 1.0f);
            }
            if (Editor.Selecting) {
                const auto [a, b] = EditorSelection();
                if (a.Measure <= m && m <= b.Measure) {
                    const double from = m == a.Measure ? (double)a.Num / a.Den : 0.0;
                    const double to = m == b.Measure ? (double)b.Num / b.Den + 0.5 / Editor.Division : 1.0;
                    UIBox({ EditorX(lane, from), lane.T, EditorX(lane, std::min(to, 1.0)), lane.B }, GetColor(90, 150, 255), 90);
                }
            }
            if (m == Editor.CursorMeasure) {
                const float x = EditorX(lane, (double)Editor.CursorSlot / Editor.Division);
                DrawLineAA(x, lane.T - 4, x, lane.B + 4, accent, 3.0f);
            }

            // 連打の帯
            float rollstart = rolling ? lane.L : -1.0f;
            for (const auto& n : measure.Notes) {
                const float x = EditorX(lane, (double)n.Num / n.Den);
                if (n.Type == '5' || n.Type == '6' || n.Type == '7' || n.Type == '9') { rolling = true; rolltype = n.Type; rollstart = x; }
                else if (n.Type == '8' && rolling) {
                    EditorDrawRollBody(rollstart, x, cy, lh, rolltype);
                    rolling = false;
                    rollstart = -1.0f;
                }
            }
            if (rolling && rollstart >= 0) { EditorDrawRollBody(rollstart, lane.R, cy, lh, rolltype); }

            for (const auto& n : measure.Notes) {
                const float x = EditorX(lane, (double)n.Num / n.Den);
                EditorDrawNote(x, cy, lh, n.Type, 255);
                if (n.Type < '1' || n.Type > '9') { UIText(sfont, x, cy, std::string(1, n.Type), white, 1); }
            }
            if (hm == m && hs >= 0 && !Editor.Editing && Editor.Panel == EditorState::PanelType::None) {
                const float x = EditorX(lane, (double)hs / Editor.Division);
                if (Editor.Palette == '0') { DrawLineAA(x - 8, cy - 8, x + 8, cy + 8, white, 2.0f); DrawLineAA(x - 8, cy + 8, x + 8, cy - 8, white, 2.0f); }
                else { EditorDrawNote(x, cy, lh, Editor.Palette, 110); }
            }
            for (const auto& c : measure.Commands) {
                if (c.Num != 0 && tja::StartsWithKey(c.Text, "#BPMCHANGE")) { bpm = EditorCommandValue(c.Text, 10); }
            }
        }
        // 疑似再生の位置
        if (Editor.Preview) {
            const auto tl = EditorBuildTimeline();
            int pm = 0;
            double pp = 0;
            if (tl.PosAt(EditorPreviewNow(), pm, pp) && pm >= Editor.Top && pm < Editor.Top + EditorRows) {
                const UIRect lane = EditorLane(pm - Editor.Top);
                const float x = EditorX(lane, pp);
                DrawLineAA(x, lane.T - 6, x, lane.B + 6, GetColor(80, 255, 140), 3.0f);
            }
        }
        // 命令のラベル
        for (const auto& hit : EditorCommandRects()) {
            const auto& cmd = course->Measures[hit.Measure].Commands[hit.Index];
            UIBox(hit.Rect, GetColor(70, 60, 110), 230);
            UIText(sfont, hit.Rect.L + 6, (hit.Rect.T + hit.Rect.B) * 0.5f, FitText(cmd.Text, sfont.Handle, hit.Rect.R - hit.Rect.L - 10, false), white);
            DrawLineAA(hit.Rect.L, hit.Rect.B, hit.Rect.L, EditorLane(hit.Measure - Editor.Top).T, GetColor(150, 130, 220), 1.0f);
        }
        // スクロールバー
        const UIRect area = EditorGridArea();
        if (count > EditorRows) {
            const float h = area.B - area.T;
            UIBox({ area.R - 4, area.T + h * Editor.Top / count, area.R, area.T + h * std::min(count, Editor.Top + EditorRows) / count }, white, 160);
        }
    }
    // マウスの下の小節とグリッドの位置 (無ければ false)
    bool EditorHitLane(int x, int y, int& measure, int& slot) {
        measure = slot = -1;
        for (int row = 0; row < EditorRows; ++row) {
            const UIRect lane = EditorLane(row);
            const UIRect hit{ lane.L - 8, lane.T - 4, lane.R + 8, lane.B + 4 };
            if (!hit.Contains(x, y) || Editor.Top + row >= EditorMeasureCount()) { continue; }
            measure = Editor.Top + row;
            slot = std::clamp((int)std::lround((x - lane.L) / (lane.R - lane.L) * Editor.Division), 0, Editor.Division - 1);
            return true;
        }
        return false;
    }

    void EditorProc() {
        auto Back = [&] { NowScene = Scene::ModeSelect; };
        EditorUpdatePreview();

#ifndef __ANDROID__
        // ---- ファイル選択ダイアログ ----
        if (Editor.Picker) {
            if (!Editor.Picker->Done) { return; }
            const std::wstring result = Editor.Picker->Result;
            const int kind = Editor.PickerKind;
            Editor.Picker.reset();
            PointerPrev.Left = PointerPrev.Right = true;
            Input.ResyncKeys();
            if (kind == 1 && !result.empty()) { EditorOpen(fs::path(result)); }
            else if (kind == 2 && !result.empty()) {
                Editor.NewPath = fs::path(result);
                EditorPick(3, false, true, Editor.NewPath.parent_path(), L"");     // 続けて音源を選ぶ (キャンセルで音源なし)
            }
            else if (kind == 3) { EditorNewChart(Editor.NewPath, result.empty() ? fs::path() : fs::path(result)); }
            else if (kind == 4 && !result.empty() && EditorCourse() != nullptr) {
                EditorChange();
                tja::Document::SetValue(Editor.Doc.Header, "WAVE", EditorWaveValue(fs::path(result), Editor.Path.parent_path()));
            }
            return;
        }
#endif
        // ---- 自動推定の解析中 ----
        if (Editor.Estimating) {
            if (Editor.EstimateTask.valid() && Editor.EstimateTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                Editor.Estimate = Editor.EstimateTask.get();
                Editor.Estimating = false;
                if (Editor.Estimate.Ok) { Editor.Panel = EditorState::PanelType::Estimate; }
                else { EditorMessage("BPM を推定できませんでした (曲が短いか、拍がはっきりしません)"); }
                PointerPrev.Left = PointerPrev.Right = true;
            }
            return;
        }
        const PointerState pt = ReadPointer();

#ifndef __ANDROID__
        // ---- 文字入力中 ----
        if (Editor.Editing) {
            const int state = CheckKeyInput(Editor.EditHandle);
            if (state != 0) {
                if (state == 1) {
                    std::vector<char> buf(4096, '\0');
                    GetKeyInputString(buf.data(), Editor.EditHandle);
                    if (Editor.OnEdit) { Editor.OnEdit(buf.data()); }
                }
                DeleteKeyInput(Editor.EditHandle);
                Editor.EditHandle = 0;
                Editor.Editing = false;
                Editor.OnEdit = nullptr;
                Input.ResyncKeys();
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
                if (i == 2) { Back(); }
#endif
            };
            for (int i = 0; i < 3; ++i) {
                if (pt.LeftPressed && EditorStartButton(i).Contains(pt.X, pt.Y)) { Start(i); return; }
            }
#ifndef __ANDROID__
            Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, Back);
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
                Input.HitKeyProcess(VK_RETURN, KeyState::Down, [&] { EditorApplyEstimate(); Editor.Panel = EditorState::PanelType::None; });
                Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] { Editor.Panel = EditorState::PanelType::None; });
#endif
                return;
            }
            if (Editor.Panel == EditorState::PanelType::Grid) {
                for (int i = 0; i < (int)std::size(EditorDivisions); ++i) {
                    if (!EditorPanelRow(i, 4).Contains(pt.X, pt.Y)) { continue; }
                    const int g = EditorDivisions[i];
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
                    if (pt.LeftPressed && EditorPanelRow(i, 3).Contains(pt.X, pt.Y)) {
                        Editor.Panel = EditorState::PanelType::None;
                        if (list[i].Default[0] == '\0') { EditorInsertGimmick(i, ""); }
                        else {
                            EditorBeginEdit(std::string(list[i].Name) + ": " + list[i].Params + " (空白で区切る)", list[i].Default, [this, i](const std::string& v) { EditorInsertGimmick(i, v); });
                        }
                        return;
                    }
                }
            }
            else if (Editor.Panel == EditorState::PanelType::Commands) {
                const auto& list = EditorCommandTemplates();
                for (int i = 0; i < (int)list.size(); ++i) {
                    if (pt.LeftPressed && EditorPanelRow(i, 4).Contains(pt.X, pt.Y)) {
                        Editor.Panel = EditorState::PanelType::None;
                        const std::string text = list[i].second;
                        if (text.find(' ') == std::string::npos && text != "#") { EditorInsertCommand(text); }
                        else { EditorBeginEdit("命令を挿入", text, [this](const std::string& s) { EditorInsertCommand(s); }); }
                        return;
                    }
                }
            }
            else if (Editor.Panel == EditorState::PanelType::Header) {
                const auto& fields = EditorHeaderFields();
                for (int i = 0; i < (int)fields.size(); ++i) {
                    const auto f = fields[i];
#ifndef __ANDROID__
                    if (pt.LeftPressed && std::string(f.Key) == "WAVE" && EditorWaveButton(i).Contains(pt.X, pt.Y)) {
                        EditorPick(4, false, true, Editor.Path.parent_path(), L"");
                        return;
                    }
                    if (pt.LeftPressed && std::string(f.Key) == "SONGLINK" && EditorWaveButton(i).Contains(pt.X, pt.Y)) {
                        EditorFetchSong();
                        return;
                    }
                    if (pt.LeftPressed && std::string(f.Key) == "MOVIELINK" && EditorWaveButton(i).Contains(pt.X, pt.Y)) {
                        EditorFetchMovie();
                        return;
                    }
#endif
                    if (pt.LeftPressed && EditorPanelRow(i, 2).Contains(pt.X, pt.Y)) {
                        auto& lines = f.CourseField ? course->Header : Editor.Doc.Header;
                        EditorBeginEdit(f.Key, tja::Document::GetValue(lines, f.Key), [this, f](const std::string& s) {
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
            Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] { Editor.Panel = EditorState::PanelType::None; });
#endif
            return;
        }

        // ---- ボタン ----
        if (pt.LeftPressed) {
            for (auto& b : EditorButtons()) {
                if (b.Rect.Contains(pt.X, pt.Y)) {
                    Skin.Base->Title.SE.Ka.Play();
                    b.Action();
                    return;
                }
            }
        }

        // ---- 命令のラベル: クリックで編集、右クリックで削除 ----
        for (const auto& hit : EditorCommandRects()) {
            if (!hit.Rect.Contains(pt.X, pt.Y)) { continue; }
            if (pt.RightPressed) {
                EditorChange();
                auto& cmds = course->Measures[hit.Measure].Commands;
                EditorMessage("命令を削除しました: " + cmds[hit.Index].Text);
                cmds.erase(cmds.begin() + hit.Index);
                return;
            }
            if (pt.LeftPressed) {
                const int m = hit.Measure, i = hit.Index;
                EditorBeginEdit("命令を編集 (空欄で削除)", course->Measures[m].Commands[i].Text, [this, m, i](const std::string& s) {
                    auto& cmds = EditorCourse()->Measures[m].Commands;
                    if (i >= (int)cmds.size()) { return; }
                    EditorChange();
                    const std::string t(strtrim(s));
                    if (t.empty()) { cmds.erase(cmds.begin() + i); }
                    else { cmds[i].Text = t; }
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
        if (EditorHitLane(pt.X, pt.Y, hm, hs) && pt.LeftPressed && shift) {
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
        if (EditorHitLane(pt.X, pt.Y, hm, hs)) {
            if (pt.LeftPressed || pt.RightPressed) {
                Editor.Selecting = false;
                Editor.CursorMeasure = hm;
                Editor.CursorSlot = hs;
                auto& measure = course->Measures[hm];
                const auto* existing = tja::Document::NoteAt(measure, hs, Editor.Division);
                const char type = pt.RightPressed ? '0' : (existing != nullptr && existing->Type == Editor.Palette ? '0' : Editor.Palette);
                if (!(type == '0' && existing == nullptr)) {
                    EditorChange();
                    tja::Document::SetNote(measure, hs, Editor.Division, type);
                    if (type != '0') { (type == '2' || type == '4' ? Skin.Base->Playing.SE.Ka : Skin.Base->Playing.SE.Don).Play(); }
                }
                return;
            }
        }
        if (pt.Wheel != 0) {
            Editor.Top = std::clamp(Editor.Top - pt.Wheel, 0, std::max(EditorMeasureCount() - 1, 0));
        }

        // ---- キーボード ----
#ifndef __ANDROID__
        auto PlaceAtCursor = [&](char type) {
            auto& measure = course->Measures[Editor.CursorMeasure];
            if (type == '0' && tja::Document::NoteAt(measure, Editor.CursorSlot, Editor.Division) == nullptr) { return; }
            EditorChange();
            tja::Document::SetNote(measure, Editor.CursorSlot, Editor.Division, type);
        };
        auto MoveSlot = [&](int dir) {
            Editor.CursorSlot += dir;
            if (Editor.CursorSlot < 0) {
                if (Editor.CursorMeasure > 0) { --Editor.CursorMeasure; Editor.CursorSlot = Editor.Division - 1; }
                else { Editor.CursorSlot = 0; }
            }
            if (Editor.CursorSlot >= Editor.Division) {
                if (Editor.CursorMeasure + 1 < EditorMeasureCount()) { ++Editor.CursorMeasure; Editor.CursorSlot = 0; }
                else { Editor.CursorSlot = Editor.Division - 1; }
            }
            EditorClamp();
        };
        if (ctrl) {
            Input.HitKeyProcess('S', KeyState::Down, [&] { EditorSave(); });
            Input.HitKeyProcess('Z', KeyState::Down, [&] { EditorUndo(false); });
            Input.HitKeyProcess('Y', KeyState::Down, [&] { EditorUndo(true); });
            Input.HitKeyProcess('C', KeyState::Down, [&] { EditorCopy(false); });
            Input.HitKeyProcess('X', KeyState::Down, [&] { if (EditorCopy(true)) { Editor.Selecting = false; } });
            Input.HitKeyProcess('V', KeyState::Down, [&] { EditorPaste(); });
            return;
        }
        // Shift+矢印で範囲を選ぶ (矢印だけなら選択を解除して移動)
        auto Select = [&] { if (shift) { EditorBeginSelection(); } else { Editor.Selecting = false; } };
        for (char c = '0'; c <= '9'; ++c) {
            Input.HitKeyProcess(c, KeyState::Down, [&] { Editor.Palette = c; });
            Input.HitKeyProcess(VK_NUMPAD0 + (c - '0'), KeyState::Down, [&] { Editor.Palette = c; });
        }
        Input.HitKeyProcess(VK_UP, KeyState::Down, [&] { Select(); --Editor.CursorMeasure; EditorClamp(); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_DOWN, KeyState::Down, [&] { Select(); ++Editor.CursorMeasure; EditorClamp(); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_LEFT, KeyState::Down, [&] { Select(); MoveSlot(-1); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_RIGHT, KeyState::Down, [&] { Select(); MoveSlot(1); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_RETURN, KeyState::Down, [&] { PlaceAtCursor(Editor.Palette); MoveSlot(1); });
        Input.HitKeyProcess(VK_DELETE, KeyState::Down, [&] { PlaceAtCursor('0'); });
        Input.HitKeyProcess(VK_F5, KeyState::Down, [&] { EditorTestPlay(); });
        Input.HitKeyProcess(VK_SPACE, KeyState::Down, [&] { EditorTogglePreview(); });
        Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] { EditorClose(); });
#else
        Touch.Process(TouchType::Other, [&] { EditorClose(); }, Skin.Base->ConfigMenu.Image.Back);
#endif
    }

    // ===================================================================== 録画 (Library/PlayRecorder.hpp)
#ifndef __ANDROID__
    struct RecorderState {
        bool Active = false;
        rec::VideoWriter Video;
        rec::AudioCapture Audio;
        bool HasAudio = false;
        fs::path Ffmpeg, VideoPath, AudioPath, OutPath;
        int Fps = 60, Width = 0, Height = 0;
        bool Bgra = false;
        ID3D11DeviceContext* Context = nullptr;
        ID3D11Texture2D* BackBuffer = nullptr;
        ID3D11Texture2D* Staging[2]{};
        ID3D11Texture2D* Resolve = nullptr;     // バックバッファがマルチサンプルの時
        int Cur = 0;
        bool Pending = false;                   // 前回コピーしたフレームをまだ読んでいない
        int PendingCount = 0;
        uint64_t Scheduled = 0;                 // 書くと決めたフレーム数
        std::chrono::steady_clock::time_point StartedAt{};
        std::thread Muxer;
        std::atomic_bool Muxing = false;
        std::mutex StatusMutex;
        std::string Status;
        std::chrono::steady_clock::time_point StatusUntil{};
        // 譜面から音声を作るための記録 (録画の開始からの秒)
        std::vector<std::pair<double, int>> SeLog;      // 効果音 (0: ドン 1: カッ 2: 風船)
        bool SongStarted = false;
        double SongAt = 0, SongPos = 0, SongSpeed = 1.0;
        float SongGain = 1, SeGain = 1;
        fs::path SongPath, SePath[3];
        double Duration = 0;
        // 保存の確認 (リザルトで聞く)
        std::thread Finisher;                   // 映像のエンコードの終わりを待つ
        std::atomic_bool VideoOk = false;
        bool AwaitingDecision = false;
        bool AbortRequested = false;            // 途中退出・リロード: 止めた時に保存しない
        double AudioDelay = 0;
        size_t Dropped = 0;
    } Recorder;

    double RecorderSeconds() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - Recorder.StartedAt).count();
    }
    // 演奏の効果音を鳴らす (録画中は時刻を記録して、譜面から音声を作る時に使う)
    void PlayHitSe(int kind) {
        auto& se = Skin.Base->Playing.SE;
        (kind == 0 ? se.Don : kind == 1 ? se.Ka : se.Balloon).Play();
        if (Recorder.Active) { Recorder.SeLog.push_back({ RecorderSeconds(), kind }); }
    }
    void RecorderNoteSong(double posms) {
        if (!Recorder.Active || Recorder.SongStarted) { return; }
        Recorder.SongStarted = true;
        Recorder.SongAt = RecorderSeconds();
        Recorder.SongPos = posms / 1000.0;
    }

    void RecorderStatus(std::string text, int seconds = 6) {
        std::lock_guard lock(Recorder.StatusMutex);
        Recorder.Status = std::move(text);
        Recorder.StatusUntil = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    }
    void RecorderRelease() {
        for (auto*& t : Recorder.Staging) { if (t) { t->Release(); t = nullptr; } }
        if (Recorder.Resolve) { Recorder.Resolve->Release(); Recorder.Resolve = nullptr; }
        Recorder.Context = nullptr;
        Recorder.BackBuffer = nullptr;
    }
    void RecorderStart() {
        if (Recorder.Active || !Config.RecordPlay || Editor.Testing) { return; }
        if (Recorder.AwaitingDecision) { RecorderDecide(true); }     // 前の録画の確認が残っていれば保存する
        if (Recorder.Muxer.joinable()) { Recorder.Muxer.join(); }     // 前の録画の書き出しを待つ
        Recorder.SeLog.clear();
        Recorder.SongStarted = false;
        Recorder.AbortRequested = false;
        Recorder.SongPath = Chart.OriginalData.SongPath;
        Recorder.SongSpeed = Chart.SongSpeed > 0 ? Chart.SongSpeed : 1.0;
        Recorder.SongGain = (float)(Chart.OriginalData.SongVolume * (Config.SongVolume / 100.0) / 100.0);
        Recorder.SeGain = (float)(Chart.OriginalData.SeVolume * (Config.SEVolume / 100.0) / 100.0);
        Recorder.SePath[0] = U8ToPath(Skin.Base->Playing.SE.Don.FilePath);
        Recorder.SePath[1] = U8ToPath(Skin.Base->Playing.SE.Ka.FilePath);
        Recorder.SePath[2] = U8ToPath(Skin.Base->Playing.SE.Balloon.FilePath);
        Recorder.Ffmpeg = rec::FindFfmpeg(GetExecutablePath());
        if (Recorder.Ffmpeg.empty()) { RecorderStatus("ffmpeg が見つからないため録画できません (Config の「ツール」から導入できます)"); return; }
        auto* device = (ID3D11Device*)GetUseDirect3D11Device();
        Recorder.Context = (ID3D11DeviceContext*)GetUseDirect3D11DeviceContext();
        Recorder.BackBuffer = (ID3D11Texture2D*)GetUseDirect3D11BackBufferTexture2D();
        if (device == nullptr || Recorder.Context == nullptr || Recorder.BackBuffer == nullptr) { RecorderStatus("Direct3D 11 で動いていないため録画できません"); RecorderRelease(); return; }
        D3D11_TEXTURE2D_DESC desc{};
        Recorder.BackBuffer->GetDesc(&desc);
        if (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) { Recorder.Bgra = false; }
        else if (desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || desc.Format == DXGI_FORMAT_B8G8R8X8_UNORM) { Recorder.Bgra = true; }
        else { RecorderStatus("画面の形式に対応していないため録画できません"); RecorderRelease(); return; }
        Recorder.Width = (int)desc.Width;
        Recorder.Height = (int)desc.Height;
        D3D11_TEXTURE2D_DESC sd = desc;
        sd.MipLevels = 1;
        sd.ArraySize = 1;
        sd.SampleDesc = { 1, 0 };
        sd.Usage = D3D11_USAGE_STAGING;
        sd.BindFlags = 0;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        sd.MiscFlags = 0;
        bool ok = SUCCEEDED(device->CreateTexture2D(&sd, nullptr, &Recorder.Staging[0])) && SUCCEEDED(device->CreateTexture2D(&sd, nullptr, &Recorder.Staging[1]));
        if (ok && desc.SampleDesc.Count > 1) {
            D3D11_TEXTURE2D_DESC rd = desc;
            rd.SampleDesc = { 1, 0 };
            rd.BindFlags = 0;
            rd.MiscFlags = 0;
            ok = SUCCEEDED(device->CreateTexture2D(&rd, nullptr, &Recorder.Resolve));
        }
        if (!ok) { RecorderStatus("録画の準備に失敗しました"); RecorderRelease(); return; }

        // 保存先: recordings/日時_曲名.mp4
        std::error_code ec;
        const fs::path dir = GetExecutablePath() / "recordings";
        fs::create_directories(dir, ec);
        char stamp[32];
        const std::time_t now = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &now);
        std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm);
        std::string title = Chart.OriginalData.Title;
        for (char& c : title) { if (std::string_view("\\/:*?\"<>|").find(c) != std::string_view::npos || (unsigned char)c < 0x20) { c = '_'; } }
        if (title.size() > 60) { title.resize(60); }
        const std::string name = std::string(stamp) + (title.empty() ? "" : "_" + title);
        Recorder.OutPath = dir / U8ToPath(name + ".mp4");
        Recorder.VideoPath = dir / U8ToPath(name + ".video.mp4");
        Recorder.AudioPath = dir / U8ToPath(name + ".wav");
        Recorder.Fps = std::clamp(Config.RecordFps, 15, 120);
        if (!Recorder.Video.Start(Recorder.Ffmpeg, Recorder.VideoPath, Recorder.Width, Recorder.Height, Recorder.Fps, Recorder.Bgra, Config.RecordCodec, std::clamp(Config.RecordCrf, 0, 51))) {
            RecorderStatus("ffmpeg を起動できませんでした");
            RecorderRelease();
            return;
        }
        Recorder.HasAudio = Recorder.Audio.Start(Recorder.AudioPath);
        Recorder.StartedAt = std::chrono::steady_clock::now();
        Recorder.Scheduled = 0;
        Recorder.Pending = false;
        Recorder.Cur = 0;
        Recorder.Active = true;
    }
    void RecorderReadPending() {
        if (!Recorder.Pending) { return; }
        ID3D11Texture2D* tex = Recorder.Staging[Recorder.Cur ^ 1];
        D3D11_MAPPED_SUBRESOURCE map{};
        if (SUCCEEDED(Recorder.Context->Map(tex, 0, D3D11_MAP_READ, 0, &map))) {
            const size_t row = (size_t)Recorder.Width * 4;
            std::vector<uint8_t> frame(row * Recorder.Height);
            for (int y = 0; y < Recorder.Height; ++y) {
                std::memcpy(frame.data() + row * y, (const uint8_t*)map.pData + (size_t)map.RowPitch * y, row);
            }
            Recorder.Context->Unmap(tex, 0);
            Recorder.Video.Push(std::move(frame), Recorder.PendingCount);
        }
        Recorder.Pending = false;
    }
    // Draw の後、ScreenFlip の前に呼ぶ。録画のフレームレートの時刻を過ぎた時だけ GPU でコピーし、前回のコピーを読む
    void RecorderCapture() {
        if (!Recorder.Active) { return; }
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - Recorder.StartedAt).count();
        const uint64_t target = (uint64_t)(elapsed * Recorder.Fps) + 1;
        if (target <= Recorder.Scheduled) { return; }
        const int count = (int)std::min<uint64_t>(target - Recorder.Scheduled, (uint64_t)Recorder.Fps);   // 止まっていた分は同じ画で埋める
        Recorder.Scheduled = target;
        RenderVertex();     // DxLib の描画待ちを先に出し切る
        ID3D11Texture2D* dst = Recorder.Staging[Recorder.Cur];
        if (Recorder.Resolve) {
            D3D11_TEXTURE2D_DESC desc{};
            Recorder.BackBuffer->GetDesc(&desc);
            Recorder.Context->ResolveSubresource(Recorder.Resolve, 0, Recorder.BackBuffer, 0, desc.Format);
            Recorder.Context->CopyResource(dst, Recorder.Resolve);
        }
        else {
            Recorder.Context->CopyResource(dst, Recorder.BackBuffer);
        }
        RecorderReadPending();          // 1 つ前にコピーしたフレーム (もう GPU の処理は終わっている)
        Recorder.Pending = true;
        Recorder.PendingCount = count;
        Recorder.Cur ^= 1;
    }
    void RecorderStop() {
        if (!Recorder.Active) { return; }
        RecorderReadPending();          // 最後にコピーしたフレームを読む
        Recorder.Active = false;
        RecorderRelease();
        Recorder.Duration = RecorderSeconds();
        if (Recorder.HasAudio) { Recorder.Audio.Stop(); }
        Recorder.AudioDelay = Recorder.HasAudio ? std::chrono::duration<double>(Recorder.Audio.StartedAt() - Recorder.StartedAt).count() : 0.0;
        Recorder.Dropped = Recorder.Video.Dropped();
        // 映像のエンコードの残りは別スレッドで待つ (その間にリザルトで保存するか聞く)
        Recorder.VideoOk = false;
        if (Recorder.Finisher.joinable()) { Recorder.Finisher.join(); }
        Recorder.Finisher = std::thread([this] { Recorder.VideoOk = Recorder.Video.Finish(); });
        if (Recorder.AbortRequested) {
            Recorder.AbortRequested = false;
            RecorderDecide(false);      // 途中退出・リロード: 中断して保存しない
        }
        else if (NowScene == Scene::Result) {
            Recorder.AwaitingDecision = true;
        }
        else {
            RecorderDecide(true);       // 段位の次の曲へ進んだ時などは確認せずに保存する
        }
    }
    // 録画した音声 (または譜面から作った音声) の WAV を用意する。作れなければ空
    fs::path RecorderPrepareAudio() {
        std::error_code ec;
        const bool captured = Recorder.HasAudio && fs::exists(Recorder.AudioPath, ec) && rec::WavPeak(Recorder.AudioPath) > 0.001f;
        if (Config.RecordAudio == 0 && captured) { return Recorder.AudioPath; }
        // 譜面から作る: 曲と効果音をメインスレッドでデコードし (DxLib)、混ぜて WAV にする
        auto load = [](const fs::path& path) {
            rec::Pcm pcm;
            std::error_code e;
            if (path.empty() || !fs::is_regular_file(path, e)) { return pcm; }
            const int handle = LoadSoftSound(PathToU8(path).c_str());
            if (handle == -1) { return pcm; }
            int channels = 0, bits = 0, rate = 0, isfloat = 0;
            GetSoftSoundFormat(handle, &channels, &bits, &rate, &isfloat);
            const LONGLONG count = GetSoftSoundSampleNum(handle);
            const void* image = GetSoftSoundDataImage(handle);
            if (image != nullptr && count > 0 && channels > 0 && (bits == 16 || bits == 8 || (bits == 32 && isfloat))) {
                pcm.Channels = channels;
                pcm.Rate = rate;
                pcm.Data.resize((size_t)count * channels);
                for (size_t i = 0; i < pcm.Data.size(); ++i) {
                    pcm.Data[i] = bits == 16 ? ((const int16_t*)image)[i] / 32768.0f
                        : bits == 8 ? (((const uint8_t*)image)[i] - 128) / 128.0f
                        : ((const float*)image)[i];
                }
            }
            DeleteSoftSound(handle);
            return pcm;
        };
        std::vector<float> mix((size_t)std::max(1.0, Recorder.Duration * rec::MixRate) * 2, 0.0f);
        bool any = false;
        if (Recorder.SongStarted) {
            const rec::Pcm song = load(Recorder.SongPath);
            if (song.Frames() > 0) {
                rec::MixPcm(mix, song, Recorder.SongAt, Recorder.SongPos, Recorder.SongSpeed, Recorder.SongGain);
                any = true;
            }
        }
        rec::Pcm se[3];
        for (int k = 0; k < 3; ++k) { se[k] = load(Recorder.SePath[k]); }
        for (const auto& [t, kind] : Recorder.SeLog) {
            if (kind >= 0 && kind < 3 && se[kind].Frames() > 0) { rec::MixPcm(mix, se[kind], t, 0.0, 1.0, Recorder.SeGain, 2.0); any = true; }
        }
        if (!any) { return captured ? Recorder.AudioPath : fs::path(); }
        // 音が割れないよう、大きすぎる時だけ全体を下げる
        float peak = 0.0f;
        for (float v : mix) { peak = std::max(peak, std::abs(v)); }
        if (peak > 0.98f) { for (float& v : mix) { v *= 0.98f / peak; } }
        const fs::path path = fs::path(Recorder.AudioPath).replace_extension(L".mix.wav");
        if (!rec::WriteWav(path, mix)) { return captured ? Recorder.AudioPath : fs::path(); }
        Recorder.AudioDelay = 0.0;      // 譜面から作った音声は録画の開始が 0 秒
        return path;
    }
    // 保存する (true) / 破棄する (false)
    void RecorderDecide(bool save) {
        Recorder.AwaitingDecision = false;
        if (Recorder.Muxer.joinable()) { Recorder.Muxer.join(); }
        const fs::path audio = save ? RecorderPrepareAudio() : fs::path();
        const std::string ext = rec::FindFormat(Config.RecordFormat).Ext;
        Recorder.OutPath = fs::path(Recorder.OutPath).replace_extension(U8ToPath(std::string(".") + ext));
        Recorder.Muxing = true;
        RecorderStatus(save ? "録画を書き出しています..." : "録画を破棄しています...", 600);
        Recorder.Muxer = std::thread([this, save, audio] {
            if (Recorder.Finisher.joinable()) { Recorder.Finisher.join(); }
            std::error_code ec;
            bool ok = false;
            if (save && Recorder.VideoOk) {
                ok = rec::Export(Recorder.Ffmpeg, Recorder.VideoPath, audio, Recorder.AudioDelay, Recorder.OutPath, rec::FindFormat(Config.RecordFormat).Ext);
                if (!ok && audio.empty() && rec::FindFormat(Config.RecordFormat).Video) {
                    fs::rename(Recorder.VideoPath, fs::path(Recorder.OutPath).replace_extension(L".mp4"), ec);     // 音声なしの映像だけ残す
                    ok = !ec;
                }
            }
            fs::remove(Recorder.VideoPath, ec);
            fs::remove(Recorder.AudioPath, ec);
            fs::remove(fs::path(Recorder.AudioPath).replace_extension(L".mix.wav"), ec);
            if (!save) { RecorderStatus("録画を中断しました (保存していません)", 4); }
            else {
                RecorderStatus(ok ? "録画を保存しました: recordings/" + PathToU8(Recorder.OutPath.filename()) + (Recorder.Dropped ? " (" + std::to_string(Recorder.Dropped) + " フレーム欠落)" : "")
                    : std::string("録画の書き出しに失敗しました"));
            }
            Recorder.Muxing = false;
        });
    }
    // リザルトでの保存の確認 (Y: 保存 / N: 破棄、ボタンのクリックも可)。確認せずにリザルトを出たら保存する
    UIRect RecorderPromptButton(int i) const {
        const float cx = Skin.Info.Resolution.X * 0.5f, y = Skin.Info.Resolution.Y * 0.5f + 10.0f;
        const float w = 180.0f, h = 44.0f;
        const float x = i == 0 ? cx - w - 10.0f : cx + 10.0f;
        return { x, y, x + w, y + h };
    }
    void RecorderPromptProc() {
        if (!Recorder.AwaitingDecision) { return; }
        if (NowScene != Scene::Result) { RecorderDecide(true); return; }
        const bool y = (__gotkeyboardinput['Y'] & 1) != 0, n = (__gotkeyboardinput['N'] & 1) != 0;
        int mx = 0, my = 0;
        GetMousePoint(&mx, &my);
        const bool click = (GetMouseInput() & MOUSE_INPUT_LEFT) != 0;
        static bool prevclick = true;
        const bool pressed = click && !prevclick;
        prevclick = click;
        if (y || (pressed && RecorderPromptButton(0).Contains(mx, my))) { RecorderDecide(true); }
        else if (n || (pressed && RecorderPromptButton(1).Contains(mx, my))) { RecorderDecide(false); }
    }
    // ScreenFlip の直前 (録画した後) に描くので、録画には映らない
    void RecorderOverlay() {
        const FontData& sfont = ConfigSmallFont();
        if (Recorder.AwaitingDecision && NowScene == Scene::Result) {
            const float cx = Skin.Info.Resolution.X * 0.5f, cy = Skin.Info.Resolution.Y * 0.5f;
            UIBox({ cx - 240.0f, cy - 60.0f, cx + 240.0f, cy + 70.0f }, GetColor(16, 18, 28), 230);
            UIBox({ cx - 240.0f, cy - 60.0f, cx + 240.0f, cy + 70.0f }, GetColor(255, 200, 40), 255, false);
            UIText(sfont, cx, cy - 30.0f, "録画を保存しますか? (" + std::string(rec::FindFormat(Config.RecordFormat).Ext) + ")", GetColor(255, 255, 255), 1);
            const char* labels[] = { "保存する (Y)", "破棄する (N)" };
            for (int i = 0; i < 2; ++i) {
                const UIRect r = RecorderPromptButton(i);
                UIBox(r, i == 0 ? GetColor(255, 200, 40) : GetColor(60, 64, 84), 240);
                UIText(sfont, (r.L + r.R) * 0.5f, (r.T + r.B) * 0.5f, labels[i], i == 0 ? GetColor(0, 0, 0) : GetColor(255, 255, 255), 1);
            }
        }
        if (Recorder.Active) {
            const float x = (float)Skin.Info.Resolution.X - 16.0f;
            DrawCircleAA(x - ConfigTextWidth("REC") - 14.0f, 18.0f, 6.0f, 16, GetColor(240, 40, 40), TRUE);
            UIText(sfont, x, 18.0f, "REC", GetColor(255, 255, 255), 2);
        }
        std::lock_guard lock(Recorder.StatusMutex);
        if (!Recorder.Status.empty() && std::chrono::steady_clock::now() < Recorder.StatusUntil && (Recorder.Muxing || !Recorder.Active)) {
            UIText(sfont, 16, (float)Skin.Info.Resolution.Y - sfont.Size * 1.2f, FitText(Recorder.Status, sfont.Handle, (float)Skin.Info.Resolution.X - 32, false), GetColor(255, 220, 90));
        }
    }
    void RecorderAbort() {
        if (Recorder.Active) { Recorder.AbortRequested = true; }
    }
#else
    void RecorderAbort() {}
    void PlayHitSe(int kind) {
        auto& se = Skin.Base->Playing.SE;
        (kind == 0 ? se.Don : kind == 1 ? se.Ka : se.Balloon).Play();
    }
#endif

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

    static const char* CourseLabel(int course) {
        static const char* names[] = { "かんたん", "ふつう", "むずかしい", "おに", "裏", "タワー", "段位" };
        return course >= 0 && course < (int)std::size(names) ? names[course] : "すべて";
    }

    // 絞り込み条件を検索条件に変換する (画面で指定した条件は、キーワード欄の同じ種類の条件より優先する)
    // 名前が name のジャンルフォルダと、その中のジャンルフォルダの名前 (小文字)
    void CollectGenreFolders(const std::vector<std::unique_ptr<BoxData>>& datas, const std::string& name, bool inside, std::vector<std::string>& out) const {
        for (const auto& data : datas) {
            if (!data->IsGenre()) { continue; }
            const GenreData& genre = *data->GetGenre();
            const bool match = inside || genre.Name == name;
            if (match && std::ranges::find(out, ToLower(genre.Name)) == out.end()) { out.push_back(ToLower(genre.Name)); }
            CollectGenreFolders(genre.Datas, name, match, out);
        }
    }
    SearchQuery BuildFilterQuery(const FilterConditions& f) const {
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

    template<class F>
    void ForEachChart(const std::vector<std::unique_ptr<BoxData>>& datas, F&& f) const {
        for (const auto& data : datas) {
            if (data->IsGenre()) { ForEachChart(data->GetGenre()->Datas, f); }
            else if (data->GetChart()->IsDan == IsDanSelect()) { f(*data->GetChart()); }
        }
    }
    size_t CountFilterMatches(const FilterConditions& f) const {
        const SearchQuery q = BuildFilterQuery(f);
        size_t count = 0;
        ForEachChart(TempBoxDatas, [&](const ChartData& chart) { count += MatchChart(chart, q) ? 1 : 0; });
        return count;
    }

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

    static ConfigItemType FilterRowType(int row) {
        switch (row) {
        case RowKeyword: return ConfigItemType::Text;
        case RowFavorite: return ConfigItemType::Bool;
        default: return ConfigItemType::Choice;
        }
    }
    static const char* FilterRowLabel(int row) {
        static const char* labels[] = { "キーワード", "一致のしかた", "ジャンル", "コース", "お気に入りのみ" };
        return row >= 0 && row < (int)std::size(labels) ? labels[row] : "";
    }
    static const char* FilterRowHelp(int row) {
        switch (row) {
        case RowKeyword: return "タイトル・サブタイトル・ジャンルで探す";
        case RowExact: return "完全一致: タイトルかサブタイトルが同じ譜面だけ";
        case RowGenre: return "譜面フォルダのジャンルで絞り込む (中のジャンルも含む)";
        case RowCourse: return "そのコースがある譜面だけ";
        case RowFavorite: return "★ を付けた譜面だけ (選曲画面の F5 で ★)";
        default: return "ランダム: 当てはまる譜面から 1 曲選ぶ";
        }
    }
    std::string FilterRowValue(int row) {
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
    void FilterStep(int row, int dir) {
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

    // ---- レイアウト (文字は設定画面と同じ小さいフォントで、行の高さもその大きさから決める) ----
    float FilterLineHeight() const { return ConfigLineHeight(); }
    UIRect FilterPanelRect() const {
        const float x = (float)Skin.Info.Resolution.X, y = (float)Skin.Info.Resolution.Y;
        const float lh = FilterLineHeight();
        const float h = std::min(y * 0.9f, lh * (2.2f + 1.8f * RowButtons + 5.6f) + 40.0f);
        return { x * 0.14f, (y - h) * 0.5f, x * 0.86f, (y + h) * 0.5f };
    }
    float FilterRowHeight() const { return FilterLineHeight() * 1.8f; }
    UIRect FilterRowRect(int row) const {
        const UIRect r = FilterPanelRect();
        const float h = FilterRowHeight();
        const float y = r.T + FilterLineHeight() * 2.2f + 8.0f + h * row;
        return { r.L + 16, y, r.R - 16, y + h };
    }
    UIRect FilterValueRect(int row) const {
        const UIRect r = FilterPanelRect();
        const UIRect rr = FilterRowRect(row);
        const float cy = (rr.T + rr.B) * 0.5f, h = (rr.B - rr.T) * 0.36f;
        return { r.L + (r.R - r.L) * 0.34f, cy - h, r.R - 32, cy + h };
    }
    UIRect FilterArrowRect(int row, bool right) const {
        const UIRect v = FilterValueRect(row);
        const float w = std::max(36.0f, v.B - v.T);
        return right ? UIRect{ v.R - w, v.T, v.R, v.B } : UIRect{ v.L, v.T, v.L + w, v.B };
    }
    UIRect FilterButtonRect(int index) const {
        const UIRect r = FilterPanelRect();
        const float lh = FilterLineHeight();
        const float w = (r.R - r.L - 32 - 16 * (FilterButtonCount - 1)) / (float)FilterButtonCount;
        const float x = r.L + 16 + (w + 16) * index;
        return { x, r.B - 16 - lh * 1.6f, x + w, r.B - 16 };
    }

    // ---- 開閉・適用 ----
    void OpenFilterPanel() {
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
    void CloseFilterPanel() {
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
        Input.ResyncKeys();
#endif
    }
    void ApplyFilter(bool random) {
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
        DemoSong.Delete();
        CloseFilterPanel();
    }

    void DrawFilterPanel(const FontData& titlefont) {
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
            UIBox(b, current ? accent : GetColor(50, 54, 70), 230);
            UIBox(b, white, 160, false);
            UIText(sfont, (b.L + b.R) * 0.5f, (b.T + b.B) * 0.5f, FitText(buttons[i], sfont.Handle, b.R - b.L - 12, false), current ? black : white, 1);
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
            DrawKeyInputString((int)v.L + 12, (int)(((v.T + v.B) * 0.5f) - sfont.Size * 0.5f), FilterUI.EditHandle);
            SetDrawAreaFull();
        }
#endif
    }

    void FilterPanelProc() {
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
                Input.ResyncKeys();
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
        Input.HitKeyProcess(VK_UP, KeyState::Down, [&] { Move(-1); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_DOWN, KeyState::Down, [&] { Move(1); }, Config.KeyHoldProcInterval);
        Input.HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { Move(-1); }, Config.KeyHoldProcInterval);
        Input.HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { Move(1); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_LEFT, KeyState::Down, [&] { Change(-1); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_RIGHT, KeyState::Down, [&] { Change(1); }, Config.KeyHoldProcInterval);
        Input.HitKeyProcess(VK_RETURN, KeyState::Down, [&] { Activate(FilterUI.Selected); });
        Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, [&] { Activate(FilterUI.Selected); });
        Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, [&] { Activate(FilterUI.Selected); });
        Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] { Se(false); CloseFilterPanel(); });
#else
        Touch.Process(TouchType::Other, [&] { Se(false); CloseFilterPanel(); },
            IsDanSelect() ? Skin.Base->DanSelect.Image.Back : Skin.Base->SongSelect.Image.Back);
#endif
    }

    // ---- 選曲・段位選択画面の共通処理 ----
    void SelectSceneEnter() {
        FilterUI.Open = false;
        PointerPrev.Left = PointerPrev.Right = PointerPrev.Middle = true;
#ifndef __ANDROID__
        SetMouseDispFlag(TRUE);
#endif
    }
    void SelectSceneLeave() {
        if (FilterUI.Open) {
            CloseFilterPanel();
        }
#ifndef __ANDROID__
        SetMouseDispFlag(!Config.FullScreen);
#endif
    }
    // 一覧の箱のうち、ポインタの下にあるもの (重なっている場合は中心が近いほう)。なければ -1
    int HitTestSelectBox(const GraphData& box, float listy, float distance, const PointerState& pt) const {
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

    std::atomic_bool EndFlag = false;

    Timer AverageTimer;
    Timer FPSTimer;
    double BackFPS = 60;
    uint64_t NowFPS = 0;

    Timer TPSTimer;
    double NowTPS = 10000;
    static constexpr double TpsStallSeconds = 0.1;  // これより長い間隔は止まっていたものとして TPS に含めない

    // 0.5 秒ごとに回数を数えて FPS を求める
    struct RateCounter {
        uint64_t Count = 0;
        double MaxGapMs = 0;        // 区間内で最も長かった間隔
        std::chrono::steady_clock::time_point WindowStart = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point Last = std::chrono::steady_clock::now();
        double Rate = 0;
        double LastMaxGapMs = 0;
        void Tick() {
            const auto now = std::chrono::steady_clock::now();
            MaxGapMs = std::max(MaxGapMs, std::chrono::duration<double, std::milli>(now - Last).count());
            Last = now;
            ++Count;
            const double elapsed = std::chrono::duration<double>(now - WindowStart).count();
            if (elapsed >= 0.5) {
                Rate = Count / elapsed;
                LastMaxGapMs = MaxGapMs;
                Count = 0;
                MaxGapMs = 0;
                WindowStart = now;
            }
        }
    };
    RateCounter FpsCounter;

    Timer DrawTimer;
    double BackDrawTime = 1;
    double NowDrawTime = 0;

    struct FadeData {
        Timer Timer;
        const double Time = 0.35;
        void Draw() {
            if (Timer.GetElapsed().Second() > Time) {
                Timer.Reset();
                return;
            }
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255 * (1 - GetEasingRate(Timer.GetElapsed().Second() / Time, ease::Base::In, ease::Line::Linear)));
            DrawFillBox(0, 0, Skin.Info.Resolution.X, Skin.Info.Resolution.Y, GetColor(0, 0, 0));
            SetDrawBlendMode(0, 0);
        }
    } Fade;

    bool Init() {

        if (fs::exists(GetExecutablePath() / "temp.tja")) {
            fs::remove(GetExecutablePath() / "temp.tja");
        }

        SetOutApplicationLogValidFlag(FALSE);
        SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8);
        SetAlwaysRunFlag(TRUE);
        SetUseDivGraphFlag(FALSE);

#ifndef __ANDROID__
        // 初めての起動: この PC に合わせたおすすめ設定にしておく (音声デバイスはこの後で作るので、ここで決める)
        if (Config.FirstRun) {
            for (auto& change : BuildRecommendations()) {
                change.Apply();
            }
            Config.FirstRun = false;
            Config.Write();
        }
        SetWaitVSyncFlag(Config.WaitVSync);
        SetDoubleStartValidFlag(Config.MultiBoot);
        ChangeWindowMode(!Config.FullScreen);
        // 最大化ボタンを使えるようにする (画面はウィンドウに合わせて拡大。枠をつかんでのサイズ変更は処理が止まるので WndProc で無効にしている)
        SetWindowSizeChangeEnableFlag(TRUE, TRUE);
        SetMainWindowText("FreedomApopicStyle");
#ifndef __ANDROID__
        SetWindowIconID(101);       // FreedomApopicStyle.rc の IDI_ICON1 (タイトルバー・タスクバーのアイコン)
#endif
        SetWindowVisibleFlag(FALSE);

        SoundDevice.SetDevice(
            Config.SoundDeviceType,
            Config.Exclusive,
            Config.BufferSize,
            Config.SampleRate
        );
#endif

        if (DxLib_Init() == -1) {
            return false;
        }

        if (int rate = GetRefreshRate(); rate > 0) {
            RefreshRate = rate;
        }
        BackFPS = RefreshRate;

#ifdef __ANDROID__

        JNIEnv* env;
        const ANativeActivity* NativeActivity;
        int AssetCopyState = 0;

        NativeActivity = GetNativeActivity();

        while (ProcessMessage() == 0 && AssetCopyState == 0) {
            ClearDrawScreen();

            if (NativeActivity->vm->AttachCurrentThreadAsDaemon(&env, NULL) != JNI_OK) {
                return false;
            }

            jclass jclass_MainActivity = env->GetObjectClass(NativeActivity->clazz);
            jfieldID jfieldID_mAssetCopyState = env->GetFieldID(jclass_MainActivity, "mAssetCopyState", "I");
            AssetCopyState = env->GetIntField(NativeActivity->clazz, jfieldID_mAssetCopyState);
            env->DeleteLocalRef(jclass_MainActivity);
            NativeActivity->vm->DetachCurrentThread();

            DrawString(0, 0, "スキンアセットの読み込み中", GetColor(255, 255, 255));
            ScreenFlip();
        }
#endif
        Skin.Init();
        Skin.Load(Config.SkinName);
        EnumChart(Config.SongDirectories);

#ifndef __ANDROID__
        SetWindowVisibleFlag(TRUE);
        SetWindowZOrder(DX_WIN_ZTYPE_TOP, false);
        // 失敗した場合は従来のウィンドウメッセージによる入力判定で動作する
        RawKeyboard::GetInstance().Start(GetMainWindowHandle());
        DiscordRpc::GetInstance().Start(DiscordAppId());
#else
        ellipse.Load();
#endif
        return true;
    }

    void Draw() {

        FpsCounter.Tick();
        BackFPS = FpsCounter.Rate;
        FPSTimer.Start();
        DrawTimer.Start();

        if (NowScene != MemScene) {

            Fade.Timer.Start();

            switch (MemScene) {
            case Scene::SongSelect:
                SongSelectEnd();
                break;
            case Scene::DanSelect:
                DanSelectEnd();
                break;
            case Scene::MultiRoom:
                MultiRoomEnd();
                break;
            case Scene::ConfigMenu:
                ConfigMenuEnd();
                break;
            case Scene::Editor:
                EditorEnd();
                break;
            case Scene::Playing:
                PlayingEnd();
                break;
            case Scene::Result:
                ResultEnd();
                break;
            }

            MemScene = NowScene;

            switch (NowScene) {
            case Scene::Title:
                TitleInit();
                break;
            case Scene::ModeSelect:
                ModeSelectInit();
                break;
            case Scene::SongSelect:
                SongSelectInit();
                break;
            case Scene::DanSelect:
                DanSelectInit();
                break;
            case Scene::MultiRoom:
                MultiRoomInit();
                break;
            case Scene::Playing:
                PlayingInit();
                break;
            case Scene::ConfigMenu:
                ConfigMenuInit();
                break;
            case Scene::Editor:
                EditorInit();
                break;
            }
        }

        switch (NowScene) {
        case Scene::Title:
            TitleDraw();
            break;
        case Scene::ModeSelect:
            ModeSelectDraw();
            break;
        case Scene::SongSelect:
            SongSelectDraw();
            break;
        case Scene::DanSelect:
            DanSelectDraw();
            break;
        case Scene::MultiRoom:
            MultiRoomDraw();
            break;
        case Scene::Loading:
            LoadingDraw();
            break;
        case Scene::Playing:
            PlayingDraw();
            break;
        case Scene::Result:
            ResultDraw();
            break;
        case Scene::ConfigMenu:
            ConfigMenuDraw();
            break;
        case Scene::Editor:
            EditorDraw();
            break;
        }

#ifdef __ANDROID__
        ellipse.Draw();
#endif

        if (Fade.Timer.IsRunning()) {
            Fade.Draw();
        }

        BackDrawTime = (BackDrawTime + DrawTimer.GetElapsed().Second()) / 2;

        if (Config.ViewDebug) {
            if (!AverageTimer.IsRunning() || AverageTimer.GetElapsed().Second() > 0.5) {
                AverageTimer.Start();
                NowFPS = BackFPS;
                NowDrawTime = BackDrawTime;
            }
            DrawFormatString(0, 0, GetColor(255, 255, 255), "FPS:%llu (max %.1fms)\nTPS:%.0f\nDrawTime:%.12lf",
                NowFPS, FpsCounter.LastMaxGapMs, NowTPS, NowDrawTime);
        }
    }
#ifndef __ANDROID__
    // そのフレームで受け取った Raw Input のキーイベント (Proc の先頭で取り出す)
    std::vector<RawKeyboard::Event> KeyEvents;
    bool RawEventsSeen = false;     // Raw Input のキーイベントを受け取ったことがある
    int64_t LegacyHitTick = 0;      // ウィンドウのキー入力で最後に判定した時刻 (QPC)
    // Raw Input が途中で届かなくなったことの検出 (ウィンドウには押した通知が来たのに Raw Input が来ない)
    unsigned char WndKeysPrev[256]{};
    int64_t RawLastDownTick = 0;
    int64_t PendingWndDown = 0;     // まだ Raw Input で確かめられていない、ウィンドウでの押下時刻
    int64_t LastReregister = 0;
    int64_t LastHitTick = 0;
#endif

    // ウィンドウにはキーを押した通知が来たのに、少し待っても Raw Input のイベントが来ない時は、
    // Raw Input の受け取りを他に奪われた (Config の文字入力やダイアログでのフォーカスの切り替えで DxLib が登録し直すなど) とみなし、
    // ウィンドウのキー入力での判定に戻して登録し直す (届き始めたら Raw Input に戻る)
#ifndef __ANDROID__
    void CheckRawInputAlive() {
        auto& raw = RawKeyboard::GetInstance();
        const int64_t now = RawKeyboard::Now();
        for (const auto& e : KeyEvents) {
            if (e.Down) { RawLastDownTick = std::max(RawLastDownTick, e.Time); }
        }
        const bool pressed = !NewlyPressedKeys(WndKeysPrev).empty();
        if (!raw.IsActive() || !RawEventsSeen) {
            PendingWndDown = 0;
            return;
        }
        if (pressed && PendingWndDown == 0) {
            PendingWndDown = now;
        }
        if (PendingWndDown != 0) {
            if (RawLastDownTick >= PendingWndDown - RawKeyboard::MsToTicks(100)) {
                PendingWndDown = 0;     // Raw Input も届いている
            }
            else if (now - PendingWndDown > RawKeyboard::MsToTicks(150)) {
                PendingWndDown = 0;
                RawEventsSeen = false;  // ウィンドウのキー入力で判定する
                if (now - LastReregister > RawKeyboard::MsToTicks(2000)) {
                    LastReregister = now;
                    raw.Reregister();
                }
            }
        }
    }
#endif

#ifdef __ANDROID__
    // ファイル管理アプリから zip をインポートしたら (MainActivity.ImportSerial が増えたら)、譜面の一覧を作り直す
    int ImportSerialSeen = 0;
    int ImportCheckFrame = 0;
    bool ImportPending = false;
    void AndroidCheckImport() {
        if (++ImportCheckFrame >= 30) {
            ImportCheckFrame = 0;
            const ANativeActivity* activity = GetNativeActivity();
            JNIEnv* env = nullptr;
            if (activity->vm->AttachCurrentThreadAsDaemon(&env, nullptr) == JNI_OK) {
                jclass cls = env->GetObjectClass(activity->clazz);
                if (jfieldID field = env->GetFieldID(cls, "ImportSerial", "I")) {
                    const int serial = env->GetIntField(activity->clazz, field);
                    if (serial != ImportSerialSeen) { ImportSerialSeen = serial; ImportPending = true; }
                }
                if (env->ExceptionCheck()) { env->ExceptionClear(); }
                env->DeleteLocalRef(cls);
            }
        }
        // 選曲中に一覧を作り直すと選んでいる箱が無くなるので、メニューの画面で行う
        if (ImportPending && (NowScene == Scene::Title || NowScene == Scene::ModeSelect)) {
            ImportPending = false;
            EnumChart(Config.SongDirectories);
        }
    }
#endif
    void Proc() {

#ifndef __ANDROID__
        RawKeyboard::GetInstance().Drain(KeyEvents);
        if (!RawEventsSeen && !KeyEvents.empty()) {
            // Raw Input が届いた (初めて、または止まった後に再開した): ウィンドウのキー入力で判定済みの打鍵を、もう一度判定しないよう捨てる
            RawEventsSeen = true;
            std::erase_if(KeyEvents, [&](const RawKeyboard::Event& e) { return e.Time <= LegacyHitTick; });
        }
        CheckRawInputAlive();
#endif
        OnlineUpdate();
        DiscordUpdate();
#ifdef __ANDROID__
        AndroidCheckImport();
#endif

        // TPS は元の計算方法 (前回の値と今回の間隔から求めた値の平均)。
        // ただし読み込みや画面の切り替え・ウィンドウ操作などで 1 回だけ長く止まった間隔は、処理速度ではないので数えない
        if (TPSTimer.IsRunning()) {
            const double dt = TPSTimer.GetElapsed().Second();
            if (dt > 0.0 && dt < TpsStallSeconds) {
                NowTPS = (NowTPS + 1.0 / dt) / 2.0;
            }
        }
        TPSTimer.Start();

#ifdef __ANDROID__
        Touch.Num = GetTouchInputDownLog(Touch.List, TOUCHINPUTPOINT_MAX);
#endif

        switch (NowScene) {
        case Scene::Title:
            TitleProc();
            break;
        case Scene::ModeSelect:
            ModeSelectProc();
            break;
        case Scene::SongSelect:
            SongSelectProc();
            break;
        case Scene::DanSelect:
            DanSelectProc();
            break;
        case Scene::MultiRoom:
            MultiRoomProc();
            break;
        case Scene::Loading:
            LoadingProc();
            break;
        case Scene::Playing:
            PlayingProc();
            break;
        case Scene::Result:
            ResultProc();
            break;
        case Scene::ConfigMenu:
            ConfigMenuProc();
            break;
        case Scene::Editor:
            EditorProc();
            break;
        }
    }

    // atomic なので mutex は不要
    static inline std::atomic_bool _waitvsyncLog = false;
    static inline void _LogUpdate(const std::atomic_bool* endflag) {
        while (!*endflag) {
            WaitVSync(1);
            _waitvsyncLog = true;
        }
    }
    static inline bool GetVSyncWaitLog() {
        return _waitvsyncLog.exchange(false);
    }

    void Main() {

        if (!Init()) {
            return;
        }

        Timer WaitTimer;
        WaitTimer.Start();
        uint64_t waittime = 0;
        bool drawflag = false;

        std::thread thd([&] { _LogUpdate(&EndFlag); });

        while (true) {

            if (ProcessMessage() == -1 || NowScene == Scene::End) {
                EndFlag = true;
            }
            if (EndFlag) {
                break;
            }
            if (Config.WaitVSync && Config.FastInput) {
                if (GetVSyncWaitLog() && drawflag) {
                    WaitTimer.Start();
                    waittime = (uint64_t)(1000000000 * Config.FastDrawRate / RefreshRate);
                    drawflag = false;
                }
                Proc();
                if ((uint64_t)WaitTimer.GetElapsed().NanoSecond() > waittime) {
                    Draw();
#ifndef __ANDROID__
                    RecorderCapture();
                    RecorderOverlay();
                    RecorderPromptProc();
#endif
                    ScreenFlip();
                    ClearDrawScreen();
                    WaitTimer.Reset();
                    drawflag = true;
                }
            }
            else {
                Draw();
#ifndef __ANDROID__
                RecorderCapture();
                RecorderOverlay();
                RecorderPromptProc();
#endif
                Proc();
                ScreenFlip();
                ClearDrawScreen();
            }
        }

        OnlineDisconnect();
#ifndef __ANDROID__
        RecorderStop();
        if (Recorder.AwaitingDecision) { RecorderDecide(true); }     // 確認の途中で終了したら保存する
        if (Recorder.Muxer.joinable()) { Recorder.Muxer.join(); }     // 録画の書き出しが終わるまで待つ
        if (Recorder.Finisher.joinable()) { Recorder.Finisher.join(); }
#endif

        // 監視スレッドが DxLib を呼んでいる間に DxLib_End しないよう、先に終了を待つ
        thd.join();
#ifndef __ANDROID__
        RawKeyboard::GetInstance().Stop();
        DiscordRpc::GetInstance().Stop();
#endif
        DxLib_End();
    }
};