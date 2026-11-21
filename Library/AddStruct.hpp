#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>
#include "DxLib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

template<class T>
struct Pos2D {
    T X = { 0 };
    T Y = { 0 };
};
template<class T>
void from_json(const json& data, Pos2D<T>& v);

template<class T>
struct Size2D {
    T Width = 0;
    T Height = 0;
};
template<class T>
void from_json(const json& data, Size2D<T>& v);

template<class T>
struct Rect2D {
    T Top = 0;
    T Bottom = 0;
    T Left = 0;
    T Right = 0;
};
template<class T>
void from_json(const json& data, Rect2D<T>& v);

template<class T>
struct Color3 {
    T R = 0;
    T G = 0;
    T B = 0;
};
template<class T>
void from_json(const json& data, Color3<T>& v);

template<class T>
struct Color4 {
    T R = 0;
    T G = 0;
    T B = 0;
    T A = 0;
};
template<class T>
void from_json(const json& data, Color4<T>& v);


#define JSONPARSE(name) if (data.find(#name) != data.end()) name = data[#name]

// 非同期読み込み中でも、直後にハンドルを使う処理 (フォント設定・周波数取得など) は同期で読み込む
struct ScopedSyncLoad {
    int Prev = GetUseASyncLoadFlag();
    ScopedSyncLoad() { SetUseASyncLoadFlag(FALSE); }
    ~ScopedSyncLoad() { SetUseASyncLoadFlag(Prev); }
    ScopedSyncLoad(const ScopedSyncLoad&) = delete;
    ScopedSyncLoad& operator=(const ScopedSyncLoad&) = delete;
};

struct GraphData {

    Pos2D<float> Pos;
    Size2D<float> Size;
    Pos2D<int> Div{ 1,1 };

    void Load(const std::string& filepath, Pos2D<int> div = { 1,1 }) {

        int _w = 0, _h = 0;
        GetImageSize_File(filepath.c_str(), &_w, &_h);
        Size = { (float)_w, (float)_h };
        Div = div;

        LoadDiv(filepath, _w, _h);
    }
    void Load(const std::string& dirpath, const json& data) {

        std::string FilePath = "";

        JSONPARSE(FilePath);

        FilePath = (dirpath + FilePath);

        int _w = 0, _h = 0;
        GetImageSize_File(FilePath.c_str(), &_w, &_h);
        Size = { (float)_w, (float)_h };

        JSONPARSE(Size);
        JSONPARSE(Pos);
        JSONPARSE(Div);

        LoadDiv(FilePath, _w, _h);
    }
    void Delete() {
        for (int handle : Handles) {
            DeleteGraph(handle);
        }
        Handles.clear();
    }
    void Draw(Pos2D<float> pos, int index = 0) {
        if (index < 0 || Handles.size() <= static_cast<size_t>(index)) {
            return;
        }
        DrawExtendGraphF(
            Pos.X + pos.X - (Size.Width * 0.5f),
            Pos.Y + pos.Y - (Size.Height * 0.5f),
            Pos.X + pos.X + (Size.Width * 0.5f),
            Pos.Y + pos.Y + (Size.Height * 0.5f),
            Handles[index],
            TRUE
        );
    }
    void RectDraw(Pos2D<float> pos, Pos2D<float> originpos, Size2D<float> size, int index = 0) {
        if (index < 0 || Handles.size() <= static_cast<size_t>(index)) {
            return;
        }
        DrawRectGraphF(
            Pos.X + pos.X - (Size.Width * 0.5f),
            Pos.Y + pos.Y - (Size.Height * 0.5f),
            originpos.X,
            originpos.Y,
            size.Width,
            size.Height,
            Handles[index],
            TRUE
        );
    }

    std::vector<int> Handles;

private:

    void LoadDiv(const std::string& filepath, int w, int h) {

        this->Delete();

        if (Div.X <= 0 || Div.Y <= 0) {
            Div = { 1,1 };
        }

        const int num = Div.X * Div.Y;
        std::vector<int> handles(num, -1);
        if (LoadDivGraphF(filepath.c_str(), num, Div.X, Div.Y, (float)w / Div.X, (float)h / Div.Y, handles.data()) == -1) {
            return;
        }
        Handles = std::move(handles);
    }
};

struct FontData {

    float Size = 0;
    float Thick = 0;
    float Edge = 0;
    int Space = 0;
    enum Position : int {
        Left,
        Center,
        Right
    } Position = Left;

    void Load(const std::string& filepath, const std::string& fontname, float size, float thick, float edge, int space) {

        ScopedSyncLoad sync;
        this->Delete();

#ifndef __ANDROID__
        MemHandle = AddFontFile(filepath.c_str());
#endif

        Size = size;
        Thick = thick * 0.5f;
        Edge = edge;
        Space = space;

        Handle = CreateFontToHandle(fontname.c_str(), Size, Thick, DX_FONTTYPE_ANTIALIASING_EDGE_16X16, DX_CHARSET_UTF8, Edge);
        SetFontSpaceToHandle(Space, Handle);
    }
    void Load(const std::string& dirpath, const json& data) {

        std::string FontName = "";
        std::string FilePath = "";

        ScopedSyncLoad sync;
        this->Delete();

        JSONPARSE(FilePath);

        FilePath = (dirpath + FilePath);

#ifndef __ANDROID__
        MemHandle = AddFontFile(FilePath.c_str());
#endif

        JSONPARSE(Size);
        JSONPARSE(Thick);
        Thick *= 0.5f;
        JSONPARSE(Edge);
        if (data.find("Position") != data.end()) ((int&)Position) = data["Position"].get<int>();
        JSONPARSE(Space);

        JSONPARSE(FontName);

        Handle = CreateFontToHandle(FontName.c_str(), Size, Thick, DX_FONTTYPE_ANTIALIASING_EDGE_16X16, DX_CHARSET_UTF8, Edge);
        SetFontSpaceToHandle(Space, Handle);
    }
    void Delete() {
        if (Handle != -1) {
            DeleteFontToHandle(Handle);
        }
#ifndef __ANDROID__
        if (MemHandle != nullptr) {
            RemoveFontFile(MemHandle);
        }
#endif
        Handle = -1;
        MemHandle = nullptr;
    }
    void Draw(Pos2D<float> pos, unsigned int color, unsigned int edgecolor, const std::string& str) {
        if (Handle == -1) { return; }
        if (str.empty()) { return; }
        float offset = 0;
        switch (Position)
        {
        case Left:
            break;
        case Center:
            offset = GetDrawStringWidthToHandle(str.c_str(), -1, Handle) * 0.5f;
            break;
        case Right:
            offset = GetDrawStringWidthToHandle(str.c_str(), -1, Handle);
            break;
        }
        DrawStringFToHandle(
            pos.X - offset,
            pos.Y - Size * 0.5f,
            str.c_str(),
            color,
            Handle,
            edgecolor
        );
    }
    void Draw(Pos2D<float> pos, unsigned int color, unsigned int edgecolor, unsigned int strlen, const std::string& str) {
        if (Handle == -1) { return; }
        if (str.empty()) { return; }
        float offset = 0;
        switch (Position)
        {
        case Left:
            break;
        case Center:
            offset = strlen * 0.5f;
            break;
        case Right:
            offset = strlen;
            break;
        }
        DrawStringFToHandle(
            pos.X - offset,
            pos.Y - Size * 0.5f,
            str.c_str(),
            color,
            Handle,
            edgecolor
        );
    }

    int Handle = -1;
    void* MemHandle = nullptr;
};

struct SoundData {

    static constexpr int DefaultSEBufferNum = 4;

    int BufferNum = 0;
    int Frequency = 0;
    int Volume = 0;
    std::string FilePath;       // 読み込んだファイル (録画の音声を譜面から作る時に使う)

    void Load(const std::string& filepath, int buffernum = 1) {
        ScopedSyncLoad sync;
        this->Delete();
        BufferNum = buffernum;
        FilePath = filepath;
        Handle = LoadSoundMem(filepath.c_str(), BufferNum);

        Frequency = GetFrequencySoundMem(Handle);
        Volume = GetVolumeSoundMem(Handle);
    }
    void Load(const std::string& dirpath, const json& data) {

        std::string FilePath = "";

        // 効果音は連打で重ねて鳴らすため、指定が無ければ複数バッファを確保する (1 だと鳴り直しで前の音が途切れる)
        BufferNum = DefaultSEBufferNum;
        JSONPARSE(FilePath);
        JSONPARSE(BufferNum);
        if (BufferNum < 1) { BufferNum = 1; }

        FilePath = (dirpath + FilePath);

        ScopedSyncLoad sync;
        this->Delete();
        this->FilePath = FilePath;
        Handle = LoadSoundMem(FilePath.c_str(), BufferNum);

        Frequency = GetFrequencySoundMem(Handle);
        Volume = GetVolumeSoundMem(Handle);
    }
    void Load(const void* filedata, size_t filesize, int buffernum = 1) {
        ScopedSyncLoad sync;
        this->Delete();
        BufferNum = buffernum;
        Handle = LoadSoundMemByMemImage(filedata, filesize, BufferNum);

        Frequency = GetFrequencySoundMem(Handle);
        Volume = GetVolumeSoundMem(Handle);
    }
    void Delete() {
        if (Handle != -1) {
            DeleteSoundMem(Handle);
        }
        Handle = -1;
    }
    void Play(bool toppositionflag = true) const {
        PlaySoundMem(Handle, DX_PLAYTYPE_BACK, toppositionflag);
    }
    void Stop() const {
        StopSoundMem(Handle);
    }
    // 非同期で読み込みを開始する (演奏を止めずに段位の次の曲を先読みするため)
    void LoadAsync(const std::string& filepath, int buffernum = 1) {
        this->Delete();
        BufferNum = buffernum;
        const int prev = GetUseASyncLoadFlag();
        SetUseASyncLoadFlag(TRUE);
        Handle = LoadSoundMem(filepath.c_str(), BufferNum);
        SetUseASyncLoadFlag(prev);
        Frequency = 0;
        Volume = 0;
    }
    // 非同期読み込みの完了を待ち、周波数などを取得する
    void FinishAsync() {
        if (Handle == -1) {
            return;
        }
        WaitHandleASyncLoad(Handle);
        Frequency = GetFrequencySoundMem(Handle);
        Volume = GetVolumeSoundMem(Handle);
    }
    bool IsPlay() const {
        return CheckSoundMem(Handle) == TRUE;
    }
    void SetFrequency(int frequency) const {
        SetFrequencySoundMem(frequency, Handle);
    }
    void SetVolume(float volume) const {
        ChangeVolumeSoundMem(static_cast<int>((volume / 100.0) * 255.0), Handle);
    }
    void SetCurrent(double time) const {
        SetSoundCurrentTime(time, Handle);
    }

    int Handle = -1;
};

// 背景動画。譜面の時刻に合わせて再生位置を同期する
struct MovieData {

    bool Load(const std::string& filepath, double speed) {
        this->Delete();
        Handle = LoadGraph(filepath.c_str());
        if (Handle == -1) {
            return false;
        }
        GetGraphSizeF(Handle, &Size.Width, &Size.Height);
        SetMovieVolumeToGraph(0, Handle);
        SetPlaySpeedRateMovieToGraph(Handle, speed);
        return true;
    }
    bool IsLoaded() const {
        return Handle != -1;
    }

    // TargetMs: 今表示すべき動画上の位置 (ミリ秒)。負なら開始前
    // Running : 譜面が進行中か (止まっている間は動画も一時停止する)
    void Sync(double TargetMs, bool Running) {
        if (Handle == -1) {
            return;
        }
        if (!Running || TargetMs < 0) {
            if (m_Playing) {
                PauseMovieToGraph(Handle);
                m_Playing = false;
            }
            if (TargetMs < 0 && !m_AtStart) {
                Seek(0);
            }
            return;
        }

        const int pos = TellMovieToGraph(Handle);
        if (!m_Playing) {
            if (m_Ended && TargetMs >= pos) {
                return;     // 最後まで再生済み (最終フレームを表示し続ける)
            }
            Seek(TargetMs);
            PlayMovieToGraph(Handle);
            m_Playing = true;
            return;
        }
        if (GetMovieStateToGraph(Handle) == 0) {
            m_Playing = false;  // 終端に達して停止した
            m_Ended = true;
            return;
        }
        // ずれが大きい時だけシークする。シーク直後は位置の反映が遅れるため、しばらくは再シークしない
        if (std::abs(pos - TargetMs) > SeekThresholdMs && std::chrono::steady_clock::now() - m_LastSeek > SeekCooldown) {
            Seek(TargetMs);
        }
    }

    // アスペクト比を保ったまま、指定範囲の中央に収めて描画する
    void Draw(float x, float y, float w, float h) const {
        if (Handle == -1 || Size.Width <= 0 || Size.Height <= 0) {
            return;
        }
        const float scale = std::min(w / Size.Width, h / Size.Height);
        const float dw = Size.Width * scale;
        const float dh = Size.Height * scale;
        DrawExtendGraphF(x + (w - dw) * 0.5f, y + (h - dh) * 0.5f, x + (w + dw) * 0.5f, y + (h + dh) * 0.5f, Handle, FALSE);
    }

    void Delete() {
        if (Handle != -1) {
            DeleteGraph(Handle);
        }
        Size = Size2D<float>();
        Handle = -1;
        m_Playing = false;
        m_Ended = false;
        m_AtStart = true;
    }

    int Handle = -1;
    Size2D<float> Size{ 0,0 };

private:
    void Seek(double ms) {
        SeekMovieToGraph(Handle, (int)std::max(0.0, ms));
        m_LastSeek = std::chrono::steady_clock::now();
        m_AtStart = ms <= 0;
        m_Ended = false;
    }

    static constexpr double SeekThresholdMs = 120.0;
    static constexpr std::chrono::milliseconds SeekCooldown{ 1000 };

    bool m_Playing = false;
    bool m_Ended = false;
    bool m_AtStart = true;
    std::chrono::steady_clock::time_point m_LastSeek{};
};

#undef JSONPARSE

#define FROMJSON(name) data.at(#name).get_to(v.name)
template<class T>
inline void from_json(const json& data, Pos2D<T>& v) {
    FROMJSON(X);
    FROMJSON(Y);
}

template<class T>
inline void from_json(const json& data, Size2D<T>& v) {
    FROMJSON(Width);
    FROMJSON(Height);
}

template<class T>
inline void from_json(const json& data, Rect2D<T>& v) {
    FROMJSON(Top);
    FROMJSON(Bottom);
    FROMJSON(Left);
    FROMJSON(Right);
}

template<class T>
inline void from_json(const json& data, Color3<T>& v) {
    FROMJSON(R);
    FROMJSON(G);
    FROMJSON(B);
}

template<class T>
inline void from_json(const json& data, Color4<T>& v) {
    FROMJSON(R);
    FROMJSON(G);
    FROMJSON(B);
    FROMJSON(A);
}
#undef FROMJSON