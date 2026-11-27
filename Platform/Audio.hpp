#pragma once
// 音声 (自前のミキサー。Windows は WASAPI、Android は SDL3 の AudioStream に流す)。音は番号 (ハンドル) で扱う。
// 音源・効果音は読み込んだ時に float の波形へ展開し、再生中は確保をしない (コールバックの中は足し算と補間だけ)。
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace snd {
    // 展開した波形 (interleaved float、-1～1)
    struct Pcm {
        std::vector<float> Samples;
        int Rate = 44100;
        int Channels = 2;
        int64_t Frames() const { return Channels > 0 ? (int64_t)Samples.size() / Channels : 0; }
    };

    struct DeviceOptions {
        int Rate = 48000;
        int BufferFrames = 0;       // 0: 機器に任せる。小さいほど遅延が小さい
        bool Exclusive = false;     // Windows: WASAPI の排他モード (使えなければ共有モード)
    };
    bool Init(const DeviceOptions& options);
    void Shutdown();
    bool IsInit();
    int DeviceRate();
    const char* DeviceDescription();    // 開いた出力の説明 ("WASAPI 排他 48000Hz 3.0ms" など)

    // ---- 波形 (読み込み・作成) ----
    std::shared_ptr<Pcm> Decode(const std::string& path);                 // WAV / OGG / MP3 / FLAC。失敗すると nullptr
    std::shared_ptr<Pcm> DecodeMemory(const void* data, size_t size);

    // ---- 音 (ハンドル。失敗すると -1) ----
    // voices: 同時に重ねて鳴らせる数 (効果音の連打用)
    int Load(const std::string& path, int voices = 1);
    int LoadMemory(const void* data, size_t size, int voices = 1);
    int LoadAsync(const std::string& path, int voices = 1);     // 別スレッドで展開する (IsReady / Wait で完了を確かめる)
    int Create(std::shared_ptr<const Pcm> pcm, int voices = 1);
    int Duplicate(int handle);
    void Delete(int handle);
    void DeleteAll();
    bool IsValid(int handle);
    bool IsReady(int handle);
    void Wait(int handle);
    std::shared_ptr<const Pcm> Data(int handle);

    // fromTop: 先頭から (false なら止めた位置・SetPositionMs の位置から)
    void Play(int handle, bool fromTop = true, bool loop = false);
    void Stop(int handle);
    bool IsPlaying(int handle);
    void SetDeleteOnFinish(int handle, bool on);    // 鳴り終わったら消す

    void SetVolume(int handle, int volume);         // 0～255
    int Volume(int handle);
    void SetFrequency(int handle, int hz);          // 再生の周波数 (元の周波数より大きいと速く・高く)。0 で元に戻す
    int Frequency(int handle);
    int OriginalFrequency(int handle);
    void SetPositionMs(int handle, double ms);      // 次に再生する位置 (再生中ならすぐ移る)
    double PositionMs(int handle);
    int64_t TotalFrames(int handle);
    double TotalMs(int handle);
}
