#include "../Library/NumericCast.hpp"
#include "../Library/EditorLayout.hpp"
#include "../Library/KeyMapLayout.hpp"
#include "../Library/PlayerCharAnimation.hpp"
#include "../Library/PreviewRoll.hpp"
#include "../Library/OsuTaiko.hpp"
#include "../Library/Easing.hpp"
#include "../Library/TimeStretch.hpp"
#include "../Library/RecordingAudio.hpp"
#include "../Library/ChartCatalog.hpp"
#include "../Library/SkinCatalog.hpp"
#include "../Library/EditorForms.hpp"
#include "../Library/Mp3Timing.hpp"
#include <fstream>
#include <chrono>
#include <iostream>
#include <limits>

static void Check(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
#include "TempoTests.hpp"
#include "AudioAlignmentTests.hpp"
#include "EditorTransformTests.hpp"
static void Mp3TimingTests() {
    std::vector<uint8_t> data(200);
    data[0] = 255; data[1] = 251; data[2] = 144; data[3] = 192;
    std::memcpy(data.data() + 21, "Xing", 4);
    data[28] = 1; data[32] = 78;
    std::memcpy(data.data() + 33, "LAME3.100", 9);
    data[54] = 0x24; data[55] = 0x04; data[56] = 0x38;
    const auto info = mp3timing::Parse(data.data(), data.size());
    Check(info.Rate == 44100 && info.Delay == 576 && info.Padding == 1080, "MP3 gapless tag");
    Check(info.Head(89328, 44100) == 576, "untrimmed Windows MP3 delay");
    Check(info.Head(88200, 44100) == 0, "gapless decoder not double corrected");
    Check(info.Head(89328, 48000) == 0 && info.Head(89856, 44100) == 0, "unknown decoder untouched");
    std::memcpy(data.data() + 21, "Info", 4);
    Check(mp3timing::Parse(data.data(), data.size()).Head(90480, 44100) == 1728, "CBR Info frame skipped");
    for (size_t size = 0; size < 57; ++size) Check(mp3timing::Parse(data.data(), size).Rate == 0, "truncated MP3 tag");
    data[21] = 0;
    Check(mp3timing::Parse(data.data(), data.size()).Rate == 0, "untagged audio untouched");
}
static tja::Document RoundTrip(const tja::Document& doc) {
    const std::string text = doc.Write();
    const auto lines = osutaiko::Split(text, '\n');
    return tja::Document::Parse(lines);
}
static void DocumentTests() {
    auto doc = tja::Document::NewChart("test", "song.mp3", 2);
    auto& m = doc.Courses[0].Measures[0];
    tja::Document::SetNote(m, 1, 3, '1');
    tja::Document::SetNote(m, 2, 8, '4');
    tja::Document::AddCommand(m, 1, 2, "#GOGOSTART");
    tja::Document::AddCommand(m, 1, 1, "#GOGOEND");
    auto roundtrip = RoundTrip(doc);
    const auto& restored = roundtrip.Courses[0].Measures[0];
    Check(tja::Document::NoteAt(restored, 1, 3)->Type == '1', "triplet round trip");
    Check(tja::Document::NoteAt(restored, 1, 4)->Type == '4', "mixed grid round trip");
    Check(restored.Commands.size() == 2 && restored.Commands[1].Num == restored.Commands[1].Den, "end command round trip");
    tja::Document::SetNote(m, 1, 3, '0');
    Check(!tja::Document::NoteAt(m, 1, 3), "touch erase");
    m.Notes = { { 1, 1048573, '1' }, { 1, 1048571, '2' } };
    bool rejected = false;
    try { doc.Write(); } catch (const std::length_error&) { rejected = true; }
    Check(rejected, "unbounded LCM rejected before allocation");
    m.Notes = { { 1, 0, '1' } };
    rejected = false;
    try { doc.Write(); } catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected, "invalid denominator rejected");
}
static std::vector<std::pair<char, double>> NoteTimes(const tja::Document& doc) {
    double bpm = std::stod(tja::Document::GetValue(doc.Header, "BPM"));
    double time = -1000 * std::stod(tja::Document::GetValue(doc.Header, "OFFSET")), ratio = 1;
    std::vector<std::pair<char, double>> notes;
    for (const auto& measure : doc.Courses[0].Measures) {
        for (const auto& command : measure.Commands) {
            if (command.Num) { continue; }
            if (command.Text.starts_with("#BPMCHANGE ")) { bpm = std::stod(command.Text.substr(11)); }
            if (command.Text.starts_with("#MEASURE ")) { ratio = std::stod(command.Text.substr(9)); }
        }
        const double duration = 240000 / bpm * ratio;
        for (const auto& note : measure.Notes) { notes.push_back({ note.Type, time + duration * note.Num / note.Den }); }
        time += duration;
    }
    return notes;
}
static void OsuTests() {
    const std::string source = R"(osu file format v14
[General]
AudioFilename: song.ogg
Mode: 1
PreviewTime: 1500
[Metadata]
Title: Test
Version: Oni
[Difficulty]
SliderMultiplier: 1.4
OverallDifficulty: 5
[TimingPoints]
1000,500,4,0,0,100,1,0
1500,-50,4,0,0,100,0,1
2000,250,3,0,0,100,1,0
[HitObjects]
256,192,1250,1,0
256,192,1500,1,6
256,192,2000,2,0,L|300:192,1,140
256,192,2500,8,0,3000
)";
    const auto imported = osutaiko::Parse(source);
    Check(imported.has_value(), "Mode 1 accepted");
    const auto doc = RoundTrip(*imported);
    const auto times = NoteTimes(doc);
    const std::vector<std::pair<char, double>> expected{ { '1',1250 }, { '4',1500 }, { '5',2000 }, { '8',2250 }, { '9',2500 }, { '8',3000 } };
    Check(times.size() == expected.size(), "all taiko objects imported");
    for (size_t i = 0; i < times.size(); ++i) {
        Check(times[i].first == expected[i].first && std::abs(times[i].second - expected[i].second) < 0.2, "BPM, OFFSET and slider time preserved");
    }
    Check(tja::Document::GetValue(doc.Header, "WAVE") == "song.ogg", "relative audio path");
    Check(tja::Document::GetValue(doc.Header, "DEMOSTART") == "1.5", "preview milliseconds");
    const auto metadata = osutaiko::Metadata(source);
    Check(metadata && metadata->size() < 500 && metadata->find("111") == std::string::npos, "selection metadata avoids expanded note rows");
    std::string legacySource = source;
    legacySource.replace(legacySource.find("v14"), 3, "v4");
    const auto legacy = osutaiko::Parse(legacySource);
    Check(legacy.has_value(), "legacy osu version accepted");
    const auto legacyTimes = NoteTimes(*legacy);
    Check(std::abs(legacyTimes[0].second - 1274) < 0.2, "legacy osu v4 24ms correction");
    Check(tja::Document::GetValue(legacy->Header, "FASOSUFORMAT") == "4", "imported charts carry native timing marker");
    legacySource.replace(legacySource.find("PreviewTime: 1500"), 17, "PreviewTime: -1");
    Check(tja::Document::GetValue(osutaiko::Parse(legacySource)->Header, "DEMOSTART") == "0", "missing preview sentinel stays zero");
    Check(osutaiko::AudioFilename("\"folder\\song.mp3\"") == "folder/song.mp3", "osu audio path normalization");
    std::string standard = source;
    standard.replace(standard.find("Mode: 1"), 7, "Mode: 0");
    Check(!osutaiko::Parse(standard), "other modes rejected");
    std::string invalid = source;
    invalid.replace(invalid.find("1000,500"), 8, "1000,nan");
    Check(!osutaiko::Parse(invalid), "nonfinite timing rejected");
    Check(!osutaiko::Parse("osu file format v14\n[General]\nMode:1\n[TimingPoints]\n0,\n[HitObjects]\ninvalid"), "malformed input rejected");
}
int main() {
    Mp3TimingTests();
    AudioAlignmentTests();
    try {
        {
            playerchar::Clock clock;clock.Update(0,120,0);clock.Update(500,120,9);
            Check(clock.Beats==1&&!std::isfinite(clock.JumpAt),"character follows BPM before milestone");
            clock.Update(500,120,10);Check(clock.JumpAt==1,"character jumps at ten combo");
            clock.Update(750,240,11);Check(clock.Beats==2&&clock.JumpAt==1,"BPM change keeps animation phase and does not retrigger jump");
            clock.Update(750,240,20);Check(clock.JumpAt==1,"milestones during a jump do not restart it");
            clock.Update(1250,120,30);Check(clock.JumpAt==3,"milestone after completion starts a new jump");
            clock.Update(1250,120,40,2,false);Check(!std::isfinite(clock.JumpAt),"gogo cancels jumping");
            clock.Update(1250,120,41);Check(!std::isfinite(clock.JumpAt),"leaving gogo does not replay a queued jump");
            clock.Update(0,120,0);Check(clock.Beats==0&&!std::isfinite(clock.JumpAt),"rewind resets character animation");
            clock.Update(std::numeric_limits<double>::max(),std::numeric_limits<double>::max(),0);
            Check(std::isfinite(clock.Beats)&&!std::isfinite(clock.JumpAt),"overflow resets character clock safely");
        }
        Check(numeric::Cast<uint64_t>(-1.0)==0,"negative time cannot wrap unsigned");
        Check(numeric::Cast<uint64_t>(std::numeric_limits<double>::infinity())==UINT64_MAX,"large time saturates");
        Check(numeric::Cast<int>(std::numeric_limits<double>::quiet_NaN())==0,"NaN conversion");
        Check(numeric::Cast<int>(-2.9)==-2,"fractional conversion truncates toward zero");
        Check(editlayout::PageOffset(25,100,2)==0 && editlayout::PageOffset(99,100,2)==0,"first half stays fixed");
        {
            const auto keys=keymapview::Layout({{65,66},{70},{74},{75,65,-1,256}},1280,112,44,128,640,660);
            Check(keys.size()==6&&keys[0].Code==65&&keys[1].Code==66&&keys[4].Code==75,"key map keeps keys in their assigned groups");
            Check(keys[0].X==keys[1].X&&keys[0].Y<keys[1].Y&&keys[2].X>keys[1].X,"keys stack vertically within each lane");
            const auto wrapped=keymapview::Layout({{65,66,67,68,69,70,71,72}},360,112,44,128,180,660);
            Check(wrapped.size()==8&&wrapped[0].Y<wrapped.back().Y,"many key boxes wrap upwards");
            for(const auto& key:wrapped)Check(key.X>=56&&key.X<=304,"key map rows remain inside screen");
            Check(keymapview::Layout({},1280,112,44,128,640,660).empty(),"empty key map draws no boxes");
        }
        Check(editlayout::PageOffset(100,100,2)==100 && editlayout::PageOffset(199,100,2)==100,"second half stays fixed");
        Check(editlayout::PageOffset(149,100,1.5)==100 && editlayout::PageOffset(200,100,2)==100,"partial page and end boundary");
        {
            double next=0;int hits=0,heard=0,bursts=0;
            for(int now=0;now<=1000;now+=16) previewroll::Advance(next,hits,0,false,now,1000,1000.0/30,[&]{++heard;},[&]{++bursts;});
            Check(hits==30&&heard==30&&bursts==0,"roll plays every scheduled hit across frames");
            next=0;hits=heard=bursts=0;
            previewroll::Advance(next,hits,7,true,1000,1000,1000.0/30,[&]{++heard;},[&]{++bursts;});
            previewroll::Advance(next,hits,7,true,2000,1000,1000.0/30,[&]{++heard;},[&]{++bursts;});
            Check(hits==7&&heard==7&&bursts==1,"balloon and kusudama stop at target and burst once");
            next=0;hits=heard=bursts=0;
            previewroll::Advance(next,hits,100,true,100,100,1000.0/30,[&]{++heard;},[&]{++bursts;});
            Check(heard==3&&bursts==0,"unfinished balloon does not burst");
        }
        DocumentTests(); OsuTests(); EditorTransformTests();
        {
            skincatalog::Watch watch;
            catalog::Snapshot original, first, final, partial;
            original.Entries["skinconfig.json"]={10,1};
            first=original; first.Entries["skinconfig.json"]={11,2};
            final=first; final.Entries["Image/note.png"]={30,3};
            Check(!watch.Accept(original),"skin watcher initializes");
            Check(!watch.Accept(first)&&!watch.Accept(final),"skin watcher waits through changing writes");
            partial=final; partial.Complete=false;
            Check(!watch.Accept(partial)&&!watch.Accept(final),"partial scans do not confirm a skin update");
            Check(watch.Accept(final),"stable skin assets update accepted");
            Check(!watch.Accept(final),"unchanged skin not reloaded");
            catalog::Snapshot removed;
            Check(!watch.Accept(removed)&&watch.Accept(removed),"stable skin deletion accepted");
        }
        TempoTests();
        Check(rec::VolumeGain(100, 25) == 0.25f && rec::VolumeGain(50, 50) == 0.25f, "recording gain follows percentages");
        Check(rec::VolumeGain(100, 0) == 0 && rec::VolumeGain(200, 200) == 1, "recording mute and gain bounds");
        std::vector<float> quiet{ 0.1f, -0.2f, 0 }, loud{ 2.0f, -1.0f, std::numeric_limits<float>::infinity() };
        rec::LimitMixPeak(quiet); rec::LimitMixPeak(loud);
        Check(quiet[0] == 0.1f && quiet[1] == -0.2f, "recording never boosts quiet audio");
        Check(std::abs(loud[0] - 0.98f) < 1e-6 && std::abs(loud[1] + 0.49f) < 1e-6 && loud[2] == 0, "recording limits peaks without changing balance");
        auto measureForm = editform::SplitCommand("#MEASURE 7/8");
        Check(measureForm.Fields.size() == 2 && measureForm.Join({ "5", "4" }) == "#MEASURE 5/4", "measure values have separate fields");
        auto branchForm = editform::SplitCommand("#BRANCHSTART p,80,90");
        Check(branchForm.Fields.size() == 3 && branchForm.Join({ "r", "10", "20" }) == "#BRANCHSTART r,10,20", "branch values have separate fields");
        Check(catalog::IsArchive("Map.OSZ") && catalog::IsArchive("map.zip") && !catalog::IsArchive("map.osu"), "osz archive extension");
        namespace fs = std::filesystem;
        const fs::path fixture = fs::absolute("build") / ("catalog-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(fixture);
        const std::vector<fs::path> roots{ fixture };
        const auto empty = catalog::Scan(roots);
        { std::ofstream file(fixture / "a.tja"); file << "TITLE:one\n"; }
        const auto added = catalog::Scan(roots);
        Check(empty.Complete && added.Complete && added.Entries.size() == 1 && empty.Entries != added.Entries, "catalog detects imported chart");
        { std::ofstream file(fixture / "a.tja"); file << "TITLE:changed title\n"; }
        const auto edited = catalog::Scan(roots);
        Check(edited.Entries != added.Entries && edited.Entries == catalog::Scan(roots).Entries, "catalog detects change and stable snapshot");
        fs::remove(fixture / "a.tja");
        Check(catalog::Scan(roots).Entries.empty(), "catalog detects removed chart");
        fs::remove(fixture);
        std::vector<float> pcm(8000);
        for (size_t i = 0; i < pcm.size(); ++i) { pcm[i] = (float)std::sin(i * 0.1); }
        Check(tstretch::Wsola(pcm, 1, 8000, std::numeric_limits<double>::quiet_NaN()).empty(), "nonfinite playback speed rejected");
        Check(tstretch::Wsola(pcm, 1, 8000, std::numeric_limits<double>::min()).empty(), "stretch allocation overflow rejected");
        std::atomic_bool cancel = true;
        Check(tstretch::Wsola(pcm, 1, 8000, 0.5, &cancel).empty(), "stretch cancellation");
        const auto stretched = tstretch::Wsola(pcm, 1, 8000, 0.5);
        Check(stretched.size() > pcm.size() && std::ranges::all_of(stretched, [](float f) { return std::isfinite(f); }), "valid stretch remains usable");
        for (int line = ease::Linear; line <= ease::Bounce; ++line) {
            for (int base = ease::In; base <= ease::OutIn; ++base) {
                Check(ease::GetEasingRate(0, (ease::Base)base, (ease::Line)line) == 0, "gradient start");
                Check(ease::GetEasingRate(1, (ease::Base)base, (ease::Line)line) == 1, "gradient end");
                for (int step = 1; step < 100; ++step) {
                    Check(std::isfinite(ease::GetEasingRate(step / 100.0, (ease::Base)base, (ease::Line)line)), "finite easing");
                }
            }
        }





        std::cout << "PASS: core tests\n";
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
