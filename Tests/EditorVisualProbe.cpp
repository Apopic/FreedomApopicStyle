#include "GameTestAccess.hpp"
#include <iostream>
static LRESULT CALLBACK ProbeHook(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_WINDOWPOSCHANGING) reinterpret_cast<WINDOWPOS*>(lparam)->flags &= ~SWP_SHOWWINDOW;
    return WndProc(window, message, wparam, lparam);
}
static bool CaptionIntegrationTest() {
    const HWND window = GetMainWindowHandle();
    const auto style = GetWindowLongPtr(window,GWL_STYLE);
    if (!(style & WS_MAXIMIZEBOX) || !(style & WS_MINIMIZEBOX)) return false;
    SetHookWinProc(ProbeHook);
    auto click = [&](int hit, WPARAM expected, UINT down) {
        RECT bounds{}; GetWindowRect(window,&bounds);
        POINT point{}; bool found=false;
        for (long y=bounds.top;y<bounds.top+70 && !found;++y) for (long x=bounds.left;x<bounds.right;++x) {
            if (CaptionHitTest(window,{x,y})==hit) { point={x,y}; found=true; break; }
        }
        if (!found) return false;
        SendMessage(window,WM_KILLFOCUS,0,0);
        if (SendMessage(window,WM_MOUSEACTIVATE,(WPARAM)window,MAKELPARAM(hit,down)) != MA_ACTIVATE) { std::cerr<<"FAIL: initial click discarded on activation\n"; return false; }
        SendMessage(window,down,hit,MAKELPARAM((short)point.x,(short)point.y));
        if (__windowdrag.Button != (WPARAM)hit) { std::cerr<<"FAIL: caption press message "<<down<<'\n'; return false; }
        ScreenToClient(window,&point);
        SendMessage(window,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM((short)point.x,(short)point.y));
        SendMessage(window,WM_LBUTTONUP,0,MAKELPARAM((short)point.x,(short)point.y));
        MSG message{};
        if (!PeekMessage(&message,window,WM_SYSCOMMAND,WM_SYSCOMMAND,PM_REMOVE) || message.wParam != expected) return false;
        DispatchMessage(&message);
        return __windowdrag.Button==0 && GetCapture()!=window;
    };
    for (int cycle=0;cycle<10;++cycle) {
        const UINT down=cycle%2 ? WM_NCLBUTTONDBLCLK : WM_NCLBUTTONDOWN;
        if (!click(HTMINBUTTON,SC_MINIMIZE,down) || !IsIconic(window)) return false;
        SendMessage(window,WM_SYSCOMMAND,SC_RESTORE,0);
        if (IsIconic(window)) return false;
        if (!click(HTMAXBUTTON,SC_MAXIMIZE,down) || !IsZoomed(window)) return false;
        if (!click(HTMAXBUTTON,SC_RESTORE,WM_NCLBUTTONDBLCLK) || IsZoomed(window)) return false;
    }
    std::cout << "PASS: 10 repeated DxLib minimize/maximize/restore cycles, including double-click messages\n";
    return true;
}
int main() {
    ChangeWindowMode(TRUE); SetWindowStyleMode(7); SetWindowSizeChangeEnableFlag(TRUE,TRUE); SetWindowVisibleFlag(FALSE); SetGraphMode(1280, 720, 32);
    if (DxLib_Init() == -1) return 2;
    if (!CaptionIntegrationTest()) { std::cerr << "FAIL: DxLib caption integration\n"; return 1; }
    SetDrawScreen(DX_SCREEN_BACK);
    auto game = std::make_unique<Game>();
    auto& Config = GameTestAccess::Config(*game);
    auto& Skin = GameTestAccess::Skin(*game);
    Skin.Info.Resolution = {1280,720};
    Skin.Base = new std::remove_pointer_t<decltype(Skin.Base)>();
    Skin.Base->Playing.Image.Lane.Size.Height=130;
    Skin.Base->Playing.Image.Note.Load((fs::absolute("../Android/app/src/main/assets/Default/Image/Playing").string() + "/"), json{{"FilePath","Note.png"},{"Div",{{"X",15},{"Y",1}}},{"Size",{{"Width",128},{"Height",128}}}});

    // Android で文字幅取得が失敗しても、ツールバーと編集レーンを画面内に収める。
    game->EditorFontSize = 16; game->EditorFontData.Size = 16;
    game->EditorFontData.Handle = 0x7fffffff;
    if (GetStrlen("test", 0x7fffffff) != 0 || GetStrlenCached("test", 0x7fffffff) != 0) return 1;
    for (const auto& button : game->EditorButtons()) {
        if (!std::isfinite(button.Rect.R) || button.Rect.L < 0 || button.Rect.R > 1280) {
            std::cerr << "FAIL: unavailable font pushes toolbar offscreen\n"; return 1;
        }
    }
    if (game->EditorButtonsBottom >= 720 * .4f) return 1;
    game->EditorFontData.Handle = -1; game->EditorFontSize = 0;
    game->Editor.Open = true;
    game->Editor.Doc = tja::Document::NewChart("Visual check", "", 8);
    game->Editor.Path = "visual-test.tja";
    game->Editor.Folds = {{4,6}}; game->Editor.OpenFolds = {{0,2}};
    for (int i=0;i<16;++i) tja::Document::SetNote(game->Editor.Doc.Courses[0].Measures[0],i,16,i<3?'1':i%2?'2':'1');
    for (int i=0;i<4;++i) tja::Document::SetNote(game->Editor.Doc.Courses[0].Measures[1],28+i,32,'1');
    tja::Document::SetNote(game->Editor.Doc.Courses[0].Measures[2],0,1,'3');
    tja::Document::SetNote(game->Editor.Doc.Courses[0].Measures[2],1,16,'1');
    game->EditorButtons();
    for (const auto& button : game->EditorButtons()) if (button.Label == "音源長→小節数") { game->PointerPrev.X=(int)button.Rect.L+4; game->PointerPrev.Y=(int)button.Rect.T+4; }
    game->EditorDraw(); SaveDrawScreenToPNG(0,0,1280,720,"build/editor-layout.png");
    const auto lane = game->EditorLane(0);
    const float noteHeight = game->EditorGridNoteHeight();
    const float extent=std::max(game->EditorNoteWidthPerHeight('1'),game->EditorNoteWidthPerHeight('2'));
    if (std::abs(lane.R-(game->EditorGridArea().R-10))>.01f) { std::cerr<<"FAIL: editor lane leaves horizontal space\n"; return 1; }
    const float sixteenthGap=(lane.R-lane.L)/16;
    const float expectedRatio=0.61731f;
    if (std::abs(noteHeight*extent/sixteenthGap-expectedRatio)>.001f) { std::cerr<<"FAIL: reference triplet spacing proportion\n"; return 1; }
    int hitMeasure=-1, hitSlot=-1;
    if (!game->EditorHitLane((int)game->EditorX(lane,7.0/16),(int)((lane.T+lane.B)/2),hitMeasure,hitSlot) || hitMeasure!=0 || hitSlot!=7) return 1;
    Skin.Info.Resolution={1600,900};
    game->EditorButtons();
    const auto largeLane=game->EditorLane(0);
    if (std::abs(largeLane.R-(game->EditorGridArea().R-10))>.01f || std::abs(game->EditorGridNoteHeight()*extent/((largeLane.R-largeLane.L)/16)-expectedRatio)>.001f) return 1;
    Skin.Info.Resolution={1280,720};
    game->EditorButtons();
    const auto savedSpeed=Config.ChartSpeed;
    Config.ChartSpeed=2.5;
    if (game->EditorGridNoteHeight()!=noteHeight) { std::cerr<<"FAIL: gameplay speed changes editor note size\n"; return 1; }
    Config.ChartSpeed=savedSpeed;
    tja::Document::SetValue(game->Editor.Doc.Header,"BPM","175"); ++game->Editor.Revision;
    if (game->EditorGridNoteHeight()!=noteHeight) return 1;
    game->Editor.Division=8;
    if (game->EditorGridNoteHeight() != noteHeight) return 1;
    tja::Document::SetNote(game->Editor.Doc.Courses[0].Measures[1],1,128,'1');
    if (game->EditorGridNoteHeight() != noteHeight) { std::cerr<<"FAIL: density changed note size\n"; return 1; }
    tja::Document::AddCommand(game->Editor.Doc.Courses[0].Measures[2],0,1,"#MEASURE 3/4"); ++game->Editor.Revision;
    if (game->EditorGridNoteHeight() != noteHeight) { std::cerr<<"FAIL: short measure changed note size\n"; return 1; }
    const auto shortLane=game->EditorLane(2);
    if (std::abs((shortLane.R-shortLane.L)/(lane.R-lane.L)-.75f)>.001f) return 1;
    tja::Document::AddCommand(game->Editor.Doc.Courses[0].Measures[2],0,1,"#MEASURE 16/1"); ++game->Editor.Revision;
    const auto longLane=game->EditorLane(2);
    if (longLane.R<=game->EditorGridArea().R || std::abs(game->EditorGridNoteHeight()*extent/((game->EditorLane(0).R-game->EditorLane(0).L)/16)-expectedRatio)>.001f) return 1;
    if(game->EditorGridNoteHeight()!=noteHeight) return 1;
    game->EditorScrollHorizontal(1);
    if(game->EditorLane(2).L>=longLane.L) return 1;
    game->EditorHorizontalOffset=0;
    game->EditorApplyPreviewSpeed(.85); if(std::abs(game->Editor.PreviewSpeed-.85)>1e-9) return 1;
    game->Editor.Doc.Courses[0].Measures[2].Commands.pop_back(); ++game->Editor.Revision;
    game->Editor.Division=16;
    for (const auto& button : game->EditorButtons()) if (button.Rect.R>1280 || button.Rect.B>720) return 1;
    Config.EditorCustomGrids={7};
    game->Editor.Panel=Game::EditorState::PanelType::Grid;
    game->EditorDraw(); SaveDrawScreenToPNG(0,0,1280,720,"build/editor-grids.png");
    game->Editor.Panel=Game::EditorState::PanelType::None;
    game->EditorBeginFields("#MEASURE",{{"分子","3"},{"分母","4"}},[](auto){});
    game->EditorDraw(); SaveDrawScreenToPNG(0,0,1280,720,"build/editor-fields.png");
    game->EditorFinishFields(false);
    game->EditorBeginWaveAlign();
    if (game->Editor.FormFields.empty() || game->Editor.FormFields[0].Value!="10") return 1;
    game->EditorDraw(); SaveDrawScreenToPNG(0,0,1280,720,"build/editor-wave-dialog.png");
    game->EditorFinishFields(false);
    game->EditorChange();
    tja::Document::SetNote(game->Editor.Doc.Courses[0].Measures[3],0,1,'7');
    game->EditorBalloonPlaced(3,0,1,true);
    if(!game->Editor.FormOpen) return 1;
    game->EditorDraw(); SaveDrawScreenToPNG(0,0,1280,720,"build/editor-balloon-dialog.png");
    game->EditorFinishFields(true,{"17"});
    if(tja::Document::GetValue(game->Editor.Doc.Courses[0].Header,"BALLOON")!="17")return 1;
    game->EditorUndo(false);
    if(!game->Editor.Doc.Courses[0].Measures[3].Notes.empty())return 1;
    const bool savedJiro=Config.JiroChartMode,savedAuto=Config.AutoPlay;
    bool jiroPreviewFound=false;
    for(const auto& button:game->EditorButtons()) if(button.Label=="次郎形式再生") {
        button.Action();jiroPreviewFound=true;break;
    }
    if(!jiroPreviewFound || game->NowScene==Game::Scene::Loading || !game->EditorJiroPreviewEnabled()) return 1;
    Config.JiroChartMode=savedJiro;Config.AutoPlay=savedAuto;
    game->EditorFontData.Delete(); game->EditorPanelFontData.Delete();
    Skin.Dispose(); DxLib_End(); std::cout << "PASS: editor screenshots rendered\n";
}
