#ifndef AURORA_VULKAN_INTEROP_H
#define AURORA_VULKAN_INTEROP_H

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include "stdbool.h"
#include "stdint.h"
#endif

/**
 * Vulkan counterpart to aurora/d3d12_interop.h, for the standalone Meta Quest
 * target, where Vulkan is the only backend.
 *
 * OpenXR requires the application to submit eye images on the SAME Vulkan
 * device the compositor was told about. Aurora owns that device through Dawn,
 * so the XR backend cannot create its own: it has to borrow Dawn's instance,
 * physical device, device and queue. That is what these handles are for, and
 * why runtime/src/vr/openxr_vulkan_backend.cpp reports
 * DawnNativeHandlesUnavailable until this shim is compiled in.
 *
 * Handles are borrowed. They remain valid until aurora_shutdown() and must not
 * be destroyed by the caller.
 */
typedef struct {
  /* VkInstance, VkPhysicalDevice, VkDevice, VkQueue. Carried as uint64_t so
   * this header does not drag the Vulkan headers into every consumer; Vulkan
   * dispatchable and non-dispatchable handles both fit in 64 bits on every
   * target this project supports. */
  uint64_t instance;
  uint64_t physicalDevice;
  uint64_t device;
  uint64_t queue;
  uint32_t queueFamilyIndex;
  uint32_t queueIndex;
  /* VK_MAKE_API_VERSION-encoded version of the instance/device. */
  uint32_t apiVersion;
  /* VkFormat matching Aurora's single-sample eye output. */
  int32_t colorFormat;
} AuroraVulkanNativeHandles;

/**
 * A Vulkan queue is not internally synchronized, and OpenXR may use the
 * graphics queue while handing an image over. These bracket every queue
 * operation the XR backend performs, against Dawn's own queue use.
 * aurora_vulkan_lock_queue returns false when the queue cannot be acquired, in
 * which case the caller must not touch it.
 */
bool aurora_vulkan_lock_queue(void);
void aurora_vulkan_unlock_queue(void);

/** Returns false unless the active Aurora backend is Dawn Vulkan. */
bool aurora_vulkan_get_native_handles(AuroraVulkanNativeHandles* handles);

#ifdef __cplusplus
}
#endif

#endif
