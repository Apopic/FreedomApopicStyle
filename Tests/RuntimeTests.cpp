#include "GameTestAccess.hpp"
#include <iostream>

static void Check(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
static void WindowTests() {
    const HWND window = CreateWindowExW(0, L"STATIC", L"FAS caption regression", WS_OVERLAPPEDWINDOW, 100, 100, 500, 300, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    Check(window != nullptr, "hidden caption test window");
    ApplyDarkWindowMode(window);
    RECT bounds{}; GetWindowRect(window, &bounds);
    auto pointFor = [&](LRESULT wanted) {
        for (long y = bounds.top; y < bounds.top + 60; ++y) {
            for (long x = bounds.left; x < bounds.right; ++x) { POINT point{ x,y }; if (CaptionHitTest(window, point) == wanted) { return point; } }
        }
        throw std::runtime_error("caption hit test point missing");
    };
    MSG message{};
    for (const auto& [hit, command] : std::vector<std::pair<int,int>>{ { HTCLOSE, SC_CLOSE }, { HTMINBUTTON, SC_MINIMIZE }, { HTMAXBUTTON, SC_MAXIMIZE } }) {
        const auto point = pointFor(hit);
        __windowdrag.Button = hit;
        WndProc(window, WM_MOUSEMOVE, 0, 0);
        Check(__windowdrag.Button == hit && !PeekMessage(&message, window, WM_SYSCOMMAND, WM_SYSCOMMAND, PM_REMOVE), "mouse move does not consume caption release");
        WndProc(window, WM_NCLBUTTONUP, hit, MAKELPARAM((short)point.x, (short)point.y));
        Check(PeekMessage(&message, window, WM_SYSCOMMAND, WM_SYSCOMMAND, PM_REMOVE) && message.wParam == command, "caption release posts correct command");
        WndProc(window, WM_NCLBUTTONUP, hit, MAKELPARAM((short)point.x, (short)point.y));
        Check(!PeekMessage(&message, window, WM_SYSCOMMAND, WM_SYSCOMMAND, PM_REMOVE), "caption release fires once");
    }
    __windowdrag.Button = HTCLOSE;
    ReleaseCaptionButton(window, { bounds.left + 100, bounds.top + 100 });
    Check(!PeekMessage(&message, window, WM_SYSCOMMAND, WM_SYSCOMMAND, PM_REMOVE), "release outside button cancels");
    __windowdrag.Button = HTCLOSE; WndProc(window, WM_CAPTURECHANGED, 0, 0);
    Check(__windowdrag.Button == 0, "lost capture cancels caption press");
    DestroyWindow(window);
}
int main() {
    try {
        WindowTests();
        Check(Game::RequiresJiroTiming({"#HBSCROLL","#BPMCHANGE -540","#MEASURE -4/16"}),"reverse HB gimmick selects Jiro timing");
        Check(Game::RequiresJiroTiming({"#HBSCROLL","#BPMCHANGE 135","#MEASURE 4/4"}),"all HB charts select Jiro timing");
        Check(Game::RequiresJiroTiming({"#BMSCROLL"}),"BM charts request Jiro timing");
        Check(Game::RequiresJiroTiming({"  #HBSCROLL // comment"}),"HB directive with comment");
        Check(!Game::RequiresJiroTiming({"// #HBSCROLL", "#BPMCHANGE -120"}),"negative BPM alone does not select HB timing");
        Game::PlayData chart;
        {
            Game::PlayData stops;
            stops.BpmEvents={{100,120},{200,-540}};
            stops.DelayEvents={{150,111,120}};
            stops.BuildTiming(120,true);
            Check(stops.DelayEvents[0].Time==200 && stops.DelayEvents[0].Bpm==-540,"Jiro stop snaps to BPM change and uses updated BPM");
        }
        chart.BpmEvents = { { 0, 120 }, { 1000, 240 }, { 2500, -120 }, { 1800, 60 } };
        chart.DelayEvents = { { 100, 900, 120 }, { 300, 10, 120 }, { 900, 300, 120 }, { 2000, 100, 240 } };
        chart.RawNoteDatas.resize(1);
        chart.RawNoteDatas[0].AbsTime = 3000;
        chart.BuildTiming(120, true);
        chart.BuildScrollLanes(true);
        for (double t = -10; t < 2300; t += 0.7) {
            double expected = t;
            for (const auto& delay : chart.DelayEvents) {
                if (delay.Time > t) { break; }
                if (t <= delay.Time + delay.Length) { expected = delay.Time; break; }
            }
            Check(chart.FrozenTime(t) == expected, "overlapping DELAY semantics");
        }
        for (int scroll = 1; scroll <= 100; ++scroll) { chart.JiroPxPrefix(420, scroll / 10.0); chart.JiroDelayPrefix(420, scroll / 10.0); }
        Check(chart.JiroPxPrefixCache.size() == 100 && chart.JiroDelayPrefixCache.size() == 100, "more than 64 scroll values remain cached");
        const auto* prefix = chart.JiroPxPrefix(420, 1).data();
        for (int i = 0; i < 10; ++i) { Check(chart.JiroPxPrefix(420, 1).data() == prefix, "cache reused across frames"); }

        Game::PlayData simple;
        simple.RawNoteDatas.resize(1); simple.RawNoteDatas[0].AbsTime = 2000;
        simple.BuildTiming(120, true); simple.BuildScrollLanes(true);
        for (double now = 100; now <= 120; now += 0.25) {
            const double x = simple.JiroScrollPx(simple.RawNoteDatas[0], simple.MakeScrollFrame(now), 420, 1, false);
            Check(std::abs(x - (2000 - now) * 0.21) < 1e-8, "smooth subpixel motion");
            const double bm = simple.ScrollPxFast(simple.RawNoteDatas[0], simple.MakeScrollFrame(now), 420);
            Check(std::abs(bm - (2000 - now) * 0.21) < 1e-8, "BMSCROLL smooth subpixel motion");
        }
        const double overflow = simple.JiroScrollPx(simple.RawNoteDatas[0], simple.MakeScrollFrame(0), 420, 1e12, false);
        Check(overflow == (double)std::numeric_limits<int32_t>::min(), "legacy overflow gimmick preserved");
        for (double scroll : { -16.0, -4.5, 4.5, 16.0 }) {
            for (double now=100; now<120; now+=0.125) {
                const double x=simple.JiroScrollPx(simple.RawNoteDatas[0],simple.MakeScrollFrame(now),420,scroll,false);
                Check(std::abs(x-(2000-now)*.21*scroll)<1e-8,"fast reverse keeps interpolation above one pixel");
            }
        }

        auto game = std::make_unique<Game>();
        auto& Config = GameTestAccess::Config(*game);

        game->Editor.Doc=tja::Document::NewChart("measure cache","",4);game->Editor.Course=0;game->Editor.Open=true;
        game->EditorMeasureRatio(0);game->Editor.CursorMeasure=0;game->Editor.CursorSlot=0;
        game->EditorInsertCommand("#MEASURE 2/1");
        Check(game->EditorMeasureRatio(0)==2,"MEASURE immediately invalidates cached lane length");
        game->Editor.Doc = tja::Document::NewChart("test", "", 10);
        game->Editor.Open = true;
        game->Editor.Folds = { { 1, 3 }, { 6, 8 } };
        game->EditorChange();
        game->Editor.Doc.Courses[0].Measures.insert(game->Editor.Doc.Courses[0].Measures.begin(), tja::Measure{});
        game->EditorRemapFolds(0, 0, 1);
        Check(game->Editor.Folds == std::vector<std::pair<int,int>>{ { 2, 4 }, { 7, 9 } }, "insert retains folded measures");
        game->EditorUndo(false);
        Check(game->Editor.Folds == std::vector<std::pair<int,int>>{ { 1, 3 }, { 6, 8 } }, "undo restores folds");
        game->Editor.CursorMeasure = 2;
        game->EditorClamp();
        Check(game->Editor.Folds.size() == 2, "cursor normalization does not expand folds");
        game->EditorToggleFold(2);
        Check(!game->EditorFoldAt(2) && game->EditorOpenFoldAt(2), "opening fold retains registration");
        game->EditorChange();
        game->Editor.Doc.Courses[0].Measures.insert(game->Editor.Doc.Courses[0].Measures.begin(), tja::Measure{});
        game->EditorRemapFolds(0, 0, 1);
        Check(game->EditorOpenFoldAt(3) && game->EditorOpenFoldAt(3)->first == 2, "open registration follows insert");
        game->EditorUndo(false);
        Check(game->EditorOpenFoldAt(2) && !game->EditorFoldAt(2), "undo restores open fold registration");
        game->EditorToggleFold(2);
        Check(game->EditorFoldAt(2) && !game->EditorOpenFoldAt(2), "registered fold can close again");
        game->EditorRemoveFoldRegistration(2);
        Check(!game->EditorFoldAt(2) && !game->EditorOpenFoldAt(2) && game->EditorMeasureCount() == 10, "fold deletion retains measures");
        game->EditorUndo(false);
        Check(game->EditorFoldAt(2), "fold deletion can be undone");
        game->EditorToggleFold(2); game->EditorRemoveFoldRegistration(2);
        Check(!game->EditorOpenFoldAt(2), "open fold registration removed");
        game->EditorUndo(false); game->EditorToggleFold(2);

        game->EditorChange();
        tja::Document::AddCommand(game->Editor.Doc.Courses[0].Measures[0], 0, 1, "#MEASURE 2/1");
        Check(game->EditorDivOf(0) == 32 && game->EditorDivOf(1) == 32, "long measure grid");
        const auto& timeline = game->EditorTimelineCached();
        Check(timeline.TimeAt(0, 0.5) == 2000, "long measure timeline");
        tja::Document::SetValue(game->Editor.Doc.Header, "OFFSET", "0.001");
        const double before = game->EditorSongPosition(2000);
        tja::Document::SetValue(game->Editor.Doc.Header, "OFFSET", "0.004");
        Check(std::abs(game->EditorSongPosition(2000) - before + 3) < 1e-9, "3ms OFFSET produces 3ms audio shift");
        game->Editor.Doc = tja::Document::NewChart("gradient", "", 4); ++game->Editor.Revision;
        for (int i = 0; i < 4; ++i) { tja::Document::SetNote(game->Editor.Doc.Courses[0].Measures[0], i, 4, '1'); }
        game->Editor.Selecting = true; game->Editor.SelMeasure = 0; game->Editor.SelNum = 0; game->Editor.SelDen = 16;
        game->Editor.CursorMeasure = 0; game->Editor.CursorSlot = 15;
        game->EditorInsertCommand("#SCROLL 1>9 quad in");
        const auto& commands = game->Editor.Doc.Courses[0].Measures[0].Commands;
        Check(commands.size() == 4 && commands[0].Text == "#SCROLL 1" && commands[3].Text == "#SCROLL 9", "gradient endpoints");
        Check(std::abs(std::stod(commands[1].Text.substr(8)) - 1.888889) < 1e-6, "gradient uses selected easing");
        game->Editor.Path = "build/test.tja"; game->Editor.CursorMeasure = 2;
        Config.TrainingMode = true; game->EditorTestPlay();
        Check(game->Editor.Testing && game->Editor.TestMeasure == 2 && !Config.TrainingMode, "test play locks starting measure");
        Config.TrainingMode = game->Editor.SavedTraining;
        const auto savedCustom = Config.EditorCustomGrids, savedGrids = Config.EditorGrids;
        game->EditorAddGrid(7);
        Check(game->Editor.Division == 7 && std::ranges::binary_search(game->EditorGridChoices(), 7), "custom grid accepted");
        Check(std::ranges::binary_search(game->EditorGridList(), 7), "custom grid in favorites");
        game->EditorAddGrid(0); Check(game->Editor.Division == 7, "invalid custom grid rejected");
        game->EditorRemoveGrid(16);
        Check(std::ranges::binary_search(game->EditorGridChoices(), 16) && game->Editor.Division == 7, "default grid cannot be removed");
        game->Editor.CursorSlot = 3;
        game->EditorRemoveGrid(7);
        Check(!std::ranges::binary_search(game->EditorGridChoices(), 7) && game->Editor.Division == 16 && game->Editor.CursorSlot == 7, "custom grid deletion restores default and remaps cursor");
        Check(!std::ranges::binary_search(game->EditorGridList(), 7), "deleted grid removed from favorites");
        Check(!game->EditorButtonHelp("音源長→小節数").empty(), "audio length button has description");
        for (int i = 1000; i < 1064; ++i) { Config.EditorCustomGrids.push_back(i); }
        game->Editor.GridPage = 3;
        const auto page = game->EditorGridPanelChoices();
        Check(!page.empty() && page.size() <= Game::EditorGridsPerPage && page.back() == 1063, "many custom grids remain reachable through pages");
        game->Editor.GridPage = 0;
        Config.EditorCustomGrids = savedCustom; Config.EditorGrids = savedGrids; Config.Write();
        game->Editor.Doc = tja::Document::NewChart("length", "", 2); ++game->Editor.Revision;
        game->Editor.CursorMeasure = 0; game->Editor.Course = 0;
        game->EditorResizeForAudioLength(20);
        Check(game->EditorMeasureCount() == 10, "audio length extends empty measures");
        tja::Document::SetNote(game->Editor.Doc.Courses[0].Measures[9], 0, 1, '1'); ++game->Editor.Revision;
        game->EditorResizeForAudioLength(10);
        Check(game->EditorMeasureCount() == 10, "audio length preserves existing notes");
        tja::Document::SetNote(game->Editor.Doc.Courses[0].Measures[9], 0, 1, '0'); ++game->Editor.Revision;
        game->EditorResizeForAudioLength(10);
        Check(game->EditorMeasureCount() == 5, "audio length trims only blank tail");
        game->Editor.Doc = tja::Document::NewChart("length tempo change", "", 2); ++game->Editor.Revision;
        tja::Document::AddCommand(game->Editor.Doc.Courses[0].Measures[1], 0, 1, "#BPMCHANGE 240");
        game->EditorResizeForAudioLength(10);
        Check(game->EditorMeasureCount() == 9, "audio length uses final BPM after tempo change");
        tja::Document::SetValue(game->Editor.Doc.Header, "OFFSET", "-2"); ++game->Editor.Revision;
        game->EditorResizeForAudioLength(10);
        Check(game->EditorMeasureCount() == 7, "audio length accounts for negative OFFSET");
        game->Editor.Doc = tja::Document::NewChart("osu timing", "", 100); ++game->Editor.Revision;
        tja::Document::SetValue(game->Editor.Doc.Header, "BPM", "137.5");
        tja::Document::SetValue(game->Editor.Doc.Header, "FASOSUFORMAT", "14");
        const bool savedJiro = Config.JiroChartMode; Config.JiroChartMode = true;
        Check(std::abs(game->EditorBuildTimeline().TimeAt(99, 1) - 100 * 240000.0 / 137.5) < 1e-5, "osu timeline avoids integer measure drift");
        Config.JiroChartMode = savedJiro;
        game->Editor.Doc = tja::Document::NewChart("demo cursor", "", 4); ++game->Editor.Revision;
        tja::Document::SetValue(game->Editor.Doc.Header,"OFFSET","1.25");
        tja::Document::AddCommand(game->Editor.Doc.Courses[0].Measures[1],0,1,"#BPMCHANGE 240");
        tja::Document::AddCommand(game->Editor.Doc.Courses[0].Measures[2],0,1,"#MEASURE 3/4");
        game->Editor.Division=16; game->Editor.CursorMeasure=2; game->Editor.CursorSlot=4;
        const auto oldDemo=tja::Document::GetValue(game->Editor.Doc.Header,"DEMOSTART");
        const auto oldChartOffset=Config.ChartOffset; const auto oldSongOffset=Config.SongOffset;
        Config.ChartOffset=120; Config.SongOffset=-70; game->Editor.PreviewSpeed=.5;
        game->EditorSetDemoStart();
        Check(std::abs(std::stod(tja::Document::GetValue(game->Editor.Doc.Header,"DEMOSTART"))-2)<1e-6,"DEMOSTART includes cursor grid, tempo, meter and OFFSET");
        game->EditorUndo(false);
        Config.ChartOffset=oldChartOffset; Config.SongOffset=oldSongOffset; game->Editor.PreviewSpeed=1;
        Check(tja::Document::GetValue(game->Editor.Doc.Header,"DEMOSTART")==oldDemo,"DEMOSTART undo");
        game->Editor.Doc=tja::Document::NewChart("wave", "", 4); ++game->Editor.Revision;
        tja::Document::SetValue(game->Editor.Doc.Header,"OFFSET","-0.2");
        auto& waveMeasures=game->Editor.Doc.Courses[0].Measures;
        tja::Document::SetNote(waveMeasures[0],1,4,'1'); tja::Document::SetNote(waveMeasures[0],1,2,'2');
        tja::Document::SetNote(waveMeasures[1],1,4,'3'); tja::Document::SetNote(waveMeasures[1],1,2,'5');
        game->Editor.Selecting=false;
        const auto waveBefore=game->Editor.Doc.Write();
        game->EditorApplyWaveAlign({{.8,1},{1.1,1},{2.83,1},{3.3,1}},250);
        const auto& aligned=game->Editor.Doc.Courses[0].Measures;
        Check(aligned[0].Notes.size()==2 && aligned[1].Notes.size()==2 && aligned[0].Notes[0].Type=='1' && aligned[0].Notes[1].Type=='2' && aligned[1].Notes[0].Type=='3',"wave correction retains colours and count");
        const auto& alignedTimeline=game->EditorTimelineCached();
        Check(std::abs(alignedTimeline.TimeAt(0,(double)aligned[0].Notes[0].Num/aligned[0].Notes[0].Den)/1000+.2-.8)<.002,"wave correction maps audio OFFSET to note position");
        Check(aligned[1].Notes[1].Num==1 && aligned[1].Notes[1].Den==2 && aligned[1].Notes[1].Type=='5',"roll anchors unchanged");
        game->EditorUndo(false); Check(game->Editor.Doc.Write()==waveBefore,"wave correction undo restores exact document");
        game->Editor.Selecting=true; game->Editor.SelMeasure=0; game->Editor.SelNum=0; game->Editor.SelDen=16;
        game->Editor.CursorMeasure=0; game->Editor.CursorSlot=15;
        game->EditorApplyWaveAlign({{.8,1},{1.1,1},{2.83,1}},250);
        Check(game->Editor.Doc.Courses[0].Measures[1].Notes[0].Num==1 && game->Editor.Doc.Courses[0].Measures[1].Notes[0].Den==4,"wave correction respects selection");
        game->Editor.Selecting=false;
        game->Editor.Doc=tja::Document::NewChart("wave boundary", "", 3); ++game->Editor.Revision;
        tja::Document::SetNote(game->Editor.Doc.Courses[0].Measures[0],31,32,'1');
        tja::Document::SetNote(game->Editor.Doc.Courses[0].Measures[1],1,2,'2');
        tja::Document::AddCommand(game->Editor.Doc.Courses[0].Measures[1],0,1,"#BPMCHANGE 240");
        game->EditorApplyWaveAlign({{2.05,1},{2.6,1}},250);
        Check(game->Editor.Doc.Courses[0].Measures[0].Notes.empty() && game->Editor.Doc.Courses[0].Measures[1].Notes.size()==2,"wave correction crosses barline while preserving count");
        Check(game->Editor.Doc.Courses[0].Measures[1].Commands[0].Text=="#BPMCHANGE 240","wave correction preserves tempo commands");
        tja::Document::AddCommand(game->Editor.Doc.Courses[0].Measures[1],0,1,"#DELAY -1"); ++game->Editor.Revision;
        const auto reverseBefore=game->Editor.Doc.Write();
        game->EditorApplyWaveAlign({{1.05,1},{1.6,1}},250);
        Check(game->Editor.Doc.Write()==reverseBefore,"nonmonotone timeline is left unchanged");
        const auto savedDirectories = Config.SongDirectories;
        const auto root = fs::absolute("build") / ("live-catalog-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
        { std::ofstream genre(root / "genre.ini"); genre << "GenreName=Test\n"; }
        const auto chartA = root / "a.tja", chartB = root / "b.tja";
        auto writeChart = [&](const fs::path& path, const std::string& title) { std::ofstream file(path, std::ios::binary); file << tja::Document::NewChart(title, "", 1).Write(); };
        writeChart(chartA, "original");
        Config.SongDirectories = { Game::PathToU8(root) }; game->NowScene = Game::Scene::SongSelect;
        game->EnumChart(Config.SongDirectories);
        game->TempBoxDatas.front()->GetGenre()->IsOpen = true; game->BoxDatasUpdate();
        game->BoxDataIndex = 1;
        auto poll = [&] {
            std::promise<catalog::Snapshot> ready;
            game->ChartWatchTask = ready.get_future(); game->ChartWatchTaskGeneration = game->ChartWatchGeneration;
            ready.set_value(catalog::Scan({ root })); game->ChartWatchLast = std::chrono::steady_clock::now();
            game->ChartWatchUpdate();
        };
        writeChart(chartA, "changed title");
        poll(); Check(game->BoxDatas[game->BoxDataIndex]->GetChart()->Title == "original", "catalog waits for stable writes");
        poll(); Check(game->BoxDatas[game->BoxDataIndex]->GetChart()->Title == "changed title", "live catalog reloads changed metadata");
        Check(game->TempBoxDatas.front()->GetGenre()->IsOpen, "live reload preserves open genres");
        writeChart(chartB, "imported"); poll(); poll();
        Check(game->BoxDatas.size() == 3 && game->BoxDatas[game->BoxDataIndex]->GetChart()->ChartPath == chartA, "live import preserves selected song");
        fs::remove(chartA); poll(); poll();
        Check(game->BoxDatas.size() == 2 && game->BoxDataIndex < (int)game->BoxDatas.size(), "live deletion leaves valid selection");
        fs::remove(chartB); fs::remove(root / "genre.ini"); fs::remove(root);
        Config.SongDirectories = savedDirectories;
        std::cout << "PASS: Jiro and OFFSET, fold reopening and undo, eased gradient, fixed test start, live catalog reload and selection\n";
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
