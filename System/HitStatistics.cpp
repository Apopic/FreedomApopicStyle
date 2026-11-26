#include "../Game.hpp"

std::string Game::FormatSigned(double v, int decimals) {
    const double scale = std::pow(10.0, decimals);
    const double rounded = std::round(v * scale) / scale;
    std::string str = FormatNumber(rounded == 0 ? 0.0 : rounded, decimals);
    return (rounded > 0 ? "+" : "") + str;
}

auto Game::SummarizeHitErrors(const std::vector<float>& errors, double limit) -> HitErrorSummary {
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

double Game::HitErrorLimit() const { return std::max(1.0, Config.JudgeOk); }

void Game::RecordHitError(double error) {
    if (Config.AutoPlay || !std::isfinite(error)) { return; }
    HitStats.Play.push_back((float)error);
    HitStats.RecentHits.push_back({ (float)error, std::chrono::steady_clock::now() });
    while (HitStats.RecentHits.size() > HitErrorStats::MaxRecent) { HitStats.RecentHits.pop_front(); }
}

void Game::BeginPlayStats() {
    // 段位の 2 曲目以降は 1 曲目から続けて集計する
    if (!(Dan.Active && Dan.SongIndex > 0)) {
        HitStats.Play.clear();
    }
    HitStats.RecentHits.clear();
}

void Game::FinishPlayStats() {
    HitStats.LastPlay = SummarizeHitErrors(HitStats.Play, HitErrorLimit());
}

void Game::DrawHitErrorMeter(float cx, float cy, float width, const HitErrorSummary* summary, bool recent, const FontData* font) {
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

std::string Game::HitErrorText(const HitErrorSummary& sum) {
    if (sum.Count == 0) { return "まだ記録がありません"; }
    const char* tendency = std::abs(sum.Mean) < 1.0 ? "" : sum.Mean > 0 ? " (遅め)" : " (早め)";
    return "平均 " + FormatSigned(sum.Mean, 1) + "ms" + tendency + "  ばらつき " + FormatNumber(sum.StdDev, 1) + "ms  (" + std::to_string(sum.Count) + " 打)";
}
