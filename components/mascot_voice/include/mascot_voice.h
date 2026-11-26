// A page's hearing of Claude's mascot: its moments told as each frame is drawn go to the buzzer (devices::QueueMoment, which the
// shell plays as the glass's next refresh starts), and its music — the typing, the spin, the success, the wave, the thinking —
// keeps time with its move on the buzzer's one voice (rules in mascot_voice_rules.h). The hardware test's mascot page and the agent
// deck's screens each keep one Voice per mascot they show. Everything under the LVGL lock.

#pragma once

#include <cstdint>

#include "lvgl.h"
#include "mascot_voice_rules.h"
#include "widgets/mascot.h"

namespace mascot_voice {

class Voice {
public:
    // Where this mascot's moments go: held for the next refresh, while the voice is on. A mascot rebuilt is attached anew. Several mascots
    // may share one voice, which hears them all as one: `same_blink_ms`, when not 0, makes a blink told within that of the last the same
    // blink, heard once — the needs-you list's avatars, which blink on one clock (AGENT_DECK 4.7).
    void Attach(const widgets::Mascot &mascot, uint32_t same_blink_ms = 0);

    // A moment told by a mascot attached to this voice: held for the next refresh while the voice is on.
    void Tell(widgets::MascotSfx sfx);

    // Whether the mascot is heard at all: the deck's sound setting, the hardware test's Move sounds toggle. Turning it off stops
    // the music and drops the moments held.
    void SetOn(bool on);
    bool On() const { return on_; }

    // A look at the mascot at `now` (an lv_tick_get() time): plays, keeps or stops its music as it stands.
    void Listen(const widgets::Mascot &mascot, uint32_t now);

    // Stops the music; `drop` drops the moments held and not yet played, so none sounds over what comes next. The success plays to
    // its end unless `force`.
    void Stop(bool drop = true, bool force = true);

private:
    void Hear(const Heard &heard, bool force);

    Heard heard_;
    bool on_ = true;
    bool typing_ = false;
    uint32_t typing_since_ = 0;
    uint32_t same_blink_ms_ = 0;
    bool blinked_ = false;
    uint32_t last_blink_ms_ = 0;
};

}  // namespace mascot_voice
