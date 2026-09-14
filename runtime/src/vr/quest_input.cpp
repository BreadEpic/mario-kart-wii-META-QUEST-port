// SPDX-License-Identifier: GPL-3.0-or-later
#include "vr/quest_input.h"
#include <chrono>
#include <mutex>

namespace mkw::vr {
namespace {
std::mutex quest_input_mutex;
QuestInput quest_input;
QuestStickCalibration quest_calibration;
QuestButtonMapping quest_mapping;
uint16_t quest_pending_buttons = 0;
uint16_t quest_previous_buttons = 0;
std::chrono::steady_clock::time_point quest_input_time;
QuestPadFilter quest_pad_filter; // Guest thread only.
bool tutorial_pause_pending=false;
}

void PublishQuestInput(const QuestInput& input) noexcept {
    std::lock_guard lock(quest_input_mutex);
    const auto buttons = MapQuestInput(input, {}, quest_mapping).button;
    if (input.active) {
        // XR and guest frames have different rates: retain a short tap until
        // PADRead observes it, even if the button was released between reads.
        quest_pending_buttons |= buttons & ~quest_previous_buttons;
    } else {
        quest_pending_buttons = 0;
        tutorial_pause_pending = false;
    }
    quest_previous_buttons = buttons;
    quest_input = input;
    quest_input_time = std::chrono::steady_clock::now();
}

void RequestQuestPausePulse() noexcept {
    std::lock_guard lock(quest_input_mutex);tutorial_pause_pending=true;
}

bool ReadQuestPad(PADStatus& output, bool blocked) noexcept {
    QuestInput snapshot;
    QuestStickCalibration calibration;
    QuestButtonMapping mapping;
    uint16_t pending_buttons = 0;
    bool tutorialPause=false;
    {
        std::lock_guard lock(quest_input_mutex);
        snapshot = quest_input;
        calibration = quest_calibration;
        mapping = quest_mapping;
        pending_buttons = quest_pending_buttons;
        quest_pending_buttons = 0;
        // Never keep accelerating if the XR frame loop stalls or exits.
        if (std::chrono::steady_clock::now() - quest_input_time > std::chrono::milliseconds(250)) {
            snapshot = {};
            tutorial_pause_pending = false;
        }
        if(snapshot.active) { tutorialPause=tutorial_pause_pending;tutorial_pause_pending=false; }
    }
    if (!snapshot.active) {
        quest_pad_filter = {};
        return false; // Preserve the normal desktop/gamepad source.
    }
    auto pad = MapQuestInput(snapshot, calibration, mapping);
    pad.button |= pending_buttons;
    if (snapshot.reverse) pad.button &= ~(PAD_BUTTON_A | PAD_TRIGGER_R);
    if (pad.button & PAD_BUTTON_A) pad.analogA = 255;
    if (pad.button & PAD_BUTTON_B) pad.analogB = 255;
    if (pad.button & PAD_TRIGGER_L) pad.triggerL = 255;
    if (pad.button & PAD_TRIGGER_R) pad.triggerR = 255;
    output = quest_pad_filter.Apply(pad, blocked);
    if(tutorialPause) { output={};output.err=PAD_ERR_NONE;output.button=PAD_BUTTON_START; }
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
void SetQuestButtonMapping(QuestButtonMapping mapping) noexcept {
    std::lock_guard lock(quest_input_mutex);
    if(mapping.swapItemTrick!=quest_mapping.swapItemTrick || mapping.swapCockpitDriftBrake!=quest_mapping.swapCockpitDriftBrake)
        quest_pending_buttons=quest_previous_buttons=0;
    quest_mapping=mapping;
}
} // namespace mkw::vr
