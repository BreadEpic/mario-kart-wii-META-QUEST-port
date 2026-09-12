// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gx.hpp"
#include <vector>
#include <cstring>
#include <cmath>
namespace aurora::gx {
struct NativeWheelArray {
    const void* source{};
    std::vector<uint8_t> bytes;
    std::array<float,12> modelView{};
    gfx::Range uploaded{};
};
inline std::vector<NativeWheelArray> nativeWheelArrays;
inline NativeWheelArray* native_wheel_array(const AttrArray& array) {
    if(g_gxState.currentPnMtx>=MaxPnMtx || g_gxState.vtxDesc[GX_VA_PNMTXIDX]==GX_DIRECT) return nullptr;
    for(auto& replacement:nativeWheelArrays) {
        if(array.data!=replacement.source || array.size>replacement.bytes.size()) continue;
        // These vehicle bodies have one rigid bone. A matching asset alone
        // would also rotate an opponent using the same vehicle/character.
        const auto* matrix=reinterpret_cast<const float*>(&g_gxState.pnMtx[g_gxState.currentPnMtx].pos);
        bool matches=true;
        for(int i=0;i<12;++i) if(std::abs(matrix[i]-replacement.modelView[i])>(i%4==3?0.1f:0.002f)) { matches=false; break; }
        if(matches) return &replacement;
    }
    return nullptr;
}
}
