// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "vr/vr_controls.h"
#include "vr/steering_wheel.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace mkw::vr {

// A row-major affine 3x4, the same shape and convention as an NW4R/GX Mtx and
// as Aurora's Mat3x4: a point is transformed as out = M * (p, 1).
using Mtx34 = std::array<float, 12>;

inline constexpr Mtx34 kIdentityMtx34{
    1.0f, 0.0f, 0.0f, 0.0f, //
    0.0f, 1.0f, 0.0f, 0.0f, //
    0.0f, 0.0f, 1.0f, 0.0f,
};

// Thread-safe live switch: game -> first person -> far -> game.
void MkwVRCycleCamera() noexcept;
CameraMode MkwVRGetCameraMode() noexcept;
void MkwVRSetCameraMode(CameraMode mode) noexcept;

// Where the driver's head sits in the kart's own frame, in metres. The kart
// frame is the EGG convention: +x right, +y up, +z forward.
struct FirstPersonHeadOffsets {
    float right = 0.0f;
    float up = 1.0f;
    float forward = 0.0f;
};

// The camera relocation published to Aurora for one guest frame: a transform
// from the game's recorded view space into the space the headset renders from.
struct FirstPersonAnchor {
    Mtx34 anchor_from_scene = kIdentityMtx34;
    bool valid = false;
    uint64_t guest_frame_index = 0;
    float units_per_meter = 0.0f;
    WheelGeometry native_wheel{};
    bool bike=false;
};

// ---------------------------------------------------------------------------
// Pure math. Header-only and free of guest access, so it is directly testable.
// ---------------------------------------------------------------------------

namespace detail {

inline constexpr float kAnchorEpsilon = 1.0e-6f;

inline bool IsFiniteFloat(const float* value) noexcept {
    // The runtime is built with -ffast-math, which permits the compiler to fold
    // std::isfinite to true. Inspect the object representation instead, the way
    // the presentation policy validates its own floats.
    uint32_t bits = 0;
    std::memcpy(&bits, value, sizeof(bits));
    return (bits & 0x7F800000u) != 0x7F800000u;
}

inline bool IsFiniteMtx34(const Mtx34& value) noexcept {
    for (const float& element : value) {
        if (!IsFiniteFloat(&element)) {
            return false;
        }
    }
    return true;
}

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline float Dot(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 Cross(const Vec3& a, const Vec3& b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline bool Normalize(Vec3& value) noexcept {
    const float length_squared = Dot(value, value);
    if (!IsFiniteFloat(&length_squared) || !(length_squared > kAnchorEpsilon)) {
        return false;
    }
    const float inverse_length = 1.0f / std::sqrt(length_squared);
    value.x *= inverse_length;
    value.y *= inverse_length;
    value.z *= inverse_length;
    return true;
}

// out = matrix * (x, y, z, 1)
inline Vec3 TransformPoint(const Mtx34& matrix, float x, float y, float z) noexcept {
    return {
        matrix[0] * x + matrix[1] * y + matrix[2] * z + matrix[3],
        matrix[4] * x + matrix[5] * y + matrix[6] * z + matrix[7],
        matrix[8] * x + matrix[9] * y + matrix[10] * z + matrix[11],
    };
}

} // namespace detail

inline Mtx34 ComposeMtx(const Mtx34& a,const Mtx34& b) noexcept {
    Mtx34 out{};
    for(int row=0;row<3;++row) for(int col=0;col<4;++col) {
        out[row*4+col]=col==3?a[row*4+3]:0;
        for(int k=0;k<3;++k) out[row*4+col]+=a[row*4+k]*b[k*4+col];
    }
    return out;
}
inline bool InvertMtx(const Mtx34& m,Mtx34& out) noexcept {
    if(!detail::IsFiniteMtx34(m)) return false;
    const detail::Vec3 a{m[0],m[4],m[8]},b{m[1],m[5],m[9]},c{m[2],m[6],m[10]};
    const auto x=detail::Cross(b,c),y=detail::Cross(c,a),z=detail::Cross(a,b);
    const float det=detail::Dot(a,x);
    if(!detail::IsFiniteFloat(&det) || std::abs(det)<1e-6f) return false;
    out={x.x/det,x.y/det,x.z/det,0,y.x/det,y.y/det,y.z/det,0,z.x/det,z.y/det,z.z/det,0};
    for(int row=0;row<3;++row) out[row*4+3]=-(out[row*4]*m[3]+out[row*4+1]*m[7]+out[row*4+2]*m[11]);
    return detail::IsFiniteMtx34(out);
}
inline float EyeBehindControls(float eyeForward,float controlsForward,float units,float radius) noexcept {
    const float clearance=std::clamp(0.40f+radius/units*0.3f,0.45f,0.65f)*units;
    return std::min(eyeForward,controlsForward-clearance);
}

inline float CharacterCockpitScale(float eyeHeight) noexcept {
    if(!detail::IsFiniteFloat(&eyeHeight)) return 1;
    return std::clamp(eyeHeight/100.0f,1.0f,2.5f);
}
inline float ValidPlayerScale(float scale) noexcept {
    return detail::IsFiniteFloat(&scale) && scale>=0.1f && scale<=4.0f ? scale:1.0f;
}
inline Mtx34 ScaleModelBasis(Mtx34 pose,const std::array<float,3>& scale) noexcept {
    for(int row=0;row<3;++row) for(int col=0;col<3;++col) pose[row*4+col]*=scale[col];
    return pose;
}
inline bool NeutralPlayerScale(const std::array<float,3>& scale) noexcept {
    for(float value:scale) if(!detail::IsFiniteFloat(&value) || std::abs(value-1)>0.001f) return false;
    return true;
}

// Eye position resources are in the face bone's local coordinates, whose axes
// differ between characters. Transform their centre through the complete bind
// matrix before applying the vehicle-specific driver placement.
inline bool ComputeDriverEyeFromBounds(const Mtx34& face, const Mtx34& placement,
        detail::Vec3 minimum, detail::Vec3 maximum, std::array<float,3>& eye) noexcept {
    if(!detail::IsFiniteMtx34(face) || !detail::IsFiniteMtx34(placement)) return false;
    const std::array<float,6> bounds{minimum.x,minimum.y,minimum.z,maximum.x,maximum.y,maximum.z};
    for(const auto& value:bounds) if(!detail::IsFiniteFloat(&value) || std::abs(value)>500) return false;
    if(minimum.x>maximum.x || minimum.y>maximum.y || minimum.z>maximum.z) return false;
    const auto model=detail::TransformPoint(face,(minimum.x+maximum.x)*0.5f,
        (minimum.y+maximum.y)*0.5f,(minimum.z+maximum.z)*0.5f);
    const auto seat=detail::TransformPoint(placement,model.x,model.y,model.z);
    const std::array<float,3> result{seat.x,seat.y,seat.z};
    for(const auto& value:result) if(!detail::IsFiniteFloat(&value) || std::abs(value)>500) return false;
    if(seat.y<5) return false;
    eye=result;
    return true;
}

// Remove the visible vehicle's world transform from the evaluated head pose.
// This retains the riding posture, but never imports kart motion into the seat.
inline bool ComputeSeatedEye(const Mtx34& faceWorld,const Mtx34& bodyWorld,
        detail::Vec3 eyeLocal,std::array<float,3>& eye) noexcept {
    if(!detail::IsFiniteMtx34(faceWorld) || !detail::IsFiniteMtx34(bodyWorld)) return false;
    const detail::Vec3 a{bodyWorld[0],bodyWorld[4],bodyWorld[8]},
        b{bodyWorld[1],bodyWorld[5],bodyWorld[9]},c{bodyWorld[2],bodyWorld[6],bodyWorld[10]};
    const auto bc=detail::Cross(b,c),ca=detail::Cross(c,a),ab=detail::Cross(a,b);
    const float det=detail::Dot(a,bc);
    if(!detail::IsFiniteFloat(&det) || std::abs(det)<1e-6f) return false;
    const auto world=detail::TransformPoint(faceWorld,eyeLocal.x,eyeLocal.y,eyeLocal.z);
    const detail::Vec3 delta{world.x-bodyWorld[3],world.y-bodyWorld[7],world.z-bodyWorld[11]};
    const std::array<float,3> result{detail::Dot(bc,delta)/det,detail::Dot(ca,delta)/det,detail::Dot(ab,delta)/det};
    for(const auto& v:result) if(!detail::IsFiniteFloat(&v) || std::abs(v)>500) return false;
    if(result[1]<5) return false;
    eye=result;
    return true;
}

struct SeatedEyeReference {
    std::array<float,3> value{},candidate{};
    unsigned stable=0;
    bool valid=false;
    void Observe(const std::array<float,3>& sample,bool safe,bool freeze) {
        if(freeze && valid) return;
        if(!safe) { stable=0; return; }
        float delta=0;
        for(int i=0;i<3;++i) delta=std::max(delta,std::abs(sample[i]-candidate[i]));
        stable=stable && delta<2.0f ? stable+1 : 1;
        candidate=sample;
        if(stable>=8) { value=sample;valid=true;stable=8; }
    }
};

// Neutral authored hand targets, transformed by the visible kart body. Do not
// use the animated hand IK targets: feeding their steering rotation back into
// the controller angle would make the input chase its own animation.
inline WheelGeometry ComputeNativeWheelGeometry(const Mtx34& seat_from_body,
        detail::Vec3 left, detail::Vec3 right, float units) noexcept {
    WheelGeometry out{};
    if (!detail::IsFiniteMtx34(seat_from_body) || !detail::IsFiniteFloat(&units) || units<=0) return out;
    if (left.x>right.x) std::swap(left,right);
    const auto a=detail::TransformPoint(seat_from_body,left.x,left.y,left.z);
    const auto b=detail::TransformPoint(seat_from_body,right.x,right.y,right.z);
    detail::Vec3 x{b.x-a.x,b.y-a.y,b.z-a.z};
    const float radius=std::sqrt(detail::Dot(x,x))/(2*units);
    if (!detail::IsFiniteFloat(&radius) || radius<0.04f || radius>1.0f || !detail::Normalize(x)) return out;
    // Kart +X points left when looking along its +Z driving direction.
    // WheelHand uses headset +X (right), so reverse the authored lateral axis.
    x={-x.x,-x.y,-x.z};
    detail::Vec3 y{seat_from_body[1],seat_from_body[5],seat_from_body[9]};
    const float projection=detail::Dot(x,y);
    y={y.x-x.x*projection,y.y-x.y*projection,y.z-x.z*projection};
    if (!detail::Normalize(y)) return out;
    const auto z=detail::Cross(x,y);
    out.center={(a.x+b.x)/(2*units),(a.y+b.y)/(2*units),(a.z+b.z)/(2*units)};
    for(const auto& value:out.center)
        if (!detail::IsFiniteFloat(&value) || std::abs(value)>5) return {};
    out.right={x.x,x.y,x.z}; out.up={y.x,y.y,y.z}; out.normal={z.x,z.y,z.z};
    out.radius=radius; out.valid=true;
    return out;
}

inline WheelGeometry ComputeNativeHandlebarGeometry(const Mtx34& seatFromHandle,
        const Mtx34& seatFromBody, detail::Vec3 left,detail::Vec3 right,float units) noexcept {
    auto out=ComputeNativeWheelGeometry(seatFromHandle,left,right,units);
    if(!out.valid || !detail::IsFiniteMtx34(seatFromBody)) return {};
    // Use the body's neutral axes, not the already-steered handle's axes.
    // Otherwise the visual steering feeds back into the next input sample.
    detail::Vec3 x{-seatFromBody[0],-seatFromBody[4],-seatFromBody[8]},
        forward{seatFromBody[2],seatFromBody[6],seatFromBody[10]};
    if(!detail::Normalize(x)) return {};
    const float along=detail::Dot(forward,x);
    forward={forward.x-along*x.x,forward.y-along*x.y,forward.z-along*x.z};
    if(!detail::Normalize(forward)) return {};
    const auto vertical=detail::Cross(x,forward);
    out.right={x.x,x.y,x.z}; out.up={forward.x,forward.y,forward.z};
    out.normal={vertical.x,vertical.y,vertical.z};
    return out;
}

// Builds the anchor from the game's view matrix (world -> recorded view space),
// the kart's pose (kart-local -> world), and head offsets already converted to
// world units.
//
// The translation moves the camera onto the head. With level_horizon the
// rotation keeps the recorded camera's heading but drops its pitch and roll, so
// the headset owns pitch and roll outright; without it the recorded camera's
// orientation is kept whole and only the eye moves. Returns false and leaves
// `out` untouched when the inputs cannot produce an orthonormal frame.
inline bool ComputeFirstPersonAnchor(const Mtx34& view_from_world, const Mtx34& kart_from_local,
                                     float head_right_units, float head_up_units,
                                     float head_forward_units, bool level_horizon,
                                     Mtx34& out, bool align_to_kart = false) noexcept {
    using namespace detail;
    if (!IsFiniteMtx34(view_from_world) || !IsFiniteMtx34(kart_from_local)) {
        return false;
    }
    const Vec3 head_world =
        TransformPoint(kart_from_local, head_right_units, head_up_units, head_forward_units);
    const Vec3 a = TransformPoint(view_from_world, head_world.x, head_world.y, head_world.z);
    if (!IsFiniteFloat(&a.x) || !IsFiniteFloat(&a.y) || !IsFiniteFloat(&a.z)) {
        return false;
    }

    // Rows of the anchor's rotation. Identity keeps the recorded camera's own
    // orientation and moves the eye only.
    Vec3 rows[3]{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
    if (level_horizon) {
        // World +Y in view coordinates: the column of the view rotation that
        // the world up axis selects.
        Vec3 up{view_from_world[1], view_from_world[5], view_from_world[9]};
        if (!Normalize(up)) {
            return false;
        }
        // Level the recorded camera's forward (-Z in its own space) onto the
        // horizon plane. Looking near-straight up or down leaves nothing to
        // project, so recover the heading from the camera's up axis instead.
        const Vec3 camera_forward{0.0f, 0.0f, -1.0f};
        float along = Dot(camera_forward, up);
        Vec3 forward{camera_forward.x - up.x * along, camera_forward.y - up.y * along,
                     camera_forward.z - up.z * along};
        if (align_to_kart) {
            // Kart +Z supplies the driving direction. Do not inherit the chase
            // camera's heading lag or cinematic orbit in the driver's seat.
            Vec3 heading{kart_from_local[2], 0.0f, kart_from_local[10]};
            if (!Normalize(heading)) return false;
            forward = {view_from_world[0] * heading.x + view_from_world[2] * heading.z,
                       view_from_world[4] * heading.x + view_from_world[6] * heading.z,
                       view_from_world[8] * heading.x + view_from_world[10] * heading.z};
        }
        if (!Normalize(forward)) {
            const Vec3 camera_up{0.0f, 1.0f, 0.0f};
            along = Dot(camera_up, up);
            forward = {camera_up.x - up.x * along, camera_up.y - up.y * along,
                       camera_up.z - up.z * along};
            if (!Normalize(forward)) {
                return false;
            }
        }
        Vec3 right = Cross(forward, up);
        if (!Normalize(right)) {
            return false;
        }
        // Re-derive up from the orthonormalized pair so a slightly non-rigid
        // view matrix cannot leave a skewed frame behind.
        rows[0] = right;
        rows[1] = Cross(right, forward);
        rows[2] = {-forward.x, -forward.y, -forward.z};
    }

    Mtx34 anchor{};
    for (uint32_t row = 0; row < 3; ++row) {
        anchor[row * 4 + 0] = rows[row].x;
        anchor[row * 4 + 1] = rows[row].y;
        anchor[row * 4 + 2] = rows[row].z;
        anchor[row * 4 + 3] = -Dot(rows[row], a);
    }
    if (!IsFiniteMtx34(anchor)) {
        return false;
    }
    out = anchor;
    return true;
}

inline Mtx34 ComputeDioramaAnchor(float distance, float height) noexcept {
    const float pitch = std::atan2(height, distance);
    const float c = std::cos(pitch), s = std::sin(pitch);
    return {1, 0, 0, 0, 0, c, -s, -c * height + s * distance,
            0, s, c, -s * height - c * distance};
}

inline bool ComputeKartDioramaAnchor(const Mtx34& view, const Mtx34& kart,
                                     float distance, float height, Mtx34& out) noexcept {
    Mtx34 centered{};
    if (!ComputeFirstPersonAnchor(view, kart, 0, 0, 0, true, centered, true)) return false;
    const auto overview = ComputeDioramaAnchor(distance, height);
    for (size_t row = 0; row < 3; ++row) {
        for (size_t col = 0; col < 4; ++col) {
            out[row * 4 + col] = (col == 3 ? overview[row * 4 + 3] : 0.0f);
            for (size_t k = 0; k < 3; ++k)
                out[row * 4 + col] += overview[row * 4 + k] * centered[k * 4 + col];
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Per-frame observation. Called from the translated-code observers on the guest
// thread; the anchor is consumed by the producer at its Aurora frame seal.
// ---------------------------------------------------------------------------

// Enables anchor computation and sets the head offsets and world scale used to
// convert them. Called whenever the configuration or the F10 toggle changes.
void MkwVRFirstPersonConfigure(bool enabled, const FirstPersonHeadOffsets& offsets,
                               float units_per_meter) noexcept;

// Reads the current [vr] first-person settings and applies them here and to the
// presentation policy's world scale. The single place those settings are
// interpreted, shared by startup and the F10 settings bar.
void MkwVRFirstPersonApplyConfiguredSettings() noexcept;

// Reads the race camera and the player's kart and republishes the anchor. Call
// once per guest frame, after the kart and camera updates and before the draws.
// race_camera_address is the frame's own RaceCamera, or zero if none was seen.
void MkwVRFirstPersonUpdate(uint64_t guest_frame_index, uint32_t race_camera_address) noexcept;

// Drops every captured pointer and the held anchor. Call on race entry/exit.
void MkwVRFirstPersonReset() noexcept;
// Guest thread only, immediately after the frame's draws have been recorded.
void MkwVRFirstPersonRestoreDriver() noexcept;

// Producer-side read. Thread-safe. A valid anchor is also what marks the mode
// as engaged, and so what selects the first-person world scale: it is invalid
// whenever the mode is off, the race has not produced a usable anchor, or the
// anchor has been missing long enough to give up holding the last one.
FirstPersonAnchor MkwVRFirstPersonGetAnchor() noexcept;

} // namespace mkw::vr
