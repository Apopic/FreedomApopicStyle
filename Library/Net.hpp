#pragma once
// FreedomApopicStyle オンラインプレイ用の通信プロトコル。
// ゲーム本体と MultiServer の両方から使うため、DxLib には依存しないこと。
//
// ■ 接続の流れ
//   1. 鍵交換 (平文):  ClientHello / ServerHello で X25519 の公開鍵とノンスを交換する
//   2. 以降のフレームはすべて ChaCha20-Poly1305 で暗号化・改ざん検出する
//      | Magic(4) | Size(4) | 暗号文 (Type(4) + Payload) | Tag(16) |
//      メッセージの種類も暗号化される。鍵は接続ごと・方向ごとに別で、フレームごとに通し番号をノンスに使う
//
// ■ 楽曲の配布
//   譜面と音源は SongBegin / SongChunk / SongEnd で 256KB ずつストリーム送信し、受信側はメモリ上で組み立てる。
//   受信側 (サーバー・ゲスト) はディスクに書き込まない。
//
// ペイロードは Writer / Reader によるリトルエンディアンのバイナリ。
// 受信側はすべての長さ・件数を検証し、不正なデータは読み捨てる (サーバーが落ちないように)。

#include "Socket.h"
#include "NetCrypto.hpp"
#ifndef _WIN32
#include <netinet/in.h>
#include <netinet/tcp.h>      // TCP_NODELAY
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

namespace fasnet {

    inline constexpr uint32_t ProtocolVersion = 5;
    inline constexpr uint32_t HandshakeMagic = 0x33484146;          // "FAH3"
    inline constexpr uint32_t FrameMagic = 0x33534146;              // "FAS3"
    inline constexpr uint32_t MaxFrameSize = 1024 * 1024;           // 暗号化フレーム 1 つの上限
    inline constexpr uint32_t MaxHandshakeSize = 4096;
    inline constexpr size_t SongChunkSize = 256 * 1024;
    inline constexpr uint32_t MaxChartSize = 32u * 1024 * 1024;
    inline constexpr uint32_t MaxAudioSize = 512u * 1024 * 1024;
    inline constexpr size_t MaxPlayers = 4;
    inline constexpr size_t MaxNameLength = 48;                     // UTF-8 のバイト数
    inline constexpr size_t MaxTextLength = 1024;
    inline constexpr uint32_t StartDelayMs = 3000;                  // Start 受信から演奏開始までの猶予
    inline constexpr uint16_t DefaultPort = 8080;

    enum class Msg : uint32_t {
        Hello = 1,      // C->S  Hello
        Welcome,        // S->C  Welcome
        Reject,         // S->C  Text (理由)。送信後に切断される
        RoomState,      // S->C  RoomState
        Ping,           // C->S  u64 (送信時刻)
        Pong,           // S->C  u64 (Ping の値をそのまま返す)
        SongBegin,      // C->S  SongInfo (ホストのみ) / S->C SongInfo (ホスト以外へ)
        SongChunk,      // C->S  SongChunk / S->C SongChunk
        SongEnd,        // C->S  U32Value / S->C U32Value (SongSerial)
        Ready,          // C->S  U32Value (読み込みを終えた曲の SongSerial。0 は未完了)
        StartRequest,   // C->S  なし (ホストのみ。全員 Ready の時だけ受理)
        Start,          // S->C  StartInfo
        Hit,            // C->S  HitEvent / S->C HitEvent (他プレイヤーへ転送)
        Finish,         // C->S  ScoreState (演奏終了)
        ReturnLobby,    // C->S  なし (ホストのみ)
        TransferHost,   // C->S  u32 (新しいホストの PlayerId)
        SetName,        // C->S  Text
        SetTitle,       // C->S  Text (称号。空なら表示しない)
    };

    enum class PlayerState : uint8_t {
        Lobby,      // 待機中
        Loading,    // 譜面を受信・読み込み中
        Ready,      // 読み込み完了
        Playing,    // 演奏中
        Finished,   // 演奏終了
    };

    enum class RoomPhase : uint8_t {
        Lobby,          // 曲未選択
        SongSelected,   // 曲を配布して全員の読み込み待ち
        Playing,        // 演奏中
        Result,         // 全員の演奏が終了
    };

    // ---------------------------------------------------------------- シリアライズ

    class Writer {
    public:
        template<class T> requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
        Writer& Put(T value) {
            const size_t n = Buffer.size();
            Buffer.resize(n + sizeof(T));
            std::memcpy(Buffer.data() + n, &value, sizeof(T));
            return *this;
        }
        Writer& Str(std::string_view str) {
            Put<uint32_t>(static_cast<uint32_t>(str.size()));
            Buffer.insert(Buffer.end(), str.begin(), str.end());
            return *this;
        }
        Writer& Bytes(std::span<const uint8_t> bytes) {
            Put<uint32_t>(static_cast<uint32_t>(bytes.size()));
            Buffer.insert(Buffer.end(), bytes.begin(), bytes.end());
            return *this;
        }
        std::vector<uint8_t> Buffer;
    };

    class Reader {
    public:
        explicit Reader(std::span<const uint8_t> data) : m_Data(data) {}

        template<class T> requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
        T Get() {
            T value{};
            if (!m_Ok || Remaining() < sizeof(T)) {
                m_Ok = false;
                return value;
            }
            std::memcpy(&value, m_Data.data() + m_Pos, sizeof(T));
            m_Pos += sizeof(T);
            return value;
        }
        std::string Str(size_t maxlength = MaxTextLength) {
            const uint32_t size = Get<uint32_t>();
            if (!m_Ok || size > maxlength || Remaining() < size) {
                m_Ok = false;
                return {};
            }
            std::string ret(reinterpret_cast<const char*>(m_Data.data() + m_Pos), size);
            m_Pos += size;
            return ret;
        }
        std::vector<uint8_t> Bytes(size_t maxlength = MaxFrameSize) {
            const uint32_t size = Get<uint32_t>();
            if (!m_Ok || size > maxlength || Remaining() < size) {
                m_Ok = false;
                return {};
            }
            std::vector<uint8_t> ret(m_Data.begin() + m_Pos, m_Data.begin() + m_Pos + size);
            m_Pos += size;
            return ret;
        }
        // 件数の上限付きで要素数を読む
        size_t Count(size_t maxcount) {
            const uint32_t count = Get<uint32_t>();
            if (!m_Ok || count > maxcount) {
                m_Ok = false;
                return 0;
            }
            return count;
        }
        void Fail() { m_Ok = false; }
        bool Ok() const { return m_Ok; }

    private:
        size_t Remaining() const { return m_Data.size() - m_Pos; }

        std::span<const uint8_t> m_Data;
        size_t m_Pos = 0;
        bool m_Ok = true;
    };

    // 型 T をペイロードとして読み込む (失敗時は false)
    template<class T>
    bool Decode(std::span<const uint8_t> payload, T& out) {
        Reader r(payload);
        out.Read(r);
        return r.Ok();
    }
    template<class T>
    std::vector<uint8_t> Encode(const T& data) {
        Writer w;
        data.Write(w);
        return std::move(w.Buffer);
    }

    // 名前などの表示用文字列から制御文字を除き、長さを制限する (UTF-8 の途中では切らない)
    inline std::string SanitizeText(std::string_view str, size_t maxlength) {
        std::string ret;
        for (unsigned char c : str) {
            if (c < 0x20 || c == 0x7F) { continue; }
            ret.push_back(static_cast<char>(c));
        }
        if (ret.size() > maxlength) {
            size_t cut = maxlength;
            while (cut > 0 && (static_cast<unsigned char>(ret[cut]) & 0xC0) == 0x80) { --cut; }
            ret.resize(cut);
        }
        return ret;
    }

    // ---------------------------------------------------------------- メッセージ

    struct Text {
        std::string Value;
        void Write(Writer& w) const { w.Str(Value); }
        void Read(Reader& r) { Value = r.Str(); }
    };

    struct U32Value {
        uint32_t Value = 0;
        void Write(Writer& w) const { w.Put(Value); }
        void Read(Reader& r) { Value = r.Get<uint32_t>(); }
    };

    struct U64Value {
        uint64_t Value = 0;
        void Write(Writer& w) const { w.Put(Value); }
        void Read(Reader& r) { Value = r.Get<uint64_t>(); }
    };

    struct Hello {
        uint32_t Version = ProtocolVersion;
        std::string Name;
        std::string Title;              // 称号 (空なら表示しない)
        void Write(Writer& w) const { w.Put(Version).Str(Name).Str(Title); }
        void Read(Reader& r) {
            Version = r.Get<uint32_t>();
            if (Version != ProtocolVersion) { return; }     // 古いクライアントは以降の形式が違う (バージョン違いとして断る)
            Name = r.Str(MaxTextLength);
            Title = r.Str(MaxTextLength);
        }
    };

    inline constexpr size_t MaxAddresses = 8;

    // サーバーに接続できるアドレス (他の人を招待するため。Discord の「参加」にも使う)
    struct AddressEntry {
        std::string Label;              // 「LAN」「外部」など
        std::string Host;               // IPv4 アドレス
        uint16_t Port = DefaultPort;
        std::string Text() const { return Host + ":" + std::to_string(Port); }
    };

    struct Welcome {
        uint32_t PlayerId = 0;
        std::vector<AddressEntry> Addresses;
        void Write(Writer& w) const {
            w.Put(PlayerId);
            const size_t n = std::min(Addresses.size(), MaxAddresses);
            w.Put<uint32_t>(static_cast<uint32_t>(n));
            for (size_t i = 0; i < n; ++i) { w.Str(Addresses[i].Label).Str(Addresses[i].Host).Put(Addresses[i].Port); }
        }
        void Read(Reader& r) {
            PlayerId = r.Get<uint32_t>();
            Addresses.resize(r.Count(MaxAddresses));
            for (auto& a : Addresses) {
                a.Label = r.Str(MaxTextLength);
                a.Host = r.Str(64);
                a.Port = r.Get<uint16_t>();
                if (a.Port == 0) { r.Fail(); }
            }
        }
    };

    // 「ホスト:ポート」「ホスト」(ポートは既定値) を分ける。IPv6 は [addr]:port で書く
    inline std::optional<std::pair<std::string, uint16_t>> SplitHostPort(std::string_view text, uint16_t defport = DefaultPort) {
        auto trim = [](std::string_view v) {
            auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
            while (!v.empty() && space(v.front())) { v.remove_prefix(1); }
            while (!v.empty() && space(v.back())) { v.remove_suffix(1); }
            return v;
        };
        text = trim(text);
        if (text.empty() || text.size() > 255) { return std::nullopt; }
        std::string_view host = text, port;
        if (text.front() == '[') {
            const size_t close = text.find(']');
            if (close == std::string_view::npos) { return std::nullopt; }
            host = text.substr(1, close - 1);
            if (close + 1 < text.size()) {
                if (text[close + 1] != ':') { return std::nullopt; }
                port = text.substr(close + 2);
            }
        }
        else if (const size_t colon = text.rfind(':'); colon != std::string_view::npos && text.find(':') == colon) {
            host = text.substr(0, colon);
            port = text.substr(colon + 1);
        }
        if (host.empty()) { return std::nullopt; }
        uint16_t value = defport;
        if (!port.empty()) {
            unsigned v = 0;
            for (char c : port) {
                if (c < '0' || c > '9') { return std::nullopt; }
                v = v * 10 + (unsigned)(c - '0');
                if (v > 65535) { return std::nullopt; }
            }
            if (v == 0) { return std::nullopt; }
            value = (uint16_t)v;
        }
        return std::pair<std::string, uint16_t>{ std::string(host), value };
    }

    struct ScoreState {
        uint64_t Score = 0;
        uint64_t Good = 0;
        uint64_t Ok = 0;
        uint64_t Bad = 0;
        uint64_t Roll = 0;
        uint64_t Combo = 0;
        uint64_t MaxCombo = 0;
        uint64_t HitNote = 0;
        double Accuracy = 0.0;

        void Write(Writer& w) const {
            w.Put(Score).Put(Good).Put(Ok).Put(Bad).Put(Roll).Put(Combo).Put(MaxCombo).Put(HitNote).Put(Accuracy);
        }
        void Read(Reader& r) {
            Score = r.Get<uint64_t>();
            Good = r.Get<uint64_t>();
            Ok = r.Get<uint64_t>();
            Bad = r.Get<uint64_t>();
            Roll = r.Get<uint64_t>();
            Combo = r.Get<uint64_t>();
            MaxCombo = r.Get<uint64_t>();
            HitNote = r.Get<uint64_t>();
            Accuracy = r.Get<double>();
        }
    };

    struct PlayerInfo {
        uint32_t Id = 0;
        std::string Name;
        std::string Title;              // 称号 (空なら表示しない)
        bool IsHost = false;
        PlayerState State = PlayerState::Lobby;
        ScoreState Score;

        void Write(Writer& w) const {
            w.Put(Id).Str(Name).Str(Title).Put<uint8_t>(IsHost).Put(State);
            Score.Write(w);
        }
        void Read(Reader& r) {
            Id = r.Get<uint32_t>();
            Name = r.Str(MaxNameLength);
            Title = r.Str(MaxNameLength);
            IsHost = r.Get<uint8_t>() != 0;
            State = r.Get<PlayerState>();
            if (State > PlayerState::Finished) { r.Fail(); }
            Score.Read(r);
        }
    };

    struct RoomState {
        RoomPhase Phase = RoomPhase::Lobby;
        uint32_t SongSerial = 0;
        std::string Title;
        std::string Subtitle;
        int32_t CourseIndex = 0;
        uint64_t Level = 0;
        double SongSpeed = 1.0;
        std::vector<PlayerInfo> Players;

        const PlayerInfo* Find(uint32_t id) const {
            auto it = std::ranges::find(Players, id, &PlayerInfo::Id);
            return it != Players.end() ? &*it : nullptr;
        }
        PlayerInfo* Find(uint32_t id) {
            auto it = std::ranges::find(Players, id, &PlayerInfo::Id);
            return it != Players.end() ? &*it : nullptr;
        }

        void Write(Writer& w) const {
            w.Put(Phase).Put(SongSerial).Str(Title).Str(Subtitle).Put(CourseIndex).Put(Level).Put(SongSpeed);
            w.Put<uint32_t>(static_cast<uint32_t>(Players.size()));
            for (const auto& p : Players) { p.Write(w); }
        }
        void Read(Reader& r) {
            Phase = r.Get<RoomPhase>();
            if (Phase > RoomPhase::Result) { r.Fail(); }
            SongSerial = r.Get<uint32_t>();
            Title = r.Str();
            Subtitle = r.Str();
            CourseIndex = r.Get<int32_t>();
            Level = r.Get<uint64_t>();
            SongSpeed = r.Get<double>();
            Players.resize(r.Count(MaxPlayers));
            for (auto& p : Players) { p.Read(r); }
        }
    };

    // ホストが選んだ曲。譜面と音源をそのまま配布する
    // 楽曲配布の開始。続いて SongChunk が届き、SongEnd で完了する
    struct SongInfo {
        uint32_t Serial = 0;            // サーバーが付ける通し番号 (ホストからの送信時は 0)
        int32_t CourseIndex = 0;
        uint64_t Level = 0;
        double SongSpeed = 1.0;
        std::string Title;
        std::string Subtitle;
        uint32_t ChartSize = 0;
        uint32_t AudioSize = 0;

        void Write(Writer& w) const {
            w.Put(Serial).Put(CourseIndex).Put(Level).Put(SongSpeed).Str(Title).Str(Subtitle).Put(ChartSize).Put(AudioSize);
        }
        void Read(Reader& r) {
            Serial = r.Get<uint32_t>();
            CourseIndex = r.Get<int32_t>();
            Level = r.Get<uint64_t>();
            SongSpeed = r.Get<double>();
            Title = r.Str();
            Subtitle = r.Str();
            ChartSize = r.Get<uint32_t>();
            AudioSize = r.Get<uint32_t>();
            if (!(SongSpeed > 0.0 && SongSpeed <= 4.0) || ChartSize == 0 || ChartSize > MaxChartSize || AudioSize == 0 || AudioSize > MaxAudioSize) {
                r.Fail();
            }
        }
    };

    struct SongChunk {
        enum Kind : uint8_t { Chart = 0, Audio = 1 };
        uint32_t Serial = 0;
        uint8_t Type = Chart;
        uint32_t Offset = 0;
        std::vector<uint8_t> Data;

        void Write(Writer& w) const { w.Put(Serial).Put(Type).Put(Offset).Bytes(Data); }
        void Read(Reader& r) {
            Serial = r.Get<uint32_t>();
            Type = r.Get<uint8_t>();
            Offset = r.Get<uint32_t>();
            Data = r.Bytes(SongChunkSize);
            if (Type > Audio || Data.empty()) { r.Fail(); }
        }
    };

    // 受信して組み立てた曲 (メッセージではない)
    struct SongData {
        uint32_t Serial = 0;
        int32_t CourseIndex = 0;
        uint64_t Level = 0;
        double SongSpeed = 1.0;
        std::string Title;
        std::string Subtitle;
        std::vector<uint8_t> Chart;
        std::vector<uint8_t> Audio;

        // 使い終わった譜面・音源をメモリ上から消す
        void Wipe() {
            crypto::SecureZero(Chart);
            crypto::SecureZero(Audio);
        }
    };

    // SongBegin / SongChunk / SongEnd を検証しながらメモリ上で組み立てる (サーバーとゲストで共用)
    class SongReceiver {
    public:
        ~SongReceiver() { Clear(); }

        bool Begin(const SongInfo& info) {
            Clear();
            m_Info = info;
            m_Chart.reserve(info.ChartSize);
            m_Audio.reserve(info.AudioSize);
            m_Active = true;
            return true;
        }
        // 順番通りに届いたチャンクだけを受け付ける (TCP なので順序は保証されている)
        bool Append(const SongChunk& chunk) {
            if (!m_Active || chunk.Serial != m_Info.Serial) {
                return false;
            }
            auto& buffer = chunk.Type == SongChunk::Chart ? m_Chart : m_Audio;
            const size_t expected = chunk.Type == SongChunk::Chart ? m_Info.ChartSize : m_Info.AudioSize;
            if (chunk.Offset != buffer.size() || buffer.size() + chunk.Data.size() > expected) {
                return false;
            }
            buffer.insert(buffer.end(), chunk.Data.begin(), chunk.Data.end());
            return true;
        }
        bool IsActive() const { return m_Active; }
        bool IsComplete() const { return m_Active && m_Chart.size() == m_Info.ChartSize && m_Audio.size() == m_Info.AudioSize; }
        const SongInfo& Info() const { return m_Info; }
        size_t Received() const { return m_Chart.size() + m_Audio.size(); }
        size_t Total() const { return (size_t)m_Info.ChartSize + m_Info.AudioSize; }
        const std::vector<uint8_t>& ChartData() const { return m_Chart; }
        const std::vector<uint8_t>& AudioData() const { return m_Audio; }

        std::optional<SongData> Take() {
            if (!IsComplete()) {
                return std::nullopt;
            }
            SongData song;
            song.Serial = m_Info.Serial;
            song.CourseIndex = m_Info.CourseIndex;
            song.Level = m_Info.Level;
            song.SongSpeed = m_Info.SongSpeed;
            song.Title = m_Info.Title;
            song.Subtitle = m_Info.Subtitle;
            song.Chart = std::move(m_Chart);
            song.Audio = std::move(m_Audio);
            m_Chart.clear();
            m_Audio.clear();
            m_Active = false;
            return song;
        }
        void Clear() {
            crypto::SecureZero(m_Chart);
            crypto::SecureZero(m_Audio);
            m_Info = SongInfo();
            m_Active = false;
        }

    private:
        SongInfo m_Info;
        std::vector<uint8_t> m_Chart;
        std::vector<uint8_t> m_Audio;
        bool m_Active = false;
    };

    // 譜面・音源を SongChunk のペイロード列に分割する
    inline std::vector<std::vector<uint8_t>> EncodeSongChunks(uint32_t serial, std::span<const uint8_t> chart, std::span<const uint8_t> audio) {
        std::vector<std::vector<uint8_t>> out;
        auto split = [&](std::span<const uint8_t> data, uint8_t type) {
            for (size_t offset = 0; offset < data.size(); offset += SongChunkSize) {
                SongChunk chunk;
                chunk.Serial = serial;
                chunk.Type = type;
                chunk.Offset = (uint32_t)offset;
                const auto part = data.subspan(offset, std::min(SongChunkSize, data.size() - offset));
                chunk.Data.assign(part.begin(), part.end());
                out.push_back(Encode(chunk));
                crypto::SecureZero(chunk.Data);
            }
        };
        split(chart, SongChunk::Chart);
        split(audio, SongChunk::Audio);
        return out;
    }

    struct StartInfo {
        uint32_t DelayMs = StartDelayMs;
        void Write(Writer& w) const { w.Put(DelayMs); }
        void Read(Reader& r) { DelayMs = r.Get<uint32_t>(); }
    };

    // Lane はゲーム本体の HitType (DonLeft=0 ... KaBig=5, Empty=-1 は見逃し)
    struct HitEvent {
        uint32_t PlayerId = 0;
        int8_t Lane = -1;
        int8_t Judge = -1;          // JudgeType (None=-1)
        char NoteType = '\0';
        ScoreState Score;

        void Write(Writer& w) const {
            w.Put(PlayerId).Put(Lane).Put(Judge).Put(NoteType);
            Score.Write(w);
        }
        void Read(Reader& r) {
            PlayerId = r.Get<uint32_t>();
            Lane = r.Get<int8_t>();
            Judge = r.Get<int8_t>();
            NoteType = r.Get<char>();
            Score.Read(r);
            if (Lane < -1 || Lane > 5 || Judge < -1 || Judge > 3) { r.Fail(); }
        }
    };

    // ---------------------------------------------------------------- ソケット

    enum class HandshakeType : uint32_t {
        ClientHello = 1,    // Version(4) Flags(1) Nonce(32) Public(32)
        ServerHello = 2,    // Nonce(32) Public(32)
        Reject = 3,         // Text
    };

    struct SessionConfig {
        bool IsServer = false;
    };

    class NetSocket : public TCPSocket {
    public:
        NetSocket() = default;
        explicit NetSocket(TCPSocket&& sock) : TCPSocket(std::move(sock)) {}

        // フレームを細かく送るため、Nagle を無効にしないと遅延 ACK と合わさって大きく遅れる
        bool SetNoDelay() {
#ifdef _WIN32
            BOOL flag = TRUE;
#else
            int flag = 1;
#endif
            return setsockopt(this->sock(), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag)) == 0;
        }
        // ブロック中の送受信を別スレッドから解除する
        void Shutdown() {
#ifdef _WIN32
            shutdown(this->sock(), SD_BOTH);
#else
            shutdown(this->sock(), SHUT_RDWR);
#endif
        }

        // ---- 鍵交換用の平文フレーム ----
        bool SendPlain(HandshakeType type, std::span<const uint8_t> payload) {
            const uint32_t head[3] = { HandshakeMagic, (uint32_t)type, (uint32_t)payload.size() };
            std::vector<uint8_t> frame(sizeof(head) + payload.size());
            std::memcpy(frame.data(), head, sizeof(head));
            std::copy(payload.begin(), payload.end(), frame.begin() + sizeof(head));
            return this->RawSend(frame.data(), (int)frame.size());
        }
        bool RecvPlain(HandshakeType& type, std::vector<uint8_t>& payload) {
            uint32_t head[3]{};
            if (!this->RawRecv(head, (int)sizeof(head)) || head[0] != HandshakeMagic || head[2] > MaxHandshakeSize) {
                return false;
            }
            type = (HandshakeType)head[1];
            payload.resize(head[2]);
            return payload.empty() || this->RawRecv(payload.data(), (int)payload.size());
        }

        // ---- 暗号化フレーム (送信は送信スレッド、受信は受信スレッドだけが使う) ----
        void SetKeys(const crypto::Key32& sendkey, const crypto::Key32& recvkey) {
            m_Send.SetKey(sendkey);
            m_Recv.SetKey(recvkey);
            m_SendSeq = 0;
            m_RecvSeq = 0;
        }
        bool SendFrame(Msg type, std::span<const uint8_t> payload) {
            const uint32_t size = (uint32_t)(sizeof(uint32_t) + payload.size());
            if (size > MaxFrameSize) {
                return false;
            }
            std::vector<uint8_t> frame(8 + size + crypto::AeadTagSize);
            crypto::StoreLE32(frame.data(), FrameMagic);
            crypto::StoreLE32(frame.data() + 4, size);
            crypto::StoreLE32(frame.data() + 8, (uint32_t)type);
            std::copy(payload.begin(), payload.end(), frame.begin() + 12);
            uint8_t nonce[12];
            MakeNonce(m_SendSeq++, nonce);
            m_Send.Seal(nonce, std::span(frame.data(), 8), std::span(frame.data() + 8, size), frame.data() + 8 + size);
            return this->RawSend(frame.data(), (int)frame.size());
        }
        bool RecvFrame(Msg& type, std::vector<uint8_t>& payload) {
            uint8_t head[8];
            if (!this->RawRecv(head, (int)sizeof(head))) {
                return false;
            }
            const uint32_t size = crypto::LoadLE32(head + 4);
            // 不正なヘッダは巨大なメモリ確保などを防ぐため接続ごと破棄する
            if (crypto::LoadLE32(head) != FrameMagic || size < sizeof(uint32_t) || size > MaxFrameSize) {
                return false;
            }
            std::vector<uint8_t> body(size + crypto::AeadTagSize);
            if (!this->RawRecv(body.data(), (int)body.size())) {
                return false;
            }
            uint8_t nonce[12];
            MakeNonce(m_RecvSeq++, nonce);
            if (!m_Recv.Open(nonce, std::span(head, 8), std::span(body.data(), size), body.data() + size)) {
                return false;   // 改ざん・鍵の不一致
            }
            type = (Msg)crypto::LoadLE32(body.data());
            payload.assign(body.begin() + 4, body.begin() + size);
            crypto::SecureZero(body);
            return true;
        }

    private:
        static void MakeNonce(uint64_t seq, uint8_t nonce[12]) {
            std::memset(nonce, 0, 4);
            crypto::StoreLE64(nonce + 4, seq);
        }

        crypto::Aead m_Send;
        crypto::Aead m_Recv;
        uint64_t m_SendSeq = 0;
        uint64_t m_RecvSeq = 0;
    };

    // ---------------------------------------------------------------- 接続

    // 受信スレッドと送信スレッドを持つ 1 本の暗号化された接続。
    // 鍵交換は受信スレッドの最初に行い、それまでに Send されたメッセージは鍵が決まってから送る。
    // メインスレッドは Send でキューに積み、Poll で受信済みメッセージを取り出すだけ。
    class Connection {
    public:
        struct Message {
            Msg Type{};
            std::vector<uint8_t> Payload;
        };

        Connection() = default;
        ~Connection() { Close(); }
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        void Start(NetSocket&& sock, SessionConfig config) {
            Close();
            m_Sock = std::move(sock);
            m_Sock.SetNoDelay();
            m_Config = std::move(config);
            m_Closing = false;
            m_Alive = true;
            m_Secured = false;
            m_Started = true;
            m_RecvThread = std::thread([this] { RecvLoop(); });
            m_SendThread = std::thread([this] { SendLoop(); });
        }

        bool IsStarted() const { return m_Started; }
        bool IsAlive() const { return m_Started && m_Alive; }
        bool IsSecured() const { return m_Secured; }

        // bulk: 楽曲データなどの大きな送信。通常のメッセージ (打撃・Ping など) を先に送る
        void Send(Msg type, std::vector<uint8_t> payload = {}, bool bulk = false) {
            if (!m_Started) { return; }
            {
                std::lock_guard<std::mutex> lock(m_SendMutex);
                if (bulk) {
                    m_BulkBytes += payload.size();
                    m_BulkQueue.push_back({ type, std::move(payload) });
                }
                else {
                    m_SendQueue.push_back({ type, std::move(payload) });
                }
            }
            m_SendCv.notify_one();
        }
        template<class T>
        void Send(Msg type, const T& data) {
            Send(type, Encode(data));
        }

        // 受信済みのメッセージをすべて取り出す (out は上書き)
        void Poll(std::vector<Message>& out) {
            out.clear();
            std::lock_guard<std::mutex> lock(m_RecvMutex);
            out.swap(m_RecvQueue);
        }

        // 未送信の楽曲データのバイト数 (進捗表示用)
        size_t PendingBulkBytes() {
            std::lock_guard<std::mutex> lock(m_SendMutex);
            return m_BulkBytes;
        }

        // 送信キューが空になるまで待ってから閉じる (Reject などを確実に届けるため)
        void FlushAndClose(std::chrono::milliseconds timeout) {
            const auto limit = std::chrono::steady_clock::now() + timeout;
            while (IsAlive() && std::chrono::steady_clock::now() < limit) {
                {
                    std::lock_guard<std::mutex> lock(m_SendMutex);
                    if (m_SendQueue.empty() && m_BulkQueue.empty() && !m_Sending) { break; }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            Close();
        }

        void Close() {
            if (!m_Started) { return; }
            m_Closing = true;
            m_Sock.Shutdown();
            m_SendCv.notify_all();
            if (m_RecvThread.joinable()) { m_RecvThread.join(); }
            if (m_SendThread.joinable()) { m_SendThread.join(); }
            m_Sock.Close();
            m_Started = false;
            m_Alive = false;
            m_Secured = false;
            {
                std::lock_guard<std::mutex> lock(m_SendMutex);
                m_SendQueue.clear();
                m_BulkQueue.clear();
                m_BulkBytes = 0;
            }
            {
                std::lock_guard<std::mutex> lock(m_RecvMutex);
                m_RecvQueue.clear();
            }
        }

    private:
        // ---- 鍵交換 ----
        void PushReject(const std::string& reason) {
            std::lock_guard<std::mutex> lock(m_RecvMutex);
            m_RecvQueue.push_back({ Msg::Reject, Encode(Text{ reason }) });
        }
        static crypto::Key32 RandomKey() {
            crypto::Key32 key{};
            crypto::RandomBytes(key.data(), key.size());
            return key;
        }
        bool DeriveKeys(const crypto::Key32& priv, const crypto::Key32& peerpub,
            const crypto::Key32& clientnonce, const crypto::Key32& servernonce,
            const crypto::Key32& clientpub, const crypto::Key32& serverpub) {

            crypto::Key32 shared = crypto::X25519(priv, peerpub);
            uint8_t zero = 0;
            for (uint8_t b : shared) { zero |= b; }
            if (zero == 0) {
                return false;   // 低位数の点 (不正な公開鍵)
            }
            // 鍵交換の内容すべてを salt に含め、途中で書き換えられていれば鍵が一致しないようにする
            crypto::Sha256 transcript;
            transcript.Update(std::string_view("FAS3"));
            transcript.Update(clientnonce);
            transcript.Update(servernonce);
            transcript.Update(clientpub);
            transcript.Update(serverpub);
            const uint8_t flags = 0;
            transcript.Update(&flags, 1);
            const auto salt = transcript.Final();

            std::vector<uint8_t> ikm(shared.begin(), shared.end());
            static constexpr std::string_view info = "FreedomApopicStyle session v3";
            auto okm = crypto::Hkdf(salt, ikm, std::span(reinterpret_cast<const uint8_t*>(info.data()), info.size()), 64);

            crypto::Key32 c2s{}, s2c{};
            std::copy(okm.begin(), okm.begin() + 32, c2s.begin());
            std::copy(okm.begin() + 32, okm.end(), s2c.begin());
            if (m_Config.IsServer) { m_Sock.SetKeys(s2c, c2s); }
            else { m_Sock.SetKeys(c2s, s2c); }

            crypto::SecureZero(shared.data(), shared.size());
            crypto::SecureZero(ikm);
            crypto::SecureZero(okm);
            crypto::SecureZero(c2s.data(), c2s.size());
            crypto::SecureZero(s2c.data(), s2c.size());
            return true;
        }
        bool Handshake() {
            crypto::Key32 priv = RandomKey();
            const crypto::Key32 pub = crypto::X25519Base(priv);
            const crypto::Key32 nonce = RandomKey();
            bool ok = false;

            if (!m_Config.IsServer) {
                Writer w;
                w.Put(ProtocolVersion).Put<uint8_t>(0);     // Flags (予約)
                w.Buffer.insert(w.Buffer.end(), nonce.begin(), nonce.end());
                w.Buffer.insert(w.Buffer.end(), pub.begin(), pub.end());
                HandshakeType type{};
                std::vector<uint8_t> reply;
                if (m_Sock.SendPlain(HandshakeType::ClientHello, w.Buffer) && m_Sock.RecvPlain(type, reply)) {
                    if (type == HandshakeType::Reject) {
                        Text reason;
                        Decode(reply, reason);
                        PushReject(reason.Value);
                    }
                    else if (type == HandshakeType::ServerHello && reply.size() == 64) {
                        crypto::Key32 servernonce{}, serverpub{};
                        std::copy(reply.begin(), reply.begin() + 32, servernonce.begin());
                        std::copy(reply.begin() + 32, reply.end(), serverpub.begin());
                        ok = DeriveKeys(priv, serverpub, nonce, servernonce, pub, serverpub);
                    }
                }
            }
            else {
                HandshakeType type{};
                std::vector<uint8_t> hello;
                if (m_Sock.RecvPlain(type, hello) && type == HandshakeType::ClientHello && hello.size() == 4 + 1 + 64) {
                    const uint32_t version = crypto::LoadLE32(hello.data());
                    auto reject = [&](const std::string& reason) {
                        m_Sock.SendPlain(HandshakeType::Reject, Encode(Text{ reason }));
                    };
                    if (version != ProtocolVersion) {
                        reject("バージョンが一致しません (サーバー: " + std::to_string(ProtocolVersion) + ", クライアント: " + std::to_string(version) + ")");
                    }
                    else {
                        crypto::Key32 clientnonce{}, clientpub{};
                        std::copy(hello.begin() + 5, hello.begin() + 37, clientnonce.begin());
                        std::copy(hello.begin() + 37, hello.end(), clientpub.begin());
                        std::vector<uint8_t> reply(nonce.begin(), nonce.end());
                        reply.insert(reply.end(), pub.begin(), pub.end());
                        ok = m_Sock.SendPlain(HandshakeType::ServerHello, reply)
                            && DeriveKeys(priv, clientpub, clientnonce, nonce, clientpub, pub);
                    }
                }
            }
            crypto::SecureZero(priv.data(), priv.size());
            return ok;
        }

        void RecvLoop() {
            const bool secured = Handshake();
            {
                std::lock_guard<std::mutex> lock(m_SendMutex);
                m_Secured = secured;
                m_HandshakeDone = true;
            }
            m_SendCv.notify_all();

            while (secured && !m_Closing) {
                Message msg;
                if (!m_Sock.RecvFrame(msg.Type, msg.Payload)) {
                    break;
                }
                std::lock_guard<std::mutex> lock(m_RecvMutex);
                m_RecvQueue.push_back(std::move(msg));
            }
            m_Alive = false;
            m_SendCv.notify_all();
        }
        void SendLoop() {
            while (true) {
                Message msg;
                bool bulk = false;
                {
                    std::unique_lock<std::mutex> lock(m_SendMutex);
                    m_SendCv.wait(lock, [&] {
                        return m_Closing || !m_Alive || (m_HandshakeDone && (!m_Secured || !m_SendQueue.empty() || !m_BulkQueue.empty()));
                    });
                    if (m_Closing || !m_Alive || !m_Secured) { break; }
                    if (!m_SendQueue.empty()) {
                        msg = std::move(m_SendQueue.front());
                        m_SendQueue.pop_front();
                    }
                    else {
                        msg = std::move(m_BulkQueue.front());
                        m_BulkQueue.pop_front();
                        bulk = true;
                    }
                    m_Sending = true;
                }
                const size_t bytes = msg.Payload.size();
                const bool ok = m_Sock.SendFrame(msg.Type, msg.Payload);
                crypto::SecureZero(msg.Payload);
                {
                    std::lock_guard<std::mutex> lock(m_SendMutex);
                    if (bulk) { m_BulkBytes -= std::min(m_BulkBytes, bytes); }
                    m_Sending = false;
                }
                if (!ok) {
                    m_Alive = false;
                    break;
                }
            }
        }

        NetSocket m_Sock;
        SessionConfig m_Config;
        std::thread m_RecvThread;
        std::thread m_SendThread;
        std::atomic_bool m_Closing = false;
        std::atomic_bool m_Alive = false;
        std::atomic_bool m_Secured = false;
        bool m_Started = false;

        std::mutex m_SendMutex;
        std::condition_variable m_SendCv;
        std::deque<Message> m_SendQueue;
        std::deque<Message> m_BulkQueue;
        size_t m_BulkBytes = 0;
        bool m_Sending = false;
        bool m_HandshakeDone = false;

        std::mutex m_RecvMutex;
        std::vector<Message> m_RecvQueue;
    };

}
