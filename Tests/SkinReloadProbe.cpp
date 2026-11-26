#include "GameTestAccess.hpp"
#include <iostream>
static void Check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static std::vector<int> Counts(){return {GetHandleNum(DX_HANDLETYPE_GRAPH),GetHandleNum(DX_HANDLETYPE_FONT),GetHandleNum(DX_HANDLETYPE_SOUND)};}
static void ClearFiles(json& data){if(data.is_object()){for(auto it=data.begin();it!=data.end();++it){if(it.key()=="FilePath")*it="";else ClearFiles(*it);}}else if(data.is_array())for(auto&v:data)ClearFiles(v);}
int main(){ChangeWindowMode(TRUE);SetWindowVisibleFlag(FALSE);if(DxLib_Init()<0)return 2;
auto game = std::make_unique<Game>();
auto& Config = GameTestAccess::Config(*game);
auto& Skin = GameTestAccess::Skin(*game);
try{auto root=fs::absolute("build/skin-reload-fixture");fs::create_directories(root/"Default");fs::create_directories(root/"Active");json data;{std::ifstream f("../Android/app/src/main/assets/Default/skinconfig.json");f>>data;}ClearFiles(data);
auto write=[&](const char* name,const json& d){std::ofstream f(root/name/"skinconfig.json");f<<d.dump();};
auto original=data;original["Info"]["Name"]="Default";write("Default",original);data["Info"]["Name"]="Active";data["Base"]["Title"]["Image"]["Logo"]={{"FilePath","dot.png"},{"Div",{{"X",1},{"Y",1}}},{"Size",{{"Width",8},{"Height",8}}},{"Pos",{{"X",8},{"Y",8}}}};
int image=MakeARGB8ColorSoftImage(8,8);FillSoftImage(image,200,80,40,255);SaveSoftImageToPng(Game::PathToU8(root/"Active/dot.png").c_str(),image,0);DeleteSoftImage(image);write("Active",data);
Skin.Init(root);Check(Skin.Infos.size()==2,"skin discovery");Skin.Init(root);Check(Skin.Infos.size()==2,"discovery does not duplicate entries");Check(Skin.TryLoad("Active"),"initial valid load");Config.SkinName="Active";const auto counts=Counts();std::cout<<"initial load\n"<<std::flush;
for(int i=0;i<20;++i){Check(Skin.TryLoad("Active"),"repeated valid reload");Check(Counts()==counts,"reload releases old graphics/fonts/sounds");}
std::cout<<"repeated reloads complete\n"<<std::flush;auto bad=data;bad["Base"]["Result"]["Font"]["Title"]["Size"]="bad";write("Active",bad);auto* before=Skin.Base;Check(!Skin.TryLoad("Active"),"late schema error rejected");Check(Skin.Base==before&&Counts()==counts,"partial load rolls back without leaking handles");
std::cout<<"bad schema complete\n"<<std::flush;bad=data;bad["Base"]["Title"]["Image"]["Logo"]["FilePath"]="missing.png";write("Active",bad);Check(!Skin.TryLoad("Active",nullptr,true)&&Skin.Base==before,"unfinished asset update keeps old skin");
std::cout<<"missing asset complete\n"<<std::flush;write("Active",data);
{std::ofstream f(root/"Active/dot.png",std::ios::binary);f<<"unfinished image";}
Check(!Skin.TryLoad("Active",nullptr,true)&&Skin.Base==before&&Counts()==counts,"corrupted asset rolls back without leaking");
image=MakeARGB8ColorSoftImage(8,8);FillSoftImage(image,200,80,40,255);SaveSoftImageToPng(Game::PathToU8(root/"Active/dot.png").c_str(),image,0);DeleteSoftImage(image);
game->SkinWatchRoot=root;game->SkinWatch.Accept(skincatalog::Scan(root));
data["Info"]["Version"]="changed";write("Active",data);
auto queue=[&]{std::promise<catalog::Snapshot> ready;game->SkinWatchTask=ready.get_future();ready.set_value(skincatalog::Scan(root));game->SkinWatchLast=std::chrono::steady_clock::now();};
queue();game->NowScene=game->MemScene=Game::Scene::Playing;game->SkinWatchUpdate();Check(Skin.Info.Version!="changed","playing defers reload");
game->NowScene=game->MemScene=Game::Scene::Editor;game->SkinWatchUpdate();Check(Skin.Info.Version!="changed","editor defers reload");
game->NowScene=game->MemScene=Game::Scene::Title;game->SkinWatchUpdate();Check(Skin.Info.Version!="changed","first changed scan waits for stability");
queue();game->SkinWatchUpdate();Check(Skin.Info.Version=="changed","second stable scan applies reload in menu");
game->NowScene=game->MemScene=Game::Scene::ConfigMenu;game->ConfigUI.SkinOnEnter="Active";
fs::remove(root/"Active/skinconfig.json");game->SkinWatch={};queue();game->SkinWatchUpdate();Check(Config.SkinName=="Default"&&Skin.Info.Name=="Default","deleted selected skin falls back on first scan while Config is open");
Check(game->ConfigUI.SkinOnEnter=="Default"&&!Skin.IsLoading&&GetASyncLoadNum()==0,"fallback finishes all loads and updates Config baseline");
json config;{std::ifstream f(GetExecutablePath()/"config.json");f>>config;}Check(config["SkinName"]=="Default","fallback persists Default setting");
Skin.Dispose();Skin.Init(fs::absolute("../Android/app/src/main/assets"));std::string loadError;
Check(Skin.TryLoad("Default",&loadError),"actual Default startup");
const bool actualLoaded=Skin.TryLoad("Default",&loadError,true);Check(actualLoaded,loadError.c_str());
Skin.Dispose();DxLib_End();std::cout<<"PASS: safe reload, stable handle counts, exception rollback, removed-skin fallback, saved config and actual Default assets\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';Skin.Dispose();DxLib_End();return 1;}}
