#include "vr/frame_statistics.h"
#include "vr/adaptive_resolution.h"
#include <limits>
int main() {
    mkw::vr::AdaptiveResolution resolution;
    for(int i=0;i<20;++i) resolution.Observe(40,90,true);
    if(std::abs(resolution.Scale()-.7f)>.001f) return 4;
    for(int i=0;i<20;++i) resolution.Observe(90,90,true);
    if(std::abs(resolution.Scale()-1)>.001f) return 5;
    resolution.Observe(40,90,true);resolution.Observe(40,90,false);
    if(resolution.Scale()!=1) return 6;
    mkw::vr::FrameStatistics stats;
    if(stats.Percentile(.95f)!=0) return 1;
    for(int i=1;i<=100;++i) stats.Add(float(i));
    stats.Add(std::numeric_limits<float>::quiet_NaN());stats.Add(-1);
    if(stats.Percentile(.95f)!=95||stats.Percentile(.99f)!=99) return 2;
    for(int i=0;i<512;++i) stats.Add(11);
    return stats.Percentile(.99f)==11?0:3;
}
