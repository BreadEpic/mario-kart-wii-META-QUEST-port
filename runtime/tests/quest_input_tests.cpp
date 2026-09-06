// SPDX-License-Identifier: GPL-3.0-or-later
#include "vr/quest_input.h"
#include <chrono>
#include <iostream>
#include <thread>

using namespace mkw::vr;
int failures = 0;
void Check(bool condition, const char* message) {
    if (!condition) { ++failures; std::cerr << "FAILED: " << message << '\n'; }
}
int main() {
    QuestInput input{};
    Check(MapQuestInput(input).err == PAD_ERR_NO_CONTROLLER, "inactive controllers stay disconnected");
    input.active = true;
    input.steering_x = 0.1f;
    Check(MapQuestInput(input).stickX == 0, "stick noise is inside deadzone");
    input.steering_x = 1;
    input.steering_y = 0;
    Check(MapQuestInput(input).stickX == 100, "full right steering");
    input.steering_x = 0;
    input.steering_y = -1;
    Check(MapQuestInput(input).stickY == -100, "down retains GameCube sign for menus and item aim");
    input.steering_x = 1;
    input.steering_y = 1;
    auto pad = MapQuestInput(input);
    Check(pad.stickX == 71 && pad.stickY == 71, "diagonal is normalized");
    for (int x = 0; x <= 100; ++x) {
        input.steering_x = x / 100.0f; input.steering_y = 0.2f;
        const auto right = MapQuestInput(input).stickX;
        input.steering_x = -x / 100.0f;
        Check(MapQuestInput(input).stickX == -right, "left and right steering are symmetric");
    }
    const QuestStickCalibration calibrated{0.15f, 1.0f, -0.1f, 0.05f};
    input.steering_x = -0.1f; input.steering_y = 0.05f;
    Check(MapQuestInput(input, calibrated).stickX == 0 && MapQuestInput(input, calibrated).stickY == 0,
          "calibrated physical centre produces neutral steering");
    input.steering_x = 1;
    Check(MapQuestInput(input, calibrated).stickX == 100, "centre correction preserves full right");
    input.steering_x = -1;
    Check(MapQuestInput(input, calibrated).stickX == -100, "centre correction preserves full left");
    input.steering_x = 0.8f; input.steering_y = 0;
    Check(MapQuestInput(input, {0.15f, 0.8f, 0, 0}).stickX == 100, "outer calibration restores maximum steering");
    input.steering_x = 0.2f;
    Check(MapQuestInput(input, {0.25f, 1, 0, 0}).stickX == 0, "larger deadzone rejects stronger drift");
    input = {}; input.active = true;
    input.accelerate = 1; input.item = 1; input.drift = 1;
    pad = MapQuestInput(input);
    Check(pad.button == (PAD_BUTTON_A | PAD_TRIGGER_L | PAD_TRIGGER_R), "accelerate, item and drift work together");
    Check(pad.analogA == 255 && pad.triggerL == 255 && pad.triggerR == 255, "GC analog channels agree with buttons");
    input = {}; input.active = true;
    input.confirm = true; input.brake = true; input.look_back = true; input.pause = true;
    pad = MapQuestInput(input);
    Check(pad.button == (PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X | PAD_BUTTON_START), "face buttons and pause mapping");
    input.look_back = false; input.trick = true;
    Check((MapQuestInput(input).button & PAD_BUTTON_UP) != 0, "X alone still triggers a trick");
    input = {}; input.active = true;
    input.tricks_x = -1; input.tricks_y = -1;
    Check(MapQuestInput(input).button == (PAD_BUTTON_LEFT | PAD_BUTTON_DOWN), "right stick drives GC trick directions");
    QuestPadFilter filter;
    input = {}; input.active = true; input.accelerate = 1; input.steering_x = 1;
    pad = MapQuestInput(input);
    auto blocked = filter.Apply(pad, true);
    Check(blocked.err == PAD_ERR_NONE && blocked.button == 0 && blocked.stickX == 0, "F10 neutralizes without disconnecting");
    blocked = filter.Apply(pad, false);
    Check(blocked.button == 0 && blocked.analogA == 0 && blocked.stickX == 0, "held input stays suppressed after F10 closes");
    input.accelerate = 0; input.steering_x = 0;
    filter.Apply(MapQuestInput(input), false);
    input.accelerate = 1; input.steering_x = 1;
    blocked = filter.Apply(MapQuestInput(input), false);
    Check(blocked.button == PAD_BUTTON_A && blocked.stickX == 100, "release and repress restores controls");
    PublishQuestInput(input);
    PADStatus result{};
    Check(ReadQuestInputSnapshot().accelerate == 1, "menu reads latest snapshot without consuming gameplay taps");
    Check(ReadQuestPad(result, false) && result.button == PAD_BUTTON_A, "published XR snapshot reaches guest reader");
    PublishQuestInput({});
    result.button = PAD_BUTTON_B;
    Check(!ReadQuestPad(result, false) && result.button == PAD_BUTTON_B, "focus loss preserves desktop input instead of stale Quest state");
    input = {}; input.active = true; input.pause = true;
    PublishQuestInput(input);
    input.pause = false;
    PublishQuestInput(input);
    Check(ReadQuestPad(result, false) && (result.button & PAD_BUTTON_START), "short pause tap survives XR/guest frame-rate mismatch");
    Check(ReadQuestPad(result, false) && result.button == 0, "short tap is consumed once");
    PublishQuestInput(input);
    std::this_thread::sleep_for(std::chrono::milliseconds(280));
    Check(!ReadQuestPad(result, false), "stalled XR loop expires input");
    Check(!ReadQuestInputSnapshot().active, "stalled XR input also expires in VR menu");
    input = {}; input.active = true; input.trick = true; input.look_back = true;
    input.accelerate = 1; input.steering_x = 1;
    pad = MapQuestInput(input);
    Check(pad.button == 0 && pad.stickX == 0, "VR menu chord does not trigger gameplay actions");
    PublishQuestInput({});
    SetQuestStickCalibration(calibrated);
    input = {}; input.active = true; input.steering_x = -0.1f; input.steering_y = 0.05f;
    PublishQuestInput(input);
    Check(ReadQuestPad(result, false) && result.stickX == 0 && result.stickY == 0,
          "saved calibration reaches guest controls");
    Check(ReadQuestInputSnapshot().steering_x == -0.1f, "diagnostics retain raw stick values");
    SetQuestStickCalibration({});
    PublishQuestInput({});
    if (!failures) std::cout << "Quest input checks passed\n";
    return failures ? 1 : 0;
}
