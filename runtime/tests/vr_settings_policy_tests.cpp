#include "vr/mkw_vr_policy.h"
#include <iostream>

int main() {
    using namespace mkw::vr;
    MkwVRPolicyReset();
    MkwVRPolicyConfig config;
    config.enabled = true;
    MkwVRPolicyConfigure(config);
    MkwVRPolicySetSessionActive(true);
    MkwVRPolicySetAvailableBindings(kMkwVRRequiredImmersiveBindings);
    MkwVRPolicyPublishScene({VRSceneMode::Race, 1, 10});
    MkwVRCameraObservation camera;
    camera.valid = true;
    camera.guest_frame_index = 10;
    camera.view_from_world = {1,0,0,0,0,1,0,0,0,0,1,0};
    MkwVRPolicyPublishRaceCamera(camera);
    const auto race = MkwVRPolicyGetSnapshot();
    MkwVRPolicySetSettingsVisible(true);
    const auto menu = MkwVRPolicyGetSnapshot();
    MkwVRPolicySetSettingsVisible(true);
    const auto menuAgain = MkwVRPolicyGetSnapshot();
    MkwVRPolicySetSettingsVisible(false);
    const auto resumed = MkwVRPolicyGetSnapshot();
    if (race.presentation != VRPresentationMode::ImmersiveRace ||
        menu.presentation != VRPresentationMode::VirtualScreen ||
        menu.content_tag == race.content_tag || menuAgain.content_tag != menu.content_tag ||
        resumed.presentation != VRPresentationMode::ImmersiveRace || resumed.content_tag == menu.content_tag ||
        resumed.content_tag == race.content_tag) {
        std::cerr << "VR settings must show a mono UI and invalidate cached images at each transition\n";
        return 1;
    }
    return 0;
}
