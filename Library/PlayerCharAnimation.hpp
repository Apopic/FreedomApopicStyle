#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
namespace playerchar {
struct Clock {
    double LastTime=std::numeric_limits<double>::quiet_NaN(),Beats=0,JumpAt=std::numeric_limits<double>::quiet_NaN();
    uint64_t LastCombo=0;
    void Update(double now,double bpm,uint64_t combo,double jumpBeats=2,bool allowJump=true){
        if(!std::isfinite(now))return;
        if(std::isfinite(LastTime)){
            if(now<LastTime){Beats=0;JumpAt=std::numeric_limits<double>::quiet_NaN();}
            else Beats+=(now-LastTime)*(std::isfinite(bpm)?std::abs(bpm):120)/60000;
        }
        if(!std::isfinite(Beats)){Beats=0;JumpAt=std::numeric_limits<double>::quiet_NaN();}
        const bool jumping=std::isfinite(JumpAt)&&Beats-JumpAt>=0&&Beats-JumpAt<jumpBeats;
        if(!allowJump||combo<LastCombo)JumpAt=std::numeric_limits<double>::quiet_NaN();
        else if(combo/10>LastCombo/10&&!jumping)JumpAt=Beats;
        LastTime=now;LastCombo=combo;
    }
};
}
