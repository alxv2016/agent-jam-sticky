// The clock every deck screen carries, top-right in `caption`: `Fri 7 Nov  2:32 PM` from the RTC, blank while the time has never been set. A screen
// builds it, calls Update() when it enters and from its own timer, which counts no faster than DESIGN's `motion.self-update` (the words change
// once a minute, `motion.clock-tick`).

#pragma once

#include "lvgl.h"

namespace agent_deck {

class Clock {
public:
    // The label in `parent`, aligned to its top-right.
    void Build(lv_obj_t *parent);
    // Reads the RTC and sets the words when they changed.
    void Update();
    lv_obj_t *Label() const { return label_; }

private:
    lv_obj_t *label_ = nullptr;
    char text_[32] = {};
};

}  // namespace agent_deck
