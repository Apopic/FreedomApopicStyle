#pragma once
#include "Library/Include.h"
#include <filesystem>
#include <fstream>
#include <complex>
#include <thread>
#include <mutex>

namespace fs = std::filesystem;
using namespace libarrier;

class _ConfigData {
public:

	_ConfigData() {
		Load();
	}

	void Load() {

		std::ifstream ifs("config.json");

		if (!ifs.is_open()) {
			ifs.close();
			Write();
			ifs = std::ifstream("config.json");
		}

		json data = json::parse(ifs);

#define JSONDATA(name) name = data.value(#name, name)\

		JSONDATA(PlayerName);
		JSONDATA(AutoPlay);
		JSONDATA(RandomRate);
		JSONDATA(HiddenLevel);
		JSONDATA(SuddenLevel);
		JSONDATA(JudgeGood);
		JSONDATA(JudgeOk);
		JSONDATA(JudgeBad);
		JSONDATA(SongOffset);
		JSONDATA(ChartSpeed);
		JSONDATA(SongSpeed);
		JSONDATA(BGBrightness);
		JSONDATA(SkinName);
		JSONDATA(SongDirectories);
		JSONDATA(SongVolume);
		JSONDATA(SEVolume);
		JSONDATA(HitNoteDisp);
		JSONDATA(RollSpeed);
		JSONDATA(WaitVSync);
		JSONDATA(FastInput);
		JSONDATA(FastDrawRate);
		JSONDATA(SoundDeviceType);
		JSONDATA(Exclusive);
		JSONDATA(SampleRate);
		JSONDATA(BufferSize);
		JSONDATA(FullScreen);
		JSONDATA(ViewDebug);
		JSONDATA(KaInputLeft);
		JSONDATA(DonInputLeft);
		JSONDATA(DonInputRight);
		JSONDATA(KaInputRight);

#undef JSONDATA

		ifs.close();
	}

	void Write() {

#define JSONDATA(name) {#name, name}
		nlohmann::ordered_json data = {
			JSONDATA(PlayerName),
			JSONDATA(AutoPlay),
			JSONDATA(RandomRate),
			JSONDATA(HiddenLevel),
			JSONDATA(SuddenLevel),
			JSONDATA(JudgeGood),
			JSONDATA(JudgeOk),
			JSONDATA(JudgeBad),
			JSONDATA(SongOffset),
			JSONDATA(ChartSpeed),
			JSONDATA(SongSpeed),
			JSONDATA(BGBrightness),
			JSONDATA(SkinName),
			JSONDATA(SongDirectories),
			JSONDATA(SongVolume),
			JSONDATA(SEVolume),
			JSONDATA(HitNoteDisp),
			JSONDATA(RollSpeed),
			JSONDATA(WaitVSync),
			JSONDATA(FastInput),
			JSONDATA(FastDrawRate),
			JSONDATA(SoundDeviceType),
			JSONDATA(Exclusive),
			JSONDATA(SampleRate),
			JSONDATA(BufferSize),
			JSONDATA(FullScreen),
			JSONDATA(ViewDebug),
			JSONDATA(KaInputLeft),
			JSONDATA(DonInputLeft),
			JSONDATA(DonInputRight),
			JSONDATA(KaInputRight),
		};
#undef JSONDATA

		KeyNameKaLeft = GetKeyName(KaInputLeft);
		KeyNameDonLeft = GetKeyName(DonInputLeft);
		KeyNameDonRight = GetKeyName(DonInputRight);
		KeyNameKaRight = GetKeyName(KaInputRight);

		std::ofstream ofs("config.json");
		ofs << data.dump(4) << "\n";
		ofs.close();
	}

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

	std::string PlayerName = "NoName";
	size_t PlayerNameStrlen = 0;

	bool AutoPlay = false;
	bool HitNoteDisp = true;
	bool WaitVSync = true;
	bool FastInput = true;
	bool Exclusive = false;
	bool FullScreen = false;
	bool ViewDebug = false;

	int RandomRate = 0;
	int SoundDeviceType = 0; // None = 0, WASAPI = 1, XAudio = 2, MMEwaveOut = 3, ASIO = 4
	int SampleRate = 192000;
	int BufferSize = 480;

	double HiddenLevel = 0.0;
	double SuddenLevel = 0.0;
	double JudgeGood = 25;
	double JudgeOk = 75;
	double JudgeBad = 100;
	double SongOffset = 0;
	double ChartSpeed = 1;
	double SongSpeed = 1;
	double BGBrightness = 100;
	double RollSpeed = 10;
	double FastDrawRate = 0.5;

	float SongVolume = 62;
	float SEVolume = 70;

	std::string SkinName = "Default";
	std::vector<std::string> SongDirectories{ "Songs" };

	std::vector<int> KaInputLeft{ 'D','S',0,0 };
	std::vector<int> DonInputLeft{ 'F','G',0,0 };
	std::vector<int> DonInputRight{ 'J','H',0,0 };
	std::vector<int> KaInputRight{ 'K','L',0,0 };

	std::vector<std::string> KeyNameKaLeft{ "D","S","*","*" };
	std::vector<std::string> KeyNameDonLeft{ "F","G","*","*" };
	std::vector<std::string> KeyNameDonRight{ "J","H","*","*" };
	std::vector<std::string> KeyNameKaRight{ "K","L","*","*" };

} Config;

class _Skin {
public:

	_Skin() {
		for (const auto& f : fs::recursive_directory_iterator("Skins")) {
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

		std::ifstream ifs(FilePath);
		json data = json::parse(ifs);
		ifs.close();

		std::string SkinDir = fs::path(FilePath).parent_path().string() + "\\";

		const json& Infodata = data["Info"];
		const json& Titledata = data["Base"]["Title"];
		const json& ModeSelectdata = data["Base"]["ModeSelect"];
		const json& SongSelectdata = data["Base"]["SongSelect"];
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

		DataLoad(SongSelect, Image, BackGround);
		DataLoad(SongSelect, Image, Box);
		DataLoad(SongSelect, Image, TitleBox);
		DataLoad(SongSelect, Image, CourseBox);
		DataLoad(SongSelect, Image, Crown);

		DataLoad(SongSelect, Font, Title);
		DataLoad(SongSelect, Font, SubTitle);
		DataLoad(SongSelect, Font, BoxTitle);
		DataLoad(SongSelect, Font, BoxSubTitle);
		DataLoad(SongSelect, Font, Course);
		DataLoad(SongSelect, Font, Level);

		DataLoad(SongSelect, SE, Don);
		DataLoad(SongSelect, SE, Ka);

#pragma endregion

#pragma region Playing

		ValLoad(Playing, Config, TitlePos);
		ValLoad(Playing, Config, SubTitlePos);
		ValLoad(Playing, Config, PlayerNamePos);
		ValLoad(Playing, Config, LaneExtendRate);
		ValLoad(Playing, Config, JudgeUpperExplosionFrameTime);
		ValLoad(Playing, Config, GoGoFireFrameTime);

		DataLoad(Playing, Image, Box);
		DataLoad(Playing, Image, BackGround);
		DataLoad(Playing, Image, LaneFrame);
		DataLoad(Playing, Image, Lane);
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

		DataLoad(Playing, Font, Title);
		DataLoad(Playing, Font, SubTitle);
		DataLoad(Playing, Font, PlayerName);

		DataLoad(Playing, SE, Don);
		DataLoad(Playing, SE, Ka);
		DataLoad(Playing, SE, Balloon);

#pragma endregion

#pragma region Result

		ValLoad(Result, Config, TitlePos);
		ValLoad(Result, Config, SubTitlePos);
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

		DataLoad(ConfigMenu, Font, String);

		DataLoad(ConfigMenu, SE, Don);
		DataLoad(ConfigMenu, SE, Ka);

#pragma endregion

#pragma endregion

#undef ConfLoad
#undef DataLoad

		Config.PlayerNameStrlen = GetStrlen(Config.PlayerName, Skin.Base->Playing.Font.PlayerName.Handle);
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
			} Config;
			struct _Image {
				GraphData BackGround;
				GraphData Box;
				GraphData TitleBox;
				GraphData CourseBox;
				GraphData Crown;
			} Image;
			struct _Font {
				FontData Title;
				FontData SubTitle;
				FontData BoxTitle;
				FontData BoxSubTitle;
				FontData Course;
				FontData Level;
			} Font;
			struct _SE {
				SoundData Don;
				SoundData Ka;
			} SE;
			struct _BGM {
			} BGM;
		} SongSelect;
		struct _Playing {
			struct _Config {
				Pos2D<float> TitlePos;
				Pos2D<float> SubTitlePos;
				Pos2D<float> PlayerNamePos;
				double LaneExtendRate;
				double JudgeUpperExplosionFrameTime;
				double GoGoFireFrameTime;
			} Config;
			struct _Image {
				GraphData Box;
				GraphData BackGround;
				GraphData LaneFrame;
				GraphData Lane;
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
			} Image;
			struct _Font {
				FontData Title;
				FontData SubTitle;
				FontData PlayerName;
			} Font;
			struct _SE {
				SoundData Don;
				SoundData Ka;
				SoundData Balloon;
			} SE;
			struct _BGM {
			} BGM;
		} Playing;
		struct _Result {
			struct _Config {
				Pos2D<float> TitlePos;
				Pos2D<float> SubTitlePos;
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
			} Image;
			struct _Font {
				FontData Title;
				FontData SubTitle;
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

class Game {
public:

	enum class Scene {
		Null,
		End,
		Title,
		ModeSelect,
		SongSelect,
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

	void TitleInit() {
		Skin.Base->Title.SE.Don.SetVolume(Config.SEVolume);
		Skin.Base->Title.SE.Ka.SetVolume(Config.SEVolume);
	}

	void TitleDraw() {
		Skin.Base->Title.Image.BackGround.Draw({ 0, 0 });
		Skin.Base->Title.Image.Logo.Draw({});
	}

	void TitleProc() {

		Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] {
			EndFlag = true;
			});
		static auto DonKeyProc = [&] {
			Skin.Base->Title.SE.Don.Play();
			NowScene = Scene::ModeSelect;
			};

		Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, DonKeyProc);
		Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, DonKeyProc);
		Input.HitKeyProcess(VK_RETURN, KeyState::Down, DonKeyProc);
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
		Chart.Init();
		DemoSongPlayBlank.Reset();
		DemoSong.Delete();
		Skin.Base->ModeSelect.SE.Don.SetVolume(Config.SEVolume);
		Skin.Base->ModeSelect.SE.Ka.SetVolume(Config.SEVolume);
	}

	void ModeSelectDraw() {
		Skin.Base->Title.Image.BackGround.Draw({ 0, 0 });
		for (size_t i = 0; i < (size_t)Mode::Count; ++i) {
			unsigned int c = 100 * ((size_t)ModeSelector == i);
			SetDrawAddColor(c, c, c);
			Skin.Base->ModeSelect.Image.Box.Draw({ 0, Skin.Base->ModeSelect.Config.BoxDistance * i });
			SetDrawAddColor(0, 0, 0);
			Skin.Base->ModeSelect.Font.Modes.Draw({
				Skin.Base->ModeSelect.Config.ModesPos.X,
				Skin.Base->ModeSelect.Config.ModesPos.Y + (Skin.Base->ModeSelect.Config.BoxDistance * i) },
				GetColor(255, 255, 255),
				GetColor(0, 0, 0),
				magic_enum::enum_name((Mode)i).data());
		}
	}

	void ModeSelectProc() {

		Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] {
			NowScene = Scene::Title;
			});

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
				(int&)ModeSelector -= 1;
				if (ModeSelector <= Mode::Single) { ModeSelector = Mode::Single; }
			}
			else {
				(int&)ModeSelector += 1;
				if (ModeSelector >= Mode::Count) { ModeSelector = Mode::End; }
			}
			};

		Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, DonKeyProc);
		Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, DonKeyProc);
		Input.HitKeyProcess(VK_RETURN, KeyState::Down, DonKeyProc);

		Input.HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { KaKeyProc(false); });
		Input.HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { KaKeyProc(true); });
		Input.HitKeyesProcess({ VK_UP,VK_LEFT }, KeyState::Down, [&] { KaKeyProc(false); });
		Input.HitKeyesProcess({ VK_DOWN,VK_RIGHT }, KeyState::Down, [&] { KaKeyProc(true); });
	}

	enum class CourseType {
		Null = -1,
		Easy,
		Normal,
		Hard,
		Oni,
		Edit,
		Count
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
		} Strlen;

		std::string Title = "";
		std::string Subtitle = "";
		bool TitleDisplay = true;
		bool SubtitleDisplay = true;
		StrlenData TitleStrlen = StrlenData();
		StrlenData SubtitleStrlen = StrlenData();
		double BPM = 120.0;
		double SongOffset = 0.0;
		double MovieOffset = 0.0;
		double DemoStart = 0.0;
		float SongVolume = 100.0;
		float SeVolume = 100.0;
		std::u8string SongPath = u8"";
		std::u8string MoviePath = u8"";
		std::u8string ChartPath = u8"";
		std::string SongLink = "";
		CourseData Courses[(size_t)CourseType::Count];

		bool Load(const fs::path& path) {
			std::ifstream ifs(path);
			if (!ifs.is_open()) {
				return false;
			}

			ChartPath = path.u8string();

			std::string line;
			std::vector<std::string> Lines;

			while (std::getline(ifs,line)) {
				Lines.push_back(line);
			}

			size_t index = 0;
			uint64_t level = 0;
			uint64_t addscore = 0;
			std::vector<uint64_t> balloon;
			CourseType course = CourseType::Null;

			for (size_t i = 0; i < Lines.size(); i++) {
				Exsubstr(Lines[i], "TITLE:", [&](std::string data) {
					if (data.find("--") == 0) {
						data = data.replace(data.find("--"), 2, "");
						TitleDisplay = false;
					}
					Title = data;
					TitleStrlen.Generic = GetStrlen(data, Skin.Base->SongSelect.Font.Title.Handle);
					TitleStrlen.Playing = GetStrlen(data, Skin.Base->Playing.Font.Title.Handle);
					TitleStrlen.Result = GetStrlen(data, Skin.Base->Result.Font.Title.Handle);
					});
				Exsubstr(Lines[i], "SUBTITLE:", [&](std::string data) {
					if (data.find("--") == 0) {
						data = data.replace(data.find("--"), 2, "");
						SubtitleDisplay = false;
					}
					Subtitle = data;
					SubtitleStrlen.Generic = GetStrlen(data, Skin.Base->SongSelect.Font.Title.Handle);
					SubtitleStrlen.Playing = GetStrlen(data, Skin.Base->Playing.Font.SubTitle.Handle);
					SubtitleStrlen.Result = GetStrlen(data, Skin.Base->Result.Font.SubTitle.Handle);
					});
				Exsubstr(Lines[i], "BPM:", [&](const std::string& data) {
					if (!data.empty()) {
						BPM = std::stod(data);
					}
					});
				Exsubstr(Lines[i], "OFFSET:", [&](const std::string& data) {
					if (!data.empty()) {
						SongOffset = std::stod(data);
					}
					});
				Exsubstr(Lines[i], "MOVIEOFFSET:", [&](const std::string& data) {
					if (!data.empty()) {
						MovieOffset = stod(data);
					}
					});
				Exsubstr(Lines[i], "DEMOSTART:", [&](const std::string& data) {
					if (!data.empty()) {
						DemoStart = std::stod(data);
					}
					});
				Exsubstr(Lines[i], "SONGVOL:", [&](const std::string& data) {
					if (!data.empty()) {
						SongVolume = std::stof(data);
					}
					});
				Exsubstr(Lines[i], "SEVOL:", [&](const std::string& data) {
					if (!data.empty()) {
						SeVolume = std::stof(data);
					}
					});
				Exsubstr(Lines[i], "WAVE:", [&](const std::string& data) {
					if (!data.empty()) {
						SongPath = path.parent_path().u8string() + u8"\\" + std::u8string(data.begin(), data.end());
					}
					});
				Exsubstr(Lines[i], "BGMOVIE:", [&](const std::string& data) {
					if (!data.empty()) {
						MoviePath = path.parent_path().u8string() + u8"\\" + std::u8string(data.begin(), data.end());
					}
					});
				Exsubstr(Lines[i], "SONGLINK:", [&](const std::string& data) {
					if (!data.empty()) {
						SongLink = data;
					}
					});
				Exsubstr(Lines[i], "COURSE:", [&](const std::string& data) {
					if (!data.empty()) {
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
						if (course != CourseType::Null) {
							return;
						}
						course = (CourseType)std::stoi(str);
					}
					});
				Exsubstr(Lines[i], "LEVEL:", [&](const std::string& data) {
					if (!data.empty()) {
						level = std::stoull(data);
					}
					});
				Exsubstr(Lines[i], "SCOREINIT:", [&](const std::string& data) {
					if (!data.empty()) {
						addscore = stoull(data);
					}
					});
				Exsubstr(Lines[i], "BALLOON:", [&](const std::string& data) {
					if (!data.empty()) {
						auto datas = split(data, ',');
						for (const auto d : datas) {
							balloon.push_back(std::stoull(d));
						}
					}
					});
				Exsubstr(Lines[i], "#START", [&](const std::string& data) {
					if (course == CourseType::Null) {
						course = CourseType::Oni;
					}
					Courses[(size_t)course].Index = index;
					Courses[(size_t)course].Level = level;
					Courses[(size_t)course].AddScore = addscore;
					Courses[(size_t)course].Balloons = balloon;
					Courses[(size_t)course].IsPlayable = true;
					balloon = std::vector<uint64_t>();
					});
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
			std::ifstream ifs(path);

			if (!ifs.is_open()) {
				return;
			}

			std::string line;
			std::vector<std::string> Lines;

			while (std::getline(ifs, line)) {
				Lines.push_back(line);
			}

			auto ColorCodeParse = [&](std::string data) -> Color3<int> {
				data.replace(0, 1, "0x");
				int color = stoi(data, nullptr, 16);
				int r = 0, g = 0, b = 0;
				GetColor2(color, &r, &g, &b);
				return { (r - 128) * 2, (g - 128) * 2, (b - 128) * 2 };
				};

			for (size_t i = 0; i < Lines.size(); ++i) {
				Exsubstr(Lines[i], "GenreName=", [&](const std::string& data) {
					Name = data;
					});
				Exsubstr(Lines[i], "GenreCaption=", [&](const std::string& data) {
					Caption = data;
					});
				Exsubstr(Lines[i], "GenreColor=", [&](const std::string& data) {
					GenreColor = ColorCodeParse(data);
					});
				Exsubstr(Lines[i], "FontColor=", [&](const std::string& data) {
					FontColor = ColorCodeParse(data);
					});
				Exsubstr(Lines[i], "FontEdgeColor=", [&](const std::string& data) {
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
		return 2.0;
	}

	SoundData DemoSong = SoundData();

	size_t CourseIndex = 0;
	bool IsCourseSelect = false;

	void EnumChart(const std::vector<std::string>& dir) {
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
			recusiveproc(TempBoxDatas, dir[i], "", recusiveproc);
		}

		BoxDatasUpdate();
		BoxDataIndex = std::clamp<size_t>(BoxDataIndex, 0, BoxDatas.size() - 1);
	}
	void BoxDatasUpdate() {
		BoxDatas.clear();
		BoxDatas.reserve(TempBoxDatas.capacity());
		auto recusiveproc = [&](const std::vector<std::unique_ptr<BoxData>>& datas, auto f) -> void {
			for (size_t i = 0; i < datas.size(); ++i) {
				if (datas[i]->IsGenre()) {
					BoxDatas.push_back(datas[i].get());
					if (datas[i]->GetGenre()->IsOpen) {
						f(datas[i]->GetGenre()->Datas, f);
					}
				}
			}
			for (size_t i = 0; i < datas.size(); ++i) {
				if (datas[i]->IsGenre()) { continue; }
				BoxDatas.push_back(datas[i].get());
			}
			if (BoxDatas.empty()) {
				BoxDatasUpdate();
			}
			};
		recusiveproc(TempBoxDatas, recusiveproc);
	}

	void SongDownload(const std::string& link, const fs::path& path) {
		if (!link.empty() && !fs::exists(path)) {
			if (MessageBox(NULL, TEXT("音声ファイルがありません。ダウンロードしますか？"), "", MB_YESNO) == IDYES) {
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
		}
	}

	void SongSelectInit() {
		Skin.Base->SongSelect.SE.Don.SetVolume((Config.SEVolume));
		Skin.Base->SongSelect.SE.Ka.SetVolume((Config.SEVolume));
		if (BoxDatas.empty()) {
			MessageBox(NULL, TEXT("譜面がありません。"), TEXT("エラー"), MB_ICONERROR);
			NowScene = Scene::Title;
			return;
		}
	}
	void SongSelectEnd() {
		DemoSongPlayBlank.Reset();
		DemoSong.Delete();
		IsCourseSelect = false;
	}
	void SongSelectDraw() {

		Skin.Base->SongSelect.Image.BackGround.Draw({});

		if (!IsCourseSelect) {
			for (int i = (BoxDataIndex - 6); i < (BoxDataIndex + 6); ++i) {
				if (i < 0 || i >= BoxDatas.size()) {
					continue;
				}

				Pos2D<float> pos = {
					Skin.Base->SongSelect.Config.BoxDistance.X,
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
				Skin.Base->SongSelect.Image.Box.Draw(pos);
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

			for (size_t i = 0; i < (size_t)CourseType::Count; ++i) {
				
				unsigned int c = 100 * (CourseIndex == i);
				SetDrawAddColor(c, c, c);
				Skin.Base->SongSelect.Image.CourseBox.Draw({ 0, 80.0f * i });
				SetDrawAddColor(0, 0, 0);

				if (!BoxDatas[BoxDataIndex]->GetChart()->Courses[i].IsPlayable) {
					continue;
				}

				Skin.Base->SongSelect.Font.Course.Draw({ 
					Skin.Base->SongSelect.Config.CoursePos.X,
					Skin.Base->SongSelect.Config.CoursePos.Y + (80.0f * i)},
					GetColor(255, 255, 255),
					GetColor(0, 0, 0),
					magic_enum::enum_name((CourseType)i).data()
				);
				Skin.Base->SongSelect.Font.Level.Draw({
					Skin.Base->SongSelect.Config.LevelPos.X,
					Skin.Base->SongSelect.Config.LevelPos.Y + (80.0f * i) },
					GetColor(255, 255, 255),
					GetColor(0, 0, 0),
					u8StrToStr(u8"★×") + std::to_string(BoxDatas[BoxDataIndex]->GetChart()->Courses[i].Level)
					);
			}
		}
	}
	void SongSelectProc() {
		Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] {
			if (IsCourseSelect) {
				IsCourseSelect = false;
			}
			else {
				NowScene = Scene::ModeSelect;
			}
			});
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
					DemoSong.Delete();
					NowScene = Scene::Loading;
				}
			}
			};
		static auto KaInputProc = [&](bool direction) {
			Skin.Base->SongSelect.SE.Ka.Play();
			if (!direction) {
				if (!IsCourseSelect) {
					BoxDataIndex = BoxDataIndex == 0 ? BoxDatas.size() - 1 : BoxDataIndex - 1;
					DemoSongPlayBlank.Stop();
					DemoSong.Delete();
				}
				else {
					CourseIndex -= CourseIndex > 0;
				}
			}
			else {
				if (!IsCourseSelect) {
					BoxDataIndex = BoxDataIndex == BoxDatas.size() - 1 ? 0 : BoxDataIndex + 1;
					DemoSongPlayBlank.Stop();
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
				DemoSong.Load(u8StrToStr(BoxDatas[BoxDataIndex]->GetChart()->SongPath));
				SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMNOPRESS);
				DemoSong.SetCurrent(BoxDatas[BoxDataIndex]->GetChart()->DemoStart);
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

		Input.HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] { KaInputProc(false); });
		Input.HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] { KaInputProc(true); });
		Input.HitKeyesProcess({ VK_UP, VK_LEFT }, KeyState::Down, [&] { KaInputProc(false); });
		Input.HitKeyesProcess({ VK_DOWN, VK_RIGHT }, KeyState::Down, [&] { KaInputProc(true); });

		Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, DonInputProc);
		Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, DonInputProc);
		Input.HitKeyProcess(VK_RETURN, KeyState::Down, DonInputProc);

		Input.HitKeyProcess(VK_TAB, KeyState::Down, RandomInputProc);
		Input.HitKeyProcess(VK_F1, KeyState::Down, [&] {
			Skin.Base->SongSelect.SE.Don.Play();
			NowScene = Scene::ConfigMenu;
			PrevScene = Scene::ConfigMenu; });
		Input.HitKeyProcess(VK_F2, KeyState::Down, [&] { 
			Skin.Base->SongSelect.SE.Don.Play();
			EnumChart(Config.SongDirectories); });
		Input.HitKeyProcess(VK_F3, KeyState::Down, [&] { 
			Skin.Base->SongSelect.SE.Don.Play();
			Config.Load();
			});
		Input.HitKeyProcess(VK_F4, KeyState::Down, [&] { 
			Skin.Base->SongSelect.SE.Don.Play();
			Skin.Load(Config.SkinName);
			});
	}

	void LoadingDraw() {
		DrawFormatString(0, 8, GetColor(255, 255, 255), "譜面読み込み中…");
	}
	void LoadingProc() {

		ChartData LoadData = *BoxDatas[BoxDataIndex]->GetChart();
		Chart.Init();

		std::ifstream ifs(fs::path(LoadData.ChartPath));
		if (!ifs.is_open()) {

			NowScene = Scene::SongSelect;
			return;
		}

		std::string line;
		std::vector<std::string> Lines;

		while (std::getline(ifs, line)) {
			Lines.push_back(line);
		}

		NoteData MainData;
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
				for (size_t j = (size_t)LoadData.Courses[i].Index; j < Lines.size(); ++j) {
					if (Lines[j].find("#END") != std::string::npos) {
						Lines[j] = "";
						break;
					}
					Lines[j] = "";
				}
			}
		}

		size_t BarlineCounter[2]{};
		size_t BarlineNoteCount = 0;

		bool StartFlag = false;
		bool NextFlag = false;
		bool BarlineDisplay = true;
		bool BarlineLoading = false;
		bool AddBarline = false;
		bool NowRollFlag = false;
		size_t RollStartIndex = 0;
		char RollType = '\0';

		size_t BalloonIndex = 0;
		size_t NoteCount = 0;

		for (size_t i = 0; i < Lines.size(); ++i) {
			try {
				Exsubstr(Lines[i], "#START", [&](const std::string& data) {
					StartFlag = true;
					});
				Exsubstr(Lines[i], "#END", [&](const std::string& data) {
					StartFlag = false;
					});
				Exsubstr(Lines[i], "#GOGOSTART", [&](const std::string& data) {
					MainData.GoGoStart = true;
					});
				Exsubstr(Lines[i], "#GOGOEND", [&](const std::string& data) {
					MainData.GoGoEnd = true;
					});
				Exsubstr(Lines[i], "#BARLINEON", [&](const std::string& data) {
					BarlineDisplay = true;
					});
				Exsubstr(Lines[i], "#BARLINEOFF", [&](const std::string& data) {
					BarlineDisplay = false;
					});
				if (!StartFlag) {
					Exsubstr(Lines[i], "#BMSCROLL", [&](const std::string& data) {
						Chart.ScrollType = ScrollType::BMSCROLL;
						});
					Exsubstr(Lines[i], "#HBSCROLL", [&](const std::string& data) {
						Chart.ScrollType = ScrollType::HBSCROLL;
						});
				}
				Exsubstr(Lines[i], "#SCROLL", [&](const std::string& data) {
					if (data.find("i") != std::string::npos) {
						int Uindex = data.rfind("+") == std::string::npos ? 0 : data.rfind("+");
						int Dindex = data.rfind("-") == std::string::npos ? 0 : data.rfind("-");

						bool Flag = Uindex > Dindex;

						if (data.rfind("+") != std::string::npos && Flag) {
							std::string real = strtrim(data.substr(0, data.rfind("+")));
							std::string imag = strtrim(data.substr(data.rfind("+") + 1, data.rfind("i") - (data.rfind("+") + 1)));
							MainData.Scroll = real.empty() ? 0 : stod(real);
							MainData.Scrolli = imag.empty() ? -1 : stod(imag) * -1;
						}
						if (data.rfind("-") != std::string::npos && !Flag) {
							std::string real = strtrim(data.substr(0, data.rfind("-")));
							std::string imag = strtrim(data.substr(data.rfind("-") + 1, data.rfind("i") - (data.rfind("-") + 1)));
							MainData.Scroll = real == "" ? 0 : stod(real);
							MainData.Scrolli = imag == "" ? 1 : stod(imag);
						}
					}
					else {
						MainData.Scroll = stod(data);
						MainData.Scrolli = 0;
					}
					});
				Exsubstr(Lines[i], "#BPMCHANGE", [&](const std::string& data) {
					MainData.BPM = stod(data);
					MainData.BpmChangeFlag = true;
					});
				Exsubstr(Lines[i], "#MEASURE", [&](const std::string& data) {
					auto sp = split(data, '/');
					MainData.Measure = stod(sp[0]) / stod(sp[1]);
					});
				Exsubstr(Lines[i], "#DELAY", [&](const std::string& data) {
					Chart.RawNoteDatas.back().RelaTime += stod(data) * 1000;
					MainData.AbsTime += stod(data) * 1000;
					});

				if (Lines[i].find("#") != std::string::npos) { continue; }
				if (!StartFlag) { continue; }

				if (!BarlineLoading) {
					BarlineLoading = true;
					for (size_t j = i; j < Lines.size(); ++j) {
						if (Lines[j].find("#") != std::string::npos) { continue; }
						for (size_t k = 0, strsize = Lines[j].size(); k < strsize; ++k) {
							if (Lines[j][k] == ',') {
								goto BARLINEREADEND;
							}
							else if (Lines[j][k] >= '0' && Lines[j][k] <= '9') {
								++BarlineNoteCount;
							}
						}
					}
				BARLINEREADEND:;
				}
			}
			catch (const std::invalid_argument) {
				MessageBox(NULL, TEXT(std::string(std::to_string(i + 1) + "行目の記述が不正です。").c_str()), TEXT("エラー"), MB_ICONERROR);
				NowScene = Scene::SongSelect;
				return;
			}

			for (size_t j = 0, strsize = Lines[i].size(); j < strsize; ++j) {
				bool ChartFlag = (Lines[i][j] >= '0' && Lines[i][j] <= '9');
				bool EndFlag = Lines[i][j] == ',';
				bool EmptyFlag = BarlineNoteCount == 0;
				if (ChartFlag || EndFlag || EmptyFlag) {

					if (EndFlag && !EmptyFlag) {
						BarlineLoading = false;
						AddBarline = false;
						BarlineNoteCount = 0;
						break;
					}

					MainData.NoteType = Lines[i][j];

					double barlinetime = (240000 / MainData.BPM) * MainData.Measure;
					double divtime = barlinetime / (EmptyFlag ? 1 : BarlineNoteCount);

					MainData.RelaTime = divtime;

					if (!AddBarline) {
						AddBarline = true;
						if (BarlineDisplay) {
							MainData.BarlineDisplay = true;
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
						!MainData.BarlineDisplay;

					if (MainData.NoteType >= '1' && MainData.NoteType <= '4') {
						NoteCount++;
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
						if (BalloonIndex < balloon.size()) {
							ballooncount = balloon[BalloonIndex];
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

					MainData.AbsTime += divtime;
					MainData.PosTime += MainData.RelaTime * (std::signbit(MainData.BPM) || std::signbit(MainData.Measure) ? -1 : 1);

					if (EndFlag) {
						BarlineLoading = false;
						AddBarline = false;
						BarlineNoteCount = 0;
						break;
					}
				}
			}
		}

		Chart.AddScore = LoadData.Courses[CourseIndex].AddScore;
		if (Chart.AddScore == 0) {
			Chart.AddScore = 100'0000 / (double)NoteCount;
		}

		SetCreateSoundDataType(DX_SOUNDDATATYPE_FILE);
		Chart.SongData.Load(u8StrToStr(BoxDatas[BoxDataIndex]->GetChart()->SongPath));
		SetCreateSoundDataType(DX_SOUNDDATATYPE_MEMNOPRESS);

		if (!LoadData.MoviePath.empty()) {
			Chart.Movie.Load(u8StrToStr(LoadData.MoviePath), Config.SongSpeed, (LoadData.MovieOffset < 0 ? LoadData.MovieOffset * -1000 : Chart.SongBlankTime));
		}

		Chart.SongData.SetVolume(Chart.OriginalData.SongVolume * (Config.SongVolume / 100));
		int freq = Chart.SongData.Frequency * Config.SongSpeed;
		Chart.SongData.SetFrequency(freq);
		Chart.SongSpeed = (double)freq / Chart.SongData.Frequency;

		Skin.Base->Playing.SE.Don.SetVolume(Chart.OriginalData.SeVolume * (Config.SEVolume / 100));
		Skin.Base->Playing.SE.Ka.SetVolume(Chart.OriginalData.SeVolume * (Config.SEVolume / 100));
		Skin.Base->Playing.SE.Balloon.SetVolume(Chart.OriginalData.SeVolume * (Config.SEVolume / 100));

		HitNote = _HitNote();

		for (auto&& taiko : MiniTaikoFlash) {
			taiko.Reset();
		}

		WaitVSync(10);
		Chart.NowTime.Start();
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

	enum class JudgeType {
		None = -1,
		Good,
		Ok,
		Bad,
		Roll
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

	struct JudgeData {

		uint64_t Score = 0;
		uint64_t Good = 0;
		uint64_t Ok = 0;
		uint64_t Bad = 0;
		uint64_t Roll = 0;
		uint64_t Combo = 0;
		uint64_t MaxCombo = 0;
		uint64_t HitNote = 0;
		double ScoreRateGood = 0;
		double ScoreRateOk = 0;
		double Accuracy = 0;

		void Hit(JudgeType type, uint64_t addscore) {

			if (HitNote != 0) {
				Accuracy = ((Good / (double)HitNote) + ((Ok / (double)HitNote) * 0.5)) * 100;
			}

			switch (type) {
			case JudgeType::Good:
				++Good;
				++Combo;
				++HitNote;
				Score += addscore;
				break;
			case JudgeType::Ok:
				++Ok;
				++Combo;
				++HitNote;
				Score += addscore * 0.5;
				break;
			case JudgeType::Bad:
				++Bad;
				Combo = 0;
				++HitNote;
				break;
			case JudgeType::Roll:
				++Roll;
				Score += 100;
				break;
			}

			if (Combo > MaxCombo) { ++MaxCombo; }
		}
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

	struct PlayData {

		void Init() {
			RawNoteDatas.clear();
			SongData.Delete();
			NowTime.Reset();
			Movie.Init();
			ScrollType = ScrollType::Normal;
			OriginalData = ChartData();
			Judge = JudgeData();
			Roll = RollData();
			AutoPlayLR = false;
			NowGoGo = false;
			SongBlankTime = 0;
			SongSpeed = 1.0;
			AddScore = 0;
			NowBPM = 0;
		}

		std::vector<NoteData> RawNoteDatas = std::vector<NoteData>();

		ChartData OriginalData;

		JudgeData Judge;
		uint64_t AddScore = 0;

		SoundData SongData;
		double SongBlankTime = 0;
		double SongSpeed = 1.0;

		Timer NowTime;
		ScrollType ScrollType = ScrollType::Normal;
		double NowBPM = 0;
		bool NowGoGo = false;

		struct RollData{
			Timer ViewEndTimer;
			double ViewEndTime = 0.75;
			uint64_t NowCount = 0;
			bool IsEnd = false;
		} Roll;

		struct MovieData {
			void Init() {
				DeleteGraph(Handle);
				Size = { 0,0 };
				Handle = -1;
			}
			void Load(const std::string& path, double speed, double time) {
				Handle = LoadGraph(path.c_str());
				GetGraphSizeF(Handle, &Size.Width, &Size.Height);
				float ExtendRate = Size.Height / 720.0f;
				Size = { Size.Width / ExtendRate, Size.Height / ExtendRate };
				SetMovieVolumeToGraph(0, Handle);
				SetPlaySpeedRateMovieToGraph(Handle, speed);
				SeekMovieToGraph(Handle, time);
			}
			int Handle = -1;
			Size2D<float> Size;		
		} Movie;

		bool AutoPlayLR = false;
		Timer WaitRollTime;
	};

	PlayData Chart;

	Timer MiniTaikoFlash[4];
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
	} HitNote;

	void PlayingEnd() {
		Chart.SongData.Delete();
	}
	void PlayingDraw() {

		const double NowTime = ChartNowTime(1000);

		Skin.Base->Playing.Image.BackGround.Draw({});

		if (Chart.Movie.Handle != -1 && Config.BGBrightness > 0) {

			DrawExtendGraphF(
				Skin.Info.Resolution.X / 2 - Chart.Movie.Size.Width / 2,
				Skin.Info.Resolution.Y / 2 - Chart.Movie.Size.Height / 2,
				Skin.Info.Resolution.X / 2 + Chart.Movie.Size.Width / 2,
				Skin.Info.Resolution.Y / 2 + Chart.Movie.Size.Height / 2,
				Chart.Movie.Handle,
				FALSE);

			if ((NowTime + (Chart.OriginalData.MovieOffset * -1000)) > 128 && Chart.NowTime.IsRunning()) {
				PlayMovieToGraph(Chart.Movie.Handle);
			}

			SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255 * (1 - (Config.BGBrightness / 100)));
			DrawFillBox(0, 0, Skin.Info.Resolution.X, Skin.Info.Resolution.Y, GetColor(0, 0, 0));
			SetDrawBlendMode(0, 0);
		}

		if ((Chart.Movie.Handle != -1)) {
			SetDrawBlendMode(DX_BLENDMODE_ALPHA, 225);
		}
		Skin.Base->Playing.Image.LaneFrame.Draw({});
		Skin.Base->Playing.Image.Lane.Draw({});
		SetDrawBlendMode(0, 0);

		Skin.Base->Playing.Image.Note.Draw({});

		if (Chart.NowGoGo) {
			Skin.Base->Playing.Image.GoGoFire.Draw({}, (size_t)(NowTime / Skin.Base->Playing.Config.GoGoFireFrameTime) % Skin.Base->Playing.Image.GoGoFire.Div.X);
		}

		{

			size_t i = HitNote.Index;
			const double JudgeUnderExplosionTime = Skin.Base->Playing.Config.JudgeUpperExplosionFrameTime * Skin.Base->Playing.Image.JudgeUnderExplosion.Div.X;

			for (size_t c = 0; c < HitNote.Size(); ++c) {
				auto&& data = HitNote.Datas[i];
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
					Skin.Base->Playing.Image.JudgeUnderExplosion.Draw({}, drawindex);
				}
				else {
					data.JudgeUnderExplosion.IsActive = false;
				}

				++i;
				if (!(i < HitNote.Size())) {
					i = 0;
				}
			}
		}

		static auto NoteAlpha = [&](double _one, AlphaType Type) {
			_one = std::clamp(_one, 0.0, 1.0);
			int alpha = 255;
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
	Skin.Base->Playing.Image.Note.Pos.Y
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

		for (auto&& data : Chart.RawNoteDatas | std::ranges::views::reverse) {

			double NoteTheta = atan2(data.Scrolli, data.Scroll);

			NotePos = GetNotePos(data);

			if (data.BarlineDisplay) {
				SetDrawBlendMode(0, 0);
				if (InRange(NotePos.X, NotePos.Y)) {
					DrawLineAA(
						NotePos.X,
						NotePos.Y - 65,
						NotePos.X,
						NotePos.Y + 65,
						GetColor(255, 255, 255)
					);
				}
			}

			if (data.NoteType == '0') {
				continue;
			}

			if (data.NoteType >= '1' &&
				data.NoteType <= '4') {
				if (InRange(NotePos.X, NotePos.Y)) {
					int Alpha = 255;
					double hidden = Config.HiddenLevel;
					double sudden = Config.SuddenLevel;
					bool hiddenflag = hidden > 0;
					bool suddenflag = sudden > 0;
					if (hiddenflag || suddenflag) {
						double _abs = std::abs(std::complex<double>{ NotePos.X - NoteOrigin.X, NotePos.Y - NoteOrigin.Y })* (data.AbsTime < NowTime ? -1 : 1);
						double leveling = (Skin.Base->Playing.Image.Lane.Size.Width / DX_PI);
						double feedrange = (Skin.Base->Playing.Image.Lane.Size.Width / DX_TWO_PI);
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
							(float)(NotePos.Y - NoteOrigin.Y)
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
				const Pos2D<double>& dnote = GetNotePos(Chart.RawNoteDatas[data.RollEndIndex]);

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

		Skin.Base->Playing.Image.Base.Draw({});
		Skin.Base->Playing.Image.NamePlate.Draw({});
		Skin.Base->Playing.Image.MiniTaiko.Draw({});

		Skin.Base->Playing.Font.PlayerName.Draw(
			Skin.Base->Playing.Config.PlayerNamePos,
			GetColor(255, 255, 255),
			GetColor(0, 0, 0),
			Config.PlayerNameStrlen,
			Config.PlayerName
			);

		{

			size_t i = HitNote.Index;

			for (size_t c = 0; c < HitNote.Size(); ++c) {
				auto& data = HitNote.Datas[i];
				if (!data.MoveTimer.IsRunning()) {
					data.MoveTimer.Start();
				}

				if (data.FlyingNote.IsActive && Config.HitNoteDisp && data.MoveElapsedTime < data.FlyingNote.MoveTime()) {

					float _one = (data.MoveElapsedTime / data.FlyingNote.MoveTime());

					std::complex<float> _pos1 = { 840, -90 };
					std::complex<float> _pos2 = std::polar(280.0f, (DX_PI_F / 2) + std::arg(_pos1));
					std::complex<float> _r = { (_pos1.real() / 2) + _pos2.real(), (_pos1.imag() / 2) + _pos2.imag() };
					float mem0arg = std::arg(_r);
					_r += { -840, 90 };
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
					Skin.Base->Playing.Image.JudgeString.Draw({}, (size_t)data.JudgeString.Type);
					SetDrawBlendMode(0, 0);
				}
				else {
					data.JudgeString.IsActive = false;
				}

				if (!data.FlyingNote.IsActive && !data.JudgeUnderExplosion.IsActive && !data.JudgeString.IsActive) {
					data = HitNoteData();
				}

				++i;
				if (!(i < HitNote.Size())) {
					i = 0;
				}
			}
		}

		static auto TaikoAlpha = [&](size_t index) {
			double alpha = 255 * (1 - GetEasingRate(MiniTaikoFlash[index].GetElapsed().MilliSecond() / MiniTaikoFlashTime, ease::Base::In, ease::Line::Cubic));
			if (alpha < 0) { MiniTaikoFlash[index].Reset(); }
			SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
			};

		if (MiniTaikoFlash[0].IsRunning()) {
			TaikoAlpha(0);
			Skin.Base->Playing.Image.MiniTaiko_Don.Draw({ Skin.Base->Playing.Image.MiniTaiko_Don.Size.Width * -0.5f, 0 }, 0);
		}
		if (MiniTaikoFlash[1].IsRunning()) {
			TaikoAlpha(1);
			Skin.Base->Playing.Image.MiniTaiko_Ka.Draw({ Skin.Base->Playing.Image.MiniTaiko_Ka.Size.Width * -0.5f, 0 }, 0);
		}
		if (MiniTaikoFlash[2].IsRunning()) {
			TaikoAlpha(2);
			Skin.Base->Playing.Image.MiniTaiko_Don.Draw({ Skin.Base->Playing.Image.MiniTaiko_Don.Size.Width * 0.5f, 0 }, 1);
		}
		if (MiniTaikoFlash[3].IsRunning()) {
			TaikoAlpha(3);
			Skin.Base->Playing.Image.MiniTaiko_Ka.Draw({ Skin.Base->Playing.Image.MiniTaiko_Ka.Size.Width * 0.5f, 0 }, 1);
		}
		SetDrawBlendMode(0, 0);

		static auto ComboDraw = [&](uint64_t num) {
			int digit = std::digit(num);
			float offset = Skin.Base->Playing.Image.ComboNumber.Size.Width * (digit - 1) / 2;
			int i = 0;
			do {
				Skin.Base->Playing.Image.ComboNumber.Draw({ offset, 0 }, num % 10);
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
				Skin.Base->Playing.Image.ScoreNumber.Draw({ offset, 0 }, num % 10);
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
				Skin.Base->Playing.Image.RollNumber.Draw({ offset, 0 }, num % 10);
				num /= 10;
				++i;
				offset -= Skin.Base->Playing.Image.RollNumber.Size.Width;
			} while (i < digit);
			};

		if (Chart.Roll.NowCount > 0) {
			RollDraw(Chart.Roll.NowCount);
		}
		if (Chart.Judge.Combo >= 3) {
			ComboDraw(Chart.Judge.Combo);
		}
		ScoreDraw(Chart.Judge.Score);

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

		if (Config.ViewDebug) {
			DrawFormatString(0, 0, GetColor(255, 255, 255), "\n\n\nNowTime:%lf\nBPM:%lf\nChartPath:%s", ChartNowTime(1) / Chart.SongSpeed, Chart.NowBPM * Chart.SongSpeed, Chart.OriginalData.ChartPath.c_str());
		}
	}
	void PlayingProc() {

		const double NowTime = ChartNowTime(1000);

		if (Chart.SongBlankTime < NowTime && Chart.SongBlankTime + 128 > NowTime && !Chart.SongData.IsPlay()) {
			Chart.SongData.Play();
		}
		else if (Chart.SongBlankTime + 5000 < NowTime && !Chart.SongData.IsPlay()) {
			NowScene = Scene::Result;
			return;
		}

		static auto JudgeNote = [&](double nowtime, char type) {

			auto& Judge = Chart.Judge;
			size_t rollcount = 0;
			size_t ballooncount = 0;
			NoteData* balloondata = nullptr;

			bool NextImage = false;

			for (auto&& data : Chart.RawNoteDatas) {

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

				const double _HitError = data.AbsTime - nowtime;
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
					HitNote.Add(HitNoteData(data.NoteType, JudgeType::Good));
					Judge.Hit(JudgeType::Good, Chart.AddScore);
				}
				else if (OkHit) {
					HitNote.Add(HitNoteData(data.NoteType, JudgeType::Ok));
					Judge.Hit(JudgeType::Ok, Chart.AddScore);
				}
				else if (BadHit) {
					HitNote.Add(HitNoteData('\0', JudgeType::Bad));
					Judge.Hit(JudgeType::Bad, 0);
				}

				data.HitFlag = true;
				data.NoteType = '\0';

				return;
			}

			if (rollcount > 0) {
				HitNote.Add(HitNoteData(NextImage ? '6' : '5', JudgeType::Roll));
				Judge.Hit(JudgeType::Roll, 100);
				if (Chart.Roll.ViewEndTimer.IsRunning() && Chart.Roll.IsEnd) {
					Chart.Roll.IsEnd = false;
					Chart.Roll.NowCount = 0;
					Chart.Roll.ViewEndTimer.Reset();
				}
				Chart.Roll.NowCount++;
			}

			if (type == '1' && balloondata != nullptr) {
				--balloondata->BalloonCount;
				Chart.Roll.NowCount = balloondata->BalloonCount;
				Chart.Roll.ViewEndTimer.Reset();
				Judge.Hit(JudgeType::Roll, 100);
				if (balloondata->BalloonCount <= 0) {
					Skin.Base->Playing.SE.Balloon.Play();
					HitNote.Add(HitNoteData('3', JudgeType::Roll));
					balloondata->NoteType = '0';
					balloondata->HitFlag = true;
					balloondata->BalloonFlag = 2;
				}
			}
		};

		if (Chart.Roll.ViewEndTimer.GetElapsed().Second() > Chart.Roll.ViewEndTime) {
			Chart.Roll.IsEnd = false;
			Chart.Roll.NowCount = 0;
			Chart.Roll.ViewEndTimer.Reset();
		}

		for (auto&& data : Chart.RawNoteDatas) {

			bool HitFlag = data.AbsTime < NowTime;

			if (data.GoGoStart && HitFlag) {
				Chart.NowGoGo = true;
			}
			if (data.GoGoEnd && HitFlag) {
				Chart.NowGoGo = false;
			}

			if (data.AbsTime - Config.JudgeBad > NowTime) { continue; }
			if (data.HitFlag) { continue; }

			const double _HitError = data.AbsTime - NowTime;
			const bool BadHit = _HitError > -Config.JudgeBad && _HitError < Config.JudgeBad;

			if (data.BigNoteTime != 0 && Config.JudgeGood < NowTime - data.BigNoteTime) {
				JudgeNote(NowTime, data.NoteType - 2);
			}

			if (!data.HitFlag &&
				(data.NoteType >= '1' && data.NoteType <= '4') &&
				data.BigNoteTime == 0 &&
				_HitError < -Config.JudgeBad) {
				Chart.Judge.Hit(JudgeType::Bad, 0);
				data.HitFlag = true;
			}

			if ((data.NoteType >= '5' && data.NoteType <= '6') &&
				HitFlag) {
				data.RollFlag = 1;
				if (data.RollEndTime < NowTime) {
					data.RollFlag = 2;
					data.HitFlag = true;
					Chart.Roll.IsEnd = true;
					Chart.Roll.ViewEndTimer.Start();
				}
			}

			if ((data.NoteType == '7' || data.NoteType == '9') &&
				HitFlag) {
				data.BalloonFlag = 1;
				if (data.RollEndTime < NowTime) {
					data.BalloonFlag = 2;
					data.HitFlag = true;
				}
			}
		}

		if (Config.AutoPlay) {
			size_t RollCount = 0;
			NoteData* BalloonData = nullptr;
			bool NextImage = false;
			for (auto&& data : Chart.RawNoteDatas) {

				bool HitFlag = data.AbsTime < NowTime;
				bool IsHitNote = (data.NoteType >= '1' && data.NoteType <= '4');

				if (data.RollFlag == 1) {
					++RollCount;
					NextImage = data.NoteType == '6';
				}

				if (data.BalloonFlag == 1) {
					BalloonData = &data;
				}

				if (HitFlag && !data.HitFlag && IsHitNote) {
					HitNote.Add(HitNoteData(data.NoteType, JudgeType::Good));
					Chart.Judge.Hit(JudgeType::Good, 0);
					switch (data.NoteType) {
					case '1':
						Skin.Base->Playing.SE.Don.Play();
						MiniTaikoFlash[0 + Chart.AutoPlayLR * 2].Start();
						Chart.AutoPlayLR = !Chart.AutoPlayLR;
						break;
					case '2':
						Skin.Base->Playing.SE.Ka.Play();
						MiniTaikoFlash[1 + Chart.AutoPlayLR * 2].Start();
						Chart.AutoPlayLR = !Chart.AutoPlayLR;
						break;
					case '3':
						Skin.Base->Playing.SE.Don.Play();
						Skin.Base->Playing.SE.Don.Play();
						MiniTaikoFlash[0].Start();
						MiniTaikoFlash[2].Start();
						break;
					case '4':
						Skin.Base->Playing.SE.Ka.Play();
						Skin.Base->Playing.SE.Ka.Play();
						MiniTaikoFlash[1].Start();
						MiniTaikoFlash[3].Start();
						break;
					}
					data.NoteType = '\0';
					data.HitFlag = true;
				}
			}

			if (RollCount > 0 && !Chart.WaitRollTime.IsRunning()) {
				Skin.Base->Playing.SE.Don.Play();
				if (Chart.Roll.ViewEndTimer.IsRunning() && Chart.Roll.IsEnd) {
					Chart.Roll.IsEnd = false;
					Chart.Roll.NowCount = 0;
					Chart.Roll.ViewEndTimer.Reset();
				}
				Chart.AutoPlayLR = !Chart.AutoPlayLR;
				Chart.Judge.Roll++;
				Chart.Roll.NowCount++;
				HitNote.Add(HitNoteData(NextImage ? '6' : '5', JudgeType::Roll));
				Chart.WaitRollTime.Start();
			}
			if (BalloonData != nullptr && !Chart.WaitRollTime.IsRunning()) {
				Skin.Base->Playing.SE.Don.Play();
				Chart.AutoPlayLR = !Chart.AutoPlayLR;
				Chart.Judge.Roll++;
				--BalloonData->BalloonCount;
				Chart.Roll.NowCount = BalloonData->BalloonCount;
				Chart.Roll.ViewEndTimer.Reset();
				Chart.WaitRollTime.Start();
				if (BalloonData->BalloonCount <= 0) {
					Skin.Base->Playing.SE.Balloon.Play();
					HitNote.Add(HitNoteData('3', JudgeType::Roll));
					BalloonData->NoteType = '0';
					BalloonData->HitFlag = true;
					BalloonData->BalloonFlag = 2;
				}
			}
			if (Chart.WaitRollTime.GetElapsed().Second() > 1.0 / Config.RollSpeed) {
				Chart.WaitRollTime.Reset();
			}
		}
		else {
			Input.HitKeyesProcess(Config.DonInputLeft, KeyState::Down, [&] {
				Skin.Base->Playing.SE.Don.Play();
				MiniTaikoFlash[0].Start();
				JudgeNote(NowTime, '1');
				});
			Input.HitKeyesProcess(Config.KaInputLeft, KeyState::Down, [&] {
				Skin.Base->Playing.SE.Ka.Play();
				MiniTaikoFlash[1].Start();
				JudgeNote(NowTime, '2');
				});
			Input.HitKeyesProcess(Config.DonInputRight, KeyState::Down, [&] {
				Skin.Base->Playing.SE.Don.Play();
				MiniTaikoFlash[2].Start();
				JudgeNote(NowTime, '1');
				});
			Input.HitKeyesProcess(Config.KaInputRight, KeyState::Down, [&] {
				Skin.Base->Playing.SE.Ka.Play();
				MiniTaikoFlash[3].Start();
				JudgeNote(NowTime, '2');
				});
		}

		Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] {
			NowScene = Scene::SongSelect;
			});
		Input.HitKeyProcess(VK_TAB, KeyState::Down, [&] {
			NowScene = Scene::Loading;
			});
	}

	void ResultDraw() {

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

			ScoreDraw(Chart.Judge.Score);
			AccuracyDraw(Chart.Judge.Accuracy);
			JudgeDraw(Skin.Base->Result.Config.GoodPos, Chart.Judge.Good);
			JudgeDraw(Skin.Base->Result.Config.OkPos, Chart.Judge.Ok);
			JudgeDraw(Skin.Base->Result.Config.BadPos, Chart.Judge.Bad);
			JudgeDraw(Skin.Base->Result.Config.RollPos, Chart.Judge.Roll);
			JudgeDraw(Skin.Base->Result.Config.MaxComboPos, Chart.Judge.MaxCombo);

			int crownindex = 0;
			if (Chart.Judge.Accuracy >= 75) {
				crownindex = 1;
			}
			if (Chart.Judge.Accuracy >= 90) {
				crownindex = 2;
			}
			if (Chart.Judge.Accuracy >= 90 && Chart.Judge.Bad == 0) {
				crownindex = 3;
			}
			if (Chart.Judge.Accuracy >= 90 && Chart.Judge.Bad == 0 && Chart.Judge.Ok == 0) {
				crownindex = 4;
			}

			Skin.Base->Result.Image.Crown.Draw({}, crownindex);
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
	}
	void ResultProc() {
		Input.HitKeyProcess(VK_RETURN, KeyState::Down, [&] {
			NowScene = Scene::SongSelect;
			return;
			});
	}

	enum class ConfigGenreData {
		Genre = 0,
		Game,
		Key,
	} ConfigGenre = ConfigGenreData::Genre;

	std::vector<std::vector<std::string>> ConfigMenuString{
		{ 
			"GameConfig",
			"KeyConfig"
		},
		{
			"PlayerName",
			"AutoPlay",
			"HiddenLevel",
			"SuddenLevel",
			"RandomRate",
			"JudgeGood",
			"JudgeOk",
			"JudgeBad",
			"SongOffset",
			"ChartSpeed",
			"SongSpeed",
			"BGBrightness",
			"SkinName",
			"SongDirectories",
			"SongVolume",
			"SEVolume",
			"HitNoteDisp",
			"RollSpeed",
			"WaitVSync",
			"FastInput",
			"FastDrawRate",
			"SoundDeviceType",
			"Exclusive",
			"SampleRate",
			"BufferSize",
			"FullScreen",
			"ViewDebug",
		},
        {
			"KaInputLeft",
			"DonInputLeft",
			"DonInputRight",
			"KaInputRight",
		}
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

		void Load() {
			GetKeyInputString(Buffer, Handle);
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
					Bool = str == "true" ? true : str == "false" ? false : Bool;
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
		return { 0, ConfigGenre == ConfigGenreData::Game ? (i - ConfigSelector) * Skin.Base->ConfigMenu.Config.BoxDistance : i * Skin.Base->ConfigMenu.Config.BoxDistance };
	};

	void ConfigDataDraw(int i,int& j, std::string data) {
		if (i == j) {
			Skin.Base->ConfigMenu.Font.String.Draw(
				{ Skin.Base->ConfigMenu.Config.ValPos.X,
				Skin.Base->ConfigMenu.Config.ValPos.Y + GetConfigPos(i).Y },
				GetColor(255,255,255),
				GetColor(0,0,0),
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
				if (data.size() > 1) {
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
					Skin.Base->ConfigMenu.Config.ValPos.Y + GetConfigPos(i).Y },
					(i == ConfigSelector) && (c == ConfigKeySelector) ? GetColor(255, 255, 0) : GetColor(255, 255, 255),
					GetColor(0, 0, 0),
					data[c]
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
	template<typename T, typename I>
	void ConfigVectorInput(int& i, T& data, I& input) {
		if (ConfigSelector == i) {
			if (ConfigInputFlag == 1) {
				input = data;
			}
			if (ConfigInputFlag == 2) {
				data.clear();
				for (auto&& elem : InputData.Vector) {
					data.push_back(elem);
				}
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
			Skin.Base->ConfigMenu.Font.String.Draw({
				Skin.Base->ConfigMenu.Config.StringPos.X,
				Skin.Base->ConfigMenu.Config.StringPos.Y + GetConfigPos(i).Y },
				GetColor(255, 255, 255),
				GetColor(0, 0, 0),
				GetStrlen(ConfigMenuString[(int)ConfigGenre][i], Skin.Base->ConfigMenu.Font.String.Handle),
				ConfigMenuString[(int)ConfigGenre][i]
				);

			SetDrawAddColor(0, 0, 0);
			if (ConfigInputFlag == 0) {
				SetDrawBlendMode(0, 0);
			}

			if (CheckKeyInput(InputData.Handle) == 0) {
				SetKeyInputStringFont(Skin.Base->ConfigMenu.Font.String.Handle);
				DrawKeyInputString(640, 340, InputData.Handle);
			}

			if (int j = 0; ConfigGenre == ConfigGenreData::Game) {
				ConfigDataDraw(i, j, Config.PlayerName);
				ConfigDataDraw(i, j, Config.AutoPlay ? "true" : "false");
				ConfigDataDraw(i, j, std::to_string(Config.HiddenLevel));
				ConfigDataDraw(i, j, std::to_string(Config.SuddenLevel));
				ConfigDataDraw(i, j, std::to_string(Config.RandomRate));
				ConfigDataDraw(i, j, std::to_string(Config.JudgeGood));
				ConfigDataDraw(i, j, std::to_string(Config.JudgeOk));
				ConfigDataDraw(i, j, std::to_string(Config.JudgeBad));
				ConfigDataDraw(i, j, std::to_string(Config.SongOffset));
				ConfigDataDraw(i, j, std::to_string(Config.ChartSpeed));
				ConfigDataDraw(i, j, std::to_string(Config.SongSpeed));
				ConfigDataDraw(i, j, std::to_string(Config.BGBrightness));
				ConfigDataDraw(i, j, Config.SkinName);
				ConfigVectorDraw(i, j, Config.SongDirectories);
				ConfigDataDraw(i, j, std::to_string(Config.SongVolume));
				ConfigDataDraw(i, j, std::to_string(Config.SEVolume));
				ConfigDataDraw(i, j, Config.HitNoteDisp ? "true" : "false");
				ConfigDataDraw(i, j, std::to_string(Config.RollSpeed));
				ConfigDataDraw(i, j, Config.WaitVSync ? "true" : "false");
				ConfigDataDraw(i, j, Config.FastInput ? "true" : "false");
				ConfigDataDraw(i, j, std::to_string(Config.FastDrawRate));
				ConfigDataDraw(i, j, std::to_string(Config.SoundDeviceType));
				ConfigDataDraw(i, j, Config.Exclusive ? "true" : "false");
				ConfigDataDraw(i, j, std::to_string(Config.SampleRate));
				ConfigDataDraw(i, j, std::to_string(Config.BufferSize));
				ConfigDataDraw(i, j, Config.FullScreen ? "true" : "false");
				ConfigDataDraw(i, j, Config.ViewDebug ? "true" : "false");
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
		}
		SetDrawAddColor(0, 0, 0);
		SetDrawBlendMode(0, 0);
	}
	void ConfigMenuProc() {

		if (ConfigInputFlag == 0) {

			Input.HitKeyProcess(VK_ESCAPE, KeyState::Down, [&] {
				if (ConfigGenre == ConfigGenreData::Genre) {
					NowScene = PrevScene;
				}
				else {
					ConfigGenre = ConfigGenreData::Genre;
					ConfigSelector = 0;
				}
			});

			Input.HitKeyProcess(VK_UP, KeyState::Down, [&] {
				Skin.Base->Title.SE.Ka.Play();
				ConfigSelector <= 0 ? 0 : ConfigSelector--;
				});
			Input.HitKeyProcess(VK_DOWN, KeyState::Down, [&] {
				Skin.Base->Title.SE.Ka.Play();
				ConfigSelector >= ConfigMenuString[(int)ConfigGenre].size() - 1 ? ConfigSelector = ConfigMenuString[(int)ConfigGenre].size() - 1 : ConfigSelector++;
				});

			if (ConfigGenre == ConfigGenreData::Key) {
				Input.HitKeyProcess(VK_LEFT, KeyState::Down, [&] {
					Skin.Base->Title.SE.Ka.Play();
					ConfigKeySelector <= 0 ? 0 : ConfigKeySelector--;
					});
				Input.HitKeyProcess(VK_RIGHT, KeyState::Down, [&] {
					Skin.Base->Title.SE.Ka.Play();
					ConfigKeySelector >= 4 - 1 ? 4 : ConfigKeySelector++;
					});
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
					ConfigDataInput(i, Config.HiddenLevel, InputData.Double);
					ConfigDataInput(i, Config.SuddenLevel, InputData.Double);
					ConfigDataInput(i, Config.RandomRate, InputData.Int);
					ConfigDataInput(i, Config.JudgeGood, InputData.Double);
					ConfigDataInput(i, Config.JudgeOk, InputData.Double);
					ConfigDataInput(i, Config.JudgeBad, InputData.Double);
					ConfigDataInput(i, Config.SongOffset, InputData.Double);
					ConfigDataInput(i, Config.ChartSpeed, InputData.Double);
					ConfigDataInput(i, Config.SongSpeed, InputData.Double);
					ConfigDataInput(i, Config.BGBrightness, InputData.Double);
					ConfigDataInput(i, Config.SkinName, InputData.String);
					ConfigVectorInput(i, Config.SongDirectories, InputData.Vector);
					ConfigDataInput(i, Config.SongVolume, InputData.Float);
					ConfigDataInput(i, Config.SEVolume, InputData.Float);
					ConfigDataInput(i, Config.HitNoteDisp, InputData.Bool);
					ConfigDataInput(i, Config.RollSpeed, InputData.Double);
					ConfigDataInput(i, Config.WaitVSync, InputData.Bool);
					ConfigDataInput(i, Config.FastInput, InputData.Bool);
					ConfigDataInput(i, Config.FastDrawRate, InputData.Double);
					ConfigDataInput(i, Config.SoundDeviceType, InputData.Int);
					ConfigDataInput(i, Config.Exclusive, InputData.Bool);
					ConfigDataInput(i, Config.SampleRate, InputData.Int);
					ConfigDataInput(i, Config.BufferSize, InputData.Int);
					ConfigDataInput(i, Config.FullScreen, InputData.Bool);
					ConfigDataInput(i, Config.ViewDebug, InputData.Bool);
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
			}
		}
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
		double Time = 0.35;
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

		SetOutApplicationLogValidFlag(FALSE);
		SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8);

		ChangeWindowMode(!Config.FullScreen);
		SetMainWindowText("FreedomApopicStyle");
		SetWaitVSyncFlag(Config.WaitVSync);
		SetAlwaysRunFlag(TRUE);
		SetUseDivGraphFlag(FALSE);
		SetWindowVisibleFlag(FALSE);

		SoundDevice.SetDevice(
			Config.SoundDeviceType,
			Config.Exclusive,
			Config.BufferSize,
			Config.SampleRate
		);

		if (DxLib_Init() == -1) {
			MessageBox(NULL, TEXT("DxLibの初期化に失敗しました"), TEXT("FreedomApopicStyle"), MB_OK | MB_ICONERROR);
			return false;
		}

		Skin.Load(Config.SkinName);
		EnumChart(Config.SongDirectories);

		SetWindowVisibleFlag(TRUE);
		SetWindowZOrder(DX_WIN_ZTYPE_TOP, false);

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
			case Scene::ConfigMenu:
				ConfigMenuEnd();
				break;
			case Scene::Playing:
				PlayingEnd();
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
			case Scene::Playing:
				break;
			case Scene::Result:
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
		WaitVSync(1);
		uint64_t mem = 0, now = 0;
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

	int Main() {

		if (!Init()) {
			return -1;
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
		}

		thd.join();
		DxLib_End();
		return 0;
	}
};