// SPDX-License-Identifier: GPL-3.0-or-later
#include "vr/quest_input.h"
#include <chrono>
#include <mutex>

namespace mkw::vr {
namespace {
std::mutex quest_input_mutex;
QuestInput quest_input;
QuestStickCalibration quest_calibration;
uint16_t quest_pending_buttons = 0;
uint16_t quest_previous_buttons = 0;
std::chrono::steady_clock::time_point quest_input_time;
QuestPadFilter quest_pad_filter; // Guest thread only.
}

void PublishQuestInput(const QuestInput& input) noexcept {
    std::lock_guard lock(quest_input_mutex);
    const auto buttons = MapQuestInput(input).button;
    if (input.active) {
        // XR and guest frames have different rates: retain a short tap until
        // PADRead observes it, even if the button was released between reads.
        quest_pending_buttons |= buttons & ~quest_previous_buttons;
    } else {
        quest_pending_buttons = 0;
    }
    quest_previous_buttons = buttons;
    quest_input = input;
    quest_input_time = std::chrono::steady_clock::now();
}

bool ReadQuestPad(PADStatus& output, bool blocked) noexcept {
    QuestInput snapshot;
    QuestStickCalibration calibration;
    uint16_t pending_buttons = 0;
    {
        std::lock_guard lock(quest_input_mutex);
        snapshot = quest_input;
        calibration = quest_calibration;
        pending_buttons = quest_pending_buttons;
        quest_pending_buttons = 0;
        // Never keep accelerating if the XR frame loop stalls or exits.
        if (std::chrono::steady_clock::now() - quest_input_time > std::chrono::milliseconds(250)) {
            snapshot = {};
        }
    }
    if (!snapshot.active) {
        quest_pad_filter = {};
        return false; // Preserve the normal desktop/gamepad source.
    }
    auto pad = MapQuestInput(snapshot, calibration);
    pad.button |= pending_buttons;
    if (pad.button & PAD_BUTTON_A) pad.analogA = 255;
    if (pad.button & PAD_BUTTON_B) pad.analogB = 255;
    if (pad.button & PAD_TRIGGER_L) pad.triggerL = 255;
    if (pad.button & PAD_TRIGGER_R) pad.triggerR = 255;
    output = quest_pad_filter.Apply(pad, blocked);
    return true;
}

QuestInput ReadQuestInputSnapshot() noexcept {
    std::lock_guard lock(quest_input_mutex);
    if (std::chrono::steady_clock::now() - quest_input_time > std::chrono::milliseconds(250)) return {};
    return quest_input;
}

void SetQuestStickCalibration(const QuestStickCalibration& calibration) noexcept {
    std::lock_guard lock(quest_input_mutex);
    quest_calibration = calibration;
}
} // namespace mkw::vr
