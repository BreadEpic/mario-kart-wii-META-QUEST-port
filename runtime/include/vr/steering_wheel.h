// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <bit>
#include <cstdint>

namespace mkw::vr {
// Metres in a fixed seated frame: +X right, +Y up, -Z forward.
struct WheelHand { float x=0, y=0, z=0, squeeze=0; bool tracked=false; };
struct WheelState {
    float angle=0, steering=0;
    std::array<bool, 2> held{};
};
class SteeringWheel {
public:
    static constexpr float Radius=0.18f, Height=-0.30f, Depth=-0.42f;
    WheelState Update(const std::array<WheelHand, 2>& hands, bool active, float dt) {
        const auto finite=[](float value) { return (std::bit_cast<uint32_t>(value)&0x7f800000u)!=0x7f800000u; };
        if (!finite(dt)) dt=0;
        dt=std::clamp(dt, 0.0f, 0.05f);
        float sum=0; int count=0;
        for (int h=0; h<2; ++h) {
            const auto& p=hands[h];
            const bool valid=p.tracked&&finite(p.x)&&finite(p.y)&&finite(p.z)&&finite(p.squeeze);
            const bool down=finite(p.squeeze)&&p.squeeze > (pressed_[h] ? 0.35f : 0.65f);
            if(!valid) { state_.held[h]=false; pressed_[h]=down; continue; }
            const float y=p.y-Height, z=p.z-Depth;
            const float radial=std::hypot(p.x,y);
            const bool near_rim=std::abs(radial-Radius)<0.10f && std::abs(z)<0.20f;
            const float angle=-std::atan2(y,p.x);
            if (!active || !down || radial>Radius+0.30f || std::abs(z)>0.80f)
                state_.held[h]=false;
            else if (!pressed_[h] && near_rim) {
                state_.held[h]=true;
                last_[h]=angle;
                target_[h]=state_.angle;
                center_[h]=false;
            }
            if (state_.held[h]) {
                // Angle is undefined at the hub. Keep ownership and the last
                // steering value, then rebase on exit to avoid a 180-degree jump.
                if (radial<0.05f) center_[h]=true;
                else {
                    if (!center_[h]) target_[h]+=std::remainder(angle-last_[h], 6.283185307f);
                    last_[h]=angle;
                    center_[h]=false;
                }
                target_[h]=std::clamp(target_[h],-1.570796327f,1.570796327f);
                sum+=target_[h]; ++count;
            }
            pressed_[h]=down;
        }
        const float target=count ? sum/count : 0;
        state_.angle += (target-state_.angle)*(1-std::exp(-dt*(count?24.0f:12.0f)));
        state_.steering=active ? std::clamp(state_.angle/1.570796327f,-1.0f,1.0f) : 0;
        if (!active) state_.angle=0;
        return state_;
    }
private:
    WheelState state_{};
    std::array<bool,2> pressed_{};
    std::array<bool,2> center_{};
    std::array<float,2> last_{}, target_{};
};
} // namespace mkw::vr
