// SPDX-License-Identifier: GPL-3.0-or-later
#include "vr/steering_wheel.h"
#include <iostream>
#include <limits>
using namespace mkw::vr;
int failures=0;
void Check(bool ok,const char* message) { if(!ok) { std::cerr<<message<<'\n';++failures; } }
WheelHand Rim(float angle,float squeeze=1) {
    return {SteeringWheel::Radius*std::cos(angle),SteeringWheel::Height+SteeringWheel::Radius*std::sin(angle),
        SteeringWheel::Depth,squeeze,true};
}
int main() {
    constexpr float pi=3.141592653589793f,dt=1.0f/90;
    SteeringWheel wheel;
    std::array<WheelHand,2> hands{Rim(pi),Rim(0)};
    auto state=wheel.Update(hands,true,dt);
    Check(state.held[0]&&state.held[1]&&state.steering==0,"grab both hands without a steering jump");
    for(int i=1;i<=90;++i) {
        hands={Rim(pi-i*pi/180),Rim(-i*pi/180)};
        state=wheel.Update(hands,true,dt);
    }
    for(int i=0;i<30;++i) state=wheel.Update(hands,true,dt);
    Check(state.steering>0.99f,"clockwise quarter turn produces full right steering");
    hands[1].squeeze=0;
    state=wheel.Update(hands,true,dt);
    Check(state.held[0]&&!state.held[1]&&state.steering>0.98f,"one hand can release without losing the other grip");
    hands[0].tracked=false;
    state=wheel.Update(hands,true,dt);
    Check(!state.held[0]&&!state.held[1],"tracking loss releases the wheel immediately");
    hands[0].tracked=true;
    state=wheel.Update(hands,true,dt);
    Check(!state.held[0],"tracking recovery needs a fresh squeeze");
    hands[0].squeeze=0;wheel.Update(hands,true,dt);hands[0].squeeze=1;
    Check(wheel.Update(hands,true,dt).held[0],"release and squeeze allows reacquisition");
    Check(wheel.Update(hands,false,dt).steering==0,"leaving cockpit clears steering");
    wheel={};hands={Rim(pi),Rim(0)};wheel.Update(hands,true,dt);
    hands[0].z=SteeringWheel::Depth-0.70f;hands[1].z=SteeringWheel::Depth+0.70f;
    state=wheel.Update(hands,true,dt);
    Check(state.held[0]&&state.held[1],"arcade gestures allow 70cm forward and backward travel");
    hands[1].x=0;hands[1].y=SteeringWheel::Height;
    for(int i=0;i<30;++i) state=wheel.Update(hands,true,dt);
    Check(state.held[1],"hand at wheel center retains grip");
    const float before=state.angle;
    hands[1]=Rim(pi);
    state=wheel.Update(hands,true,dt);
    Check(state.held[1]&&std::abs(state.angle-before)<0.001f,"crossing center cannot flip steering 180 degrees");
    hands[0].z=SteeringWheel::Depth-0.85f;
    Check(!wheel.Update(hands,true,dt).held[0],"hands beyond the expanded reach still release");
    wheel={};hands={Rim(pi,0),Rim(0,0)};
    hands[0].z=0;hands[0].squeeze=1;
    Check(!wheel.Update(hands,true,dt).held[0],"grip far from wheel does not grab it");
    hands[0]=Rim(pi);Check(!wheel.Update(hands,true,dt).held[0],"moving an already squeezed hand onto rim cannot grab");
    hands[0].squeeze=0;wheel.Update(hands,true,dt);hands[0].squeeze=1;wheel.Update(hands,true,dt);
    for(int i=1;i<=90;++i) { hands[0]=Rim(pi+i*pi/180);state=wheel.Update(hands,true,dt); }
    for(int i=0;i<30;++i) state=wheel.Update(hands,true,dt);
    Check(state.steering < -0.99f,"counterclockwise wrap through pi produces full left steering");
    hands[0].x=std::numeric_limits<float>::quiet_NaN();
    state=wheel.Update(hands,true,std::numeric_limits<float>::quiet_NaN());
    Check(!state.held[0]&&std::isfinite(state.steering),"invalid tracking/time cannot poison wheel state");
    std::cout<<(failures?"FAIL":"PASS")<<": steering wheel scenarios\n";
    return failures?1:0;
}
