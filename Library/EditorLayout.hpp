#pragma once
#include <algorithm>
#include <cmath>

namespace editlayout {
    inline float PageOffset(float position,float width,double measureRatio) {
        if(!std::isfinite(position)||!std::isfinite(width)||width<=0||!std::isfinite(measureRatio)) return 0;
        const float last=width*static_cast<float>(std::max(0.0,std::ceil(measureRatio)-1));
        return std::clamp(std::floor(position/width)*width,0.0f,last);
    }
}
