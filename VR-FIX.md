# VR release notes

## Quality and presentation — 2026-09-06

- Increased the default VR render scale from 0.75 to 1.0. This renders about 78% more pixels per
  eye while leaving game simulation and frame pacing unchanged. The previous profile is backed up
  as `UserData/Config.before-quality.toml`.
- The first-person camera is anchored to the kart and its heading instead of the chase-camera
  origin. The default anchor is 110 game units above and 120 units ahead of the kart.
- The diorama camera uses a 1,600-unit distance, 1,200-unit height, and an independent 1,000
  game-units-per-metre scale.
- HUD scissor state is retained in the uniform data and applied in the panel shader. Pipeline-cache
  versions are bumped whenever the uniform contract changes.
- Reused eye targets are explicitly cleared on their first use, including when the EFB list resumes
  with `Load`.
- Screen-attached alpha effects keep the original perspective camera when they neither test nor
  write depth. This avoids moving overlays in front of the eyes; the Blooper ink effect still needs
  an in-headset visual check.

## VR options and camera controls

The diorama is built from the kart's centre and heading rather than the chase-camera origin. The
first-person position, diorama distance, height, scale, HUD hand, Wii filter, FPS counter, and
camera mode are configurable and saved to `Config.toml`.

The VR options panel opens with `F10` or `X + Y` on a Quest-compatible controller. Use the stick to
navigate, `A` to confirm, `B` to go back, and `X + Y` to close. The panel is temporarily presented
as the OpenXR virtual screen so it remains visible in the headset. Gameplay input is blocked while
the panel is open, but the game is not automatically paused. Presentation transitions invalidate
cached eye images and recenter tracking when returning to a race.

Performance presets are 65% (Performance), 80% (Balanced), 100% (Quality), and 120% (Ultra). A
custom render scale from 50% to 150% is also available and is applied on the next launch.

## OpenXR controller compatibility

The runtime now suggests bindings for the following standard OpenXR interaction profiles:

- Meta/Oculus Touch, Touch Plus, Touch Pro, Quest 1/Rift S, Quest 2, and Rift CV1.
- Valve Index.
- Microsoft Mixed Reality Motion Controllers and Samsung Odyssey controllers.
- HTC Vive, Vive Cosmos, and Vive Focus 3 controllers.
- ByteDance PICO Neo3, PICO 4, PICO G3, and PICO Ultra controllers.
- The Khronos Simple Controller fallback profile for basic menu navigation.

Mappings preserve the existing Quest layout where the hardware has equivalent controls. Vive-style
trackpads drive steering and tricks; squeeze clicks provide drift; trigger clicks confirm; and the
menu button cancels or pauses. Profiles with no analog controls remain usable for menu navigation.
The right stick or trackpad click cycles the three VR cameras on profiles that expose that control.
The left grip pose drives the hand HUD on every supported profile.

## Quest stick calibration

The default steering settings remain a centred stick, a radial 15% deadzone, and 100% outer range.
The VR options panel exposes raw X/Y values, the mapped game direction, one-second centre
calibration after a one-second rest, a 0–40% deadzone, a 60–100% outer range, and reset.

Calibration is refused when tracking is unavailable or the stick is moving too much. The settings
are stored as `[vr] stick_deadzone`, `stick_outer`, `stick_center_x`, and `stick_center_y`.
Centre correction preserves the physical -100/+100 endpoints, and diagnostic output continues to
show the raw stick data.

## Validation

- Release C++ build completed successfully with the pinned LLVM-MinGW toolchain.
- The VR test set passes, including first-person camera transforms, OpenXR controller profile
  coverage, Quest input mapping, stale-input expiry, F10 suppression, frame delivery, and settings
  transitions.
- The generated PAL `RMCP01` game build starts in a desktop fallback profile and exits normally.
- A physical headset race, controller tracking recovery, and visual placement of the minimap still
  require on-device verification. VR remains experimental and the desktop mirror remains available
  as a fallback.
