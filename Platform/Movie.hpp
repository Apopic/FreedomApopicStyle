#pragma once
// 背景動画。ffmpeg.exe に生の映像 (BGRA) を出させて、別スレッドで受け取る。
// 表示は譜面の時刻 (Sync の TargetMs) に合うフレームだけをテクスチャへ送るので、描画のループは待たない。
#include "Gfx.hpp"
#include <atomic>
#include <memory>
#include <string>

namespace gfx {
    // ffmpeg.exe の場所 (空なら動画は使えない)
    void SetMoviePlayerPath(const std::string& ffmpeg);

    class Movie {
    public:
        Movie();
        ~Movie();
        Movie(const Movie&) = delete;
        Movie& operator=(const Movie&) = delete;

        bool Open(const std::string& path);
        void Close();
        bool IsOpen() const;
        float Width() const;
        float Height() const;
        // targetMs: 今表示すべき動画の位置 (ミリ秒。負なら開始前)。running: 進めるか (止めると最後のフレームのまま)
        void Sync(double targetMs, bool running);
        const Texture& Frame() const;
        bool Ended() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> P;
    };
}
