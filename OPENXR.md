# Experimental OpenXR VR

WiiCompiled includes an opt-in OpenXR renderer. The Windows implementation uses D3D12 and submits
both eyes to the active OpenXR runtime on the same graphics device as Aurora. There is no CPU
texture readback and no second graphics device.

VR is still experimental. It falls back to the normal desktop mirror when the runtime, headset,
GPU, or graphics binding is unavailable unless `required = true` is selected.

## Requirements

- Windows 10 or 11, 64-bit.
- An active Windows OpenXR runtime and a connected compatible headset.
- A D3D12-capable GPU and driver accepted by both OpenXR and Dawn.
- A build made with `MKW_ENABLE_OPENXR=ON`, which is enabled by default on Windows.

The supported OpenXR distribution target is Windows with D3D12. Linux and other platforms are not
supported release targets.

## Configuration

OpenXR is disabled by default. The configuration file is next to the installed game and can contain:

```toml
[vr]
enabled = false
required = false
render_scale = 1.0
world_units_per_meter = 500.0
hud_distance_meters = 2.0
hud_width_meters = 2.4
hud_virtual_screen = true
stop_at_display_copy = true
skip_copy_clears = true
first_person = false
first_person_units_per_meter = 10.0
first_person_head_up_meters = 1.0
first_person_head_forward_meters = 0.0
first_person_head_right_meters = 0.0
```

Set `enabled = true`, close the game completely, and launch it again. The enable switch in the F10
panel also requires a restart. `required = false` is the safe default: a missing runtime, detached
headset, unsupported GPU, or graphics-binding failure returns to desktop mode.

`render_scale` multiplies the runtime-recommended eye dimensions. `world_units_per_meter` controls
headset translation in the game world. `hud_distance_meters` and `hud_width_meters` size the
head-locked virtual screen. `hud_virtual_screen` controls whether the race's 2D layer uses that
screen. `stop_at_display_copy` and `skip_copy_clears` are diagnostic switches for frame replay.
The first-person values control the camera described below and are also available in the F10 panel.

## Camera and HUD

Every race starts with the original game camera. During an immersive race, click the right
thumbstick to cycle through the original camera, the first-person cockpit, and the distant diorama
camera. The click is latched, so holding the stick advances only once. Menus do not consume camera
changes. Profiles without a right-stick click keep their normal camera.

With `hud_virtual_screen = true`, the complete race HUD, including the circuit minimap and item
roulette, follows the left controller as a 30 cm panel in the original and diorama cameras. In the
first-person cockpit it is anchored in front of the seat, independently of controller tracking and
head turns. If tracking or focus is lost, the HUD returns to the normal virtual screen. Menus retain
their existing presentation.

The first-person view is placed at Player 1's authored head bone and hides only that driver's model.
If a vehicle does not expose the expected head bone, its driver-seat parameters provide the
fallback. Position follows the kart simulation exactly, while impact rotations are held and blended
back for comfort. The diorama uses a much larger world scale and follows the kart's centre and
driving direction. The game's own transforms are not modified; Aurora composes the VR view and eye
transforms around them.

Cockpit mode presents the vehicle's wheel or motorcycle handlebar and tracked controller hands in
the seated frame. Squeeze either grip near the control to grab it. One or both hands can steer;
joining or releasing a hand preserves the steering target, and common two-arm movement is ignored.
The grab tolerates broad forward/back movement, centre crossings and brief tracking loss. Adaptive
smoothing damps tracking tremor while keeping fast steering responsive at 72, 90 and 120 Hz.
Releasing both grips returns steering to the left stick. Native steering is enabled by default and
animates the vehicle's original control; it can be disabled to use the procedural VR control. Hands use the scene
depth buffer, so the kart and track correctly occlude them. The renderer uses the Meta hand mesh
extension when available and articulated glove models as a fallback.

The seated calibration uses the evaluated eye position for each driver and vehicle. Tall characters
receive a comfortable world scale, while lightning and other temporary player scaling resize the
viewpoint, control position and grab radius together. Temporary scale cannot replace the neutral
calibration when the camera is changed.

## Controller compatibility

The runtime suggests bindings for standard OpenXR interaction profiles and silently ignores profiles
that the active runtime does not advertise.

| Profile family | Analog controls | Digital controls | Hand HUD |
| --- | --- | --- | --- |
| Meta/Oculus Touch, Touch Plus, Touch Pro, Quest 1/Rift S, Quest 2, Rift CV1 | Left/right thumbsticks, triggers, squeeze | A/B/X/Y, menu, stick click | Left grip pose |
| ByteDance PICO Neo3, PICO 4, PICO G3, PICO Ultra | Left/right thumbsticks, triggers, squeeze | A/B/X/Y or menu, stick click | Left grip pose |
| Valve Index | Left/right thumbsticks, triggers, squeeze | A/B, system, stick click | Left grip pose |
| Microsoft Mixed Reality / Samsung Odyssey | Left/right thumbsticks, trigger | Menu, squeeze, stick click | Left grip pose |
| HTC Vive / Vive Cosmos / Vive Focus 3 | Left/right trackpads, triggers | Menu, trigger, squeeze, trackpad click | Left grip pose |
| Khronos Simple Controller | None | Select and menu | Left grip pose |

Equivalent layouts use the same in-game actions. Vive-style trackpads replace thumbsticks. A
controller without analog inputs remains usable for menu navigation. The profile table is kept in
`runtime/include/vr/openxr_controller_profiles.h` and checked by
`mkw_openxr_controller_profiles_tests`.

### Quest-style actions

| Controller control | In-game action |
| --- | --- |
| Left stick | Steer and navigate menus; in cockpit it steers after both hands release the wheel |
| Right trigger | Accelerate |
| Left trigger, held | Brake, then reverse in every camera; overrides held acceleration and drift |
| A | Accelerate / confirm outside cockpit; hop / drift in cockpit; confirm in menus |
| B | Brake / cancel |
| Y | Use or hold an item |
| X | Trick / bike wheelie |
| Right stick directions | Directional tricks; vertical input starts or ends bike wheelies |
| Left and right squeeze | Grab the physical wheel in cockpit; right squeeze hops/drifts outside cockpit |
| Left menu button | Pause |
| Right stick click | Cycle VR cameras |
| X + Y | Open or close VR settings without passing either action to gameplay |

The OpenXR system button keeps its system function. Player 1 uses the VR action set while the
session is focused; the normal keyboard/gamepad path resumes when VR input is unavailable. Players
2–4 are unaffected. A radial 15% stick deadzone filters drift, short button taps survive frame-rate
differences, and a stale XR snapshot expires after 250 ms. F10 suppresses gameplay input until held
controls are released after the panel closes.

The game continues to display GameCube prompts. Haptics and user-editable remapping of these fixed
VR bindings are not implemented yet.

## First-person camera tuning

`first_person_head_up_meters`, `first_person_head_forward_meters`, and
`first_person_head_right_meters` tune the driver's head anchor. The diorama position uses the kart's
centre and heading, not the chase-camera origin. These values are live in the F10 panel and saved to
the configuration file.

## Backend status

| Backend | Status |
| --- | --- |
| Windows D3D12 | Implemented: same-adapter, same-device asynchronous OpenXR submission. |
| Linux / other platforms | Not supported by the current distribution. |

## Known limitations

- Only the PAL `RMCP01` translation has the race instrumentation required for immersive rendering.
- Wii Remote support is a separate input path and has its own documented limitations.
- Dedicated Quest, Android, and Apple visionOS packaging is not implemented.
- Scene-specific comfort options, culling fixes, replay/spectator classification, and advanced VR
  remapping are future work.
- The desktop window remains available as a mirror and fallback.

OpenXR diagnostics are written to the normal run log under
`%LOCALAPPDATA%\\WiiCompiled\\Logs`. Search for `OpenXR` when reporting startup or submission
failures.

## Local validation

The runtime and generated PAL `RMCP01` game build compile and link with the pinned LLVM-MinGW
toolchain. The VR camera, input, delivery, policy, and controller-profile CTest targets pass. A
desktop smoke test initializes D3D12, loads game scenes, stays responsive, and exits normally.

A physical headset race is still required to verify controller tracking recovery, camera placement,
and minimap legibility on the target hardware.
