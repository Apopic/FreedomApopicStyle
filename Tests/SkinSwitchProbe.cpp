#include "../SystemCommon.cpp"
#include <iostream>
int main(int argc,char** argv){
    ChangeWindowMode(TRUE);SetWindowVisibleFlag(FALSE);if(DxLib_Init()<0)return 2;
    try{
        _Skin skin;skin.Init(argc>1?fs::absolute(argv[1]):fs::absolute("../Android/app/src/main/assets"));
        for(int i=0;i<3;++i)for(const auto& info:skin.Infos){
            std::cerr<<"LOAD "<<info.Name<<std::endl;
            std::string error;if(!skin.TryLoad(info.Name,&error))throw std::runtime_error(error);
            if(!skin.Base||skin.IsLoading||GetASyncLoadNum()!=0)throw std::runtime_error("incomplete switch");
            if(argc>1){
                for(const auto& voice:skin.Base->Playing.SE.ComboVoice)if(!voice.second||voice.second->Handle==-1)throw std::runtime_error("converted combo voice failed to load");
                if(info.Name=="FAS-original-test"){
                    const auto& pl=skin.Base->Playing;
                    if(pl.SE.DonBig.Handle==-1||pl.SE.KaBig.Handle==-1||pl.SE.FullComboVoice.Handle==-1||pl.SE.ComboVoice.size()!=2)throw std::runtime_error("original skin audio resources");
                    if(pl.Image.PlayerChar.Handles.size()!=4||pl.Image.KeyMapBox.Handles.size()!=2||pl.Config.PlayerCharGoGoFrames!=std::vector<int>({2,3}))throw std::runtime_error("original skin character/key view resources");
                    if(pl.Image.ProgressBar.Size.Width!=700||pl.Image.ProgressBar.Pos.X+350!=1277)throw std::runtime_error("original skin gauge geometry");
                }
            }
            int width=0,height=0;GetWindowSize(&width,&height);
            if(i>0&&(width!=900||height!=640))throw std::runtime_error("skin switch changed window dimensions");
            SetWindowSize(900,640);
            std::cerr<<"READY "<<info.Name<<std::endl;
        }
        skin.Dispose();skin.Dispose();DxLib_End();std::cout<<"PASS: repeated actual skin switching and disposal\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';DxLib_End();return 1;}
}
