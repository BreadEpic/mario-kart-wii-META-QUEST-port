// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gx.hpp"
#include <vector>
#include <cstring>
#include <cmath>
#include <atomic>
#include <aurora/native_wheel_match.hpp>
namespace aurora::gx {
struct NativeWheelArray {
    const void* source{};
    std::vector<uint8_t> bytes;
    std::array<float,12> modelView{};
    gfx::Range uploaded{};
};
inline std::vector<NativeWheelArray> nativeWheelArrays;
inline uint32_t nativeWheelMatches=0;
inline std::atomic<uint32_t> nativeWheelLastMatches{0};
inline bool native_wheel_source(const void* source) {
    for(const auto& replacement:nativeWheelArrays) if(source==replacement.source) return true;
    return false;
}
inline NativeWheelArray* native_wheel_array(const AttrArray& array,const uint8_t* vertices,
    uint32_t vertexBytes,uint32_t vertexStride,uint32_t positionOffset) {
    const bool indexedMatrix=g_gxState.vtxDesc[GX_VA_PNMTXIDX]==GX_DIRECT;
    if(!indexedMatrix && g_gxState.currentPnMtx>=MaxPnMtx) return nullptr;
    for(auto& replacement:nativeWheelArrays) {
        if(array.data!=replacement.source || array.size>replacement.bytes.size()) continue;
        // A matching asset alone would also animate an opponent. Multi-joint
        // models need a per-position ownership check, not a blanket exclusion.
        uint16_t matching=0;
        for(uint32_t slot=0;slot<MaxPnMtx;++slot) {
            if(!indexedMatrix && slot!=g_gxState.currentPnMtx) continue;
            const auto* matrix=reinterpret_cast<const float*>(&g_gxState.pnMtx[slot].pos);
            bool matches=true;
            for(int i=0;i<12;++i) {
                uint32_t bits;std::memcpy(&bits,&matrix[i],4);
                if((bits&0x7f800000u)==0x7f800000u ||
                    std::abs(matrix[i]-replacement.modelView[i])>(i%4==3?0.1f:0.002f)) { matches=false;break; }
            }
            if(matches) matching|=uint16_t(1u<<slot);
        }
        if(!matching) continue;
        if(indexedMatrix && !NativeWheelDrawMatches(
            {static_cast<const uint8_t*>(array.data),array.size},
            {replacement.bytes.data(),array.size},array.stride,
            {vertices,vertexBytes},vertexStride,positionOffset,
            g_gxState.vtxDesc[GX_VA_POS]==GX_INDEX8?1:2,matching)) continue;
        ++nativeWheelMatches;return &replacement;
    }
    return nullptr;
}
}
