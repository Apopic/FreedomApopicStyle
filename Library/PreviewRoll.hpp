#pragma once
#include <cmath>

namespace previewroll {
    template<class Hit,class Burst>
    void Advance(double& next,int& hits,int target,bool balloon,double now,double end,double interval,Hit hit,Burst burst) {
        if(!std::isfinite(next)||!std::isfinite(interval)||interval<=0) return;
        while(next<=now && next<end && (!balloon || hits<target)) {
            ++hits;
            const double following=next+interval;
            next=following>next ? following : end;
            hit();
            if(balloon && hits==target) burst();
        }
    }
}
