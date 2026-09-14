#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
namespace mkw::vr {
class FrameStatistics {
    std::array<float,512> samples_{};
    size_t size_=0, next_=0;
public:
    void Add(float milliseconds) {
        if(!std::isfinite(milliseconds)||milliseconds<0) return;
        samples_[next_]=milliseconds;next_=(next_+1)%samples_.size();size_=std::min(size_+1,samples_.size());
    }
    float Percentile(float fraction) const {
        if(!size_) return 0;
        auto sorted=samples_;
        std::sort(sorted.begin(),sorted.begin()+size_);
        const auto index=static_cast<size_t>(std::ceil(std::clamp(fraction,0.0f,1.0f)*size_));
        return sorted[index?index-1:0];
    }
};
}
