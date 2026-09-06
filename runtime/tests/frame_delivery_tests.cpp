#include "vr/frame_delivery.h"
#include <iostream>
#include <stdexcept>

using mkw::vr::detail::FrameDelivery;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        FrameDelivery state;
        require(!state.CanDisplay(7, 1), "uninitialized image displayed");
        require(!state.Start(0, 7, 1), "invalid token accepted");
        require(state.Start(1, 7, 1), "first job rejected");
        require(!state.Start(2, 7, 1), "GPU job overwritten");
        require(!state.Complete(2), "stale callback accepted");
        require(!state.CanDisplay(7, 1), "unrendered image displayed");
        require(state.Complete(1), "valid image rejected");
        require(!state.Complete(1), "duplicate callback accepted");
        require(state.CanDisplay(7, 1), "ready image absent");
        require(!state.CanDisplay(8, 1) && !state.CanDisplay(7, 2), "image escaped its scene/session");
        require(state.Start(2, 7, 1), "next GPU job rejected");
        for (int tick = 0; tick < 90; ++tick) {
            require(state.CanDisplay(7, 1), "display consumed cached image while GPU job pending");
            require(state.PendingToken() == 2, "display mutated GPU ownership");
        }
        state.Cancel();
        require(!state.PendingToken() && !state.CanDisplay(7, 1), "cancel left an unwritten released image visible");
        state.Start(3, 8, 2);
        state.InvalidateCache();
        require(state.PendingToken() == 3, "cache invalidation canceled live GPU work");
        state.Complete(3);
        require(state.CanDisplay(8, 2) && !state.CanDisplay(7, 1), "new session image not isolated");

        // One second on a 180-step timeline: game image completion every three
        // steps (60 Hz), display every two (90 Hz). No simulation work is
        // triggered by a repeated compositor display.
        FrameDelivery cadence;
        cadence.Start(1, 5, 1);
        cadence.Complete(1);
        int generated = 0, displayed = 0;
        for (int t = 0; t < 180; ++t) {
            if (t % 3 == 0) {
                const auto token = static_cast<uint64_t>(++generated + 1);
                require(cadence.Start(token, 5, 1) && cadence.Complete(token), "60 Hz image lost");
            }
            if (t % 2 == 0 && cadence.CanDisplay(5, 1)) ++displayed;
        }
        require(generated == 60 && displayed == 90, "display and game cadences coupled");
        std::cout << "Frame delivery tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
