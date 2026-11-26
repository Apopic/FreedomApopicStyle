#include "../Game.hpp"
#include <iostream>
int main() {
    try {
        ChangeWindowMode(TRUE);SetWindowVisibleFlag(FALSE);SetGraphMode(360,100,32);
        if(DxLib_Init()<0)return 2;
        const int sound=MakeSoftSoundCustom(1,16,44100,64,FALSE);
        if(sound==-1)throw std::runtime_error("soft sound fixture");
        try { ScopedSoftSound owned{sound};throw std::runtime_error("decode interrupted"); }catch(const std::runtime_error&){}
        if(GetSoftSoundSampleNum(sound)!=-1)throw std::runtime_error("soft sound leaked on exception");
        SetDrawScreen(DX_SCREEN_BACK);ClearDrawScreen();
        {
            auto game=std::make_unique<Game>();
            game->IsMulti=true;game->DrawPlayerChar(0);
            if(std::isfinite(game->PlayerCharAnim.LastTime))throw std::runtime_error("multiplayer character animation must stay inactive");
            game->IsMulti=false;game->DrawPlayerChar(0);
            const Game::EditorIcon icons[]={Game::EditorIcon::Left,Game::EditorIcon::Right,Game::EditorIcon::TaikoSound,Game::EditorIcon::TaikoSound,Game::EditorIcon::JiroPreview};
            for(int i=0;i<5;++i) {
                Game::EditorButton button;button.Rect={float(i*70+5),10,float(i*70+65),80};button.Icon=icons[i];button.Active=i==2;
                game->EditorDrawIcon(button,GetColor(255,255,255));
            }
            const auto path=GetExecutablePath()/"editor-illustration-buttons.png";
            if(SaveDrawScreenToPNG(0,0,360,100,path.string().c_str())<0)throw std::runtime_error("icon screenshot");
            for(int y=0;y<100;++y)for(int x=0;x<360;++x) { const auto pixel=GetPixel(x,y); int r=0,g=0,b=0; GetColor2(pixel,&r,&g,&b); if(r!=g||g!=b)throw std::runtime_error("icons must be monochrome"); }
            // Exercise device recreation while the real timing observer is running.
            std::atomic_bool end=false,stop=false;
            std::thread observer([&]{game->_LogUpdate(&end,&stop);});
            try {
                _Skin skin;
                skin.Init(fs::path(__FILE__).parent_path().parent_path()/"Android/app/src/main/assets");
                if(skin.Infos.empty())throw std::runtime_error("skin switching fixture missing");
                for(int pass=0;pass<3;++pass)for(const auto& info:skin.Infos) {
                    std::string error;
                    if(!skin.TryLoad(info.Name,&error))throw std::runtime_error(error);
                    skin.Base->ConfigMenu.Font.String.Draw({20,20},GetColor(255,255,255),0,"skin switch");
                    ScreenFlip();
                }
                stop=true;observer.join();
                if(!game->GetVSyncWaitLog())throw std::runtime_error("timing observer did not signal");
            } catch(...) {stop=true;if(observer.joinable())observer.join();throw;}
        }
        for(int iteration=0;iteration<3;++iteration) {
            ChangeWindowMode(TRUE);SetWindowVisibleFlag(FALSE);
            if(DxLib_Init()<0) return 2;
            std::atomic_bool muxDone=false,stretchDone=false;
            auto game=std::make_unique<Game>();
            game->Editor.SongPcm={.25f};
            game->Editor.StretchCancel=std::make_shared<std::atomic_bool>(false);
            auto* pcm=&game->Editor.SongPcm;
            game->Editor.StretchTask=std::async(std::launch::async,[pcm,&stretchDone]{
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                if(pcm->size()!=1 || (*pcm)[0]!=.25f) throw std::runtime_error("PCM lifetime");
                stretchDone=true;return std::vector<float>{};
            });
            game->Recorder.Muxer=std::thread([&muxDone]{std::this_thread::sleep_for(std::chrono::milliseconds(20));muxDone=true;});
            game->EditorFontData.Handle=CreateFontToHandle(nullptr,18,1);
            game->Cleanup();game->Cleanup();game.reset();
            if(!muxDone || !stretchDone || DxLib_IsInit())throw std::runtime_error("cleanup failed");
        }
        JudgeData judge;
        judge.ScoreRateGood=1.5;
        judge.Hit(JudgeType::Good,100,'1');
        if(judge.Score!=150 || judge.Branch.Score!=150) throw std::runtime_error("score and branch accumulation");
        judge.Score=UINT64_MAX;judge.Branch.Score=UINT64_MAX;
        judge.Hit(JudgeType::Good,100,'1');
        if(judge.Score!=UINT64_MAX || judge.Branch.Score!=UINT64_MAX) throw std::runtime_error("score saturation");
        {
            auto game=std::make_unique<Game>();game->EditorReturnScene=Game::Scene::SongSelect;
            game->NowScene=Game::Scene::Editor;game->Editor.Open=true;
            game->Editor.Dirty=true;game->EditorClose();
            if(!game->Editor.Open||game->NowScene!=Game::Scene::Editor)throw std::runtime_error("unsaved confirmation");
            game->EditorClose();
            if(game->Editor.Open||game->NowScene!=Game::Scene::SongSelect)throw std::runtime_error("return to song selection");
        }
        {
            auto game=std::make_unique<Game>();
            game->Editor.Doc=tja::Document::NewChart("preview roll","",4);game->Editor.Course=0;game->Editor.Open=true;
            auto& course=game->Editor.Doc.Courses[0];tja::Document::SetValue(course.Header,"BALLOON","3,7");
            tja::Document::SetNote(course.Measures[0],0,1,'5');tja::Document::SetNote(course.Measures[0],1,2,'8');
            tja::Document::SetNote(course.Measures[1],0,1,'7');tja::Document::SetNote(course.Measures[1],1,2,'8');
            tja::Document::SetNote(course.Measures[2],0,1,'9');tja::Document::SetNote(course.Measures[2],1,2,'8');
            const auto timeline=game->EditorBuildTimeline();
            int found=0;for(const auto& note:timeline.Notes)if(note.Type=='7'||note.Type=='9') {
                if(note.End<=note.Time||note.Hits!=(note.Type=='7'?3:7))throw std::runtime_error("preview roll metadata");++found;
            }
            if(found!=2||timeline.BalloonCounts.at({1,0,1})!=3||timeline.BalloonCounts.at({2,0,1})!=7)throw std::runtime_error("balloon label mapping");
        }
        {
            auto game=std::make_unique<Game>();
            game->Editor.Doc=tja::Document::NewChart("coarse waveform grid","",2);game->Editor.Course=0;game->Editor.Open=true;
            auto& measure=game->Editor.Doc.Courses[0].Measures[0];
            tja::Document::SetNote(measure,1,5,'1');tja::Document::SetNote(measure,9,20,'2');tja::Document::SetNote(measure,7,10,'3');
            measure.Resolution=1920;
            const auto before=game->Editor.Doc.Write();
            game->EditorApplyWaveAlign({{.501,1},{1.001,1},{1.501,1}},250);
            const auto& aligned=game->Editor.Doc.Courses[0].Measures[0];
            if(aligned.Resolution!=1||aligned.Notes.size()!=3||aligned.Notes[0].Den!=4||aligned.Notes[1].Den!=2||aligned.Notes[2].Den!=4)
                throw std::runtime_error("wave alignment minimal common grid");
            if(aligned.Notes[0].Type!='1'||aligned.Notes[1].Type!='2'||aligned.Notes[2].Type!='3'||game->Editor.Undo.size()!=1||game->Editor.Undo[0].Doc.Write()!=before)
                throw std::runtime_error("wave alignment notes and undo");
            game->Editor.Selecting=true;game->Editor.SelMeasure=0;game->Editor.SelNum=1;game->Editor.SelDen=4;
            game->Editor.CursorMeasure=0;game->Editor.CursorSlot=12;
            tja::Document::AddCommand(game->Editor.Doc.Courses[0].Measures[0],1,2,"#BPMCHANGE 240");
            const auto beforeScroll=game->Editor.Doc.Write();
            ++game->Editor.Revision;
            game->EditorUniformScroll(120);
            const auto& commands=game->Editor.Doc.Courses[0].Measures[0].Commands;
            bool start=false,change=false,end=false;
            for(const auto& c:commands) {
                start|=c.Num==1&&c.Den==4&&c.Text=="#SCROLL 1";
                change|=c.Num==1&&c.Den==2&&c.Text=="#SCROLL 0.5";
                end|=c.Num==13&&c.Den==16&&c.Text=="#SCROLL 1";
            }
            if(!start||!change||!end||game->Editor.Undo.size()!=2||game->Editor.Undo.back().Doc.Write()!=beforeScroll)
                throw std::runtime_error("selection uniform scroll and undo");
        }
        tools::Installer installer;
        installer.Start([](tools::Progress&,std::string&)->bool { throw std::bad_alloc(); },"done");
        installer.Join();
        if(installer.State().State!=tools::Progress::Phase::Failed) throw std::runtime_error("installer exception state");
        installer.Start([](tools::Progress&,std::string&){return true;},"done");
        installer.Join();
        if(installer.State().State!=tools::Progress::Phase::Done) throw std::runtime_error("installer retry state");
        // Exception unwinding must also join the owned recording worker.
        std::atomic_bool finished=false;
        try {auto game=std::make_unique<Game>();game->Recorder.Muxer=std::thread([&finished]{finished=true;});throw std::runtime_error("injected");}
        catch(const std::runtime_error&) {}
        if(!finished)return 1;
        std::cout<<"PASS: repeated and idempotent cleanup, PCM worker lifetime, recording worker join, exception unwinding\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
