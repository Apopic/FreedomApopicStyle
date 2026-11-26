#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace keymapview {
struct Key {int Code;float X,Y;};
inline std::vector<Key> Layout(const std::vector<std::vector<int>>& maps,float screenWidth,float boxWidth,float boxHeight,float distance,float centerX,float bottomY){
    if(maps.empty()||!std::isfinite(screenWidth)||screenWidth<=0||!std::isfinite(boxWidth)||boxWidth<=0)return {};
    const float pitch=std::max(std::isfinite(distance)?distance:0,boxWidth+4);
    const int columns=std::max(1,int((screenWidth-16-boxWidth)/pitch)+1);
    const int groupsPerRow=std::min(columns,int(maps.size()));
    std::vector<Key> out;
    float bottom=bottomY;
    for(int first=0;first<int(maps.size());first+=groupsPerRow){
        const int count=std::min(groupsPerRow,int(maps.size())-first);
        const float half=((count-1)*pitch+boxWidth)*.5f;
        const float center=std::clamp(centerX,half,std::max(half,screenWidth-half));
        int tallest=0;
        for(int lane=0;lane<count;++lane){
            std::vector<int> codes;std::array<bool,256> seen{};
            for(int code:maps[first+lane])if(code>=0&&code<256&&!seen[code]){seen[code]=true;codes.push_back(code);}
            tallest=std::max(tallest,int(codes.size()));
            const float x=center+(lane-(count-1)*.5f)*pitch;
            for(int i=0;i<int(codes.size());++i)out.push_back({codes[i],x,bottom-(int(codes.size())-1-i)*(boxHeight+4)});
        }
        bottom-=std::max(1,tallest)*(boxHeight+4);
    }    return out;
}
}
