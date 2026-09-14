// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <aurora/aurora.h>

#include <string>

namespace mkw::vr {

enum class OpenXRStartupResult {
    Disabled,
    Prepared,
    Unavailable,
};
struct OpenXRDiagnostics {
    float displayFps=0,newImageFps=0,runtimeHz=0,intervalP95=0,intervalP99=0;
    uint32_t width=0,height=0;
    uint64_t canceled=0;
    float rasterScale=1;
};
OpenXRDiagnostics OpenXRGetDiagnostics();

// Performs the OpenXR instance/system and graphics-requirements work that must
// happen before Aurora selects an adapter. On success this may force the
// backend and populate AuroraConfig's XR interop fields.
OpenXRStartupResult OpenXRPrepareAurora(AuroraConfig& config);

// Completes the graphics binding and starts the asynchronous XR pacing thread.
// Call after aurora_initialize(), while Aurora's frame worker is idle.
bool OpenXRStartAfterAurora(AuroraBackend active_backend);

// Stops publishing stereo work, drains the pacing thread, and destroys the XR
// session before aurora_shutdown(). Safe to call after partial initialization.
void OpenXRShutdownBeforeAurora() noexcept;

// Services an XR-owned teardown request at the producer's safe frame boundary:
// after aurora_begin_frame() has granted the worker's prepare phase and before
// aurora_end_frame_tagged() seals the current frame. No-op unless the pacing
// thread has fallen back to desktop rendering.
void OpenXRServiceProducerFrameBoundary() noexcept;

bool OpenXRIsRunning() noexcept;
std::string OpenXRLastError();

} // namespace mkw::vr
