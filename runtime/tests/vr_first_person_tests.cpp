// SPDX-License-Identifier: GPL-3.0-or-later
//
// The first-person VR camera's transform, tested without a guest. Everything
// here exercises ComputeFirstPersonAnchor, which turns the game's own view and
// kart matrices into the relocation Aurora composes onto each eye.

#include "vr/mkw_vr_first_person.h"
#include "vr/cockpit_stabilizer.h"
#include "vr/native_wheel_mesh.h"

#include <cmath>
#include <initializer_list>
#include <iostream>
#include <limits>

namespace {

using mkw::vr::ComputeFirstPersonAnchor;
using mkw::vr::kIdentityMtx34;
using mkw::vr::Mtx34;

int g_failures = 0;

void Check(bool condition, const char* what) {
    if (!condition) {
        ++g_failures;
        std::cerr << "FAILED: " << what << '\n';
    }
}

void CheckNear(float actual, float expected, const char* what, float tolerance = 1.0e-3f) {
    if (!(std::fabs(actual - expected) <= tolerance)) {
        ++g_failures;
        std::cerr << "FAILED: " << what << " (expected " << expected << ", got " << actual << ")\n";
    }
}

// out = matrix * (x, y, z, 1)
void Apply(const Mtx34& matrix, float x, float y, float z, float out[3]) {
    out[0] = matrix[0] * x + matrix[1] * y + matrix[2] * z + matrix[3];
    out[1] = matrix[4] * x + matrix[5] * y + matrix[6] * z + matrix[7];
    out[2] = matrix[8] * x + matrix[9] * y + matrix[10] * z + matrix[11];
}

// A view matrix for a camera at `eye` looking along -Z with no pitch or roll.
Mtx34 LevelViewAt(float x, float y, float z) {
    Mtx34 view = kIdentityMtx34;
    view[3] = -x;
    view[7] = -y;
    view[11] = -z;
    return view;
}

// The same, pitched down by `radians` about the view's X axis. Rows are the
// camera's axes in world space, which is what a world -> view matrix holds.
Mtx34 PitchedViewAt(float x, float y, float z, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    Mtx34 view{};
    view[0] = 1.0f;
    view[5] = c;
    view[6] = s;
    view[9] = -s;
    view[10] = c;
    view[3] = -(view[0] * x + view[1] * y + view[2] * z);
    view[7] = -(view[4] * x + view[5] * y + view[6] * z);
    view[11] = -(view[8] * x + view[9] * y + view[10] * z);
    return view;
}

Mtx34 KartAt(float x, float y, float z) {
    Mtx34 pose = kIdentityMtx34;
    pose[3] = x;
    pose[7] = y;
    pose[11] = z;
    return pose;
}

void TestNeutralInputsProduceIdentity() {
    Mtx34 anchor{};
    Check(ComputeFirstPersonAnchor(kIdentityMtx34, kIdentityMtx34, 0.0f, 0.0f, 0.0f,
                                   /*level_horizon=*/true, anchor),
          "a camera already at the head must produce an anchor");
    for (size_t i = 0; i < anchor.size(); ++i) {
        CheckNear(anchor[i], kIdentityMtx34[i], "neutral inputs must produce the identity anchor");
    }
}

void TestUnlevelledAnchorIsPureTranslation() {
    // Camera 5 m behind and 2 m above the origin, kart at the origin, head 1 m up.
    const Mtx34 view = LevelViewAt(0.0f, 2.0f, 5.0f);
    const Mtx34 kart = KartAt(0.0f, 0.0f, 0.0f);
    Mtx34 anchor{};
    Check(ComputeFirstPersonAnchor(view, kart, 0.0f, 1.0f, 0.0f, /*level_horizon=*/false, anchor),
          "an unlevelled anchor must be computable");

    // The head sits at (0, -1, -5) in view space, so the anchor's translation
    // is its negation.
    CheckNear(anchor[3], 0.0f, "no lateral offset");
    CheckNear(anchor[7], 1.0f, "the anchor cancels the head's -1 view-space height");
    CheckNear(anchor[11], 5.0f, "the anchor cancels the head's -5 view-space depth");

    // Rotation untouched, so a world point keeps its orientation and only shifts.
    float moved[3];
    Apply(anchor, 0.0f, -1.0f, -5.0f, moved);
    CheckNear(moved[0], 0.0f, "the head lands at the eye origin (x)");
    CheckNear(moved[1], 0.0f, "the head lands at the eye origin (y)");
    CheckNear(moved[2], 0.0f, "the head lands at the eye origin (z)");
}

void TestLevellingRemovesCameraPitch() {
    // A chase camera looking down at the kart, which is the ordinary Mario Kart
    // Wii case: first person must not inherit that downward tilt.
    const float pitch = 0.35f;
    const Mtx34 view = PitchedViewAt(0.0f, 2.0f, 5.0f, pitch);
    const Mtx34 kart = KartAt(0.0f, 0.0f, 0.0f);
    Mtx34 anchor{};
    Check(ComputeFirstPersonAnchor(view, kart, 0.0f, 1.0f, 0.0f, /*level_horizon=*/true, anchor),
          "a pitched camera must still produce an anchor");

    // The anchored camera's axes, expressed in world space: rows of A_rot times
    // the view rotation. Its forward is -row2, and it must be horizontal.
    const float worldUp[3]{0.0f, 1.0f, 0.0f};
    float rowInWorld[3][3];
    for (size_t row = 0; row < 3; ++row) {
        for (size_t axis = 0; axis < 3; ++axis) {
            // view's rows are the camera axes in world space, so a view-space
            // vector returns to world space through view's transpose.
            rowInWorld[row][axis] = anchor[row * 4 + 0] * view[0 * 4 + axis] +
                                    anchor[row * 4 + 1] * view[1 * 4 + axis] +
                                    anchor[row * 4 + 2] * view[2 * 4 + axis];
        }
    }
    const float forwardDotUp = -(rowInWorld[2][0] * worldUp[0] + rowInWorld[2][1] * worldUp[1] +
                                 rowInWorld[2][2] * worldUp[2]);
    CheckNear(forwardDotUp, 0.0f, "the levelled forward axis must be horizontal");
    const float rightDotUp = rowInWorld[0][0] * worldUp[0] + rowInWorld[0][1] * worldUp[1] +
                             rowInWorld[0][2] * worldUp[2];
    CheckNear(rightDotUp, 0.0f, "the levelled right axis must be horizontal");
    const float upDotUp = rowInWorld[1][0] * worldUp[0] + rowInWorld[1][1] * worldUp[1] +
                          rowInWorld[1][2] * worldUp[2];
    CheckNear(upDotUp, 1.0f, "the levelled up axis must be world up");

    // The head still lands exactly at the eye origin.
    float head[3];
    Apply(view, 0.0f, 1.0f, 0.0f, head);
    float moved[3];
    Apply(anchor, head[0], head[1], head[2], moved);
    CheckNear(moved[0], 0.0f, "the head lands at the eye origin under levelling (x)");
    CheckNear(moved[1], 0.0f, "the head lands at the eye origin under levelling (y)");
    CheckNear(moved[2], 0.0f, "the head lands at the eye origin under levelling (z)");
}

void TestAnchorRotationStaysOrthonormal() {
    // Straight down at the kart: the camera's own forward projects to nothing on
    // the horizon plane, so the heading has to be recovered from its up axis.
    const float kHalfPi = 1.57079632679f;
    for (const float pitch : {0.0f, 0.35f, kHalfPi, -kHalfPi, 3.0f}) {
        const Mtx34 view = PitchedViewAt(3.0f, 12.0f, -7.0f, pitch);
        Mtx34 anchor{};
        Check(ComputeFirstPersonAnchor(view, KartAt(3.0f, 0.0f, -20.0f), 0.1f, 1.0f, 0.2f,
                                       /*level_horizon=*/true, anchor),
              "every camera pitch must produce an anchor");
        for (size_t row = 0; row < 3; ++row) {
            for (size_t other = row; other < 3; ++other) {
                float dot = 0.0f;
                for (size_t axis = 0; axis < 3; ++axis) {
                    dot += anchor[row * 4 + axis] * anchor[other * 4 + axis];
                }
                CheckNear(dot, row == other ? 1.0f : 0.0f,
                          "the anchor's rotation must stay orthonormal");
            }
        }
    }
}

void TestNonFiniteInputIsRejected() {
    Mtx34 broken = kIdentityMtx34;
    broken[3] = std::numeric_limits<float>::infinity();
    Mtx34 anchor = kIdentityMtx34;
    anchor[3] = 1234.0f;
    Check(!ComputeFirstPersonAnchor(broken, kIdentityMtx34, 0.0f, 1.0f, 0.0f, true, anchor),
          "a non-finite view matrix must be rejected");
    CheckNear(anchor[3], 1234.0f, "a rejected anchor must leave the output untouched");
}

void TestDegenerateKartPoseIsRejected() {
    Mtx34 collapsed{};
    Mtx34 anchor{};
    // A zeroed view matrix has no world up to level against.
    Check(!ComputeFirstPersonAnchor(collapsed, kIdentityMtx34, 0.0f, 1.0f, 0.0f, true, anchor),
          "a collapsed view matrix must be rejected");
}

} // namespace

int main() {
    using namespace mkw::vr;
    CockpitStabilizer stable;
    auto simulation=KartAt(100,20,300);
    auto seated=stable.Update(simulation,false,1.0f/60);
    CheckNear(seated[3],100,"cockpit follows simulation position before impact");
    auto hit=simulation; hit[3]=180;hit[7]=160;hit[2]=1;hit[10]=0;
    for(int i=0;i<90;++i) seated=stable.Update(hit,true,1.0f/60);
    CheckNear(seated[3],180,"impact keeps cockpit attached to kart horizontally");
    CheckNear(seated[7],160,"cockpit follows simulation elevation without visual shake");
    CheckNear(seated[2],0,"impact cannot spin cockpit yaw");
    seated=stable.Update(hit,false,1.0f/60);
    CheckNear(seated[3],180,"damage recovery cannot leave a positional offset");
    Check(seated[2]>0&&seated[2]<1,"damage recovery still blends orientation");
    for(int i=0;i<150;++i) seated=stable.Update(hit,false,1.0f/60);
    CheckNear(seated[3],180,"seat returns to kart after impact",0.1f);
    CheckNear(seated[2],1,"seat returns to driving direction",0.01f);
    hit[3]=10000;
    seated=stable.Update(hit,true,1.0f/60);
    CheckNear(seated[3],10000,"respawn relocation cannot leave camera behind");
    const auto diorama = ComputeDioramaAnchor(2000.0f, 1600.0f);
    const auto eye = detail::TransformPoint(diorama, 0.0f, 1600.0f, 2000.0f);
    CheckNear(eye.x, 0.0f, "diorama eye x");
    CheckNear(eye.y, 0.0f, "diorama eye y");
    CheckNear(eye.z, 0.0f, "diorama eye z");
    CheckNear(diorama[7], 0.0f, "diorama aims at scene centre");
    Check(diorama[11] < -2500.0f, "diorama is distant from scene centre");
    Mtx34 centeredDiorama{};
    const Mtx34 orbitView{0, 0, 1, 140, 0, 1, 0, -90, -1, 0, 0, -200};
    const auto testKart = KartAt(300, 20, -170);
    Check(ComputeKartDioramaAnchor(orbitView, testKart, 1600, 1200, centeredDiorama),
          "diorama accepts a chase camera with an independent heading");
    const auto recordedKart = detail::TransformPoint(orbitView, 300, 20, -170);
    const auto target = detail::TransformPoint(centeredDiorama, recordedKart.x, recordedKart.y, recordedKart.z);
    CheckNear(target.x, 0, "kart stays centred horizontally during chase orbit");
    CheckNear(target.y, 0, "kart stays centred vertically during chase orbit");
    CheckNear(target.z, -2000, "diorama distance is measured from kart, not chase camera");
    Mtx34 driver{};
    Check(ComputeFirstPersonAnchor(kIdentityMtx34, kIdentityMtx34, 0, 110, 35, true, driver, true),
          "driver follows kart heading");
    const auto ahead = detail::TransformPoint(driver, 0, 110, 135);
    CheckNear(ahead.z, -100, "kart forward must be in front of driver");
    CheckNear(ahead.y, 0, "driver sits at requested head height");
    Check(NextCameraMode(CameraMode::Game) == CameraMode::FirstPerson &&
          NextCameraMode(CameraMode::FirstPerson) == CameraMode::Far &&
          NextCameraMode(CameraMode::Far) == CameraMode::Game, "camera cycle order");
    CameraClickLatch click;
    Check(!click.Update(true, true), "held button on connect must not switch");
    Check(!click.Update(true, false), "release arms camera switch");
    Check(click.Update(true, true), "press switches once");
    Check(!click.Update(true, true), "holding does not repeat");
    Check(!click.Update(false, false), "focus loss resets button");
    Check(!click.Update(true, true), "held button after focus loss does not switch");
    click.Update(true, false);
    Check(click.Update(true, true), "next deliberate press switches again");
    TestNeutralInputsProduceIdentity();
    TestUnlevelledAnchorIsPureTranslation();
    TestLevellingRemovesCameraPitch();
    TestAnchorRotationStaysOrthonormal();
    TestNonFiniteInputIsRejected();
    TestDegenerateKartPoseIsRejected();
    // Character eye vertices use face-local axes (+X forward, -Y up), not
    // the kart axes. Baby Mario and Bowser need different eye/neck offsets.
    Mtx34 faceSmall{0,0,1,0, 0,-1,0,48.31057f, 1,0,0,-0.61309f};
    Mtx34 faceLarge{0,0,1,0, 0,-1,0,139.83763f, 1,0,0,42.46158f};
    auto placement=kIdentityMtx34;
    std::array<float,3> smallEye{},largeEye{};
    Check(ComputeDriverEyeFromBounds(faceSmall,placement,{5.1962f,-31.3396f,-16.6544f},
        {20.2551f,5.7841f,16.5134f},smallEye),"small character eye geometry accepted");
    Check(ComputeDriverEyeFromBounds(faceLarge,placement,{33.5831f,-29.8363f,-22.3588f},
        {48.5397f,-17.5208f,22.3588f},largeEye),"large character eye geometry accepted");
    CheckNear(smallEye[1],61.08832f,"baby eye height follows face geometry");
    CheckNear(largeEye[1],163.51618f,"large character eye height follows face geometry");
    CheckNear(largeEye[2],83.52298f,"long face moves eyes forward instead of using Mario's offset");
    placement[7]=-30; placement[11]=-49;
    Check(ComputeDriverEyeFromBounds(faceLarge,placement,{33.5831f,-29.8363f,-22.3588f},
        {48.5397f,-17.5208f,22.3588f},largeEye),"vehicle-specific driver placement accepted");
    CheckNear(largeEye[1],133.51618f,"vehicle seat translation is applied once");
    auto bikeWorld=kIdentityMtx34;
    bikeWorld[3]=1000; bikeWorld[7]=300; bikeWorld[11]=-2000;
    auto ridingFace=bikeWorld;
    ridingFace[7]+=85; ridingFace[11]+=12;
    std::array<float,3> ridingEye{};
    Check(ComputeSeatedEye(ridingFace,bikeWorld,{0,8,4},ridingEye),"evaluated bike posture is measurable");
    CheckNear(ridingEye[1],93,"bike posture replaces low bind-pose height");
    CheckNear(ridingEye[2],16,"body transform removed without repeating seat translation");
    SeatedEyeReference ridingReference;
    for(int i=0;i<8;++i) ridingReference.Observe(ridingEye,true,false);
    Check(ridingReference.valid,"normal camera pre-calibrates the riding posture");
    ridingReference.Observe({0,20,0},false,true);
    CheckNear(ridingReference.value[1],93,"damage cannot change calibrated height");
    for(int i=0;i<30;++i) ridingReference.Observe({0,120,40},true,true);
    CheckNear(ridingReference.value[1],93,"cockpit stays stable during character animations");
    ridingReference={};
    Check(!ridingReference.valid,"new race clears previous character posture");
    Check(!ComputeDriverEyeFromBounds(faceSmall,placement,{1,0,0},{0,1,1},smallEye),
        "invalid eye bounds fall back without poisoning the camera");
    const auto nativeWheel=ComputeNativeWheelGeometry(driver,{-24,80,75},{24,80,75},100);
    Check(nativeWheel.valid,"authored hand targets produce a native wheel");
    CheckNear(nativeWheel.radius,0.24f,"native radius follows vehicle hand spacing");
    CheckNear(nativeWheel.center[1],-0.30f,"native wheel follows measured driver eye height");
    CheckNear(nativeWheel.center[2],-0.40f,"native wheel is in front of the seat");
    const auto grip=detail::TransformPoint(driver,24,80,75);
    const auto mapped=nativeWheel.ToWheel({grip.x/100,grip.y/100,grip.z/100,1,true});
    CheckNear(mapped.x,-0.24f,"kart positive X maps to the driver's left grip");
    CheckNear(mapped.y,SteeringWheel::Height,"native grip vertical origin");
    CheckNear(mapped.z,SteeringWheel::Depth,"native grip depth origin");
    const Mtx34 bikeBasis{-1,0,0,0, 0,1,0,0, 0,0,-1,0};
    auto handlePose=bikeBasis; handlePose[7]=-40;handlePose[11]=-60;
    const auto bars=ComputeNativeHandlebarGeometry(handlePose,bikeBasis,{-25,0,0},{25,0,0},100);
    Check(bars.valid,"bike handle geometry available");
    CheckNear(bars.center[1],-0.4f,"bike grab height comes from handle part");
    CheckNear(bars.center[2],-0.6f,"bike grab depth comes from handle part rather than chassis");
    CheckNear(bars.radius,0.25f,"handle width follows the selected bike");
    CheckNear(bars.up[2],-1,"bike steering plane points forward");
    // The handle may already be animated by the game; its rotation must not
    // rotate the reference frame used to interpret the next hand gesture.
    handlePose[0]=-0.7071f;handlePose[2]=-0.7071f;handlePose[8]=0.7071f;handlePose[10]=-0.7071f;
    const auto turnedBars=ComputeNativeHandlebarGeometry(handlePose,bikeBasis,{-25,0,0},{25,0,0},100);
    CheckNear(turnedBars.right[0],1,"animated handle yaw cannot feed back into VR steering");
    auto banked=kIdentityMtx34;
    banked[0]=0.70710678f;banked[1]=-0.70710678f;
    banked[4]=0.70710678f;banked[5]=0.70710678f;
    banked[3]=100;banked[7]=20;
    auto localBar=kIdentityMtx34;localBar[7]=30;localBar[11]=40;
    const auto movingBar=ComposeMtx(banked,localBar);
    Mtx34 inverseBank{},inverseMoving{};
    Check(InvertMtx(banked,inverseBank)&&InvertMtx(movingBar,inverseMoving),"bike transforms invert");
    const auto levelBar=ComposeMtx(ComposeMtx(kIdentityMtx34,inverseBank),movingBar);
    CheckNear(levelBar[7],30,"banking cannot move handlebar height");
    CheckNear(levelBar[3],0,"banking cannot move handlebars sideways");
    const auto renderCorrection=ComposeMtx(inverseMoving,levelBar);
    const auto renderedBar=ComposeMtx(movingBar,renderCorrection);
    CheckNear(renderedBar[7],levelBar[7],"native model and grab volume share the stabilized handle pose");
    CheckNear(EyeBehindControls(80,40,100,20),-6,"seat moves behind controls with usable clearance");
    CheckNear(EyeBehindControls(-50,40,100,20),-50,"already comfortable seat stays in place");
    {
        SeatedEyeReference reference;
        for(int i=0;i<10;++i) reference.Observe({0,140,20},NeutralPlayerScale({1,1,1}),true);
        for(int i=0;i<30;++i) reference.Observe({0,56,8},NeutralPlayerScale({.4f,.4f,.4f}),true);
        CheckNear(reference.value[1],140,"lightning in chase camera never replaces neutral seat");
        reference.Observe({0,140,20},true,true);
        CheckNear(reference.value[1],140,"camera switches and recovery preserve neutral seat");
        auto body=kIdentityMtx34; body[7]=-100; body[11]=-50;
        auto normal=ComputeNativeWheelGeometry(body,{-18,70,8},{18,70,8},100);
        body[7]*=.4f;body[11]*=.4f;
        auto small=ComputeNativeWheelGeometry(ScaleModelBasis(body,{.4f,.4f,.4f}),{-18,70,8},{18,70,8},40);
        Check(normal.valid && small.valid,"lightning keeps native steering targets valid");
        for(int i=0;i<3;++i) CheckNear(small.center[i],normal.center[i],"scaled wheel remains at the same hand position");
        CheckNear(small.radius,normal.radius,"scaled wheel remains grabbable at the same radius");
    }
    CheckNear(CharacterCockpitScale(80),1,"normal and small drivers preserve baseline world scale");
    const float giantScale=CharacterCockpitScale(200);
    CheckNear(200/(100*giantScale),1,"large driver cockpit height is normalized in metres");
    const float lightning=ValidPlayerScale(0.3f);
    CheckNear(200*lightning,60,"lightning lowers the world-space eye position");
    CheckNear(100*giantScale*lightning,60,"lightning scales headset translation and stereo world scale together");
    CheckNear(ValidPlayerScale(0),1,"uninitialized player scale is ignored");
    SteeringWheel nativeInput;
    std::array<WheelHand,2> nativeHands{};
    // Right hand clockwise from the headset's viewpoint, independently of
    // the authored kart X axis. This catches the original mirrored input.
    for(int step=0;step<=90;++step) {
        const float angle=-step*3.14159265f/180;
        WheelHand hand{nativeWheel.center[0]+0.24f*std::cos(angle),
            nativeWheel.center[1]+0.24f*std::sin(angle),nativeWheel.center[2],1,true};
        nativeHands[1]=nativeWheel.ToWheel(hand);
        nativeInput.Update(nativeHands,true,1.0f/90,0.24f);
    }
    Check(nativeInput.Update(nativeHands,true,1.0f/90,0.24f).steering>0.9f,
        "physical clockwise motion steers right in native mode");
    std::vector<detail::Vec3> mesh;
    for(int i=0;i<32;++i) {
        const float a=i*6.2831853f/32;
        mesh.push_back({13*std::cos(a),30+13*std::sin(a),-8});
    }
    mesh.push_back({0,30,40}); // dashboard, outside wheel plane
    const auto original=mesh;
    Check(RotateNativeWheelVertices(mesh,{0,30,-8},13,1.570796327f)==32,
          "only steering wheel disc vertices rotate");
    CheckNear(mesh[0].x,0,"native wheel quarter turn X");
    CheckNear(mesh[0].y,43,"native wheel quarter turn Y");
    CheckNear(mesh.back().z,40,"dashboard is not deformed");
    CheckNear(original[0].x,13,"original mesh retained for other vehicles");
    // Spins around all three axes must animate the chassis without taking the
    // held wheel with it. Exercise the render-copy correction at normal and
    // lightning scale, with an independent physical steering angle.
    for(float scale : {1.0f,0.3f}) for(int axis=0;axis<3;++axis) for(int step=0;step<=36;++step) {
        const float a=step*6.2831853f/36,c=std::cos(a),s=std::sin(a);
        Mtx34 spin=kIdentityMtx34;
        const int u=(axis+1)%3,v=(axis+2)%3;
        spin[u*4+u]=c;spin[u*4+v]=-s;spin[v*4+u]=s;spin[v*4+v]=c;
        spin[3]=17;spin[7]=24;spin[11]=-31;
        Mtx34 stable=kIdentityMtx34;stable[3]=17;stable[7]=24;stable[11]=-31;
        stable=ScaleModelBasis(stable,{scale,scale,scale});
        const auto rendered=ScaleModelBasis(spin,{scale,scale,scale});
        Mtx34 inverseRendered{};
        Check(InvertMtx(rendered,inverseRendered),"animated body is invertible at lightning scale");
        const auto correction=ComposeMtx(inverseRendered,stable);
        auto corrected=original,turned=original;
        Check(RotateNativeWheelVertices(corrected,{0,30,-8},13,0.6f,&correction)==32,
              "spin correction selects only wheel vertices");
        RotateNativeWheelVertices(turned,{0,30,-8},13,0.6f);
        for(size_t i=0;i<32;++i) {
            const auto actual=detail::TransformPoint(rendered,corrected[i].x,corrected[i].y,corrected[i].z);
            const auto expected=detail::TransformPoint(stable,turned[i].x,turned[i].y,turned[i].z);
            CheckNear(actual.x,expected.x,"wheel stays stable during spin X");
            CheckNear(actual.y,expected.y,"wheel stays stable during spin Y");
            CheckNear(actual.z,expected.z,"wheel stays stable during spin Z");
        }
        CheckNear(corrected.back().x,original.back().x,"chassis keeps its own animation X");
        CheckNear(corrected.back().y,original.back().y,"chassis keeps its own animation Y");
        CheckNear(corrected.back().z,original.back().z,"chassis keeps its own animation Z");
    }
    Check(!ComputeNativeWheelGeometry(driver,{0,0,0},{0,0,0},100).valid,"missing hand span falls back");
    Check(!ComputeNativeWheelGeometry(driver,{-24,80,75},{24,80,75},0).valid,"invalid world scale falls back");
    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    return 0;
}
