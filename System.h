#pragma once
#ifndef __ANDROID__
#include "resource.h"
#endif
#include "Include.h"

#ifndef __ANDROID__

fs::path GetExecutablePath() {
    std::vector<char> buffer(MAX_PATH);
    while (true) {
        uint32_t len = GetModuleFileName(NULL, buffer.data(), (DWORD)buffer.size());
        if (len == 0) return L"";
        if (len < buffer.size()) {
            return fs::path(buffer.data()).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
}

#else

fs::path GetExecutablePath() {
    fs::path dir = "/storage/emulated/0/Documents/FAS";
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
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
    char InputString[1024];

    NativeActivity = GetNativeActivity();

    {
        if (NativeActivity->vm->AttachCurrentThreadAsDaemon(&env, NULL) != JNI_OK) {
            return "";
        }

        jclass jclass_MainActivity = env->GetObjectClass(NativeActivity->clazz);
        jmethodID jmethodID_StartInputDialog = env->GetMethodID(jclass_MainActivity, "StartInputStringDialog", "(Ljava/lang/String;)V");
        env->CallVoidMethod(NativeActivity->clazz, jmethodID_StartInputDialog, env->NewStringUTF(theme.c_str()));
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
            strcpy(InputString, chars_InputString);
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
            Write();
            ifs = std::ifstream(GetExecutablePath() / "config.json");
        }

        json data = json::parse(ifs);

#define JSONDATA(name) name = data.value(#name, name)\

        JSONDATA(PlayerName);
        JSONDATA(AutoPlay);
        JSONDATA(ServerAddress);
        JSONDATA(ServerPort);
        JSONDATA(RandomRate);
        JSONDATA(HiddenLevel);
        JSONDATA(SuddenLevel);
        JSONDATA(JudgeGood);
        JSONDATA(JudgeOk);
        JSONDATA(JudgeBad);
        JSONDATA(SongOffset);
        JSONDATA(JudgeOffset);
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

#undef JSONDATA

        ifs.close();
    }

    void Write() {

#define JSONDATA(name) {#name, name}
        nlohmann::ordered_json data = {
                JSONDATA(PlayerName),
                JSONDATA(AutoPlay),
                JSONDATA(ServerAddress),
                JSONDATA(ServerPort),
                JSONDATA(RandomRate),
                JSONDATA(HiddenLevel),
                JSONDATA(SuddenLevel),
                JSONDATA(JudgeGood),
                JSONDATA(JudgeOk),
                JSONDATA(JudgeBad),
                JSONDATA(SongOffset),
                JSONDATA(JudgeOffset),
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
        KeyNameKaLeft = GetKeyName(KaInputLeft);
        KeyNameDonLeft = GetKeyName(DonInputLeft);
        KeyNameDonRight = GetKeyName(DonInputRight);
        KeyNameKaRight = GetKeyName(KaInputRight);
#endif
        std::ofstream ofs(GetExecutablePath() / "config.json");
        ofs << data.dump(4) << "\n";
        ofs.close();
    }

#ifndef __ANDROID__
    std::vector<std::string> GetKeyName(std::vector<int>& keys) {
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

    std::string PlayerName = "NoName";

    std::string ServerAddress = "localhost";
    uint16_t ServerPort = 8080;

    bool AutoPlay = false;
    bool TrainingMode = false;
    bool HitNoteDisp = true;
    bool WaitVSync = true;
    bool FastInput = true;
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
    double SongOffset = 0;
    double ChartSpeed = 1;
    double SongSpeed = 1;
    double BGBrightness = 100;
    double RollSpeed = 30;
    double KeyHoldProcInterval = 0.15;
    double FastDrawRate = 0.5;

    float SongVolume = 62.0f;
    float SEVolume = 70.0f;
    float TouchTaikoWidth = 560.0f;
    float TouchTaikoHeight = 280.0f;

    std::string SkinName = "Default";
    std::vector<std::string> SongDirectories{ "Songs" };

#ifndef __ANDROID__
    std::vector<int> KaInputLeft{ 'D','S',0,0 };
    std::vector<int> DonInputLeft{ 'F','G',0,0 };
    std::vector<int> DonInputRight{ 'J','H',0,0 };
    std::vector<int> KaInputRight{ 'K','L',0,0 };

    std::vector<std::string> KeyNameKaLeft{ "D","S","*","*" };
    std::vector<std::string> KeyNameDonLeft{ "F","G","*","*" };
    std::vector<std::string> KeyNameDonRight{ "J","H","*","*" };
    std::vector<std::string> KeyNameKaRight{ "K","L","*","*" };
#endif

} Config;

class _Skin {
public:

    void Init() {
        for (const auto& f : fs::recursive_directory_iterator(GetExecutablePath() / "Skins")) {
            if (f.path().extension() == ".json") {
                std::ifstream ifs(f.path());
                json data = json::parse(ifs);
                if (data.find("Info") == data.end()) {
                    continue;
                }
                Infos.push_back(
                    SkinInfo{
                            data["Info"]["Name"],
                            f.path().string()
                    }
                );
                ifs.close();
            }
        }
    }

    bool IsLoading = false;

    void Load(const std::string& name) {
        IsLoading = true;
        for (size_t i = 0, size = Infos.size(); i < size; ++i) {
            if (name == Infos[i].Name) {
                FilePath = Infos[i].Path;
                break;
            }
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

#define ValLoad(base, type, keyname) Base->base.type.keyname = base##data[#type].value(#keyname, Base->base.type.keyname)
#define DataLoad(base, type, keyname) Base->base.type.keyname.Load(SkinDir, base##data[#type][#keyname])

        Dispose();
        Base = new _Base();

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

        DataLoad(Playing, Font, Title);
        DataLoad(Playing, Font, SubTitle);
        DataLoad(Playing, Font, PlayerName);
        DataLoad(Playing, Font, ExamName);
        DataLoad(Playing, Font, ExamVal);

        DataLoad(Playing, SE, Don);
        DataLoad(Playing, SE, Ka);
        DataLoad(Playing, SE, Balloon);
        DataLoad(Playing, SE, DanFall);

#pragma endregion

#pragma region Result

        ValLoad(Result, Config, TitlePos);
        ValLoad(Result, Config, SubTitlePos);
        ValLoad(Result, Config, PlayerNamePos);
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

#undef ConfLoad
#undef DataLoad

        IsLoading = false;

    }

    void Dispose() {
        if (Base != nullptr) {
            delete Base;
        }
    }

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
                Pos2D<float> ExamNamePos;
                Pos2D<float> ExamValPos;
                float MultiPlayLaneDistance;
                double LaneExtendRate;
                double JudgeUpperExplosionFrameTime;
                double GoGoFireFrameTime;
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
            } Image;
            struct _Font {
                FontData Title;
                FontData SubTitle;
                FontData PlayerName;
                FontData ExamName;
                FontData ExamVal;
            } Font;
            struct _SE {
                SoundData Don;
                SoundData Ka;
                SoundData Balloon;
                SoundData DanFall;
            } SE;
            struct _BGM {
            } BGM;
        } Playing;
        struct _Result {
            struct _Config {
                Pos2D<float> TitlePos;
                Pos2D<float> SubTitlePos;
                Pos2D<float> PlayerNamePos;
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

        if (HitNote != 0) {
            Accuracy = ((Good / (double)HitNote) + ((Ok / (double)HitNote) * 0.5)) * 100;
        }
        if (Branch.HitNote != 0) {
            Branch.Accuracy = ((Branch.Good / (double)Branch.HitNote) + ((Branch.Ok / (double)Branch.HitNote) * 0.5)) * 100;
        }

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
    }

    auto operator<=>(const JudgeData&) const = default;
};

enum class HitType {
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

struct PlayerData {

    std::string Name = "\0";
    bool IsHost = false;
    short State = 0;

    Packet::bytearray ToBytes() const {
        Packet::bytearray ret;
        Packet::StoreBytes(ret, Name);
        Packet::StoreBytes(ret, IsHost);
        Packet::StoreBytes(ret, State);
        return ret;
    }
    Packet::byte_view FromBytes(Packet::byte_view view) {
        Packet::LoadBytes(view, Name);
        Packet::LoadBytes(view, IsHost);
        Packet::LoadBytes(view, State);
        return view;
    }

    auto operator<=>(const PlayerData&) const = default;
};

struct SharedData {
    std::vector<PlayerData> Players = std::vector<PlayerData>();
    std::vector<uint8_t> FileData = std::vector<uint8_t>();
    std::vector<uint8_t>  WaveData = std::vector<uint8_t>();
    JudgeData Judge = JudgeData();
    HitType HitKey = HitType::Null;
    int MyIndex = 0;
    int GetIndex = 0;
    int CourseIndex = 0;
    int PlayerCount = 0;
    double SongSpeed = 1.0;

    Packet::bytearray ToBytes() const {
        Packet::bytearray ret;
        Packet::StoreBytes(ret, Players);
        Packet::StoreBytes(ret, FileData);
        Packet::StoreBytes(ret, WaveData);
        Packet::StoreBytes(ret, Judge);
        Packet::StoreBytes(ret, HitKey);
        Packet::StoreBytes(ret, MyIndex);
        Packet::StoreBytes(ret, GetIndex);
        Packet::StoreBytes(ret, CourseIndex);
        Packet::StoreBytes(ret, PlayerCount);
        Packet::StoreBytes(ret, SongSpeed);
        return ret;
    }
    Packet::byte_view FromBytes(Packet::byte_view view) {
        Packet::LoadBytes(view, Players);
        Packet::LoadBytes(view, FileData);
        Packet::LoadBytes(view, WaveData);
        Packet::LoadBytes(view, Judge);
        Packet::LoadBytes(view, HitKey);
        Packet::LoadBytes(view, MyIndex);
        Packet::LoadBytes(view, GetIndex);
        Packet::LoadBytes(view, CourseIndex);
        Packet::LoadBytes(view, PlayerCount);
        Packet::LoadBytes(view, SongSpeed);
        return view;
    }

    void Clear() {
        if (!FileData.empty()) { FileData.clear(); }
        if (!WaveData.empty()) { WaveData.clear(); }
    }

    auto operator<=>(const SharedData&) const = default;
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

        static auto DonKeyProc = [&] {
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
    }

    void ModeSelectDraw() {
        Skin.Base->ModeSelect.Image.BackGround.Draw({ 0, 0 });
        for (size_t i = 0; i < (size_t)Mode::Count; ++i) {
            unsigned int c = 100 * ((size_t)ModeSelector == i);
            SetDrawAddColor(c, c, c);
            Skin.Base->ModeSelect.Image.Box.Draw({ 0, Skin.Base->ModeSelect.Config.BoxDistance * i });
            SetDrawAddColor(0, 0, 0);
            Skin.Base->ModeSelect.Font.Modes.Draw({
                                                          Skin.Base->ModeSelect.Config.ModesPos.X,
                                                          Skin.Base->ModeSelect.Config.ModesPos.Y + (Skin.Base->ModeSelect.Config.BoxDistance * i) }, GetColor(255, 255, 255), GetColor(0, 0, 0), magic_enum::enum_name((Mode)i).data());
        }
    }

    void ModeSelectProc() {

#ifndef __ANDROID__
        Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] {
            NowScene = Scene::Title;
            });
#endif

        static auto DonKeyProc = [&] {
            Skin.Base->Title.SE.Don.Play();
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
            case Mode::Config:
                PrevScene = Scene::ModeSelect;
                NowScene = Scene::ConfigMenu;
                break;
            case Mode::End:
                NowScene = Scene::Title;
                break;
            }
            };

        static auto KaKeyProc = [&](bool direction) {
            Skin.Base->Title.SE.Ka.Play();
            if (!direction) {
                ModeSelector > Mode::Single ? (int&)ModeSelector -= 1 : (int&)ModeSelector = 4;
            }
            else {
                ModeSelector < Mode::End ? (int&)ModeSelector += 1 : (int&)ModeSelector = 0;
            }
            };

#ifndef __ANDROID__
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
    };

    class ChartData {
    public:

        ChartData() {};
        ChartData(const fs::path& path) { this->Load(path); }

        struct StrlenData {
            size_t Generic = 0;
            size_t Playing = 0;
            size_t Result = 0;
            std::vector<size_t> Dan;
        } Strlen;

        std::string Title = "";
        std::string Subtitle = "";
        std::vector<std::string> DanTitle;
        std::vector<std::string> DanSubtitle;
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
        fs::path SongPath = u8"";
        fs::path MoviePath = u8"";
        std::vector<fs::path> DanSongPath;
        fs::path ChartPath = "";
        std::string SongLink = "";
        std::string MovieLink = "";
        CourseData Courses[(size_t)CourseType::Count];

        std::vector<ExamData> ExamDatas;
        std::vector<size_t> DanIndex;
        bool IsDan = false;

        bool Load(const fs::path& path) {
            TextfileReader text(path);
            ChartPath = path;

            size_t index = 0;
            uint64_t level = 0;
            uint64_t addscore = 0;
            std::vector<uint64_t> balloon;
            CourseType course = CourseType::Null;

            ExamData Exam;
            std::vector<std::string> DanTitles;
            std::vector<std::string> DanSubtitles;
            std::vector<size_t> DanTitleStrlens;
            std::vector<size_t> DanSubtitleStrlens;
            std::vector<size_t> DanIndexs;
            std::vector<fs::path> DanSongPaths;

            try {
                for (size_t i = 0; i < text.lines().size(); ++i) {
#ifdef __ANDROID__
                    if (!text[i].empty() && text[i].back() == '\n') {
                        text[i].remove_suffix(1);
                    }
                    if (!text[i].empty() && text[i].back() == '\r') {
                        text[i].remove_suffix(1);
                    }
#endif

                    Exsubstr(text[i], "TITLE:", [&](std::string_view data) {
                        if (data.find("--") == 0) {
                            data.remove_prefix(2);
                            TitleDisplay = false;
                        }
                        Title = data;
                        TitleStrlen.Generic = GetStrlen(data, Skin.Base->SongSelect.Font.Title.Handle);
                        TitleStrlen.Playing = GetStrlen(data, Skin.Base->Playing.Font.Title.Handle);
                        TitleStrlen.Result = GetStrlen(data, Skin.Base->Result.Font.Title.Handle);
                        });
                    Exsubstr(text[i], "SUBTITLE:", [&](std::string_view data) {
                        if (data.find("--") == 0) {
                            data.remove_prefix(2);
                            SubtitleDisplay = false;
                        }
                        Subtitle = data;
                        SubtitleStrlen.Generic = GetStrlen(data, Skin.Base->SongSelect.Font.Title.Handle);
                        SubtitleStrlen.Playing = GetStrlen(data, Skin.Base->Playing.Font.SubTitle.Handle);
                        SubtitleStrlen.Result = GetStrlen(data, Skin.Base->Result.Font.SubTitle.Handle);
                        });
                    Exsubstr(text[i], "BPM:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        BPM = svtov<double>(data);
                        });
                    Exsubstr(text[i], "OFFSET:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        SongOffset = svtov<double>(data);
                        });
                    Exsubstr(text[i], "MOVIEOFFSET:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        MovieOffset = svtov<double>(data);
                        });
                    Exsubstr(text[i], "DEMOSTART:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        DemoStart = svtov<double>(data);
                        });
                    Exsubstr(text[i], "SONGVOL:", [&](std::string_view data) {
                        if (!data.empty()) { return; }
                        SongVolume = svtov<double>(data);
                        });
                    Exsubstr(text[i], "SEVOL:", [&](std::string_view data) {
                        if (!data.empty()) { return; }
                        SeVolume = svtov<float>(data);
                        });
                    Exsubstr(text[i], "WAVE:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        SongPath = path.parent_path().u8string() + u8"\\" + std::u8string(data.begin(), data.end());
                        });
                    Exsubstr(text[i], "BGMOVIE:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        MoviePath = path.parent_path().u8string() + u8"\\" + std::u8string(data.begin(), data.end());
                        });
                    Exsubstr(text[i], "SONGLINK:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        SongLink = data;
                        });
                    Exsubstr(text[i], "MOVIELINK:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        MovieLink = data;
                        });
                    Exsubstr(text[i], "COURSE:", [&](std::string_view data) {
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
                        else if (str == "dan") {
                            course = CourseType::Dan;
                        }
                        if (course != CourseType::Null) {
                            return;
                        }
                        course = (CourseType)svtov<int>(data);
                        });
                    Exsubstr(text[i], "LEVEL:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        level = svtov<uint64_t>(data);
                        });
                    Exsubstr(text[i], "SCOREINIT:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        addscore = svtov<uint64_t>(data);
                        });
                    Exsubstr(text[i], "BALLOON:", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        auto sp = split(data, ',');
                        for (auto&& s : sp) {
                            balloon.push_back(svtov<uint64_t>(s));
                        }
                        });
                    Exsubstr(text[i], "EXAM" + std::to_string(ExamDatas.size() + 1) + ":", [&](std::string_view data) {
                        if (data.empty()) { return; }
                        auto sp = split(data, ',');
                        if (sp[3] == "m") {
                            Exam.Range = ExamRange::More;
                        }
                        if (sp[3] == "l") {
                            Exam.Range = ExamRange::Less;
                        }
                        if (sp[0] == "g") {
                            Exam.ExamType = ExamTypes::Accuracy;
                        }
                        else if (sp[0] == "jp") {
                            Exam.ExamType = ExamTypes::Good;
                        }
                        else if (sp[0] == "jg") {
                            Exam.ExamType = ExamTypes::Ok;
                        }
                        else if (sp[0] == "jb") {
                            Exam.ExamType = ExamTypes::Bad;
                        }
                        else if (sp[0] == "s") {
                            Exam.ExamType = ExamTypes::Score;
                        }
                        else if (sp[0] == "r") {
                            Exam.ExamType = ExamTypes::Roll;
                        }
                        else if (sp[0] == "h") {
                            Exam.ExamType = ExamTypes::HitNote;
                        }
                        else if (sp[0] == "c") {
                            Exam.ExamType = ExamTypes::MaxCombo;
                        }
                        Exam.PassVal[0] = svtov<double>(sp[1]);
                        Exam.PassVal[1] = svtov<double>(sp[2]);
                        ExamDatas.push_back(Exam);
                        });
                    Exsubstr(text[i], "#START", [&](std::string_view data) {
                        if (course == CourseType::Null) {
                            course = CourseType::Oni;
                        }
                        IsDan = (course == CourseType::Dan);
                        Courses[(size_t)course].Index = index;
                        Courses[(size_t)course].Level = level;
                        Courses[(size_t)course].AddScore = addscore;
                        Courses[(size_t)course].Balloons = balloon;
                        Courses[(size_t)course].IsPlayable = true;
                        balloon = std::vector<uint64_t>();
                        });
                    if (IsDan) {
                        Exsubstr(text[i], "#NEXTSONG", [&](std::string_view data) {
                            if (data.empty()) { return; }
                            auto sp = split(data, ',');
                            DanTitles.push_back(std::string(sp[0]));
                            DanTitleStrlens.push_back(GetStrlen(sp[0], Skin.Base->Playing.Font.Title.Handle));
                            DanSubtitles.push_back(std::string(sp[1]));
                            DanSubtitleStrlens.push_back(GetStrlen(sp[1], Skin.Base->Playing.Font.SubTitle.Handle));
                            DanIndexs.push_back(i);
                            DanSongPaths.push_back(path.parent_path().u8string() + u8"\\" + std::u8string(sp[3].begin(), sp[3].end()));
                            });
                        Exsubstr(text[i], "#END", [&](std::string_view data) {
                            DanTitle = DanTitles;
                            TitleStrlen.Dan = DanTitleStrlens;
                            DanSubtitle = DanSubtitles;
                            SubtitleStrlen.Dan = DanSubtitleStrlens;
                            DanIndex = DanIndexs;
                            DanSongPath = DanSongPaths;
                            });
                    }
                }
            }
            catch (...) {
#ifndef __ANDROID__
                MessageBox(NULL, TEXT(std::string(path.string() + "の読み込みに失敗しました").c_str()), TEXT("エラー"), MB_ICONERROR);
#else
                ShowToast(std::string(path.string() + "の読み込みに失敗しました").c_str());
#endif
            }
            return true;
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

        void Load(const fs::path& path) {

            FilePath = path.string();
            TextfileReader text(path);

            auto ColorCodeParse = [&](std::string_view data) -> Color3<int> {
                std::string str(data);
                str.replace(0, 1, "0x");
                int color = std::stoi(str, nullptr, 16);
                int r = 0, g = 0, b = 0;
                GetColor2(color, &r, &g, &b);
                return { (r - 128) * 2, (g - 128) * 2, (b - 128) * 2 };
                };

            for (size_t i = 0; i < text.lines().size(); ++i) {
                Exsubstr(text[i], "GenreName=", [&](std::string_view data) {
                    Name = data;
                    });
                Exsubstr(text[i], "GenreCaption=", [&](std::string_view data) {
                    Caption = data;
                    });
                Exsubstr(text[i], "GenreColor=", [&](std::string_view data) {
                    GenreColor = ColorCodeParse(data);
                    });
                Exsubstr(text[i], "FontColor=", [&](std::string_view data) {
                    FontColor = ColorCodeParse(data);
                    });
                Exsubstr(text[i], "FontEdgeColor=", [&](std::string_view data) {
                    FontEdgeColor = ColorCodeParse(data);
                    });
            }
        }

    };

    struct BoxData {

        BoxData() {}
        BoxData(ChartData* ptr) {
            delete m_Genre;
            m_Genre = nullptr;
            m_Chart = ptr;
        }
        BoxData(GenreData* ptr) {
            delete m_Chart;
            m_Chart = nullptr;
            m_Genre = ptr;
        }
        ~BoxData() {
            delete m_Chart;
            delete m_Genre;
        }

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

    void EnumChart(const std::vector<std::string>& dir) {
        if (dir.empty()) { return; }
        TempBoxDatas.clear();
        TempBoxDatas.reserve(dir.capacity());
        size_t LoadCount = 0;
        auto recusiveproc = [&](std::vector<std::unique_ptr<BoxData>>& data, const fs::path& dirpath, const fs::path& genrepath, auto& f, const GenreData& genredata = {}) -> void {
            fs::path _genrepath = "";
            auto fpit = fs::directory_iterator(dirpath);
            bool is_genre = false;
            for (const auto& fp : fpit) {
                if (fp == genrepath) {
                    break;
                }
                if (fp.path().filename() == "genre.ini") {
                    _genrepath = fp.path();
                    BoxData* _data = new BoxData(new GenreData(fp.path()));
                    _data->SetBoxColor();
                    data.push_back(std::unique_ptr<BoxData>(_data));
                    f(data.back()->GetGenre()->Datas, fp.path().parent_path(), _genrepath, f, *data.back()->GetGenre());
                    is_genre = true;
                    break;
                }
            }
            if (!is_genre) {
                fpit = fs::directory_iterator(dirpath);
                for (const auto& fp : fpit) {
                    if (fp.path().extension() == ".tja") {
                        BoxData* _data = new BoxData(new ChartData(fp.path()));
                        _data->SetBoxColor(genredata);
                        data.push_back(std::unique_ptr<BoxData>(_data));
                    }
                    if (fp.is_directory()) {
                        f(data, fp.path(), "", f, genredata);
                    }
                }
            }
            };

        for (size_t i = 0, size = dir.size(); i < size; ++i) {
            recusiveproc(TempBoxDatas, GetExecutablePath() / dir[i], "", recusiveproc);
        }

        BoxDatasUpdate();
        if (!BoxDatas.empty()) {
            BoxDataIndex = std::clamp<size_t>(BoxDataIndex, 0, BoxDatas.size() - 1);
        }
    }
    void BoxDatasUpdate(std::string_view keyword = "") {

        static auto TolowerFind = [](std::string_view str, std::string_view findstr) {
            return (bool)(ToLower(str).find(ToLower(findstr)) != std::string::npos);
            };
        if (TempBoxDatas.empty()) {
            return;
        }
        BoxDatas.clear();
        BoxDatas.reserve(TempBoxDatas.capacity());
        auto recusiveproc = [&](const std::vector<std::unique_ptr<BoxData>>& datas, auto f) -> void {
            for (size_t i = 0; i < datas.size(); ++i) {
                if (datas[i]->IsGenre()) {
                    if (!keyword.empty()) {
                        for (auto& data : datas[i]->GetGenre()->Datas) {
                            if (data->IsGenre()) { continue; }
                            if (data->GetChart()->IsDan != IsDanSelect()) { continue; }

                            bool is_find = false;
                            Exsubstr(keyword, "g=", [&](std::string_view strdata) {
                                if (strdata.empty()) { return; }
                                if (is_find) { return; }
                                is_find = TolowerFind(datas[i]->GetGenre()->Name, strdata);
                                });
                            Exsubstr(keyword, "l=", [&](std::string_view strdata) {
                                if (strdata.empty()) { return; }
                                if (is_find) { return; }
                                is_find = std::ranges::any_of(data->GetChart()->Courses, [&](const CourseData& course)
                                    { return course.Level == svtov<uint64_t>(strdata); });
                                });
                            Exsubstr(keyword, "l<=", [&](std::string_view strdata) {
                                if (strdata.empty()) { return; }
                                if (is_find) { return; }
                                is_find = std::ranges::any_of(data->GetChart()->Courses, [&](const CourseData& course)
                                    { return course.Level <= svtov<uint64_t>(strdata); });
                                });
                            Exsubstr(keyword, "l>=", [&](std::string_view strdata) {
                                if (strdata.empty()) { return; }
                                if (is_find) { return; }
                                is_find = std::ranges::any_of(data->GetChart()->Courses, [&](const CourseData& course)
                                    { return course.Level >= svtov<uint64_t>(strdata); });
                                });
                            Exsubstr(keyword, "c=", [&](std::string_view strdata) {
                                if (strdata.empty()) { return; }
                                if (is_find) { return; }
                                if (svtov<int>(strdata) >= (int)CourseType::Count) { return; }
                                is_find = data->GetChart()->Courses[svtov<int>(strdata)].IsPlayable;
                                });
                            Exsubstr(keyword, "b=", [&](std::string_view strdata) {
                                if (strdata.empty()) { return; }
                                if (is_find) { return; }
                                is_find = (int)data->GetChart()->BPM == svtov<int>(strdata);
                                });
                            Exsubstr(keyword, "b<=", [&](std::string_view strdata) {
                                if (strdata.empty()) { return; }
                                if (is_find) { return; }
                                is_find = data->GetChart()->BPM <= svtov<double>(strdata);
                                });
                            Exsubstr(keyword, "b>=", [&](std::string_view strdata) {
                                if (strdata.empty()) { return; }
                                if (is_find) { return; }
                                is_find = data->GetChart()->BPM >= svtov<double>(strdata);
                                });

                            if (is_find) {
                                BoxDatas.push_back(data.get());
                                continue;
                            }
                            if (!TolowerFind(data->GetChart()->Title, keyword)
                                && !TolowerFind(data->GetChart()->Subtitle, keyword)) {
                                continue;
                            }
                            BoxDatas.push_back(data.get());
                        }
                        continue;
                    }
                    BoxDatas.push_back(datas[i].get());
                    if (datas[i]->GetGenre()->IsOpen) {
                        f(datas[i]->GetGenre()->Datas, f);
                    }
                }
            }
            for (size_t i = 0; i < datas.size(); ++i) {
                if (datas[i]->IsGenre()) { continue; }
                if (datas[i]->GetChart()->IsDan != IsDanSelect()) { continue; }

                if (bool is_find = false; !keyword.empty()) {

                    Exsubstr(keyword, "l=", [&](std::string_view strdata) {
                        if (strdata.empty()) { return; }
                        if (is_find) { return; }
                        is_find = std::ranges::any_of(datas[i]->GetChart()->Courses, [&](const CourseData& course)
                            { return course.Level == svtov<uint64_t>(strdata); });
                        });
                    Exsubstr(keyword, "l<=", [&](std::string_view strdata) {
                        if (strdata.empty()) { return; }
                        if (is_find) { return; }
                        is_find = std::ranges::any_of(datas[i]->GetChart()->Courses, [&](const CourseData& course)
                            { return course.Level <= svtov<uint64_t>(strdata); });
                        });
                    Exsubstr(keyword, "l>=", [&](std::string_view strdata) {
                        if (strdata.empty()) { return; }
                        if (is_find) { return; }
                        is_find = std::ranges::any_of(datas[i]->GetChart()->Courses, [&](const CourseData& course)
                            { return course.Level >= svtov<uint64_t>(strdata); });
                        });
                    Exsubstr(keyword, "c=", [&](std::string_view strdata) {
                        if (strdata.empty()) { return; }
                        if (is_find) { return; }
                        if (svtov<int>(strdata) >= (int)CourseType::Count) { return; }
                        is_find = datas[i]->GetChart()->Courses[svtov<int>(strdata)].IsPlayable;
                        });
                    Exsubstr(keyword, "b=", [&](std::string_view strdata) {
                        if (strdata.empty()) { return; }
                        if (is_find) { return; }
                        is_find = (int)datas[i]->GetChart()->BPM == svtov<int>(strdata);
                        });
                    Exsubstr(keyword, "b<=", [&](std::string_view strdata) {
                        if (strdata.empty()) { return; }
                        if (is_find) { return; }
                        is_find = datas[i]->GetChart()->BPM <= svtov<double>(strdata);
                        });
                    Exsubstr(keyword, "b>=", [&](std::string_view strdata) {
                        if (strdata.empty()) { return; }
                        if (is_find) { return; }
                        is_find = datas[i]->GetChart()->BPM >= svtov<double>(strdata);
                        });

                    if (is_find) {
                        BoxDatas.push_back(datas[i].get());
                        continue;
                    }
                    if (!TolowerFind(datas[i]->GetChart()->Title, keyword)
                        && !TolowerFind(datas[i]->GetChart()->Subtitle, keyword)) {
                        continue;
                    }
                }
                BoxDatas.push_back(datas[i].get());
            }
            if (BoxDatas.empty()) {
                BoxDatasUpdate();
            }
            };
        recusiveproc(TempBoxDatas, recusiveproc);
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
        if (FAILED(hr)) { return; }

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
        CoUninitialize();

        for (auto&& file : DropFiles) {

            fs::path path(StrToWStr(file));

            if (path.extension() != ".zip") {
                fs::path dir = ImportFolderPath / path.filename();
                if (fs::is_directory(path)) {
                    fs::copy(path, dir, std::filesystem::copy_options::recursive | fs::copy_options::overwrite_existing);
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
                if (name.back() == '/') { name.pop_back(); }
                if (entry.isDir()) {
                    fs::create_directories(ImportFolderPath / StrToWStr(name));
                    continue;
                }
                std::vector<uint8_t> content = entry.readContent();
                MemToFile(ImportFolderPath / name, content);
            }
        }

        EnumChart(Config.SongDirectories);
    }
#endif
    void SongDownload(const std::string& link, const fs::path& path) {
        if (!link.empty() && !fs::exists(path)) {
#ifndef __ANDROID__
            if (MessageBox(NULL, TEXT("音源ファイルがありません。ダウンロードしますか？"), "", MB_YESNO) == IDYES) {

                if (fs::exists("song.ogg")) {
                    fs::remove("song.ogg");
                }

                std::string powershell = "powershell -Command ";
                std::string command = powershell + "yt-dlp -x --audio-format vorbis -o song " + link;
                if (std::system(command.c_str()) != 0) {
                    MessageBox(NULL, TEXT("音源のダウンロードに失敗しました"), TEXT("エラー"), MB_ICONERROR);
                    return;
                }

                if (fs::exists("song.ogg")) {
                    fs::rename("song.ogg", path);
                }
            }
#endif
        }
    }
    void MovieDownload(const std::string& link, const fs::path& path) {
        if (!link.empty() && !fs::exists(path)) {
#ifndef __ANDROID__
            if (MessageBox(NULL, TEXT("動画ファイルがありません。ダウンロードしますか？"), "", MB_YESNO) == IDYES) {

                if (fs::exists("movie.avi")) {
                    fs::remove("movie.avi");
                }

                std::string powershell = "powershell -Command ";
                std::string command = powershell + "yt-dlp --recode-video avi -o movie " + link;
                if (std::system(command.c_str()) != 0) {
                    MessageBox(NULL, TEXT("動画のダウンロードに失敗しました"), TEXT("エラー"), MB_ICONERROR);
                    return;
                }

                if (fs::exists("movie.avi")) {
                    fs::rename("movie.avi", path);
                }
            }
#endif
        }
    }

    void SongSelectInit() {

#ifndef __ANDROID__
        SetDragFileValidFlag(TRUE);
#endif
        Skin.Base->SongSelect.SE.Don.SetVolume((Config.SEVolume));
        Skin.Base->SongSelect.SE.Ka.SetVolume((Config.SEVolume));

        if (BoxDatas.empty()) {
#ifndef __ANDROID__
            MessageBox(NULL, TEXT("譜面がありません。"), TEXT("エラー"), MB_ICONERROR);
#else
            ShowToast("譜面がありません。");
#endif
            NowScene = Scene::ModeSelect;
            return;
        }

        BoxDatasUpdate();
    }
    void SongSelectEnd() {

#ifndef __ANDROID__
        SetDragFileValidFlag(FALSE);
#endif

        DemoSongPlayBlank.Reset();
        DemoSong.Delete();
        IsCourseSelect = false;
    }
    void SongSelectDraw() {

        Skin.Base->SongSelect.Image.BackGround.Draw({});

        if (!IsCourseSelect) {

            if (InputData.Handle != 0) {
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, 50);
            }

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
                }
            }
            SetDrawBlendMode(0, 0);

#ifndef __ANDROID__
            if (InputData.Handle != 0) {

                Skin.Base->SongSelect.Image.SearchBox.Draw({});
                SetKeyInputDrawArea(
                    Skin.Base->SongSelect.Image.SearchBox.Pos.X - Skin.Base->SongSelect.Image.SearchBox.Size.Width / 2,
                    Skin.Base->SongSelect.Image.SearchBox.Pos.Y - Skin.Base->SongSelect.Image.SearchBox.Size.Height / 2,
                    Skin.Base->SongSelect.Image.SearchBox.Pos.X + Skin.Base->SongSelect.Image.SearchBox.Size.Width / 2,
                    Skin.Base->SongSelect.Image.SearchBox.Pos.Y + Skin.Base->SongSelect.Image.SearchBox.Size.Height / 2,
                    InputData.Handle);
                DrawKeyInputString(Skin.Base->SongSelect.Config.KeyWordPos.X, Skin.Base->SongSelect.Config.KeyWordPos.Y, InputData.Handle);
            }
#else
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
                Skin.Base->SongSelect.Font.Level.Draw({ Skin.Base->SongSelect.Config.LevelPos.X,Skin.Base->SongSelect.Config.LevelPos.Y + y }, GetColor(255, 255, 255), GetColor(0, 0, 0), ToStr<std::u8string>(u8"★×") + std::to_string(BoxDatas[BoxDataIndex]->GetChart()->Courses[i].Level));

                ScoreData Score = ScoreDataLoad(BoxDatas[BoxDataIndex]->GetChart()->ChartPath.string(), i);

                Skin.Base->SongSelect.Font.HighScore.Draw({ Skin.Base->SongSelect.Config.HighScorePos.X,Skin.Base->SongSelect.Config.HighScorePos.Y + y }, GetColor(255, 255, 255), GetColor(0, 0, 0), std::to_string(Score.Score));
                Skin.Base->SongSelect.Image.Crown.Draw({ 0, y }, Score.Crown);
            }
        }
#ifdef __ANDROID__
        SetDrawBlendMode(0, 0);
        Skin.Base->SongSelect.Image.Back.Draw({});
#endif
    }
    void SongSelectProc() {

        static auto BackInputProc = [&] {
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

        static auto DonInputProc = [&] {
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
        static auto KaInputProc = [&](bool direction) {
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
                DemoSong.Load(ToStr(BoxDatas[BoxDataIndex]->GetChart()->SongPath.u8string()));
                SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMNOPRESS);
                DemoSong.SetCurrent(BoxDatas[BoxDataIndex]->GetChart()->DemoStart * 1000.0);
                DemoSong.SetVolume(BoxDatas[BoxDataIndex]->GetChart()->SongVolume * (Config.SongVolume / 100));
                DemoSong.Play(FALSE);
            }
        }
        static auto RandomInputProc = [&] {
            Skin.Base->SongSelect.SE.Ka.Play();
            BoxDataIndex = GetRand(BoxDatas.size() - 1);
            DemoSongPlayBlank.Stop();
            DemoSong.Delete();
            };
        static auto SearchInputProc = [&]() {

            Skin.Base->SongSelect.SE.Don.Play();
#ifndef __ANDROID__
            if (InputData.Handle == 0) {
                InputData.Handle = MakeKeyInput(CHAR_MAX, false, false, false);
                SetActiveKeyInput(InputData.Handle);
                SetKeyInputStringFont(Skin.Base->SongSelect.Font.KeyWord.Handle);
                return;
            }

            InputData.Load();
            BoxDatasUpdate(ToLower(InputData.Buffer));

            if (BoxDatas.empty()) {
                BoxDatasUpdate();
            }

            DeleteKeyInput(InputData.Handle);
#else
            InputData.Load(GetInputText("曲を検索"));
            BoxDatasUpdate(ToLower(InputData.Buffer));

            if (BoxDatas.empty()) {
                BoxDatasUpdate();
            }
#endif
            InputData.Handle = 0;
            BoxDataIndex = 0;
            DemoSongPlayBlank.Reset();
            DemoSong.Delete();
            };

        static auto ConfigInputProc = [&] {
            Skin.Base->SongSelect.SE.Don.Play();
            NowScene = Scene::ConfigMenu;
            PrevScene = Scene::SongSelect;
            };

#ifndef __ANDROID__

        if (InputData.Handle == 0) {

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

                const int MouseWheel = Input.GetMouseWheel();

                if (MouseWheel != 0) {
                    Skin.Base->SongSelect.SE.Ka.Play();
                    const int& _mousewheel = (std::abs(MouseWheel) % BoxDatas.size()) * (std::signbit(MouseWheel) ? -1 : 1) * -1;
                    if (std::signbit(_mousewheel)) {
                        BoxDataIndex = BoxDataIndex + _mousewheel <= -1 ? BoxDatas.size() + _mousewheel : BoxDataIndex + _mousewheel;
                    }
                    else {
                        BoxDataIndex = BoxDataIndex + _mousewheel >= BoxDatas.size() ? (BoxDataIndex + _mousewheel) - BoxDatas.size() : BoxDataIndex + _mousewheel;
                    }
                    DemoSongPlayBlank.Reset();
                    DemoSong.Delete();
                }

                FileImport();
            }

            return;
        }

        Input.HitKeyProcess(VK_RETURN, KeyState::Down, SearchInputProc);

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
        NowSongCount = 0;
        CourseIndex = 0;

        if (BoxDatas.empty()) {
#ifndef __ANDROID__
            MessageBox(NULL, TEXT("譜面がありません。"), TEXT("エラー"), MB_ICONERROR);
#else
            ShowToast("譜面がありません。");
#endif
            NowScene = Scene::ModeSelect;
            return;
        }

        BoxDatasUpdate();
    }
    void DanSelectEnd() {
#ifndef __ANDROID__
        SetDragFileValidFlag(FALSE);
#endif
    }
    void DanSelectDraw() {

        Skin.Base->DanSelect.Image.BackGround.Draw({});

        if (InputData.Handle != 0) {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 50);
        }

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
                Skin.Base->DanSelect.Image.Crown.Draw(pos, ScoreDataLoad(BoxDatas[i]->GetChart()->ChartPath.string(), (int)CourseType::Dan).Crown);
            }
        }
        SetDrawBlendMode(0, 0);

#ifndef __ANDROID__
        if (InputData.Handle != 0) {

            Skin.Base->DanSelect.Image.SearchBox.Draw({});
            SetKeyInputDrawArea(
                Skin.Base->DanSelect.Image.SearchBox.Pos.X - Skin.Base->DanSelect.Image.SearchBox.Size.Width / 2,
                Skin.Base->DanSelect.Image.SearchBox.Pos.Y - Skin.Base->DanSelect.Image.SearchBox.Size.Height / 2,
                Skin.Base->DanSelect.Image.SearchBox.Pos.X + Skin.Base->DanSelect.Image.SearchBox.Size.Width / 2,
                Skin.Base->DanSelect.Image.SearchBox.Pos.Y + Skin.Base->DanSelect.Image.SearchBox.Size.Height / 2,
                InputData.Handle);
            DrawKeyInputString(Skin.Base->DanSelect.Config.KeyWordPos.X, Skin.Base->DanSelect.Config.KeyWordPos.Y, InputData.Handle);
        }
#else
        SetDrawBlendMode(0, 0);
        Skin.Base->DanSelect.Image.Back.Draw({});
        Skin.Base->DanSelect.Image.Config.Draw({});
        Skin.Base->DanSelect.Image.Random.Draw({});
        Skin.Base->DanSelect.Image.Search.Draw({});
#endif
    }
    void DanSelectProc() {

        static auto BackInputProc = [&] {
            NowScene = Scene::ModeSelect;
            };

#ifndef __ANDROID__
        Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
#else
        Touch.Process(TouchType::Other, BackInputProc, Skin.Base->DanSelect.Image.Back);
#endif

        static auto DonInputProc = [&] {
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
        static auto KaInputProc = [&](bool direction) {
            Skin.Base->DanSelect.SE.Ka.Play();
            if (!direction) {
                BoxDataIndex = BoxDataIndex == 0 ? BoxDatas.size() - 1 : BoxDataIndex - 1;
            }
            else {
                BoxDataIndex = BoxDataIndex == BoxDatas.size() - 1 ? 0 : BoxDataIndex + 1;
            }
            };
        static auto RandomInputProc = [&] {
            Skin.Base->DanSelect.SE.Ka.Play();
            BoxDataIndex = GetRand(BoxDatas.size() - 1);
            };
        static auto SearchInputProc = [&]() {

            Skin.Base->DanSelect.SE.Don.Play();

#ifndef __ANDROID__
            if (InputData.Handle == 0) {
                InputData.Handle = MakeKeyInput(CHAR_MAX, false, false, false);
                SetActiveKeyInput(InputData.Handle);
                SetKeyInputStringFont(Skin.Base->DanSelect.Font.KeyWord.Handle);
                return;
            }

            InputData.Load();
            BoxDatasUpdate(ToLower(InputData.Buffer));

            if (BoxDatas.empty()) {
                BoxDatasUpdate();
            }

            DeleteKeyInput(InputData.Handle);
#else
            InputData.Load(GetInputText("曲を検索"));
            BoxDatasUpdate(ToLower(InputData.Buffer));

            if (BoxDatas.empty()) {
                BoxDatasUpdate();
            }
#endif
            InputData.Handle = 0;
            BoxDataIndex = 0;

            };

        static auto ConfigInputProc = [&] {
            Skin.Base->SongSelect.SE.Don.Play();
            NowScene = Scene::ConfigMenu;
            PrevScene = Scene::DanSelect;
            };

#ifndef __ANDROID__
        if (InputData.Handle == 0) {

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

                const int MouseWheel = Input.GetMouseWheel();

                if (MouseWheel != 0) {
                    Skin.Base->DanSelect.SE.Ka.Play();
                    const int& _mousewheel = (std::abs(MouseWheel) % BoxDatas.size()) * (std::signbit(MouseWheel) ? -1 : 1) * -1;
                    if (std::signbit(_mousewheel)) {
                        BoxDataIndex = BoxDataIndex + _mousewheel <= -1 ? BoxDatas.size() + _mousewheel : BoxDataIndex + _mousewheel;
                    }
                    else {
                        BoxDataIndex = BoxDataIndex + _mousewheel >= BoxDatas.size() ? (BoxDataIndex + _mousewheel) - BoxDatas.size() : BoxDataIndex + _mousewheel;
                    }
                }

                FileImport();
            }

            return;
        }

        Input.HitKeyProcess(VK_RETURN, KeyState::Down, SearchInputProc);
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

    SharedData Shared = SharedData();
    TCPSocket Socket = TCPSocket();
    std::vector<uint8_t> FileData = std::vector<uint8_t>();
    std::vector<uint8_t> WaveData = std::vector<uint8_t>();

    bool IsMulti = false;
    bool IsLoad = false;
    bool IsSelect = false;
    int GrantIndex = -1;

    void MultiDataInit() {
        if (IsMulti) { Socket.Close(); }
        Shared = SharedData();
        Socket = TCPSocket();
        FileData.clear();
        WaveData.clear();
        IsMulti = false;
        IsLoad = false;
        IsSelect = false;
    }
    bool IsHost() const {
        return (!Shared.Players.empty() && Shared.Players[Shared.MyIndex].IsHost);
    }
    bool CheckState(short i) const {
        return std::ranges::all_of(Shared.Players, [&](const PlayerData& data) { return data.State == i; });
    }
    bool Connect(const std::string& address, uint16_t port) {
        return Socket.Connect(IPAddress::SolveHostName(address)->Port(port));
    }
    template<typename T>
    void Send(T& data) {
        Socket.ASyncEncryptionSend(Packet(data));
    }
    template<typename T>
    void Recv(T& dest) {
        if (Socket.Available() > 0) {
            auto pak = *Socket.ASyncEncryptionRecv().get();
            if (pak.GetHeader().value().IsSameAs<T>()) {
                dest = *pak.Get<T>();
            }
        }
    }
    void MultiRoomInit() {
        Skin.Base->MultiRoom.SE.Don.SetVolume(Config.SEVolume);
        Skin.Base->MultiRoom.SE.Ka.SetVolume(Config.SEVolume);
        if (!IsMulti) {
            if (Connect(Config.ServerAddress, Config.ServerPort)) {
                Shared.Players.push_back(PlayerData{ .Name = Config.PlayerName });
                Socket.CryptEngine.Init(sharedkey);
                IsMulti = true;
            }
        }
    }
    void MultiRoomEnd() {
        DemoSong.Delete();
        DemoSongPlayBlank.Reset();
    }
    void MultiRoomDraw() {

        Skin.Base->MultiRoom.Image.BackGround.Draw({});

        if (!IsMulti) {
            Skin.Base->MultiRoom.Font.String.Draw(
                { 16,16 },
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                "Server Connecting...");
            return;
        }

        Skin.Base->MultiRoom.Image.TitleBox.Draw({});

        if (IsLoad) {
            Skin.Base->MultiRoom.Font.Title.Draw(
                Skin.Base->MultiRoom.Config.TitlePos,
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                Chart.OriginalData.Title
            );
            Skin.Base->MultiRoom.Font.SubTitle.Draw(
                Skin.Base->MultiRoom.Config.SubTitlePos,
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                Chart.OriginalData.Subtitle
            );
            Skin.Base->MultiRoom.Font.Course.Draw(
                Skin.Base->MultiRoom.Config.CoursePos,
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                magic_enum::enum_name((CourseType)Shared.CourseIndex).data()
            );
            Skin.Base->MultiRoom.Font.Level.Draw(
                Skin.Base->MultiRoom.Config.LevelPos,
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                ToStr<std::u8string>(u8"★×") + std::to_string(Chart.OriginalData.Courses[Shared.CourseIndex].Level)
            );
        }

        for (size_t i = 0; i < Shared.Players.size(); i++) {

            bool g = 1 - (GrantIndex == i) * IsSelect;

            Skin.Base->MultiRoom.Image.PlayerBox.Draw({ 0, 100.0f * i });
            Skin.Base->MultiRoom.Font.Player.Draw(
                { Skin.Base->MultiRoom.Config.PlayerPos.X,Skin.Base->MultiRoom.Config.PlayerPos.Y + 100.0f * i },
                GetColor(255 * g, 255 * g, 255 * !Shared.Players[i].State),
                GetColor(0, 0, 0),
                GetStrlen(Shared.Players[i].Name, Skin.Base->MultiRoom.Font.Player.Handle),
                Shared.Players[i].Name
            );

            if (Shared.Players[i].IsHost) {
                Skin.Base->MultiRoom.Image.Crown.Draw({ 0, 100.0f * i }, 3);
            }
        }

#ifdef __ANDROID__
        Skin.Base->MultiRoom.Image.Back.Draw({});
        Skin.Base->MultiRoom.Image.Config.Draw({});
        Skin.Base->MultiRoom.Image.Host.Draw({});
#endif
    }

    void MultiRoomProc() {

        static auto BackInputProc = [&] {
            if (IsMulti) {
                if (Shared.Players[Shared.MyIndex].State > 0) {
                    Shared.Players[Shared.MyIndex].State = 0;
                    return;
                }
            }
            MultiDataInit();
            NowScene = Scene::ModeSelect;
            };

        if (IsMulti) {
            if (IsLoad) {
                if (IsHost()) {
                    Shared.Clear();
                }
                if (Shared.HitKey == HitType::Enter) {
                    if (CheckState(2)) {
                        if (!IsHost()) {
                            Skin.Base->MultiRoom.SE.Don.Play();
                        }
                        SetSpeed();
                        WaitVSync(10);
                        Chart.NowTime.Start();
                        NowScene = Scene::Playing;
                        return;
                    }
                    Shared.Players[Shared.MyIndex].State = 2;
                }
                else {
                    if (!DemoSongPlayBlank.IsRunning()) {
                        DemoSong.Delete();
                        DemoSongPlayBlank.Start();
                    }
                    if (DemoSongPlayBlank.GetElapsed().Second() > DemoSongPlayBlankTime() &&
                        !DemoSong.IsPlay()) {
                        SetCreateSoundDataType(DX_SOUNDDATATYPE_FILE);
                        DemoSong.Load(WaveData.data(), WaveData.size());
                        SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMNOPRESS);
                        DemoSong.SetCurrent(Chart.OriginalData.DemoStart * 1000.0);
                        DemoSong.SetVolume(Chart.OriginalData.SongVolume * (Config.SongVolume / 100));
                        DemoSong.Play(FALSE);
                    }
                }
            }
            else {

                if (!Shared.FileData.empty() && !Shared.WaveData.empty()) {
                    FileData = std::move(Shared.FileData);
                    WaveData = std::move(Shared.WaveData);
                    PrevScene = Scene::MultiRoom;
                    NowScene = Scene::Loading;
                }

                static auto HostInputProc = [&] {
                    if (IsHost() && Shared.PlayerCount >= 2) {
                        Skin.Base->MultiRoom.SE.Don.Play();
                        GrantIndex = Shared.MyIndex;
                        IsSelect = true;
                    }
                    };

#ifndef __ANDROID__
                Input.HitKeyProcess(VK_TAB, KeyState::Down, HostInputProc);
#else
                Touch.Process(TouchType::Other, HostInputProc, Skin.Base->MultiRoom.Image.Host);
#endif
            }

            static auto DonInputProc = [&] {
                if (IsSelect) {
                    Skin.Base->MultiRoom.SE.Don.Play();
                    std::ranges::for_each(Shared.Players, [](PlayerData& data) {
                        data.State = 0;
                        data.IsHost = false;
                        });
                    Shared.Players[GrantIndex].IsHost = true;
                    GrantIndex = -1;
                    IsSelect = false;
                    return;
                }
                else if (IsHost()) {
                    if (!IsLoad) {
                        Skin.Base->MultiRoom.SE.Don.Play();
                        NowScene = Scene::SongSelect;
                    }
                    else if (CheckState(1)) {
                        Skin.Base->MultiRoom.SE.Don.Play();
                        Shared.HitKey = HitType::Enter;
                    }
                }
                if (!Shared.Players[Shared.MyIndex].State && IsLoad) {
                    Skin.Base->MultiRoom.SE.Don.Play();
                    Shared.Players[Shared.MyIndex].State = 1;
                }
                };

            static auto KaInputProc = [&](bool direction) {
                if (IsSelect) {
                    Skin.Base->MultiRoom.SE.Ka.Play();
                    if (!direction) {
                        GrantIndex <= 0 ? 0 : GrantIndex--;
                    }
                    else {
                        GrantIndex >= Shared.PlayerCount - 1 ? Shared.PlayerCount - 1 : GrantIndex++;
                    }
                }
                };

            static auto ConfigInputProc = [&] {
                if (Shared.Players[Shared.MyIndex].State == 0) {
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

            Input.HitKeyProcess(VK_F1, KeyState::Down, ConfigInputProc);
        }
        Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
#else
            Touch.Process(TouchType::LeftKa, [&] { KaInputProc(false); });
            Touch.Process(TouchType::RightKa, [&] { KaInputProc(true); });
            Touch.Process(TouchType::Don, DonInputProc);
            Touch.Process(TouchType::Other, ConfigInputProc, Skin.Base->MultiRoom.Image.Config);
    }
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

        double SongSpeed = !IsMulti ? Config.SongSpeed : Shared.SongSpeed;

#ifndef __ANDROID__
        if (!Chart.OriginalData.MoviePath.empty()) {
            Chart.Movie.Load(ToStr(Chart.OriginalData.MoviePath.u8string()), SongSpeed,
                Chart.OriginalData.MovieOffset < 0 ? Chart.OriginalData.MovieOffset * -1000 : -Chart.SongBlankTime);
            Chart.Movie.Resize(Skin.Info.Resolution.Y);
        }
#endif

        Chart.SongData.SetVolume(Chart.OriginalData.SongVolume * (Config.SongVolume / 100));
        int freq = Chart.SongData.Frequency * SongSpeed;
        Chart.SongData.SetFrequency(freq);
        Chart.SongSpeed = (double)freq / Chart.SongData.Frequency;
    }
    void LoadingDraw() {
        DrawFormatString(0, 8, GetColor(255, 255, 255), "ChartLoading...");
    }
    void LoadingProc() {

        ChartData LoadData;

        if (!IsMulti || IsHost()) {
            LoadData = *BoxDatas[BoxDataIndex]->GetChart();
            LoadData.Load(LoadData.ChartPath);
        }
        else {
            CourseIndex = Shared.CourseIndex;
            MemToFile("temp.tja", FileData);
            LoadData.Load("temp.tja");
        }

        TextfileReader text(LoadData.ChartPath);
        std::vector<std::string_view> lines(text.begin(), text.end());

        if (fs::exists("temp.tja")) {
            fs::remove("temp.tja");
        }

        bool dan_init = true;
        if (Chart.IsDanMode()) {
            dan_init = IsDanFall();
        }

        Chart.Init(dan_init);

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
        bool NextFlag = false;
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

        size_t StartIndex = 0;

        if (LoadData.IsDan) {
            if (Chart.ExamDatas.empty()) {
                Chart.ExamDatas.resize(LoadData.ExamDatas.size());
            }
            StartIndex = LoadData.DanIndex[NowSongCount];
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
                    auto sp = split(data, ',');
                    BranchData item;
                    switch (sp[0][0]) {
                    case 'p':
                        item.Type = IfBranchType::Perfect;
                        item.ExpertBranch = svtov<double>(sp[1]);
                        item.MasterBranch = svtov<double>(sp[2]);
                        break;
                    case 'r':
                        item.Type = IfBranchType::Roll;
                        item.ExpertBranch = svtov<double>(sp[1]);
                        item.MasterBranch = svtov<double>(sp[2]);
                        break;
                    case 's':
                        item.Type = IfBranchType::Score;
                        item.ExpertBranch = svtov<double>(sp[1]);
                        item.MasterBranch = svtov<double>(sp[2]);
                        break;
                    }
                    BranchCount = 0;
                    item.AbsTime = Chart.RawNoteDatas.back().AbsTime - 150;
                    item.StartMs = MainData.AbsTime;
                    item.Start = true;
                    Chart.BranchDatas.push_back(item);
                    NowBranchStartFlag = true;
                    BranchStartFindFlag = true;
                    BranchAddTime = 0;
                    });
                if (NowBranchStartFlag) {
                    if (lines[i].find("#NEXTSONG") == std::string_view::npos) {
                        Exsubstr(lines[i], "#N", [&](std::string_view data) {
                            if (BranchCount == 0) { MemData = MainData; }
                            MainData = MemData;
                            if (BranchCount != 0) { Chart.RawNoteDatas.back().RelaTime += -BranchAddTime; }
                            if (BranchCount == 0) { MainData.BranchStart = true; }
                            MainData.IsBranch = BranchType::Normal;
                            BranchAddTime = 0;
                            ++BranchCount;
                            });
                    }
                    if (lines[i].find("#END") == std::string_view::npos && lines[i].find("#EXAM") == std::string_view::npos) {
                        Exsubstr(lines[i], "#E", [&](std::string_view data) {
                            if (BranchCount == 0) { MemData = MainData; }
                            MainData = MemData;
                            if (BranchCount != 0) { Chart.RawNoteDatas.back().RelaTime += -BranchAddTime; }
                            if (BranchCount == 0) { MainData.BranchStart = true; }
                            MainData.IsBranch = BranchType::Expert;
                            BranchAddTime = 0;
                            ++BranchCount;
                            });
                    }
                    if (lines[i].find("#MEASURE") == std::string_view::npos) {
                        Exsubstr(lines[i], "#M", [&](std::string_view data) {
                            if (BranchCount == 0) { MemData = MainData; }
                            MainData = MemData;
                            if (BranchCount != 0) { Chart.RawNoteDatas.back().RelaTime += -BranchAddTime; }
                            if (BranchCount == 0) { MainData.BranchStart = true; }
                            MainData.IsBranch = BranchType::Master;
                            BranchAddTime = 0;
                            ++BranchCount;
                            });
                    }
                    Exsubstr(lines[i], "#BRANCHEND", [&](std::string_view data) {
                        if (BranchCount == 0) { MainData.BranchStart = true; }
                        MainData.IsBranch = BranchType::Null;
                        NowBranchStartFlag = false;
                        BranchAddTime = 0;
                        BranchCount = 0;
                        });
                }
                Exsubstr(lines[i], "#SCROLL", [&](std::string_view data) {
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
                    });
                Exsubstr(lines[i], "#BPMCHANGE", [&](std::string_view data) {
                    MainData.BPM = svtov<double>(data);
                    MainData.BpmChangeFlag = true;
                    });
                Exsubstr(lines[i], "#MEASURE", [&](std::string_view data) {
                    auto sp = split(data, '/');
                    MainData.Measure = svtov<double>(sp[0]) / svtov<double>(sp[1]);
                    });
                Exsubstr(lines[i], "#DELAY", [&](std::string_view data) {
                    Chart.RawNoteDatas.back().RelaTime += svtov<double>(data) * 1000;
                    MainData.AbsTime += svtov<double>(data) * 1000;
                    });

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
            catch (const std::invalid_argument) {
#ifndef __ANDROID__
                MessageBox(NULL, TEXT(std::string(std::to_string(i + 1) + "行目の記述が不正です。").c_str()), TEXT("エラー"), MB_ICONERROR);
#else
                ShowToast(std::string(std::to_string(i + 1) + "行目の記述が不正です。").c_str());
#endif
                NowScene = PrevScene;
                return;
            }

            for (size_t j = 0, strsize = lines[i].size(); j < strsize; ++j) {
                bool ChartFlag = (lines[i][j] >= '0' && lines[i][j] <= '9');
                bool EndFlag = lines[i][j] == ',';
                bool EmptyFlag = BarlineNoteCount == 0;
                if (ChartFlag || EndFlag || EmptyFlag) {

                    if (EndFlag && !EmptyFlag) {
                        BarlineLoading = false;
                        AddBarline = false;
                        BarlineNoteCount = 0;
                        break;
                    }

                    MainData.NoteType = lines[i][j];

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
MainData.NoteType = '\0';\
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

                    if (!NowRollFlag && (MainData.NoteType >= '5' && MainData.NoteType <= '7') || MainData.NoteType == '9') {
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
                    MainData.PosTime += MainData.RelaTime * (std::signbit(MainData.BPM) || std::signbit(MainData.Measure) ? -1 : 1);

                    if (NowBranchStartFlag) {
                        BranchAddTime += MainData.RelaTime * (std::signbit(MainData.BPM) || std::signbit(MainData.Measure) ? -1 : 1);
                    }

                    if (EndFlag) {
                        BarlineLoading = false;
                        AddBarline = false;
                        BarlineNoteCount = 0;
                        break;
                    }
                }
            }
        }

        if (Chart.IsDanMode()) {
            Chart.DanBalloonIndex += BalloonIndex;
        }
        if (NowSongCount == 0) {
            Chart.AllNoteCount = NoteCount;
        }
        Chart.AllBarlineCount = BarlineCount;
        Chart.AddScore = LoadData.Courses[CourseIndex].AddScore;
        if (Chart.AddScore == 0) {
            Chart.AddScore = 1000000 / (double)Chart.AllNoteCount;
        }
        Chart.NowBranchFlag = BranchStartFindFlag ? BranchType::Normal : BranchType::Null;
        if (Config.TrainingMode && !Chart.IsDanMode() && Chart.IsBranchChart()) {
            if (Training.MemBranchFlag > BranchType::Null) {
                Chart.NowBranchFlag = Training.MemBranchFlag;
            }
        }

        SetCreateSoundDataType(DX_SOUNDDATATYPE_FILE);
        if (!Chart.IsDanMode()) {
            if (!IsMulti || IsHost()) {
                Chart.SongData.Load(ToStr(LoadData.SongPath.u8string()));
            }
            else {
                Chart.SongData.Load(WaveData.data(), WaveData.size());
            }
        }
        else {
            Chart.SongData.Load(ToStr(LoadData.DanSongPath[NowSongCount].u8string()));
        }
        SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMNOPRESS);

        SetSpeed();

        Skin.Base->Playing.SE.Don.SetVolume(Chart.OriginalData.SeVolume * (Config.SEVolume / 100));
        Skin.Base->Playing.SE.Ka.SetVolume(Chart.OriginalData.SeVolume * (Config.SEVolume / 100));
        Skin.Base->Playing.SE.Balloon.SetVolume(Chart.OriginalData.SeVolume * (Config.SEVolume / 100));

        HitNote.resize(Shared.PlayerCount <= 1 ? 1 : Shared.PlayerCount);
        Chart.Judge.resize(Shared.PlayerCount <= 1 ? 1 : Shared.PlayerCount);
        Chart.Judge[0].ScoreRateGood = ScoreRateCalc(Config.JudgeGood, 25.0);
        Chart.Judge[0].ScoreRateOk = ScoreRateCalc(Config.JudgeOk, 75.0);

        if (IsMulti) {
            if (!IsLoad) {
                if (IsHost()) {
                    Shared.CourseIndex = CourseIndex;
                    Shared.FileData = FileToMem(fs::path(LoadData.ChartPath));
                    Shared.WaveData = FileToMem(fs::path(LoadData.SongPath));
                    WaveData = Shared.WaveData;
                }
                IsLoad = true;
                NowScene = Scene::MultiRoom;
                return;
            }
        }
        else if (!Config.TrainingMode || Chart.IsDanMode()) {
            WaitVSync(10);
            Chart.NowTime.Start();
        }

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
        double BMTime = 0;
        double PosTime = 0;
        bool BMFlag = false;
        bool BpmChangeFlag = false;
        bool BpmSpawnFlag = false;

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
        double AbsTime = 0;
        double StartMs = 0;
        bool Start = false;
        bool BranchFlag = false;
        IfBranchType Type = IfBranchType::Null;
        double ExpertBranch = 0;
        double MasterBranch = 0;
    };

    struct ExamStreamData {
        double ExamVals = 0.0;
        bool IsFall = false;
    };

    struct PlayData {

        void Init(bool dan_init) {
            if (dan_init) {
                ExamDatas.clear();
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

        std::vector<ExamStreamData> ExamDatas = std::vector<ExamStreamData>();
        size_t DanBalloonIndex = 0;

    };

    PlayData Chart;

    Timer MiniTaikoFlash[16];
    double MiniTaikoFlashTime = 160;

    double ChartNowTime(uint64_t elapsed) const {
        return (Chart.NowTime.GetElapsed().Second() * elapsed) * Chart.SongSpeed;
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

    size_t NowSongCount = 0;

    bool IsDanFall() const {
        return (std::ranges::any_of(Chart.ExamDatas, &ExamStreamData::IsFall));
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
    void PlayingInit() {
        if (IsMulti) {
            auto names_view = Shared.Players | std::views::transform(&PlayerData::Name);
            Names.assign(names_view.begin(), names_view.end());
            auto it = Names.begin() + Shared.MyIndex;
            std::rotate(Names.begin(), it, it + 1);
        }
        for (auto&& taiko : MiniTaikoFlash) {
            taiko.Reset();
        }
    }
    void PlayingEnd() {
        Chart.SongData.Delete();
    }
    void PlayingDraw() {

        const double NowTime = ChartNowTime(1000) + Training.Offset;

        Skin.Base->Playing.Image.BackGround.Draw({});

#ifndef __ANDROID__
        if (Chart.Movie.Handle != -1 && Config.BGBrightness > 0) {

            DrawExtendGraphF(
                Skin.Info.Resolution.X / 2 - Chart.Movie.Size.Width / 2,
                Skin.Info.Resolution.Y / 2 - Chart.Movie.Size.Height / 2,
                Skin.Info.Resolution.X / 2 + Chart.Movie.Size.Width / 2,
                Skin.Info.Resolution.Y / 2 + Chart.Movie.Size.Height / 2,
                Chart.Movie.Handle,
                FALSE);

            if ((NowTime + (Chart.OriginalData.MovieOffset * -1000)) > 128 && Chart.NowTime.IsRunning()) {
                Chart.Movie.Play();
            }

            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255 * (1 - (Config.BGBrightness / 100)));
            DrawFillBox(0, 0, Skin.Info.Resolution.X, Skin.Info.Resolution.Y, GetColor(0, 0, 0));
            SetDrawBlendMode(0, 0);
        }
#endif
        int idx = 0;

        do {

            Pos2D<float> DelayPos = {
                    0.0f,
                    idx * Skin.Base->Playing.Config.MultiPlayLaneDistance
            };

            if (Shared.PlayerCount == 4) {
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

            switch (Chart.NowBranchFlag) {
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

            static auto NoteAlpha = [&](double _one, AlphaType Type) {
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

            static auto GetNotePos = [&](NoteData& data)->Pos2D<double> {
                bool BMScroll = ScrollType::BMSCROLL == Chart.ScrollType;
                Pos2D<double> _ret;
                if (ScrollType::Normal == Chart.ScrollType) {
                    double _temp = ((data.AbsTime - NowTime) / (240 / data.BPM));
                    _ret = { _temp,_temp };
                }
                else {
                    double _bpm = data.BMFlag || data.BpmSpawnFlag ? data.BPM : Chart.NowBPM;
                    double optime = (((data.BMFlag || data.BpmSpawnFlag ? data.AbsTime : data.BMTime) - NowTime) / (240 / _bpm));
                    _ret = { optime,optime };
                }

                _ret = {
                        _ret.X *= Skin.Base->Playing.Config.LaneExtendRate * (BMScroll ? 1 : data.Scroll),
                        _ret.Y *= Skin.Base->Playing.Config.LaneExtendRate * (BMScroll ? 0 : data.Scrolli)
                };

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

            auto&& ProcNotes = Chart.RawNoteDatas;
            double _addms = ProcNotes[0].AbsTime;
            for (int i = 0, size = ProcNotes.size(); i < size; ++i) {
                NoteData& data = ProcNotes[i];

                if (data.AbsTime < NowTime) {
                    data.BMFlag = true;
                    Chart.NowBPM = data.BPM;
                }

                if (data.BpmChangeFlag) {
                    if (data.BPM * data.Measure > 0) {
                        for (int j = i + 1; j < size; ++j) {
                            auto& jdata = ProcNotes[j];
                            if (jdata.BpmChangeFlag && jdata.BPM * jdata.Measure < 0) {
                                data.BpmChangeFlag = false;
                                for (int k = j; k < size; ++k) {
                                    auto& kdata = ProcNotes[k];
                                    if (kdata.AbsTime < jdata.AbsTime) {
                                        kdata.BpmSpawnFlag = true;
                                    }
                                    else {
                                        kdata.BpmSpawnFlag = false;
                                    }
                                }
                                break;
                            }
                            if (jdata.BpmChangeFlag && jdata.BPM * jdata.Measure > 0) {
                                data.BpmChangeFlag = false;
                                for (int k = j; k < size; ++k) {
                                    ProcNotes[k].BpmSpawnFlag = false;
                                }
                                break;
                            }
                        }
                    }
                    else {
                        data.BpmChangeFlag = false;
                    }
                }

                if (data.BMFlag || i == 0) { _addms = data.AbsTime; data.BMTime = data.AbsTime; continue; }
                double _bpm = (Chart.NowBPM / ProcNotes[i - 1].BPM);
                _addms += ProcNotes[i - 1].RelaTime / _bpm;
                data.BMTime = _addms;
            }

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
                    const Pos2D<double>& dnote = {
                            GetNotePos(Chart.RawNoteDatas[data.RollEndIndex]).X,
                            GetNotePos(Chart.RawNoteDatas[data.RollEndIndex]).Y + BranchDelayPos
                    };

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

            static auto TaikoAlpha = [&](size_t index) {
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

            static auto ComboDraw = [&](uint64_t num) {
                int digit = std::digit(num);
                float offset = Skin.Base->Playing.Image.ComboNumber.Size.Width * (digit - 1) / 2;
                int i = 0;
                do {
                    Skin.Base->Playing.Image.ComboNumber.Draw({ offset, DelayPos.Y }, num % 10);
                    num /= 10;
                    ++i;
                    offset -= Skin.Base->Playing.Image.ComboNumber.Size.Width;
                } while (i < digit);
                };

            static auto ScoreDraw = [&](uint64_t num) {
                int digit = std::digit(num);
                float offset = Skin.Base->Playing.Image.ScoreNumber.Size.Width - (digit - 1) + digit;
                int i = 0;
                do {
                    Skin.Base->Playing.Image.ScoreNumber.Draw({ offset, DelayPos.Y }, num % 10);
                    num /= 10;
                    ++i;
                    offset -= Skin.Base->Playing.Image.ScoreNumber.Size.Width;
                } while (i < digit);
                };
            static auto RollDraw = [&](uint64_t num) {
                int digit = std::digit(num);
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

        } while (idx < Shared.PlayerCount && IsMulti);

        if (Shared.PlayerCount < 4) {
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
            else {
                if (Chart.OriginalData.TitleDisplay) {
                    Skin.Base->Playing.Font.Title.Draw(
                        Skin.Base->Playing.Config.TitlePos,
                        GetColor(255, 255, 255),
                        GetColor(0, 0, 0),
                        Chart.OriginalData.TitleStrlen.Dan[NowSongCount],
                        Chart.OriginalData.DanTitle[NowSongCount]
                    );
                }
                if (Chart.OriginalData.SubtitleDisplay) {
                    Skin.Base->Playing.Font.SubTitle.Draw(
                        Skin.Base->Playing.Config.SubTitlePos,
                        GetColor(255, 255, 255),
                        GetColor(0, 0, 0),
                        Chart.OriginalData.SubtitleStrlen.Dan[NowSongCount],
                        Chart.OriginalData.DanSubtitle[NowSongCount]
                    );
                }
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
            if (Chart.IsDanMode()) {

                Skin.Base->Playing.Image.DanBackGround.Draw({});

                for (size_t i = 0; i < Chart.ExamDatas.size(); i++) {

                    Skin.Base->Playing.Image.ExamProgressBar.Draw({ 0,120.0f * i }, 0);

                    auto OriginalExamData = Chart.OriginalData.ExamDatas[i];
                    auto ExamData = Chart.ExamDatas[i];
                    double Ratio = (double)ExamData.ExamVals / (double)OriginalExamData.PassVal[0];
                    float Width = Skin.Base->Playing.Image.ExamProgressBar.Size.Width * Ratio;
                    float MaxWidth = Skin.Base->Playing.Image.ExamProgressBar.Size.Width;

                    if (!ExamData.IsFall) {
                        Skin.Base->Playing.Image.ExamProgressBar.RectDraw(
                            { 0,120.0f * i },
                            { 0, Skin.Base->Playing.Image.ExamProgressBar.Size.Height },
                            { Width < MaxWidth ? Width : MaxWidth,
                              Skin.Base->Playing.Image.ExamProgressBar.Size.Height },
                            1
                        );
                    }

                    bool IsFall = ExamData.IsFall;
                    bool IsPass = OriginalExamData.PassVal[0] <= ExamData.ExamVals && OriginalExamData.Range == ExamRange::More;
                    std::string valstr = !IsFall ? std::to_string((int)ExamData.ExamVals) : "0";
                    std::string examname = magic_enum::enum_name(OriginalExamData.ExamType).data();

                    Skin.Base->Playing.Font.ExamName.Draw(
                        { Skin.Base->Playing.Config.ExamNamePos.X,
                          Skin.Base->Playing.Config.ExamNamePos.Y + (120.0f * i) },
                        GetColor(255, 255, 255),
                        GetColor(0, 0, 0),
                        GetStrlen(examname, Skin.Base->Playing.Font.ExamName.Handle),
                        examname
                    );
                    Skin.Base->Playing.Font.ExamVal.Draw(
                        { Skin.Base->Playing.Config.ExamValPos.X,
                          Skin.Base->Playing.Config.ExamValPos.Y + (120.0f * i) },
                        GetColor(255, 255 * !IsFall, 255 * !IsFall * !IsPass),
                        GetColor(0, 0, 0),
                        GetStrlen(valstr, Skin.Base->Playing.Font.ExamVal.Handle),
                        valstr
                    );
                }
            }
        }

#ifdef __ANDROID__
        Skin.Base->Playing.Image.Back.Draw({});
#endif
        if (Config.ViewDebug) {
            DrawFormatString(0, 0, GetColor(255, 255, 255), "\n\n\nNowTime:%lf\nBPM:%lf\nPath:%s", ChartNowTime(1) / Chart.SongSpeed, Chart.NowBPM * Chart.SongSpeed, Chart.OriginalData.ChartPath.u8string().c_str());
        }
    }
    void HitAction(HitType type) {
        if (Shared.PlayerCount >= 2) {
            Shared.HitKey = type;
            Shared.Judge = Chart.Judge[0];
            Shared.GetIndex = Shared.MyIndex;
        }
    }
    void BranchChange(BranchData branchdata, IfBranchType ifbranchtype, double judge) {
        if (branchdata.Type == ifbranchtype) {
            if (branchdata.ExpertBranch > judge) {
                Chart.NowBranchAnimation =
                    (Chart.NowBranchFlag == BranchType::Expert ? ABranchType::Expert_Normal :
                        (Chart.NowBranchFlag == BranchType::Master ? ABranchType::Master_Normal : ABranchType::Null));
                Chart.NowBranchFlag = BranchType::Normal;
            }
            else if (branchdata.ExpertBranch <= judge && branchdata.MasterBranch > judge) {
                Chart.NowBranchAnimation =
                    (Chart.NowBranchFlag == BranchType::Normal ? ABranchType::Normal_Expert :
                        (Chart.NowBranchFlag == BranchType::Master ? ABranchType::Master_Expert : ABranchType::Null));
                Chart.NowBranchFlag = BranchType::Expert;
            }
            else if (branchdata.MasterBranch <= judge) {
                Chart.NowBranchAnimation =
                    (Chart.NowBranchFlag == BranchType::Normal ? ABranchType::Normal_Master :
                        (Chart.NowBranchFlag == BranchType::Expert ? ABranchType::Expert_Master : ABranchType::Null));
                Chart.NowBranchFlag = BranchType::Master;
            }
        }
    }
    void PlayingProc() {

        const double NowTime = ChartNowTime(1000) + Training.Offset;

        if (Chart.NowTime.IsRunning()) {
            if (Chart.SongBlankTime < NowTime && Chart.SongBlankTime + 128 > NowTime && !Chart.SongData.IsPlay()) {
                if (Training.BarlineIndex < Chart.AllBarlineCount) {
                    Chart.SongData.Play(TRUE);
                }
            }
            else if (Chart.SongBlankTime + 5000 < NowTime && !Chart.SongData.IsPlay()) {
                if (!IsMulti && (Config.TrainingMode || (Chart.IsDanMode() && (NowSongCount < Chart.OriginalData.DanIndex.size() - 1) && !IsDanFall()))) {
                    NowSongCount += Chart.IsDanMode();
                    NowScene = Scene::Loading;
                    return;
                }
                if (!IsMulti || CheckState(3)) {
                    NowScene = Scene::Result;
                    return;
                }
                Shared.Players[Shared.MyIndex].State = 3;
            }
        }

        if (Chart.NowBranchFlag != BranchType::Null && !Chart.LevelHold) {
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
                    BranchChange(Chart.BranchDatas[i], IfBranchType::Perfect, Chart.Judge[0].Branch.Accuracy);
                    BranchChange(Chart.BranchDatas[i], IfBranchType::Roll, (double)Chart.Judge[0].Branch.Roll);
                    BranchChange(Chart.BranchDatas[i], IfBranchType::Score, (double)Chart.Judge[0].Branch.Score);
                    Chart.BranchDatas[i].BranchFlag = false;
                    Chart.BranchDatas[i].Start = false;
                    Chart.BranchAnimationTimer.Start();
                }
            }
        }

        if (IsMulti) {
            if (Shared.PlayerCount >= 2) {
                Chart.Judge[0].NoteType = '\0';
                Chart.Judge[0].HitJudge = JudgeType::None;
                if (Shared.HitKey != HitType::Null && Shared.GetIndex != Shared.MyIndex) {
                    Shared.GetIndex += (Shared.GetIndex <= Shared.MyIndex);
                    switch (Shared.HitKey) {
                    case HitType::DonLeft:
                    case HitType::DonRight:
                    case HitType::KaLeft:
                    case HitType::KaRight:
                        MiniTaikoFlash[(int)Shared.HitKey + (4 * Shared.GetIndex)].Start();
                        if (Shared.Judge.HitJudge != JudgeType::None) {
                            HitNote[Shared.GetIndex].Add(HitNoteData(Shared.Judge.NoteType, Shared.Judge.HitJudge));
                        }
                        break;
                    case HitType::DonBig:
                    case HitType::KaBig:
                        MiniTaikoFlash[((int)Shared.HitKey - 4) + (4 * Shared.GetIndex)].Start();
                        MiniTaikoFlash[((int)Shared.HitKey - 2) + (4 * Shared.GetIndex)].Start();
                        if (Shared.Judge.HitJudge != JudgeType::None) {
                            HitNote[Shared.GetIndex].Add(HitNoteData(((int)Shared.HitKey + 48) - 1, Shared.Judge.HitJudge));
                        }
                        break;
                    case HitType::Empty:
                        Chart.Judge[Shared.GetIndex].Combo = 0;
                        break;
                    }
                    Chart.Judge[Shared.GetIndex] = Shared.Judge;
                    Shared.HitKey = HitType::Null;
                }
            }
        }
        else {
            if (Config.TrainingMode && !Chart.IsDanMode()) {
                if (!Chart.NowTime.IsRunning()) {
                    if (!Training.BarlineMoveTimer.IsRunning()) {

                        static auto MoveInputProc = [&](bool direction) {
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

                        static auto WarpInputProc = [&](bool direction) {
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

                        static auto StartInputProc = [&] {
                            Chart.NowTime.Start();
                            if (Chart.SongBlankTime < NowTime
                                && Training.BarlineIndex > 0
                                && Training.BarlineIndex < Chart.AllBarlineCount) {
                                if (Chart.Movie.Handle != -1) {
                                    SeekMovieToGraph(Chart.Movie.Handle, (Chart.OriginalData.MovieOffset < 0) ? Training.Offset + Chart.OriginalData.MovieOffset * -1000 : Training.Offset - Chart.SongBlankTime);
                                }
                                Chart.SongData.SetCurrent(NowTime - Chart.SongBlankTime);
                                Chart.SongData.Play(FALSE);
                            }
                            };

                        static auto BranchChangeProc = [&](bool direction) {
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
                        Input.HitKeyProcess(VK_NEXT, KeyState::Down, [] { MoveInputProc(false); }, Config.KeyHoldProcInterval);
                        Input.HitKeyProcess(VK_PRIOR, KeyState::Down, [] { MoveInputProc(true); }, Config.KeyHoldProcInterval);
                        Input.HitKeyProcess(VK_DOWN, KeyState::Down, [] { BranchChangeProc(false); });
                        Input.HitKeyProcess(VK_UP, KeyState::Down, [] { BranchChangeProc(true); });
                        Input.HitKeyProcess(VK_END, KeyState::Down, [] { WarpInputProc(false); });
                        Input.HitKeyProcess(VK_HOME, KeyState::Down, [] { WarpInputProc(true); });

                        Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, StartInputProc);
                        Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, StartInputProc);
                        Input.HitKeyProcess(VK_SPACE, KeyState::Down, StartInputProc);
                        Input.HitKeyProcess(VK_RETURN, KeyState::Down, StartInputProc);
#else
                        Touch.Process(TouchType::RightKa, [] { MoveInputProc(false); });
                        Touch.Process(TouchType::LeftKa, [] { MoveInputProc(false); });
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

            if (Chart.IsDanMode()) {
                for (size_t i = 0; i < Chart.OriginalData.ExamDatas.size(); i++) {

                    auto OriginalExamData = Chart.OriginalData.ExamDatas[i];
                    auto&& ExamVal = Chart.ExamDatas[i].ExamVals;

                    switch (OriginalExamData.ExamType) {
                    case ExamTypes::Accuracy:
                        ExamVal = std::abs(((int)OriginalExamData.PassVal[0] * (int)OriginalExamData.Range) - (int)Chart.Judge[0].Accuracy);
                        break;
                    case ExamTypes::Good:
                        ExamVal = std::abs(((int)OriginalExamData.PassVal[0] * (int)OriginalExamData.Range) - (int)Chart.Judge[0].Good);
                        break;
                    case ExamTypes::Ok:
                        ExamVal = std::abs(((int)OriginalExamData.PassVal[0] * (int)OriginalExamData.Range) - (int)Chart.Judge[0].Ok);
                        break;
                    case ExamTypes::Bad:
                        ExamVal = std::abs(((int)OriginalExamData.PassVal[0] * (int)OriginalExamData.Range) - (int)Chart.Judge[0].Bad);
                        break;
                    case ExamTypes::Score:
                        ExamVal = std::abs(((int)OriginalExamData.PassVal[0] * (int)OriginalExamData.Range) - (int)Chart.Judge[0].Score);
                        break;
                    case ExamTypes::Roll:
                        ExamVal = std::abs(((int)OriginalExamData.PassVal[0] * (int)OriginalExamData.Range) - (int)Chart.Judge[0].Roll);
                        break;
                    case ExamTypes::HitNote:
                        ExamVal = std::abs(((int)OriginalExamData.PassVal[0] * (int)OriginalExamData.Range) - (int)Chart.Judge[0].HitNote);
                        break;
                    case ExamTypes::MaxCombo:
                        ExamVal = std::abs(((int)OriginalExamData.PassVal[0] * (int)OriginalExamData.Range) - (int)Chart.Judge[0].MaxCombo);
                        break;
                    }

                    if (OriginalExamData.Range == ExamRange::Less &&
                        Chart.ExamDatas[i].ExamVals <= 0) {
                        if (!Chart.ExamDatas[i].IsFall) {
                            Skin.Base->Playing.SE.DanFall.Play();
                            Chart.ExamDatas[i].IsFall = true;
                        }
                    }
                }
            }

            static auto BackInputProc = [&] {
                Training.Init();
                NowScene = PrevScene;
                };
            static auto ReLoadInputProc = [&] {
                if (Chart.IsDanMode()) { return; }
                NowScene = Scene::Loading;
                };

#ifndef __ANDROID__
            Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
            Input.HitKeyProcess(VK_TAB, KeyState::Down, ReLoadInputProc);
#else
            Touch.Process(TouchType::Other, BackInputProc, Skin.Base->Playing.Image.Back);
#endif
        }

        static auto JudgeNote = [&](double nowtime, char type) {

            auto& Judge = Chart.Judge[0];
            size_t rollcount = 0;
            size_t ballooncount = 0;
            NoteData* balloondata = nullptr;

            bool NextImage = false;

            for (auto&& data : Chart.RawNoteDatas) {

                bool NoteBranch = data.IsBranch == Chart.NowBranchFlag || data.IsBranch == BranchType::Null;

                if (!NoteBranch) {
                    continue;
                }
                if (data.HitFlag) {
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
                    Skin.Base->Playing.SE.Balloon.Play();
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

        for (auto&& data : Chart.RawNoteDatas) {

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

                for (auto&& data : Chart.RawNoteDatas) {

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
                            Skin.Base->Playing.SE.Don.Play();
                            MiniTaikoFlash[0 + Chart.AutoPlayLR * 2].Start();
                            Chart.AutoPlayLR = !Chart.AutoPlayLR;
                            HitAction((HitType)(0 + Chart.AutoPlayLR * 2));
                            break;
                        case '2':
                            Skin.Base->Playing.SE.Ka.Play();
                            MiniTaikoFlash[1 + Chart.AutoPlayLR * 2].Start();
                            Chart.AutoPlayLR = !Chart.AutoPlayLR;
                            HitAction((HitType)(1 + Chart.AutoPlayLR * 2));
                            break;
                        case '3':
                            Skin.Base->Playing.SE.Don.Play();
                            Skin.Base->Playing.SE.Don.Play();
                            MiniTaikoFlash[0].Start();
                            MiniTaikoFlash[2].Start();
                            HitAction(HitType::DonBig);
                            break;
                        case '4':
                            Skin.Base->Playing.SE.Ka.Play();
                            Skin.Base->Playing.SE.Ka.Play();
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
                    Skin.Base->Playing.SE.Don.Play();
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
                    Skin.Base->Playing.SE.Don.Play();
                    Chart.AutoPlayLR = !Chart.AutoPlayLR;
                    Chart.Judge[0].Roll++;
                    Chart.Judge[0].Branch.Roll++;
                    --BalloonData->BalloonCount;
                    Chart.Judge[0].Rolls.NowCount = BalloonData->BalloonCount;
                    Chart.RollViewEndTimer.Reset();
                    Chart.WaitRollTime.Start();
                    HitAction((HitType)(0 + Chart.AutoPlayLR * 2));
                    if (BalloonData->BalloonCount <= 0) {
                        Skin.Base->Playing.SE.Balloon.Play();
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
                Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, [&] {
                    Skin.Base->Playing.SE.Don.Play();
                    MiniTaikoFlash[0].Start();
                    JudgeNote(NowTime, '1');
                    HitAction(HitType::DonLeft);
                    });
                Input.HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] {
                    Skin.Base->Playing.SE.Ka.Play();
                    MiniTaikoFlash[1].Start();
                    JudgeNote(NowTime, '2');
                    HitAction(HitType::KaLeft);
                    });
                Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, [&] {
                    Skin.Base->Playing.SE.Don.Play();
                    MiniTaikoFlash[2].Start();
                    JudgeNote(NowTime, '1');
                    HitAction(HitType::DonRight);
                    });
                Input.HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] {
                    Skin.Base->Playing.SE.Ka.Play();
                    MiniTaikoFlash[3].Start();
                    JudgeNote(NowTime, '2');
                    HitAction(HitType::KaRight);
                    });
#else
                Touch.Process(TouchType::LeftDon, [&] {
                    Skin.Base->Playing.SE.Don.Play();
                    MiniTaikoFlash[0].Start();
                    JudgeNote(NowTime, '1');
                    HitAction(HitType::DonLeft);
                    });
                Touch.Process(TouchType::LeftKa, [&] {
                    Skin.Base->Playing.SE.Ka.Play();
                    MiniTaikoFlash[1].Start();
                    JudgeNote(NowTime, '2');
                    HitAction(HitType::KaLeft);
                    });
                Touch.Process(TouchType::RightDon, [&] {
                    Skin.Base->Playing.SE.Don.Play();
                    MiniTaikoFlash[2].Start();
                    JudgeNote(NowTime, '1');
                    HitAction(HitType::DonRight);
                    });
                Touch.Process(TouchType::RightKa, [&] {
                    Skin.Base->Playing.SE.Ka.Play();
                    MiniTaikoFlash[3].Start();
                    JudgeNote(NowTime, '2');
                    HitAction(HitType::KaRight);
                    });
#endif
            }
        }
    }

    int ResultIndex = 0;
    int CrownIndex = 0;

    void ScoreDataSave() {

        json data;
        JudgeData Judge = Chart.Judge[0];
        fs::path filepath = GetExecutablePath() / "scoredata.json";
        std::string ChartPath = Chart.OriginalData.ChartPath.string();
        std::string CourseName = magic_enum::enum_name((CourseType)CourseIndex).data();

        if (!fs::exists(filepath)) {
            data = json::object();
        }
        else {
            std::ifstream ifs(filepath, std::ios::binary);
            std::vector<uint8_t> bson_vec((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            data = json::from_bson(bson_vec);
            ifs.close();
        }

        data[ChartPath][CourseName]["Score"] = Judge.Score;
        data[ChartPath][CourseName]["Crown"] = CrownIndex;

        std::vector<uint8_t> v_bson = json::to_bson(data);
        std::ofstream ofs(filepath, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(v_bson.data()), v_bson.size());
    }
    ScoreData ScoreDataLoad(const std::string& chartpath, int courseindex) {

        ScoreData Score;
        fs::path filepath = GetExecutablePath() / "scoredata.json";
        std::ifstream ifs(filepath, std::ios::binary);
        std::string CourseName = magic_enum::enum_name((CourseType)courseindex).data();

        if (!ifs.is_open()) {
            ifs.close();
            return Score;
        }

        std::vector<uint8_t> bson_vec((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        json data = json::from_bson(bson_vec);

        if (data.contains(chartpath) && data[chartpath].contains(CourseName)) {
            Score.Score = data[chartpath][CourseName]["Score"];
            Score.Crown = data[chartpath][CourseName]["Crown"];
        }
        return Score;
    }
    void ResultEnd() {

        if (IsMulti) {
            std::ranges::for_each(Shared.Players, [](PlayerData& data) { data.State = 0; });
            Shared.Judge = JudgeData();
            IsLoad = false;
            ResultIndex = 0;
            return;
        }
        else if (Chart.Judge[0].Score > ScoreDataLoad(Chart.OriginalData.ChartPath.string(), CourseIndex).Score) {
            ScoreDataSave();
        }

        CrownIndex = 0;
        Chart.Init(true);
    }
    void ResultDraw() {

        SetDrawBlendMode(0, 0);
        Skin.Base->Result.Image.BackGround.Draw({});
        Skin.Base->Result.Image.JudgeScore.Draw({});
        Skin.Base->Result.Image.Accuracy.Draw({});
        Skin.Base->Result.Image.Score.Draw({});

        static auto ScoreDraw = [&](uint64_t num) {
            int digit = std::digit(num);

            float offset = 0;
            int i = 0;
            do {
                Skin.Base->Result.Image.Number.Draw({ offset + Skin.Base->Result.Config.ScorePos.X, Skin.Base->Result.Config.ScorePos.Y }, num % 10);
                num /= 10;
                ++i;
                offset -= Skin.Base->Result.Image.Number.Size.Width;
            } while (i < digit);
            };
        static auto AccuracyDraw = [&](double Rate) {
            int iRate = Rate * 100;
            int digit = std::digit(iRate) + 2;
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
        static auto JudgeDraw = [&](const Pos2D<float>& Pos, uint64_t num) {
            int digit = std::digit(num) + 1;
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
            if (!IsDanFall()) {
                CrownIndex = 2;
                for (size_t i = 0, m = 0, GoldPass = 0; i < Chart.ExamDatas.size(); ++i) {
                    if (Chart.OriginalData.ExamDatas[i].Range == ExamRange::More) {
                        ++m;
                        if (Chart.ExamDatas[i].ExamVals >= Chart.OriginalData.ExamDatas[i].PassVal[1]) {
                            ++GoldPass;
                        }
                    }
                    if (GoldPass >= m) {
                        CrownIndex = 3;
                        break;
                    }
                }
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
        if (!IsMulti) {
            Skin.Base->Result.Font.PlayerName.Draw(
                Skin.Base->Result.Config.PlayerNamePos,
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                GetStrlen(Config.PlayerName, Skin.Base->Result.Font.PlayerName.Handle),
                Config.PlayerName
            );
        }
        else {
            Skin.Base->Result.Font.PlayerName.Draw(
                Skin.Base->Result.Config.PlayerNamePos,
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                GetStrlen(Shared.Players[ResultIndex].Name, Skin.Base->Result.Font.PlayerName.Handle),
                Names[ResultIndex]
            );
        }

#ifdef __ANDROID__
        Skin.Base->Result.Image.Back.Draw({});
#endif
    }
    void ResultProc() {

        static auto BackInputProc = [&] {
            if (!IsMulti) {
                NowScene = PrevScene;
            }
            else if (IsHost()) {
                NowScene = Scene::MultiRoom;
                Shared.HitKey = HitType::Back;
            }
            };

#ifndef __ANDROID__
        Input.HitKeyProcess(VK_RETURN, KeyState::Down, BackInputProc);
#else
        Touch.Process(TouchType::Other, BackInputProc, Skin.Base->Result.Image.Back);
#endif

        if (IsMulti && Shared.PlayerCount >= 2) {

#ifndef __ANDROID__
            Input.HitKeyProcess(VK_LEFT, KeyState::Down, [&] {
                Skin.Base->SongSelect.SE.Ka.Play();
                ResultIndex > 0 ? --ResultIndex : 0;
                }, Config.KeyHoldProcInterval);
            Input.HitKeyProcess(VK_RIGHT, KeyState::Down, [&] {
                Skin.Base->SongSelect.SE.Ka.Play();
                ResultIndex < Shared.PlayerCount - 1 ? ++ResultIndex : ResultIndex;
                }, Config.KeyHoldProcInterval);
#else
            Touch.Process(TouchType::LeftKa, [&] {
                Skin.Base->SongSelect.SE.Ka.Play();
                ResultIndex > 0 ? --ResultIndex : 0;
                });
            Touch.Process(TouchType::RightKa, [&] {
                Skin.Base->SongSelect.SE.Ka.Play();
                ResultIndex < Shared.PlayerCount - 1 ? ++ResultIndex : ResultIndex;
                });
#endif
            if (!IsHost()) {
                if (Shared.HitKey == HitType::Back) {
                    NowScene = Scene::MultiRoom;
                }
            }
        }
    }

    enum class ConfigGenreData {
        Genre = 0,
        Game,
        Key,
    } ConfigGenre = ConfigGenreData::Genre;

    std::vector<std::vector<std::string>> ConfigMenuString{
#ifndef __ANDROID__
            {
                    "GameConfig",
                    "KeyConfig"
            },
#endif
            {
#ifdef __ANDROID__
                    "TouchTaikoWidth",
                    "TouchTaikoHeight",
#endif
                    "PlayerName",
                    "AutoPlay",
                    "ServerAddress",
                    "ServerPort",
                    "HiddenLevel",
                    "SuddenLevel",
                    "RandomRate",
                    "JudgeGood",
                    "JudgeOk",
                    "JudgeBad",
                    "SongOffset",
                    "JudgeOffset",
                    "ChartSpeed",
                    "SongSpeed",
                    "TrainingMode",
                    "BGBrightness",
                    "SkinName",
                    "SongDirectories",
                    "SongVolume",
                    "SEVolume",
                    "HitNoteDisp",
                    "RollSpeed",
#ifndef __ANDROID__
                    "WaitVSync",
                    "KeyHoldProcInterval",
#endif
                    "FastInput",
                    "FastDrawRate",
#ifndef __ANDROID__
                    "SoundDeviceType",
#endif
                    "Exclusive",
                    "SampleRate",
                    "BufferSize",
#ifndef __ANDROID__
                    "FullScreen",
#endif
                    "ViewDebug",
#ifndef __ANDROID__
                    "MultiBoot"
#endif
            },
#ifndef __ANDROID__
            {
                    "KaInputLeft",
                    "DonInputLeft",
                    "DonInputRight",
                    "KaInputRight",
            }
#endif
    };

    struct InputData {

        int Handle = 0;
        char Buffer[1024];
        int Int = 0;
        bool Bool = false;
        double Double = 0;
        float Float = 0;
        std::string String = "";
        std::vector<std::string> Vector;

        void Load(const std::string str = "") {
#ifndef __ANDROID__
            GetKeyInputString(Buffer, Handle);
#else
            size_t len = str.copy(Buffer, sizeof(Buffer) - 1);
            Buffer[len] = '\0';
#endif
            ConvertVal(Buffer);
        }

        void ConvertVal(std::string str) {
            Vector.clear();
            if (str.find(',') != std::string::npos) {
                Vector = split(str, ',');
            }
            else {
                try {
                    Int = std::stoi(str);
                    Double = std::stod(str);
                    Float = std::stof(str);
                }
                catch (...) {
                    Bool = str == "true" || !(str == "false") && Bool;
                    String = str.empty() ? String : str;
                    Vector.push_back(str);
                }
            }
        }
    } InputData;

    int ConfigKeyCode = 0;
    int ConfigSelector = 0;
    int ConfigKeySelector = 0;
    short ConfigInputFlag = 0;
    bool IsGenreSelected = false;

    Pos2D<float> GetConfigPos(int i) const {
#ifndef __ANDROID__
        return { 0, ConfigGenre == ConfigGenreData::Game ? (i - ConfigSelector) * Skin.Base->ConfigMenu.Config.BoxDistance : i * Skin.Base->ConfigMenu.Config.BoxDistance };
#else
        return { 0, (i - ConfigSelector) * Skin.Base->ConfigMenu.Config.BoxDistance };
#endif
    };

    void ConfigDataDraw(int i, int& j, std::string data) {
        if (i == j) {
            Skin.Base->ConfigMenu.Font.String.Draw(
                { Skin.Base->ConfigMenu.Config.ValPos.X,
                  Skin.Base->ConfigMenu.Config.ValPos.Y + GetConfigPos(i).Y },
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                data
            );
        }
        j++;
    }
    void ConfigVectorDraw(int i, int& j, std::vector<std::string> data) {
        if (i == j) {
            std::string str;
            for (size_t c = 0; c < data.size(); c++) {
                str += data[c];
                if (c < data.size() - 1) {
                    str += ',';
                }
            }
            Skin.Base->ConfigMenu.Font.String.Draw(
                { Skin.Base->ConfigMenu.Config.ValPos.X,
                  Skin.Base->ConfigMenu.Config.ValPos.Y + GetConfigPos(i).Y },
                GetColor(255, 255, 255),
                GetColor(0, 0, 0),
                str);
        }
        j++;
    }
    void ConfigKeyDraw(int i, int& j, std::vector<std::string> data) {
        if (i == j) {
            for (size_t c = 0; c < data.size(); c++) {
                Skin.Base->ConfigMenu.Font.String.Draw({
                                                               Skin.Base->ConfigMenu.Config.ValPos.X + (100 * c),
                                                               Skin.Base->ConfigMenu.Config.ValPos.Y + GetConfigPos(i).Y }, (i == ConfigSelector) && (c == ConfigKeySelector) ? GetColor(255, 255, 0) : GetColor(255, 255, 255), GetColor(0, 0, 0), data[c]
                                                               );
            }
        }
        j++;
    }

    template<typename T, typename I>
    void ConfigDataInput(int& i, T& data, I& input) {
        if (ConfigSelector == i) {
            if (ConfigInputFlag == 1) {
                input = data;
            }
            if (ConfigInputFlag == 2) {
                data = input;
            }
        }
        i++;
    }

    void ConfigMenuEnd() {
        ConfigSelector = 0;
        ConfigKeySelector = 0;
    }
    void ConfigMenuDraw() {

        Skin.Base->ConfigMenu.Image.BackGround.Draw({});

        for (int i = (ConfigSelector - 8); i < (ConfigSelector + 8); ++i) {
            if (i < 0 || i >= ConfigMenuString[(int)ConfigGenre].size()) {
                continue;
            }
            unsigned int c = 100 * (ConfigSelector == i);
            SetDrawAddColor(c, c, c);
            SetDrawBlendMode(DX_BLENDGRAPHTYPE_ALPHA, (ConfigInputFlag == 0) ? 255 : 50);
            Skin.Base->ConfigMenu.Image.Box.Draw(GetConfigPos(i));
            SetDrawAddColor(c, c, c);
            Skin.Base->ConfigMenu.Font.String.Draw({ Skin.Base->ConfigMenu.Config.StringPos.X,Skin.Base->ConfigMenu.Config.StringPos.Y + GetConfigPos(i).Y }, GetColor(255, 255, 255), GetColor(0, 0, 0), GetStrlen(ConfigMenuString[(int)ConfigGenre][i], Skin.Base->ConfigMenu.Font.String.Handle), ConfigMenuString[(int)ConfigGenre][i]);

            SetDrawAddColor(0, 0, 0);
            if (ConfigInputFlag == 0) {
                SetDrawBlendMode(0, 0);
            }

#ifndef __ANDROID__
            if (CheckKeyInput(InputData.Handle) == 0) {
                DrawKeyInputString(640, 340, InputData.Handle);
            }
            if (int j = 0; ConfigGenre == ConfigGenreData::Game) {
                ConfigDataDraw(i, j, Config.PlayerName);
                ConfigDataDraw(i, j, Config.AutoPlay ? "true" : "false");
                ConfigDataDraw(i, j, Config.ServerAddress);
                ConfigDataDraw(i, j, std::to_string(Config.ServerPort));
                ConfigDataDraw(i, j, std::to_string(Config.HiddenLevel));
                ConfigDataDraw(i, j, std::to_string(Config.SuddenLevel));
                ConfigDataDraw(i, j, std::to_string(Config.RandomRate));
                ConfigDataDraw(i, j, std::to_string(Config.JudgeGood));
                ConfigDataDraw(i, j, std::to_string(Config.JudgeOk));
                ConfigDataDraw(i, j, std::to_string(Config.JudgeBad));
                ConfigDataDraw(i, j, std::to_string(Config.SongOffset));
                ConfigDataDraw(i, j, std::to_string(Config.JudgeOffset));
                ConfigDataDraw(i, j, std::to_string(Config.ChartSpeed));
                ConfigDataDraw(i, j, std::to_string(Config.SongSpeed));
                ConfigDataDraw(i, j, Config.TrainingMode ? "true" : "false");
                ConfigDataDraw(i, j, std::to_string(Config.BGBrightness));
                ConfigDataDraw(i, j, Config.SkinName);
                ConfigVectorDraw(i, j, Config.SongDirectories);
                ConfigDataDraw(i, j, std::to_string(Config.SongVolume));
                ConfigDataDraw(i, j, std::to_string(Config.SEVolume));
                ConfigDataDraw(i, j, Config.HitNoteDisp ? "true" : "false");
                ConfigDataDraw(i, j, std::to_string(Config.RollSpeed));
                ConfigDataDraw(i, j, Config.WaitVSync ? "true" : "false");
                ConfigDataDraw(i, j, std::to_string(Config.KeyHoldProcInterval));
                ConfigDataDraw(i, j, Config.FastInput ? "true" : "false");
                ConfigDataDraw(i, j, std::to_string(Config.FastDrawRate));
                ConfigDataDraw(i, j, std::to_string(Config.SoundDeviceType));
                ConfigDataDraw(i, j, Config.Exclusive ? "true" : "false");
                ConfigDataDraw(i, j, std::to_string(Config.SampleRate));
                ConfigDataDraw(i, j, std::to_string(Config.BufferSize));
                ConfigDataDraw(i, j, Config.FullScreen ? "true" : "false");
                ConfigDataDraw(i, j, Config.ViewDebug ? "true" : "false");
                ConfigDataDraw(i, j, Config.MultiBoot ? "true" : "false");
            }
            if (int j = 0; ConfigGenre == ConfigGenreData::Key) {

                ConfigKeyDraw(i, j, Config.KeyNameKaLeft);
                ConfigKeyDraw(i, j, Config.KeyNameDonLeft);
                ConfigKeyDraw(i, j, Config.KeyNameDonRight);
                ConfigKeyDraw(i, j, Config.KeyNameKaRight);

                if (ConfigInputFlag == 1) {

                    SetDrawAddColor(0, 0, 0);
                    SetDrawBlendMode(0, 0);

                    Skin.Base->ConfigMenu.Font.String.Draw(
                        { 640,300 },
                        GetColor(255, 255, 255),
                        GetColor(0, 0, 0),
                        "Press Any Key"
                    );
                }
            }
#else
            int j = 0;
            ConfigDataDraw(i, j, std::to_string(Config.TouchTaikoWidth));
            ConfigDataDraw(i, j, std::to_string(Config.TouchTaikoHeight));
            ConfigDataDraw(i, j, Config.PlayerName);
            ConfigDataDraw(i, j, Config.AutoPlay ? "true" : "false");
            ConfigDataDraw(i, j, Config.ServerAddress);
            ConfigDataDraw(i, j, std::to_string(Config.ServerPort));
            ConfigDataDraw(i, j, std::to_string(Config.HiddenLevel));
            ConfigDataDraw(i, j, std::to_string(Config.SuddenLevel));
            ConfigDataDraw(i, j, std::to_string(Config.RandomRate));
            ConfigDataDraw(i, j, std::to_string(Config.JudgeGood));
            ConfigDataDraw(i, j, std::to_string(Config.JudgeOk));
            ConfigDataDraw(i, j, std::to_string(Config.JudgeBad));
            ConfigDataDraw(i, j, std::to_string(Config.SongOffset));
            ConfigDataDraw(i, j, std::to_string(Config.JudgeOffset));
            ConfigDataDraw(i, j, std::to_string(Config.ChartSpeed));
            ConfigDataDraw(i, j, std::to_string(Config.SongSpeed));
            ConfigDataDraw(i, j, Config.TrainingMode ? "true" : "false");
            ConfigDataDraw(i, j, std::to_string(Config.BGBrightness));
            ConfigDataDraw(i, j, Config.SkinName);
            ConfigVectorDraw(i, j, Config.SongDirectories);
            ConfigDataDraw(i, j, std::to_string(Config.SongVolume));
            ConfigDataDraw(i, j, std::to_string(Config.SEVolume));
            ConfigDataDraw(i, j, Config.HitNoteDisp ? "true" : "false");
            ConfigDataDraw(i, j, std::to_string(Config.RollSpeed));
            ConfigDataDraw(i, j, Config.FastInput ? "true" : "false");
            ConfigDataDraw(i, j, std::to_string(Config.FastDrawRate));
            ConfigDataDraw(i, j, Config.Exclusive ? "true" : "false");
            ConfigDataDraw(i, j, std::to_string(Config.SampleRate));
            ConfigDataDraw(i, j, std::to_string(Config.BufferSize));
            ConfigDataDraw(i, j, Config.ViewDebug ? "true" : "false");
#endif
        }
        SetDrawAddColor(0, 0, 0);
        SetDrawBlendMode(0, 0);

#ifdef __ANDROID__
        Skin.Base->ConfigMenu.Image.Back.Draw({});
#endif
    }
    void ConfigMenuProc() {

        static auto BackInputProc = [&] {
            if (ConfigGenre == ConfigGenreData::Genre) {
                NowScene = PrevScene;
            }
            else {
                ConfigGenre = ConfigGenreData::Genre;
            }
            ConfigSelector = 0;
            };

        static auto KaInputProc = [&](bool direction) {
            Skin.Base->Title.SE.Ka.Play();
            if (!direction) {
                ConfigSelector <= 0 ? 0 : ConfigSelector--;
            }
            else {
                ConfigSelector >= ConfigMenuString[(int)ConfigGenre].size() - 1 ? ConfigSelector = ConfigMenuString[(int)ConfigGenre].size() - 1 : ConfigSelector++;
            }
            };

#ifndef __ANDROID__
        if (ConfigInputFlag == 0) {

            Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, BackInputProc);
            Input.HitKeyProcess(VK_UP, KeyState::Down, [&] { KaInputProc(false); }, Config.KeyHoldProcInterval);
            Input.HitKeyProcess(VK_DOWN, KeyState::Down, [&] { KaInputProc(true); }, Config.KeyHoldProcInterval);

            if (ConfigGenre == ConfigGenreData::Key) {
                Input.HitKeyProcess(VK_LEFT, KeyState::Down, [&] {
                    Skin.Base->Title.SE.Ka.Play();
                    ConfigKeySelector <= 0 ? 0 : ConfigKeySelector--;
                    }, Config.KeyHoldProcInterval);
                Input.HitKeyProcess(VK_RIGHT, KeyState::Down, [&] {
                    Skin.Base->Title.SE.Ka.Play();
                    ConfigKeySelector >= 4 - 1 ? 4 : ConfigKeySelector++;
                    }, Config.KeyHoldProcInterval);
            }
        }
        else {

            if (ConfigGenre == ConfigGenreData::Game) {
                if (CheckKeyInput(InputData.Handle) != 0) {
                    InputData.Load();
                    ConfigInputFlag = 2;
                }
            }
            if (ConfigGenre == ConfigGenreData::Key) {
                for (int i = 0; i < 256; i++) {
                    Input.HitKeyProcess(i, KeyState::Down, [&] {
                        ConfigKeyCode = i;
                        switch (ConfigKeyCode) {
                        case 27:
                        case 37:
                        case 38:
                        case 39:
                        case 40:
                            ConfigKeyCode = 0;
                            break;
                        case 13:
                            ConfigKeyCode = 0;
                            ConfigInputFlag = 2;
                            Skin.Base->Title.SE.Don.Play();
                            break;
                        default:
                            ConfigInputFlag = 2;
                            Skin.Base->Title.SE.Don.Play();
                            break;
                        }
                        });
                }
            }
        }

        Input.HitKeyProcess(VK_RETURN, KeyState::Down, [&] {
            Skin.Base->Title.SE.Don.Play();
            if (ConfigGenre == ConfigGenreData::Genre) {
                ConfigGenre = (ConfigGenreData)(ConfigSelector + 1);
                ConfigSelector = 0;
            }
            else if (ConfigInputFlag == 0) {
                if (ConfigGenre == ConfigGenreData::Game) {
                    ConfigInputFlag = 1;
                    InputData.Handle = MakeKeyInput(255, false, true, false);
                    SetActiveKeyInput(InputData.Handle);
                    SetKeyInputStringFont(Skin.Base->ConfigMenu.Font.String.Handle);
                }
                if (ConfigGenre == ConfigGenreData::Key) {
                    ConfigInputFlag = 1;
                    ConfigKeyCode = 0;
                }
            }
            });

        if (ConfigGenre != ConfigGenreData::Genre) {
            if (int i = 0; ConfigInputFlag >= 1) {
                if (ConfigGenre == ConfigGenreData::Game) {
                    ConfigDataInput(i, Config.PlayerName, InputData.String);
                    ConfigDataInput(i, Config.AutoPlay, InputData.Bool);
                    ConfigDataInput(i, Config.ServerAddress, InputData.String);
                    ConfigDataInput(i, Config.ServerPort, InputData.Int);
                    ConfigDataInput(i, Config.HiddenLevel, InputData.Double);
                    ConfigDataInput(i, Config.SuddenLevel, InputData.Double);
                    ConfigDataInput(i, Config.RandomRate, InputData.Int);
                    ConfigDataInput(i, Config.JudgeGood, InputData.Double);
                    ConfigDataInput(i, Config.JudgeOk, InputData.Double);
                    ConfigDataInput(i, Config.JudgeBad, InputData.Double);
                    ConfigDataInput(i, Config.SongOffset, InputData.Double);
                    ConfigDataInput(i, Config.JudgeOffset, InputData.Double);
                    ConfigDataInput(i, Config.ChartSpeed, InputData.Double);
                    ConfigDataInput(i, Config.SongSpeed, InputData.Double);
                    ConfigDataInput(i, Config.TrainingMode, InputData.Bool);
                    ConfigDataInput(i, Config.BGBrightness, InputData.Double);
                    ConfigDataInput(i, Config.SkinName, InputData.String);
                    ConfigDataInput(i, Config.SongDirectories, InputData.Vector);
                    ConfigDataInput(i, Config.SongVolume, InputData.Float);
                    ConfigDataInput(i, Config.SEVolume, InputData.Float);
                    ConfigDataInput(i, Config.HitNoteDisp, InputData.Bool);
                    ConfigDataInput(i, Config.RollSpeed, InputData.Double);
                    ConfigDataInput(i, Config.WaitVSync, InputData.Bool);
                    ConfigDataInput(i, Config.KeyHoldProcInterval, InputData.Double);
                    ConfigDataInput(i, Config.FastInput, InputData.Bool);
                    ConfigDataInput(i, Config.FastDrawRate, InputData.Double);
                    ConfigDataInput(i, Config.SoundDeviceType, InputData.Int);
                    ConfigDataInput(i, Config.Exclusive, InputData.Bool);
                    ConfigDataInput(i, Config.SampleRate, InputData.Int);
                    ConfigDataInput(i, Config.BufferSize, InputData.Int);
                    ConfigDataInput(i, Config.FullScreen, InputData.Bool);
                    ConfigDataInput(i, Config.ViewDebug, InputData.Bool);
                    ConfigDataInput(i, Config.MultiBoot, InputData.Bool);
                }
                if (ConfigGenre == ConfigGenreData::Key) {
                    ConfigDataInput(i, Config.KaInputLeft[ConfigKeySelector], ConfigKeyCode);
                    ConfigDataInput(i, Config.DonInputLeft[ConfigKeySelector], ConfigKeyCode);
                    ConfigDataInput(i, Config.DonInputRight[ConfigKeySelector], ConfigKeyCode);
                    ConfigDataInput(i, Config.KaInputRight[ConfigKeySelector], ConfigKeyCode);
                }
            }
            if (ConfigInputFlag == 2) {

                Config.Write();
                ConfigKeyCode = 0;
                ConfigInputFlag = 0;
                DeleteKeyInput(InputData.Handle);
                InputData.Handle = 0;

                if (IsMulti) {

                    Shared.Players[Shared.MyIndex].Name = Config.PlayerName;

                    if (IsHost()) {
                        Shared.SongSpeed = Config.SongSpeed;
                    }
                }
            }
        }
#else
        Touch.Process(TouchType::Other, BackInputProc, Skin.Base->ConfigMenu.Image.Back);
        Touch.Process(TouchType::LeftKa, [&] { KaInputProc(false); });
        Touch.Process(TouchType::RightKa, [&] { KaInputProc(true); });
        Touch.Process(TouchType::Don, [&] {
            Skin.Base->Title.SE.Don.Play();
            ConfigInputFlag = 1;
            });

        if (int i = 0; ConfigInputFlag >= 1) {
            ConfigDataInput(i, Config.TouchTaikoWidth, InputData.Double);
            ConfigDataInput(i, Config.TouchTaikoWidth, InputData.Double);
            ConfigDataInput(i, Config.PlayerName, InputData.String);
            ConfigDataInput(i, Config.AutoPlay, InputData.Bool);
            ConfigDataInput(i, Config.ServerAddress, InputData.String);
            ConfigDataInput(i, Config.ServerPort, InputData.Int);
            ConfigDataInput(i, Config.HiddenLevel, InputData.Double);
            ConfigDataInput(i, Config.SuddenLevel, InputData.Double);
            ConfigDataInput(i, Config.RandomRate, InputData.Int);
            ConfigDataInput(i, Config.JudgeGood, InputData.Double);
            ConfigDataInput(i, Config.JudgeOk, InputData.Double);
            ConfigDataInput(i, Config.JudgeBad, InputData.Double);
            ConfigDataInput(i, Config.SongOffset, InputData.Double);
            ConfigDataInput(i, Config.JudgeOffset, InputData.Double);
            ConfigDataInput(i, Config.ChartSpeed, InputData.Double);
            ConfigDataInput(i, Config.SongSpeed, InputData.Double);
            ConfigDataInput(i, Config.TrainingMode, InputData.Bool);
            ConfigDataInput(i, Config.BGBrightness, InputData.Double);
            ConfigDataInput(i, Config.SkinName, InputData.String);
            ConfigDataInput(i, Config.SongDirectories, InputData.Vector);
            ConfigDataInput(i, Config.SongVolume, InputData.Float);
            ConfigDataInput(i, Config.SEVolume, InputData.Float);
            ConfigDataInput(i, Config.HitNoteDisp, InputData.Bool);
            ConfigDataInput(i, Config.RollSpeed, InputData.Double);
            ConfigDataInput(i, Config.FastInput, InputData.Bool);
            ConfigDataInput(i, Config.FastDrawRate, InputData.Double);
            ConfigDataInput(i, Config.Exclusive, InputData.Bool);
            ConfigDataInput(i, Config.SampleRate, InputData.Int);
            ConfigDataInput(i, Config.BufferSize, InputData.Int);
            ConfigDataInput(i, Config.ViewDebug, InputData.Bool);

            if (ConfigInputFlag == 1) {
                InputData.Load(GetInputText(ConfigMenuString[0][ConfigSelector]));
                ConfigInputFlag = 2;
            }
            else if (ConfigInputFlag == 2) {

                ellipse.Load();
                Config.Write();
                ConfigKeyCode = 0;
                ConfigInputFlag = 0;
                InputData.Handle = 0;

                if (IsMulti) {
                    Shared.Players[Shared.MyIndex].Name = Config.PlayerName;
                    if (IsHost()) {
                        Shared.SongSpeed = Config.SongSpeed;
                    }
                }
            }
        }
#endif
    }

    bool EndFlag = false;

    Timer AverageTimer;
    Timer FPSTimer;
    double BackFPS = GetRefreshRate();
    uint64_t NowFPS = 0;

    Timer TPSTimer;
    uint64_t NowTPS = 10000;

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
        SetWaitVSyncFlag(Config.WaitVSync);
        SetDoubleStartValidFlag(Config.MultiBoot);
        ChangeWindowMode(!Config.FullScreen);
        SetMainWindowText("FreedomApopicStyle");
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
#else
        ellipse.Load();
#endif
        return true;
    }

    void Draw() {

        if (FPSTimer.IsRunning()) {
            BackFPS = (BackFPS + (1 / FPSTimer.GetElapsed().Second())) / 2;
        }
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
            DrawFormatString(0, 0, GetColor(255, 255, 255), "FPS:%llu\nTPS:%llu\nDrawTime:%.12lf", NowFPS, NowTPS, NowDrawTime);
        }
    }
    void Proc() {

        if (TPSTimer.IsRunning()) {
            NowTPS = (NowTPS + (1 / TPSTimer.GetElapsed().Second())) / 2;
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
        }
    }

    static inline std::atomic_bool _waitvsyncLog = false;
    static inline std::mutex _syncmtx;
    static inline void _LogUpdate(bool* endflag) {
        ;
        while (true) {
            WaitVSync(1);
            if (*endflag) { break; }
            std::lock_guard<std::mutex> lock(_syncmtx);
            _waitvsyncLog = true;
        }
    }
    static inline bool GetVSyncWaitLog() {
        std::lock_guard<std::mutex> lock(_syncmtx);
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

            auto prev = Shared;

            if (ProcessMessage() == -1 || NowScene == Scene::End) {
                EndFlag = true;
            }
            if (EndFlag) {
                break;
            }
            if (Config.WaitVSync && Config.FastInput) {
                if (GetVSyncWaitLog() && drawflag) {
                    WaitTimer.Start();
                    waittime = (uint64_t)(1000000000 * Config.FastDrawRate / GetRefreshRate());
                    drawflag = false;
                }
                Proc();
                if ((uint64_t)WaitTimer.GetElapsed().NanoSecond() > waittime) {
                    Draw();
                    ScreenFlip();
                    ClearDrawScreen();
                    WaitTimer.Reset();
                    drawflag = true;
                }
            }
            else {
                Draw();
                Proc();
                ScreenFlip();
                ClearDrawScreen();
            }
            if (Shared != prev) {
                Send(Shared);
            }
            if (IsMulti) {
                Recv(Shared);
            }
        }

        DxLib_End();
        thd.join();
    }
};