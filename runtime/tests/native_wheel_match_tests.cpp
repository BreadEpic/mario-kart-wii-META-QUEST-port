// SPDX-License-Identifier: GPL-3.0-or-later
#include <aurora/native_wheel_match.hpp>
#include <array>
#include <iostream>
int main() {
    int failures=0;
    const auto check=[&](bool value,const char* message) { if(!value) { ++failures;std::cerr<<message<<'\n'; } };
    std::array<uint8_t,36> original{},replacement{};
    replacement[12]=7; // Position 1 is on the wheel; 0 and 2 belong to the body/wing.
    // PN matrix byte, texture matrix byte, big-endian position index.
    std::array<uint8_t,12> vertices{0,30,0,0, 0,30,0,1, 3,30,0,2};
    const auto matches=[&](uint16_t mask) {
        return aurora::NativeWheelDrawMatches(original,replacement,12,vertices,4,2,2,mask);
    };
    check(matches(1),"mixed body/wing draw accepts wheel positions on the local body");
    check(!matches(2),"matching only an unrelated wing joint cannot animate wheel");
    check(!matches(0),"same mesh on opponent without local matrix is rejected");
    vertices[4]=6;
    check(!matches(1),"wheel position under another matrix is rejected");
    check(matches(4),"local body can occupy a different palette slot");
    vertices[4]=1;
    check(!matches(1),"malformed matrix selector rejected");
    vertices[4]=0;vertices[11]=3;
    check(!matches(1),"out-of-range position index rejected");
    vertices[11]=2;
    replacement[24]=1;
    check(!matches(1),"selection touching another joint cannot deform that joint");
    replacement[24]=0;
    std::array<uint8_t,4> indexed8{0,1, 3,2};
    check(aurora::NativeWheelDrawMatches(original,replacement,12,indexed8,2,1,1,1),"8-bit position indices supported");
    check(!aurora::NativeWheelDrawMatches(original,replacement,12,indexed8,2,2,1,1),"invalid vertex layout rejected");
    check(!aurora::NativeWheelDrawMatches(original,original,12,indexed8,2,1,1,1),"unchanged positions are not counted as animation");
    std::cout<<(failures?"FAIL":"PASS")<<": native wheel indexed matrix ownership\n";
    return failures?1:0;
}
