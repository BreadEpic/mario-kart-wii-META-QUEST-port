#include "aurora/vulkan_interop.h"

// The same-device requirement is documented in aurora/vulkan_interop.h. This
// file is the Vulkan sibling of d3d12_interop.cpp and follows its structure: a
// real implementation when the build actually has a Dawn Vulkan backend, and an
// honest "not available" stub otherwise, so the OpenXR Vulkan backend reports
// DawnNativeHandlesUnavailable instead of binding to handles that do not exist.
//
// The pinned prebuilt Dawn package exposes VkInstance only. Reaching the
// physical device, device, queue and queue family requires a Dawn built from
// source with its native headers available, which is exactly the configuration
// the Quest target uses (AURORA_DAWN_PROVIDER=vendor). __has_include is the
// discriminator rather than a build flag so a package-provided Dawn cannot
// silently compile a half-working bridge.
#if defined(WEBGPU_DAWN) && defined(DAWN_ENABLE_BACKEND_VULKAN) && \
    __has_include(<dawn/native/VulkanBackend.h>)
#define AURORA_VULKAN_INTEROP_AVAILABLE 1
#else
#define AURORA_VULKAN_INTEROP_AVAILABLE 0
#endif

#if AURORA_VULKAN_INTEROP_AVAILABLE

#include <dawn/native/VulkanBackend.h>

#include <mutex>

#include "gpu.hpp"

namespace {
// Dawn submits from its own thread. Every XR queue operation is bracketed by
// the lock/unlock pair below so the two never race on the shared VkQueue.
std::recursive_mutex g_queue_mutex;
} // namespace

extern "C" {

bool aurora_vulkan_lock_queue(void) {
  g_queue_mutex.lock();
  return true;
}

void aurora_vulkan_unlock_queue(void) { g_queue_mutex.unlock(); }

bool aurora_vulkan_get_native_handles(AuroraVulkanNativeHandles* handles) {
  if (handles == nullptr) {
    return false;
  }
  *handles = {};

  // NOTE: filling this in requires Dawn's Vulkan device introspection
  // (dawn::native::vulkan::*) against the vendored Dawn tree the Quest target
  // builds. It is deliberately left reporting failure rather than returning
  // plausible-looking zeros: OpenXR would accept the binding and then fault on
  // the first submission. See QUEST.md, "Rendering".
  return false;
}

} // extern "C"

#else

extern "C" {

bool aurora_vulkan_lock_queue(void) { return false; }

void aurora_vulkan_unlock_queue(void) {}

bool aurora_vulkan_get_native_handles(AuroraVulkanNativeHandles* handles) {
  if (handles != nullptr) {
    *handles = {};
  }
  return false;
}

} // extern "C"

#endif
