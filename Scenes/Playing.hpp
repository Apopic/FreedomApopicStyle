// Game member declarations. Outside the class, resolve the complete Game interface.
#if !defined(FAS_GAME_MEMBER_DECLARATIONS)
#include "../Game.hpp"
#else

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
        int BigNoteLane=-1;

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
        int MeasureIndex = -1;          // 小節の頭の位置なら、書かれた順の小節の番号 (分岐の小節も数える。譜面制作の小節の番号と同じ)
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

        libarrier::Timer MoveTimer;
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

        bool AutoUsed = false;      // 演奏中に一度でもオートプレイだった (記録を残さない)
        void Init(bool dan_init) {
            if (dan_init) {
                AutoUsed = false;
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
            DelayMaxEnd.clear();
            DelayPrefix.clear();
            PxPrefixCache.clear();
            JiroPxPrefixCache.clear();
            JiroDelayPrefixCache.clear();
            NoteScroll.clear();
            NoteScrollDirty = true;
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
            IsOsuChart = false;
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
        bool IsOsuChart = false; // osu! 由来の譜面では小節線を背面に描く

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
        std::vector<double> DelayMaxEnd;

        void BuildTiming(double initbpm, bool integerms) {
            if (integerms) {
                for (auto& e : BpmEvents) { e.Time = std::max(0.0, JiroTrunc(e.Time)); }
                for (auto& e : DelayEvents) { e.Time = std::max(0.0, JiroTrunc(e.Time)); e.Length = JiroTrunc(e.Length); }
            }
            BpmEvents.insert(BpmEvents.begin(), { -std::numeric_limits<double>::infinity(), initbpm });
            if (integerms) {
                // 次郎は読み込み後、停止区間内のBPM変更へ停止の頭を移し、その位置のBPMで距離を補正する。
                for (auto& delay:DelayEvents) {
                    for (const auto& bpm:BpmEvents) {
                        if (delay.Time<=bpm.Time && bpm.Time<=delay.Time+delay.Length) { delay.Time=bpm.Time; }
                        if (bpm.Time<=delay.Time) { delay.Bpm=bpm.Value; }
                    }
                }
            }
            for (auto& e : BpmEvents) { if (e.Value == 0.0) { e.Value = 1e-9; } }   // 0 除算を避ける
            PxPrefixCache.clear();
            JiroPxPrefixCache.clear();
            JiroDelayPrefixCache.clear();
            NoteScrollDirty = true;

            BpmIndex.Assign(BpmEvents.size(), [&](size_t i) { return BpmEvents[i].Time; });
            // SectionMeasures[k] = 区間 1..k-1 の距離の合計 (区間 0 は BPM: の値で、足すことはない)
            SectionMeasures.assign(BpmEvents.size(), 0);
            for (size_t i = 1; i < BpmEvents.size(); ++i) {
                const double span = i - 1 == 0 ? 0 : (BpmEvents[i].Time - BpmEvents[i - 1].Time) * BpmEvents[i - 1].Value / 240000.0;
                SectionMeasures[i] = SectionMeasures[i - 1] + span;
            }
            std::ranges::stable_sort(DelayEvents, {}, &DelayEvent::Time);
            DelayPrefix.assign(DelayEvents.size() + 1, 0);
            DelayMaxEnd.resize(DelayEvents.size());
            for (size_t i = 0; i < DelayEvents.size(); ++i) {
                DelayPrefix[i + 1] = DelayPrefix[i] + DelayEvents[i].Length * DelayEvents[i].Bpm / 240000.0;
                DelayMaxEnd[i] = std::max(i ? DelayMaxEnd[i - 1] : -std::numeric_limits<double>::infinity(), DelayEvents[i].Time + DelayEvents[i].Length);
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
            const auto it = std::ranges::lower_bound(DelayMaxEnd, now);
            if (it != DelayMaxEnd.end()) {
                const auto& d = DelayEvents[it - DelayMaxEnd.begin()];
                if (d.Time <= now) { return d.Time; }
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

        // ---- 音符の表示位置の前計算 ----
        // 位置 = (フレームごとに 1 回だけ求める今の値) + (音符ごとに 1 回だけ求めた値) の差し引きにして、
        // 毎フレーム・音符ごとの二分探索 (BPM 区間・#DELAY・#SCROLL) をなくす。結果は ScrollDistance / ScrollPx と同じ
        struct NoteScrollCache {
            size_t Section = 0;         // SectionAt(音符の時刻)
            double Tail = 0;            // 区間の頭からの距離 + SectionMeasures[区間] (小節単位)
            size_t DelayHi = 0;         // upper_bound(DelayEvents, 音符の時刻)
            double JiroBpm = 0;         // JiroBpmAt(音符の時刻)
            double JiroScroll[3]{};     // JiroScrollAt(音符の時刻, 分岐)
        };
        std::vector<NoteScrollCache> NoteScroll;
        bool NoteScrollDirty = true;
        const NoteScrollCache& NoteScrollOf(const NoteData& n) {
            if (NoteScrollDirty || NoteScroll.size() != RawNoteDatas.size()) {
                NoteScrollDirty = false;
                NoteScroll.resize(RawNoteDatas.size());
                for (size_t i = 0; i < RawNoteDatas.size(); ++i) {
                    const double t = RawNoteDatas[i].AbsTime;
                    auto& c = NoteScroll[i];
                    c.Section = BpmEvents.empty() ? 0 : SectionAt(t);
                    c.Tail = BpmEvents.empty() || c.Section == 0 ? 0.0 : SectionMeasures[c.Section] + (t - BpmEvents[c.Section].Time) * BpmEvents[c.Section].Value / 240000.0;
                    c.DelayHi = (size_t)(std::ranges::upper_bound(DelayEvents, t, {}, &DelayEvent::Time) - DelayEvents.begin());
                    c.JiroBpm = JiroBpmAt(t);
                    for (int lane = 0; lane < 3; ++lane) { c.JiroScroll[lane] = JiroScrollAt(t, JiroBranchOf(lane)); }
                }
            }
            const size_t i = (size_t)(&n - RawNoteDatas.data());
            static const NoteScrollCache none{};
            return i < NoteScroll.size() ? NoteScroll[i] : none;
        }
        struct ScrollFrame {
            double Now = 0;
            size_t Cur = 0, Next = 0;
            double Head = 0;            // (次の区間の頭 - 今) × 今の BPM - SectionMeasures[次の区間]
            size_t DelayLo = 0;         // lower_bound(DelayEvents, 今)
        };
        ScrollFrame MakeScrollFrame(double now) const {
            ScrollFrame f;
            f.Now = now;
            if (!BpmEvents.empty()) {
                f.Cur = SectionAt(now);
                f.Next = f.Cur + 1;
                if (f.Next < BpmEvents.size()) { f.Head = (BpmEvents[f.Next].Time - now) * BpmEvents[f.Cur].Value / 240000.0 - SectionMeasures[f.Next]; }
            }
            f.DelayLo = (size_t)(std::ranges::lower_bound(DelayEvents, now, {}, &DelayEvent::Time) - DelayEvents.begin());
            return f;
        }
        // ScrollDistance と同じ値を前計算から求める
        double ScrollDistanceFast(const NoteData& n, const ScrollFrame& f) {
            const double t = n.AbsTime, now = f.Now;
            if (BpmEvents.empty()) { return (t - now) * NowBPM / 240000.0; }
            const auto& c = NoteScrollOf(n);
            double dist;
            if (c.Section == 0 || f.Next >= BpmEvents.size() || t < BpmEvents[f.Next].Time) {
                dist = (t - now) * BpmEvents[c.Section].Value / 240000.0;
            }
            else {
                dist = f.Head + c.Tail;
                if (c.Section < f.Next) { dist += SectionMeasures[f.Next] - SectionMeasures[c.Section]; }   // 書かれた順で前の区間 (足す区間は無い)
            }
            if (t >= now && c.DelayHi > f.DelayLo) { dist -= DelayPrefix[c.DelayHi] - DelayPrefix[f.DelayLo]; }
            return dist;
        }
        // 区間の整数和を前計算し、動いている区間の端数だけ補間する。
        double ScrollPxFast(const NoteData& n, const ScrollFrame& f, double unit) {
            const double t = n.AbsTime, now = f.Now;
            if (BpmEvents.empty()) { return std::trunc(ScrollDistance(t, now) * unit); }
            const auto& c = NoteScrollOf(n);
            auto px = [&](double dt, double bpm) { return std::trunc(dt * bpm / 240000.0 * unit); };
            double x;
            if (c.Section == 0 || f.Next >= BpmEvents.size() || t < BpmEvents[f.Next].Time) {
                x = px(t - now, BpmEvents[c.Section].Value);
            }
            else {
                x = px(BpmEvents[f.Next].Time - now, BpmEvents[f.Cur].Value);
                if (c.Section > f.Next) { const auto& p = PxPrefix(unit); x += p[c.Section] - p[f.Next]; }
                x += px(t - BpmEvents[c.Section].Time, BpmEvents[c.Section].Value);
            }
            if (t >= now) { for (size_t k = f.DelayLo; k < c.DelayHi; ++k) { x -= px(DelayEvents[k].Length, DelayEvents[k].Bpm); } }
            const bool direct = c.Section == 0 || f.Next >= BpmEvents.size() || t < BpmEvents[f.Next].Time;
            const double moving = direct ? (t - now) * BpmEvents[c.Section].Value / 240000.0 * unit :
                (BpmEvents[f.Next].Time - now) * BpmEvents[f.Cur].Value / 240000.0 * unit;
            return x + (std::isfinite(moving) ? moving - std::trunc(moving) : 0.0);
        }

        // ---- 次郎と同じ整数の位置 ----
        // 次郎は位置を 32 bit の整数で計算する: 区間ごとに (int)(時間 / (60/BPM*4*1000) * 1 小節の px * SCROLL) を求め、
        // 範囲外の値は最小値 (0x80000000) になり、足し算は 32 bit で回り込む。極端な SCROLL や長い小節で
        // 位置があふれて音符が別の場所に回り込んで見えるギミックも、この計算で同じに見える
        static constexpr double JiroMeasurePx = 420.0;     // 次郎の 1 小節の px (Notedist=2)
        static int32_t JiroTimeToPx(double dt, double bpm, double unit, double scroll) {
            const double v = dt / (60.0 / bpm * 4.0 * 1000.0) * unit * scroll;
            if (!(v > -2147483649.0 && v < 2147483648.0)) { return std::numeric_limits<int32_t>::min(); }
            const double t = JiroTrunc(v);
            if (!(t >= -2147483648.0 && t < 2147483648.0)) { return std::numeric_limits<int32_t>::min(); }
            return (int32_t)t;
        }
        static double JiroIntTime(double t) {
            if (!std::isfinite(t)) { return t < 0 ? -2147483648.0 : 2147483647.0; }
            return std::floor(t);
        }
        mutable std::map<std::pair<double, double>, std::vector<uint32_t>> JiroPxPrefixCache;
        const std::vector<uint32_t>& JiroPxPrefix(double unit, double scroll) const {
            const auto key = std::make_pair(unit, scroll);
            if (auto it = JiroPxPrefixCache.find(key); it != JiroPxPrefixCache.end()) { return it->second; }
            std::vector<uint32_t> p(BpmEvents.size() + 1, 0);
            for (size_t m = 0; m < BpmEvents.size(); ++m) {
                uint32_t span = 0;
                if (m >= 1 && m + 1 < BpmEvents.size()) { span = (uint32_t)JiroTimeToPx(JiroIntTime(BpmEvents[m + 1].Time) - JiroIntTime(BpmEvents[m].Time), BpmEvents[m].Value, unit, scroll); }
                p[m + 1] = p[m] + span;
            }
            return JiroPxPrefixCache.emplace(key, std::move(p)).first->second;
        }
        // 正の #DELAY で寄せる距離の累積 (32 bit で回り込む)。音符ごとに #DELAY を 1 つずつ引くと 音符数 × #DELAY 数 になるので前計算する
        mutable std::map<std::pair<double, double>, std::vector<uint32_t>> JiroDelayPrefixCache;
        const std::vector<uint32_t>& JiroDelayPrefix(double unit, double scroll) const {
            const auto key = std::make_pair(unit, scroll);
            if (auto it = JiroDelayPrefixCache.find(key); it != JiroDelayPrefixCache.end()) { return it->second; }
            std::vector<uint32_t> p(DelayEvents.size() + 1, 0);
            for (size_t k = 0; k < DelayEvents.size(); ++k) {
                p[k + 1] = p[k] + (DelayEvents[k].Length > 0 ? (uint32_t)JiroTimeToPx(DelayEvents[k].Length, DelayEvents[k].Bpm, unit, scroll) : 0u);
            }
            return JiroDelayPrefixCache.emplace(key, std::move(p)).first->second;
        }
        // 音符の位置 (判定の位置からの px)。hb: #HBSCROLL (区間を足す・#DELAY で寄せる)、false なら通常のスクロール
        double JiroScrollPx(const NoteData& n, const ScrollFrame& f, double unit, double scroll, bool hb) {
            const double t = JiroIntTime(n.AbsTime), now = JiroIntTime(f.Now);
            if (BpmEvents.empty()) { return JiroTimeToPx(t - now, NowBPM, unit, scroll); }
            const auto& c = NoteScrollOf(n);
            const size_t next = f.Next;
            uint32_t x;
            if (!hb || c.Section == 0 || next >= BpmEvents.size() || t < JiroIntTime(BpmEvents[next].Time)) {
                x = (uint32_t)JiroTimeToPx(t - now, BpmEvents[c.Section].Value, unit, scroll);
            }
            else {
                x = (uint32_t)JiroTimeToPx(JiroIntTime(BpmEvents[next].Time) - now, BpmEvents[f.Cur].Value, unit, scroll);
                if (c.Section > next) {
                    // キャッシュ全消去による毎フレームの再構築を避ける。32 MiB を超える分は必要区間だけ計算する。
                    const auto key = std::make_pair(unit, scroll);
                    if (JiroPxPrefixCache.contains(key) ||
                        (JiroPxPrefixCache.size() + 1) * (BpmEvents.size() + 1) <= 8 * 1024 * 1024) {
                        const auto& p = JiroPxPrefix(unit, scroll); x += p[c.Section] - p[next];
                    }
                    else {
                        for (size_t k = next; k < c.Section; ++k) {
                            x += (uint32_t)JiroTimeToPx(JiroIntTime(BpmEvents[k + 1].Time) - JiroIntTime(BpmEvents[k].Time), BpmEvents[k].Value, unit, scroll);
                        }
                    }
                }
                x += (uint32_t)JiroTimeToPx(t - JiroIntTime(BpmEvents[c.Section].Time), BpmEvents[c.Section].Value, unit, scroll);
            }
            if (hb) {
                const size_t hi = std::min(c.DelayHi, DelayEvents.size());
                if (f.DelayLo < hi) {
                    const auto key = std::make_pair(unit, scroll);
                    if (JiroDelayPrefixCache.contains(key) ||
                        (JiroDelayPrefixCache.size() + 1) * (DelayEvents.size() + 1) <= 8 * 1024 * 1024) {
                        const auto& d = JiroDelayPrefix(unit, scroll); x -= d[hi] - d[f.DelayLo];
                    }
                    else {
                        for (size_t k = f.DelayLo; k < hi; ++k) {
                            if (DelayEvents[k].Length > 0) { x -= (uint32_t)JiroTimeToPx(DelayEvents[k].Length, DelayEvents[k].Bpm, unit, scroll); }
                        }
                    }
                }
            }
            // 整数の区間和・32 bit の回り込みは残し、動いている区間だけを補間する。
            const bool direct = !hb || c.Section == 0 || next >= BpmEvents.size() || t < JiroIntTime(BpmEvents[next].Time);
            const double dt = direct ? t - f.Now : JiroIntTime(BpmEvents[next].Time) - f.Now;
            const double b = BpmEvents[direct ? c.Section : f.Cur].Value;
            const double continuous = dt / (60.0 / b * 4.0 * 1000.0) * unit * scroll;
            const double integerTime = (direct ? t - now : JiroIntTime(BpmEvents[next].Time) - now);
            const double quantized = integerTime / (60.0 / b * 4.0 * 1000.0) * unit * scroll;
            const double correction = continuous - JiroTrunc(quantized);
            // 高速な逆走では 1ms 未満でも 1px 以上動く。補間量ではなく整数変換の範囲で判定する。
            const bool valid = std::isfinite(continuous) && quantized > -2147483649.0 && quantized < 2147483648.0;
            return (double)(int32_t)x + (valid ? correction : 0.0);
        }

        uint64_t AddScore = 0;
        uint64_t AllNoteCount = 0;

        libarrier::Timer BranchAnimationTimer;
        double BranchAnimationTime = 0.2;
        BranchType NowBranchFlag = BranchType::Null;
        ABranchType NowBranchAnimation = ABranchType::Null;
        bool LevelHold = false;

        SoundData SongData;
        double SongBlankTime = 0;
        double SongSpeed = 1.0;

        MovieData Movie;

        libarrier::Timer NowTime;
        ScrollType ScrollType = ScrollType::Normal;
        double NowBPM = 0;
        bool NowGoGo = false;

        bool AutoPlayLR = false;
        libarrier::Timer WaitRollTime;

        libarrier::Timer RollViewEndTimer;
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
            NoteScrollDirty = true;
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

    libarrier::Timer MiniTaikoFlash[16];
    double MiniTaikoFlashTime = 160;

    int RefreshRate = 60;

    double ChartNowTime(uint64_t elapsed, bool FrameCounter = false, double fastdrawrate = 0, double extendrate = 1) const;


    struct _HitNote {
    private:

        static const size_t m_size = 64;

    public:




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
    const std::string& LaneTitle(size_t lane) const;

    struct TrainingData {

        libarrier::Timer BarlineMoveTimer;
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
        bool ClearTarget = false;
        double ClearOpacity = 0;
        std::chrono::steady_clock::time_point BackgroundAt{};
        uint64_t LastCombo = 0;
        bool Failed = false;
        SoundData NextSong;                     // 演奏中に先読みした次の曲
        size_t NextSongIndex = SIZE_MAX;

        void Reset() {
            NextSong.Delete();
            *this = DanState();
        }
    } Dan;

    static const char* ExamLabel(ExamTypes type);
    static std::string ExamValueText(ExamTypes type, double value);
    static std::string ExamConditionText(const ExamData& exam);

    DanCounters DanTotalCounters() const;
    DanCounters DanSongCounters() const;
    static double ExamCounterValue(ExamTypes type, const DanCounters& c);
    // 以上 (m): 金 >= PassVal[1], 赤 >= PassVal[0], 終了時に届いていなければ不合格
    // 未満 (l): PassVal[0] 以上になった時点で不合格、PassVal[1] 未満なら金
    static ExamStatus EvaluateExam(const ExamData& exam, double value, bool finished);

    // songfinished: 現在の曲が終わった (この曲の個別条件と、最終曲なら全体条件を確定する)
    void UpdateDanExams(bool songfinished);
    // 0: 不合格, 2: 合格, 3: 金合格 (王冠画像の番号)
    int DanCrown() const;

    // 段位選択画面: 選択中の段位の課題曲と条件を表示する
    void DrawDanInfo(const ChartData& chart) const;

    void SetDrawBranchArea(Pos2D<float> DelayPos) const;
    playerchar::Clock PlayerCharAnim;
    struct SkinVoiceState {
        uint64_t LastCombo = 0;
        bool FullComboPlayed = false;
        std::optional<std::chrono::steady_clock::time_point> FullComboAt;
    } SkinVoice;
    static bool HasImage(const GraphData& g);
    void SkinVoiceReset();
    // コンボ数に応じたボイスを再生する
    void UpdateFullComboVoice(std::chrono::steady_clock::time_point now);
    void SkinVoiceUpdate();
    float DrawKeyMap();
    void DrawPlayerChar(double now);
    // 画像の 1 コマを (pos からのずれで) 描く。番号が範囲外なら何もしない
    static void DrawFrame(const GraphData& g, Pos2D<float> pos, int index);
    // 角を伸ばさずに 9 分割で伸ばして描く (slice <= 0 なら普通に伸ばす)
    static void DrawSliced(const GraphData& g, Pos2D<float> pos, float slice, int index = 0);
    void PlayingInit();
    void PlayingEnd();
    void PlayingDraw();
    // 自分の打撃を他プレイヤーへ送る (判定とスコアも一緒に送り、受信側は再計算しない)
    void HitAction(HitType type);
    void ApplyRemoteHit(const fasnet::HitEvent& hit);
    std::chrono::steady_clock::time_point RemoteRollAt[4]{};
    // 次郎1 の分岐条件: 値 < 達人の条件 なら (値 < 玄人の条件 ? 普通 : 玄人)、そうでなければ達人
    //   精度 (p) と連打数 (r) は最後の #SECTION から、スコア (s) は曲の最初からの値で判定する
    // 分岐の条件から次の分岐を決める (値 < 達人の条件 なら (値 < 玄人の条件 ? 普通 : 玄人)、そうでなければ達人)
    BranchType BranchDecide(const BranchData& branchdata) const;
    static ABranchType BranchAnimationOf(BranchType from, BranchType to);
    // 分岐を始める前の片付け: 今の分岐の、分岐の始まりより前の音符は残し、他の分岐の音符は表示しない
    void BranchCleanup(double startms);
    // 次郎形式の分岐 (taikojiro の Game_EvaluateBranch と同じタイミング):
    //   ・次の分岐 (始まりがまだ来ていない最初の分岐) を、判定の時刻の 200ms 前から判定の時刻を過ぎるまで毎フレーム判定し直す
    //     (判定の時刻 = 1 つ前の小節の頭。連打数の分岐は分岐の頭)
    //   ・音符の分岐は、分岐の始まりの 50ms 前に切り替える
    //   ・分岐の表示は、決まった分岐へ 1 段ずつ 100ms かけて動く (普通→達人は玄人を通って 200ms)
    void JiroBranchUpdate(double now);
    // 次郎形式の判定の進み方 (taikojiro の Game_PassNotes と同じ):
    //   ・判定する音符は分岐ごとに書かれた順に 1 つずつ。今の音符を過ぎた (不可の幅を越えた、または次の音符が可の幅に入った) ら
    //     見逃し (不可) にして次へ進む (1 フレームに 1 つ)。書かれた順と時刻の順が違う音符 (ダミーノーツ) はここで見逃しになる
    //   ・見逃した音符は消えずに流れていく。他の分岐の音符は時刻を過ぎたら判定せずに終わらせる
    void JiroPassNotes(double now);
    // 次郎形式で今判定する音符 (打った音符は先に飛ばす)。無ければ nullptr
    NoteData* JiroCursorNote();

    void BranchChange(const BranchData& branchdata);
    void PlayingProc();
#endif // FAS_GAME_MEMBER_DECLARATIONS
