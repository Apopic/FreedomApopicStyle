#include "GameTestAccess.hpp"
#include <iostream>
static void Check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
int main(){ChangeWindowMode(TRUE);SetWindowVisibleFlag(FALSE);if(DxLib_Init()<0)return 2;
auto game = std::make_unique<Game>();
auto& Config = GameTestAccess::Config(*game);
auto& Skin = GameTestAccess::Skin(*game);
Skin.Base=new std::remove_pointer_t<decltype(Skin.Base)>();Skin.Info.Resolution={1280,720};
try{game->Editor.Testing=true;game->Editor.Path=fs::absolute("build/gimmick-fixture.tja");
game->Editor.TestText="TITLE:HB timing regression\nBPM:135\nCOURSE:Oni\n#HBSCROLL\n#START\n1,\n#BPMCHANGE 270\n#MEASURE 2/16\n2\n#DELAY 0.445\n0,\n#BPMCHANGE 270\n#MEASURE 7/16\n0\n#DELAY 0.223\n00\n#DELAY 0.223\n00\n#DELAY 0.334\n00,\n#BPMCHANGE -540\n#MEASURE -4/16\n#DELAY 0.111\n00,\n#BPMCHANGE 270\n#MEASURE 7/16\n#DELAY 0.334\n000\n#DELAY 0.223\n00\n#DELAY 0.223\n00\n#DELAY 0.557\n0,\n#BPMCHANGE 270\n#MEASURE 1/8\n4\n#DELAY 0.335\n000,\n#BPMCHANGE 135\n#MEASURE 4/4\n1,\n#END\n";
game->CourseIndex=(size_t)Game::CourseType::Oni;Config.JiroChartMode=true;game->LoadingProc();Check(Config.JiroChartMode,"HB loads with selected Jiro timing");
const Game::NoteData* big=nullptr;const Game::NoteData* preceding=nullptr;
for(auto&n:game->Chart.RawNoteDatas){if(n.NoteType=='4')big=&n;if(n.NoteType=='2')preceding=&n;}
Check(big&&preceding,"fixture notes loaded");double reverse=0;for(auto&e:game->Chart.BpmEvents)if(e.Value<0)reverse=e.Time;
auto x=[&](double t){return game->Chart.JiroScrollPx(*big,game->Chart.MakeScrollFrame(game->Chart.FrozenTime(t)),420,1,true);};
Check(x(preceding->AbsTime+100)>250,"future big Ka stays on right after previous hit");
Check(x(reverse+180)>x(reverse-20)+20,"big Ka reverses away from judgment during negative BPM");
auto movingX=[&](double t){return game->Chart.JiroScrollPx(*big,game->Chart.MakeScrollFrame(t),420,1,true);};
for(double t=reverse+20;t<reverse+40;t+=.125)Check(std::abs((movingX(t+.125)-movingX(t))-.118125)<1e-7,"negative 540 BPM has continuous subpixel velocity outside DELAY freeze");
Check(std::abs(x(big->AbsTime))<1,"big Ka arrives only at its hit time");
std::cout<<"PASS: HB selection, BPM-before-DELAY event boundaries, future big Ka reverses on right\n";
// Verify the supplied full chart too, when the optional local reference is present.
TextLines reference;if(reference.Load("build/sushi-reference.tja")){game->Editor.TestText.clear();for(auto l:reference.lines)game->Editor.TestText+=std::string(l)+"\n";game->LoadingProc();int index=0;for(auto&n:game->Chart.RawNoteDatas)if(n.NoteType>='1'&&n.NoteType<='4'&&++index==203){auto px=[&](double t){return game->Chart.JiroScrollPx(n,game->Chart.MakeScrollFrame(game->Chart.FrozenTime(t)),420,1,true);};Check(n.NoteType=='4'&&px(51068)>300&&px(53268)>px(52768)+50,"SUSHI reference 203rd big Ka stays right and reverses");std::cout<<"PASS: supplied SUSHI chart 203rd note stays right ("<<px(51068)<<"px), reverses "<<px(52768)<<" -> "<<px(53268)<<"px\n";}}
Skin.Dispose();DxLib_End();return 0;}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';DxLib_End();return 1;}}
