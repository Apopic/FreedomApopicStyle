#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstring>
#include <memory>
#include "../Platform/Gfx.hpp"
#include "../Platform/Text.hpp"
#include "../Platform/Audio.hpp"
#include "../Platform/Movie.hpp"
#include "nlohmann/json.hpp"
#include "Mp3Timing.hpp"
#include "NumericCast.hpp"

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

// mp3 のエンコーダーの遅延・詰め物を除いた波形 (録画の合成・譜面制作の波形表示で使う)
inline std::shared_ptr<snd::Pcm> LoadAlignedPcm(const std::string& path) {
    const auto info = mp3timing::Read(path);
    auto pcm = snd::Decode(path);
    if (!pcm) { return nullptr; }
    const auto count = pcm->Frames();
    const int head = info.Head(count, pcm->Rate);
    const auto frames = info.Samples - info.Delay - info.Padding;
    if (!head || frames <= 0 || head + frames > count) { return pcm; }
    auto trimmed = std::make_shared<snd::Pcm>();
    trimmed->Rate = pcm->Rate;
    trimmed->Channels = pcm->Channels;
    trimmed->Samples.assign(pcm->Samples.begin() + (size_t)head * pcm->Channels, pcm->Samples.begin() + (size_t)(head + frames) * pcm->Channels);
    return trimmed;
}

struct GraphData {

    std::string SourcePath;

    Pos2D<float> Pos;
    Size2D<float> Size;
    Pos2D<int> Div{ 1,1 };

    void Load(const std::string& filepath, Pos2D<int> div = { 1,1 }) {

        int _w = 0, _h = 0;
        gfx::ImageSize(filepath, _w, _h);
        Size = { (float)_w, (float)_h };
        Div = div;

        LoadDiv(filepath, _w, _h);
    }
    void Load(const std::string& dirpath, const json& data) {

        std::string FilePath = "";

        JSONPARSE(FilePath);
        const bool nofile = FilePath.empty();

        FilePath = (dirpath + FilePath);

        int _w = 0, _h = 0;
        if (!nofile) { gfx::ImageSize(FilePath, _w, _h); }
        Size = { (float)_w, (float)_h };

        JSONPARSE(Size);
        JSONPARSE(Pos);
        JSONPARSE(Div);

        // スキンに無い画像 (FilePath が空) は読まない。フォルダを読むと「読めた・大きさ 0」になり、代わりの画像が使われなくなる
        if (nofile) { this->Delete(); return; }
        LoadDiv(FilePath, _w, _h);
    }
    void Delete() {
        Handles.clear();
        SourcePath.clear();
    }
    void Draw(Pos2D<float> pos, int index = 0) const {
        if (index < 0 || Handles.size() <= static_cast<size_t>(index)) {
            return;
        }
        gfx::DrawTexture(Handles[(size_t)index],
            Pos.X + pos.X - (Size.Width * 0.5f),
            Pos.Y + pos.Y - (Size.Height * 0.5f),
            Pos.X + pos.X + (Size.Width * 0.5f),
            Pos.Y + pos.Y + (Size.Height * 0.5f));
    }
    void RectDraw(Pos2D<float> pos, Pos2D<float> originpos, Size2D<float> size, int index = 0) const {
        if (index < 0 || Handles.size() <= static_cast<size_t>(index)) {
            return;
        }
        if (Size.Width <= 0 || Size.Height <= 0 || size.Width <= 0 || size.Height <= 0) { return; }
        const auto& frame = Handles[(size_t)index];
        const float left = Pos.X + pos.X - Size.Width * .5f;
        const float top = Pos.Y + pos.Y - Size.Height * .5f;
        gfx::DrawTexturePart(frame, left, top, left + size.Width, top + size.Height,
            originpos.X * frame.Width() / Size.Width,
            originpos.Y * frame.Height() / Size.Height,
            size.Width * frame.Width() / Size.Width, size.Height * frame.Height() / Size.Height);
    }

    // 分割したコマ (1 枚のテクスチャを共有し、切り出す位置だけが違う)
    std::vector<gfx::Texture> Handles;

private:

    void LoadDiv(const std::string& filepath, int w, int h) {

        this->Delete();

        if (Div.X <= 0 || Div.Y <= 0) {
            Div = { 1,1 };
        }
        if (Div.X > 4096 || Div.Y > 4096 || (int64_t)Div.X * Div.Y > 16384) {
            throw std::invalid_argument("画像の分割数が大きすぎます");
        }
        if (w <= 0 || h <= 0) return;

        auto handles = gfx::LoadDivTexture(filepath, Div.X, Div.Y, (float)w / Div.X, (float)h / Div.Y);
        if (handles.empty()) { return; }
        Handles = std::move(handles);
        SourcePath = filepath;
    }
};

struct FontData {

    float Size = 0;
    float WidthScale = 1;
    float Thick = 0;
    float Edge = 0;
    int Space = 0;
    enum Position : int {
        Left,
        Center,
        Right
    } Position = Left;

    void Load(const std::string& filepath, const std::string& fontname, float size, float thick, float edge, int space) {
        this->Delete();
        WidthScale = 1;
        Size = size;
        Thick = thick * 0.5f;
        Edge = edge;
        Space = space;
        Open(filepath, fontname);
    }
    void Load(const std::string& dirpath, const json& data) {

        std::string FontName = "";
        std::string FilePath = "";

        this->Delete();

        JSONPARSE(FilePath);
        const bool nofile = FilePath.empty();

        FilePath = (dirpath + FilePath);

        WidthScale = data.value("WidthScale",1.0f);
        if(!std::isfinite(WidthScale)||WidthScale<=0)WidthScale=1;
        JSONPARSE(Size);
        JSONPARSE(Thick);
        Thick *= 0.5f;
        JSONPARSE(Edge);
        if (data.find("Position") != data.end()) ((int&)Position) = data["Position"].get<int>();
        JSONPARSE(Space);

        JSONPARSE(FontName);

        Open(nofile ? std::string() : FilePath, FontName);
    }
    void Delete() {
        if (Handle != -1) {
            txt::CloseFont(Handle);
        }
        Handle = -1;
    }
    void Draw(Pos2D<float> pos, unsigned int color, unsigned int edgecolor, const std::string& str) const {
        if (Handle == -1 || str.empty()) { return; }
        Draw(pos, color, edgecolor, (size_t)txt::Width(Handle, str), str);
    }
    void Draw(Pos2D<float> pos, unsigned int color, unsigned int edgecolor, size_t strlen, const std::string& str) const {
        if (Handle == -1) { return; }
        if (str.empty()) { return; }
        float offset = 0;
        switch (Position)
        {
        case Left:
            break;
        case Center:
            offset = WidthScale * strlen * 0.5f;
            break;
        case Right:
            offset = WidthScale * strlen;
            break;
        }
        txt::Draw(Handle, pos.X - offset, pos.Y - Size * 0.5f, str, color, edgecolor, WidthScale, 1.0f);
    }

    int Handle = -1;

private:
    void Open(const std::string& filepath, const std::string& fontname) {
        if (!filepath.empty()) { txt::RegisterFontFile(filepath); }
        txt::FontOptions o;
        o.File = filepath;
        o.Name = fontname;
        o.Size = Size;
        o.Thick = numeric::Cast<int>(Thick);
        o.Edge = numeric::Cast<int>(Edge);
        o.Space = Space;
        Handle = txt::OpenFont(o);
    }
};

struct SoundData {
    static constexpr int DefaultSEBufferNum = 4;
    int BufferNum = 0;
    int Frequency = 0;
    int Volume = 0;
    mp3timing::Info Timing;
    double StartMilliseconds = 0;
    void UpdateTiming() {
        const int rate = snd::OriginalFrequency(Handle);
        StartMilliseconds = rate > 0 ? Timing.Head(snd::TotalFrames(Handle), rate) * 1000.0 / rate : 0;
    }
    std::string FilePath;       // 読み込んだファイル (録画の音声を譜面から作る時に使う)

    void Load(const std::string& filepath, int buffernum = 1) {
        const auto timing = mp3timing::Read(filepath);
        this->Delete();
        BufferNum = std::clamp(buffernum,1,64);
        FilePath = filepath;
        Handle = snd::Load(filepath, BufferNum);
        Timing = timing;
        UpdateTiming();
        Frequency = snd::Frequency(Handle);
        Volume = snd::Volume(Handle);
    }
    void Load(const std::string& dirpath, const json& data) {

        std::string FilePath = "";

        // 効果音は連打で重ねて鳴らすため、指定が無ければ複数バッファを確保する (1 だと鳴り直しで前の音が途切れる)
        BufferNum = DefaultSEBufferNum;
        JSONPARSE(FilePath);
        JSONPARSE(BufferNum);
        BufferNum = std::clamp(BufferNum,1,64);

        if (FilePath.empty()) { this->Delete(); return; }
        FilePath = (dirpath + FilePath);

        Load(FilePath, BufferNum);
    }
    void Load(const void* filedata, size_t filesize, int buffernum = 1) {
        this->Delete();
        BufferNum = buffernum;
        Handle = snd::LoadMemory(filedata, filesize, BufferNum);
        Timing = mp3timing::Parse(filedata, filesize);
        UpdateTiming();
        Frequency = snd::Frequency(Handle);
        Volume = snd::Volume(Handle);
    }
    void Delete() {
        if (Handle != -1) {
            snd::Delete(Handle);
        }
        Handle = -1;
    }
    void Play(bool toppositionflag = true) const {
        if (toppositionflag && StartMilliseconds > 0) {
            SetCurrent(0);
            toppositionflag = false;
        }
        snd::Play(Handle, toppositionflag);
    }
    void Stop() const {
        snd::Stop(Handle);
    }
    // 非同期で読み込みを開始する (演奏を止めずに段位の次の曲を先読みするため)
    void LoadAsync(const std::string& filepath, int buffernum = 1) {
        const auto timing = mp3timing::Read(filepath);
        this->Delete();
        FilePath = filepath;
        BufferNum = std::clamp(buffernum,1,64);
        Handle = snd::LoadAsync(filepath, BufferNum);
        Frequency = 0;
        Volume = 0;
        StartMilliseconds = 0;
        Timing = timing;
    }
    // 非同期読み込みの完了を待ち、周波数などを取得する
    void FinishAsync() {
        if (Handle == -1) {
            return;
        }
        snd::Wait(Handle);
        UpdateTiming();
        Frequency = snd::Frequency(Handle);
        Volume = snd::Volume(Handle);
    }
    bool IsPlay() const {
        return snd::IsPlaying(Handle);
    }
    void SetFrequency(int frequency) const {
        snd::SetFrequency(Handle, frequency);
    }
    void SetVolume(float volume) const {
        snd::SetVolume(Handle, static_cast<int>((volume / 100.0) * 255.0));
    }
    void SetCurrent(double time) const {
        snd::SetPositionMs(Handle, time + StartMilliseconds);
    }
    double Current() const {
        return snd::PositionMs(Handle) - StartMilliseconds;
    }

    int Handle = -1;
};

// 背景動画。譜面の時刻に合わせて再生位置を同期する
struct MovieData {

    bool Load(const std::string& filepath, double speed) {
        (void)speed;    // 表示する位置は譜面の時刻から決めるので、再生速度は使わない
        this->Delete();
        if (!m_Movie->Open(filepath)) {
            return false;
        }
        Size = { m_Movie->Width(), m_Movie->Height() };
        return true;
    }
    bool IsLoaded() const {
        return m_Movie->IsOpen();
    }

    // TargetMs: 今表示すべき動画上の位置 (ミリ秒)。負なら開始前
    // Running : 譜面が進行中か (止まっている間は動画も一時停止する)
    void Sync(double TargetMs, bool Running) {
        m_Movie->Sync(TargetMs, Running);
    }

    // アスペクト比を保ったまま、指定範囲の中央に収めて描画する
    void Draw(float x, float y, float w, float h) const {
        if (!IsLoaded() || Size.Width <= 0 || Size.Height <= 0) {
            return;
        }
        const float scale = std::min(w / Size.Width, h / Size.Height);
        const float dw = Size.Width * scale;
        const float dh = Size.Height * scale;
        gfx::DrawTexture(m_Movie->Frame(), x + (w - dw) * 0.5f, y + (h - dh) * 0.5f, x + (w + dw) * 0.5f, y + (h + dh) * 0.5f);
    }

    void Delete() {
        m_Movie->Close();
        Size = Size2D<float>();
    }

    Size2D<float> Size{ 0,0 };

private:
    std::shared_ptr<gfx::Movie> m_Movie = std::make_shared<gfx::Movie>();
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
